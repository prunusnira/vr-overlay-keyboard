#include "imgui_input_session.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cstdint>

namespace {
constexpr int kTextEditBackspace = 0x200009;

std::size_t controllerIndex(keyboard::PointerSource source) {
    return source == keyboard::PointerSource::LeftController ? 0 : 1;
}

std::uint32_t nextUtf8CodePoint(const char *&cursor) {
    const auto lead = static_cast<unsigned char>(*cursor++);
    if (lead < 0x80) {
        return lead;
    }
    if ((lead & 0xE0) == 0xC0 && cursor[0] != '\0') {
        return ((lead & 0x1F) << 6) | (static_cast<unsigned char>(*cursor++) & 0x3F);
    }
    if ((lead & 0xF0) == 0xE0 && cursor[0] != '\0' && cursor[1] != '\0') {
        const std::uint32_t codePoint = ((lead & 0x0F) << 12) |
            ((static_cast<unsigned char>(cursor[0]) & 0x3F) << 6) |
            (static_cast<unsigned char>(cursor[1]) & 0x3F);
        cursor += 2;
        return codePoint;
    }
    if ((lead & 0xF8) == 0xF0 && cursor[0] != '\0' && cursor[1] != '\0' && cursor[2] != '\0') {
        const std::uint32_t codePoint = ((lead & 0x07) << 18) |
            ((static_cast<unsigned char>(cursor[0]) & 0x3F) << 12) |
            ((static_cast<unsigned char>(cursor[1]) & 0x3F) << 6) |
            (static_cast<unsigned char>(cursor[2]) & 0x3F);
        cursor += 3;
        return codePoint;
    }
    return 0xFFFD;
}

int utf8CodePointCount(const std::string &text) {
    int count = 0;
    for (unsigned char byte : text) {
        if ((byte & 0xC0) != 0x80) {
            ++count;
        }
    }
    return count;
}

// 프로토타입의 버튼 event filter처럼 편집기에 전달할 포인터 상태만 잠시 차단한다.
// 범위를 벗어나면 원래 상태를 복구하므로 이후 버튼은 같은 누름·뗌 이벤트를 받는다.
class EditorPointerScope final {
public:
    explicit EditorPointerScope(bool suppress) : m_io(ImGui::GetIO()), m_suppress(suppress) {
        if (m_suppress) {
            m_clicked = m_io.MouseClicked[ImGuiMouseButton_Left];
            m_down = m_io.MouseDown[ImGuiMouseButton_Left];
            m_released = m_io.MouseReleased[ImGuiMouseButton_Left];
            m_io.MouseClicked[ImGuiMouseButton_Left] = false;
            m_io.MouseDown[ImGuiMouseButton_Left] = false;
            m_io.MouseReleased[ImGuiMouseButton_Left] = false;
        }
    }

    ~EditorPointerScope() {
        if (m_suppress) {
            m_io.MouseClicked[ImGuiMouseButton_Left] = m_clicked;
            m_io.MouseDown[ImGuiMouseButton_Left] = m_down;
            m_io.MouseReleased[ImGuiMouseButton_Left] = m_released;
        }
    }

private:
    ImGuiIO &m_io;
    bool m_suppress = false;
    bool m_clicked = false;
    bool m_down = false;
    bool m_released = false;
};
}

void ImGuiInputSession::beginFrame() {
    m_virtualControlBounds.clear();
    const ImGuiIO &io = ImGui::GetIO();
    if (!io.MouseDown[ImGuiMouseButton_Left] && !io.MouseReleased[ImGuiMouseButton_Left]) {
        m_pressedButtonId = 0;
        m_draggedScrollbarId = 0;
        m_draggedSliderId = 0;
    }
}

void ImGuiInputSession::endFrame() {
    // 후보나 언어 버튼이 도중에 사라져도 완료된 포인터 누름이 다음 클릭에 남지 않게 한다.
    if (ImGui::GetIO().MouseReleased[ImGuiMouseButton_Left]) {
        m_pressedButtonId = 0;
        m_draggedScrollbarId = 0;
        m_draggedSliderId = 0;
    }
    m_previousPointerTargets = m_virtualControlBounds;
    m_controllerClicks.clear();
}

