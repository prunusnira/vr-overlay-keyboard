#pragma once

#include "../core/app_contracts.h"

#include <openvr.h>

#include <functional>
#include <string>

class SteamVrActionSource final {
public:
    using ToggleCallback = std::function<void()>;
    using PointerCallback = std::function<void(const keyboard::ControllerPointerSamples &)>;

    bool initialize(const std::string &absoluteManifestPath,
                    ToggleCallback toggleCallback,
                    PointerCallback pointerCallback,
                    std::string *error);
    void shutdown();
    bool poll(std::string *error);

private:
    vr::IVRInput *m_input = nullptr;
    vr::VRActionSetHandle_t m_actionSet = vr::k_ulInvalidActionSetHandle;
    vr::VRActionHandle_t m_toggleAction = vr::k_ulInvalidActionHandle;
    vr::VRActionHandle_t m_pointerPoseAction = vr::k_ulInvalidActionHandle;
    vr::VRActionHandle_t m_pointerClickAction = vr::k_ulInvalidActionHandle;
    vr::VRInputValueHandle_t m_leftHandSource = vr::k_ulInvalidInputValueHandle;
    vr::VRInputValueHandle_t m_rightHandSource = vr::k_ulInvalidInputValueHandle;
    ToggleCallback m_toggleCallback;
    PointerCallback m_pointerCallback;
    bool m_initialized = false;
};
