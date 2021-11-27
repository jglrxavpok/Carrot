//
// Created by jglrxavpok on 08/09/2021.
//

#include "NetworkInterface.h"
#include "packets/HandshakePackets.h"
#include "core/Macros.h"
#include "core/io/Logging.hpp"

namespace Carrot::Network {

    std::size_t NetworkInterface::decodePacket(void* userData, ConnectionState connectionState, const std::span<std::uint8_t>& packetData) {
        auto buffer = PacketBuffer(packetData);

        Packet::Ptr packet = nullptr;

        switch(connectionState) {
            case ConnectionState::Handshake:
                packet = Handshake::ServerBoundPackets.make(buffer.packetType);
                break;

            case ConnectionState::Play:
                verify(hasSetPlayProtocol, "No protocol has been set with setPlayProtocol!");
                packet = playProtocol.make(buffer.packetType);
                break;

            default: TODO
        }

        assert(packet);
        packet->readAdditional(buffer.data);

        // handshake is handled by engine; rest is up to the game systems
        if(connectionState == ConnectionState::Handshake) {
            handleHandshakePacket(userData, packet);
        } else {
            handleGamePacket(userData, packet);
        }
        return buffer.sizeOf();
    }

    void NetworkInterface::setPlayProtocol(Protocol serverBoundProtocol) {
        playProtocol = serverBoundProtocol;
        hasSetPlayProtocol = true;
    }

    void NetworkInterface::readTCP(void *userData, asio::ip::tcp::socket& socket) {
        socket.async_wait(asio::ip::tcp::socket::wait_type::wait_read, [this, &socket, userData](const asio::error_code& error) {
            if(!error) {
                if(socket.available() == 0) { // end of connection
                    onDisconnect(userData);
                    return; // stop reading from this socket
                }
                std::vector<std::uint8_t> readBuffer(socket.available());

                asio::error_code readError;
                std::size_t readSize = socket.receive(asio::buffer(readBuffer), 0, readError);

                if(userData && !readError) {
                    std::size_t processedSize = 0;
                    do {
                        std::size_t processedBytes = decodePacket(userData, getConnectionState(userData), std::span{readBuffer.data() + processedSize, readSize-processedSize});
                        processedSize += processedBytes;
                    } while(processedSize < readSize);
                } else {
                    Carrot::Log::error("Read error: " + error.message());
                }
            } else {
                Carrot::Log::error("Wait error: " + error.message());
            }

            readTCP(userData, socket);
        });
    }

    void NetworkInterface::readUDP(asio::ip::udp::socket& udpSocket) {
        udpSocket.async_wait(asio::ip::udp::socket::wait_type::wait_read, [this, &udpSocket](const asio::error_code& error) {
            if(!error) {
                std::vector<std::uint8_t> readBuffer(udpSocket.available());

                asio::ip::udp::endpoint remoteEndpoint;
                asio::error_code readError;

                std::size_t readSize = udpSocket.receive_from(asio::buffer(readBuffer), remoteEndpoint, 0, readError);

                // std::cout << "UDP readSize: " << readSize << ", available: " << readBuffer.size() << std::endl;
                auto userData = getUDPUserData(remoteEndpoint);
                if(userData == nullptr) {
                    Carrot::Log::error("No client!");
                }
                if(readError) {
                    Carrot::Log::error("Read error: " + readError.message());
                }
                if(userData && !readError) {
                    try {
                        std::size_t processedSize = 0;
                        do {
                            std::size_t processedBytes = decodePacket(userData, getConnectionState(userData), std::span{readBuffer.data() + processedSize, readSize-processedSize});
                            processedSize += processedBytes;
                        } while(processedSize < readSize);
                    } catch(std::exception& e) {
                        std::string remoteEndpointStr = remoteEndpoint.address().to_string() + ":" + std::to_string(remoteEndpoint.port());
                        Carrot::Log::error("Network error from UDP endpoint %s: %s", remoteEndpointStr.c_str(), e.what());
                    }
                }
            } else {
                Carrot::Log::error("Wait error: " + error.message());
            }

            readUDP(udpSocket);
        });
    }
}