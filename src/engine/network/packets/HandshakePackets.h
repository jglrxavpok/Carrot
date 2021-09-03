//
// Created by jglrxavpok on 01/09/2021.
//

#pragma once

#include <engine/utils/Macros.h>
#include <engine/network/Packet.hpp>

namespace Carrot::Network {
    struct Handshake {
        static Protocol ServerBoundPackets;

        enum PacketIDs: PacketID {
            OpenConnectionWin32ID,
            SetUDP,
            SetUsername,
        };

        class OpenConnectionWin32: public Packet {
        public:
            explicit OpenConnectionWin32(): Packet(Handshake::PacketIDs::OpenConnectionWin32ID) {}

        protected:
            void writeAdditional(std::vector<std::uint8_t>& data) const override {}

            void readAdditional(const std::vector<std::uint8_t>& data) override {}
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
                IO::VectorReader r{data};
                r >> port;
            }
        };

        class SetClientUsername: public Packet {
        public:
            std::u32string username = U"";

            explicit SetClientUsername(): Packet(Handshake::PacketIDs::SetUsername) {}
            explicit SetClientUsername(std::u32string_view username): Packet(Handshake::PacketIDs::SetUsername), username(username) {}

        protected:
            void writeAdditional(std::vector<std::uint8_t>& data) const override {
                data << username;
            }

            void readAdditional(const std::vector<std::uint8_t>& data) override {
                IO::VectorReader r{data};
                r >> username;
            }
        };
    };
}
