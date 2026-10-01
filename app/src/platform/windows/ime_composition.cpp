#include "ime_composition.h"

#include <imm.h>

#include <utility>

namespace {
std::wstring compositionString(HIMC context, DWORD kind) {
    // IMM의 길이는 UTF-16 문자 수가 아니라 바이트 수다. 음수 오류와 홀수 길이를 먼저 거른다.
    const LONG bytes = ImmGetCompositionStringW(context, kind, nullptr, 0);
    if (bytes <= 0 || bytes % sizeof(wchar_t) != 0) {
        return {};
    }
    std::wstring value(static_cast<std::size_t>(bytes) / sizeof(wchar_t), L'\0');
    const LONG copied = ImmGetCompositionStringW(context, kind, value.data(), static_cast<DWORD>(bytes));
    if (copied < 0 || copied > bytes || copied % sizeof(wchar_t) != 0) {
        return {};
    }
    value.resize(static_cast<std::size_t>(copied) / sizeof(wchar_t));
    return value;
}

std::string utf8(const std::wstring &text) {
    if (text.empty()) {
        return {};
    }
    const int length = static_cast<int>(text.size());
    const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), length,
                                         nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) {
        return {};
    }
    std::string value(static_cast<std::size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), length,
                        value.data(), bytes, nullptr, nullptr);
    return value;
}
}

void WindowsImeComposition::setCompositionCallback(CompositionCallback callback) {
    m_compositionCallback = std::move(callback);
    publish();
}

void WindowsImeComposition::setCommitCallback(CommitCallback callback) {
    m_commitCallback = std::move(callback);
}

bool WindowsImeComposition::processMessage(HWND window, UINT message, WPARAM wParam,
                                          LPARAM lParam, LRESULT &result) {
    result = 0;
    switch (message) {
    case WM_IME_SETCONTEXT:
        // 조합 문자열은 앱의 고정 미리보기 영역에 그린다. 후보 창은 TSF sink가 지원 여부에 따라 처리한다.
        result = DefWindowProcW(window, message, wParam, lParam & ~ISC_SHOWUICOMPOSITIONWINDOW);
        return true;
    case WM_IME_STARTCOMPOSITION:
        m_composition = {true, {}};
        publish();
        return true;
    case WM_IME_COMPOSITION: {
        const HIMC context = ImmGetContext(window);
        if (!context) {
            return true;
        }
        std::string committed;
        if (lParam & GCS_RESULTSTR) {
            // 조합 갱신(GCS_COMPSTR/CS_INSERTCHAR)은 편집 버퍼에 넣지 않고 확정 결과만 한 번 전달한다.
            committed = utf8(compositionString(context, GCS_RESULTSTR));
            m_composition.preedit.clear();
        }
        if (lParam & GCS_COMPSTR) {
            m_composition.preedit = utf8(compositionString(context, GCS_COMPSTR));
            m_composition.active = true;
        } else if (lParam == 0) {
            m_composition.preedit.clear();
        }
        ImmReleaseContext(window, context);
        if (!committed.empty() && m_commitCallback) {
            m_commitCallback(committed);
        }
        publish();
        // 기본 IME 창이나 ImGui backend에 다시 넘기면 같은 조합 메시지가 중복 처리된다.
        return true;
    }
    case WM_IME_ENDCOMPOSITION:
        m_composition = {};
        publish();
        return true;
    default:
        return false;
    }
}

void WindowsImeComposition::cancel(HWND window) {
    if (const HIMC context = ImmGetContext(window)) {
        // Clear input은 조합도 취소한다. 매 프레임/매 키에는 조합 종료나 포커스 재설정을 요청하지 않는다.
        ImmNotifyIME(context, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
        ImmReleaseContext(window, context);
    }
    m_composition = {};
    publish();
}

bool WindowsImeComposition::isComposing() const {
    return m_composition.active;
}

void WindowsImeComposition::publish() {
    if (m_compositionCallback) {
        m_compositionCallback(m_composition);
    }
}
