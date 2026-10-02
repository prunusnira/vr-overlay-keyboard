#pragma once

#include <string>
#include <string_view>

namespace keyboard {

class HangulComposer final {
public:
    enum class EditAction {
        None,
        Insert,
        ReplaceActive,
    };

    struct Edit {
        EditAction action = EditAction::None;
        std::string text;
        std::string activeText;
    };

    Edit press(std::string_view jamo);
    Edit backspace();
    bool hasActiveComposition() const;
    void commit();

private:
    std::string render() const;
    Edit updateActive(std::string replacement = {});
    Edit startLeading(int leading);
    Edit startVowel(int vowel);

    int m_leading = -1;
    int m_vowel = -1;
    int m_trailing = 0;
    bool m_active = false;
};

} // namespace keyboard
