#include "openvr_overlay.h"

#include <Windows.h>
#include <GL/gl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <utility>

namespace {
constexpr char kOverlayKey[] = "com.prunusnira.vr-overlay-keyboard.main";
constexpr char kOverlayName[] = "VR Overlay Keyboard";
constexpr float kOverlayWidthMeters = 1.45f;
constexpr float kOverlayDistanceMeters = 1.25f;
constexpr float kStickDeadzone = 0.18f;
constexpr float kMinimumOverlayWidthMeters = 0.55f;
constexpr float kMaximumOverlayWidthMeters = 2.80f;
constexpr float kOverlayZoomMetersPerSecond = 0.90f;
constexpr float kMinimumGripDistanceMeters = 0.25f;
constexpr float kMaximumGripDistanceMeters = 4.00f;
constexpr float kGripDistanceMetersPerSecond = 0.80f;
constexpr float kMaximumManipulationDeltaSeconds = 0.05f;

float applyStickDeadzone(float value) {
    if (!std::isfinite(value)) {
        return 0.0f;
    }
    const float magnitude = std::abs(value);
    if (magnitude <= kStickDeadzone) {
        return 0.0f;
    }
    const float scaledMagnitude = (magnitude - kStickDeadzone) / (1.0f - kStickDeadzone);
    return std::copysign(scaledMagnitude, value);
}

float normalizedPointerOffset(float offsetPercent) {
    if (!std::isfinite(offsetPercent)) {
        return 0.0f;
    }
    return std::clamp(offsetPercent / 100.0f,
                       -keyboard::kMaximumPointerOffsetPercent / 100.0f,
                       keyboard::kMaximumPointerOffsetPercent / 100.0f);
}

void setRuntimeError(std::string *error, vr::EVRInitError code) {
    if (error) {
        *error = vr::VR_GetVRInitErrorAsEnglishDescription(code);
    }
}

vr::HmdMatrix34_t composeTransforms(const vr::HmdMatrix34_t &left,
                                    const vr::HmdMatrix34_t &right) {
    vr::HmdMatrix34_t result{};
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            for (int axis = 0; axis < 3; ++axis) {
                result.m[row][column] += left.m[row][axis] * right.m[axis][column];
            }
        }
        result.m[row][3] = left.m[row][3];
        for (int axis = 0; axis < 3; ++axis) {
            result.m[row][3] += left.m[row][axis] * right.m[axis][3];
        }
    }
    return result;
}

bool invertRigidTransform(const vr::HmdMatrix34_t &transform, vr::HmdMatrix34_t *inverse) {
    if (!inverse) {
        return false;
    }
    *inverse = {};
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            inverse->m[row][column] = transform.m[column][row];
        }
        inverse->m[row][3] = -(inverse->m[row][0] * transform.m[0][3] +
                               inverse->m[row][1] * transform.m[1][3] +
                               inverse->m[row][2] * transform.m[2][3]);
    }
    return true;
}

vr::HmdMatrix34_t controllerTransform(const keyboard::ControllerPointerSample &sample) {
    vr::HmdMatrix34_t transform{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            transform.m[row][column] = sample.deviceToAbsoluteTracking[row * 4 + column];
        }
    }
    return transform;
}

bool isFiniteTransform(const vr::HmdMatrix34_t &transform) {
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 4; ++column) {
            if (!std::isfinite(transform.m[row][column])) {
                return false;
            }
        }
    }
    return true;
}

std::size_t handIndex(keyboard::ControllerHand hand) {
    return hand == keyboard::ControllerHand::Left ? 0 : 1;
}
}

OpenVrOverlay::~OpenVrOverlay() {
    shutdown();
}

