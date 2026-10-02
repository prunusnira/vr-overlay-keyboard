#include "kana_composer.h"

#include <string_view>

namespace keyboard {
namespace {
struct KanaMapping {
    std::string_view roman;
    std::string_view kana;
};

constexpr KanaMapping kMappings[] = {
    {"a", u8"あ"}, {"i", u8"い"}, {"u", u8"う"}, {"e", u8"え"}, {"o", u8"お"},
    {"ka", u8"か"}, {"ki", u8"き"}, {"ku", u8"く"}, {"ke", u8"け"}, {"ko", u8"こ"},
    {"ga", u8"が"}, {"gi", u8"ぎ"}, {"gu", u8"ぐ"}, {"ge", u8"げ"}, {"go", u8"ご"},
    {"sa", u8"さ"}, {"shi", u8"し"}, {"si", u8"し"}, {"su", u8"す"}, {"se", u8"せ"}, {"so", u8"そ"},
    {"za", u8"ざ"}, {"ji", u8"じ"}, {"zi", u8"じ"}, {"zu", u8"ず"}, {"ze", u8"ぜ"}, {"zo", u8"ぞ"},
    {"ta", u8"た"}, {"chi", u8"ち"}, {"ti", u8"ち"}, {"tsu", u8"つ"}, {"tu", u8"つ"}, {"te", u8"て"}, {"to", u8"と"},
    {"da", u8"だ"}, {"di", u8"ぢ"}, {"du", u8"づ"}, {"de", u8"で"}, {"do", u8"ど"},
    {"na", u8"な"}, {"ni", u8"に"}, {"nu", u8"ぬ"}, {"ne", u8"ね"}, {"no", u8"の"},
    {"ha", u8"は"}, {"hi", u8"ひ"}, {"fu", u8"ふ"}, {"hu", u8"ふ"}, {"he", u8"へ"}, {"ho", u8"ほ"},
    {"ba", u8"ば"}, {"bi", u8"び"}, {"bu", u8"ぶ"}, {"be", u8"べ"}, {"bo", u8"ぼ"},
    {"pa", u8"ぱ"}, {"pi", u8"ぴ"}, {"pu", u8"ぷ"}, {"pe", u8"ぺ"}, {"po", u8"ぽ"},
    {"ma", u8"ま"}, {"mi", u8"み"}, {"mu", u8"む"}, {"me", u8"め"}, {"mo", u8"も"},
    {"ya", u8"や"}, {"yu", u8"ゆ"}, {"yo", u8"よ"},
    {"ra", u8"ら"}, {"ri", u8"り"}, {"ru", u8"る"}, {"re", u8"れ"}, {"ro", u8"ろ"},
    {"wa", u8"わ"}, {"wo", u8"を"}, {"wi", u8"うぃ"}, {"we", u8"うぇ"},
    {"kya", u8"きゃ"}, {"kyu", u8"きゅ"}, {"kyo", u8"きょ"},
    {"gya", u8"ぎゃ"}, {"gyu", u8"ぎゅ"}, {"gyo", u8"ぎょ"},
    {"sha", u8"しゃ"}, {"shu", u8"しゅ"}, {"sho", u8"しょ"},
    {"sya", u8"しゃ"}, {"syu", u8"しゅ"}, {"syo", u8"しょ"},
    {"ja", u8"じゃ"}, {"ju", u8"じゅ"}, {"jo", u8"じょ"},
    {"jya", u8"じゃ"}, {"jyu", u8"じゅ"}, {"jyo", u8"じょ"},
    {"zya", u8"じゃ"}, {"zyu", u8"じゅ"}, {"zyo", u8"じょ"},
    {"cha", u8"ちゃ"}, {"chu", u8"ちゅ"}, {"cho", u8"ちょ"},
    {"tya", u8"ちゃ"}, {"tyu", u8"ちゅ"}, {"tyo", u8"ちょ"},
    {"cya", u8"ちゃ"}, {"cyu", u8"ちゅ"}, {"cyo", u8"ちょ"},
    {"dya", u8"ぢゃ"}, {"dyu", u8"ぢゅ"}, {"dyo", u8"ぢょ"},
    {"nya", u8"にゃ"}, {"nyu", u8"にゅ"}, {"nyo", u8"にょ"},
    {"hya", u8"ひゃ"}, {"hyu", u8"ひゅ"}, {"hyo", u8"ひょ"},
    {"bya", u8"びゃ"}, {"byu", u8"びゅ"}, {"byo", u8"びょ"},
    {"pya", u8"ぴゃ"}, {"pyu", u8"ぴゅ"}, {"pyo", u8"ぴょ"},
    {"mya", u8"みゃ"}, {"myu", u8"みゅ"}, {"myo", u8"みょ"},
    {"rya", u8"りゃ"}, {"ryu", u8"りゅ"}, {"ryo", u8"りょ"},
    {"fa", u8"ふぁ"}, {"fi", u8"ふぃ"}, {"fe", u8"ふぇ"}, {"fo", u8"ふぉ"},
    {"va", u8"ゔぁ"}, {"vi", u8"ゔぃ"}, {"vu", u8"ゔ"}, {"ve", u8"ゔぇ"}, {"vo", u8"ゔぉ"},
    {"kwa", u8"くぁ"}, {"kwi", u8"くぃ"}, {"kwe", u8"くぇ"}, {"kwo", u8"くぉ"},
    {"gwa", u8"ぐぁ"}, {"gwi", u8"ぐぃ"}, {"gwe", u8"ぐぇ"}, {"gwo", u8"ぐぉ"},
    {"tsa", u8"つぁ"}, {"tsi", u8"つぃ"}, {"tse", u8"つぇ"}, {"tso", u8"つぉ"},
    {"xa", u8"ぁ"}, {"xi", u8"ぃ"}, {"xu", u8"ぅ"}, {"xe", u8"ぇ"}, {"xo", u8"ぉ"},
    {"xya", u8"ゃ"}, {"xyu", u8"ゅ"}, {"xyo", u8"ょ"}, {"xwa", u8"ゎ"}, {"xtsu", u8"っ"},
    {"la", u8"ぁ"}, {"li", u8"ぃ"}, {"lu", u8"ぅ"}, {"le", u8"ぇ"}, {"lo", u8"ぉ"},
    {"lya", u8"ゃ"}, {"lyu", u8"ゅ"}, {"lyo", u8"ょ"}, {"lwa", u8"ゎ"}, {"ltu", u8"っ"}, {"ltsu", u8"っ"},
};

bool isVowel(char value) {
    return value == 'a' || value == 'i' || value == 'u' || value == 'e' || value == 'o';
}

bool isConsonant(char value) {
    return value >= 'a' && value <= 'z' && !isVowel(value);
}

std::string renderRomaji(std::string_view romaji) {
    std::string output;
    for (std::size_t offset = 0; offset < romaji.size();) {
        const char current = romaji[offset];
        const std::size_t remaining = romaji.size() - offset;

        if (current == 'n') {
            if (remaining == 1) {
                output.push_back('n');
                break;
            }
            if (romaji[offset + 1] == '\'') {
                output += u8"ん";
                offset += 2;
                continue;
            }
            if (romaji[offset + 1] == 'n') {
                if (remaining == 2) {
                    output += u8"ん";
                    break;
                }
                const char afterDoubleN = romaji[offset + 2];
                output += u8"ん";
                if (isVowel(afterDoubleN) || afterDoubleN == 'y') {
                    ++offset;
                } else {
                    offset += 2;
                }
                continue;
            }
            if (!isVowel(romaji[offset + 1]) && romaji[offset + 1] != 'y') {
                output += u8"ん";
                ++offset;
                continue;
            }
        }

        if (remaining >= 2 && current == romaji[offset + 1] && isConsonant(current) && current != 'n') {
            output += u8"っ";
            ++offset;
            continue;
        }

        std::size_t bestLength = 0;
        std::string_view bestKana;
        bool hasPendingPrefix = false;
        const std::string_view suffix = romaji.substr(offset);
        for (const KanaMapping &mapping : kMappings) {
            if (suffix.size() >= mapping.roman.size() &&
                suffix.substr(0, mapping.roman.size()) == mapping.roman &&
                mapping.roman.size() > bestLength) {
                bestLength = mapping.roman.size();
                bestKana = mapping.kana;
            }
            if (suffix.size() < mapping.roman.size() && mapping.roman.substr(0, suffix.size()) == suffix) {
                hasPendingPrefix = true;
            }
        }
        if (bestLength > 0) {
            output.append(bestKana);
            offset += bestLength;
        } else if (hasPendingPrefix) {
            output.append(suffix);
            break;
        } else {
            output.push_back(current);
            ++offset;
        }
    }
    return output;
}
}

KanaComposer::Edit KanaComposer::press(char roman) {
    if (roman < 'a' || roman > 'z') {
        commit();
        return {EditAction::Insert, std::string(1, roman), {}};
    }
    const bool wasActive = m_active;
    m_romaji.push_back(roman);
    const std::string rendered = render();
    m_active = true;
    return {wasActive ? EditAction::ReplaceActive : EditAction::Insert, rendered, rendered};
}

KanaComposer::Edit KanaComposer::backspace() {
    if (!m_active) {
        return {};
    }
    if (!m_romaji.empty()) {
        m_romaji.pop_back();
    }
    if (m_romaji.empty()) {
        commit();
        return {EditAction::ReplaceActive, {}, {}};
    }
    const std::string rendered = render();
    return {EditAction::ReplaceActive, rendered, rendered};
}

bool KanaComposer::hasActiveComposition() const {
    return m_active;
}

void KanaComposer::commit() {
    m_romaji.clear();
    m_active = false;
}

std::string KanaComposer::render() const {
    return renderRomaji(m_romaji);
}

} // namespace keyboard
