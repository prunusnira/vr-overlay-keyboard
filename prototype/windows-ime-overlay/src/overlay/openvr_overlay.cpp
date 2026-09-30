#include "openvr_overlay.h"

#include <algorithm>
#include <utility>

namespace {
constexpr char kOverlayKey[] = "com.prunusnira.vr-overlay-keyboard.ime-prototype";
constexpr char kOverlayName[] = "VRChat IME Keyboard Prototype";
constexpr float kOverlayWidthMeters = 1.45f;
}

OpenVrOverlay::OpenVrOverlay() = default;

OpenVrOverlay::~OpenVrOverlay() {
    shutdown();
}

bool OpenVrOverlay::initialize(QString *errorMessage) {
    shutdown();

    vr::EVRInitError initError = vr::VRInitError_None;
    m_system = vr::VR_Init(&initError, vr::VRApplication_Overlay);
    if (initError != vr::VRInitError_None || !m_system) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(vr::VR_GetVRInitErrorAsEnglishDescription(initError));
        }
        return false;
    }
    m_runtimeInitialized = true;
    m_overlay = vr::VROverlay();
    if (!m_overlay) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("SteamVR did not provide IVROverlay.");
        }
        shutdown();
        return false;
    }

    const vr::VROverlayError overlayError = m_overlay->CreateDashboardOverlay(
        kOverlayKey, kOverlayName, &m_overlayHandle, &m_thumbnailHandle);
    if (overlayError != vr::VROverlayError_None) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(m_overlay->GetOverlayErrorNameFromEnum(overlayError));
        }
        shutdown();
        return false;
    }
    m_thumbnailTextureSet = false;

    m_overlay->SetOverlayWidthInMeters(m_overlayHandle, kOverlayWidthMeters);
    m_overlay->SetOverlayInputMethod(m_overlayHandle, vr::VROverlayInputMethod_Mouse);
    m_overlay->SetOverlayAlpha(m_overlayHandle, 1.0f);
    return true;
}

void OpenVrOverlay::shutdown() {
    if (m_overlay) {
        if (m_thumbnailHandle != vr::k_ulOverlayHandleInvalid) {
            m_overlay->DestroyOverlay(m_thumbnailHandle);
        }
        if (m_overlayHandle != vr::k_ulOverlayHandleInvalid) {
            m_overlay->DestroyOverlay(m_overlayHandle);
        }
    }
    m_overlayHandle = vr::k_ulOverlayHandleInvalid;
    m_thumbnailHandle = vr::k_ulOverlayHandleInvalid;
    m_thumbnailTextureSet = false;
    m_overlay = nullptr;
    m_system = nullptr;
    if (m_runtimeInitialized) {
        vr::VR_Shutdown();
        m_runtimeInitialized = false;
    }
}

void OpenVrOverlay::showDashboard() {
    if (m_overlay && m_overlayHandle != vr::k_ulOverlayHandleInvalid) {
        m_overlay->ShowDashboard(kOverlayKey);
    }
}

bool OpenVrOverlay::updateTexture(const QImage &image, QString *errorMessage) {
    if (!m_overlay || m_overlayHandle == vr::k_ulOverlayHandleInvalid || image.isNull()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("OpenVR overlay is not initialized or the image is empty.");
        }
        return false;
    }

    const QImage rgbaImage = image.convertToFormat(QImage::Format_RGBA8888);
    m_textureHeight = static_cast<uint32_t>(rgbaImage.height());
    vr::HmdVector2_t mouseScale = {
        static_cast<float>(rgbaImage.width()),
        static_cast<float>(rgbaImage.height())
    };
    m_overlay->SetOverlayMouseScale(m_overlayHandle, &mouseScale);

    const vr::VROverlayError error = m_overlay->SetOverlayRaw(
        m_overlayHandle,
        const_cast<uchar *>(rgbaImage.constBits()),
        static_cast<uint32_t>(rgbaImage.width()),
        static_cast<uint32_t>(rgbaImage.height()),
        4);
    if (error != vr::VROverlayError_None) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(m_overlay->GetOverlayErrorNameFromEnum(error));
        }
        return false;
    }

    if (m_thumbnailHandle != vr::k_ulOverlayHandleInvalid && !m_thumbnailTextureSet) {
        const vr::VROverlayError thumbnailError = m_overlay->SetOverlayRaw(
            m_thumbnailHandle,
            const_cast<uchar *>(rgbaImage.constBits()),
            static_cast<uint32_t>(rgbaImage.width()),
            static_cast<uint32_t>(rgbaImage.height()),
            4);
        m_thumbnailTextureSet = thumbnailError == vr::VROverlayError_None;
    }
    return true;
}

void OpenVrOverlay::pollEvents() {
    if (!m_overlay || m_overlayHandle == vr::k_ulOverlayHandleInvalid) {
        return;
    }

    vr::VREvent_t event{};
    while (m_overlay->PollNextOverlayEvent(m_overlayHandle, &event, sizeof(event))) {
        if (!m_pointerCallback) {
            continue;
        }

        // OpenVR mouse coordinates start at the texture's bottom-left; Qt widgets start at top-left.
        const float topLeftY = static_cast<float>(m_textureHeight) - 1.0f - event.data.mouse.y;
        const float clampedY = std::clamp(topLeftY, 0.0f, static_cast<float>(m_textureHeight - 1));
        const QPointF position(event.data.mouse.x, clampedY);
        switch (event.eventType) {
        case vr::VREvent_MouseMove:
            m_pointerCallback(QEvent::MouseMove, position, Qt::NoButton);
            break;
        case vr::VREvent_MouseButtonDown: {
            const Qt::MouseButton button = translateMouseButton(
                static_cast<vr::EVRMouseButton>(event.data.mouse.button));
            if (button != Qt::NoButton) {
                m_pointerCallback(QEvent::MouseButtonPress, position, button);
            }
            break;
        }
        case vr::VREvent_MouseButtonUp: {
            const Qt::MouseButton button = translateMouseButton(
                static_cast<vr::EVRMouseButton>(event.data.mouse.button));
            if (button != Qt::NoButton) {
                m_pointerCallback(QEvent::MouseButtonRelease, position, button);
            }
            break;
        }
        default:
            break;
        }
    }
}

void OpenVrOverlay::setPointerCallback(PointerCallback callback) {
    m_pointerCallback = std::move(callback);
}

Qt::MouseButton OpenVrOverlay::translateMouseButton(vr::EVRMouseButton button) const {
    switch (button) {
    case vr::VRMouseButton_Left:
        return Qt::LeftButton;
    case vr::VRMouseButton_Right:
        return Qt::RightButton;
    default:
        return Qt::NoButton;
    }
}
