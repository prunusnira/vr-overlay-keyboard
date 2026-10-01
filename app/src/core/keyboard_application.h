#pragma once

#include "app_contracts.h"
#include "controller_summon_trigger.h"

namespace keyboard {

class KeyboardApplication final : public KeyboardActions {
public:
    using StateChangedCallback = std::function<void(const AppUiState &)>;

    KeyboardApplication(OverlayControlPort &overlay,
                        InputLanguagePort &languages,
                        VirtualKeyPort &virtualKeys,
                        CandidateSelectionPort &candidates,
                        ChatboxPort &chatbox,
                        SettingsPort &settings);

    void setStateChangedCallback(StateChangedCallback callback);
    const AppUiState &state() const;
    void refresh();
    void setCandidates(const CandidateSnapshot &snapshot);
    void setComposition(const CompositionSnapshot &snapshot);
    void updateControllerButtons(const std::vector<ControllerButtonState> &buttons);
    void setStatus(const std::string &message);

    bool toggleOverlay() override;
    bool showOverlay() override;
    bool hideOverlay() override;
    bool setOptionsOpen(bool open) override;
    bool applySettings(const AppSettings &settings) override;
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
    SettingsPort &m_settings;
    ControllerSummonTrigger m_summonTrigger;
    AppUiState m_state;
    StateChangedCallback m_stateChangedCallback;
};

} // namespace keyboard
