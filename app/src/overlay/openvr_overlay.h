#pragma once

#include "../core/app_contracts.h"

#include <openvr.h>

#include <cstdint>
#include <functional>
#include <string>

class OpenVrOverlay final : public keyboard::OverlayControlPort {
public:
    using PointerCallback = std::function<void(const keyboard::PointerEvent &)>;

    OpenVrOverlay() = default;
    ~OpenVrOverlay() override;

    OpenVrOverlay(const OpenVrOverlay &) = delete;
    OpenVrOverlay &operator=(const OpenVrOverlay &) = delete;

    bool initialize(std::string *error);
    void shutdown();
    bool show(std::string *error) override;
    bool hide(std::string *error) override;
    bool isVisible() const override;
    bool updateTexture(const keyboard::ImageFrame &frame, std::string *error);
    void handleControllerPointers(const keyboard::ControllerPointerSamples &samples);
    void setPointerCallback(PointerCallback callback);

private:
    bool computePointerPosition(const keyboard::ControllerPointerSample &sample, int *x, int *y) const;
    void dispatchPointerEvent(keyboard::PointerEventType type,
                              int x,
                              int y,
                              keyboard::PointerButton button = keyboard::PointerButton::None);
    void resetPointerState();
    bool failWithOverlayError(const char *operation, vr::VROverlayError error, std::string *message) const;

    vr::IVROverlay *m_overlay = nullptr;
    vr::VROverlayHandle_t m_handle = vr::k_ulOverlayHandleInvalid;
    std::uint32_t m_textureWidth = 1024;
    std::uint32_t m_textureHeight = 1024;
    bool m_runtimeInitialized = false;
    PointerCallback m_pointerCallback;
    keyboard::ControllerHand m_captureHand = keyboard::ControllerHand::Right;
    keyboard::ControllerHand m_hoverHand = keyboard::ControllerHand::Right;
    bool m_hasCaptureHand = false;
    bool m_hasHoverHand = false;
    bool m_selectWasPressed = false;
    bool m_pointerInsideOverlay = false;
    int m_lastPointerX = -1;
    int m_lastPointerY = -1;
};
