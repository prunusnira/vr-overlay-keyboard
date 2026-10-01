#pragma once

#include "../../core/app_contracts.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class WindowsInputLanguageService final : public keyboard::InputLanguagePort {
public:
    std::vector<keyboard::InputLanguage> loadedLanguages() override;
    keyboard::InputLanguageActivationResult activate(keyboard::KeyboardLanguage language,
                                                     std::string *error) override;

private:
    std::unordered_map<std::string, std::uintptr_t> m_layouts;
};
