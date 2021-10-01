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
#include <engine/network/NetworkInterface.h>
#include <engine/utils/UUID.h>

namespace Carrot::Network {

    /// Dedicated Server to which game clients can connect.
    /// Uses TCP to maintain connection and send "critical" messages.
    /// Uses UDP for quick message sending.
    class Server: public NetworkInterface {

        struct ConnectedClient: public std::enable_shared_from_this<ConnectedClient> {

            using Ptr = std::shared_ptr<ConnectedClient>;

            explicit ConnectedClient(asio::io_context& context): tcpSocket(context) {}

            Carrot::UUID uuid;
            std::u32string username = U"<<<<unknown, handshake not finished>>>>";
            asio::ip::tcp::socket tcpSocket;
            asio::ip::udp::endpoint udpEndpoint;
            ConnectionState currentState = ConnectionState::Handshake;
        };

    public:
        struct IPacketConsumer {
            /// Handles an incoming packet.
            virtual void consumePacket(const Carrot::UUID& clientID, const Packet::Ptr packet) = 0;
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
        const std::list<ConnectedClient::Ptr>& getConnectedClients() const {
            return clients;
        }

    private:
        void threadFunction();

        void sendTCP(ConnectedClient& client, const Packet::Ptr& data);
        void sendUDP(ConnectedClient& client, const Packet::Ptr& data);

        void acceptClients();
        void addClient(const ConnectedClient::Ptr& client);
        void disconnect(const ConnectedClient::Ptr& client);

        void handleHandshakePacket(ConnectedClient& client, const Packet::Ptr& packet);

    protected:
        void handleHandshakePacket(void *userData, const Packet::Ptr& packet) override;
        void handleGamePacket(void *userData, const Packet::Ptr& packet) override;

        ConnectionState getConnectionState(void *userData) override;

        void onDisconnect(void* userData) override;

        void* getUDPUserData(const asio::ip::udp::endpoint& endpoint) override;

    private:
        asio::io_context ioContext;
        asio::ip::tcp::acceptor tcpAcceptor;
        asio::ip::udp::socket udpSocket;
        std::thread networkThread;
        std::thread acceptorThread;
        std::list<ConnectedClient::Ptr> clients;
        std::unordered_map<asio::ip::udp::endpoint, ConnectedClient::Ptr> clientEndpoints;
        IPacketConsumer* packetConsumer = nullptr;
    };
}
