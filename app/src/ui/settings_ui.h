#pragma once

#include "../core/app_contracts.h"
#include "imgui_input_session.h"

class SettingsUi final {
public:
    SettingsUi(keyboard::KeyboardActions &actions, ImGuiInputSession &inputSession);
    void draw(const keyboard::AppUiState &state);

private:
    keyboard::KeyboardActions &m_actions;
    ImGuiInputSession &m_inputSession;
};
