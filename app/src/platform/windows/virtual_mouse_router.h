#pragma once

#include "../../core/app_contracts.h"

#include <Windows.h>

#include <functional>
#include <string>

// 한국어 IME가 가상 버튼의 실제 마우스 클릭으로 조합을 확정하기 전에 UI 포인터 이벤트로 전환한다.
// 훅은 앱 UI 스레드에만 설치하며, 키보드 화면의 등록된 가상 컨트롤 영역만 처리한다.
class WindowsVirtualMouseRouter final {
public:
    using HitTestCallback = std::function<bool(int, int)>;
    using PointerCallback = std::function<void(const keyboard::PointerEvent &)>;

    WindowsVirtualMouseRouter() = default;
    ~WindowsVirtualMouseRouter();
    WindowsVirtualMouseRouter(const WindowsVirtualMouseRouter &) = delete;
    WindowsVirtualMouseRouter &operator=(const WindowsVirtualMouseRouter &) = delete;
    bool initialize(HWND window, HitTestCallback hitTest, PointerCallback pointer, std::string *error);
    void shutdown();
    void bringHookToFront();
    void pollCancellation();
    void cancel();

private:
    static LRESULT CALLBACK mouseHook(int code, WPARAM message, LPARAM data);
    bool route(int code, UINT message, const MOUSEHOOKSTRUCT &mouse);
    void emit(keyboard::PointerEventType type, const POINT &point);

    HWND m_window = nullptr;
    HHOOK m_hook = nullptr;
    HitTestCallback m_hitTest;
    PointerCallback m_pointer;
    bool m_ownsGesture = false;
};
