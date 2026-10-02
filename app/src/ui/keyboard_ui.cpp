#include "keyboard_ui.h"

#include "../core/keyboard_layout.h"
#include "ui_text_catalog.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <filesystem>
#include <iterator>
#include <sstream>
#include <utility>

namespace {
constexpr float kFontSize = 14.0f;
constexpr float kKeyHeight = 41.0f;
constexpr float kBottomRowHeight = 43.0f;
constexpr float kKeyWidth = 54.0f;
constexpr float kImePreviewHeight = 80.0f;

std::string candidateSignature(const keyboard::CandidateSnapshot &snapshot) {
    std::ostringstream signature;
    signature << snapshot.active << ':' << snapshot.selectedIndex << ':' << snapshot.currentPage;
    for (const std::string &candidate : snapshot.candidates) {
        signature << ':' << candidate.size() << ':' << candidate;
    }
    return signature.str();
}

int mouseButtonIndex(keyboard::PointerButton button) {
    switch (button) {
    case keyboard::PointerButton::Left: return ImGuiMouseButton_Left;
    case keyboard::PointerButton::Right: return ImGuiMouseButton_Right;
    default: return -1;
    }
}

const char *localized(keyboard::UiLanguage language, keyboard::ui_text::TextId id) {
    return keyboard::ui_text::text(language, id);
}

const keyboard::InputLanguage *activeInputLanguage(const keyboard::AppUiState &state) {
    const auto found = std::find_if(state.inputLanguages.begin(), state.inputLanguages.end(),
        [](const keyboard::InputLanguage &language) { return language.active; });
    return found == state.inputLanguages.end() ? nullptr : &*found;
}

bool languageButtonIsActive(const keyboard::AppUiState &state, keyboard::KeyboardLanguage target) {
    const keyboard::InputLanguage *active = activeInputLanguage(state);
    if (!active) {
        return false;
    }
    if (target == keyboard::KeyboardLanguage::English) {
        if (active->kind == keyboard::InputLanguageKind::English) {
            return true;
        }
        if (state.imeMode.available && active->kind != keyboard::InputLanguageKind::Korean &&
            active->kind != keyboard::InputLanguageKind::Japanese) {
            return !state.imeMode.native && !state.imeMode.fullShape;
        }
        if ((active->kind == keyboard::InputLanguageKind::Korean ||
             active->kind == keyboard::InputLanguageKind::Japanese) && state.imeMode.available) {
            return !state.imeMode.native &&
                (active->kind != keyboard::InputLanguageKind::Japanese || !state.imeMode.fullShape);
        }
        return false;
    }
    const keyboard::InputLanguageKind requested = target == keyboard::KeyboardLanguage::Korean
        ? keyboard::InputLanguageKind::Korean
        : keyboard::InputLanguageKind::Japanese;
    if (active->kind != requested) {
        return false;
    }
    if (!state.imeMode.available) {
        return true;
    }
    return state.imeMode.native &&
        (target != keyboard::KeyboardLanguage::Japanese || !state.imeMode.katakana);
}

void drawKeyLegend(const keyboard::KeyboardKeyDefinition &key,
                   keyboard::KeyboardLayoutKind layout,
                   const ImVec2 &minimum,
                   const ImVec2 &maximum) {
    const char *primary = keyboard::primaryKeyLabel(key, layout);
    const char *secondary = keyboard::secondaryKeyLabel(key, layout);
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    if (secondary && *secondary) {
        const ImVec2 secondarySize = ImGui::CalcTextSize(secondary);
        constexpr float kSecondarySize = 9.0f;
        const float scaledWidth = secondarySize.x * kSecondarySize / kFontSize;
        drawList->AddText(ImGui::GetFont(), kSecondarySize,
                          ImVec2(maximum.x - scaledWidth - 5.0f, minimum.y + 3.0f),
                          IM_COL32(123, 202, 183, 255), secondary);
    }
    const ImVec2 labelSize = ImGui::GetFont()->CalcTextSizeA(15.0f, FLT_MAX, 0.0f, primary);
    const ImVec2 labelPosition((minimum.x + maximum.x - labelSize.x) * 0.5f,
                               (minimum.y + maximum.y - labelSize.y) * 0.5f + 2.0f);
    drawList->AddText(ImGui::GetFont(), 15.0f, labelPosition,
                      ImGui::GetColorU32(ImGuiCol_Text), primary);
}

std::string directVirtualCharacter(const keyboard::KeyboardKeyDefinition &key,
                                   keyboard::KeyboardLayoutKind layout,
                                   bool withShift,
                                   bool capsLockEnabled) {
    const char *primary = keyboard::primaryKeyLabel(key, layout);
    const char *shifted = keyboard::secondaryKeyLabel(key, layout);
    if (layout == keyboard::KeyboardLayoutKind::KoreanDubeolsik) {
        return withShift && shifted && *shifted ? shifted : primary;
    }

    if (key.code >= keyboard::KeyCode::A && key.code <= keyboard::KeyCode::Z) {
        const bool uppercase = withShift != capsLockEnabled;
        const char letter = static_cast<char>(
            (uppercase ? 'A' : 'a') + static_cast<int>(key.code) - static_cast<int>(keyboard::KeyCode::A));
        return std::string(1, letter);
    }
    if (withShift && shifted && *shifted) {
        return shifted;
    }
    return primary;
}

std::string directJapaneseCharacter(const keyboard::KeyboardKeyDefinition &key,
                                    bool withShift,
                                    bool capsLockEnabled) {
    if (!withShift && !capsLockEnabled) {
        switch (key.code) {
        case keyboard::KeyCode::OemMinus: return u8"ー";
        case keyboard::KeyCode::OemComma: return u8"、";
        case keyboard::KeyCode::OemPeriod: return u8"。";
        default: break;
        }
    }
    return directVirtualCharacter(key, keyboard::KeyboardLayoutKind::Qwerty,
                                  withShift, capsLockEnabled);
}
}