bool OpenVrOverlay::initialize(std::string *error) {
    shutdown();

    vr::EVRInitError initError = vr::VRInitError_None;
    m_system = vr::VR_Init(&initError, vr::VRApplication_Overlay);
    if (initError != vr::VRInitError_None || !m_system) {
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

    const vr::VROverlayError widthError = m_overlay->SetOverlayWidthInMeters(m_handle, kOverlayWidthMeters);
    if (widthError != vr::VROverlayError_None) {
        failWithOverlayError("SetOverlayWidthInMeters", widthError, error);
        shutdown();
        return false;
    }
    m_overlayWidthMeters = kOverlayWidthMeters;
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
    m_isGripDragging = false;
    m_gripAwaitingRelease = {};
    if (m_overlay && m_handle != vr::k_ulOverlayHandleInvalid) {
        m_overlay->HideOverlay(m_handle);
        m_overlay->DestroyOverlay(m_handle);
    }
    m_handle = vr::k_ulOverlayHandleInvalid;
    m_overlay = nullptr;
    m_system = nullptr;
    m_hasAbsoluteWorldTransform = false;
    m_absoluteWorldTransform = {};
    m_overlayWidthMeters = kOverlayWidthMeters;
    m_lastGripManipulationUpdate = {};
    m_hasMouseScale = false;
    m_pointerCallback = {};
    m_interactionStatusCallback = {};
    m_lastInteractionStatus.clear();
    if (m_overlayTexture != 0) {
        const GLuint texture = static_cast<GLuint>(m_overlayTexture);
        glDeleteTextures(1, &texture);
        m_overlayTexture = 0;
        m_overlayTextureWidth = 0;
        m_overlayTextureHeight = 0;
    }
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
    if (!m_hasAbsoluteWorldTransform && !placeInFrontOfHead(error)) {
        return false;
    }
    const vr::VROverlayError result = m_overlay->ShowOverlay(m_handle);
    if (result != vr::VROverlayError_None) {
        return failWithOverlayError("ShowOverlay", result, error);
    }
    // 소환 Grip을 계속 누른 채로 막 나타난 오버레이를 즉시 끌고 가지 않도록 먼저 손을 놓게 한다.
    m_gripAwaitingRelease = {true, true};
    return true;
}

bool OpenVrOverlay::placeInFrontOfHead(std::string *error) {
    if (!m_system || !m_overlay) {
        if (error) {
            *error = "SteamVR tracking is not initialized.";
        }
        return false;
    }

    std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> poses{};
    m_system->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding,
                                              0.0f,
                                              poses.data(),
                                              static_cast<std::uint32_t>(poses.size()));
    const vr::TrackedDevicePose_t &headPose = poses[vr::k_unTrackedDeviceIndex_Hmd];
    if (!headPose.bPoseIsValid || !headPose.bDeviceIsConnected ||
        !isFiniteTransform(headPose.mDeviceToAbsoluteTracking)) {
        if (error) {
            *error = "The HMD pose is not available; the overlay could not be placed in the room.";
        }
        return false;
    }

    vr::HmdMatrix34_t headToOverlay{};
    headToOverlay.m[0][0] = 1.0f;
    headToOverlay.m[1][1] = 1.0f;
    headToOverlay.m[2][2] = 1.0f;
    headToOverlay.m[2][3] = -kOverlayDistanceMeters;
    const vr::HmdMatrix34_t worldTransform = composeTransforms(
        headPose.mDeviceToAbsoluteTracking, headToOverlay);
    if (!setAbsoluteTransform(worldTransform, "SetOverlayTransformAbsolute", error)) {
        return false;
    }
    m_hasAbsoluteWorldTransform = true;
    return true;
}

bool OpenVrOverlay::setAbsoluteTransform(const vr::HmdMatrix34_t &transform,
                                         const char *operation,
                                         std::string *error) {
    if (!m_overlay || m_handle == vr::k_ulOverlayHandleInvalid) {
        if (error) {
            *error = "The OpenVR overlay is not initialized.";
        }
        return false;
    }
    if (!isFiniteTransform(transform)) {
        if (error) {
            *error = "The requested overlay transform contains an invalid coordinate.";
        }
        return false;
    }
    const vr::VROverlayError result = m_overlay->SetOverlayTransformAbsolute(
        m_handle, vr::TrackingUniverseStanding, &transform);
    if (result != vr::VROverlayError_None) {
        return failWithOverlayError(operation, result, error);
    }
    m_absoluteWorldTransform = transform;
    m_hasAbsoluteWorldTransform = true;
    return true;
}

