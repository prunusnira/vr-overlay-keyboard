#include "core/keyboard_application.h"
#include "ime/tsf_input.h"
#include "input/steamvr_action_source.h"
#include "osc/osc_client.h"
#include "overlay/openvr_overlay.h"
#include "platform/windows/imgui_host.h"
#include "platform/windows/input_language_service.h"
#include "platform/windows/settings_store.h"
#include "platform/windows/virtual_key_sender.h"
#include "ui/keyboard_ui.h"

#include <Windows.h>
#include <objbase.h>

#include <filesystem>
#include <string>
#include <vector>

namespace {
constexpr ULONGLONG kTargetFrameIntervalMs = 16;
constexpr ULONGLONG kOverlayTextureIntervalMs = 16;

std::wstring executablePath() {
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr,
                                            buffer.data(),
                                            static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return {};
    }
    buffer.resize(length);
    return buffer;
}

std::string wideToUtf8(const std::wstring &text) {
    if (text.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8,
                                         WC_ERR_INVALID_CHARS,
                                         text.data(),
                                         static_cast<int>(text.size()),
                                         nullptr,
                                         0,
                                         nullptr,
                                         nullptr);
    if (size <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8,
                        WC_ERR_INVALID_CHARS,
                        text.data(),
                        static_cast<int>(text.size()),
                        result.data(),
                        size,
                        nullptr,
                        nullptr);
    return result;
}