KeyboardUi::KeyboardUi(keyboard::KeyboardActions &actions)
    : m_actions(actions), m_settingsUi(actions, m_inputSession) {
    addImeFonts();
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(22.0f, 16.0f);
    style.FramePadding = ImVec2(8.0f, 5.0f);
    style.ItemSpacing = ImVec2(6.0f, 6.0f);
    style.WindowRounding = 0.0f;
    style.FrameRounding = 7.0f;
    style.ChildRounding = 10.0f;
    style.ScrollbarRounding = 6.0f;
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.Colors[ImGuiCol_Text] = ImVec4(0.93f, 0.96f, 0.97f, 1.0f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.40f, 0.47f, 0.52f, 1.0f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.043f, 0.063f, 0.086f, 1.0f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.063f, 0.090f, 0.125f, 1.0f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.145f, 0.196f, 0.25f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.043f, 0.071f, 0.098f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.10f, 0.17f, 0.18f, 1.0f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.12f, 0.21f, 0.20f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.086f, 0.129f, 0.169f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.11f, 0.16f, 0.20f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.15f, 0.28f, 0.25f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.13f, 0.27f, 0.25f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.17f, 0.36f, 0.32f, 1.0f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.45f, 0.39f, 1.0f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.21f, 0.75f, 0.65f, 1.0f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.43f, 0.88f, 0.75f, 1.0f);
    appendLog("Ready. VR key presses type here without changing Windows focus.");
}

