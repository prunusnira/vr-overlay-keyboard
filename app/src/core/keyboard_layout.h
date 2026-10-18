#pragma once

#include "app_contracts.h"

#include <optional>
#include <vector>

namespace keyboard {

enum class KeyboardKeyKind : std::uint8_t {
    Character,
    Backspace,
    CapsLock,
    Enter,
    Shift,
    RightShift,
    KoreanMode,
    Space,
    HiraganaMode,
};

struct KeyboardKeyDefinition {
    KeyCode code = KeyCode::A;
    KeyboardKeyKind kind = KeyboardKeyKind::Character;
    const char *qwertyLabel = nullptr;
    const char *shiftedLabel = nullptr;
    const char *koreanLabel = nullptr;
    const char *shiftedKoreanLabel = nullptr;
    float widthUnits = 1.0f;
};

using KeyboardRow = std::vector<KeyboardKeyDefinition>;

struct KeyboardRowDefinition {
    KeyboardRow keys;
    // The first character key's horizontal offset from the Q key, in key-width units.
    // Rows without this value are centered around the Q row.
    std::optional<float> firstCharacterOffsetUnits;
};

const std::vector<KeyboardRowDefinition> &keyboardRows();
KeyboardLayoutKind keyboardLayoutFor(InputLanguageKind language, const ImeModeSnapshot &mode);
const char *primaryKeyLabel(const KeyboardKeyDefinition &key, KeyboardLayoutKind layout);
const char *secondaryKeyLabel(const KeyboardKeyDefinition &key, KeyboardLayoutKind layout);

} // namespace keyboard
