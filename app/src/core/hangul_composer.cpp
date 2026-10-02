#include "hangul_composer.h"

#include <array>
#include <cstdint>
#include <utility>

namespace keyboard {
namespace {
constexpr std::uint32_t kSyllableBase = 0xAC00;
constexpr int kVowelCount = 21;
constexpr int kTrailingCount = 28;

constexpr std::array<std::uint32_t, 19> kLeadingJamo = {
    0x3131, 0x3132, 0x3134, 0x3137, 0x3138, 0x3139, 0x3141, 0x3142, 0x3143,
    0x3145, 0x3146, 0x3147, 0x3148, 0x3149, 0x314A, 0x314B, 0x314C, 0x314D, 0x314E,
};

constexpr std::array<std::uint32_t, 21> kVowelJamo = {
    0x314F, 0x3150, 0x3151, 0x3152, 0x3153, 0x3154, 0x3155, 0x3156, 0x3157, 0x3158, 0x3159,
    0x315A, 0x315B, 0x315C, 0x315D, 0x315E, 0x315F, 0x3160, 0x3161, 0x3162, 0x3163,
};

std::uint32_t firstCodePoint(std::string_view text) {
    if (text.empty()) {
        return 0;
    }
    const auto lead = static_cast<unsigned char>(text[0]);
    if (lead < 0x80) {
        return lead;
    }
    if ((lead & 0xE0) == 0xC0 && text.size() >= 2) {
        return ((lead & 0x1F) << 6) |
            (static_cast<unsigned char>(text[1]) & 0x3F);
    }
    if ((lead & 0xF0) == 0xE0 && text.size() >= 3) {
        return ((lead & 0x0F) << 12) |
            ((static_cast<unsigned char>(text[1]) & 0x3F) << 6) |
            (static_cast<unsigned char>(text[2]) & 0x3F);
    }
    if ((lead & 0xF8) == 0xF0 && text.size() >= 4) {
        return ((lead & 0x07) << 18) |
            ((static_cast<unsigned char>(text[1]) & 0x3F) << 12) |
            ((static_cast<unsigned char>(text[2]) & 0x3F) << 6) |
            (static_cast<unsigned char>(text[3]) & 0x3F);
    }
    return 0;
}

std::string toUtf8(std::uint32_t codePoint) {
    std::string result;
    if (codePoint <= 0x7F) {
        result.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FF) {
        result.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint <= 0xFFFF) {
        result.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        result.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        result.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        result.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
    return result;
}

int leadingIndex(std::uint32_t codePoint) {
    for (std::size_t index = 0; index < kLeadingJamo.size(); ++index) {
        if (kLeadingJamo[index] == codePoint) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

int vowelIndex(std::uint32_t codePoint) {
    for (std::size_t index = 0; index < kVowelJamo.size(); ++index) {
        if (kVowelJamo[index] == codePoint) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

int trailingForLeading(int leading) {
    constexpr int kMap[] = {1, 2, 4, 7, 0, 8, 16, 17, 0, 19, 20, 21, 22, 0, 23, 24, 25, 26, 27};
    return leading >= 0 && leading < 19 ? kMap[leading] : 0;
}

int combineVowels(int first, int second) {
    if (first == 8 && second == 0) return 9;   // ㅗ + ㅏ = ㅘ
    if (first == 8 && second == 1) return 10;  // ㅗ + ㅐ = ㅙ
    if (first == 8 && second == 20) return 11; // ㅗ + ㅣ = ㅚ
    if (first == 13 && second == 4) return 14; // ㅜ + ㅓ = ㅝ
    if (first == 13 && second == 5) return 15; // ㅜ + ㅔ = ㅞ
    if (first == 13 && second == 20) return 16;// ㅜ + ㅣ = ㅟ
    if (first == 18 && second == 20) return 19;// ㅡ + ㅣ = ㅢ
    return -1;
}

int reduceVowel(int vowel) {
    switch (vowel) {
    case 9: case 10: case 11: return 8;
    case 14: case 15: case 16: return 13;
    case 19: return 18;
    default: return -1;
    }
}

int combineTrailing(int first, int second) {
    if (first == 1 && second == 1) return 2;   // ㄱ + ㄱ = ㄲ
    if (first == 19 && second == 19) return 20;// ㅅ + ㅅ = ㅆ
    if (first == 1 && second == 19) return 3;  // ㄱ + ㅅ = ㄳ
    if (first == 4 && second == 22) return 5;  // ㄴ + ㅈ = ㄵ
    if (first == 4 && second == 27) return 6;  // ㄴ + ㅎ = ㄶ
    if (first == 8 && second == 1) return 9;   // ㄹ + ㄱ = ㄺ
    if (first == 8 && second == 16) return 10; // ㄹ + ㅁ = ㄻ
    if (first == 8 && second == 17) return 11; // ㄹ + ㅂ = ㄼ
    if (first == 8 && second == 19) return 12; // ㄹ + ㅅ = ㄽ
    if (first == 8 && second == 25) return 13; // ㄹ + ㅌ = ㄾ
    if (first == 8 && second == 26) return 14; // ㄹ + ㅍ = ㄿ
    if (first == 8 && second == 27) return 15; // ㄹ + ㅎ = ㅀ
    if (first == 17 && second == 19) return 18;// ㅂ + ㅅ = ㅄ
    return 0;
}

struct SplitTrailing {
    int remaining = 0;
    int movedLeading = -1;
};

SplitTrailing splitTrailing(int trailing) {
    switch (trailing) {
    case 2: return {1, 0};  // ㄲ -> ㄱ + ㄱ
    case 3: return {1, 9};  // ㄳ -> ㄱ + ㅅ
    case 5: return {4, 12}; // ㄵ -> ㄴ + ㅈ
    case 6: return {4, 18}; // ㄶ -> ㄴ + ㅎ
    case 9: return {8, 0};  // ㄺ -> ㄹ + ㄱ
    case 10: return {8, 6}; // ㄻ -> ㄹ + ㅁ
    case 11: return {8, 7}; // ㄼ -> ㄹ + ㅂ
    case 12: return {8, 9}; // ㄽ -> ㄹ + ㅅ
    case 13: return {8, 16};// ㄾ -> ㄹ + ㅌ
    case 14: return {8, 17};// ㄿ -> ㄹ + ㅍ
    case 15: return {8, 18};// ㅀ -> ㄹ + ㅎ
    case 18: return {17, 9};// ㅄ -> ㅂ + ㅅ
    case 20: return {19, 9};// ㅆ -> ㅅ + ㅅ
    case 1: return {0, 0};
    case 4: return {0, 2};
    case 7: return {0, 3};
    case 8: return {0, 5};
    case 16: return {0, 6};
    case 17: return {0, 7};
    case 19: return {0, 9};
    case 21: return {0, 11};
    case 22: return {0, 12};
    case 23: return {0, 14};
    case 24: return {0, 15};
    case 25: return {0, 16};
    case 26: return {0, 17};
    case 27: return {0, 18};
    default: return {};
    }
}

int reduceLeading(int leading) {
    switch (leading) {
    case 1: return 0;  // ㄲ -> ㄱ
    case 4: return 3;  // ㄸ -> ㄷ
    case 8: return 7;  // ㅃ -> ㅂ
    case 13: return 12;// ㅉ -> ㅈ
    default: return -1;
    }
}
}

HangulComposer::Edit HangulComposer::press(std::string_view jamo) {
    const std::uint32_t codePoint = firstCodePoint(jamo);
    const int leading = leadingIndex(codePoint);
    const int vowel = vowelIndex(codePoint);
    if (leading < 0 && vowel < 0) {
        commit();
        return {EditAction::Insert, std::string(jamo), {}};
    }

    if (vowel >= 0) {
        if (m_leading >= 0) {
            if (m_vowel < 0) {
                m_vowel = vowel;
                return updateActive();
            }
            if (m_trailing != 0) {
                const SplitTrailing split = splitTrailing(m_trailing);
                if (split.movedLeading >= 0) {
                    const std::string committed = toUtf8(kSyllableBase +
                        ((m_leading * kVowelCount + m_vowel) * kTrailingCount + split.remaining));
                    m_leading = split.movedLeading;
                    m_vowel = vowel;
                    m_trailing = 0;
                    const std::string active = render();
                    return updateActive(committed + active);
                }
            } else {
                const int combined = combineVowels(m_vowel, vowel);
                if (combined >= 0) {
                    m_vowel = combined;
                    return updateActive();
                }
            }
            commit();
            return startVowel(vowel);
        }

        if (m_vowel >= 0) {
            const int combined = combineVowels(m_vowel, vowel);
            if (combined >= 0) {
                m_vowel = combined;
                return updateActive();
            }
            commit();
        }
        return startVowel(vowel);
    }

    if (m_leading < 0) {
        commit();
        return startLeading(leading);
    }
    if (m_vowel < 0) {
        commit();
        return startLeading(leading);
    }
    const int trailing = trailingForLeading(leading);
    if (m_trailing == 0 && trailing != 0) {
        m_trailing = trailing;
        return updateActive();
    }
    if (m_trailing != 0 && trailing != 0) {
        const int combined = combineTrailing(m_trailing, trailing);
        if (combined != 0) {
            m_trailing = combined;
            return updateActive();
        }
    }
    commit();
    return startLeading(leading);
}

HangulComposer::Edit HangulComposer::backspace() {
    if (!m_active) {
        return {};
    }
    if (m_leading >= 0 && m_vowel >= 0 && m_trailing != 0) {
        const SplitTrailing split = splitTrailing(m_trailing);
        m_trailing = split.remaining;
        return updateActive();
    }
    if (m_leading >= 0 && m_vowel >= 0) {
        const int reduced = reduceVowel(m_vowel);
        if (reduced >= 0) {
            m_vowel = reduced;
        } else {
            m_vowel = -1;
        }
        return updateActive();
    }
    if (m_leading >= 0) {
        const int reduced = reduceLeading(m_leading);
        if (reduced >= 0) {
            m_leading = reduced;
            return updateActive();
        }
    } else if (m_vowel >= 0) {
        const int reduced = reduceVowel(m_vowel);
        if (reduced >= 0) {
            m_vowel = reduced;
            return updateActive();
        }
    }
    commit();
    return {EditAction::ReplaceActive, {}, {}};
}

bool HangulComposer::hasActiveComposition() const {
    return m_active;
}

void HangulComposer::commit() {
    m_leading = -1;
    m_vowel = -1;
    m_trailing = 0;
    m_active = false;
}

std::string HangulComposer::render() const {
    if (m_leading >= 0 && m_vowel >= 0) {
        const std::uint32_t syllable = kSyllableBase +
            ((m_leading * kVowelCount + m_vowel) * kTrailingCount + m_trailing);
        return toUtf8(syllable);
    }
    if (m_leading >= 0) {
        return toUtf8(kLeadingJamo[static_cast<std::size_t>(m_leading)]);
    }
    if (m_vowel >= 0) {
        return toUtf8(kVowelJamo[static_cast<std::size_t>(m_vowel)]);
    }
    return {};
}

HangulComposer::Edit HangulComposer::updateActive(std::string replacement) {
    m_active = true;
    const std::string activeText = render();
    if (replacement.empty()) {
        replacement = activeText;
    }
    return {EditAction::ReplaceActive, std::move(replacement), activeText};
}

HangulComposer::Edit HangulComposer::startLeading(int leading) {
    m_leading = leading;
    m_vowel = -1;
    m_trailing = 0;
    m_active = true;
    const std::string text = render();
    return {EditAction::Insert, text, text};
}

HangulComposer::Edit HangulComposer::startVowel(int vowel) {
    m_leading = -1;
    m_vowel = vowel;
    m_trailing = 0;
    m_active = true;
    const std::string text = render();
    return {EditAction::Insert, text, text};
}

} // namespace keyboard
