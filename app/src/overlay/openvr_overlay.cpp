#include "openvr_overlay.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace {
constexpr char kOverlayKey[] = "com.prunusnira.vr-overlay-keyboard.main";
constexpr char kOverlayName[] = "VR Overlay Keyboard";
constexpr float kOverlayWidthMeters = 1.45f;
constexpr float kOverlayDistanceMeters = 1.25f;

void setRuntimeError(std::string *error, vr::EVRInitError code) {
    if (error) {
        *error = vr::VR_GetVRInitErrorAsEnglishDescription(code);
    }
}
}

OpenVrOverlay::~OpenVrOverlay() {
    shutdown();
}

bool OpenVrOverlay::initialize(std::string *error) {
    shutdown();

    vr::EVRInitError initError = vr::VRInitError_None;
    vr::IVRSystem *system = vr::VR_Init(&initError, vr::VRApplication_Overlay);
    if (initError != vr::VRInitError_None || !system) {
        setRuntimeError(error, initError);
        return false;
    }
    m_runtimeInitialized = true;
    m_overlay = vr::VROverlay();
    if (!m_overlay) {
        if (error) {
            *error = "SteamVR did not provide IVROverlay.";
        }
        shutdown();
        return false;
    }

    const vr::VROverlayError createError = m_overlay->CreateOverlay(kOverlayKey, kOverlayName, &m_handle);
    if (createError != vr::VROverlayError_None) {
        failWithOverlayError("CreateOverlay", createError, error);
        shutdown();
        return false;
    }

    vr::HmdMatrix34_t transform{};
    transform.m[0][0] = 1.0f;
    transform.m[1][1] = 1.0f;
    transform.m[2][2] = 1.0f;
    transform.m[2][3] = -kOverlayDistanceMeters;

    // 일반 오버레이를 대시보드 탭이 아닌 HMD 기준 앞쪽 위치에 고정한다.
    const vr::VROverlayError transformError = m_overlay->SetOverlayTransformTrackedDeviceRelative(
        m_handle, vr::k_unTrackedDeviceIndex_Hmd, &transform);
    if (transformError != vr::VROverlayError_None) {
        failWithOverlayError("SetOverlayTransformTrackedDeviceRelative", transformError, error);
        shutdown();
        return false;
    }

    const vr::VROverlayError widthError = m_overlay->SetOverlayWidthInMeters(m_handle, kOverlayWidthMeters);
    if (widthError != vr::VROverlayError_None) {
        failWithOverlayError("SetOverlayWidthInMeters", widthError, error);
        shutdown();
        return false;
    }
    const vr::VROverlayError inputError = m_overlay->SetOverlayInputMethod(m_handle, vr::VROverlayInputMethod_Mouse);
    if (inputError != vr::VROverlayError_None) {
        failWithOverlayError("SetOverlayInputMethod", inputError, error);
        shutdown();
        return false;
    }
    const vr::VROverlayError alphaError = m_overlay->SetOverlayAlpha(m_handle, 1.0f);
    if (alphaError != vr::VROverlayError_None) {
        failWithOverlayError("SetOverlayAlpha", alphaError, error);
        shutdown();
        return false;
    }

    return true;
}

void OpenVrOverlay::shutdown() {
    resetPointerState();
    if (m_overlay && m_handle != vr::k_ulOverlayHandleInvalid) {
        m_overlay->HideOverlay(m_handle);
        m_overlay->DestroyOverlay(m_handle);
    }
    m_handle = vr::k_ulOverlayHandleInvalid;
    m_overlay = nullptr;
    m_pointerCallback = {};
    if (m_runtimeInitialized) {
        vr::VR_Shutdown();
        m_runtimeInitialized = false;
    }
}

bool OpenVrOverlay::show(std::string *error) {
    if (!m_overlay || m_handle == vr::k_ulOverlayHandleInvalid) {
        if (error) {
            *error = "The OpenVR overlay is not initialized.";
        }
        return false;
    }
    const vr::VROverlayError result = m_overlay->ShowOverlay(m_handle);
    return result == vr::VROverlayError_None || failWithOverlayError("ShowOverlay", result, error);
}

bool OpenVrOverlay::hide(std::string *error) {
    if (!m_overlay || m_handle == vr::k_ulOverlayHandleInvalid) {
        if (error) {
            *error = "The OpenVR overlay is not initialized.";
        }
        return false;
    }
    const vr::VROverlayError result = m_overlay->HideOverlay(m_handle);
    if (result != vr::VROverlayError_None) {
        return failWithOverlayError("HideOverlay", result, error);
    }
    resetPointerState();
    return true;
}

