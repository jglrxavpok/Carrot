//
// Created by jglrxavpok on 08/09/2021.
//

#pragma once

#include <asio.hpp>
#include <span>
#include <vector>
#include <engine/async/Coroutines.hpp>
#include "Packet.hpp"
#include "ConnectionState.h"

namespace Carrot::Network {
    class NetworkInterface {
    public:
        /// Sets the protocol used for the game
        void setPlayProtocol(Protocol serverBoundProtocol);

    protected:
        void readTCP(void* userData, asio::ip::tcp::socket& socket);
        void readUDP(asio::ip::udp::socket& socket);

        /// Decodes & dispatches a single packet and returns how many bytes have been read from 'packetData'
        std::size_t decodePacket(void* userData, ConnectionState connectionState, const std::span<std::uint8_t>& packetData);

    protected: // methods subclasses have to implement
        virtual void handleHandshakePacket(void* userData, const Packet::Ptr& packet) = 0;
        virtual void handleGamePacket(void* userData, const Packet::Ptr& packet) = 0;
        virtual ConnectionState getConnectionState(void* userData) = 0;
        virtual void onDisconnect(void* userData) = 0;

        /// Returning nullptr will cancel packet decoding
        virtual void* getUDPUserData(const asio::ip::udp::endpoint& endpoint) = 0;

    protected:
        Protocol playProtocol;
        bool hasSetPlayProtocol = false;
    };
}