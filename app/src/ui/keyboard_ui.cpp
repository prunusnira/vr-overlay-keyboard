#include "keyboard_ui.h"

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
constexpr float kKeyWidth = 70.0f;
constexpr float kImePreviewHeight = 96.0f;

keyboard::KeyCode letterKey(char letter) {
    return static_cast<keyboard::KeyCode>(
        static_cast<int>(keyboard::KeyCode::A) + static_cast<int>(letter - 'A'));
}

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

}

KeyboardUi::KeyboardUi(keyboard::KeyboardActions &actions) : m_actions(actions) {
    addImeFonts();
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(18.0f, 14.0f);
    style.FramePadding = ImVec2(10.0f, 8.0f);
    style.ItemSpacing = ImVec2(8.0f, 7.0f);
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
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("VR Overlay Keyboard##main", nullptr, windowFlags);
    ImGui::BeginChild("main-content", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);

    ImGui::Text("VR Overlay Keyboard");
    ImGui::TextWrapped("Type with the active Windows IME. Bind SteamVR toggle, pointer pose, and click actions to use the overlay in VR.");
    ImGui::Separator();

    ImGui::TextUnformatted("Chatbox text");
    const ImGuiInputSession::EditorInteraction editor = m_inputSession.drawEditor(
        "##chatbox-text", m_text, ImVec2(-1.0f, 90.0f),
        applicationIsForeground || m_focusEditorNextFrame);
    m_focusEditorNextFrame = false;
    // VR 포인터 입력은 OS 마우스 포커스를 바꾸지 않으므로, 편집창을 누를 때 실제 창도 전경으로 요청한다.
    if (editor.clicked) {
        requestEditorFocus();
    }
    m_editorFocusArmed = editor.active;
    if (m_submitAfterComposition && !state.composition.active && editor.active) {
        // IME 확정 문자가 편집창 입력 큐에 반영된 뒤 OSC로 보낸다. 조합 중 이전 문장을 보내지 않는다.
        m_submitAfterComposition = false;
        if (m_actions.submitChatboxText(m_text)) {
            appendLog("OSC request sent to 127.0.0.1:9000: /chatbox/input (send=false).");
        }
    }

    ImGui::Spacing();
    ImGui::BeginGroup();
    if (m_inputSession.button("Focus input", ImVec2(150.0f, 42.0f))) {
        requestEditorFocus();
    }
    ImGui::SameLine();
    const char *toggleLabel = state.overlayVisible ? "Hide keyboard overlay" : "Show keyboard overlay";
    if (m_inputSession.button(toggleLabel, ImVec2(210.0f, 42.0f))) {
        m_actions.toggleOverlay();
    }
    ImGui::SameLine();
    if (m_inputSession.button("Clear input", ImVec2(150.0f, 42.0f))) {
        m_submitAfterComposition = false;
        if (m_compositionCancelCallback) {
            m_compositionCancelCallback();
        }
        m_inputSession.clearEditor(m_text);
        appendLog("Editor cleared.");
    }
    ImGui::SameLine();
    if (m_inputSession.button("Fill VRChat Chatbox", ImVec2(210.0f, 42.0f))) {
        if (state.composition.active) {
            // 변환 중이면 Enter를 IME에 보내고, 다음 프레임부터 조합 종료와 편집 버퍼 반영을 기다린다.
            m_submitAfterComposition = m_editorFocusArmed &&
                m_actions.sendKey(keyboard::KeyCode::Enter, false);
        } else if (m_actions.submitChatboxText(m_text)) {
            appendLog("OSC request sent to 127.0.0.1:9000: /chatbox/input (send=false).");
        }
    }
    ImGui::EndGroup();

    ImGui::Spacing();
    ImGui::TextWrapped("Status: %s", state.status.c_str());
    ImGui::Text("Windows foreground: %s  |  Editor focus: %s",
                m_applicationIsForeground ? "this app" : "another app",
                m_editorFocusArmed ? "ready" : "not selected");

    ImGui::Spacing();
    drawInputLanguages(state.inputLanguages);
    drawCandidates(state.candidates, state.composition);
    drawKeyboard();

    if (m_inputSession.button("Input diagnostics", ImVec2(-FLT_MIN, 28.0f))) {
        m_diagnosticsExpanded = !m_diagnosticsExpanded;
    }
    if (m_diagnosticsExpanded) {
        ImGui::BeginChild("input-diagnostics", ImVec2(0.0f, 95.0f), ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar);
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
        io.AddMousePosEvent(static_cast<float>(event.x), static_cast<float>(event.y));
        break;
    case keyboard::PointerEventType::Press: {
        io.AddMousePosEvent(static_cast<float>(event.x), static_cast<float>(event.y));
        const int button = mouseButtonIndex(event.button);
        if (button >= 0) {
            io.AddMouseButtonEvent(button, true);
        }
        break;
    }
    case keyboard::PointerEventType::Release: {
        io.AddMousePosEvent(static_cast<float>(event.x), static_cast<float>(event.y));
        const int button = mouseButtonIndex(event.button);
        if (button >= 0) {
            io.AddMouseButtonEvent(button, false);
        }
        break;
    }
    case keyboard::PointerEventType::Leave:
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        break;
    case keyboard::PointerEventType::Cancel:
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

void KeyboardUi::drawInputLanguages(const std::vector<keyboard::InputLanguage> &languages) {
    ImGui::TextUnformatted("Windows input language");
    const auto activeLanguage = std::find_if(languages.begin(), languages.end(), [](const keyboard::InputLanguage &language) {
        return language.active;
    });
    if (activeLanguage == languages.end()) {
        ImGui::TextUnformatted("Current input language: detecting...");
    } else {
        ImGui::Text("Current input language: %s", activeLanguage->label.c_str());
    }

    if (languages.empty()) {
        ImGui::TextUnformatted("No loaded Windows input languages were found.");
        return;
    }
    if (!ImGui::BeginTable("input-languages", 3, ImGuiTableFlags_SizingStretchSame)) {
        return;
    }
    for (std::size_t index = 0; index < languages.size(); ++index) {
        const keyboard::InputLanguage &language = languages[index];
        ImGui::TableNextColumn();
        ImGui::PushID(static_cast<int>(index));
        if (language.active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.34f, 0.59f, 1.0f));
        }
        if (m_inputSession.button(language.label.c_str(), ImVec2(-FLT_MIN, 36.0f))) {
            m_actions.activateInputLanguage(language.id);
        }
        if (language.active) {
            ImGui::PopStyleColor();
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
}

void KeyboardUi::drawCandidates(const keyboard::CandidateSnapshot &snapshot,
                                const keyboard::CompositionSnapshot &composition) {
    const std::string signature = candidateSignature(snapshot);
    if (signature != m_candidateSignature) {
        m_candidateSignature = signature;
        if (snapshot.active) {
            appendLog("TSF candidate list updated: " + std::to_string(snapshot.candidates.size()) +
                      " entries, selected " + std::to_string(snapshot.selectedIndex) + ".");
        }
    }
    ImGui::Separator();
    ImGui::TextUnformatted("IME conversion preview / candidates");
    // 후보가 나타나거나 사라져도 키보드 위치가 바뀌지 않도록 고정 높이를 항상 확보한다.
    if (m_candidateScrollRequested) {
        ImGui::SetNextWindowScroll(ImVec2(m_candidateScrollX, 0.0f));
        m_candidateScrollRequested = false;
    }
    ImGui::BeginChild("ime-candidates",
                      ImVec2(0.0f, kImePreviewHeight),
                      ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollbar);
    if (composition.active) {
        // 일본어 단어를 변환하는 동안의 문자열은 고정 영역에 표시하고 확정 편집 문자열과 섞지 않는다.
        ImGui::TextUnformatted(composition.preedit.empty() ? "Composing..." : composition.preedit.c_str());
    } else {
        ImGui::TextDisabled("IME composition preview");
    }
    ImGui::Separator();
    if (!snapshot.active || snapshot.candidates.empty()) {
        ImGui::TextDisabled("IME candidates appear here when available.");
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
        "##candidate-scroll", ImVec2(-FLT_MIN, 14.0f), maxScroll, visibleWidth, m_candidateScrollX);
}

void KeyboardUi::drawKeyboard() {
    ImGui::Separator();
    ImGui::TextUnformatted("Keyboard");
    ImGui::TextWrapped("Korean composition and IME candidates still need validation with the target Windows input methods.");

    const char *rows[] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
    for (std::size_t row = 0; row < std::size(rows); ++row) {
        const std::string keys(rows[row]);
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float totalWidth = static_cast<float>(keys.size()) * kKeyWidth +
            static_cast<float>(keys.size() - 1) * spacing;
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), (availableWidth - totalWidth) * 0.5f));
        for (std::size_t column = 0; column < keys.size(); ++column) {
            ImGui::PushID(static_cast<int>(row * 16 + column));
            const char label[] = {keys[column], '\0'};
            if (m_inputSession.button(label, ImVec2(kKeyWidth, kKeyHeight))) {
                sendKey(letterKey(keys[column]), m_shiftForNextKey);
                if (m_shiftForNextKey) {
                    m_shiftForNextKey = false;
                }
            }
            ImGui::PopID();
            if (column + 1 < keys.size()) {
                ImGui::SameLine(0.0f, spacing);
            }
        }
    }

    ImGui::Spacing();
    if (m_inputSession.button(m_shiftForNextKey ? "Shift (on)" : "Shift", ImVec2(120.0f, kKeyHeight))) {
        m_shiftForNextKey = !m_shiftForNextKey;
    }
    ImGui::SameLine();
    if (m_inputSession.button("Backspace", ImVec2(150.0f, kKeyHeight))) {
        sendKey(keyboard::KeyCode::Backspace);
    }
    ImGui::SameLine();
    if (m_inputSession.button("한글 모드", ImVec2(130.0f, kKeyHeight))) {
        sendKey(keyboard::KeyCode::HangulMode);
    }
    ImGui::SameLine();
    if (m_inputSession.button("Space", ImVec2(170.0f, kKeyHeight))) {
        sendKey(keyboard::KeyCode::Space);
    }
    ImGui::SameLine();
    if (m_inputSession.button("Enter", ImVec2(120.0f, kKeyHeight))) {
        sendKey(keyboard::KeyCode::Enter);
    }
    ImGui::SameLine();
    if (m_inputSession.button("かな", ImVec2(90.0f, kKeyHeight))) {
        sendKey(keyboard::KeyCode::JapaneseHiraganaMode);
        appendLog("Requested Japanese Hiragana mode.");
    }
    ImGui::SameLine();
    if (m_inputSession.button("漢字", ImVec2(90.0f, kKeyHeight))) {
        sendKey(keyboard::KeyCode::JapaneseKanjiMode);
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
