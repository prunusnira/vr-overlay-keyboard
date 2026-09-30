#include "osc_client.h"

#include <QByteArray>
#include <QList>

#include <winsock2.h>

namespace {
void appendOscString(QByteArray &packet, const QByteArray &value) {
    packet.append(value);
    packet.append('\0');
    while (packet.size() % 4 != 0) {
        packet.append('\0');
    }
}
}

bool OscClient::sendChatboxInput(const QString &text, QString *errorMessage) const {
    if (text.contains(QChar::Null)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Chatbox text cannot contain a null character.");
        }
        return false;
    }

    if (text.toUcs4().size() > 144) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("VRChat Chatbox accepts at most 144 characters.");
        }
        return false;
    }

    QByteArray packet;
    appendOscString(packet, QByteArrayLiteral("/chatbox/input"));
    // VRChat arguments: text (string), send (false), notify (false).
    appendOscString(packet, QByteArrayLiteral(",sFF"));
    appendOscString(packet, text.toUtf8());

    WSADATA winsockData{};
    const int startupResult = WSAStartup(MAKEWORD(2, 2), &winsockData);
    if (startupResult != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Winsock initialization failed (%1).")
                                .arg(startupResult);
        }
        return false;
    }

    const SOCKET socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socketHandle == INVALID_SOCKET) {
        const int socketError = WSAGetLastError();
        WSACleanup();
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not create OSC UDP socket (Winsock error %1).")
                                .arg(socketError);
        }
        return false;
    }

    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(9000);
    destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    const int packetSize = static_cast<int>(packet.size());
    const int sent = sendto(socketHandle,
                            packet.constData(),
                            packetSize,
                            0,
                            reinterpret_cast<const sockaddr *>(&destination),
                            sizeof(destination));
    const int sendError = sent == SOCKET_ERROR ? WSAGetLastError() : 0;
    closesocket(socketHandle);
    WSACleanup();

    if (sent == SOCKET_ERROR) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not send OSC packet (Winsock error %1).")
                                .arg(sendError);
        }
        return false;
    }
    if (sent != packetSize) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("OSC UDP packet was sent incompletely (%1 of %2 bytes).")
                                .arg(sent)
                                .arg(packetSize);
        }
        return false;
    }

    return true;
}
