#include "imgui_host.h"

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>

#include <GL/gl.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

// Dear ImGui backend 헤더가 Windows 헤더 의존성을 줄이기 위해 이 선언을 의도적으로 숨긴다.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window,
                                                             UINT message,
                                                             WPARAM wParam,
                                                             LPARAM lParam);

namespace {
constexpr wchar_t kWindowClassName[] = L"VrOverlayKeyboardImGuiWindow";
constexpr int kWindowWidth = 1024;
constexpr int kWindowHeight = 1024;
constexpr int kWglContextMajorVersion = 0x2091;
constexpr int kWglContextMinorVersion = 0x2092;

using CreateContextAttribs = HGLRC(WINAPI *)(HDC, HGLRC, const int *);

void setPixelFormatDescription(PIXELFORMATDESCRIPTOR *description) {
    if (!description) {
        return;
    }
    *description = {};
    description->nSize = sizeof(PIXELFORMATDESCRIPTOR);
    description->nVersion = 1;
    description->dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    description->iPixelType = PFD_TYPE_RGBA;
    description->cColorBits = 32;
    description->cAlphaBits = 8;
    description->iLayerType = PFD_MAIN_PLANE;
}
}

ImGuiHost::~ImGuiHost() {
    shutdown();
}

bool ImGuiHost::initialize(const wchar_t *windowTitle, std::string *error) {
    shutdown();
    m_instance = GetModuleHandleW(nullptr);

    if (!createWindow(windowTitle, error) || !createOpenGlContext(error)) {
        shutdown();
        return false;
    }

    IMGUI_CHECKVERSION();
    // Win32 창과 OpenGL 컨텍스트를 만든 뒤 두 backend가 같은 ImGui 컨텍스트를 사용하도록 연결한다.
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();
    m_imeComposition.setCommitCallback([](const std::string &text) {
        // Windows IME에서 확정된 문자열만 ImGui의 일반 문자 입력 큐로 보낸다.
        if (ImGui::GetCurrentContext()) {
            ImGui::GetIO().AddInputCharactersUTF8(text.c_str());
        }
    });

    if (!ImGui_ImplWin32_Init(m_window)) {
        if (error) {
            *error = "Dear ImGui Win32 backend initialization failed.";
        }
        shutdown();
        return false;
    }
    m_win32BackendInitialized = true;

    if (!ImGui_ImplOpenGL3_Init("#version 130")) {
        if (error) {
            *error = "Dear ImGui OpenGL3 backend initialization failed.";
        }
        shutdown();
        return false;
    }
    m_openGlBackendInitialized = true;

    m_running = true;
    ShowWindow(m_window, SW_SHOWDEFAULT);
    UpdateWindow(m_window);
    // 시작 시 OS 입력 대상도 이 창으로 정한다. 활성화 메시지에서도 필요할 때만 복원한다.
    if (GetForegroundWindow() == m_window && GetFocus() != m_window) {
        SetFocus(m_window);
    }
    return true;
}

bool ImGuiHost::processMessages() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            m_running = false;
            return false;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    m_virtualMouseRouter.pollCancellation();
    return m_running;
}

