#pragma once

#include "../../core/app_contracts.h"
#include "ime_composition.h"
#include "virtual_mouse_router.h"

#include <Windows.h>

#include <cstdint>
#include <functional>
#include <string>

class ImGuiHost final {
public:
    ImGuiHost() = default;
    ~ImGuiHost();

    ImGuiHost(const ImGuiHost &) = delete;
    ImGuiHost &operator=(const ImGuiHost &) = delete;

    bool initialize(const wchar_t *windowTitle, std::string *error);
    bool processMessages();
    void beginFrame(const std::function<void()> &pollPointerInput = {});
    // captureOverlayFrame이 true일 때만 OpenVR 계약 형식의 RGBA 픽셀을 읽어 반환한다.
    keyboard::ImageFrame renderFrame(bool captureOverlayFrame);
    void shutdown();

    HWND windowHandle() const;
    void setCompositionCallback(WindowsImeComposition::CompositionCallback callback);
    void cancelComposition();
    bool routeVirtualMouseInput(WindowsVirtualMouseRouter::HitTestCallback hitTest,
                               WindowsVirtualMouseRouter::PointerCallback pointer, std::string *error);

private:
    static LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    bool createWindow(const wchar_t *windowTitle, std::string *error);
    bool createOpenGlContext(std::string *error);
    void setError(std::string *error, const char *operation) const;

    HINSTANCE m_instance = nullptr;
    HWND m_window = nullptr;
    HDC m_deviceContext = nullptr;
    HGLRC m_glContext = nullptr;
    bool m_win32BackendInitialized = false;
    bool m_openGlBackendInitialized = false;
    bool m_running = false;
    WindowsImeComposition m_imeComposition;
    WindowsVirtualMouseRouter m_virtualMouseRouter;
};