void KeyboardUi::draw(const keyboard::AppUiState &state, bool applicationIsForeground) {
    m_inputSession.beginFrame();
    if (m_virtualKeyReleaseNextFrame != ImGuiKey_None) {
        ImGui::GetIO().AddKeyEvent(m_virtualKeyReleaseNextFrame, false);
        m_virtualKeyReleaseNextFrame = ImGuiKey_None;
    }
    m_applicationIsForeground = applicationIsForeground;
    if (state.status != m_lastStatus && !state.status.empty()) {
        m_lastStatus = state.status;
        appendLog(state.status);
    }

    const ImGuiIO &io = ImGui::GetIO();
    const keyboard::UiLanguage language = state.settings.uiLanguage;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    // 앱 창의 포커스를 유지해 편집기 IME 세션을 보존한다. 옵션은 아래의 child overlay로 표시한다.
    constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("VR Overlay Keyboard##main", nullptr, windowFlags);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("main-content", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    ImGui::PopStyleVar();

    const ImVec2 header = ImGui::GetCursorScreenPos();
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    const ImVec2 contentMinimum = ImGui::GetWindowPos();
    const ImVec2 contentMaximum(contentMinimum.x + ImGui::GetWindowWidth(),
                                contentMinimum.y + ImGui::GetWindowHeight());
    drawList->AddRectFilledMultiColor(contentMinimum, contentMaximum,
        IM_COL32(20, 29, 39, 255), IM_COL32(13, 20, 28, 255),
        IM_COL32(13, 20, 28, 255), IM_COL32(17, 25, 34, 255));
    drawList->AddRectFilled(header, ImVec2(header.x + 32.0f, header.y + 32.0f),
                            IM_COL32(107, 218, 192, 255), 9.0f);
    drawList->AddText(ImGui::GetFont(), 14.0f, ImVec2(header.x + 9.0f, header.y + 8.0f),
                      IM_COL32(8, 30, 25, 255), "N");
    drawList->AddText(ImGui::GetFont(), 17.0f, ImVec2(header.x + 43.0f, header.y),
                      IM_COL32(237, 244, 247, 255), localized(language, keyboard::ui_text::TextId::WindowTitle));
    drawList->AddText(ImGui::GetFont(), 10.0f, ImVec2(header.x + 43.0f, header.y + 23.0f),
                      IM_COL32(101, 119, 132, 255), "CHATBOX INPUT  /  STEAMVR");
    constexpr const char *copyright = "(c) Studio Nira 2026";
    const float copyrightWidth = ImGui::CalcTextSize(copyright).x * 10.0f / kFontSize;
    drawList->AddText(ImGui::GetFont(), 10.0f,
        ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth() -
               ImGui::GetStyle().WindowPadding.x - copyrightWidth, header.y + 12.0f),
        IM_COL32(113, 131, 143, 255), copyright);
    ImGui::SetCursorScreenPos(ImVec2(header.x, header.y + 38.0f));
    ImGui::Dummy(ImVec2(0.0f, 2.0f));

    const keyboard::InputLanguage *activeLanguage = activeInputLanguage(state);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 4.0f));
    ImGui::BeginChild("status-strip", ImVec2(0.0f, 44.0f), ImGuiChildFlags_Borders);
    const ImVec2 statusPosition = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddCircleFilled(
        ImVec2(statusPosition.x + 4.0f, statusPosition.y + 8.0f), 3.5f,
        IM_COL32(55, 195, 159, 255), 12);
    const std::string statusMessage = std::string(
        localized(language, keyboard::ui_text::TextId::Status)) + ": " + state.status;
    ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), 11.0f,
        ImVec2(statusPosition.x + 13.0f, statusPosition.y - 1.0f),
        IM_COL32(197, 210, 216, 255), statusMessage.c_str());
    const std::string statusDetails = std::string(localized(language, keyboard::ui_text::TextId::WindowsForeground)) + ": " +
        localized(language, m_applicationIsForeground
            ? keyboard::ui_text::TextId::ThisApp
            : keyboard::ui_text::TextId::AnotherApp) + "    ·    " +
        localized(language, keyboard::ui_text::TextId::EditorFocus) + ": " +
        localized(language, m_editorFocusArmed
            ? keyboard::ui_text::TextId::Ready
            : keyboard::ui_text::TextId::NotSelected) + "    ·    " +
        localized(language, keyboard::ui_text::TextId::CurrentInputLanguage) + ": " +
        (activeLanguage ? activeLanguage->label
                        : localized(language, keyboard::ui_text::TextId::Detecting));
    ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), 10.0f,
        ImVec2(statusPosition.x + 13.0f, statusPosition.y + 18.0f),
        IM_COL32(148, 166, 178, 255), statusDetails.c_str());
    ImGui::EndChild();
    ImGui::PopStyleVar();

    drawMissingInputLanguagePopup(language);

    ImGui::Spacing();
    if (ImGui::BeginTable("chatbox-ime-row", 2,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadOuterX)) {
        ImGui::TableSetupColumn("chatbox", ImGuiTableColumnFlags_WidthStretch, 1.05f);
        ImGui::TableSetupColumn("ime-preview", ImGuiTableColumnFlags_WidthStretch, 0.95f);
        ImGui::TableNextColumn();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 7.0f));
        ImGui::BeginChild("chatbox-card", ImVec2(0.0f, 132.0f), ImGuiChildFlags_Borders);
        ImGui::TextUnformatted(localized(language, keyboard::ui_text::TextId::ChatboxText));
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - 154.0f);
        if (m_inputSession.button(localized(language, keyboard::ui_text::TextId::FillChatbox),
                                  ImVec2(154.0f, 30.0f))) {
            resetHangulComposition();
            if (state.composition.active) {
                // 변환 중이면 Enter를 IME에 보내고, 조합 종료와 편집 버퍼 반영 뒤에 전송한다.
                m_submitAfterComposition = m_applicationIsForeground && m_editorFocusArmed &&
                    m_actions.sendKey(keyboard::KeyCode::Enter, false);
                if (!m_submitAfterComposition) {
                    appendLog("Finish the active IME composition in this app before filling Chatbox.");
                }
            } else if (m_actions.submitChatboxText(m_text)) {
                appendLog("OSC request sent to 127.0.0.1:9000: /chatbox/input (send=false).");
            }
        }
        const ImGuiInputSession::EditorInteraction editor = m_inputSession.drawEditor(
            "##chatbox-text", m_text, ImVec2(-FLT_MIN, 78.0f),
            applicationIsForeground || m_focusEditorNextFrame);
        m_focusEditorNextFrame = false;
        // 오버레이 포인터 클릭은 ImGui 편집창만 활성화하고 Windows 전경 포커스는 바꾸지 않는다.
        m_editorFocusArmed = editor.active;
        processHangulInput();
        processKanaInput();
        if (m_submitAfterComposition && !state.composition.active && editor.active) {
            m_submitAfterComposition = false;
            if (m_actions.submitChatboxText(m_text)) {
                appendLog("OSC request sent to 127.0.0.1:9000: /chatbox/input (send=false).");
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::TableNextColumn();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 7.0f));
        ImGui::BeginChild("ime-card", ImVec2(0.0f, 132.0f), ImGuiChildFlags_Borders);
        drawCandidates(state.candidates, state.composition, language);
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::EndTable();
    }

    ImGui::Spacing();
    if (ImGui::BeginTable("keyboard-toolbar", 2,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadOuterX)) {
        ImGui::TableSetupColumn("keyboard-actions", ImGuiTableColumnFlags_WidthStretch, 0.56f);
        ImGui::TableSetupColumn("keyboard-languages", ImGuiTableColumnFlags_WidthStretch, 0.44f);
        ImGui::TableNextColumn();
        if (m_inputSession.button(localized(language, keyboard::ui_text::TextId::FocusInput),
                                  ImVec2(158.0f, 34.0f))) {
            requestEditorFocus();
        }
        ImGui::SameLine();
        const bool overlayVisible = state.overlayVisible;
        const char *toggleLabel = localized(language, overlayVisible
            ? keyboard::ui_text::TextId::HideOverlay
            : keyboard::ui_text::TextId::ShowOverlay);
        if (m_inputSession.button(toggleLabel, ImVec2(210.0f, 34.0f))) {
            overlayVisible ? m_actions.hideOverlay() : m_actions.showOverlay();
        }
        ImGui::SameLine();
        if (m_inputSession.button(localized(language, keyboard::ui_text::TextId::ClearInput),
                                  ImVec2(138.0f, 34.0f))) {
            m_submitAfterComposition = false;
            resetHangulComposition();
            resetKanaComposition();
            m_pendingHangulInput.clear();
            m_pendingKanaInput.clear();
            if (m_compositionCancelCallback) {
                m_compositionCancelCallback();
            }
            m_inputSession.clearEditor(m_text);
            appendLog("Editor cleared.");
        }
        ImGui::TableNextColumn();
        drawInputLanguages(state, language);
        ImGui::EndTable();
    }
    drawKeyboard(state, language);
    if (m_diagnosticsExpanded) {
        ImGui::BeginChild("input-diagnostics", ImVec2(0.0f, 72.0f), ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar);
        for (const keyboard::ControllerButtonState &button : state.controllerButtons) {
            if (button.button == keyboard::ControllerButton::LeftGrip ||
                button.button == keyboard::ControllerButton::RightGrip) {
                ImGui::Text("%s: %s / %s",
                    keyboard::ui_text::controllerButtonLabel(language, button.button).c_str(),
                    button.active ? "bound" : "unbound", button.pressed ? "pressed" : "released");
            }
        }
        for (const std::string &line : m_log) {
            ImGui::TextWrapped("%s", line.c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
    }

    m_settingsUi.draw(state);
    ImGui::EndChild();
    ImGui::End();
    if (m_pointerCursorVisible) {
        // ImGui 기본 마우스 커서는 VR 텍스처에 표시되지 않을 수 있어 포인터 위치를 직접 그린다.
        ImDrawList *foreground = ImGui::GetForegroundDrawList();
        const ImVec2 cursor(static_cast<float>(m_pointerCursorX), static_cast<float>(m_pointerCursorY));
        foreground->AddCircleFilled(cursor, 10.0f, IM_COL32(12, 20, 30, 220), 20);
        foreground->AddCircle(cursor, 10.0f, IM_COL32(245, 250, 255, 245), 20, 2.0f);
        foreground->AddCircleFilled(cursor, 3.0f, IM_COL32(70, 190, 255, 255), 12);
    }
    m_inputSession.endFrame();
}

void KeyboardUi::dispatchPointerEvent(const keyboard::PointerEvent &event) {
    if (!ImGui::GetCurrentContext()) {
        return;
    }
    ImGuiIO &io = ImGui::GetIO();
    // OpenVR 및 Windows 마우스 어댑터가 같은 client pixel 이벤트를 사용해 동일 버튼 흐름으로 처리한다.
    switch (event.type) {
    case keyboard::PointerEventType::Move:
        m_pointerCursorX = event.x;
        m_pointerCursorY = event.y;
        m_pointerCursorVisible = true;
        io.AddMousePosEvent(static_cast<float>(event.x), static_cast<float>(event.y));
        break;
    case keyboard::PointerEventType::Press: {
        m_pointerCursorX = event.x;
        m_pointerCursorY = event.y;
        m_pointerCursorVisible = true;
        io.AddMousePosEvent(static_cast<float>(event.x), static_cast<float>(event.y));
        const int button = mouseButtonIndex(event.button);
        if (button >= 0) {
            io.AddMouseButtonEvent(button, true);
        }
        break;
    }
    case keyboard::PointerEventType::Release: {
        m_pointerCursorX = event.x;
        m_pointerCursorY = event.y;
        m_pointerCursorVisible = true;
        io.AddMousePosEvent(static_cast<float>(event.x), static_cast<float>(event.y));
        const int button = mouseButtonIndex(event.button);
        if (button >= 0) {
            io.AddMouseButtonEvent(button, false);
        }
        break;
    }
    case keyboard::PointerEventType::Leave:
        m_pointerCursorVisible = false;
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        break;
    case keyboard::PointerEventType::Cancel:
        m_pointerCursorVisible = false;
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
        io.AddMouseButtonEvent(ImGuiMouseButton_Right, false);
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        break;
    }
}

bool KeyboardUi::isVirtualControlAt(int x, int y) const {
    return m_inputSession.isVirtualControlAt(x, y);
}

void KeyboardUi::setFocusRequestCallback(FocusRequestCallback callback) {
    m_focusRequestCallback = std::move(callback);
}

void KeyboardUi::requestEditorFocus() {
    std::string error;
    if (!m_focusRequestCallback) {
        appendLog("Focus request failed: no Windows focus adapter is connected.");
        return;
    }
    if (!m_focusRequestCallback(&error)) {
        appendLog(error.empty() ? "Windows did not grant foreground focus." : error);
        return;
    }
    m_editorFocusArmed = true;
    m_focusEditorNextFrame = true;
    appendLog("Focus request succeeded.");
}

void KeyboardUi::setCompositionCancelCallback(std::function<void()> callback) {
    m_compositionCancelCallback = std::move(callback);
}

void KeyboardUi::sendKey(keyboard::KeyCode key, bool withShift) {
    // IME 전환 키만 Windows에 보낸다. 일반 문자는 오버레이 앱의 ImGui 입력란에 직접 넣는다.
    if (!m_applicationIsForeground || !m_editorFocusArmed) {
        appendLog("IME mode key skipped because this app does not own Windows input focus.");
        return;
    }
    m_actions.sendKey(key, withShift);
}

void KeyboardUi::queueVirtualText(const std::string &text) {
    if (text.empty()) {
        return;
    }
    // ImGui 입력 이벤트는 다음 NewFrame에서 편집 위젯에 소비되므로 OS 전경 포커스와 무관하다.
    ImGui::GetIO().AddInputCharactersUTF8(text.c_str());
    m_editorFocusArmed = true;
    m_focusEditorNextFrame = true;
}

void KeyboardUi::queueVirtualKey(ImGuiKey key) {
    if (m_virtualKeyReleaseNextFrame != ImGuiKey_None) {
        ImGui::GetIO().AddKeyEvent(m_virtualKeyReleaseNextFrame, false);
    }
    ImGui::GetIO().AddKeyEvent(key, true);
    m_virtualKeyReleaseNextFrame = key;
    m_editorFocusArmed = true;
    m_focusEditorNextFrame = true;
}

void KeyboardUi::queueHangulJamo(const std::string &jamo) {
    if (jamo.empty()) {
        return;
    }
    m_pendingHangulInput.push_back({PendingHangulInput::Kind::Jamo, jamo});
    m_editorFocusArmed = true;
    m_focusEditorNextFrame = true;
}

void KeyboardUi::queueHangulBackspace() {
    m_pendingHangulInput.push_back({PendingHangulInput::Kind::Backspace, {}});
    m_editorFocusArmed = true;
    m_focusEditorNextFrame = true;
}

void KeyboardUi::processHangulInput() {
    if (m_pendingHangulInput.empty()) {
        return;
    }
    if (!m_editorFocusArmed || !m_inputSession.hasActiveEditorState()) {
        m_focusEditorNextFrame = true;
        return;
    }

    std::size_t consumed = 0;
    for (; consumed < m_pendingHangulInput.size(); ++consumed) {
        const PendingHangulInput &input = m_pendingHangulInput[consumed];
        if (m_hangulComposer.hasActiveComposition() &&
            (!m_hangulRangeActive ||
             !m_inputSession.editorCursorMatchesRange(m_hangulRangeStart, m_hangulRangeEnd))) {
            resetHangulComposition();
        }

        if (input.kind == PendingHangulInput::Kind::Backspace) {
            if (m_hangulComposer.hasActiveComposition()) {
                const keyboard::HangulComposer::Edit edit = m_hangulComposer.backspace();
                if (!applyHangulEdit(edit)) {
                    resetHangulComposition();
                    if (!m_inputSession.backspaceEditor(m_text)) {
                        break;
                    }
                }
            } else {
                m_hangulRangeActive = false;
                if (!m_inputSession.backspaceEditor(m_text)) {
                    break;
                }
            }
            continue;
        }

        const keyboard::HangulComposer::Edit edit = m_hangulComposer.press(input.text);
        if (!applyHangulEdit(edit)) {
            resetHangulComposition();
            int unusedStart = -1;
            int unusedEnd = -1;
            if (!m_inputSession.editEditorText(m_text, -1, -1, input.text, input.text,
                                               unusedStart, unusedEnd)) {
                break;
            }
        }
    }

    m_pendingHangulInput.erase(m_pendingHangulInput.begin(),
                               m_pendingHangulInput.begin() + static_cast<std::ptrdiff_t>(consumed));
    if (!m_pendingHangulInput.empty()) {
        m_focusEditorNextFrame = true;
    }
}

bool KeyboardUi::applyHangulEdit(const keyboard::HangulComposer::Edit &edit) {
    if (edit.action == keyboard::HangulComposer::EditAction::None) {
        return true;
    }
    const int replaceStart = edit.action == keyboard::HangulComposer::EditAction::ReplaceActive
        ? m_hangulRangeStart
        : -1;
    const int replaceEnd = edit.action == keyboard::HangulComposer::EditAction::ReplaceActive
        ? m_hangulRangeEnd
        : -1;
    if (edit.action == keyboard::HangulComposer::EditAction::ReplaceActive && !m_hangulRangeActive) {
        return false;
    }

    int activeStart = -1;
    int activeEnd = -1;
    if (!m_inputSession.editEditorText(m_text, replaceStart, replaceEnd, edit.text,
                                       edit.activeText, activeStart, activeEnd)) {
        return false;
    }
    m_hangulRangeActive = activeStart >= 0 && activeEnd >= activeStart;
    m_hangulRangeStart = m_hangulRangeActive ? activeStart : -1;
    m_hangulRangeEnd = m_hangulRangeActive ? activeEnd : -1;
    return true;
}

void KeyboardUi::resetHangulComposition() {
    m_hangulComposer.commit();
    m_hangulRangeActive = false;
    m_hangulRangeStart = -1;
    m_hangulRangeEnd = -1;
}

void KeyboardUi::queueKanaRoman(char roman) {
    if (roman < 'a' || roman > 'z') {
        return;
    }
    m_pendingKanaInput.push_back({PendingKanaInput::Kind::Roman, roman});
    m_editorFocusArmed = true;
    m_focusEditorNextFrame = true;
}

void KeyboardUi::queueKanaBackspace() {
    m_pendingKanaInput.push_back({PendingKanaInput::Kind::Backspace, '\0'});
    m_editorFocusArmed = true;
    m_focusEditorNextFrame = true;
}

void KeyboardUi::processKanaInput() {
    if (m_pendingKanaInput.empty()) {
        return;
    }
    if (!m_editorFocusArmed || !m_inputSession.hasActiveEditorState()) {
        m_focusEditorNextFrame = true;
        return;
    }

    std::size_t consumed = 0;
    for (; consumed < m_pendingKanaInput.size(); ++consumed) {
        const PendingKanaInput &input = m_pendingKanaInput[consumed];
        if (m_kanaComposer.hasActiveComposition() &&
            (!m_kanaRangeActive ||
             !m_inputSession.editorCursorMatchesRange(m_kanaRangeStart, m_kanaRangeEnd))) {
            resetKanaComposition();
        }

        if (input.kind == PendingKanaInput::Kind::Backspace) {
            if (m_kanaComposer.hasActiveComposition()) {
                const keyboard::KanaComposer::Edit edit = m_kanaComposer.backspace();
                if (!applyKanaEdit(edit)) {
                    resetKanaComposition();
                    if (!m_inputSession.backspaceEditor(m_text)) {
                        break;
                    }
                }
            } else {
                m_kanaRangeActive = false;
                if (!m_inputSession.backspaceEditor(m_text)) {
                    break;
                }
            }
            continue;
        }

        const keyboard::KanaComposer::Edit edit = m_kanaComposer.press(input.roman);
        if (!applyKanaEdit(edit)) {
            resetKanaComposition();
            int unusedStart = -1;
            int unusedEnd = -1;
            if (!m_inputSession.editEditorText(m_text, -1, -1, std::string(1, input.roman),
                                               std::string(1, input.roman), unusedStart, unusedEnd)) {
                break;
            }
        }
    }

    m_pendingKanaInput.erase(m_pendingKanaInput.begin(),
                             m_pendingKanaInput.begin() + static_cast<std::ptrdiff_t>(consumed));
    if (!m_pendingKanaInput.empty()) {
        m_focusEditorNextFrame = true;
    }
}

bool KeyboardUi::applyKanaEdit(const keyboard::KanaComposer::Edit &edit) {
    if (edit.action == keyboard::KanaComposer::EditAction::None) {
        return true;
    }
    const int replaceStart = edit.action == keyboard::KanaComposer::EditAction::ReplaceActive
        ? m_kanaRangeStart
        : -1;
    const int replaceEnd = edit.action == keyboard::KanaComposer::EditAction::ReplaceActive
        ? m_kanaRangeEnd
        : -1;
    if (edit.action == keyboard::KanaComposer::EditAction::ReplaceActive && !m_kanaRangeActive) {
        return false;
    }

    int activeStart = -1;
    int activeEnd = -1;
    if (!m_inputSession.editEditorText(m_text, replaceStart, replaceEnd, edit.text,
                                       edit.activeText, activeStart, activeEnd)) {
        return false;
    }
    m_kanaRangeActive = activeStart >= 0 && activeEnd >= activeStart;
    m_kanaRangeStart = m_kanaRangeActive ? activeStart : -1;
    m_kanaRangeEnd = m_kanaRangeActive ? activeEnd : -1;
    return true;
}

void KeyboardUi::resetKanaComposition() {
    m_kanaComposer.commit();
    m_kanaRangeActive = false;
    m_kanaRangeStart = -1;
    m_kanaRangeEnd = -1;
}

void KeyboardUi::appendLog(std::string message) {
    if (message.empty()) {
        return;
    }
    m_log.push_back(std::move(message));
    if (m_log.size() > 80) {
        m_log.erase(m_log.begin(), m_log.begin() + static_cast<std::ptrdiff_t>(m_log.size() - 80));
    }
}

void KeyboardUi::drawInputLanguages(const keyboard::AppUiState &state,
                                    keyboard::UiLanguage uiLanguage) {
    const std::vector<keyboard::InputLanguage> &languages = state.inputLanguages;
    if (languages.empty()) {
        ImGui::TextDisabled("%s", localized(uiLanguage, keyboard::ui_text::TextId::NoInputLanguages));
    }
    if (!ImGui::BeginTable("input-languages", 3, ImGuiTableFlags_SizingStretchSame)) {
        return;
    }
    struct LanguageButton {
        keyboard::KeyboardLanguage language;
        keyboard::ui_text::TextId text;
    };
    constexpr LanguageButton buttons[] = {
        {keyboard::KeyboardLanguage::Korean, keyboard::ui_text::TextId::KeyboardLanguageKorean},
        {keyboard::KeyboardLanguage::Japanese, keyboard::ui_text::TextId::KeyboardLanguageJapanese},
        {keyboard::KeyboardLanguage::English, keyboard::ui_text::TextId::KeyboardLanguageEnglish},
    };
    for (std::size_t index = 0; index < std::size(buttons); ++index) {
        ImGui::TableNextColumn();
        ImGui::PushID(static_cast<int>(index));
        const bool active = languageButtonIsActive(state, buttons[index].language);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.14f, 0.30f, 0.28f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f, 0.39f, 0.35f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.20f, 0.45f, 0.39f, 1.0f));
        }
        if (m_inputSession.button(localized(uiLanguage, buttons[index].text), ImVec2(-FLT_MIN, 34.0f), active)) {
            resetHangulComposition();
            m_directHangulModeOverrideActive = false;
            resetKanaComposition();
            m_directJapaneseHiraganaModeOverrideActive = false;
            std::string error;
            const keyboard::InputLanguageActivationResult result =
                m_actions.selectKeyboardLanguage(buttons[index].language, &error);
            if (result == keyboard::InputLanguageActivationResult::NotInstalled) {
                m_openMissingInputLanguagePopup = true;
            } else if (result == keyboard::InputLanguageActivationResult::Failed) {
                appendLog(error.empty() ? "Could not change the Windows input mode." : error);
            } else if (!m_applicationIsForeground &&
                       buttons[index].language == keyboard::KeyboardLanguage::Japanese) {
                m_directJapaneseHiraganaModeOverrideActive = true;
            }
        }
        if (active) {
            ImGui::PopStyleColor(3);
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
}