void ImGuiInputSession::dispatchControllerPointerEvent(const keyboard::PointerEvent &event) {
    if (!ImGui::GetCurrentContext() || event.source == keyboard::PointerSource::Desktop) {
        return;
    }

    const std::size_t index = controllerIndex(event.source);
    ControllerPointerState &pointer = m_controllerPointers[index];
    ImGuiIO &io = ImGui::GetIO();
    switch (event.type) {
    case keyboard::PointerEventType::Move:
        pointer.visible = true;
        pointer.x = event.x;
        pointer.y = event.y;
        if (pointer.editorMouseCapture) {
            io.AddMousePosEvent(static_cast<float>(event.x), static_cast<float>(event.y));
        }
        break;
    case keyboard::PointerEventType::Press:
        pointer.visible = true;
        pointer.down = true;
        pointer.x = event.x;
        pointer.y = event.y;
        pointer.pressedTargetId = pointerTargetAt(event.x, event.y);
        if (pointer.pressedTargetId == m_editorId && m_controllerEditorOwner < 0 &&
            !io.MouseDown[ImGuiMouseButton_Left]) {
            pointer.editorMouseCapture = true;
            m_controllerEditorOwner = static_cast<int>(index);
            io.AddMousePosEvent(static_cast<float>(event.x), static_cast<float>(event.y));
            io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
        }
        break;
    case keyboard::PointerEventType::Release: {
        pointer.visible = true;
        pointer.x = event.x;
        pointer.y = event.y;
        if (pointer.editorMouseCapture) {
            io.AddMousePosEvent(static_cast<float>(event.x), static_cast<float>(event.y));
            io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
            pointer.editorMouseCapture = false;
            m_controllerEditorOwner = -1;
        }
        const ImGuiID releasedTarget = pointerTargetAt(event.x, event.y);
        if (pointer.down && pointer.pressedTargetId != 0 &&
            releasedTarget == pointer.pressedTargetId) {
            m_controllerClicks.push_back({releasedTarget, ImVec2(
                static_cast<float>(event.x), static_cast<float>(event.y))});
        }
        pointer.down = false;
        pointer.pressedTargetId = 0;
        break;
    }
    case keyboard::PointerEventType::Leave:
        pointer.visible = false;
        pointer.x = -1;
        pointer.y = -1;
        if (pointer.editorMouseCapture) {
            io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        }
        break;
    case keyboard::PointerEventType::Cancel:
        pointer.visible = false;
        pointer.down = false;
        pointer.x = -1;
        pointer.y = -1;
        pointer.pressedTargetId = 0;
        if (pointer.editorMouseCapture) {
            io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
            io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
            pointer.editorMouseCapture = false;
            m_controllerEditorOwner = -1;
        }
        break;
    }
}

void ImGuiInputSession::registerPointerTarget(const ImVec4 &bounds, ImGuiID id, bool virtualControl) {
    m_virtualControlBounds.push_back({bounds, id, virtualControl});
}

ImGuiID ImGuiInputSession::pointerTargetAt(int x, int y) const {
    for (auto target = m_previousPointerTargets.rbegin(); target != m_previousPointerTargets.rend(); ++target) {
        if (x >= target->bounds.x && x < target->bounds.z &&
            y >= target->bounds.y && y < target->bounds.w) {
            return target->id;
        }
    }
    return 0;
}

bool ImGuiInputSession::controllerPointerOver(ImGuiID id, const ImVec4 &bounds) const {
    for (const ControllerPointerState &pointer : m_controllerPointers) {
        if (pointer.visible && (!pointer.down || pointer.pressedTargetId == id) &&
            pointer.x >= bounds.x && pointer.x < bounds.z &&
            pointer.y >= bounds.y && pointer.y < bounds.w) {
            return true;
        }
    }
    return false;
}

bool ImGuiInputSession::controllerPointerDownOn(ImGuiID id) const {
    return std::any_of(m_controllerPointers.begin(), m_controllerPointers.end(), [id](const auto &pointer) {
        return pointer.visible && pointer.down && pointer.pressedTargetId == id;
    });
}

