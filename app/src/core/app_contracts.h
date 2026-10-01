#pragma once

#include <cstdint>
#include <array>
#include <functional>
#include <string>
#include <vector>

namespace keyboard {

// UI와 Windows·SteamVR·OpenVR 어댑터가 공유한다. 플랫폼 SDK 타입은 이 계약에 넣지 않는다.
enum class PointerEventType {
    Move,
    Press,
    Release,
    Leave,
    Cancel,
};

enum class PointerButton {
    None,
    Left,
    Right,
};

enum class ControllerHand {
    Left,
    Right,
};

struct ControllerPointerSample {
    // 광선은 Standing 추적 좌표계의 미터 단위 origin과 방향 벡터다.
    ControllerHand hand = ControllerHand::Right;
    bool poseActive = false;
    bool selectPressed = false;
    std::array<float, 3> origin{};
    std::array<float, 3> direction{};
};

struct ControllerPointerSamples {
    std::array<ControllerPointerSample, 2> hands{};
};

struct PointerEvent {
    PointerEventType type = PointerEventType::Move;
    PointerButton button = PointerButton::None;
    int x = 0;
    int y = 0;
};

struct ImageFrame {
    // 픽셀은 위쪽 행부터 저장한 RGBA8이며 OpenVR 어댑터가 이 버퍼를 그대로 전달한다.
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgbaPixels;
};

struct InputLanguage {
    std::string id;
    std::string label;
    bool active = false;
};

struct CandidateSnapshot {
    // 후보 문자열은 TSF 어댑터가 UTF-8로 변환해 전달한다.
    bool active = false;
    std::vector<std::string> candidates;
    std::uint32_t selectedIndex = 0;
    std::uint32_t currentPage = 0;
    std::vector<std::uint32_t> pageStarts;
};

struct CompositionSnapshot {
    // 조합 중인 문자열은 확정된 편집 문자열과 분리한다. 화면에 표시해도 아직 전송할 텍스트는 아니다.
    bool active = false;
    std::string preedit;
};

enum class KeyCode : std::uint8_t {
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Backspace,
    Space,
    Enter,
    HangulMode,
    JapaneseHiraganaMode,
    JapaneseKanjiMode,
};

struct AppUiState {
    bool overlayVisible = false;
    std::vector<InputLanguage> inputLanguages;
    CandidateSnapshot candidates;
    CompositionSnapshot composition;
    std::string status = "Starting VR overlay keyboard.";
};

class OverlayControlPort {
public:
    // 앱 코어는 IVROverlay 핸들 대신 이 작은 제어 포트만 사용한다.
    virtual ~OverlayControlPort() = default;
    virtual bool show(std::string *error) = 0;
    virtual bool hide(std::string *error) = 0;
    virtual bool isVisible() const = 0;
};

class InputLanguagePort {
public:
    virtual ~InputLanguagePort() = default;
    virtual std::vector<InputLanguage> loadedLanguages() = 0;
    virtual bool activate(const std::string &languageId, std::string *error) = 0;
};

class VirtualKeyPort {
public:
    virtual ~VirtualKeyPort() = default;
    virtual bool send(KeyCode key, bool withShift, std::string *error) const = 0;
};

class CandidateSelectionPort {
public:
    virtual ~CandidateSelectionPort() = default;
    virtual bool selectCandidate(std::uint32_t index, std::string *error) = 0;
};

class ChatboxPort {
public:
    virtual ~ChatboxPort() = default;
    virtual bool sendChatboxText(const std::string &utf8Text, std::string *error) const = 0;
};

class KeyboardActions {
public:
    // UI와 SteamVR 단축 입력이 같은 사용자 동작을 호출하는 공용 경계다.
    virtual ~KeyboardActions() = default;
    virtual bool toggleOverlay() = 0;
    virtual bool sendKey(KeyCode key, bool withShift) = 0;
    virtual bool activateInputLanguage(const std::string &languageId) = 0;
    virtual bool selectCandidate(std::uint32_t index) = 0;
    virtual bool submitChatboxText(const std::string &utf8Text) = 0;
};

} // namespace keyboard
