#pragma once

#include "../core/app_contracts.h"

#include <string>

class OscClient final : public keyboard::ChatboxPort {
public:
    bool sendChatboxText(const std::string &utf8Text, std::string *error) const override;
};