bool ImGuiInputSession::consumeControllerClick(ImGuiID id, ImVec2 *position) {
    bool found = false;
    m_controllerClicks.erase(std::remove_if(m_controllerClicks.begin(), m_controllerClicks.end(),
        [id, position, &found](const ControllerClick &click) {
            if (click.targetId != id) {
                return false;
            }
            if (position) {
                *position = click.position;
            }
            found = true;
            return true;
        }), m_controllerClicks.end());
    return found;
}

ImGuiInputSession::EditorInteraction ImGuiInputSession::drawEditor(
    const char *label, std::string &text, const ImVec2 &size, bool requestFocus) {
    ImGuiWindow *window = ImGui::GetCurrentWindow();
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const ImVec2 actualSize = ImGui::CalcItemSize(size, ImGui::CalcItemWidth(), ImGui::GetFrameHeight());
    const ImRect bounds(position, ImVec2(position.x + actualSize.x, position.y + actualSize.y));
    m_editorId = window->GetID(label);
    const ImRect visibleBounds(ImMax(bounds.Min, window->ClipRect.Min), ImMin(bounds.Max, window->ClipRect.Max));
    if (!window->SkipItems && visibleBounds.GetWidth() > 0.0f && visibleBounds.GetHeight() > 0.0f) {
        registerPointerTarget(ImVec4(visibleBounds.Min.x, visibleBounds.Min.y,
                                     visibleBounds.Max.x, visibleBounds.Max.y), m_editorId, false);
    }

    const ImGuiIO &io = ImGui::GetIO();
    const bool desktopClicked = io.MouseClicked[ImGuiMouseButton_Left] &&
        ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
        window->ClipRect.Contains(io.MouseClickedPos[ImGuiMouseButton_Left]) &&
        bounds.Contains(io.MouseClickedPos[ImGuiMouseButton_Left]);
    const bool controllerClicked = consumeControllerClick(m_editorId);
    const bool clicked = desktopClicked || controllerClicked;
    const bool controllerEditorGesture = std::any_of(m_controllerPointers.begin(), m_controllerPointers.end(),
        [](const ControllerPointerState &pointer) { return pointer.editorMouseCapture; });
    if (io.MouseClicked[ImGuiMouseButton_Left]) {
        m_editorPointerGesture = clicked || controllerEditorGesture;
    }

    // 활성 앱은 이 편집창에 포커스를 유지하되, 이미 활성인 편집창을 매 프레임 재초기화하지 않는다.
    if ((requestFocus || controllerClicked) && ImGui::GetActiveID() != m_editorId) {
        ImGui::SetKeyboardFocusHere();
    }
    {
        // InputText는 바깥 클릭만으로 ActiveId를 해제한다. 가상 버튼의 클릭과 드래그를
        // 편집기에 전달하지 않아 IME 조합·커서·선택을 유지하고, 편집창에서 시작한 드래그는 허용한다.
        EditorPointerScope pointerScope(!m_editorPointerGesture);
        ImGui::InputTextMultiline(label, &text, size);
    }
    const bool active = ImGui::GetActiveID() == m_editorId;
    if (!io.MouseDown[ImGuiMouseButton_Left]) {
        m_editorPointerGesture = false;
    }
    return {clicked, active};
}

