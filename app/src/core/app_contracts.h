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

enum class PointerSource : std::uint8_t {
    Desktop,
    LeftController,
    RightController,
};

enum class ControllerHand {
    Left,
    Right,
};

// 앱 언어는 Windows 입력 언어와 분리해 키보드 UI와 옵션 문구에만 적용한다.
enum class UiLanguage : std::uint8_t {
    Korean,
    Japanese,
    English,
};

enum class InputLanguageKind : std::uint8_t {
    Other,
    Korean,
    Japanese,
    English,
};

enum class KeyboardLanguage : std::uint8_t {
    Korean,
    Japanese,
    English,
};

enum class InputLanguageActivationResult : std::uint8_t {
    Activated,
    NotInstalled,
    Failed,
};

struct ImeModeSnapshot {
    bool available = false;
    bool native = false;
    bool fullShape = false;
    bool katakana = false;
};

enum class KeyboardLayoutKind : std::uint8_t {
    Qwerty,
    KoreanDubeolsik,
};

// SteamVR 액션에 노출하는 기기 독립 논리 버튼이다. 실제 입력 경로는 사용자가 바인딩한다.
enum class ControllerButton : std::uint8_t {
    LeftGrip,
    LeftTrigger,
    LeftA,
    LeftB,
    LeftMenu,
    LeftJoystick,
    LeftTrackpad,
    RightGrip,
    RightTrigger,
    RightA,
    RightB,
    RightMenu,
    RightJoystick,
    RightTrackpad,
};

inline constexpr std::array<ControllerButton, 14> kControllerButtons = {
    ControllerButton::LeftGrip,
    ControllerButton::LeftTrigger,
    ControllerButton::LeftA,
    ControllerButton::LeftB,
    ControllerButton::LeftMenu,
    ControllerButton::LeftJoystick,
    ControllerButton::LeftTrackpad,
    ControllerButton::RightGrip,
    ControllerButton::RightTrigger,
    ControllerButton::RightA,
    ControllerButton::RightB,
    ControllerButton::RightMenu,
    ControllerButton::RightJoystick,
    ControllerButton::RightTrackpad,
};

struct ControllerButtonState {
    ControllerButton button = ControllerButton::LeftGrip;
    bool active = false;
    bool pressed = false;
};

struct AppSettings {
    UiLanguage uiLanguage = UiLanguage::Korean;
    // 포인터의 화면 크기 대비 보정량이다. 양수 X는 오른쪽, 양수 Y는 아래쪽이다.
    float pointerOffsetXPercent = 0.0f;
    float pointerOffsetYPercent = 2.4f;
    std::vector<ControllerButton> summonButtons = {
        ControllerButton::RightGrip,
        ControllerButton::RightB,
    };
    std::uint32_t summonHoldMilliseconds = 0;
};

constexpr float kMaximumPointerOffsetPercent = 50.0f;

// 저장소와 앱 코어가 같은 규칙으로 사용자 설정을 검증한다.
bool validateAppSettings(const AppSettings &settings, std::string *error = nullptr);

struct ControllerPointerSample {
    // 광선은 Standing 추적 좌표계의 미터 단위 origin과 방향 벡터다.
    ControllerHand hand = ControllerHand::Right;
    bool poseActive = false;
    bool selectPressed = false;
    bool gripPressed = false;
    // 선택 손 조이스틱의 아날로그 축은 활성 상태와 함께 -1.0~1.0 범위로 전달한다.
    bool manipulationStickActive = false;
    float manipulationStickX = 0.0f;
    float manipulationStickY = 0.0f;
    std::array<float, 3> origin{};
    std::array<float, 3> direction{};
    // OpenVR SDK 타입을 UI·코어 경계에 노출하지 않도록 3x4 포즈를 행 우선으로 전달한다.
    std::array<float, 12> deviceToAbsoluteTracking{};
};

struct ControllerPointerSamples {
    std::array<ControllerPointerSample, 2> hands{};
};

struct PointerEvent {
    PointerEventType type = PointerEventType::Move;
    PointerButton button = PointerButton::None;
    PointerSource source = PointerSource::Desktop;
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
    InputLanguageKind kind = InputLanguageKind::Other;
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
    Digit0, Digit1, Digit2, Digit3, Digit4, Digit5, Digit6, Digit7, Digit8, Digit9,
    OemMinus,
    OemEquals,
    OemLeftBracket,
    OemRightBracket,
    OemBackslash,
    OemSemicolon,
    OemApostrophe,
    OemComma,
    OemPeriod,
    OemSlash,
    CapsLock,
    Shift,
    Backspace,
    Space,
    Enter,
    HangulMode,
    JapaneseHiraganaMode,
    JapaneseKanjiMode,
};

struct AppUiState {
    bool overlayVisible = false;
    bool optionsOpen = false;
    std::vector<InputLanguage> inputLanguages;
    ImeModeSnapshot imeMode;
    std::vector<ControllerButtonState> controllerButtons;
    AppSettings settings;
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
    virtual InputLanguageActivationResult activate(KeyboardLanguage language,
                                                   std::string *error) = 0;
};

class ImeModePort {
public:
    virtual ~ImeModePort() = default;
    virtual ImeModeSnapshot currentMode() const = 0;
    virtual bool setMode(KeyboardLanguage language, std::string *error) = 0;
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

class SettingsPort {
public:
    virtual ~SettingsPort() = default;
    virtual bool load(AppSettings *settings, std::string *error) = 0;
    virtual bool save(const AppSettings &settings, std::string *error) = 0;
};

class KeyboardActions {
public:
    // UI와 SteamVR 단축 입력이 같은 사용자 동작을 호출하는 공용 경계다.
    virtual ~KeyboardActions() = default;
    virtual bool toggleOverlay() = 0;
    virtual bool showOverlay() = 0;
    virtual bool hideOverlay() = 0;
    virtual bool setOptionsOpen(bool open) = 0;
    virtual bool applySettings(const AppSettings &settings) = 0;
    virtual bool sendKey(KeyCode key, bool withShift) = 0;
    virtual InputLanguageActivationResult selectKeyboardLanguage(KeyboardLanguage language,
                                                                  std::string *error) = 0;
    virtual bool selectCandidate(std::uint32_t index) = 0;
    virtual bool submitChatboxText(const std::string &utf8Text) = 0;
};

} // namespace keyboard