void OpenVrOverlay::orientTowardsUser(vr::HmdMatrix34_t *transform) const {
    if (!m_system || !transform || !isFiniteTransform(*transform)) {
        return;
    }
    const vr::HmdMatrix34_t &overlayTransform = *transform;

    std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> poses{};
    m_system->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding,
                                              0.0f,
                                              poses.data(),
                                              static_cast<std::uint32_t>(poses.size()));
    const vr::TrackedDevicePose_t &headPose = poses[vr::k_unTrackedDeviceIndex_Hmd];
    if (!headPose.bPoseIsValid || !headPose.bDeviceIsConnected ||
        !isFiniteTransform(headPose.mDeviceToAbsoluteTracking)) {
        return;
    }

    // 월드 좌표의 위치는 유지하고, 오버레이 정면(+Z)만 HMD 위치를 향하게 한다.
    std::array<float, 3> towardHead = {
        headPose.mDeviceToAbsoluteTracking.m[0][3] - overlayTransform.m[0][3],
        headPose.mDeviceToAbsoluteTracking.m[1][3] - overlayTransform.m[1][3],
        headPose.mDeviceToAbsoluteTracking.m[2][3] - overlayTransform.m[2][3],
    };
    const auto normalize = [](std::array<float, 3> *vector) {
        const float length = std::sqrt((*vector)[0] * (*vector)[0] +
                                       (*vector)[1] * (*vector)[1] +
                                       (*vector)[2] * (*vector)[2]);
        if (!std::isfinite(length) || length < 0.0001f) {
            return false;
        }
        for (float &component : *vector) {
            component /= length;
        }
        return true;
    };
    if (!normalize(&towardHead)) {
        return;
    }

    // 화면을 기울이지 않도록 월드 위쪽을 기준으로 좌우·위쪽 축을 다시 만든다.
    std::array<float, 3> worldUp = {0.0f, 1.0f, 0.0f};
    std::array<float, 3> screenRight = {
        worldUp[1] * towardHead[2] - worldUp[2] * towardHead[1],
        worldUp[2] * towardHead[0] - worldUp[0] * towardHead[2],
        worldUp[0] * towardHead[1] - worldUp[1] * towardHead[0],
    };
    if (!normalize(&screenRight)) {
        // 오버레이가 사용자 바로 위나 아래에 있을 때는 기존 좌우 방향을 보존한다.
        screenRight = {overlayTransform.m[0][0], overlayTransform.m[1][0], overlayTransform.m[2][0]};
        const float projection = screenRight[0] * towardHead[0] +
                                 screenRight[1] * towardHead[1] +
                                 screenRight[2] * towardHead[2];
        for (std::size_t axis = 0; axis < screenRight.size(); ++axis) {
            screenRight[axis] -= projection * towardHead[axis];
        }
        if (!normalize(&screenRight)) {
            return;
        }
    }
    std::array<float, 3> screenUp = {
        towardHead[1] * screenRight[2] - towardHead[2] * screenRight[1],
        towardHead[2] * screenRight[0] - towardHead[0] * screenRight[2],
        towardHead[0] * screenRight[1] - towardHead[1] * screenRight[0],
    };
    if (!normalize(&screenUp)) {
        return;
    }

    vr::HmdMatrix34_t facingTransform = overlayTransform;
    for (std::size_t row = 0; row < 3; ++row) {
        facingTransform.m[row][0] = screenRight[row];
        facingTransform.m[row][1] = screenUp[row];
        facingTransform.m[row][2] = towardHead[row];
    }
    // HMD 추적이 유효하지 않으면 전달받은 이동 행렬을 그대로 사용한다.
    *transform = facingTransform;
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
    m_isGripDragging = false;
    m_gripAwaitingRelease = {};
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

    const bool textureSizeChanged = m_textureWidth != frame.width || m_textureHeight != frame.height;
    if (!m_hasMouseScale || textureSizeChanged) {
        // 포인터 UV를 픽셀로 환산할 때 쓰는 OpenVR 입력 크기는 첫 프레임과 크기 변경 때만 갱신한다.
        const vr::HmdVector2_t mouseScale{{static_cast<float>(frame.width), static_cast<float>(frame.height)}};
        const vr::VROverlayError scaleError = m_overlay->SetOverlayMouseScale(m_handle, &mouseScale);
        if (scaleError != vr::VROverlayError_None) {
            return failWithOverlayError("SetOverlayMouseScale", scaleError, error);
        }
        m_hasMouseScale = true;
    }
    m_textureWidth = frame.width;
    m_textureHeight = frame.height;

    GLint previousTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    if (m_overlayTexture == 0) {
        GLuint texture = 0;
        glGenTextures(1, &texture);
        m_overlayTexture = texture;
    }
    if (m_overlayTexture == 0) {
        if (error) {
            *error = "OpenGL could not create the overlay texture.";
        }
        return false;
    }

    // RGBA 프레임은 위쪽 행부터 저장하므로 OpenGL 텍스처의 행 방향에 맞춰 거꾸로 복사한다.
    const std::size_t rowBytes = static_cast<std::size_t>(frame.width) * 4;
    m_overlayUploadPixels.resize(frame.rgbaPixels.size());
    for (std::uint32_t row = 0; row < frame.height; ++row) {
        const std::uint32_t sourceRow = frame.height - row - 1;
        std::memcpy(m_overlayUploadPixels.data() + rowBytes * row,
                    frame.rgbaPixels.data() + rowBytes * sourceRow,
                    rowBytes);
    }

    const GLuint texture = static_cast<GLuint>(m_overlayTexture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    constexpr GLenum kGlClampToEdge = 0x812F;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, kGlClampToEdge);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, kGlClampToEdge);
    if (m_overlayTextureWidth != frame.width || m_overlayTextureHeight != frame.height) {
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA,
                     static_cast<GLsizei>(frame.width),
                     static_cast<GLsizei>(frame.height),
                     0,
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     m_overlayUploadPixels.data());
        m_overlayTextureWidth = frame.width;
        m_overlayTextureHeight = frame.height;
    } else {
        glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        0,
                        0,
                        static_cast<GLsizei>(frame.width),
                        static_cast<GLsizei>(frame.height),
                        GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        m_overlayUploadPixels.data());
    }
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));

    // Raw 바이트 제출은 갱신 사이에 텍스처가 비는 현상이 있어 지속되는 OpenGL 텍스처로 교체한다.
    vr::Texture_t overlayTexture{};
    overlayTexture.handle = reinterpret_cast<void *>(static_cast<std::uintptr_t>(m_overlayTexture));
    overlayTexture.eType = vr::TextureType_OpenGL;
    overlayTexture.eColorSpace = vr::ColorSpace_Auto;
    const vr::VROverlayError textureError = m_overlay->SetOverlayTexture(m_handle, &overlayTexture);
    return textureError == vr::VROverlayError_None ||
        failWithOverlayError("SetOverlayTexture", textureError, error);
}

