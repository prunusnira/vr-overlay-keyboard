#include "keyboard_application.h"

#include <algorithm>
#include <utility>

namespace keyboard {

KeyboardApplication::KeyboardApplication(OverlayControlPort &overlay,
                                         InputLanguagePort &languages,
                                         ImeModePort &imeModes,
                                         VirtualKeyPort &virtualKeys,
                                         CandidateSelectionPort &candidates,
                                         ChatboxPort &chatbox,
                                         SettingsPort &settings)
    : m_overlay(overlay),
      m_languages(languages),
      m_imeModes(imeModes),
      m_virtualKeys(virtualKeys),
      m_candidates(candidates),
      m_chatbox(chatbox),
      m_settings(settings) {
    // 구체 구현은 생성자로 주입해 코어가 Windows나 OpenVR SDK에 의존하지 않게 한다.
    m_state.overlayVisible = m_overlay.isVisible();
    std::string error;
    AppSettings loadedSettings;
    if (m_settings.load(&loadedSettings, &error) && validateAppSettings(loadedSettings, &error)) {
        m_state.settings = std::move(loadedSettings);
    } else {
        // 설정 파일이 없거나 읽을 수 없을 때는 안전한 기본 조합으로 시작한다.
        m_state.settings = AppSettings{};
        if (!error.empty()) {
            m_state.status = "Settings could not be loaded; defaults restored: " + error;
        }
    }
}

void KeyboardApplication::setStateChangedCallback(StateChangedCallback callback) {
    m_stateChangedCallback = std::move(callback);
    publish();
}

const AppUiState &KeyboardApplication::state() const {
    return m_state;
}

void KeyboardApplication::refresh() {
    // 화면에 오래된 상태가 남지 않도록 외부 어댑터의 현재값을 다시 읽는다.
    m_state.overlayVisible = m_overlay.isVisible();
    m_state.inputLanguages = m_languages.loadedLanguages();
    m_state.imeMode = m_imeModes.currentMode();
    publish();
}

void KeyboardApplication::setCandidates(const CandidateSnapshot &snapshot) {
    m_state.candidates = snapshot;
    publish();
}

void KeyboardApplication::setComposition(const CompositionSnapshot &snapshot) {
    m_state.composition = snapshot;
    publish();
}

void KeyboardApplication::updateControllerButtons(const std::vector<ControllerButtonState> &buttons) {
    m_state.controllerButtons = buttons;
    if (m_summonTrigger.update(buttons, m_state.settings) && !m_overlay.isVisible()) {
        // 호출 조합은 토글이 아니라 숨겨진 키보드를 표시하는 단방향 동작이다.
        showOverlay();
    }
}

void KeyboardApplication::setStatus(const std::string &message) {
    m_state.status = message;
    publish();
}

bool KeyboardApplication::toggleOverlay() {
    // UI 버튼과 SteamVR 액션이 동일한 표시·숨김 경로를 사용한다.
    return m_overlay.isVisible() ? hideOverlay() : showOverlay();
}

bool KeyboardApplication::showOverlay() {
    if (m_overlay.isVisible()) {
        m_state.overlayVisible = true;
        return true;
    }

    std::string error;
    const bool succeeded = m_overlay.show(&error);
    if (!succeeded) {
        setFailure(error);
        return false;
    }

    m_state.overlayVisible = true;
    m_state.status = "Keyboard overlay shown.";
    publish();
    return true;
}

bool KeyboardApplication::hideOverlay() {
    if (!m_overlay.isVisible()) {
        m_state.overlayVisible = false;
        return true;
    }

    std::string error;
    if (!m_overlay.hide(&error)) {
        setFailure(error);
        return false;
    }

    m_state.overlayVisible = false;
    m_state.status = "Keyboard overlay hidden.";
    publish();
    return true;
}

bool KeyboardApplication::setOptionsOpen(bool open) {
    m_state.optionsOpen = open;
    publish();
    return true;
}

bool KeyboardApplication::applySettings(const AppSettings &settings) {
    std::string error;
    if (!validateAppSettings(settings, &error)) {
        setFailure(error);
        return false;
    }
    if (!m_settings.save(settings, &error)) {
        setFailure(error);
        return false;
    }

    m_state.settings = settings;
    // 조합 중 옵션을 바꿔도 이미 눌린 상태로 새 설정이 즉시 발화하지 않게 해제 입력을 기다린다.
    m_summonTrigger.reset(true);
    m_state.status = "Settings saved.";
    publish();
    return true;
}

bool KeyboardApplication::sendKey(KeyCode key, bool withShift) {
    std::string error;
    if (!m_virtualKeys.send(key, withShift, &error)) {
        setFailure(error);
        return false;
    }
    return true;
}

InputLanguageActivationResult KeyboardApplication::selectKeyboardLanguage(
    KeyboardLanguage language, std::string *errorOut) {
    std::string failure;
    if (language != KeyboardLanguage::English) {
        const InputLanguageActivationResult activation = m_languages.activate(language, &failure);
        if (activation != InputLanguageActivationResult::Activated) {
            if (errorOut) {
                *errorOut = failure;
            }
            setFailure(failure);
            return activation;
        }
    } else {
        m_state.inputLanguages = m_languages.loadedLanguages();
        const auto active = std::find_if(m_state.inputLanguages.begin(),
                                         m_state.inputLanguages.end(),
                                         [](const InputLanguage &entry) { return entry.active; });
        if (active != m_state.inputLanguages.end() &&
            active->kind == InputLanguageKind::English) {
            m_state.status = "English input mode selected.";
            refresh();
            return InputLanguageActivationResult::Activated;
        }
    }

    if (!m_imeModes.setMode(language, &failure)) {
        if (errorOut) {
            *errorOut = failure;
        }
        setFailure(failure);
        return InputLanguageActivationResult::Failed;
    }
    m_state.status = language == KeyboardLanguage::Korean
        ? "Korean input mode selected."
        : language == KeyboardLanguage::Japanese
            ? "Japanese Kana input mode selected."
            : "English input mode selected.";
    refresh();
    return InputLanguageActivationResult::Activated;
}

bool KeyboardApplication::selectCandidate(std::uint32_t index) {
    std::string error;
    if (!m_candidates.selectCandidate(index, &error)) {
        setFailure(error);
        return false;
    }
    m_state.status = "Candidate selection requested.";
    publish();
    return true;
}

bool KeyboardApplication::submitChatboxText(const std::string &utf8Text) {
    std::string error;
    if (!m_chatbox.sendChatboxText(utf8Text, &error)) {
        setFailure(error);
        return false;
    }
    m_state.status = "Text sent to the VRChat Chatbox input field.";
    publish();
    return true;
}

void KeyboardApplication::publish() {
    if (m_stateChangedCallback) {
        m_stateChangedCallback(m_state);
    }
}

void KeyboardApplication::setFailure(const std::string &message) {
    // 실패 문구를 한 상태 경로로 게시해 UI와 진단 로그가 같은 결과를 보게 한다.
    m_state.status = message.empty() ? "The requested action failed." : message;
    publish();
}

} // namespace keyboard