bool OpenVrOverlay::isVisible() const {
    return m_overlay && m_handle != vr::k_ulOverlayHandleInvalid && m_overlay->IsOverlayVisible(m_handle);
}

bool OpenVrOverlay::updateTexture(const keyboard::ImageFrame &frame, std::string *error) {
    const std::size_t expectedBytes = static_cast<std::size_t>(frame.width) * frame.height * 4;
    if (!m_overlay || m_handle == vr::k_ulOverlayHandleInvalid || frame.width == 0 || frame.height == 0 ||
        frame.rgbaPixels.size() != expectedBytes) {
        if (error) {
            *error = "The OpenVR overlay is not initialized or the RGBA frame dimensions are invalid.";
        }
        return false;
    }

    m_textureWidth = frame.width;
    m_textureHeight = frame.height;
    // 포인터 UV를 같은 RGBA 프레임의 픽셀 좌표로 환산할 수 있도록 OpenVR 입력 크기를 갱신한다.
    const vr::HmdVector2_t mouseScale{{static_cast<float>(m_textureWidth), static_cast<float>(m_textureHeight)}};
    const vr::VROverlayError scaleError = m_overlay->SetOverlayMouseScale(m_handle, &mouseScale);
    if (scaleError != vr::VROverlayError_None) {
        return failWithOverlayError("SetOverlayMouseScale", scaleError, error);
    }

    const vr::VROverlayError textureError = m_overlay->SetOverlayRaw(
        m_handle,
        const_cast<std::uint8_t *>(frame.rgbaPixels.data()),
        m_textureWidth,
        m_textureHeight,
        4);
    return textureError == vr::VROverlayError_None || failWithOverlayError("SetOverlayRaw", textureError, error);
}

void OpenVrOverlay::handleControllerPointers(const keyboard::ControllerPointerSamples &samples) {
    if (!m_overlay || m_handle == vr::k_ulOverlayHandleInvalid || !isVisible()) {
        return;
    }

    const keyboard::ControllerPointerSample *leftSample = nullptr;
    const keyboard::ControllerPointerSample *rightSample = nullptr;
    for (const keyboard::ControllerPointerSample &candidate : samples.hands) {
        if (candidate.hand == keyboard::ControllerHand::Left) {
            leftSample = &candidate;
        } else if (candidate.hand == keyboard::ControllerHand::Right) {
            rightSample = &candidate;
        }
    }

    const keyboard::ControllerPointerSample *sample = nullptr;
    int x = -1;
    int y = -1;
    bool intersects = false;
    if (m_hasCaptureHand) {
        sample = m_captureHand == keyboard::ControllerHand::Left ? leftSample : rightSample;
        if (!sample) {
            // 누르는 동안 손 추적이 사라지면 ImGui에 취소 이벤트를 보내 버튼 눌림 상태를 해제한다.
            resetPointerState();
            return;
        }
        intersects = computePointerPosition(*sample, &x, &y);
    } else {
        int leftX = -1;
        int leftY = -1;
        int rightX = -1;
        int rightY = -1;
        const bool leftIntersects = leftSample && computePointerPosition(*leftSample, &leftX, &leftY);
        const bool rightIntersects = rightSample && computePointerPosition(*rightSample, &rightX, &rightY);

        if (rightIntersects && rightSample->selectPressed) {
            sample = rightSample;
            x = rightX;
            y = rightY;
        } else if (leftIntersects && leftSample->selectPressed) {
            sample = leftSample;
            x = leftX;
            y = leftY;
        } else if (m_hasHoverHand && m_hoverHand == keyboard::ControllerHand::Left && leftIntersects) {
            sample = leftSample;
            x = leftX;
            y = leftY;
        } else if (m_hasHoverHand && m_hoverHand == keyboard::ControllerHand::Right && rightIntersects) {
            sample = rightSample;
            x = rightX;
            y = rightY;
        } else if (rightIntersects) {
            sample = rightSample;
            x = rightX;
            y = rightY;
        } else if (leftIntersects) {
            sample = leftSample;
            x = leftX;
            y = leftY;
        }

        if (!sample) {
            m_hasHoverHand = false;
            m_pointerInsideOverlay = false;
            return;
        }
        m_hoverHand = sample->hand;
        m_hasHoverHand = true;
        intersects = true;
    }

    if (intersects) {
        dispatchPointerEvent(keyboard::PointerEventType::Move, x, y);
        m_pointerInsideOverlay = true;
        m_lastPointerX = x;
        m_lastPointerY = y;
        if (!m_hasCaptureHand && sample->selectPressed) {
            m_captureHand = sample->hand;
            m_hasCaptureHand = true;
            m_selectWasPressed = true;
            dispatchPointerEvent(keyboard::PointerEventType::Press, x, y, keyboard::PointerButton::Left);
        }
    } else {
        if (m_pointerInsideOverlay) {
            dispatchPointerEvent(keyboard::PointerEventType::Leave, -1, -1);
            m_pointerInsideOverlay = false;
        }
        m_lastPointerX = -1;
        m_lastPointerY = -1;
    }

    if (m_hasCaptureHand && m_selectWasPressed && !sample->selectPressed) {
        if (m_pointerInsideOverlay) {
            dispatchPointerEvent(keyboard::PointerEventType::Release,
                                 m_lastPointerX,
                                 m_lastPointerY,
                                 keyboard::PointerButton::Left);
        } else {
            dispatchPointerEvent(keyboard::PointerEventType::Cancel, -1, -1);
        }
        m_selectWasPressed = false;
        resetPointerState();
    }
}

