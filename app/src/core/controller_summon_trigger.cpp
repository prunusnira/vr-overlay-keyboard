#include "controller_summon_trigger.h"

#include <algorithm>
#include <cmath>

namespace keyboard {

bool validateAppSettings(const AppSettings &settings, std::string *error) {
    const auto fail = [error](const char *message) {
        if (error) {
            *error = message;
        }
        return false;
    };

    switch (settings.uiLanguage) {
    case UiLanguage::Korean:
    case UiLanguage::Japanese:
    case UiLanguage::English:
        break;
    default:
        return fail("Unknown application language.");
    }

    if (!std::isfinite(settings.pointerOffsetXPercent) ||
        !std::isfinite(settings.pointerOffsetYPercent) ||
        settings.pointerOffsetXPercent < -10.0f || settings.pointerOffsetXPercent > 10.0f ||
        settings.pointerOffsetYPercent < -10.0f || settings.pointerOffsetYPercent > 10.0f) {
        return fail("Pointer offsets must be between -10 and 10 percent.");
    }

    if (settings.summonHoldMilliseconds > 3000) {
        return fail("Controller summon hold time must be between 0 and 3000 milliseconds.");
    }
    if (settings.summonButtons.empty()) {
        return fail("Choose at least one controller button for keyboard summon.");
    }

    for (std::size_t index = 0; index < settings.summonButtons.size(); ++index) {
        const ControllerButton button = settings.summonButtons[index];
        if (std::find(kControllerButtons.begin(), kControllerButtons.end(), button) == kControllerButtons.end()) {
            return fail("Controller summon settings contain an unsupported button.");
        }
        if (std::find(settings.summonButtons.begin(), settings.summonButtons.begin() + index, button) !=
            settings.summonButtons.begin() + index) {
            return fail("Controller summon settings contain a duplicate button.");
        }
    }
    return true;
}

bool ControllerSummonTrigger::update(const std::vector<ControllerButtonState> &states,
                                     const AppSettings &settings,
                                     Clock::time_point now) {
    bool allPressed = !settings.summonButtons.empty();
    for (const ControllerButton required : settings.summonButtons) {
        const auto found = std::find_if(states.begin(), states.end(), [required](const ControllerButtonState &state) {
            return state.button == required;
        });
        // SteamVR 입력 액션이 없거나 바인딩되지 않은 버튼은 눌림으로 오인하지 않는다.
        if (found == states.end() || !found->active || !found->pressed) {
            allPressed = false;
            break;
        }
    }

    if (!allPressed) {
        // 한 버튼이라도 놓이면 새 조합 입력을 받을 수 있도록 유지 타이머와 1회 제한을 푼다.
        reset();
        return false;
    }
    if (m_firedUntilRelease || m_waitingForRelease) {
        return false;
    }
    if (!m_pressedSince) {
        m_pressedSince = now;
    }

    const auto heldFor = now - *m_pressedSince;
    const auto requiredDuration = std::chrono::milliseconds(settings.summonHoldMilliseconds);
    if (heldFor < requiredDuration) {
        return false;
    }

    m_firedUntilRelease = true;
    return true;
}

void ControllerSummonTrigger::reset(bool waitForRelease) {
    m_pressedSince.reset();
    m_firedUntilRelease = false;
    m_waitingForRelease = waitForRelease;
}

} // namespace keyboard
