#include "keyboard_application.h"

#include <utility>

namespace keyboard {

KeyboardApplication::KeyboardApplication(OverlayControlPort &overlay,
                                         InputLanguagePort &languages,
                                         VirtualKeyPort &virtualKeys,
                                         CandidateSelectionPort &candidates,
                                         ChatboxPort &chatbox)
    : m_overlay(overlay),
      m_languages(languages),
      m_virtualKeys(virtualKeys),
      m_candidates(candidates),
      m_chatbox(chatbox) {
    m_state.overlayVisible = m_overlay.isVisible();
}

void KeyboardApplication::setStateChangedCallback(StateChangedCallback callback) {
    m_stateChangedCallback = std::move(callback);
    publish();
}

const AppUiState &KeyboardApplication::state() const {
    return m_state;
}

void KeyboardApplication::refresh() {
    m_state.overlayVisible = m_overlay.isVisible();
    m_state.inputLanguages = m_languages.loadedLanguages();
    publish();
}

void KeyboardApplication::setCandidates(const CandidateSnapshot &snapshot) {
    m_state.candidates = snapshot;
    publish();
}

void KeyboardApplication::setStatus(const std::string &message) {
    m_state.status = message;
    publish();
}

bool KeyboardApplication::toggleOverlay() {
    std::string error;
    const bool succeeded = m_overlay.isVisible()
        ? m_overlay.hide(&error)
        : m_overlay.show(&error);
    if (!succeeded) {
        setFailure(error);
        return false;
    }

    m_state.overlayVisible = m_overlay.isVisible();
    m_state.status = m_state.overlayVisible ? "Keyboard overlay shown." : "Keyboard overlay hidden.";
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

bool KeyboardApplication::activateInputLanguage(const std::string &languageId) {
    std::string error;
    if (!m_languages.activate(languageId, &error)) {
        setFailure(error);
        return false;
    }
    m_state.status = "Input language changed.";
    refresh();
    return true;
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
    m_state.status = message.empty() ? "The requested action failed." : message;
    publish();
}

} // namespace keyboard