void ImGuiHost::beginFrame() {
    if (!m_running || !m_glContext) {
        return;
    }
    wglMakeCurrent(m_deviceContext, m_glContext);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

keyboard::ImageFrame ImGuiHost::renderFrame(bool captureOverlayFrame) {
    keyboard::ImageFrame frame;
    if (!m_running || !m_glContext || !m_deviceContext) {
        return frame;
    }

    ImGui::Render();
    RECT clientRect{};
    GetClientRect(m_window, &clientRect);
    // 창 크기는 매 프레임 조회해 리사이즈 직후에도 viewport와 오버레이 이미지 크기를 맞춘다.
    const int width = std::max(1L, clientRect.right - clientRect.left);
    const int height = std::max(1L, clientRect.bottom - clientRect.top);
    glViewport(0, 0, width, height);
    glClearColor(0.075f, 0.09f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (captureOverlayFrame) {
        frame.width = static_cast<std::uint32_t>(width);
        frame.height = static_cast<std::uint32_t>(height);
        const std::size_t rowBytes = static_cast<std::size_t>(frame.width) * 4;
        frame.rgbaPixels.resize(rowBytes * frame.height);
        std::vector<std::uint8_t> bottomUpPixels(frame.rgbaPixels.size());
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, bottomUpPixels.data());
        // OpenGL은 아래 행부터 읽으므로 OpenVR CPU 이미지가 기대하는 위쪽 기준으로 행 순서를 뒤집는다.
        for (std::uint32_t row = 0; row < frame.height; ++row) {
            const std::uint32_t sourceRow = frame.height - row - 1;
            std::memcpy(frame.rgbaPixels.data() + rowBytes * row,
                        bottomUpPixels.data() + rowBytes * sourceRow,
                        rowBytes);
        }
    }

    SwapBuffers(m_deviceContext);
    return frame;
}

void ImGuiHost::shutdown() {
    m_running = false;
    // 포인터 취소 콜백이 살아 있는 ImGui 컨텍스트를 사용할 수 있도록 backend보다 먼저 해제한다.
    m_virtualMouseRouter.shutdown();
    if (m_glContext && m_deviceContext) {
        wglMakeCurrent(m_deviceContext, m_glContext);
    }
    // backend가 GL 리소스를 정리할 수 있도록 컨텍스트보다 먼저 종료한다.
    if (m_openGlBackendInitialized) {
        ImGui_ImplOpenGL3_Shutdown();
        m_openGlBackendInitialized = false;
    }
    if (m_win32BackendInitialized) {
        ImGui_ImplWin32_Shutdown();
        m_win32BackendInitialized = false;
    }
    if (ImGui::GetCurrentContext()) {
        ImGui::DestroyContext();
    }
    if (m_glContext) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(m_glContext);
        m_glContext = nullptr;
    }
    if (m_window && IsWindow(m_window)) {
        if (m_deviceContext) {
            ReleaseDC(m_window, m_deviceContext);
            m_deviceContext = nullptr;
        }
        DestroyWindow(m_window);
    } else if (m_window && m_deviceContext) {
        ReleaseDC(m_window, m_deviceContext);
        m_deviceContext = nullptr;
    }
    m_window = nullptr;
    m_deviceContext = nullptr;
    m_instance = nullptr;
}

HWND ImGuiHost::windowHandle() const {
    return m_window;
}

void ImGuiHost::setCompositionCallback(WindowsImeComposition::CompositionCallback callback) {
    m_imeComposition.setCompositionCallback(std::move(callback));
}

void ImGuiHost::cancelComposition() {
    m_imeComposition.cancel(m_window);
}

bool ImGuiHost::routeVirtualMouseInput(WindowsVirtualMouseRouter::HitTestCallback hitTest,
                                     WindowsVirtualMouseRouter::PointerCallback pointer, std::string *error) {
    return m_virtualMouseRouter.initialize(m_window, std::move(hitTest), std::move(pointer), error);
}