void OpenVrOverlay::handleControllerPointers(const keyboard::ControllerPointerSamples &samples,
                                             const keyboard::AppSettings &settings) {
    if (!m_overlay || m_handle == vr::k_ulOverlayHandleInvalid || !isVisible()) {
        return;
    }

    std::array<const keyboard::ControllerPointerSample *, 2> handSamples{};
    for (const keyboard::ControllerPointerSample &candidate : samples.hands) {
        handSamples[handIndex(candidate.hand)] = &candidate;
    }

    // 소환에 사용한 Grip만 해제 입력을 기다린다. 일반 이동은 패널을 가리키며 누른 동안 시작할 수 있다.
    for (const keyboard::ControllerPointerSample *hand : handSamples) {
        if (hand && !hand->gripPressed) {
            m_gripAwaitingRelease[handIndex(hand->hand)] = false;
        }
    }

    if (m_isGripDragging) {
        const keyboard::ControllerPointerSample *sample = handSamples[handIndex(m_dragHand)];
        if (!sample || !sample->poseActive || !sample->gripPressed) {
            // Grip을 놓거나 손 추적이 끊기면 현재 절대 좌표에서 이동을 끝낸다.
            m_isGripDragging = false;
            if (sample && sample->gripPressed) {
                m_gripAwaitingRelease[handIndex(sample->hand)] = true;
            }
            resetPointerState();
            reportInteractionStatus("Grip drag ended; the overlay position is fixed.");
            return;
        }
        std::string dragError;
        if (!updateGripDrag(*sample, &dragError)) {
            m_isGripDragging = false;
            m_gripAwaitingRelease[handIndex(sample->hand)] = true;
            resetPointerState();
            reportInteractionStatus("Grip drag failed: " + dragError);
            return;
        }
        // 이동 중에는 같은 손의 광선이 UI를 누르지 않도록 포인터 입력을 잠시 멈춘다.
        resetPointerState();
        return;
    }

    std::array<int, 2> pointerX{{-1, -1}};
    std::array<int, 2> pointerY{{-1, -1}};
    std::array<bool, 2> intersects{};
    std::array<bool, 2> rayHitsVisiblePanel{};
    for (std::size_t index = 0; index < handSamples.size(); ++index) {
        const keyboard::ControllerPointerSample *sample = handSamples[index];
        if (sample && sample->poseActive) {
            intersects[index] = computePointerPosition(*sample,
                                                       settings.pointerOffsetXPercent,
                                                       settings.pointerOffsetYPercent,
                                                       &pointerX[index],
                                                       &pointerY[index],
                                                       &rayHitsVisiblePanel[index]);
        }
    }

    const bool leftGripReady = handSamples[0] && rayHitsVisiblePanel[0] &&
        handSamples[0]->gripPressed && !m_gripAwaitingRelease[0];
    const bool rightGripReady = handSamples[1] && rayHitsVisiblePanel[1] &&
        handSamples[1]->gripPressed && !m_gripAwaitingRelease[1];
    const keyboard::ControllerPointerSample *gripSample = leftGripReady
        ? handSamples[0]
        : rightGripReady ? handSamples[1] : nullptr;
    if (gripSample) {
        // 트리거로 UI를 누른 상태여도 Grip 이동을 먼저 처리하고 기존 클릭은 취소한다.
        std::string dragError;
        if (beginGripDrag(*gripSample, &dragError)) {
            m_isGripDragging = true;
            resetPointerState();
            reportInteractionStatus("Grip drag started.");
        } else {
            m_gripAwaitingRelease[handIndex(gripSample->hand)] = true;
            reportInteractionStatus("Grip drag could not start: " + dragError);
        }
        return;
    }

    for (std::size_t index = 0; index < handSamples.size(); ++index) {
        const keyboard::ControllerHand hand = index == 0
            ? keyboard::ControllerHand::Left
            : keyboard::ControllerHand::Right;
        const keyboard::ControllerPointerSample *sample = handSamples[index];
        if (!sample || !sample->poseActive) {
            // 한 손의 추적이 끊겨도 다른 손의 포인터와 클릭은 유지한다.
            resetPointerState(hand);
            continue;
        }

        if (intersects[index]) {
            dispatchPointerEvent(hand, keyboard::PointerEventType::Move, pointerX[index], pointerY[index]);
            m_pointerInsideOverlay[index] = true;
            if (!m_selectWasPressed[index] && sample->selectPressed) {
                m_selectWasPressed[index] = true;
                dispatchPointerEvent(hand, keyboard::PointerEventType::Press,
                                     pointerX[index], pointerY[index], keyboard::PointerButton::Left);
            }
        } else {
            if (m_pointerInsideOverlay[index]) {
                dispatchPointerEvent(hand, keyboard::PointerEventType::Leave, -1, -1);
                m_pointerInsideOverlay[index] = false;
            }
        }

        if (m_selectWasPressed[index] && !sample->selectPressed) {
            if (intersects[index]) {
                dispatchPointerEvent(hand, keyboard::PointerEventType::Release,
                                     pointerX[index], pointerY[index], keyboard::PointerButton::Left);
            } else {
                dispatchPointerEvent(hand, keyboard::PointerEventType::Cancel, -1, -1);
                m_pointerInsideOverlay[index] = false;
            }
            m_selectWasPressed[index] = false;
        }
    }

    // 먼저 현재 화면 기준으로 포인터·Grip 판정을 마친 뒤 다음 갱신에 쓸 방향을 맞춘다.
    const bool anyControllerSelectPressed =
        m_selectWasPressed[0] || m_selectWasPressed[1];
    if (!m_isGripDragging && !anyControllerSelectPressed) {
        vr::HmdMatrix34_t facingTransform = m_absoluteWorldTransform;
        orientTowardsUser(&facingTransform);
        std::string transformError;
        if (!setAbsoluteTransform(facingTransform, "SetOverlayTransformAbsolute", &transformError)) {
            reportInteractionStatus("Overlay rotation failed: " + transformError);
        }
    }
}