bool ImGuiInputSession::button(const char *label, const ImVec2 &size, bool selected, bool enabled) {
    ImGuiWindow *window = ImGui::GetCurrentWindow();
    if (window->SkipItems) {
        return false;
    }

    const ImGuiStyle &style = ImGui::GetStyle();
    const char *labelEnd = ImGui::FindRenderedTextEnd(label);
    const ImVec2 textSize = ImGui::CalcTextSize(label, labelEnd);
    const ImVec2 actualSize = ImGui::CalcItemSize(
        size, textSize.x + style.FramePadding.x * 2.0f, textSize.y + style.FramePadding.y * 2.0f);
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const ImRect bounds(position, ImVec2(position.x + actualSize.x, position.y + actualSize.y));
    ImGui::ItemSize(bounds, style.FramePadding.y);
    const ImGuiID id = window->GetID(label);
    if (!ImGui::ItemAdd(bounds, id, nullptr, ImGuiItemFlags_NoNav)) {
        return false;
    }
    const ImRect visibleBounds(ImMax(bounds.Min, window->ClipRect.Min), ImMin(bounds.Max, window->ClipRect.Max));
    registerPointerTarget(ImVec4(visibleBounds.Min.x, visibleBounds.Min.y,
                                 visibleBounds.Max.x, visibleBounds.Max.y), id);

    const ImGuiIO &io = ImGui::GetIO();
    const bool windowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    const bool desktopHovered = windowHovered && ImGui::IsMouseHoveringRect(bounds.Min, bounds.Max, true);
    const bool controllerHovered = controllerPointerOver(
        id, ImVec4(visibleBounds.Min.x, visibleBounds.Min.y,
                   visibleBounds.Max.x, visibleBounds.Max.y));
    const bool hovered = enabled && (desktopHovered || controllerHovered);
    if (hovered) {
        // HoveredId만 등록하고 편집창의 ActiveId와 키보드 포커스는 바꾸지 않는다.
        ImGui::SetHoveredID(id);
    }
    if (enabled && io.MouseClicked[ImGuiMouseButton_Left] && desktopHovered &&
        window->ClipRect.Contains(io.MouseClickedPos[ImGuiMouseButton_Left]) &&
        bounds.Contains(io.MouseClickedPos[ImGuiMouseButton_Left])) {
        m_pressedButtonId = id;
    }

    const bool desktopHeld = m_pressedButtonId == id && io.MouseDown[ImGuiMouseButton_Left] && desktopHovered;
    const bool controllerHeld = controllerPointerDownOn(id) && controllerHovered;
    const bool held = desktopHeld || controllerHeld;
    const ImGuiCol normalColor = !enabled ? ImGuiCol_FrameBg : selected ? ImGuiCol_Header : ImGuiCol_Button;
    const ImGuiCol hoveredColor = selected ? ImGuiCol_HeaderHovered : ImGuiCol_ButtonHovered;
    const ImGuiCol activeColor = selected ? ImGuiCol_HeaderActive : ImGuiCol_ButtonActive;
    ImGui::RenderFrame(bounds.Min, bounds.Max,
                       ImGui::GetColorU32(held ? activeColor : hovered ? hoveredColor : normalColor),
                       true, style.FrameRounding);
    const ImVec2 textPosition(bounds.Min.x + (actualSize.x - textSize.x) * 0.5f,
                              bounds.Min.y + (actualSize.y - textSize.y) * 0.5f);
    if (labelEnd > label) {
        ImGui::GetWindowDrawList()->AddText(
            textPosition, ImGui::GetColorU32(enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled), label, labelEnd);
    }

    // 프로토타입과 같이 같은 버튼 안에서 뗀 경우에만 한 번 실행하고, 드래그 이탈은 취소한다.
    bool desktopActivated = false;
    if (enabled && m_pressedButtonId == id && io.MouseReleased[ImGuiMouseButton_Left]) {
        m_pressedButtonId = 0;
        desktopActivated = desktopHovered;
    }
    const bool controllerClick = consumeControllerClick(id);
    return desktopActivated || (enabled && controllerClick);
}

