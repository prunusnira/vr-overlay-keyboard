#include "virtual_mouse_router.h"

#include <utility>

namespace {
thread_local WindowsVirtualMouseRouter *g_threadRouter = nullptr;
}

WindowsVirtualMouseRouter::~WindowsVirtualMouseRouter() {
    shutdown();
}

bool WindowsVirtualMouseRouter::initialize(HWND window, HitTestCallback hitTest,
                                         PointerCallback pointer, std::string *error) {
    shutdown();
    if (!window || !IsWindow(window) || !hitTest || !pointer || g_threadRouter ||
        GetWindowThreadProcessId(window, nullptr) != GetCurrentThreadId()) {
        if (error) {
            *error = "The virtual mouse routing callbacks or window are not available.";
        }
        return false;
    }
    m_window = window;
    m_hitTest = std::move(hitTest);
    m_pointer = std::move(pointer);
    // thread id를 지정하므로 다른 앱의 마우스 입력에는 이 훅이 실행되지 않는다.
    m_hook = SetWindowsHookExW(WH_MOUSE, &WindowsVirtualMouseRouter::mouseHook, nullptr, GetCurrentThreadId());
    if (!m_hook) {
        if (error) {
            *error = "Virtual mouse routing could not be installed (Windows error " +
                std::to_string(GetLastError()) + ").";
        }
        shutdown();
        return false;
    }
    g_threadRouter = this;
    return true;
}

void WindowsVirtualMouseRouter::shutdown() {
    cancel();
    if (m_hook) {
        UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
    }
    if (g_threadRouter == this) {
        g_threadRouter = nullptr;
    }
    m_window = nullptr;
    m_hitTest = {};
    m_pointer = {};
}

void WindowsVirtualMouseRouter::bringHookToFront() {
    if (!m_hook) {
        return;
    }
    // 언어 전환/조합 시작 때 IME가 새 훅을 등록할 수 있으므로 이때만 처리 순서를 다시 확보한다.
    // 새 훅 설치가 실패하면 기존 훅을 유지한다. 매 프레임 재설치하지 않는다.
    if (const HHOOK replacement = SetWindowsHookExW(
            WH_MOUSE, &WindowsVirtualMouseRouter::mouseHook, nullptr, GetCurrentThreadId())) {
        UnhookWindowsHookEx(m_hook);
        m_hook = replacement;
    }
}

void WindowsVirtualMouseRouter::pollCancellation() {
    // OS 마우스 capture를 만들지 않는다. 창 밖에서 놓친 Release나 앱 전환은 취소로 처리한다.
    if (m_ownsGesture && (GetForegroundWindow() != m_window ||
                         (GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0)) {
        cancel();
    }
}

void WindowsVirtualMouseRouter::cancel() {
    if (m_ownsGesture) {
        m_ownsGesture = false;
        emit(keyboard::PointerEventType::Cancel, POINT{-1, -1});
    }
}

void WindowsVirtualMouseRouter::emit(keyboard::PointerEventType type, const POINT &point) {
    if (m_pointer) {
        m_pointer({type, keyboard::PointerButton::Left, point.x, point.y});
    }
}

LRESULT CALLBACK WindowsVirtualMouseRouter::mouseHook(int code, WPARAM message, LPARAM data) {
    WindowsVirtualMouseRouter *router = g_threadRouter;
    if (router && code >= 0 && data &&
        router->route(code, static_cast<UINT>(message), *reinterpret_cast<const MOUSEHOOKSTRUCT *>(data))) {
        // 해당 가상 버튼 이벤트만 소비한다. IME 훅/일반 WndProc에 중복 전달하지 않는다.
        return 1;
    }
    return CallNextHookEx(nullptr, code, message, data);
}

bool WindowsVirtualMouseRouter::route(int code, UINT message, const MOUSEHOOKSTRUCT &mouse) {
    POINT point = mouse.pt;
    if (!ScreenToClient(m_window, &point)) {
        return false;
    }
    const bool leftPress = message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK;
    // Virtual UI controls must remain clickable after switching to Japanese or English layouts.
    const bool startsVirtualGesture = leftPress && mouse.hwnd == m_window && mouse.wHitTestCode == HTCLIENT &&
        GetForegroundWindow() == m_window && m_hitTest(point.x, point.y);
    const bool continuesVirtualGesture = m_ownsGesture &&
        (message == WM_MOUSEMOVE || message == WM_LBUTTONUP);
    if (!startsVirtualGesture && !continuesVirtualGesture) {
        return false;
    }
    if (code == HC_NOREMOVE) {
        // PM_NOREMOVE 조회에는 중복 Press/Release를 만들지 않는다. 실제 큐 제거 시에만 이벤트를 낸다.
        return true;
    }
    if (code != HC_ACTION) {
        return false;
    }
    if (startsVirtualGesture) {
        m_ownsGesture = true;
        emit(keyboard::PointerEventType::Press, point);
    } else if (message == WM_LBUTTONUP) {
        m_ownsGesture = false;
        emit(keyboard::PointerEventType::Release, point);
    } else {
        emit(keyboard::PointerEventType::Move, point);
    }
    return true;
}
