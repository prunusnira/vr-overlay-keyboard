#include "keyboard_layout.h"

namespace keyboard {
namespace {
KeyboardKeyDefinition character(KeyCode code,
                                const char *qwerty,
                                const char *shifted = nullptr,
                                const char *korean = nullptr,
                                const char *shiftedKorean = nullptr,
                                float widthUnits = 1.0f) {
    return {code, KeyboardKeyKind::Character, qwerty, shifted, korean, shiftedKorean, widthUnits};
}

KeyboardKeyDefinition control(KeyCode code, KeyboardKeyKind kind, float widthUnits) {
    return {code, kind, nullptr, nullptr, nullptr, nullptr, widthUnits};
}

const std::vector<KeyboardRow> kKeyboardRows = {
    {
        character(KeyCode::Digit1, "1", "!"),
        character(KeyCode::Digit2, "2", "@"),
        character(KeyCode::Digit3, "3", "#"),
        character(KeyCode::Digit4, "4", "$"),
        character(KeyCode::Digit5, "5", "%"),
        character(KeyCode::Digit6, "6", "^"),
        character(KeyCode::Digit7, "7", "&"),
        character(KeyCode::Digit8, "8", "*"),
        character(KeyCode::Digit9, "9", "("),
        character(KeyCode::Digit0, "0", ")"),
        character(KeyCode::OemMinus, "-", "_"),
        character(KeyCode::OemEquals, "=", "+"),
        control(KeyCode::Backspace, KeyboardKeyKind::Backspace, 1.8f),
    },
    {
        character(KeyCode::Q, "Q", nullptr, u8"ㅂ", u8"ㅃ"),
        character(KeyCode::W, "W", nullptr, u8"ㅈ", u8"ㅉ"),
        character(KeyCode::E, "E", nullptr, u8"ㄷ", u8"ㄸ"),
        character(KeyCode::R, "R", nullptr, u8"ㄱ", u8"ㄲ"),
        character(KeyCode::T, "T", nullptr, u8"ㅅ", u8"ㅆ"),
        character(KeyCode::Y, "Y", nullptr, u8"ㅛ"),
        character(KeyCode::U, "U", nullptr, u8"ㅕ"),
        character(KeyCode::I, "I", nullptr, u8"ㅑ"),
        character(KeyCode::O, "O", nullptr, u8"ㅐ", u8"ㅒ"),
        character(KeyCode::P, "P", nullptr, u8"ㅔ", u8"ㅖ"),
        character(KeyCode::OemLeftBracket, "[", "{"),
        character(KeyCode::OemRightBracket, "]", "}"),
        character(KeyCode::OemBackslash, "\\", "|"),
    },
    {
        control(KeyCode::CapsLock, KeyboardKeyKind::CapsLock, 1.7f),
        character(KeyCode::A, "A", nullptr, u8"ㅁ"),
        character(KeyCode::S, "S", nullptr, u8"ㄴ"),
        character(KeyCode::D, "D", nullptr, u8"ㅇ"),
        character(KeyCode::F, "F", nullptr, u8"ㄹ"),
        character(KeyCode::G, "G", nullptr, u8"ㅎ"),
        character(KeyCode::H, "H", nullptr, u8"ㅗ"),
        character(KeyCode::J, "J", nullptr, u8"ㅓ"),
        character(KeyCode::K, "K", nullptr, u8"ㅏ"),
        character(KeyCode::L, "L", nullptr, u8"ㅣ"),
        character(KeyCode::OemSemicolon, ";", ":"),
        character(KeyCode::OemApostrophe, "'", "\""),
        control(KeyCode::Enter, KeyboardKeyKind::Enter, 1.7f),
    },
    {
        control(KeyCode::Shift, KeyboardKeyKind::Shift, 1.8f),
        character(KeyCode::Z, "Z", nullptr, u8"ㅋ"),
        character(KeyCode::X, "X", nullptr, u8"ㅌ"),
        character(KeyCode::C, "C", nullptr, u8"ㅊ"),
        character(KeyCode::V, "V", nullptr, u8"ㅍ"),
        character(KeyCode::B, "B", nullptr, u8"ㅠ"),
        character(KeyCode::N, "N", nullptr, u8"ㅜ"),
        character(KeyCode::M, "M", nullptr, u8"ㅡ"),
        character(KeyCode::OemComma, ",", "<"),
        character(KeyCode::OemPeriod, ".", ">"),
        character(KeyCode::OemSlash, "/", "?"),
    },
    {
        control(KeyCode::HangulMode, KeyboardKeyKind::KoreanMode, 2.2f),
        control(KeyCode::Space, KeyboardKeyKind::Space, 6.0f),
        control(KeyCode::JapaneseHiraganaMode, KeyboardKeyKind::HiraganaMode, 1.8f),
    },
};
}

const std::vector<KeyboardRow> &keyboardRows() {
    return kKeyboardRows;
}

KeyboardLayoutKind keyboardLayoutFor(InputLanguageKind language, const ImeModeSnapshot &mode) {
    if (language == InputLanguageKind::Korean && mode.available && mode.native) {
        return KeyboardLayoutKind::KoreanDubeolsik;
    }
    return KeyboardLayoutKind::Qwerty;
}

const char *primaryKeyLabel(const KeyboardKeyDefinition &key, KeyboardLayoutKind layout) {
    if (layout == KeyboardLayoutKind::KoreanDubeolsik && key.koreanLabel) {
        return key.koreanLabel;
    }
    return key.qwertyLabel ? key.qwertyLabel : "";
}

const char *secondaryKeyLabel(const KeyboardKeyDefinition &key, KeyboardLayoutKind layout) {
    if (layout == KeyboardLayoutKind::KoreanDubeolsik && key.shiftedKoreanLabel) {
        return key.shiftedKoreanLabel;
    }
    return key.shiftedLabel ? key.shiftedLabel : "";
}

} // namespace keyboard
