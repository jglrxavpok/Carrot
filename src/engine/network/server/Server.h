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

namespace Carrot::Network {
    /// Dedicated Server to which game clients can connect.
    /// Uses TCP to maintain connection and send "critical" messages.
    /// Uses UDP for quick message sending.
    class Server {

        struct ConnectedClient {
            enum class State {
                Handshake,
                Play,
            };

            using Ptr = std::shared_ptr<ConnectedClient>;

            explicit ConnectedClient(asio::io_context& context): tcpSocket(context) {}

            std::string username = "<<<<unknown, handshake not finished>>>>";
            asio::ip::tcp::socket tcpSocket;
            asio::ip::udp::endpoint udpEndpoint;
            State currentState = State::Handshake;
        };

    public:
        /// Create a new Server, with a TCP and a UDP channel on the given port.
        explicit Server(std::uint16_t port);
        ~Server();

    public:
        void broadcastEvent(Packet::Ptr&& event);
        void broadcastMessage(Packet::Ptr&& message);

    private:
        void threadFunction();

        void sendTCP(const ConnectedClient& client, const Packet::Ptr& data);
        void sendUDP(const ConnectedClient& client, const Packet::Ptr& data);

        void acceptClients();
        void addClient(const ConnectedClient::Ptr& client);
        void readUDP();
        void readTCP(const ConnectedClient::Ptr& client);
        void decodePacket(ConnectedClient& client, const std::span<std::uint8_t>& packetData);

    private:

        asio::io_context ioContext;
        asio::ip::tcp::acceptor tcpAcceptor;
        asio::ip::udp::socket udpSocket;
        std::thread networkThread;
        std::thread acceptorThread;
        std::list<ConnectedClient::Ptr> clients;
        std::unordered_map<asio::ip::udp::endpoint, ConnectedClient::Ptr> clientEndpoints;
    };
}
