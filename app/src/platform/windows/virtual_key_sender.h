#pragma once

#include "../../core/app_contracts.h"

#include <cstdint>
#include <string>

class WindowsVirtualKeySender final : public keyboard::VirtualKeyPort {
public:
    bool send(keyboard::KeyCode key, bool withShift, std::string *error) const override;
    bool isCurrentProcessForeground() const;
    bool requestForeground(std::uintptr_t nativeWindowHandle, std::string *error) const;
};