LRESULT CALLBACK ImGuiHost::windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    ImGuiHost *host = reinterpret_cast<ImGuiHost *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto *create = reinterpret_cast<const CREATESTRUCTW *>(lParam);
        host = static_cast<ImGuiHost *>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(host));
        if (host) {
            host->m_window = window;
        }
    }

    if (host) {
        if (message == WM_IME_STARTCOMPOSITION || message == WM_INPUTLANGCHANGE) {
            host->m_virtualMouseRouter.bringHookToFront();
        } else if (message == WM_KILLFOCUS || message == WM_CANCELMODE) {
            host->m_virtualMouseRouter.cancel();
        }
        // IME 조합은 Windows 어댑터가 한 번만 처리한다. backend/default IME 창으로 중복 전달하지 않는다.
        LRESULT imeResult = 0;
        if (host->m_imeComposition.processMessage(window, message, wParam, lParam, imeResult)) {
            return imeResult;
        }
        const bool keyPress = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
        if (host->m_imeComposition.isComposing() && (keyPress || message == WM_CHAR)) {
            // 조합 중 Backspace/Space/Enter는 Windows IME가 처리한다. 확정 버퍼에 같은 편집을 반복하지 않는다.
            // KeyUp은 backend에 전달하여 조합 시작 전에 눌렸던 키도 정상적으로 해제한다.
            return message == WM_CHAR ? 0 : DefWindowProcW(window, message, wParam, lParam);
        }
    }

    // 일반 키·포인터는 공식 backend에 전달한다. IME 결과는 위 어댑터의 commit callback으로 들어온다.
    if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam)) {
        return 1;
    }

    if (host) {
        switch (message) {
        case WM_ACTIVATE:
            // 앱이 활성화되어 있는 동안 OS 포커스는 동일 HWND에 둔다. 이미 포커스가 있으면 아무 일도 하지 않는다.
            if (LOWORD(wParam) != WA_INACTIVE && GetFocus() != window) {
                SetFocus(window);
            }
            return 0;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
            // 화면의 가상 버튼 클릭은 ImGui에서만 처리하고 기본 창의 IME 처리로 재전달하지 않는다.
            return 0;
        case WM_CLOSE:
            host->m_running = false;
            PostQuitMessage(0);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        default:
            break;
        }
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

bool ImGuiHost::createWindow(const wchar_t *windowTitle, std::string *error) {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_OWNDC;
    windowClass.lpfnWndProc = &ImGuiHost::windowProcedure;
    windowClass.hInstance = m_instance;
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    windowClass.lpszClassName = kWindowClassName;
    if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        setError(error, "RegisterClassExW");
        return false;
    }

    constexpr DWORD windowStyle = WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;
    RECT windowRect{0, 0, kWindowWidth, kWindowHeight};
    AdjustWindowRectEx(&windowRect, windowStyle, FALSE, 0);
    m_window = CreateWindowExW(0,
                               kWindowClassName,
                               windowTitle ? windowTitle : L"VR Overlay Keyboard",
                               windowStyle,
                               CW_USEDEFAULT,
                               CW_USEDEFAULT,
                               windowRect.right - windowRect.left,
                               windowRect.bottom - windowRect.top,
                               nullptr,
                               nullptr,
                               m_instance,
                               this);
    if (!m_window) {
        setError(error, "CreateWindowExW");
        return false;
    }

    m_deviceContext = GetDC(m_window);
    if (!m_deviceContext) {
        setError(error, "GetDC");
        return false;
    }
    return true;
}

bool ImGuiHost::createOpenGlContext(std::string *error) {
    PIXELFORMATDESCRIPTOR pixelFormatDescription{};
    setPixelFormatDescription(&pixelFormatDescription);
    const int pixelFormat = ChoosePixelFormat(m_deviceContext, &pixelFormatDescription);
    if (pixelFormat == 0 || !SetPixelFormat(m_deviceContext, pixelFormat, &pixelFormatDescription)) {
        setError(error, "SetPixelFormat");
        return false;
    }

    // 확장 함수 주소 조회에 필요한 임시 context를 먼저 만들고, OpenGL 3.0 context로 교체한다.
    HGLRC temporaryContext = wglCreateContext(m_deviceContext);
    if (!temporaryContext || !wglMakeCurrent(m_deviceContext, temporaryContext)) {
        if (temporaryContext) {
            wglDeleteContext(temporaryContext);
        }
        setError(error, "wglCreateContext");
        return false;
    }

    const auto createContext = reinterpret_cast<CreateContextAttribs>(
        wglGetProcAddress("wglCreateContextAttribsARB"));
    if (!createContext) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(temporaryContext);
        if (error) {
            *error = "The graphics driver does not expose WGL_ARB_create_context for OpenGL 3.0.";
        }
        return false;
    }

    const int contextAttributes[] = {
        kWglContextMajorVersion, 3,
        kWglContextMinorVersion, 0,
        0,
    };
    m_glContext = createContext(m_deviceContext, nullptr, contextAttributes);
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(temporaryContext);
    if (!m_glContext || !wglMakeCurrent(m_deviceContext, m_glContext)) {
        if (m_glContext) {
            wglDeleteContext(m_glContext);
            m_glContext = nullptr;
        }
        setError(error, "Creating an OpenGL 3.0 context");
        return false;
    }
    return true;
}

void ImGuiHost::setError(std::string *error, const char *operation) const {
    if (error) {
        *error = std::string(operation) + " failed (Windows error " + std::to_string(GetLastError()) + ").";
    }
}