void OpenVrOverlay::setPointerCallback(PointerCallback callback) {
    m_pointerCallback = std::move(callback);
}

bool OpenVrOverlay::computePointerPosition(const keyboard::ControllerPointerSample &sample, int *x, int *y) const {
    if (!m_overlay || !sample.poseActive || !x || !y || m_textureWidth == 0 || m_textureHeight == 0) {
        return false;
    }
    for (float coordinate : sample.origin) {
        if (!std::isfinite(coordinate)) {
            return false;
        }
    }
    for (float component : sample.direction) {
        if (!std::isfinite(component)) {
            return false;
        }
    }
    const float directionLengthSquared = sample.direction[0] * sample.direction[0] +
                                         sample.direction[1] * sample.direction[1] +
                                         sample.direction[2] * sample.direction[2];
    if (directionLengthSquared < 0.0001f) {
        return false;
    }

    vr::VROverlayIntersectionParams_t ray{};
    ray.vSource.v[0] = sample.origin[0];
    ray.vSource.v[1] = sample.origin[1];
    ray.vSource.v[2] = sample.origin[2];
    ray.vDirection.v[0] = sample.direction[0];
    ray.vDirection.v[1] = sample.direction[1];
    ray.vDirection.v[2] = sample.direction[2];
    ray.eOrigin = vr::TrackingUniverseStanding;

    vr::VROverlayIntersectionResults_t intersection{};
    if (!m_overlay->ComputeOverlayIntersection(m_handle, &ray, &intersection)) {
        return false;
    }

    // OpenVR UV의 세로축과 화면 좌표계 방향을 맞춰 ImGui client pixel로 변환한다.
    const float pixelX = intersection.vUVs.v[0] * static_cast<float>(m_textureWidth);
    const float pixelY = (1.0f - intersection.vUVs.v[1]) * static_cast<float>(m_textureHeight);
    *x = static_cast<int>(std::clamp(pixelX, 0.0f, static_cast<float>(m_textureWidth - 1)));
    *y = static_cast<int>(std::clamp(pixelY, 0.0f, static_cast<float>(m_textureHeight - 1)));
    return true;
}

void OpenVrOverlay::dispatchPointerEvent(keyboard::PointerEventType type,
                                        int x,
                                        int y,
                                        keyboard::PointerButton button) {
    if (!m_pointerCallback) {
        return;
    }
    keyboard::PointerEvent pointer;
    pointer.type = type;
    pointer.button = button;
    pointer.x = x;
    pointer.y = y;
    m_pointerCallback(pointer);
}

void OpenVrOverlay::resetPointerState() {
    if (m_selectWasPressed) {
        dispatchPointerEvent(keyboard::PointerEventType::Cancel, -1, -1);
    }
    m_hasCaptureHand = false;
    m_hasHoverHand = false;
    m_selectWasPressed = false;
    m_pointerInsideOverlay = false;
    m_lastPointerX = -1;
    m_lastPointerY = -1;
}

bool OpenVrOverlay::failWithOverlayError(const char *operation,
                                         vr::VROverlayError error,
                                         std::string *message) const {
    if (message) {
        const char *description = m_overlay ? m_overlay->GetOverlayErrorNameFromEnum(error) : "unknown OpenVR error";
        *message = std::string(operation) + " failed: " + description;
    }
    return false;
}
