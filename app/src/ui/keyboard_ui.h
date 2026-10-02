#pragma once

#include "../core/app_contracts.h"
#include "../core/hangul_composer.h"
#include "../core/kana_composer.h"
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
    struct PendingHangulInput {
        enum class Kind {
            Jamo,
            Backspace,
        } kind = Kind::Jamo;
        std::string text;
    };

    struct PendingKanaInput {
        enum class Kind {
            Roman,
            Backspace,
        } kind = Kind::Roman;
        char roman = '\0';
    };

    void requestEditorFocus();
    void sendKey(keyboard::KeyCode key, bool withShift = false);
    void queueVirtualText(const std::string &text);
    void queueVirtualKey(ImGuiKey key);
    void queueHangulJamo(const std::string &jamo);
    void queueHangulBackspace();
    void processHangulInput();
    bool applyHangulEdit(const keyboard::HangulComposer::Edit &edit);
    void resetHangulComposition();
    void queueKanaRoman(char roman);
    void queueKanaBackspace();
    void processKanaInput();
    bool applyKanaEdit(const keyboard::KanaComposer::Edit &edit);
    void resetKanaComposition();
    void appendLog(std::string message);
    void drawInputLanguages(const keyboard::AppUiState &state, keyboard::UiLanguage uiLanguage);
    void drawMissingInputLanguagePopup(keyboard::UiLanguage uiLanguage);
    void drawCandidates(const keyboard::CandidateSnapshot &snapshot,
                        const keyboard::CompositionSnapshot &composition,
                        keyboard::UiLanguage uiLanguage);
    void drawKeyboard(const keyboard::AppUiState &state, keyboard::UiLanguage uiLanguage);
    void addImeFonts();

    keyboard::KeyboardActions &m_actions;
    ImGuiInputSession m_inputSession;
    keyboard::HangulComposer m_hangulComposer;
    keyboard::KanaComposer m_kanaComposer;
    SettingsUi m_settingsUi;
    FocusRequestCallback m_focusRequestCallback;
    std::function<void()> m_compositionCancelCallback;
    std::string m_text;
    std::string m_candidateSignature;
    std::string m_lastStatus;
    std::vector<std::string> m_log;
    std::vector<PendingHangulInput> m_pendingHangulInput;
    std::vector<PendingKanaInput> m_pendingKanaInput;
    bool m_applicationIsForeground = false;
    bool m_editorFocusArmed = false;
    bool m_focusEditorNextFrame = false;
    ImGuiKey m_virtualKeyReleaseNextFrame = ImGuiKey_None;
    bool m_shiftForNextKey = false;
    bool m_capsLockEnabled = false;
    bool m_directHangulModeOverrideActive = false;
    bool m_directHangulModeOverrideEnabled = false;
    bool m_directJapaneseHiraganaModeOverrideActive = false;
    bool m_hangulRangeActive = false;
    int m_hangulRangeStart = -1;
    int m_hangulRangeEnd = -1;
    bool m_kanaRangeActive = false;
    int m_kanaRangeStart = -1;
    int m_kanaRangeEnd = -1;
    bool m_openMissingInputLanguagePopup = false;
    bool m_diagnosticsExpanded = false;
    bool m_pointerCursorVisible = false;
    int m_pointerCursorX = -1;
    int m_pointerCursorY = -1;
    float m_candidateScrollX = 0.0f;
    bool m_candidateScrollRequested = false;
    bool m_submitAfterComposition = false;
};