bool ImGuiInputSession::sliderFloat(const char *label, const ImVec2 &size, float &value,
                                   float minimum, float maximum, const char *format) {
    ImGuiWindow *window = ImGui::GetCurrentWindow();
    if (window->SkipItems || maximum <= minimum) {
        return false;
    }
    const ImVec2 actualSize = ImGui::CalcItemSize(size, ImGui::CalcItemWidth(), ImGui::GetFrameHeight());
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const ImRect bounds(position, ImVec2(position.x + actualSize.x, position.y + actualSize.y));
    ImGui::ItemSize(bounds, ImGui::GetStyle().FramePadding.y);
    const ImGuiID id = window->GetID(label);
    if (!ImGui::ItemAdd(bounds, id, nullptr, ImGuiItemFlags_NoNav)) {
        return false;
    }
    const ImRect visibleBounds(ImMax(bounds.Min, window->ClipRect.Min), ImMin(bounds.Max, window->ClipRect.Max));
    registerPointerTarget(ImVec4(visibleBounds.Min.x, visibleBounds.Min.y,
                                 visibleBounds.Max.x, visibleBounds.Max.y), id);
    const ImGuiIO &io = ImGui::GetIO();
    const bool desktopHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
        ImGui::IsMouseHoveringRect(bounds.Min, bounds.Max, true);
    const bool controllerHovered = controllerPointerOver(
        id, ImVec4(visibleBounds.Min.x, visibleBounds.Min.y,
                   visibleBounds.Max.x, visibleBounds.Max.y));
    const bool hovered = desktopHovered || controllerHovered;
    if (hovered) {
        ImGui::SetHoveredID(id);
    }
    if (desktopHovered && io.MouseClicked[ImGuiMouseButton_Left]) {
        m_draggedSliderId = id;
    }
    // 설정 슬라이더도 가상 버튼처럼 ActiveId를 변경하지 않아 IME 조합과 드래그가 충돌하지 않는다.
    const bool desktopDragging = m_draggedSliderId == id;
    const bool controllerDragging = controllerPointerDownOn(id);
    ImVec2 controllerClickPosition{};
    const bool controllerClicked = consumeControllerClick(id, &controllerClickPosition);
    const bool dragging = desktopDragging || controllerDragging || controllerClicked;
    const float previousValue = value;
    constexpr float thumbWidth = 12.0f;
    const float travel = std::max(1.0f, actualSize.x - thumbWidth);
    value = std::clamp(value, minimum, maximum);
    const auto setFromX = [&](float pointerX) {
        const float fraction = std::clamp((pointerX - position.x - thumbWidth * 0.5f) / travel,
                                          0.0f, 1.0f);
        value = minimum + fraction * (maximum - minimum);
    };
    if (desktopDragging) {
        setFromX(io.MousePos.x);
    }
    if (controllerClicked) {
        setFromX(controllerClickPosition.x);
    }
    for (const ControllerPointerState &pointer : m_controllerPointers) {
        if (pointer.visible && pointer.down && pointer.pressedTargetId == id) {
            setFromX(static_cast<float>(pointer.x));
        }
    }
    ImGui::RenderFrame(bounds.Min, bounds.Max, ImGui::GetColorU32(
        dragging ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg),
        true, ImGui::GetStyle().FrameRounding);
    const float thumbX = position.x + (value - minimum) / (maximum - minimum) * travel;
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(thumbX, position.y + 2.0f),
        ImVec2(thumbX + thumbWidth, bounds.Max.y - 2.0f),
        ImGui::GetColorU32(dragging ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab), 3.0f);
    char valueText[64];
    ImFormatString(valueText, sizeof(valueText), format, value);
    ImGui::RenderTextClipped(bounds.Min, bounds.Max, valueText, nullptr, nullptr, ImVec2(0.5f, 0.5f));
    return value != previousValue;
}

