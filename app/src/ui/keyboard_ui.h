#pragma once

#include "../core/app_contracts.h"
#include "imgui_input_session.h"
#include "settings_ui.h"

#include <functional>
#include <string>
#include <vector>

class KeyboardUi final {
public:
    // Windows foreground 전환은 HWND를 아는 조립 지점에 위임한다.
    using FocusRequestCallback = std::function<bool(std::string *)>;

    explicit KeyboardUi(keyboard::KeyboardActions &actions);

    void draw(const keyboard::AppUiState &state, bool applicationIsForeground);
    // OpenVR/Windows 어댑터의 공용 픽셀 좌표 이벤트를 ImGui 입력 큐로 넘긴다.
    void dispatchPointerEvent(const keyboard::PointerEvent &event);
    bool isVirtualControlAt(int x, int y) const;
    void setFocusRequestCallback(FocusRequestCallback callback);
    void setCompositionCancelCallback(std::function<void()> callback);

private:
    void requestEditorFocus();
    void sendKey(keyboard::KeyCode key, bool withShift = false);
    void appendLog(std::string message);
    void drawInputLanguages(const std::vector<keyboard::InputLanguage> &languages,
                            keyboard::UiLanguage uiLanguage);
    void drawCandidates(const keyboard::CandidateSnapshot &snapshot,
                        const keyboard::CompositionSnapshot &composition,
                        keyboard::UiLanguage uiLanguage);
    void drawKeyboard(keyboard::UiLanguage uiLanguage);
    void addImeFonts();

    keyboard::KeyboardActions &m_actions;
    ImGuiInputSession m_inputSession;
    SettingsUi m_settingsUi;
    FocusRequestCallback m_focusRequestCallback;
    std::function<void()> m_compositionCancelCallback;
    std::string m_text;
    std::string m_candidateSignature;
    std::string m_lastStatus;
    std::vector<std::string> m_log;
    bool m_applicationIsForeground = false;
    bool m_editorFocusArmed = false;
    bool m_focusEditorNextFrame = false;
    bool m_shiftForNextKey = false;
    bool m_diagnosticsExpanded = false;
    bool m_pointerCursorVisible = false;
    int m_pointerCursorX = -1;
    int m_pointerCursorY = -1;
    float m_candidateScrollX = 0.0f;
    bool m_candidateScrollRequested = false;
    bool m_submitAfterComposition = false;
};
