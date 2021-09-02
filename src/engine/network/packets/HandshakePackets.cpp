//
// Created by jglrxavpok on 01/09/2021.
//

#include "HandshakePackets.h"

namespace Carrot::Network {
    Protocol Handshake::Packets = std::move(
            Carrot::Network::Protocol()
                .with<0, Handshake::SetUDPPort>()
    );
}