bool ImGuiInputSession::horizontalScrollbar(const char *label, const ImVec2 &size,
                                           float maxScroll, float visibleWidth, float &scroll) {
    ImGuiWindow *window = ImGui::GetCurrentWindow();
    if (window->SkipItems) {
        return false;
    }
    const ImVec2 actualSize = ImGui::CalcItemSize(size, ImGui::GetContentRegionAvail().x, 14.0f);
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const ImRect bounds(position, ImVec2(position.x + actualSize.x, position.y + actualSize.y));
    ImGui::ItemSize(bounds);
    const ImGuiID id = window->GetID(label);
    if (!ImGui::ItemAdd(bounds, id, nullptr, ImGuiItemFlags_NoNav)) {
        return false;
    }
    const ImRect visibleBounds(ImMax(bounds.Min, window->ClipRect.Min), ImMin(bounds.Max, window->ClipRect.Max));
    registerPointerTarget(ImVec4(visibleBounds.Min.x, visibleBounds.Min.y,
                                 visibleBounds.Max.x, visibleBounds.Max.y), id);

    const ImGuiIO &io = ImGui::GetIO();
    const bool desktopHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
        ImGui::IsMouseHoveringRect(bounds.Min, bounds.Max, true);
    const bool controllerHovered = controllerPointerOver(
        id, ImVec4(visibleBounds.Min.x, visibleBounds.Min.y,
                   visibleBounds.Max.x, visibleBounds.Max.y));
    const bool hovered = desktopHovered || controllerHovered;
    if (hovered) {
        ImGui::SetHoveredID(id);
    }
    maxScroll = std::max(0.0f, maxScroll);
    visibleWidth = std::max(1.0f, visibleWidth);
    const float previousScroll = scroll;
    scroll = std::clamp(scroll, 0.0f, maxScroll);
    const float thumbWidth = maxScroll > 0.0f
        ? std::clamp(actualSize.x * visibleWidth / (visibleWidth + maxScroll),
                     std::min(18.0f, actualSize.x), actualSize.x)
        : actualSize.x;
    const float travel = actualSize.x - thumbWidth;
    float thumbX = position.x + (maxScroll > 0.0f ? scroll / maxScroll * travel : 0.0f);
    if (desktopHovered && io.MouseClicked[ImGuiMouseButton_Left] && travel > 0.0f) {
        m_draggedScrollbarId = id;
        // 손잡이를 잡은 위치를 유지하고, 트랙 클릭은 손잡이 중심을 해당 위치로 옮긴다.
        const float clickX = io.MouseClickedPos[ImGuiMouseButton_Left].x;
        m_scrollbarGrabOffset = clickX >= thumbX && clickX <= thumbX + thumbWidth
            ? clickX - thumbX : thumbWidth * 0.5f;
    }
    const bool desktopDragging = m_draggedScrollbarId == id;
    const bool controllerDragging = controllerPointerDownOn(id);
    ImVec2 controllerClickPosition{};
    const bool controllerClicked = consumeControllerClick(id, &controllerClickPosition);
    const bool dragging = desktopDragging || controllerDragging || controllerClicked;
    if (desktopDragging && travel > 0.0f) {
        scroll = std::clamp((io.MousePos.x - position.x - m_scrollbarGrabOffset) / travel,
                            0.0f, 1.0f) * maxScroll;
        thumbX = position.x + scroll / maxScroll * travel;
    }
    if (controllerClicked && travel > 0.0f) {
        scroll = std::clamp((controllerClickPosition.x - position.x - thumbWidth * 0.5f) / travel,
                            0.0f, 1.0f) * maxScroll;
        thumbX = position.x + scroll / maxScroll * travel;
    }
    for (const ControllerPointerState &pointer : m_controllerPointers) {
        if (pointer.visible && pointer.down && pointer.pressedTargetId == id && travel > 0.0f) {
            scroll = std::clamp((static_cast<float>(pointer.x) - position.x - thumbWidth * 0.5f) / travel,
                                0.0f, 1.0f) * maxScroll;
            thumbX = position.x + scroll / maxScroll * travel;
        }
    }
    // ImGui 기본 스크롤바의 ActiveId 변경을 피하고 가로 스크롤 위치만 갱신한다.
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(bounds.Min, bounds.Max, ImGui::GetColorU32(ImGuiCol_ScrollbarBg), 6.0f);
    const ImGuiCol thumbColor = dragging ? ImGuiCol_ScrollbarGrabActive :
        hovered ? ImGuiCol_ScrollbarGrabHovered : ImGuiCol_ScrollbarGrab;
    drawList->AddRectFilled(ImVec2(thumbX, position.y),
                            ImVec2(thumbX + thumbWidth, bounds.Max.y),
                            ImGui::GetColorU32(thumbColor), 6.0f);
    return scroll != previousScroll;
}

