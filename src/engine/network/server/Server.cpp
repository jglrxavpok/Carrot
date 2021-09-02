//
// Created by jglrxavpok on 28/08/2021.
//

#include "Server.h"
#include <engine/utils/Macros.h>
#include <span>
#include <iostream>
#include <engine/network/packets/HandshakePackets.h>

namespace Carrot::Network {

    Server::Server(std::uint16_t port):
    tcpAcceptor(ioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v6(), port)),
    udpSocket(ioContext, asio::ip::udp::endpoint(asio::ip::udp::v6(), port)) {
        networkThread = std::thread([this] {
            threadFunction();
        });

        acceptClients();
    }

    Server::~Server() {
        ioContext.stop();
        networkThread.join();
    }

    void Server::acceptClients() {
        auto client = std::make_shared<ConnectedClient>(ioContext);

        tcpAcceptor.async_accept(client->tcpSocket, [this, client](const asio::error_code& error) {
            if(!error) {
                asio::ip::udp::endpoint udpEndpoint = {client->tcpSocket.remote_endpoint().address(), client->tcpSocket.remote_endpoint().port()};
                client->udpEndpoint = udpEndpoint;
                addClient(client);
            }

            acceptClients();
        });
    }

    void Server::readTCP(const ConnectedClient::Ptr& client) {
        client->tcpSocket.async_wait(asio::ip::tcp::socket::wait_type::wait_read, [this, client](const asio::error_code& error) {
            auto& tcpSocket = client->tcpSocket;
            std::cout << "hi tcp" << std::endl;
            if(!error) {
                std::vector<std::uint8_t> readBuffer(tcpSocket.available());

                asio::ip::tcp::endpoint remoteEndpoint;
                asio::error_code readError;
                std::size_t readSize = tcpSocket.receive(asio::buffer(readBuffer), 0, readError);

                if(client && !readError) {
                    decodePacket(*client, std::span{readBuffer.data(), readSize});
                }
            }

            readTCP(client);
        });
    }

    void Server::addClient(const ConnectedClient::Ptr& client) {
        clientEndpoints[client->udpEndpoint] = client;
        clients.push_back(client);

        readTCP(client);
    }

    void Server::decodePacket(ConnectedClient& client, const std::span<std::uint8_t>& packetData) {
        auto buffer = PacketBuffer(packetData);

        Packet::Ptr packet = nullptr;

        switch(client.currentState) {
            case ConnectedClient::State::Handshake:
                packet = Handshake::Packets.make(buffer.packetType);
                break;

            case ConnectedClient::State::Play:
                TODO
                break;

            default: TODO
        }

        assert(packet);
        packet->readAdditional(buffer.data);

        // TODO: dispatch

        TODO
    }

    void Server::readUDP() {
        udpSocket.async_wait(asio::ip::udp::socket::wait_type::wait_read, [this](const asio::error_code& error) {
            std::cout << "hi udp" << std::endl;
            if(!error) {
                std::vector<std::uint8_t> readBuffer(udpSocket.available());

                asio::ip::udp::endpoint remoteEndpoint;
                asio::error_code readError;

                std::cout << "Available: " << readBuffer.size() << std::endl;
                std::size_t readSize = udpSocket.receive_from(asio::buffer(readBuffer), remoteEndpoint, 0, readError);
                std::cout << "readSize: " << readSize << std::endl;

                std::cout << "Remote endpoint: " << remoteEndpoint.address().to_string() << ":" << remoteEndpoint.port() << std::endl;

                auto client = clientEndpoints[remoteEndpoint];
                if(client == nullptr) {
                    std::cerr << "No client!" << std::endl;
                }
                if(readError) {
                    std::cout << "Read error: " << readError << std::endl;
                }
                if(client && !readError) {
                    decodePacket(*client, std::span{readBuffer.data(), readSize});
                }
            } else {
                std::cout << "Error: " << error << std::endl;
            }

            readUDP();
        });
    }

    void Server::threadFunction() {
        readUDP();
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

    void Server::sendTCP(const Server::ConnectedClient& client, const Packet::Ptr& data) {
        TODO
    }

    void Server::sendUDP(const Server::ConnectedClient& client, const Packet::Ptr& data) {
        TODO
    }
}