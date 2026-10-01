#pragma once

#include "../../core/app_contracts.h"

#include <Windows.h>

#include <functional>
#include <string>

// Windows IMM 메시지를 조합 미리보기와 확정 문자열로 나눈다. ImGui/TSF 후보 구현에는 의존하지 않는다.
class WindowsImeComposition final {
public:
    using CompositionCallback = std::function<void(const keyboard::CompositionSnapshot &)>;
    using CommitCallback = std::function<void(const std::string &)>;

    void setCompositionCallback(CompositionCallback callback);
    void setCommitCallback(CommitCallback callback);
    bool processMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam, LRESULT &result);
    void cancel(HWND window);
    bool isComposing() const;

private:
    void publish();

    keyboard::CompositionSnapshot m_composition;
    CompositionCallback m_compositionCallback;
    CommitCallback m_commitCallback;
};
