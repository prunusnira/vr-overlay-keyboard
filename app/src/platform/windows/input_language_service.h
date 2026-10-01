#pragma once

#include "../../core/app_contracts.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class WindowsInputLanguageService final : public keyboard::InputLanguagePort {
public:
    std::vector<keyboard::InputLanguage> loadedLanguages() override;
    bool activate(const std::string &languageId, std::string *error) override;

private:
    std::unordered_map<std::string, std::uintptr_t> m_layouts;
};
