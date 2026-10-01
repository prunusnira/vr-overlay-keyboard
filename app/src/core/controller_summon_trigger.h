#pragma once

#include "app_contracts.h"

#include <chrono>
#include <optional>
#include <vector>

namespace keyboard {

class ControllerSummonTrigger final {
public:
    using Clock = std::chrono::steady_clock;

    // 모든 선택 버튼의 동시 누름과 설정된 유지 시간이 충족되면 한 번만 true를 반환한다.
    bool update(const std::vector<ControllerButtonState> &states,
                const AppSettings &settings,
                Clock::time_point now = Clock::now());
    void reset(bool waitForRelease = false);

private:
    std::optional<Clock::time_point> m_pressedSince;
    bool m_firedUntilRelease = false;
    bool m_waitingForRelease = true;
};

} // namespace keyboard
