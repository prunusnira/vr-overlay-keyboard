#pragma once

#include <QString>

class OscClient final {
public:
    bool sendChatboxInput(const QString &text, QString *errorMessage = nullptr) const;
};
