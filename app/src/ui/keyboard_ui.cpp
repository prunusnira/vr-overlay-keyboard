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
constexpr float kFontSize = 18.0f;
constexpr float kKeyHeight = 42.0f;
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
    const ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);
    if (secondary && *secondary) {
        drawList->AddText(ImGui::GetFont(), 13.0f,
                          ImVec2(minimum.x + 5.0f, minimum.y + 3.0f), textColor, secondary);
    }
    const ImVec2 labelSize = ImGui::CalcTextSize(primary);
    const ImVec2 labelPosition((minimum.x + maximum.x - labelSize.x) * 0.5f,
                               (minimum.y + maximum.y - labelSize.y) * 0.5f + 2.0f);
    drawList->AddText(labelPosition, textColor, primary);
}
}

KeyboardUi::KeyboardUi(keyboard::KeyboardActions &actions)
    : m_actions(actions), m_settingsUi(actions, m_inputSession) {
    addImeFonts();
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(12.0f, 8.0f);
    style.FramePadding = ImVec2(8.0f, 5.0f);
    style.ItemSpacing = ImVec2(6.0f, 4.0f);
    style.WindowRounding = 0.0f;
    style.FrameRounding = 6.0f;
    style.ChildRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.075f, 0.09f, 0.12f, 1.0f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.045f, 0.055f, 0.075f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.18f, 0.23f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.25f, 0.34f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.16f, 0.21f, 0.29f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.34f, 0.5f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.12f, 0.38f, 0.72f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.12f, 0.31f, 0.54f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.16f, 0.39f, 0.66f, 1.0f);
    appendLog("Ready. Input focus is retained while this app is active.");
}

