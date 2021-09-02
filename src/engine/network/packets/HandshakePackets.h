//
// Created by jglrxavpok on 01/09/2021.
//

#pragma once

#include <engine/utils/Macros.h>
#include <engine/network/Packet.hpp>

namespace Carrot::Network {
    struct Handshake {
        static Protocol Packets;

        enum PacketIDs: PacketID {
            SetUDP,
        };

        class SetUDPPort: public Packet {
        public:
            std::uint16_t port = -1;

            explicit SetUDPPort(): Packet(Handshake::PacketIDs::SetUDP) {}
            explicit SetUDPPort(std::uint16_t port): Packet(Handshake::PacketIDs::SetUDP), port(port) {}

        protected:
            void writeAdditional(std::vector<std::uint8_t>& data) const override {
                data << port;
            }

            void readAdditional(const std::vector<std::uint8_t>& data) override {
                TODO
            }
        };
    };
}
