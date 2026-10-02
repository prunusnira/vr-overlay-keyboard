#include "imgui_input_session.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cstdint>

namespace {
constexpr int kTextEditBackspace = 0x200009;

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
}

ImGuiInputSession::EditorInteraction ImGuiInputSession::drawEditor(
    const char *label, std::string &text, const ImVec2 &size, bool requestFocus) {
    ImGuiWindow *window = ImGui::GetCurrentWindow();
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const ImVec2 actualSize = ImGui::CalcItemSize(size, ImGui::CalcItemWidth(), ImGui::GetFrameHeight());
    const ImRect bounds(position, ImVec2(position.x + actualSize.x, position.y + actualSize.y));
    m_editorId = window->GetID(label);

    const ImGuiIO &io = ImGui::GetIO();
    const bool clicked = io.MouseClicked[ImGuiMouseButton_Left] &&
        ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
        window->ClipRect.Contains(io.MouseClickedPos[ImGuiMouseButton_Left]) &&
        bounds.Contains(io.MouseClickedPos[ImGuiMouseButton_Left]);
    if (io.MouseClicked[ImGuiMouseButton_Left]) {
        m_editorPointerGesture = clicked;
    }

    // 활성 앱은 이 편집창에 포커스를 유지하되, 이미 활성인 편집창을 매 프레임 재초기화하지 않는다.
    if (requestFocus && ImGui::GetActiveID() != m_editorId) {
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
    m_virtualControlBounds.emplace_back(visibleBounds.Min.x, visibleBounds.Min.y,
                                        visibleBounds.Max.x, visibleBounds.Max.y);

    const ImGuiIO &io = ImGui::GetIO();
    const bool windowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    const bool hovered = enabled && windowHovered && ImGui::IsMouseHoveringRect(bounds.Min, bounds.Max, true);
    if (hovered) {
        // HoveredId만 등록하고 편집창의 ActiveId와 키보드 포커스는 바꾸지 않는다.
        ImGui::SetHoveredID(id);
    }
    if (enabled && io.MouseClicked[ImGuiMouseButton_Left] && windowHovered &&
        window->ClipRect.Contains(io.MouseClickedPos[ImGuiMouseButton_Left]) &&
        bounds.Contains(io.MouseClickedPos[ImGuiMouseButton_Left])) {
        m_pressedButtonId = id;
    }

    const bool held = m_pressedButtonId == id && io.MouseDown[ImGuiMouseButton_Left] && hovered;
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
    if (enabled && m_pressedButtonId == id && io.MouseReleased[ImGuiMouseButton_Left]) {
        m_pressedButtonId = 0;
        return hovered;
    }
    return false;
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
    m_virtualControlBounds.emplace_back(visibleBounds.Min.x, visibleBounds.Min.y,
                                        visibleBounds.Max.x, visibleBounds.Max.y);
    const ImGuiIO &io = ImGui::GetIO();
    const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
        ImGui::IsMouseHoveringRect(bounds.Min, bounds.Max, true);
    if (hovered) {
        ImGui::SetHoveredID(id);
        if (io.MouseClicked[ImGuiMouseButton_Left]) {
            m_draggedSliderId = id;
        }
    }
    // 설정 슬라이더도 가상 버튼처럼 ActiveId를 변경하지 않아 IME 조합과 드래그가 충돌하지 않는다.
    const bool dragging = m_draggedSliderId == id;
    const float previousValue = value;
    constexpr float thumbWidth = 12.0f;
    const float travel = std::max(1.0f, actualSize.x - thumbWidth);
    value = std::clamp(value, minimum, maximum);
    if (dragging) {
        const float fraction = std::clamp((io.MousePos.x - position.x - thumbWidth * 0.5f) / travel,
                                          0.0f, 1.0f);
        value = minimum + fraction * (maximum - minimum);
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
    m_virtualControlBounds.emplace_back(visibleBounds.Min.x, visibleBounds.Min.y,
                                        visibleBounds.Max.x, visibleBounds.Max.y);

    const ImGuiIO &io = ImGui::GetIO();
    const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
        ImGui::IsMouseHoveringRect(bounds.Min, bounds.Max, true);
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
    if (hovered && io.MouseClicked[ImGuiMouseButton_Left] && travel > 0.0f) {
        m_draggedScrollbarId = id;
        // 손잡이를 잡은 위치를 유지하고, 트랙 클릭은 손잡이 중심을 해당 위치로 옮긴다.
        const float clickX = io.MouseClickedPos[ImGuiMouseButton_Left].x;
        m_scrollbarGrabOffset = clickX >= thumbX && clickX <= thumbX + thumbWidth
            ? clickX - thumbX : thumbWidth * 0.5f;
    }
    const bool dragging = m_draggedScrollbarId == id;
    if (dragging && travel > 0.0f) {
        scroll = std::clamp((io.MousePos.x - position.x - m_scrollbarGrabOffset) / travel,
                            0.0f, 1.0f) * maxScroll;
        thumbX = position.x + scroll / maxScroll * travel;
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
    m_virtualControlBounds.emplace_back(visibleMinimum.x, visibleMinimum.y,
                                        visibleMaximum.x, visibleMaximum.y);

    const ImGuiIO &io = ImGui::GetIO();
    const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
        ImGui::IsMouseHoveringRect(bounds.Min, bounds.Max, true);
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
    if (hovered && io.MouseClicked[ImGuiMouseButton_Left] && travel > 0.0f) {
        m_draggedScrollbarId = id;
        const float clickY = io.MouseClickedPos[ImGuiMouseButton_Left].y;
        m_scrollbarGrabOffset = clickY >= thumbY && clickY <= thumbY + thumbHeight
            ? clickY - thumbY : thumbHeight * 0.5f;
    }
    const bool dragging = m_draggedScrollbarId == id;
    if (dragging && travel > 0.0f) {
        scroll = std::clamp((io.MousePos.y - position.y - m_scrollbarGrabOffset) / travel,
                            0.0f, 1.0f) * maxScroll;
        thumbY = position.y + scroll / maxScroll * travel;
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
        : activeEnd - static_cast<int>(activeSuffix.size());
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
                      [x, y](const ImVec4 &bounds) {
        return x >= bounds.x && x < bounds.z && y >= bounds.y && y < bounds.w;
    });
}
