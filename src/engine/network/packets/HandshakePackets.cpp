//
// Created by jglrxavpok on 01/09/2021.
//

#include "HandshakePackets.h"

namespace Carrot::Network {
    Protocol Handshake::ServerBoundPackets = std::move(
            Carrot::Network::Protocol()
                .with<Handshake::PacketIDs::SetUDP, Handshake::SetUDPPort>()
                .with<Handshake::PacketIDs::OpenConnectionWin32ID, Handshake::OpenConnectionWin32>()
                .with<Handshake::PacketIDs::SetUsername, Handshake::SetClientUsername>()
    );

    Protocol Handshake::ClientBoundPackets = std::move(
            Carrot::Network::Protocol()
                    .with<Handshake::PacketIDs::ConfirmHandshake, Handshake::CompleteHandshake>()
    );
}
