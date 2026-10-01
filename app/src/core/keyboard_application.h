#pragma once

#include "app_contracts.h"

namespace keyboard {

class KeyboardApplication final : public KeyboardActions {
public:
    using StateChangedCallback = std::function<void(const AppUiState &)>;

    KeyboardApplication(OverlayControlPort &overlay,
                        InputLanguagePort &languages,
                        VirtualKeyPort &virtualKeys,
                        CandidateSelectionPort &candidates,
                        ChatboxPort &chatbox);

    void setStateChangedCallback(StateChangedCallback callback);
    const AppUiState &state() const;
    void refresh();
    void setCandidates(const CandidateSnapshot &snapshot);
    void setStatus(const std::string &message);

    bool toggleOverlay() override;
    bool sendKey(KeyCode key, bool withShift) override;
    bool activateInputLanguage(const std::string &languageId) override;
    bool selectCandidate(std::uint32_t index) override;
    bool submitChatboxText(const std::string &utf8Text) override;

private:
    void publish();
    void setFailure(const std::string &message);

    OverlayControlPort &m_overlay;
    InputLanguagePort &m_languages;
    VirtualKeyPort &m_virtualKeys;
    CandidateSelectionPort &m_candidates;
    ChatboxPort &m_chatbox;
    AppUiState m_state;
    StateChangedCallback m_stateChangedCallback;
};

} // namespace keyboard