bool isTsfSinkDisabled() {
    const wchar_t *commandLine = GetCommandLineW();
    return commandLine && std::wstring(commandLine).find(L"--disable-tsf-sink") != std::wstring::npos;
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    // TSF 초기화와 메시지 처리를 같은 UI 스레드에서 수행하기 위해 STA COM을 사용한다.
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninitializeCom = SUCCEEDED(comResult);

    ImGuiHost host;
    std::string hostError;
    if (!host.initialize(L"VR Overlay Keyboard", &hostError)) {
        const std::wstring message(hostError.begin(), hostError.end());
        MessageBoxW(nullptr,
                    message.c_str(),
                    L"VR Overlay Keyboard startup failed",
                    MB_OK | MB_ICONERROR);
        if (shouldUninitializeCom) {
            CoUninitialize();
        }
        return 1;
    }

    // 앱 기능은 계약에 의존하고, 이 진입점에서 Windows·OpenVR·OSC 구현을 연결한다.
    WindowsInputLanguageService languageService;
    WindowsSettingsStore settingsStore;
    WindowsVirtualKeySender virtualKeySender;
    TsfInput tsfInput;
    OscClient chatboxSender;
    OpenVrOverlay overlay;
    keyboard::KeyboardApplication keyboardApplication(
        overlay, languageService, tsfInput, virtualKeySender, tsfInput, chatboxSender, settingsStore);
    KeyboardUi keyboardUi(keyboardApplication);

    keyboardUi.setFocusRequestCallback([&host, &virtualKeySender](std::string *error) {
        return virtualKeySender.requestForeground(
            reinterpret_cast<std::uintptr_t>(host.windowHandle()), error);
    });
    tsfInput.setCandidateCallback([&keyboardApplication](const keyboard::CandidateSnapshot &snapshot) {
        keyboardApplication.setCandidates(snapshot);
    });
    host.setCompositionCallback([&keyboardApplication](const keyboard::CompositionSnapshot &snapshot) {
        keyboardApplication.setComposition(snapshot);
    });
    keyboardUi.setCompositionCancelCallback([&host]() { host.cancelComposition(); });

    if (FAILED(comResult)) {
        keyboardApplication.setStatus("COM initialization failed (HRESULT " +
                                      std::to_string(static_cast<unsigned long>(comResult)) +
                                      "). TSF candidates are unavailable.");
    } else if (isTsfSinkDisabled()) {
        keyboardApplication.setStatus("Diagnostic mode: TSF candidate capture is disabled.");
    } else {
        std::string tsfError;
        if (!tsfInput.initialize(&tsfError)) {
            keyboardApplication.setStatus("TSF initialization failed: " + tsfError);
        } else {
            keyboardApplication.setStatus("TSF candidate sink registered.");
        }
    }

    std::string overlayError;
    const bool overlayReady = overlay.initialize(&overlayError);
    if (overlayReady) {
        overlay.setPointerCallback([&keyboardUi](const keyboard::PointerEvent &event) {
            keyboardUi.dispatchPointerEvent(event);
        });
        overlay.setInteractionStatusCallback([&keyboardApplication](const std::string &message) {
            keyboardApplication.setStatus(message);
        });
        keyboardApplication.setStatus("OpenVR general overlay initialized and hidden.");
    } else {
        keyboardApplication.setStatus("OpenVR initialization failed: " + overlayError);
    }

    SteamVrActionSource actionSource;
    bool actionReady = false;
    if (overlayReady) {
        const std::filesystem::path manifestPath =
            std::filesystem::path(executablePath()).parent_path() /
            L"resources" / L"steamvr" / L"actions.json";
        std::error_code fileError;
        if (!manifestPath.empty() && std::filesystem::exists(manifestPath, fileError)) {
            std::string inputError;
            if (actionSource.initialize(manifestPath.u8string(), [&keyboardApplication]() {
                    keyboardApplication.toggleOverlay();
                }, [&overlay, &keyboardApplication](const keyboard::ControllerPointerSamples &samples) {
                    // 양손 샘플을 모두 오버레이에 전달해 손별 포인터 입력을 유지한다.
                    overlay.handleControllerPointers(samples,
                                                     keyboardApplication.state().settings);
                }, [&keyboardApplication](const std::vector<keyboard::ControllerButtonState> &buttons) {
                    keyboardApplication.updateControllerButtons(buttons);
                }, &inputError)) {
                actionReady = true;
                keyboardApplication.setStatus(
                    "SteamVR Input is ready. Bind keyboard toggle, pointer controls, and the selected summon buttons.");
            } else {
                keyboardApplication.setStatus("SteamVR Input initialization failed: " + inputError);
            }
        } else {
            keyboardApplication.setStatus("SteamVR Input action manifest is missing: " +
                                          wideToUtf8(manifestPath.wstring()));
        }
    }

    keyboardApplication.refresh();

    // TSF/입력기 초기화 뒤에 앱 전용 마우스 어댑터를 연결한다. 컨트롤러와 동일 UI 포인터 경로를 사용한다.
    std::string mouseRoutingError;
    if (!host.routeVirtualMouseInput([&keyboardUi](int x, int y) {
            return keyboardUi.isVirtualControlAt(x, y);
        }, [&keyboardUi](const keyboard::PointerEvent &event) {
            keyboardUi.dispatchPointerEvent(event);
        }, &mouseRoutingError)) {
        keyboardApplication.setStatus(mouseRoutingError);
    }

    ULONGLONG lastTextureUpdate = 0;
    ULONGLONG lastLanguageRefresh = 0;
    bool actionPollErrorReported = false;
    bool textureErrorReported = false;
    bool applicationIsForeground = virtualKeySender.isCurrentProcessForeground();

    while (host.processMessages()) {
        const ULONGLONG now = GetTickCount64();
        // 입력 언어 목록은 짧은 주기로 갱신해 매 프레임 열거하지 않는다.
        if (now - lastLanguageRefresh >= 250) {
            lastLanguageRefresh = now;
            keyboardApplication.refresh();
        }
        // 전경 전환 직후에도 화면 상태가 늦지 않도록 매 프레임 최신 Windows 상태를 읽는다.
        applicationIsForeground = virtualKeySender.isCurrentProcessForeground();

        host.beginFrame([&]() {
            if (actionReady) {
                std::string actionError;
                if (!actionSource.poll(&actionError) && !actionError.empty() && !actionPollErrorReported) {
                    keyboardApplication.setStatus("SteamVR Input polling failed: " + actionError);
                    actionPollErrorReported = true;
                }
            }
        });
        // 후보 선택/조합 취소 콜백이 그리는 도중 상태를 갱신해도 현재 화면의 문자열·후보 반복은 안정적으로 유지한다.
        const keyboard::AppUiState uiState = keyboardApplication.state();
        keyboardUi.draw(uiState, applicationIsForeground);
        // OpenGL 픽셀 readback은 오버레이가 표시될 때 초당 최대 약 60회 수행한다.
        const bool updateOverlayTexture =
            overlayReady && overlay.isVisible() && now - lastTextureUpdate >= kOverlayTextureIntervalMs;
        keyboard::ImageFrame frame = host.renderFrame(updateOverlayTexture);
        if (updateOverlayTexture) {
            lastTextureUpdate = now;
            std::string textureError;
            if (!overlay.updateTexture(frame, &textureError) &&
                !textureError.empty() && !textureErrorReported) {
                keyboardApplication.setStatus("OpenVR texture update failed: " + textureError);
                textureErrorReported = true;
            }
        }
        // SwapBuffers가 이미 수직 동기화로 기다린 경우 추가 대기를 피하면서 목표 프레임 간격을 지킨다.
        const ULONGLONG elapsed = GetTickCount64() - now;
        if (elapsed < kTargetFrameIntervalMs) {
            Sleep(static_cast<DWORD>(kTargetFrameIntervalMs - elapsed));
        }
    }

    actionSource.shutdown();
    overlay.shutdown();
    tsfInput.shutdown();
    // host 종료 중 발생하는 창 메시지가 이미 종료된 앱 상태에 콜백하지 않게 연결을 먼저 해제한다.
    host.setCompositionCallback({});
    host.shutdown();
    if (shouldUninitializeCom) {
        CoUninitialize();
    }
    return 0;
}
