#include "ime/tsf_input.h"
#include "overlay/openvr_overlay.h"
#include "ui/keyboard_widget.h"

#include <QApplication>
#include <QDateTime>
#include <QTimer>

#include <Windows.h>

int main(int argc, char *argv[]) {
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninitializeCom = SUCCEEDED(comResult);

    QApplication application(argc, argv);
    KeyboardWidget widget;
    widget.show();

    TsfInput ime;
    ime.setCandidateCallback([&widget](const CandidateSnapshot &snapshot) {
        widget.updateCandidates(snapshot);
    });
    ime.setStatusCallback([&widget](const QString &message) {
        widget.appendLog(message);
    });
    widget.setKeyCallback([&ime](WORD virtualKey, bool withShift, QString *error) {
        return ime.sendVirtualKey(virtualKey, withShift, error);
    });
    widget.setCandidateCallback([&ime](UINT index, QString *error) {
        return ime.selectCandidate(index, error);
    });

    if (FAILED(comResult)) {
        widget.appendLog(QStringLiteral("CoInitializeEx failed: 0x%1")
                             .arg(static_cast<quint32>(comResult), 8, 16, QLatin1Char('0')));
    } else {
        QString tsfError;
        if (!ime.initialize(&tsfError)) {
            widget.appendLog(QStringLiteral("TSF initialization failed: %1").arg(tsfError));
        }
    }

    OpenVrOverlay overlay;
    QString overlayError;
    if (overlay.initialize(&overlayError)) {
        widget.appendLog(QStringLiteral("OpenVR dashboard overlay initialized."));
        widget.setShowOverlayCallback([&overlay]() { overlay.showDashboard(); });
        overlay.setPointerCallback([&widget](QEvent::Type type, const QPointF &position, Qt::MouseButton button) {
            widget.dispatchOverlayMouseEvent(type, position, button);
        });
    } else {
        widget.appendLog(QStringLiteral("OpenVR initialization failed: %1").arg(overlayError));
    }

    QTimer timer;
    timer.setInterval(16);
    qint64 lastTextureUpdate = 0;
    qint64 lastLanguageRefresh = 0;
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        overlay.pollEvents();
        widget.refreshInputStatus();

        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - lastLanguageRefresh >= 250) {
            lastLanguageRefresh = now;
            widget.refreshInputLanguage();
        }
        if (now - lastTextureUpdate >= 100) {
            lastTextureUpdate = now;
            QString textureError;
            if (!overlay.updateTexture(widget.renderToImage(), &textureError) && !textureError.isEmpty()) {
                static bool reportedTextureError = false;
                if (!reportedTextureError) {
                    widget.appendLog(QStringLiteral("OpenVR texture update failed: %1").arg(textureError));
                    reportedTextureError = true;
                }
            }
        }
    });
    timer.start();

    const int exitCode = application.exec();
    overlay.shutdown();
    ime.shutdown();
    if (shouldUninitializeCom) {
        CoUninitialize();
    }
    return exitCode;
}