void KeyboardUi::drawMissingInputLanguagePopup(keyboard::UiLanguage uiLanguage) {
    const std::string popupTitle = std::string(localized(
        uiLanguage, keyboard::ui_text::TextId::MissingImeTitle)) + "###missing-input-language";
    if (m_openMissingInputLanguagePopup) {
        ImGui::OpenPopup(popupTitle.c_str());
        m_openMissingInputLanguagePopup = false;
    }
    if (!ImGui::BeginPopupModal(popupTitle.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    ImGui::TextWrapped("%s", localized(uiLanguage, keyboard::ui_text::TextId::MissingImeBody));
    ImGui::Spacing();
    if (m_inputSession.button(localized(uiLanguage, keyboard::ui_text::TextId::Close), ImVec2(120.0f, 36.0f))) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void KeyboardUi::drawCandidates(const keyboard::CandidateSnapshot &snapshot,
                                const keyboard::CompositionSnapshot &composition,
                                keyboard::UiLanguage uiLanguage) {
    const std::string signature = candidateSignature(snapshot);
    if (signature != m_candidateSignature) {
        m_candidateSignature = signature;
        if (snapshot.active) {
            appendLog("TSF candidate list updated: " + std::to_string(snapshot.candidates.size()) +
                      " entries, selected " + std::to_string(snapshot.selectedIndex) + ".");
        }
    }
    ImGui::TextUnformatted(localized(uiLanguage, keyboard::ui_text::TextId::ImePreviewCandidates));
    // 후보가 나타나거나 사라져도 키보드 위치가 바뀌지 않도록 고정 높이를 항상 확보한다.
    if (m_candidateScrollRequested) {
        ImGui::SetNextWindowScroll(ImVec2(m_candidateScrollX, 0.0f));
        m_candidateScrollRequested = false;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
    ImGui::BeginChild("ime-candidates",
                      ImVec2(0.0f, kImePreviewHeight),
                      ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();
    if (composition.active) {
        // 일본어 단어를 변환하는 동안의 문자열은 고정 영역에 표시하고 확정 편집 문자열과 섞지 않는다.
        ImGui::TextUnformatted(composition.preedit.empty()
            ? localized(uiLanguage, keyboard::ui_text::TextId::Composing)
            : composition.preedit.c_str());
    } else {
        ImGui::TextDisabled("%s", localized(uiLanguage, keyboard::ui_text::TextId::ImeCompositionPreview));
    }
    if (!snapshot.active || snapshot.candidates.empty()) {
        ImGui::TextDisabled("%s", localized(uiLanguage, keyboard::ui_text::TextId::ImeCandidatesAppear));
    }
    // 후보는 한 줄로 두고, 별도 가로 스크롤바가 편집창 포커스를 보존하며 위치만 바꾼다.
    const std::size_t candidateCount = snapshot.active ? snapshot.candidates.size() : 0;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.18f, 0.21f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.25f, 0.27f, 1.0f));
    for (std::size_t index = 0; index < candidateCount; ++index) {
        ImGui::PushID(static_cast<int>(index));
        const bool selected = index == snapshot.selectedIndex;
        const std::string &candidate = snapshot.candidates[index];
        const float candidateWidth = std::max(80.0f, ImGui::CalcTextSize(candidate.c_str()).x + 28.0f);
        if (m_inputSession.button(candidate.c_str(), ImVec2(candidateWidth, 32.0f), selected)) {
            m_actions.selectCandidate(static_cast<std::uint32_t>(index));
        }
        ImGui::PopID();
        if (index + 1 < candidateCount) {
            ImGui::SameLine();
        }
    }
    ImGui::PopStyleColor(2);
    const float maxScroll = ImGui::GetScrollMaxX();
    const float visibleWidth = ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2.0f;
    m_candidateScrollX = ImGui::GetScrollX();
    ImGui::EndChild();
    m_candidateScrollRequested = m_inputSession.horizontalScrollbar(
        "##candidate-scroll", ImVec2(-FLT_MIN, 12.0f), maxScroll, visibleWidth, m_candidateScrollX);
}

void KeyboardUi::drawKeyboard(const keyboard::AppUiState &state, keyboard::UiLanguage uiLanguage) {
    const keyboard::InputLanguage *active = activeInputLanguage(state);
    const keyboard::InputLanguageKind language = active
        ? active->kind
        : keyboard::InputLanguageKind::Other;
    if (m_applicationIsForeground || language != keyboard::InputLanguageKind::Korean) {
        m_directHangulModeOverrideActive = false;
    }
    const bool nativeHiraganaMode = language == keyboard::InputLanguageKind::Japanese &&
        state.imeMode.available && state.imeMode.native && state.imeMode.fullShape &&
        !state.imeMode.katakana;
    if ((m_applicationIsForeground && language != keyboard::InputLanguageKind::Japanese) ||
        (m_applicationIsForeground && nativeHiraganaMode)) {
        m_directJapaneseHiraganaModeOverrideActive = false;
    }
    keyboard::KeyboardLayoutKind layout = keyboard::keyboardLayoutFor(language, state.imeMode);
    if (language == keyboard::InputLanguageKind::Korean && m_directHangulModeOverrideActive) {
        layout = m_directHangulModeOverrideEnabled
            ? keyboard::KeyboardLayoutKind::KoreanDubeolsik
            : keyboard::KeyboardLayoutKind::Qwerty;
    }
    const bool useWindowsIme = m_applicationIsForeground && m_editorFocusArmed &&
        state.imeMode.available &&
        (language == keyboard::InputLanguageKind::Korean ||
         language == keyboard::InputLanguageKind::Japanese);
    const bool useDirectHangul = !useWindowsIme &&
        layout == keyboard::KeyboardLayoutKind::KoreanDubeolsik;
    const bool useDirectJapaneseKana = !useWindowsIme &&
        language == keyboard::InputLanguageKind::Japanese &&
        (m_directJapaneseHiraganaModeOverrideActive || nativeHiraganaMode);
    if (!useDirectHangul) {
        resetHangulComposition();
    }
    if (!useDirectJapaneseKana) {
        resetKanaComposition();
    }
    const std::vector<keyboard::KeyboardRow> &rows = keyboard::keyboardRows();
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.125f, 0.169f, 0.216f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.275f, 0.302f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.34f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.255f, 0.306f, 1.0f));
    for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        const keyboard::KeyboardRow &row = rows[rowIndex];
        const bool bottomRow = rowIndex + 1 == rows.size();
        const float keyHeight = bottomRow ? kBottomRowHeight : kKeyHeight;
        const float optionsWidth = bottomRow ? 116.0f : 0.0f;
        const float diagnosticsWidth = bottomRow ? 156.0f : 0.0f;
        float widthUnits = 0.0f;
        for (const keyboard::KeyboardKeyDefinition &key : row) {
            widthUnits += key.widthUnits;
        }
        const float spacingCount = static_cast<float>(row.size() - 1 + (bottomRow ? 2 : 0));
        const float totalSpacing = spacing * spacingCount;
        const float controlWidth = optionsWidth + diagnosticsWidth;
        const float keyWidth = std::min(kKeyWidth,
            std::max(1.0f, (availableWidth - controlWidth - totalSpacing) / widthUnits));
        const float totalWidth = keyWidth * widthUnits + totalSpacing + controlWidth;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (availableWidth - totalWidth) * 0.5f));
        if (bottomRow) {
            if (m_inputSession.button(localized(uiLanguage, keyboard::ui_text::TextId::OpenOptions),
                                      ImVec2(optionsWidth, keyHeight))) {
                m_actions.setOptionsOpen(true);
            }
            ImGui::SameLine(0.0f, spacing);
        }
        for (std::size_t column = 0; column < row.size(); ++column) {
            const keyboard::KeyboardKeyDefinition &key = row[column];
            ImGui::PushID(static_cast<int>(rowIndex));
            ImGui::PushID(static_cast<int>(column));
            const ImVec2 size(keyWidth * key.widthUnits, keyHeight);
            bool activated = false;
            switch (key.kind) {
            case keyboard::KeyboardKeyKind::Character: {
                activated = m_inputSession.button("##keyboard-key", size);
                const ImVec2 minimum = ImGui::GetItemRectMin();
                const ImVec2 maximum = ImGui::GetItemRectMax();
                drawKeyLegend(key, layout, minimum, maximum);
                if (activated) {
                    if (useWindowsIme) {
                        resetHangulComposition();
                        resetKanaComposition();
                        sendKey(key.code, m_shiftForNextKey);
                    } else if (useDirectHangul && key.koreanLabel) {
                        const char *primary = keyboard::primaryKeyLabel(key, layout);
                        const char *shifted = keyboard::secondaryKeyLabel(key, layout);
                        queueHangulJamo(m_shiftForNextKey && shifted && *shifted ? shifted : primary);
                    } else if (useDirectJapaneseKana &&
                               key.code >= keyboard::KeyCode::A &&
                               key.code <= keyboard::KeyCode::Z &&
                               !m_shiftForNextKey && !m_capsLockEnabled) {
                        const char roman = static_cast<char>(
                            'a' + static_cast<int>(key.code) - static_cast<int>(keyboard::KeyCode::A));
                        queueKanaRoman(roman);
                    } else {
                        resetHangulComposition();
                        if (useDirectJapaneseKana) {
                            resetKanaComposition();
                            queueVirtualText(directJapaneseCharacter(
                                key, m_shiftForNextKey, m_capsLockEnabled));
                        } else {
                            resetKanaComposition();
                            queueVirtualText(directVirtualCharacter(
                                key, layout, m_shiftForNextKey, m_capsLockEnabled));
                        }
                    }
                    m_shiftForNextKey = false;
                }
                break;
            }
            case keyboard::KeyboardKeyKind::Backspace:
                if (m_inputSession.button(localized(uiLanguage, keyboard::ui_text::TextId::Backspace), size)) {
                    if (useWindowsIme) {
                        resetHangulComposition();
                        resetKanaComposition();
                        sendKey(key.code);
                    } else if (useDirectHangul) {
                        queueHangulBackspace();
                    } else if (useDirectJapaneseKana) {
                        queueKanaBackspace();
                    } else {
                        resetHangulComposition();
                        resetKanaComposition();
                        queueVirtualKey(ImGuiKey_Backspace);
                    }
                }
                break;
            case keyboard::KeyboardKeyKind::CapsLock:
                if (m_inputSession.button(localized(uiLanguage, keyboard::ui_text::TextId::CapsLock), size)) {
                    if (useWindowsIme) {
                        sendKey(key.code);
                    } else {
                        m_capsLockEnabled = !m_capsLockEnabled;
                    }
                }
                break;
            case keyboard::KeyboardKeyKind::Enter:
                if (m_inputSession.button(localized(uiLanguage, keyboard::ui_text::TextId::Enter), size)) {
                    if (useWindowsIme && state.composition.active) {
                        resetHangulComposition();
                        resetKanaComposition();
                        sendKey(key.code);
                    } else {
                        resetHangulComposition();
                        resetKanaComposition();
                        queueVirtualKey(ImGuiKey_Enter);
                    }
                }
                break;
            case keyboard::KeyboardKeyKind::Shift:
                if (m_inputSession.button(localized(uiLanguage, m_shiftForNextKey
                        ? keyboard::ui_text::TextId::ShiftOn
                        : keyboard::ui_text::TextId::LeftShift), size, m_shiftForNextKey)) {
                    m_shiftForNextKey = !m_shiftForNextKey;
                }
                break;
            case keyboard::KeyboardKeyKind::KoreanMode:
                if (m_inputSession.button(localized(uiLanguage, keyboard::ui_text::TextId::KoreanMode), size,
                                          layout == keyboard::KeyboardLayoutKind::KoreanDubeolsik,
                                          language != keyboard::InputLanguageKind::Japanese)) {
                    resetHangulComposition();
                    if (!m_applicationIsForeground && language == keyboard::InputLanguageKind::Korean) {
                        m_directHangulModeOverrideActive = true;
                        m_directHangulModeOverrideEnabled =
                            layout != keyboard::KeyboardLayoutKind::KoreanDubeolsik;
                    } else {
                        sendKey(key.code);
                    }
                }
                break;
            case keyboard::KeyboardKeyKind::Space:
                if (m_inputSession.button(localized(uiLanguage, keyboard::ui_text::TextId::Space), size)) {
                    if (useWindowsIme && state.composition.active) {
                        resetHangulComposition();
                        resetKanaComposition();
                        sendKey(key.code);
                    } else {
                        resetHangulComposition();
                        resetKanaComposition();
                        queueVirtualText(" ");
                    }
                }
                break;
            case keyboard::KeyboardKeyKind::HiraganaMode:
                if (m_inputSession.button(localized(uiLanguage, keyboard::ui_text::TextId::Hiragana), size,
                                          useDirectJapaneseKana || nativeHiraganaMode,
                                          language != keyboard::InputLanguageKind::Korean)) {
                    resetHangulComposition();
                    resetKanaComposition();
                    if (language == keyboard::InputLanguageKind::Japanese && !useWindowsIme) {
                        m_directJapaneseHiraganaModeOverrideActive = true;
                        appendLog("Local Japanese Hiragana mode enabled. Kanji conversion requires Windows IME focus.");
                    } else {
                        sendKey(key.code);
                    }
                }
                break;
            }
            ImGui::PopID();
            ImGui::PopID();
            if (column + 1 < row.size()) {
                ImGui::SameLine(0.0f, spacing);
            }
        }
        if (bottomRow) {
            ImGui::SameLine(0.0f, spacing);
            if (m_inputSession.button(localized(uiLanguage, keyboard::ui_text::TextId::InputDiagnostics),
                                      ImVec2(diagnosticsWidth, keyHeight))) {
                m_diagnosticsExpanded = !m_diagnosticsExpanded;
            }
        }
    }
    ImGui::PopStyleColor(4);
}

