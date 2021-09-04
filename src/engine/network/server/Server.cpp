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
        udpSocket.non_blocking(false);
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
                client->tcpSocket.non_blocking(false);
                addClient(client);
            }

            acceptClients();
        });
    }

    void Server::readTCP(const ConnectedClient::Ptr& client) {
        client->tcpSocket.async_wait(asio::ip::tcp::socket::wait_type::wait_read, [this, client](const asio::error_code& error) {
            auto& tcpSocket = client->tcpSocket;
            if(!error) {
                std::vector<std::uint8_t> readBuffer(tcpSocket.available());

                asio::ip::tcp::endpoint remoteEndpoint;
                asio::error_code readError;
                std::size_t readSize = tcpSocket.receive(asio::buffer(readBuffer), 0, readError);

                if(client && !readError) {
                    std::size_t processedSize = 0;
                    do {
                        std::size_t processedBytes = decodePacket(*client, std::span{readBuffer.data() + processedSize, readSize-processedSize});
                        processedSize += processedBytes;
                    } while(processedSize < readSize);
                } else {
                    Carrot::Log::error("Read error: " + error.message());
                }
            } else {
                Carrot::Log::error("Wait error: " + error.message());
            }

            readTCP(client);
        });
    }

    void Server::addClient(const ConnectedClient::Ptr& client) {
        clients.push_back(client);

        readTCP(client);
    }

    std::size_t Server::decodePacket(ConnectedClient& client, const std::span<std::uint8_t>& packetData) {
        auto buffer = PacketBuffer(packetData);

        Packet::Ptr packet = nullptr;

        switch(client.currentState) {
            case ConnectedClient::State::Handshake:
                packet = Handshake::ServerBoundPackets.make(buffer.packetType);
                break;

            case ConnectedClient::State::Play:
                runtimeAssert(hasSetPlayProtocol, "No protocol has been set with setPlayProtocol!");
                packet = playProtocol.make(buffer.packetType);
                break;

            default: TODO
        }

        assert(packet);
        packet->readAdditional(buffer.data);

        // handshake is handled by engine; rest is up to the game systems
        if(client.currentState == ConnectedClient::State::Handshake) {
            handleHandshakePacket(client, packet);
        } else if(getPacketConsumer() != nullptr) {
            getPacketConsumer()->consumePacket(client.uuid, packet);
        } else {
            Carrot::Log::warn("No packet consumer!");
        }
        return buffer.sizeOf();
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
                client.currentState = ConnectedClient::State::Play;
            } break;
        }
    }

    void Server::readUDP() {
        udpSocket.async_wait(asio::ip::udp::socket::wait_type::wait_read, [this](const asio::error_code& error) {
            if(!error) {
                std::vector<std::uint8_t> readBuffer(udpSocket.available());

                asio::ip::udp::endpoint remoteEndpoint;
                asio::error_code readError;

                std::size_t readSize = udpSocket.receive_from(asio::buffer(readBuffer), remoteEndpoint, 0, readError);

                std::cout << "UDP readSize: " << readSize << ", available: " << readBuffer.size() << std::endl;
                auto client = clientEndpoints[remoteEndpoint];
                if(client == nullptr) {
                    Carrot::Log::error("No client!");
                }
                if(readError) {
                    Carrot::Log::error("Read error: " + readError.message());
                }
                if(client && !readError) {
                    try {
                        std::size_t processedSize = 0;
                        do {
                            std::size_t processedBytes = decodePacket(*client, std::span{readBuffer.data() + processedSize, readSize-processedSize});
                            processedSize += processedBytes;
                        } while(processedSize < readSize);
                    } catch(std::exception& e) {
                        auto clientUUIDStr = Carrot::toString(client->uuid);
                        auto clientUsernameStr = Carrot::toString(client->username);
                        Carrot::Log::error("Network error from client %s (%s): %s", clientUUIDStr.c_str(), clientUsernameStr.c_str(), e.what());
                    }
                }
            } else {
                Carrot::Log::error("Wait error: " + error.message());
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

    void Server::setPlayProtocol(Protocol serverBoundProtocol) {
        playProtocol = serverBoundProtocol;
        hasSetPlayProtocol = true;
    }

    void Server::sendTCP(Server::ConnectedClient& client, const Packet::Ptr& data) {
        Asio::asyncWriteToSocket(data, client.tcpSocket);
    }

    void Server::sendUDP(Server::ConnectedClient& client, const Packet::Ptr& data) {
        Asio::asyncWriteToSocket(data, udpSocket, client.udpEndpoint);
    }
}
