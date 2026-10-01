#include "core/keyboard_application.h"
#include "ime/tsf_input.h"
#include "input/steamvr_action_source.h"
#include "osc/osc_client.h"
#include "overlay/openvr_overlay.h"
#include "platform/windows/input_language_service.h"
#include "platform/windows/virtual_key_sender.h"
#include "ui/keyboard_widget.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QTimer>

#include <Windows.h>
#include <objbase.h>

#include <cstdint>

namespace {
std::string toUtf8(const QString &text) {
    const QByteArray bytes = text.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}
}

int main(int argc, char *argv[]) {
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninitializeCom = SUCCEEDED(comResult);

    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("VR Overlay Keyboard"));

    WindowsInputLanguageService languageService;
    WindowsVirtualKeySender virtualKeySender;
    TsfInput tsfInput;
    OscClient chatboxSender;
    OpenVrOverlay overlay;
    keyboard::KeyboardApplication keyboardApplication(
        overlay, languageService, virtualKeySender, tsfInput, chatboxSender);
    KeyboardWidget widget(keyboardApplication);

    widget.setFocusRequestCallback([&widget, &virtualKeySender](std::string *error) {
        return virtualKeySender.requestForeground(
            static_cast<std::uintptr_t>(widget.winId()), error);
    });
    keyboardApplication.setStateChangedCallback([&widget](const keyboard::AppUiState &state) {
        widget.setAppState(state);
    });
    tsfInput.setCandidateCallback([&keyboardApplication](const keyboard::CandidateSnapshot &snapshot) {
        keyboardApplication.setCandidates(snapshot);
    });
    widget.show();

    if (FAILED(comResult)) {
        keyboardApplication.setStatus("COM initialization failed (HRESULT " +
                                      std::to_string(static_cast<unsigned long>(comResult)) + "). TSF candidates are unavailable.");
    } else if (application.arguments().contains(QStringLiteral("--disable-tsf-sink"))) {
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
        overlay.setPointerCallback([&widget](const keyboard::PointerEvent &event) {
            widget.dispatchOverlayPointerEvent(event);
        });
        keyboardApplication.setStatus("OpenVR general overlay initialized and hidden.");
    } else {
        keyboardApplication.setStatus("OpenVR initialization failed: " + overlayError);
    }

    SteamVrActionSource actionSource;
    bool actionReady = false;
    if (overlayReady) {
        const QString manifestPath = QDir::toNativeSeparators(
            QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("resources/steamvr/actions.json")));
        if (QFileInfo::exists(manifestPath)) {
            std::string inputError;
            if (actionSource.initialize(toUtf8(manifestPath), [&keyboardApplication]() {
                    keyboardApplication.toggleOverlay();
                }, [&overlay](const keyboard::ControllerPointerSamples &samples) {
                    overlay.handleControllerPointers(samples);
                }, &inputError)) {
                actionReady = true;
                keyboardApplication.setStatus(
                    "SteamVR Input is ready. Bind Toggle Keyboard, Controller Pointer Pose, and Controller Pointer Click.");
            } else {
                keyboardApplication.setStatus("SteamVR Input initialization failed: " + inputError);
            }
        } else {
            keyboardApplication.setStatus("SteamVR Input action manifest is missing: " + toUtf8(manifestPath));
        }
    }

    keyboardApplication.refresh();

    QTimer timer;
    timer.setInterval(16);
    qint64 lastTextureUpdate = 0;
    qint64 lastInputStatusUpdate = 0;
    qint64 lastLanguageRefresh = 0;
    bool actionPollErrorReported = false;
    bool textureErrorReported = false;
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (actionReady) {
            std::string actionError;
            if (!actionSource.poll(&actionError) && !actionError.empty() && !actionPollErrorReported) {
                keyboardApplication.setStatus("SteamVR Input polling failed: " + actionError);
                actionPollErrorReported = true;
            }
        }
        if (overlayReady) {
            if (now - lastTextureUpdate >= 100) {
                lastTextureUpdate = now;
                std::string textureError;
                if (!overlay.updateTexture(widget.renderFrame(), &textureError) &&
                    !textureError.empty() && !textureErrorReported) {
                    keyboardApplication.setStatus("OpenVR texture update failed: " + textureError);
                    textureErrorReported = true;
                }
            }
        }
        if (now - lastLanguageRefresh >= 250) {
            lastLanguageRefresh = now;
            keyboardApplication.refresh();
        }
        if (now - lastInputStatusUpdate >= 250) {
            lastInputStatusUpdate = now;
            widget.refreshFocusStatus(virtualKeySender.isCurrentProcessForeground());
        }
    });
    timer.start();

    const int exitCode = application.exec();
    timer.stop();
    actionSource.shutdown();
    overlay.shutdown();
    tsfInput.shutdown();
    if (shouldUninitializeCom) {
        CoUninitialize();
    }
    return exitCode;
}
