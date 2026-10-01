#pragma once

#include "core/app_contracts.h"

class WindowsSettingsStore final : public keyboard::SettingsPort {
public:
    bool load(keyboard::AppSettings *settings, std::string *error) override;
    bool save(const keyboard::AppSettings &settings, std::string *error) override;
};
