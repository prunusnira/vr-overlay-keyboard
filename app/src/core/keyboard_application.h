#pragma once

#include "app_contracts.h"
#include "controller_summon_trigger.h"

namespace keyboard {

class KeyboardApplication final : public KeyboardActions {
public:
    KeyboardApplication(OverlayControlPort &overlay,
                        InputLanguagePort &languages,
                        ImeModePort &imeModes,
                        VirtualKeyPort &virtualKeys,
                        CandidateSelectionPort &candidates,
                        ChatboxPort &chatbox,
                        SettingsPort &settings);

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
    InputLanguageActivationResult selectKeyboardLanguage(KeyboardLanguage language,
                                                         std::string *error) override;
    bool selectCandidate(std::uint32_t index) override;
    bool submitChatboxText(const std::string &utf8Text) override;

private:
    void setFailure(const std::string &message);

    OverlayControlPort &m_overlay;
    InputLanguagePort &m_languages;
    ImeModePort &m_imeModes;
    VirtualKeyPort &m_virtualKeys;
    CandidateSelectionPort &m_candidates;
    ChatboxPort &m_chatbox;
    SettingsPort &m_settings;
    ControllerSummonTrigger m_summonTrigger;
    AppUiState m_state;
};

} // namespace keyboard
