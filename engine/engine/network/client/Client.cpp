//
// Created by jglrxavpok on 28/08/2021.
//

#include "Client.h"
#include <engine/utils/Macros.h>
#include <coroutine>
#include <core/utils/stringmanip.h>
#include <core/io/Logging.hpp>
#include <engine/network/packets/HandshakePackets.h>
#include <engine/network/AsioHelpers.h>
#include <core/async/OSThreads.h>

namespace Carrot::Network {
    Client::Client(std::u32string_view username): username(username), tcpSocket(ioContext), udpSocket(ioContext) {}

    Client::~Client() {
        ioContext.stop();
        networkThread.join();
    }

    void Client::threadFunction() {
        ioContext.run();
    }

    void Client::connect(std::string_view address, std::uint16_t port) {
        asio::ip::tcp::resolver tcpResolver(ioContext);

        std::string portStr = Carrot::sprintf("%u", port);
        auto tcpEndpoints = tcpResolver.resolve(address, portStr);
        tcpEndpoint = *tcpEndpoints.begin();

        asio::connect(tcpSocket, tcpEndpoints);
        udpSocket.open(asio::ip::udp::v6());

        asio::ip::udp::resolver udpResolver(ioContext);
        auto udpEndpoints = udpResolver.resolve(address, portStr);
        udpEndpoint = *udpEndpoints.begin();

        connected = true;
     //asio::connect(udpSocket, udpEndpoints);
        //udpSocket.bind(asio::ip::udp::endpoint(asio::ip::udp::v6(), udpEndpoint.port()));

        // https://stackoverflow.com/a/18434964
        // A dummy TX is required for the socket to acquire the local port properly under windoze
        // Transmitting an empty string works fine for this, but the TX must take place BEFORE the first call to Asynch_receive_from(...)
#ifdef WIN32
        {
            queueMessage(std::make_shared<Handshake::OpenConnectionWin32>());
        }
#endif

       // udpSocket.bind(udpEndpoint);

        Handshake::SetUDPPort setUDPPort { udpSocket.local_endpoint().port() };
        std::vector<std::uint8_t> udpBytes;
        setUDPPort.toBuffer().write(udpBytes);

        Handshake::SetClientUsername usernamePacket { username };
        std::vector<std::uint8_t> usernameBytes;
        usernamePacket.toBuffer().write(usernameBytes);

        asio::error_code ec;
        asio::write(tcpSocket, asio::buffer(udpBytes), ec);
        if(ec) {
            TODO
        }
        asio::write(tcpSocket, asio::buffer(usernameBytes), ec);
        if(ec) {
            TODO
        }

        waitForHandshakeCompletion();

        // TODO: debug, remove
        std::string usernameStr = Carrot::toString(username);
        Carrot::Log::info("Client %s is connected.", usernameStr.c_str());

        // setup listeners
        readTCP(this, tcpSocket);
        readUDP(udpSocket);

        networkThread = std::thread([this]() {
            threadFunction();
        });
        Carrot::Threads::setName(networkThread, "Client Network thread");
    }

    void Client::onDisconnect() {
        connected = false;
        tcpSocket.close();
        ioContext.stop();

        // TODO: debug, remove
        std::string usernameStr = Carrot::toString(username);
        Carrot::Log::info("Client %s is disconnected.", usernameStr.c_str());
    }

    void Client::disconnect() {
        tcpSocket.close();
        ioContext.stop();
    }

    void Client::waitForHandshakeCompletion() {
        asio::error_code error;
        tcpSocket.wait(asio::ip::tcp::socket::wait_read, error);
        if(!error) {
            std::vector<std::uint8_t> readBuffer(tcpSocket.available());
            asio::error_code readError;
            std::size_t readSize = tcpSocket.receive(asio::buffer(readBuffer), 0, readError);
            if(!readError) {
                PacketBuffer buffer{{readBuffer.data(), readSize}};
                verify(buffer.packetType == Handshake::PacketIDs::ConfirmHandshake,
                              "Expected 'ConfirmHandshake' packet, got packet with ID " + std::to_string(buffer.packetType));
            } else {
                throw std::runtime_error("Could not complete handshake, error " + error.message());
            }
        } else {
            throw std::runtime_error("Could not complete handshake, error " + error.message());
        }
    }

    void Client::queueEvent(Packet::Ptr&& event) {
        verify(connected, "Client must be connected!");
        Asio::asyncWriteToSocket(event, tcpSocket);
    }

    void Client::queueMessage(Packet::Ptr&& message) {
        verify(connected, "Client must be connected!");
        Asio::asyncWriteToSocket(message, udpSocket, udpEndpoint);
    }

    void Client::handleHandshakePacket(void *userData, const Packet::Ptr& packet) {
        throw std::runtime_error("Should never happen, handshake is handled inside 'connect'");
    }

    void Client::handleGamePacket(void *userData, const Packet::Ptr& packet) {
        if(getPacketConsumer() != nullptr) {
            getPacketConsumer()->consumePacket(packet);
        } else {
            Carrot::Log::warn("No packet consumer!");
        }
    }

    ConnectionState Client::getConnectionState(void *userData) {
        return connected ? ConnectionState::Play : ConnectionState::Handshake;
    }

    void Client::onDisconnect(void *userData) {
        onDisconnect();
    }

    void* Client::getUDPUserData(const asio::ip::udp::endpoint& endpoint) {
        return this; // don't return nullptr, otherwise this skips packet decoding (server requires this behaviour to determine whether a UDP packet corresponds to a known client)
    }
}