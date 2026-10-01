#pragma once

#include "../core/app_contracts.h"

#include <openvr.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class OpenVrOverlay final : public keyboard::OverlayControlPort {
public:
    using PointerCallback = std::function<void(const keyboard::PointerEvent &)>;
    using InteractionStatusCallback = std::function<void(const std::string &)>;

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
    void handleControllerPointers(const keyboard::ControllerPointerSamples &samples,
                                  const keyboard::AppSettings &settings);
    void setPointerCallback(PointerCallback callback);
    void setInteractionStatusCallback(InteractionStatusCallback callback);

private:
    bool placeInFrontOfHead(std::string *error);
    bool setAbsoluteTransform(const vr::HmdMatrix34_t &transform,
                              const char *operation,
                              std::string *error);
    void orientTowardsUser(vr::HmdMatrix34_t *transform) const;
    bool beginGripDrag(const keyboard::ControllerPointerSample &sample, std::string *error);
    bool updateGripDrag(const keyboard::ControllerPointerSample &sample, std::string *error);
    void reportInteractionStatus(const std::string &message);
    bool computePointerPosition(const keyboard::ControllerPointerSample &sample,
                                float offsetXPercent,
                                float offsetYPercent,
                                int *x,
                                int *y) const;
    void dispatchPointerEvent(keyboard::PointerEventType type,
                              int x,
                              int y,
                              keyboard::PointerButton button = keyboard::PointerButton::None);
    void resetPointerState();
    bool failWithOverlayError(const char *operation, vr::VROverlayError error, std::string *message) const;

    vr::IVRSystem *m_system = nullptr;
    vr::IVROverlay *m_overlay = nullptr;
    vr::VROverlayHandle_t m_handle = vr::k_ulOverlayHandleInvalid;
    std::uint32_t m_textureWidth = 1024;
    std::uint32_t m_textureHeight = 1024;
    // OpenVR가 계속 참조할 수 있도록 일반 오버레이 텍스처를 OpenGL 객체로 유지한다.
    std::uint32_t m_overlayTexture = 0;
    std::uint32_t m_overlayTextureWidth = 0;
    std::uint32_t m_overlayTextureHeight = 0;
    std::vector<std::uint8_t> m_overlayUploadPixels;
    bool m_hasMouseScale = false;
    bool m_runtimeInitialized = false;
    bool m_hasAbsoluteWorldTransform = false;
    // 위치와 정면 회전을 같은 행렬에 합쳐 한 번만 제출한다. 직전 런타임 조회값으로 이동을 덮어쓰지 않는다.
    vr::HmdMatrix34_t m_absoluteWorldTransform{};
    bool m_isGripDragging = false;
    keyboard::ControllerHand m_dragHand = keyboard::ControllerHand::Right;
    vr::HmdMatrix34_t m_dragControllerToOverlay{};
    float m_overlayWidthMeters = 1.45f;
    std::chrono::steady_clock::time_point m_lastGripManipulationUpdate{};
    std::array<bool, 2> m_gripAwaitingRelease{};
    PointerCallback m_pointerCallback;
    InteractionStatusCallback m_interactionStatusCallback;
    std::string m_lastInteractionStatus;
    keyboard::ControllerHand m_captureHand = keyboard::ControllerHand::Right;
    bool m_hasCaptureHand = false;
    bool m_selectWasPressed = false;
    bool m_pointerInsideOverlay = false;
    int m_lastPointerX = -1;
    int m_lastPointerY = -1;
};
