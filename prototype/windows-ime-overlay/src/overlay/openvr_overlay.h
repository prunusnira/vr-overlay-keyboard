#pragma once

#include <QImage>
#include <QEvent>
#include <QPointF>
#include <QString>
#include <Qt>

#include <openvr.h>

#include <functional>

class OpenVrOverlay final {
public:
    using PointerCallback = std::function<void(QEvent::Type, const QPointF &, Qt::MouseButton)>;

    OpenVrOverlay();
    ~OpenVrOverlay();

    OpenVrOverlay(const OpenVrOverlay &) = delete;
    OpenVrOverlay &operator=(const OpenVrOverlay &) = delete;

    bool initialize(QString *errorMessage);
    void shutdown();
    void showDashboard();
    bool updateTexture(const QImage &image, QString *errorMessage);
    void pollEvents();
    void setPointerCallback(PointerCallback callback);

private:
    Qt::MouseButton translateMouseButton(vr::EVRMouseButton button) const;

    vr::IVRSystem *m_system = nullptr;
    vr::IVROverlay *m_overlay = nullptr;
    vr::VROverlayHandle_t m_overlayHandle = vr::k_ulOverlayHandleInvalid;
    vr::VROverlayHandle_t m_thumbnailHandle = vr::k_ulOverlayHandleInvalid;
    uint32_t m_textureHeight = 1024;
    bool m_thumbnailTextureSet = false;
    bool m_runtimeInitialized = false;
    PointerCallback m_pointerCallback;
};
