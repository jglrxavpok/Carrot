//
// Created by jglrxavpok on 28/08/2021.
//

#pragma once

#include <cstdint>
#include <thread>
#include <span>
#include <functional>
#include <asio.hpp>
#include <engine/network/Packet.hpp>
#include <engine/utils/UUID.h>

namespace Carrot::Network {

    /// Dedicated Server to which game clients can connect.
    /// Uses TCP to maintain connection and send "critical" messages.
    /// Uses UDP for quick message sending.
    class Server {

        struct ConnectedClient: public std::enable_shared_from_this<ConnectedClient> {
            enum class State {
                Handshake,
                Play,
            };

            using Ptr = std::shared_ptr<ConnectedClient>;

            explicit ConnectedClient(asio::io_context& context): tcpSocket(context) {}

            Carrot::UUID uuid = Carrot::randomUUID();
            std::u32string username = U"<<<<unknown, handshake not finished>>>>";
            asio::ip::tcp::socket tcpSocket;
            asio::ip::udp::endpoint udpEndpoint;
            State currentState = State::Handshake;
        };

    public:
        struct IPacketConsumer {
            /// Handles an incoming packet.
            virtual void consumePacket(const Carrot::UUID& clientID, const Packet::Ptr& packet) = 0;
        };

    public:
        /// Create a new Server, with a TCP and a UDP channel on the given port.
        explicit Server(std::uint16_t port);
        ~Server();

    public:
        void broadcastEvent(Packet::Ptr&& event);
        void broadcastMessage(Packet::Ptr&& message);

    public:
        void setPacketConsumer(IPacketConsumer* packetConsumer) {
            this->packetConsumer = packetConsumer;
        }

        IPacketConsumer* getPacketConsumer() const { return packetConsumer; }

    public:
        /// Sets the protocol used for the game
        void setPlayProtocol(Protocol serverBoundProtocol);

    private:
        void threadFunction();

        void sendTCP(ConnectedClient& client, const Packet::Ptr& data);
        void sendUDP(ConnectedClient& client, const Packet::Ptr& data);

        void acceptClients();
        void addClient(const ConnectedClient::Ptr& client);
        void readUDP();
        void readTCP(const ConnectedClient::Ptr& client);

        /// Decodes & dispatches a single packet and returns how many bytes have been read from 'packetData'
        std::size_t decodePacket(ConnectedClient& client, const std::span<std::uint8_t>& packetData);
        void handleHandshakePacket(ConnectedClient& client, const Packet::Ptr& packet);

    private:
        asio::io_context ioContext;
        asio::ip::tcp::acceptor tcpAcceptor;
        asio::ip::udp::socket udpSocket;
        std::thread networkThread;
        std::thread acceptorThread;
        std::list<ConnectedClient::Ptr> clients;
        std::unordered_map<asio::ip::udp::endpoint, ConnectedClient::Ptr> clientEndpoints;
        IPacketConsumer* packetConsumer = nullptr;
        Protocol playProtocol;
        bool hasSetPlayProtocol = false;
    };
}
