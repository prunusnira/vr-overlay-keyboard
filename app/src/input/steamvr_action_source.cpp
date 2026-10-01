#include "steamvr_action_source.h"

#include <cstddef>
#include <utility>

namespace {
constexpr char kActionSetPath[] = "/actions/keyboard";
constexpr char kToggleActionPath[] = "/actions/keyboard/in/ToggleKeyboard";
constexpr char kPointerPoseActionPath[] = "/actions/keyboard/in/ControllerPose";
constexpr char kPointerClickActionPath[] = "/actions/keyboard/in/PointerClick";

void setError(std::string *error, const char *operation, vr::EVRInputError result) {
    if (error) {
        *error = std::string(operation) + " failed with OpenVR input error " + std::to_string(result) + ".";
    }
}
}

bool SteamVrActionSource::initialize(const std::string &absoluteManifestPath,
                                     ToggleCallback toggleCallback,
                                     PointerCallback pointerCallback,
                                     std::string *error) {
    shutdown();
    m_input = vr::VRInput();
    if (!m_input) {
        if (error) {
            *error = "SteamVR did not provide IVRInput.";
        }
        return false;
    }

    // 시작 시 manifest의 문자열 경로를 OpenVR handle로 바꿔 poll마다 경로 검색을 반복하지 않는다.
    vr::EVRInputError result = m_input->SetActionManifestPath(absoluteManifestPath.c_str());
    if (result != vr::VRInputError_None) {
        setError(error, "SetActionManifestPath", result);
        shutdown();
        return false;
    }
    result = m_input->GetActionSetHandle(kActionSetPath, &m_actionSet);
    if (result != vr::VRInputError_None) {
        setError(error, "GetActionSetHandle", result);
        shutdown();
        return false;
    }
    result = m_input->GetActionHandle(kToggleActionPath, &m_toggleAction);
    if (result != vr::VRInputError_None) {
        setError(error, "GetActionHandle", result);
        shutdown();
        return false;
    }
    result = m_input->GetActionHandle(kPointerPoseActionPath, &m_pointerPoseAction);
    if (result != vr::VRInputError_None) {
        setError(error, "GetActionHandle(ControllerPose)", result);
        shutdown();
        return false;
    }
    result = m_input->GetActionHandle(kPointerClickActionPath, &m_pointerClickAction);
    if (result != vr::VRInputError_None) {
        setError(error, "GetActionHandle(PointerClick)", result);
        shutdown();
        return false;
    }
    result = m_input->GetInputSourceHandle("/user/hand/left", &m_leftHandSource);
    if (result != vr::VRInputError_None) {
        setError(error, "GetInputSourceHandle(left hand)", result);
        shutdown();
        return false;
    }
    result = m_input->GetInputSourceHandle("/user/hand/right", &m_rightHandSource);
    if (result != vr::VRInputError_None) {
        setError(error, "GetInputSourceHandle(right hand)", result);
        shutdown();
        return false;
    }

    m_toggleCallback = std::move(toggleCallback);
    m_pointerCallback = std::move(pointerCallback);
    m_initialized = true;
    return true;
}

void SteamVrActionSource::shutdown() {
    m_toggleCallback = {};
    m_pointerCallback = {};
    m_input = nullptr;
    m_actionSet = vr::k_ulInvalidActionSetHandle;
    m_toggleAction = vr::k_ulInvalidActionHandle;
    m_pointerPoseAction = vr::k_ulInvalidActionHandle;
    m_pointerClickAction = vr::k_ulInvalidActionHandle;
    m_leftHandSource = vr::k_ulInvalidInputValueHandle;
    m_rightHandSource = vr::k_ulInvalidInputValueHandle;
    m_initialized = false;
}

bool SteamVrActionSource::poll(std::string *error) {
    if (!m_initialized || !m_input) {
        return false;
    }

    vr::VRActiveActionSet_t activeSet{};
    // 액션 데이터를 읽기 전에 현재 앱의 액션 세트를 SteamVR Input에 반영한다.
    activeSet.ulActionSet = m_actionSet;
    activeSet.ulRestrictedToDevice = vr::k_ulInvalidInputValueHandle;
    activeSet.ulSecondaryActionSet = vr::k_ulInvalidActionSetHandle;
    vr::EVRInputError result = m_input->UpdateActionState(&activeSet, sizeof(activeSet), 1);
    if (result != vr::VRInputError_None) {
        setError(error, "UpdateActionState", result);
        return false;
    }

    vr::InputDigitalActionData_t actionData{};
    result = m_input->GetDigitalActionData(m_toggleAction,
                                           &actionData,
                                           sizeof(actionData),
                                           vr::k_ulInvalidInputValueHandle);
    if (result != vr::VRInputError_None) {
        setError(error, "GetDigitalActionData", result);
        return false;
    }

    // bChanged와 눌림 상태를 함께 확인해 버튼을 누르고 있는 동안 토글이 반복되지 않게 한다.
    if (actionData.bActive && actionData.bState && actionData.bChanged && m_toggleCallback) {
        m_toggleCallback();
    }

    if (m_pointerCallback) {
        const vr::VRInputValueHandle_t sources[] = {m_leftHandSource, m_rightHandSource};
        const keyboard::ControllerHand hands[] = {keyboard::ControllerHand::Left, keyboard::ControllerHand::Right};
        keyboard::ControllerPointerSamples samples;
        for (std::size_t index = 0; index < 2; ++index) {
            vr::InputPoseActionData_t poseData{};
            const vr::EVRInputError poseResult = m_input->GetPoseActionDataRelativeToNow(
                m_pointerPoseAction,
                vr::TrackingUniverseStanding,
                0.0f,
                &poseData,
                sizeof(poseData),
                sources[index]);

            vr::InputDigitalActionData_t clickData{};
            const vr::EVRInputError clickResult = m_input->GetDigitalActionData(
                m_pointerClickAction,
                &clickData,
                sizeof(clickData),
                sources[index]);

            keyboard::ControllerPointerSample &sample = samples.hands[index];
            sample.hand = hands[index];
            sample.poseActive = poseResult == vr::VRInputError_None && poseData.bActive && poseData.pose.bPoseIsValid;
            sample.selectPressed = clickResult == vr::VRInputError_None && clickData.bActive && clickData.bState;
            if (sample.poseActive) {
                const vr::HmdMatrix34_t &transform = poseData.pose.mDeviceToAbsoluteTracking;
                sample.origin = {transform.m[0][3], transform.m[1][3], transform.m[2][3]};
                // OpenVR 추적 포즈의 로컬 전방은 -Z 방향이다.
                sample.direction = {-transform.m[0][2], -transform.m[1][2], -transform.m[2][2]};
            }
        }
        m_pointerCallback(samples);
    }
    return true;
}
