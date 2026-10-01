#pragma once

#include "app_contracts.h"

#include <vector>

namespace keyboard {

enum class KeyboardKeyKind : std::uint8_t {
    Character,
    Backspace,
    CapsLock,
    Enter,
    Shift,
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

const std::vector<KeyboardRow> &keyboardRows();
KeyboardLayoutKind keyboardLayoutFor(InputLanguageKind language, const ImeModeSnapshot &mode);
const char *primaryKeyLabel(const KeyboardKeyDefinition &key, KeyboardLayoutKind layout);
const char *secondaryKeyLabel(const KeyboardKeyDefinition &key, KeyboardLayoutKind layout);

} // namespace keyboard
