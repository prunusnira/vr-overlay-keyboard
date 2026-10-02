#pragma once

#include <string>

namespace keyboard {

class KanaComposer final {
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

    Edit press(char roman);
    Edit backspace();
    bool hasActiveComposition() const;
    void commit();

private:
    std::string render() const;

    std::string m_romaji;
    bool m_active = false;
};

} // namespace keyboard
