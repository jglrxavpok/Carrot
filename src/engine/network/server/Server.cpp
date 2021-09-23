//
// Created by jglrxavpok on 28/08/2021.
//

#include "Server.h"
#include <engine/utils/Macros.h>
#include <engine/io/Logging.hpp>
#include <span>
#include <iostream>
#include <engine/network/packets/HandshakePackets.h>
#include <engine/network/AsioHelpers.h>

namespace Carrot::Network {

    Server::Server(std::uint16_t port):
    tcpAcceptor(ioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v6(), port)),
    udpSocket(ioContext, asio::ip::udp::endpoint(asio::ip::udp::v6(), port)) {
        acceptClients();

        networkThread = std::thread([this] {
            threadFunction();
        });
    }

    Server::~Server() {
        ioContext.stop();
        networkThread.join();
    }

    void Server::acceptClients() {
        auto client = std::make_shared<ConnectedClient>(ioContext);

        tcpAcceptor.async_accept(client->tcpSocket, [this, client](const asio::error_code& error) {
            if(!error) {
                client->tcpSocket.non_blocking(false);
                addClient(client);
            }

            acceptClients();
        });
    }

    void Server::disconnect(const ConnectedClient::Ptr& client) {
        // TODO: sendTCP(client, std::make_shared<DisconnectPacket>());
        client->tcpSocket.close();
        clientEndpoints.erase(client->udpEndpoint);
    }

    void Server::addClient(const ConnectedClient::Ptr& client) {
        clients.push_back(client);

        readTCP(client.get(), client->tcpSocket);
    }

    void Server::handleHandshakePacket(ConnectedClient& client, const Packet::Ptr& packet) {
        switch(packet->getPacketID()) {
            case Handshake::PacketIDs::OpenConnectionWin32ID:
                break;

            case Handshake::PacketIDs::SetUDP: {
                auto setUDP = std::reinterpret_pointer_cast<const Handshake::SetUDPPort>(packet);
                client.udpEndpoint = asio::ip::udp::endpoint(client.tcpSocket.remote_endpoint().address(), setUDP->port);
                clientEndpoints[client.udpEndpoint] = client.shared_from_this();
            } break;

            case Handshake::PacketIDs::SetUsername: {
                auto setUsername = std::reinterpret_pointer_cast<const Handshake::SetClientUsername>(packet);
                client.username = setUsername->username;
                // TODO: on client connect
                std::string name = Carrot::toString(client.username);
                Carrot::Log::info("Client '%s' just connected.", name.c_str());

                sendTCP(client, std::make_shared<Handshake::CompleteHandshake>());
                client.currentState = ConnectionState::Play;
            } break;
        }
    }

    void Server::threadFunction() {
        readUDP(udpSocket);
        ioContext.run();
    }

    void Server::broadcastEvent(Packet::Ptr&& event) {
        for(const auto& client : clients) {
            sendTCP(*client, event);
        }
    }

    void Server::broadcastMessage(Packet::Ptr&& event) {
        for(const auto& client : clients) {
            sendUDP(*client, event);
        }
    }

    void Server::sendTCP(Server::ConnectedClient& client, const Packet::Ptr& data) {
        Asio::asyncWriteToSocket(data, client.tcpSocket);
    }

    void Server::sendUDP(Server::ConnectedClient& client, const Packet::Ptr& data) {
        Asio::asyncWriteToSocket(data, udpSocket, client.udpEndpoint);
    }

    void Server::handleHandshakePacket(void *userData, const Packet::Ptr& packet) {
        auto* client = reinterpret_cast<ConnectedClient*>(userData);
        handleHandshakePacket(*client, packet);
    }

    void Server::handleGamePacket(void *userData, const Packet::Ptr& packet) {
        auto* client = reinterpret_cast<ConnectedClient*>(userData);
        if(getPacketConsumer() != nullptr) {
            getPacketConsumer()->consumePacket(client->uuid, packet);
        } else {
            Carrot::Log::warn("No packet consumer!");
        }
    }

    ConnectionState Server::getConnectionState(void *userData) {
        verify(userData, "User data cannot be nullptr here");
        return reinterpret_cast<ConnectedClient*>(userData)->currentState;
    }

    void Server::onDisconnect(void *userData) {
        auto* client = reinterpret_cast<ConnectedClient*>(userData);
        clientEndpoints.erase(client->udpEndpoint);
        clients.erase(std::remove_if(WHOLE_CONTAINER(clients), [&](const auto& ptr) { return ptr.get() == client; }), clients.end());
    }

    void* Server::getUDPUserData(const asio::ip::udp::endpoint& endpoint) {
        return clientEndpoints[endpoint].get();
    }
}