void KeyboardUi::addImeFonts() {
    ImGuiIO &io = ImGui::GetIO();
    ImFontAtlas *fonts = io.Fonts;
    ImFontConfig baseConfig{};
    if (std::filesystem::exists("C:/Windows/Fonts/segoeui.ttf")) {
        ImFont *baseFont = fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf",
                                                     kFontSize,
                                                     &baseConfig,
                                                     fonts->GetGlyphRangesDefault());
        if (baseFont) {
            io.FontDefault = baseFont;
        }
    }

    ImFontConfig mergeConfig{};
    mergeConfig.MergeMode = true;
    mergeConfig.PixelSnapH = true;
    // Windows 기본 글꼴에 포함되지 않을 수 있는 한국어·일본어·중국어 글리프를 시스템 글꼴에서 합친다.
    const char *koreanFontPath = "C:/Windows/Fonts/malgun.ttf";
    if (std::filesystem::exists(koreanFontPath)) {
        fonts->AddFontFromFileTTF(koreanFontPath, kFontSize, &mergeConfig, fonts->GetGlyphRangesKorean());
    }
    const char *japaneseFontPath = "C:/Windows/Fonts/meiryo.ttc";
    if (std::filesystem::exists(japaneseFontPath)) {
        fonts->AddFontFromFileTTF(japaneseFontPath, kFontSize, &mergeConfig, fonts->GetGlyphRangesJapanese());
    }
    const char *chineseFontPath = "C:/Windows/Fonts/msyh.ttc";
    if (std::filesystem::exists(chineseFontPath)) {
        fonts->AddFontFromFileTTF(chineseFontPath, kFontSize, &mergeConfig, fonts->GetGlyphRangesChineseFull());
    }
}