bool OpenVrOverlay::beginGripDrag(const keyboard::ControllerPointerSample &sample, std::string *error) {
    if (!m_overlay || !m_hasAbsoluteWorldTransform || !sample.poseActive || !sample.gripPressed) {
        if (error) {
            *error = "The controller pose or absolute overlay position is unavailable.";
        }
        return false;
    }
    const vr::HmdMatrix34_t &overlayTransform = m_absoluteWorldTransform;

    const vr::HmdMatrix34_t controllerToWorld = controllerTransform(sample);
    if (!isFiniteTransform(controllerToWorld) || !isFiniteTransform(overlayTransform)) {
        if (error) {
            *error = "The controller or overlay transform contains an invalid coordinate.";
        }
        return false;
    }
    vr::HmdMatrix34_t worldToController{};
    if (!invertRigidTransform(controllerToWorld, &worldToController)) {
        return false;
    }
    // 누른 순간의 오버레이와 컨트롤러 사이 간격을 보존해 잡을 때 화면이 튀지 않게 한다.
    m_dragControllerToOverlay = composeTransforms(worldToController, overlayTransform);
    m_dragHand = sample.hand;
    m_hasAbsoluteWorldTransform = true;
    m_lastGripManipulationUpdate = std::chrono::steady_clock::now();
    return true;
}

