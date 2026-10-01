#include "osc_client.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdint>

namespace {
constexpr unsigned short kVrchatOscPort = 9000;
constexpr std::size_t kMaximumChatboxCodePoints = 144;

void appendOscString(std::string &packet, const std::string &value) {
    packet.append(value);
    packet.push_back('\0');
    while (packet.size() % 4 != 0) {
        packet.push_back('\0');
    }
}

bool isContinuation(unsigned char value) {
    return (value & 0xC0) == 0x80;
}

bool countUtf8CodePoints(const std::string &text, std::size_t *count) {
    std::size_t index = 0;
    std::size_t points = 0;
    while (index < text.size()) {
        const unsigned char first = static_cast<unsigned char>(text[index]);
        if (first <= 0x7F) {
            ++index;
        } else if (first >= 0xC2 && first <= 0xDF) {
            if (index + 1 >= text.size() || !isContinuation(static_cast<unsigned char>(text[index + 1]))) {
                return false;
            }
            index += 2;
        } else if (first >= 0xE0 && first <= 0xEF) {
            if (index + 2 >= text.size()) {
                return false;
            }
            const unsigned char second = static_cast<unsigned char>(text[index + 1]);
            const unsigned char third = static_cast<unsigned char>(text[index + 2]);
            if (!isContinuation(third) ||
                (first == 0xE0 ? (second < 0xA0 || second > 0xBF)
                                : first == 0xED ? (second < 0x80 || second > 0x9F)
                                                : !isContinuation(second))) {
                return false;
            }
            index += 3;
        } else if (first >= 0xF0 && first <= 0xF4) {
            if (index + 3 >= text.size()) {
                return false;
            }
            const unsigned char second = static_cast<unsigned char>(text[index + 1]);
            if (!isContinuation(static_cast<unsigned char>(text[index + 2])) ||
                !isContinuation(static_cast<unsigned char>(text[index + 3])) ||
                (first == 0xF0 ? (second < 0x90 || second > 0xBF)
                                : first == 0xF4 ? (second < 0x80 || second > 0x8F)
                                                : !isContinuation(second))) {
                return false;
            }
            index += 4;
        } else {
            return false;
        }
        ++points;
    }
    if (count) {
        *count = points;
    }
    return true;
}
}

bool OscClient::sendChatboxText(const std::string &utf8Text, std::string *error) const {
    if (utf8Text.find('\0') != std::string::npos) {
        if (error) {
            *error = "Chatbox text cannot contain a null character.";
        }
        return false;
    }

    std::size_t codePoints = 0;
    if (!countUtf8CodePoints(utf8Text, &codePoints)) {
        if (error) {
            *error = "Chatbox text is not valid UTF-8.";
        }
        return false;
    }
    if (codePoints > kMaximumChatboxCodePoints) {
        if (error) {
            *error = "VRChat Chatbox accepts at most 144 Unicode code points.";
        }
        return false;
    }

    std::string packet;
    appendOscString(packet, "/chatbox/input");
    appendOscString(packet, ",sFF");
    appendOscString(packet, utf8Text);

    WSADATA data{};
    const int startupResult = WSAStartup(MAKEWORD(2, 2), &data);
    if (startupResult != 0) {
        if (error) {
            *error = "Winsock initialization failed (" + std::to_string(startupResult) + ").";
        }
        return false;
    }

    const SOCKET socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socketHandle == INVALID_SOCKET) {
        const int socketError = WSAGetLastError();
        WSACleanup();
        if (error) {
            *error = "Could not create the OSC UDP socket (Winsock error " + std::to_string(socketError) + ").";
        }
        return false;
    }

    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(kVrchatOscPort);
    destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const int packetSize = static_cast<int>(packet.size());
    const int sent = sendto(socketHandle,
                            packet.data(),
                            packetSize,
                            0,
                            reinterpret_cast<const sockaddr *>(&destination),
                            sizeof(destination));
    const int sendError = sent == SOCKET_ERROR ? WSAGetLastError() : 0;
    closesocket(socketHandle);
    WSACleanup();

    if (sent == SOCKET_ERROR) {
        if (error) {
            *error = "Could not send the OSC packet (Winsock error " + std::to_string(sendError) + ").";
        }
        return false;
    }
    if (sent != packetSize) {
        if (error) {
            *error = "OSC UDP packet was sent incompletely (" + std::to_string(sent) + " of " +
                     std::to_string(packetSize) + " bytes).";
        }
        return false;
    }
    return true;
}
