#include "virtual_key_sender.h"

#include <Windows.h>

#include <array>

namespace {
WORD toVirtualKey(keyboard::KeyCode key) {
    if (key >= keyboard::KeyCode::A && key <= keyboard::KeyCode::Z) {
        return static_cast<WORD>('A' + static_cast<int>(key) - static_cast<int>(keyboard::KeyCode::A));
    }
    switch (key) {
    case keyboard::KeyCode::Backspace: return VK_BACK;
    case keyboard::KeyCode::Space: return VK_SPACE;
    case keyboard::KeyCode::Enter: return VK_RETURN;
    case keyboard::KeyCode::HangulMode: return VK_HANGUL;
    case keyboard::KeyCode::JapaneseHiraganaMode: return VK_IME_ON;
    case keyboard::KeyCode::JapaneseKanjiMode: return VK_KANJI;
    default: return 0;
    }
}

bool usesPhysicalScanCode(WORD virtualKey) {
    return (virtualKey >= 'A' && virtualKey <= 'Z') ||
           virtualKey == VK_BACK || virtualKey == VK_SPACE || virtualKey == VK_RETURN;
}

void appendKey(std::array<INPUT, 4> &inputs, UINT &count, WORD virtualKey, DWORD flags, bool scanCode) {
    INPUT &input = inputs[count++];
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = scanCode ? 0 : virtualKey;
    input.ki.wScan = static_cast<WORD>(MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC));
    input.ki.dwFlags = flags | (scanCode ? KEYEVENTF_SCANCODE : 0);
    input.ki.time = 0;
    input.ki.dwExtraInfo = 0;
}
}

bool WindowsVirtualKeySender::send(keyboard::KeyCode key, bool withShift, std::string *error) const {
    const HWND foregroundWindow = GetForegroundWindow();
    DWORD foregroundProcessId = 0;
    if (foregroundWindow) {
        GetWindowThreadProcessId(foregroundWindow, &foregroundProcessId);
    }
    if (foregroundProcessId != GetCurrentProcessId()) {
        if (error) {
            *error = "Key input is blocked because this application is not the Windows foreground process.";
        }
        return false;
    }

    // SendInput은 현재 포커스 창으로 전달되므로 다른 앱에 키를 보내지 않도록 전경 프로세스를 제한한다.
    const WORD virtualKey = toVirtualKey(key);
    if (virtualKey == 0) {
        if (error) {
            *error = "The requested keyboard key is not supported.";
        }
        return false;
    }
    const bool scanCode = usesPhysicalScanCode(virtualKey);
    std::array<INPUT, 4> inputs{};
    UINT count = 0;
    if (withShift) {
        // Shift와 키 누름·뗌을 하나의 입력 배열로 보내 modifier가 눌린 채 남는 경우를 줄인다.
        appendKey(inputs, count, VK_SHIFT, 0, scanCode);
    }
    appendKey(inputs, count, virtualKey, 0, scanCode);
    appendKey(inputs, count, virtualKey, KEYEVENTF_KEYUP, scanCode);
    if (withShift) {
        appendKey(inputs, count, VK_SHIFT, KEYEVENTF_KEYUP, scanCode);
    }

    const UINT sent = SendInput(count, inputs.data(), sizeof(INPUT));
    if (sent != count) {
        if (withShift) {
            INPUT releaseShift{};
            releaseShift.type = INPUT_KEYBOARD;
            releaseShift.ki.wVk = VK_SHIFT;
            releaseShift.ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(1, &releaseShift, sizeof(releaseShift));
        }
        if (error) {
            *error = "SendInput sent " + std::to_string(sent) + " of " + std::to_string(count) +
                     " events (Windows error " + std::to_string(GetLastError()) + ").";
        }
        return false;
    }
    return true;
}

bool WindowsVirtualKeySender::isCurrentProcessForeground() const {
    const HWND foregroundWindow = GetForegroundWindow();
    DWORD foregroundProcessId = 0;
    if (foregroundWindow) {
        GetWindowThreadProcessId(foregroundWindow, &foregroundProcessId);
    }
    return foregroundProcessId == GetCurrentProcessId();
}

bool WindowsVirtualKeySender::requestForeground(std::uintptr_t nativeWindowHandle, std::string *error) const {
    const HWND window = reinterpret_cast<HWND>(nativeWindowHandle);
    if (!window || !IsWindow(window)) {
        if (error) {
            *error = "The keyboard window handle is not valid.";
        }
        return false;
    }

    // 이미 활성인 창에 같은 포커스를 다시 지정하지 않아 진행 중인 IME 세션을 건드리지 않는다.
    if (GetForegroundWindow() != window) {
        SetForegroundWindow(window);
    }
    if (GetForegroundWindow() == window && GetFocus() != window) {
        SetFocus(window);
    }
    if (GetForegroundWindow() != window) {
        if (error) {
            *error = "Windows did not grant foreground focus to this application.";
        }
        return false;
    }
    return true;
}
