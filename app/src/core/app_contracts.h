#pragma once

#include <cstdint>
#include <array>
#include <functional>
#include <string>
#include <vector>

namespace keyboard {

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
    bool active = false;
    std::vector<std::string> candidates;
    std::uint32_t selectedIndex = 0;
    std::uint32_t currentPage = 0;
    std::vector<std::uint32_t> pageStarts;
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
    std::string status = "Starting VR overlay keyboard.";
};

class OverlayControlPort {
public:
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
    virtual ~KeyboardActions() = default;
    virtual bool toggleOverlay() = 0;
    virtual bool sendKey(KeyCode key, bool withShift) = 0;
    virtual bool activateInputLanguage(const std::string &languageId) = 0;
    virtual bool selectCandidate(std::uint32_t index) = 0;
    virtual bool submitChatboxText(const std::string &utf8Text) = 0;
};

} // namespace keyboard