void KeyboardUi::draw(const keyboard::AppUiState &state, bool applicationIsForeground) {
    m_inputSession.beginFrame();
    m_applicationIsForeground = applicationIsForeground;
    if (state.status != m_lastStatus && !state.status.empty()) {
        m_lastStatus = state.status;
        appendLog(state.status);
    }

    const ImGuiIO &io = ImGui::GetIO();
    const keyboard::UiLanguage language = state.settings.uiLanguage;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    // 자동 편집 포커스를 복원해도 전체 화면 창이 옵션 창을 덮지 않도록 표시 순서를 유지한다.
    constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("VR Overlay Keyboard##main", nullptr, windowFlags);
    ImGui::BeginChild("main-content", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);

    ImGui::TextUnformatted(localized(language, keyboard::ui_text::TextId::WindowTitle));
    ImGui::SameLine();
    constexpr const char *copyright = "(c) Studio Nira 2026";
    const float copyrightWidth = ImGui::CalcTextSize(copyright).x;
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),
                                  ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - copyrightWidth));
    ImGui::TextUnformatted(copyright);
    ImGui::TextWrapped("%s: %s", localized(language, keyboard::ui_text::TextId::Status), state.status.c_str());
    const keyboard::InputLanguage *activeLanguage = activeInputLanguage(state);
    ImGui::Text("%s: %s  |  %s: %s  |  %s: %s",
                localized(language, keyboard::ui_text::TextId::WindowsForeground),
                localized(language, m_applicationIsForeground
                    ? keyboard::ui_text::TextId::ThisApp
                    : keyboard::ui_text::TextId::AnotherApp),
                localized(language, keyboard::ui_text::TextId::EditorFocus),
                localized(language, m_editorFocusArmed
                    ? keyboard::ui_text::TextId::Ready
                    : keyboard::ui_text::TextId::NotSelected),
                localized(language, keyboard::ui_text::TextId::CurrentInputLanguage),
                activeLanguage ? activeLanguage->label.c_str()
                               : localized(language, keyboard::ui_text::TextId::Detecting));

    drawMissingInputLanguagePopup(language);

    if (ImGui::BeginTable("chatbox-ime-row", 2,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX)) {
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(localized(language, keyboard::ui_text::TextId::ChatboxText));
        ImGui::SameLine();
        if (m_inputSession.button(localized(language, keyboard::ui_text::TextId::FillChatbox),
                                  ImVec2(180.0f, 30.0f))) {
            if (state.composition.active) {
                // 변환 중이면 Enter를 IME에 보내고, 조합 종료와 편집 버퍼 반영 뒤에 전송한다.
                m_submitAfterComposition = m_editorFocusArmed &&
                    m_actions.sendKey(keyboard::KeyCode::Enter, false);
            } else if (m_actions.submitChatboxText(m_text)) {
                appendLog("OSC request sent to 127.0.0.1:9000: /chatbox/input (send=false).");
            }
        }
        const ImGuiInputSession::EditorInteraction editor = m_inputSession.drawEditor(
            "##chatbox-text", m_text, ImVec2(-FLT_MIN, 64.0f),
            applicationIsForeground || m_focusEditorNextFrame);
        m_focusEditorNextFrame = false;
        // VR 포인터 입력은 OS 포커스를 바꾸지 않으므로 편집창을 누를 때 실제 창도 전경으로 요청한다.
        if (editor.clicked) {
            requestEditorFocus();
        }
        m_editorFocusArmed = editor.active;
        if (m_submitAfterComposition && !state.composition.active && editor.active) {
            m_submitAfterComposition = false;
            if (m_actions.submitChatboxText(m_text)) {
                appendLog("OSC request sent to 127.0.0.1:9000: /chatbox/input (send=false).");
            }
        }
        ImGui::TableNextColumn();
        drawCandidates(state.candidates, state.composition, language);
        ImGui::EndTable();
    }

    if (ImGui::BeginTable("keyboard-toolbar", 2,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadOuterX)) {
        ImGui::TableSetupColumn("keyboard-actions", ImGuiTableColumnFlags_WidthStretch, 0.56f);
        ImGui::TableSetupColumn("keyboard-languages", ImGuiTableColumnFlags_WidthStretch, 0.44f);
        ImGui::TableNextColumn();
        if (m_inputSession.button(localized(language, keyboard::ui_text::TextId::FocusInput),
                                  ImVec2(158.0f, 36.0f))) {
            requestEditorFocus();
        }
        ImGui::SameLine();
        const bool overlayVisible = state.overlayVisible;
        const char *toggleLabel = localized(language, overlayVisible
            ? keyboard::ui_text::TextId::HideOverlay
            : keyboard::ui_text::TextId::ShowOverlay);
        if (m_inputSession.button(toggleLabel, ImVec2(210.0f, 36.0f))) {
            overlayVisible ? m_actions.hideOverlay() : m_actions.showOverlay();
        }
        ImGui::SameLine();
        if (m_inputSession.button(localized(language, keyboard::ui_text::TextId::ClearInput),
                                  ImVec2(138.0f, 36.0f))) {
            m_submitAfterComposition = false;
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
        ImGui::BeginChild("input-diagnostics", ImVec2(0.0f, 95.0f), ImGuiChildFlags_Borders,
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

    ImGui::EndChild();
    ImGui::End();
    // 옵션 창도 같은 ImGui 프레임에 그려 데스크톱과 OpenVR 오버레이에 함께 보낸다.
    m_settingsUi.draw(state);
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
    // 편집창 활성 여부는 UI에서 확인하고, 현재 전경 프로세스는 SendInput 직전에 Windows 어댑터가 검사한다.
    if (!m_editorFocusArmed) {
        appendLog("Key blocked: click the input field or Focus input before sending keys.");
        return;
    }
    // 가상 버튼은 입력창의 ActiveId를 보존하므로 매 키 입력 뒤 재포커스하지 않는다.
    m_actions.sendKey(key, withShift);
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
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.34f, 0.59f, 1.0f));
        }
        if (m_inputSession.button(localized(uiLanguage, buttons[index].text), ImVec2(-FLT_MIN, 36.0f), active)) {
            std::string error;
            const keyboard::InputLanguageActivationResult result =
                m_actions.selectKeyboardLanguage(buttons[index].language, &error);
            if (result == keyboard::InputLanguageActivationResult::NotInstalled) {
                m_openMissingInputLanguagePopup = true;
            } else if (result == keyboard::InputLanguageActivationResult::Failed) {
                appendLog(error.empty() ? "Could not change the Windows input mode." : error);
            }
        }
        if (active) {
            ImGui::PopStyleColor();
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
    for (std::size_t index = 0; index < candidateCount; ++index) {
        ImGui::PushID(static_cast<int>(index));
        const bool selected = index == snapshot.selectedIndex;
        const std::string &candidate = snapshot.candidates[index];
        const float candidateWidth = std::max(80.0f, ImGui::CalcTextSize(candidate.c_str()).x + 28.0f);
        if (m_inputSession.button(candidate.c_str(), ImVec2(candidateWidth, 42.0f), selected)) {
            m_actions.selectCandidate(static_cast<std::uint32_t>(index));
        }
        ImGui::PopID();
        if (index + 1 < candidateCount) {
            ImGui::SameLine();
        }
    }
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
    const keyboard::KeyboardLayoutKind layout = keyboard::keyboardLayoutFor(language, state.imeMode);
    const std::vector<keyboard::KeyboardRow> &rows = keyboard::keyboardRows();
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        const keyboard::KeyboardRow &row = rows[rowIndex];
        const bool bottomRow = rowIndex + 1 == rows.size();
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
                                      ImVec2(optionsWidth, kKeyHeight))) {
                m_actions.setOptionsOpen(true);
            }
            ImGui::SameLine(0.0f, spacing);
        }
        for (std::size_t column = 0; column < row.size(); ++column) {
            const keyboard::KeyboardKeyDefinition &key = row[column];
            ImGui::PushID(static_cast<int>(rowIndex));
            ImGui::PushID(static_cast<int>(column));
            const ImVec2 size(keyWidth * key.widthUnits, kKeyHeight);
            bool activated = false;
            switch (key.kind) {
            case keyboard::KeyboardKeyKind::Character: {
                activated = m_inputSession.button("##keyboard-key", size);
                const ImVec2 minimum = ImGui::GetItemRectMin();
                const ImVec2 maximum = ImGui::GetItemRectMax();
                drawKeyLegend(key, layout, minimum, maximum);
                if (activated) {
                    sendKey(key.code, m_shiftForNextKey);
                    m_shiftForNextKey = false;
                }
                break;
            }
            case keyboard::KeyboardKeyKind::Backspace:
                if (m_inputSession.button(localized(uiLanguage, keyboard::ui_text::TextId::Backspace), size)) {
                    sendKey(key.code);
                }
                break;
            case keyboard::KeyboardKeyKind::CapsLock:
                if (m_inputSession.button(localized(uiLanguage, keyboard::ui_text::TextId::CapsLock), size)) {
                    sendKey(key.code);
                }
                break;
            case keyboard::KeyboardKeyKind::Enter:
                if (m_inputSession.button(localized(uiLanguage, keyboard::ui_text::TextId::Enter), size)) {
                    sendKey(key.code);
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
                                          false, language != keyboard::InputLanguageKind::Japanese)) {
                    sendKey(key.code);
                }
                break;
            case keyboard::KeyboardKeyKind::Space:
                if (m_inputSession.button(localized(uiLanguage, keyboard::ui_text::TextId::Space), size)) {
                    sendKey(key.code);
                }
                break;
            case keyboard::KeyboardKeyKind::HiraganaMode:
                if (m_inputSession.button(localized(uiLanguage, keyboard::ui_text::TextId::Hiragana), size,
                                          false, language != keyboard::InputLanguageKind::Korean)) {
                    sendKey(key.code);
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
                                      ImVec2(diagnosticsWidth, kKeyHeight))) {
                m_diagnosticsExpanded = !m_diagnosticsExpanded;
            }
        }
    }
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