bool OpenVrOverlay::updateGripDrag(const keyboard::ControllerPointerSample &sample, std::string *error) {
    if (!sample.poseActive || !sample.gripPressed) {
        return false;
    }

    const auto now = std::chrono::steady_clock::now();
    const float elapsedSeconds = std::clamp(
        std::chrono::duration<float>(now - m_lastGripManipulationUpdate).count(),
        0.0f,
        kMaximumManipulationDeltaSeconds);
    m_lastGripManipulationUpdate = now;

    const float stickX = sample.manipulationStickActive
        ? applyStickDeadzone(sample.manipulationStickX) : 0.0f;
    const float stickY = sample.manipulationStickActive
        ? applyStickDeadzone(sample.manipulationStickY) : 0.0f;

    // 스틱 좌우는 패널 폭을 바꾸고, 위쪽 입력은 컨트롤러 앞 방향으로 거리를 늘린다.
    if (stickX != 0.0f && elapsedSeconds > 0.0f) {
        const float requestedWidth = std::clamp(
            m_overlayWidthMeters + stickX * kOverlayZoomMetersPerSecond * elapsedSeconds,
            kMinimumOverlayWidthMeters,
            kMaximumOverlayWidthMeters);
        if (std::abs(requestedWidth - m_overlayWidthMeters) > 0.0001f) {
            const vr::VROverlayError widthError = m_overlay->SetOverlayWidthInMeters(m_handle, requestedWidth);
            if (widthError != vr::VROverlayError_None) {
                return failWithOverlayError("SetOverlayWidthInMeters", widthError, error);
            }
            m_overlayWidthMeters = requestedWidth;
        }
    }
    if (stickY != 0.0f && elapsedSeconds > 0.0f) {
        // 상대 좌표의 -Z가 컨트롤러 앞쪽이므로 스틱 위는 멀리, 아래는 가까이 이동한다.
        m_dragControllerToOverlay.m[2][3] = std::clamp(
            m_dragControllerToOverlay.m[2][3] - stickY * kGripDistanceMetersPerSecond * elapsedSeconds,
            -kMaximumGripDistanceMeters,
            -kMinimumGripDistanceMeters);
    }

    vr::HmdMatrix34_t worldTransform = composeTransforms(
        controllerTransform(sample), m_dragControllerToOverlay);
    // 이동 결과의 위치에 정면 방향만 합친 뒤 한 번 제출해 회전 갱신이 이동을 되돌리지 않게 한다.
    orientTowardsUser(&worldTransform);
    return setAbsoluteTransform(worldTransform, "SetOverlayTransformAbsolute", error);
}