bool ImGuiInputSession::verticalScrollbar(const char *label, const ImVec2 &size,
                                         float maxScroll, float visibleHeight, float &scroll) {
    ImGuiWindow *window = ImGui::GetCurrentWindow();
    if (window->SkipItems) {
        return false;
    }
    const ImVec2 actualSize = ImGui::CalcItemSize(size, 24.0f, ImGui::GetContentRegionAvail().y);
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const ImRect bounds(position, ImVec2(position.x + actualSize.x, position.y + actualSize.y));
    ImGui::ItemSize(bounds);
    const ImGuiID id = window->GetID(label);
    if (!ImGui::ItemAdd(bounds, id, nullptr, ImGuiItemFlags_NoNav)) {
        return false;
    }
    const ImVec2 visibleMinimum(ImMax(bounds.Min.x, window->ClipRect.Min.x),
                                ImMax(bounds.Min.y, window->ClipRect.Min.y));
    const ImVec2 visibleMaximum(ImMin(bounds.Max.x, window->ClipRect.Max.x),
                                ImMin(bounds.Max.y, window->ClipRect.Max.y));
    registerPointerTarget(ImVec4(visibleMinimum.x, visibleMinimum.y,
                                 visibleMaximum.x, visibleMaximum.y), id);

    const ImGuiIO &io = ImGui::GetIO();
    const bool desktopHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
        ImGui::IsMouseHoveringRect(bounds.Min, bounds.Max, true);
    const bool controllerHovered = controllerPointerOver(
        id, ImVec4(visibleMinimum.x, visibleMinimum.y,
                   visibleMaximum.x, visibleMaximum.y));
    const bool hovered = desktopHovered || controllerHovered;
    if (hovered) {
        ImGui::SetHoveredID(id);
    }
    maxScroll = std::max(0.0f, maxScroll);
    visibleHeight = std::max(1.0f, visibleHeight);
    const float previousScroll = scroll;
    scroll = std::clamp(scroll, 0.0f, maxScroll);
    const float thumbHeight = maxScroll > 0.0f
        ? std::clamp(actualSize.y * visibleHeight / (visibleHeight + maxScroll),
                     std::min(32.0f, actualSize.y), actualSize.y)
        : actualSize.y;
    const float travel = actualSize.y - thumbHeight;
    float thumbY = position.y + (maxScroll > 0.0f ? scroll / maxScroll * travel : 0.0f);
    if (desktopHovered && io.MouseClicked[ImGuiMouseButton_Left] && travel > 0.0f) {
        m_draggedScrollbarId = id;
        const float clickY = io.MouseClickedPos[ImGuiMouseButton_Left].y;
        m_scrollbarGrabOffset = clickY >= thumbY && clickY <= thumbY + thumbHeight
            ? clickY - thumbY : thumbHeight * 0.5f;
    }
    const bool desktopDragging = m_draggedScrollbarId == id;
    const bool controllerDragging = controllerPointerDownOn(id);
    ImVec2 controllerClickPosition{};
    const bool controllerClicked = consumeControllerClick(id, &controllerClickPosition);
    const bool dragging = desktopDragging || controllerDragging || controllerClicked;
    if (desktopDragging && travel > 0.0f) {
        scroll = std::clamp((io.MousePos.y - position.y - m_scrollbarGrabOffset) / travel,
                            0.0f, 1.0f) * maxScroll;
        thumbY = position.y + scroll / maxScroll * travel;
    }
    if (controllerClicked && travel > 0.0f) {
        scroll = std::clamp((controllerClickPosition.y - position.y - thumbHeight * 0.5f) / travel,
                            0.0f, 1.0f) * maxScroll;
        thumbY = position.y + scroll / maxScroll * travel;
    }
    for (const ControllerPointerState &pointer : m_controllerPointers) {
        if (pointer.visible && pointer.down && pointer.pressedTargetId == id && travel > 0.0f) {
            scroll = std::clamp((static_cast<float>(pointer.y) - position.y - thumbHeight * 0.5f) / travel,
                                0.0f, 1.0f) * maxScroll;
            thumbY = position.y + scroll / maxScroll * travel;
        }
    }

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(bounds.Min, bounds.Max, ImGui::GetColorU32(ImGuiCol_ScrollbarBg), 8.0f);
    const ImGuiCol thumbColor = dragging ? ImGuiCol_ScrollbarGrabActive :
        hovered ? ImGuiCol_ScrollbarGrabHovered : ImGuiCol_ScrollbarGrab;
    drawList->AddRectFilled(ImVec2(position.x, thumbY),
                            ImVec2(bounds.Max.x, thumbY + thumbHeight),
                            ImGui::GetColorU32(thumbColor), 8.0f);
    return scroll != previousScroll;
}

