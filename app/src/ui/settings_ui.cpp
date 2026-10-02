#include "settings_ui.h"

#include "ui_text_catalog.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
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

    const ImVec2 parentPosition = ImGui::GetWindowPos();
    const ImVec2 panelSize = ImGui::GetWindowSize();
    ImGui::SetCursorScreenPos(parentPosition);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("Options##settings-window", panelSize, ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    // 옵션은 메인 창의 포커스를 빼앗지 않는 child overlay로 표시해 IME 세션을 보존한다.

    keyboard::AppSettings edited = state.settings;
    bool settingsChanged = false;
    bool rejectedEmptySelection = false;
    const keyboard::UiLanguage language = state.settings.uiLanguage;

    const ImVec2 headerPosition = ImGui::GetCursorScreenPos();
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    const ImVec2 windowMinimum = ImGui::GetWindowPos();
    const ImVec2 windowMaximum(windowMinimum.x + ImGui::GetWindowWidth(),
                               windowMinimum.y + ImGui::GetWindowHeight());
    drawList->AddRectFilledMultiColor(windowMinimum, windowMaximum,
        IM_COL32(20, 29, 39, 255), IM_COL32(13, 20, 28, 255),
        IM_COL32(13, 20, 28, 255), IM_COL32(17, 25, 34, 255));
    drawList->AddRectFilled(headerPosition,
        ImVec2(headerPosition.x + 34.0f, headerPosition.y + 34.0f), IM_COL32(107, 218, 192, 255), 9.0f);
    drawList->AddText(ImGui::GetFont(), 14.0f,
        ImVec2(headerPosition.x + 10.0f, headerPosition.y + 8.0f), IM_COL32(8, 30, 25, 255), "N");
    drawList->AddText(ImGui::GetFont(), 17.0f,
        ImVec2(headerPosition.x + 46.0f, headerPosition.y), IM_COL32(237, 244, 247, 255),
        localized(language, keyboard::ui_text::TextId::OptionsTitle));
    drawList->AddText(ImGui::GetFont(), 10.0f,
        ImVec2(headerPosition.x + 46.0f, headerPosition.y + 23.0f),
        IM_COL32(101, 119, 132, 255), "CONTROLLER  /  INPUT SETTINGS");
    const float closeWidth = 96.0f;
    ImGui::SetCursorScreenPos(ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth() -
        ImGui::GetStyle().WindowPadding.x - closeWidth, headerPosition.y + 3.0f));
    if (m_inputSession.button(localized(language, keyboard::ui_text::TextId::Close), ImVec2(closeWidth, 30.0f))) {
        m_actions.setOptionsOpen(false);
    }
    ImGui::SetCursorScreenPos(ImVec2(headerPosition.x, headerPosition.y + 39.0f));
    ImGui::Separator();

    const float scrollRegionHeight = ImGui::GetContentRegionAvail().y;
    const float bodyWidth = std::min(860.0f, ImGui::GetContentRegionAvail().x);
    ImGui::SetCursorPosX(ImGui::GetStyle().WindowPadding.x +
        std::max(0.0f, (ImGui::GetContentRegionAvail().x - bodyWidth) * 0.5f));
    float maxScrollY = 0.0f;
    float visibleHeight = 1.0f;
    if (ImGui::BeginTable("options-scroll-layout", 2,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadOuterX,
                          ImVec2(bodyWidth, scrollRegionHeight))) {
        ImGui::TableSetupColumn("options-content", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("options-scrollbar", ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableNextColumn();
        ImGui::SetNextWindowScroll(ImVec2(0.0f, m_scrollY));
        ImGui::BeginChild("options-scroll-content", ImVec2(-FLT_MIN, -FLT_MIN),
                          ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);

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
    ImGui::TextUnformatted(localized(language, keyboard::ui_text::TextId::PointerPositionAdjustment));
    ImGui::TextUnformatted(localized(language, keyboard::ui_text::TextId::PointerHorizontalOffset));
    if (m_inputSession.sliderFloat("##pointer-offset-x", ImVec2(-FLT_MIN, 30.0f),
                                   edited.pointerOffsetXPercent, -10.0f, 10.0f, "%.1f%%")) {
        settingsChanged = true;
    }
    ImGui::TextUnformatted(localized(language, keyboard::ui_text::TextId::PointerVerticalOffset));
    if (m_inputSession.sliderFloat("##pointer-offset-y", ImVec2(-FLT_MIN, 30.0f),
                                   edited.pointerOffsetYPercent, -10.0f, 10.0f, "%.1f%%")) {
        settingsChanged = true;
    }
    ImGui::TextWrapped("%s", localized(language, keyboard::ui_text::TextId::PointerOffsetHint));

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
    float holdMilliseconds = static_cast<float>(edited.summonHoldMilliseconds);
    ImGui::SameLine();
    if (m_inputSession.button("−", ImVec2(42.0f, 34.0f)) && edited.summonHoldMilliseconds >= 100) {
        edited.summonHoldMilliseconds -= 100;
        holdMilliseconds = static_cast<float>(edited.summonHoldMilliseconds);
        settingsChanged = true;
    }
    ImGui::SameLine();
    if (m_inputSession.sliderFloat("##summon-hold-duration", ImVec2(-50.0f, 34.0f),
                                   holdMilliseconds, 0.0f, 3000.0f, "%.0f ms")) {
        edited.summonHoldMilliseconds = static_cast<std::uint32_t>(std::lround(holdMilliseconds));
        settingsChanged = true;
    }
    ImGui::SameLine();
    if (m_inputSession.button("+", ImVec2(42.0f, 34.0f)) && edited.summonHoldMilliseconds <= 2900) {
        edited.summonHoldMilliseconds += 100;
        settingsChanged = true;
    }
    ImGui::TextWrapped("%s", localized(language, keyboard::ui_text::TextId::HoldRange));

        maxScrollY = ImGui::GetScrollMaxY();
        visibleHeight = std::max(1.0f,
            ImGui::GetWindowHeight() - ImGui::GetStyle().WindowPadding.y * 2.0f);
        m_scrollY = ImGui::GetScrollY();
        ImGui::EndChild();
        ImGui::TableNextColumn();
        m_inputSession.verticalScrollbar("##options-scrollbar", ImVec2(-FLT_MIN, -FLT_MIN),
                                        maxScrollY, visibleHeight, m_scrollY);
        ImGui::EndTable();
    }

    ImGui::EndChild();
    if (settingsChanged) {
        // 설정은 사용자 단위 파일에 즉시 저장하므로 앱을 다시 켜도 선택값을 유지한다.
        m_actions.applySettings(edited);
    }
}