void OpenVrOverlay::setPointerCallback(PointerCallback callback) {
    m_pointerCallback = std::move(callback);
}

void OpenVrOverlay::setInteractionStatusCallback(InteractionStatusCallback callback) {
    m_interactionStatusCallback = std::move(callback);
}

void OpenVrOverlay::reportInteractionStatus(const std::string &message) {
    // 같은 런타임 오류를 매 프레임 기록하지 않고 상태가 바뀔 때만 공용 진단 경로로 보낸다.
    if (message != m_lastInteractionStatus) {
        m_lastInteractionStatus = message;
        if (m_interactionStatusCallback) {
            m_interactionStatusCallback(message);
        }
    }
}

bool OpenVrOverlay::computePointerPosition(const keyboard::ControllerPointerSample &sample,
                                          float offsetXPercent,
                                          float offsetYPercent,
                                          int *x,
                                          int *y,
                                          bool *rayHitsVisiblePanel) const {
    if (rayHitsVisiblePanel) {
        *rayHitsVisiblePanel = false;
    }
    if (!m_overlay || !m_hasAbsoluteWorldTransform || !sample.poseActive || !x || !y ||
        m_textureWidth == 0 || m_textureHeight == 0 || !std::isfinite(m_overlayWidthMeters) ||
        m_overlayWidthMeters <= 0.0f) {
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

    const std::array<float, 3> planeOrigin = {
        m_absoluteWorldTransform.m[0][3],
        m_absoluteWorldTransform.m[1][3],
        m_absoluteWorldTransform.m[2][3],
    };
    const std::array<float, 3> planeRight = {
        m_absoluteWorldTransform.m[0][0],
        m_absoluteWorldTransform.m[1][0],
        m_absoluteWorldTransform.m[2][0],
    };
    const std::array<float, 3> planeUp = {
        m_absoluteWorldTransform.m[0][1],
        m_absoluteWorldTransform.m[1][1],
        m_absoluteWorldTransform.m[2][1],
    };
    const std::array<float, 3> planeNormal = {
        m_absoluteWorldTransform.m[0][2],
        m_absoluteWorldTransform.m[1][2],
        m_absoluteWorldTransform.m[2][2],
    };
    const auto dot = [](const std::array<float, 3> &left, const std::array<float, 3> &right) {
        return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
    };

    // 평면 자체를 투영해 보이지 않는 연장 입력 영역에서도 오프셋을 1:1로 적용한다.
    const float directionAlongNormal = dot(sample.direction, planeNormal);
    if (!std::isfinite(directionAlongNormal) || directionAlongNormal >= -0.0001f) {
        return false;
    }
    const std::array<float, 3> originToPlane = {
        planeOrigin[0] - sample.origin[0],
        planeOrigin[1] - sample.origin[1],
        planeOrigin[2] - sample.origin[2],
    };
    const float rayDistance = dot(originToPlane, planeNormal) / directionAlongNormal;
    if (!std::isfinite(rayDistance) || rayDistance < 0.0f) {
        return false;
    }
    const std::array<float, 3> hitOffset = {
        sample.origin[0] + sample.direction[0] * rayDistance - planeOrigin[0],
        sample.origin[1] + sample.direction[1] * rayDistance - planeOrigin[1],
        sample.origin[2] + sample.direction[2] * rayDistance - planeOrigin[2],
    };
    const float overlayHeightMeters = m_overlayWidthMeters * static_cast<float>(m_textureHeight) /
                                      static_cast<float>(m_textureWidth);
    if (!std::isfinite(overlayHeightMeters) || overlayHeightMeters <= 0.0f) {
        return false;
    }
    float rawU = dot(hitOffset, planeRight) / m_overlayWidthMeters + 0.5f;
    float rawV = 0.5f - dot(hitOffset, planeUp) / overlayHeightMeters;
    if (!std::isfinite(rawU) || !std::isfinite(rawV)) {
        return false;
    }
    const bool geometricallyInsidePanel = rawU >= 0.0f && rawU <= 1.0f && rawV >= 0.0f && rawV <= 1.0f;
    if (rayHitsVisiblePanel) {
        *rayHitsVisiblePanel = geometricallyInsidePanel;
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
    // 화면 안쪽은 OpenVR UV를 그대로 사용하고, 화면 바깥에서는 위의 평면 투영값을 쓴다.
    if (geometricallyInsidePanel &&
        m_overlay->ComputeOverlayIntersection(m_handle, &ray, &intersection) &&
        std::isfinite(intersection.vUVs.v[0]) && std::isfinite(intersection.vUVs.v[1]) &&
        intersection.vUVs.v[0] >= 0.0f && intersection.vUVs.v[0] <= 1.0f &&
        intersection.vUVs.v[1] >= 0.0f && intersection.vUVs.v[1] <= 1.0f) {
        rawU = intersection.vUVs.v[0];
        rawV = intersection.vUVs.v[1];
    }

    // 일정한 오프셋을 더하고, UI 범위 밖으로 나간 쪽은 평면의 연장 영역으로 입력받는다.
    const float normalizedX = rawU + normalizedPointerOffset(offsetXPercent);
    const float normalizedY = rawV + normalizedPointerOffset(offsetYPercent);
    if (normalizedX < 0.0f || normalizedX > 1.0f || normalizedY < 0.0f || normalizedY > 1.0f) {
        return false;
    }
    const float pixelX = normalizedX * static_cast<float>(m_textureWidth);
    const float pixelY = normalizedY * static_cast<float>(m_textureHeight);
    *x = static_cast<int>(std::clamp(pixelX, 0.0f, static_cast<float>(m_textureWidth - 1)));
    *y = static_cast<int>(std::clamp(pixelY, 0.0f, static_cast<float>(m_textureHeight - 1)));
    return true;
}

void OpenVrOverlay::dispatchPointerEvent(keyboard::ControllerHand hand,
                                        keyboard::PointerEventType type,
                                        int x,
                                        int y,
                                        keyboard::PointerButton button) {
    if (!m_pointerCallback) {
        return;
    }
    keyboard::PointerEvent pointer;
    pointer.type = type;
    pointer.button = button;
    pointer.source = hand == keyboard::ControllerHand::Left
        ? keyboard::PointerSource::LeftController
        : keyboard::PointerSource::RightController;
    pointer.x = x;
    pointer.y = y;
    m_pointerCallback(pointer);
}

void OpenVrOverlay::resetPointerState(keyboard::ControllerHand hand) {
    const std::size_t index = handIndex(hand);
    if (m_selectWasPressed[index]) {
        dispatchPointerEvent(hand, keyboard::PointerEventType::Cancel, -1, -1);
    } else if (m_pointerInsideOverlay[index]) {
        dispatchPointerEvent(hand, keyboard::PointerEventType::Leave, -1, -1);
    }
    m_selectWasPressed[index] = false;
    m_pointerInsideOverlay[index] = false;
}

void OpenVrOverlay::resetPointerState() {
    resetPointerState(keyboard::ControllerHand::Left);
    resetPointerState(keyboard::ControllerHand::Right);
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