void ImGuiInputSession::clearEditor(std::string &text) {
    text.clear();
    if (ImGuiInputTextState *state = ImGui::GetInputTextState(m_editorId)) {
        // 활성 편집창은 내부 버퍼를 보유하므로 외부 문자열 삭제도 다음 프레임에 반영시킨다.
        state->ReloadUserBufAndMoveToEnd();
    }
}

bool ImGuiInputSession::hasActiveEditorState() const {
    return ImGui::GetInputTextState(m_editorId) != nullptr &&
        ImGui::GetActiveID() == m_editorId;
}

bool ImGuiInputSession::editorCursorMatchesRange(int start, int end) const {
    const ImGuiInputTextState *state = ImGui::GetInputTextState(m_editorId);
    return state && ImGui::GetActiveID() == m_editorId &&
        !state->HasSelection() && state->GetCursorPos() == end &&
        start >= 0 && end >= start && end <= state->TextLen;
}

bool ImGuiInputSession::editEditorText(std::string &text,
                                       int replaceStart,
                                       int replaceEnd,
                                       const std::string &replacement,
                                       const std::string &activeSuffix,
                                       int &activeStart,
                                       int &activeEnd) {
    ImGuiInputTextState *state = ImGui::GetInputTextState(m_editorId);
    if (!state || ImGui::GetActiveID() != m_editorId) {
        return false;
    }
    if (replaceStart >= 0 || replaceEnd >= 0) {
        if (replaceStart < 0 || replaceEnd < replaceStart || replaceEnd > state->TextLen) {
            return false;
        }
        state->SetSelection(replaceStart, replaceEnd);
    }

    if (!replacement.empty()) {
        const std::size_t requiredCapacity = static_cast<std::size_t>(state->TextLen) +
            replacement.size() + 1;
        if (requiredCapacity > static_cast<std::size_t>(state->BufCapacity)) {
            text.reserve(std::max(requiredCapacity, text.capacity() * 2 + 1));
            state->BufCapacity = static_cast<int>(text.capacity() + 1);
            state->TextA.resize(state->BufCapacity);
            state->TextSrc = state->TextA.Data;
        }
    }

    if (replacement.empty()) {
        if (state->HasSelection()) {
            state->OnKeyPressed(kTextEditBackspace);
        }
    } else {
        const char *cursor = replacement.c_str();
        const char *end = cursor + replacement.size();
        while (cursor < end) {
            const std::uint32_t codePoint = nextUtf8CodePoint(cursor);
            state->OnCharPressed(codePoint);
        }
    }

    state->EditedBefore = true;
    state->EditedThisFrame = true;
    state->CursorFollow = true;
    text.assign(state->TextA.Data, static_cast<std::size_t>(state->TextLen));
    activeEnd = state->GetCursorPos();
    activeStart = activeSuffix.empty()
        ? -1
        : activeEnd - utf8CodePointCount(activeSuffix);
    if (activeStart < 0 || activeSuffix.empty()) {
        activeStart = -1;
        activeEnd = -1;
    }
    return true;
}

bool ImGuiInputSession::backspaceEditor(std::string &text) {
    ImGuiInputTextState *state = ImGui::GetInputTextState(m_editorId);
    if (!state || ImGui::GetActiveID() != m_editorId) {
        return false;
    }
    state->OnKeyPressed(kTextEditBackspace);
    state->EditedBefore = true;
    state->EditedThisFrame = true;
    text.assign(state->TextA.Data, static_cast<std::size_t>(state->TextLen));
    return true;
}

bool ImGuiInputSession::isVirtualControlAt(int x, int y) const {
    return std::any_of(m_virtualControlBounds.begin(), m_virtualControlBounds.end(),
                      [x, y](const PointerTarget &target) {
        return target.virtualControl && x >= target.bounds.x && x < target.bounds.z &&
            y >= target.bounds.y && y < target.bounds.w;
    });
}
