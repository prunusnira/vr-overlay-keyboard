#include "settings_ui.h"

#include "ui_text_catalog.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <iterator>
#include <string>

namespace {
const char *localized(keyboard::UiLanguage language, keyboard::ui_text::TextId id) {
    return keyboard::ui_text::text(language, id);
}
}

SettingsUi::SettingsUi(keyboard::KeyboardActions &actions, ImGuiInputSession &inputSession)
    : m_actions(actions), m_inputSession(inputSession) {}

void SettingsUi::draw(const keyboard::AppUiState &state) {
    if (!state.optionsOpen) {
        return;
    }

    const ImGuiIO &io = ImGui::GetIO();
    const float width = std::max(320.0f, std::min(430.0f, io.DisplaySize.x - 24.0f));
    const float height = std::max(360.0f, std::min(820.0f, io.DisplaySize.y - 24.0f));
    ImGui::SetNextWindowPos(ImVec2(std::max(12.0f, io.DisplaySize.x - width - 12.0f), 12.0f),
                            ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Appearing);
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("Options##settings-window", nullptr, flags);

    keyboard::AppSettings edited = state.settings;
    bool settingsChanged = false;
    bool rejectedEmptySelection = false;
    const keyboard::UiLanguage language = state.settings.uiLanguage;

    ImGui::TextUnformatted(localized(language, keyboard::ui_text::TextId::OptionsTitle));
    ImGui::SameLine();
    const float closeWidth = 88.0f;
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowWidth() - closeWidth - 22.0f));
    if (m_inputSession.button(localized(language, keyboard::ui_text::TextId::Close), ImVec2(closeWidth, 30.0f))) {
        m_actions.setOptionsOpen(false);
    }
    ImGui::Separator();

    ImGui::TextUnformatted(localized(language, keyboard::ui_text::TextId::AppLanguage));
    constexpr keyboard::UiLanguage languages[] = {
        keyboard::UiLanguage::Korean,
        keyboard::UiLanguage::Japanese,
        keyboard::UiLanguage::English,
    };
    for (std::size_t index = 0; index < std::size(languages); ++index) {
        ImGui::PushID(static_cast<int>(index));
        if (index > 0) {
            ImGui::SameLine();
        }
        if (m_inputSession.button(keyboard::ui_text::languageName(languages[index]),
                                  ImVec2(112.0f, 36.0f),
                                  edited.uiLanguage == languages[index])) {
            edited.uiLanguage = languages[index];
            settingsChanged = true;
        }
        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted(localized(language, keyboard::ui_text::TextId::SummonButtons));
    if (ImGui::BeginTable("summon-buttons", 2, ImGuiTableFlags_SizingStretchSame)) {
        for (std::size_t index = 0; index < keyboard::kControllerButtons.size(); ++index) {
            const keyboard::ControllerButton button = keyboard::kControllerButtons[index];
            const auto found = std::find(edited.summonButtons.begin(), edited.summonButtons.end(), button);
            const bool selected = found != edited.summonButtons.end();
            const std::string label = keyboard::ui_text::controllerButtonLabel(language, button);
            ImGui::TableNextColumn();
            ImGui::PushID(static_cast<int>(index));
            if (m_inputSession.button(label.c_str(), ImVec2(-FLT_MIN, 34.0f), selected)) {
                if (selected && edited.summonButtons.size() == 1) {
                    rejectedEmptySelection = true;
                } else if (selected) {
                    edited.summonButtons.erase(found);
                    settingsChanged = true;
                } else {
                    edited.summonButtons.push_back(button);
                    settingsChanged = true;
                }
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (rejectedEmptySelection) {
        ImGui::TextWrapped("%s", localized(language, keyboard::ui_text::TextId::AtLeastOneButton));
    }

    const bool bindingsReady = !edited.summonButtons.empty() &&
        std::all_of(edited.summonButtons.begin(), edited.summonButtons.end(), [&state](keyboard::ControllerButton button) {
            const auto found = std::find_if(state.controllerButtons.begin(), state.controllerButtons.end(),
                [button](const keyboard::ControllerButtonState &sample) {
                    return sample.button == button;
                });
            return found != state.controllerButtons.end() && found->active;
        });
    ImGui::TextWrapped("%s", localized(language, bindingsReady
        ? keyboard::ui_text::TextId::BindingsReady
        : keyboard::ui_text::TextId::BindingsMissing));
    ImGui::TextWrapped("%s", localized(language, keyboard::ui_text::TextId::BindingInstructions));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("%s: %.1f s",
                localized(language, keyboard::ui_text::TextId::HoldDuration),
                static_cast<float>(edited.summonHoldMilliseconds) / 1000.0f);
    ImGui::SameLine();
    if (m_inputSession.button("−", ImVec2(42.0f, 34.0f)) && edited.summonHoldMilliseconds >= 100) {
        edited.summonHoldMilliseconds -= 100;
        settingsChanged = true;
    }
    ImGui::SameLine();
    if (m_inputSession.button("+", ImVec2(42.0f, 34.0f)) && edited.summonHoldMilliseconds <= 2900) {
        edited.summonHoldMilliseconds += 100;
        settingsChanged = true;
    }
    ImGui::TextWrapped("%s", localized(language, keyboard::ui_text::TextId::HoldRange));

    ImGui::End();
    if (settingsChanged) {
        // 설정은 사용자 단위 파일에 즉시 저장하므로 앱을 다시 켜도 선택값을 유지한다.
        m_actions.applySettings(edited);
    }
}
