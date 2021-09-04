//
// Created by jglrxavpok on 28/08/2021.
//

#include "Client.h"
#include <engine/utils/Macros.h>
#include <coroutine>
#include <engine/utils/stringmanip.h>
#include <engine/io/Logging.hpp>
#include <engine/network/packets/HandshakePackets.h>
#include <engine/network/AsioHelpers.h>

namespace Carrot::Network {
    Client::Client(std::u32string_view username): username(username), tcpSocket(ioContext), udpSocket(ioContext) {
        networkThread = std::thread([this]() {
           threadFunction();
        });
    }

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
        udpEndpoint = *udpResolver.resolve(address, portStr).begin();

        // https://stackoverflow.com/a/18434964
        // A dummy TX is required for the socket to acquire the local port properly under windoze
        // Transmitting an empty string works fine for this, but the TX must take place BEFORE the first call to Asynch_receive_from(...)
#ifdef WIN32
        {
            Handshake::OpenConnectionWin32 open;
            std::vector<std::uint8_t> bytes;
            open.toBuffer().write(bytes);
            udpSocket.send_to(asio::buffer(bytes), udpEndpoint);
        }
#endif

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
        // TODO

        //asio::connect(udpSocket, udpEndpoint);
        connected = true;
    }

    void Client::waitForHandshakeCompletion() {
        asio::error_code error;
        tcpSocket.wait(asio::ip::tcp::socket::wait_read, error);
        if(!error) {
            std::vector<std::uint8_t> readBuffer(tcpSocket.available());
            asio::error_code readError;
            tcpSocket.receive(asio::buffer(readBuffer), 0, readError);
            if(!readError) {
                PacketBuffer buffer{readBuffer};
                runtimeAssert(buffer.packetType == Handshake::PacketIDs::ConfirmHandshake,
                              "Expected 'ConfirmHandshake' packet, got packet with ID " + std::to_string(buffer.packetType));
            } else {
                throw std::runtime_error("Could not complete handshake, error " + error.message());
            }
        } else {
            throw std::runtime_error("Could not complete handshake, error " + error.message());
        }
    }

    void Client::queueEvent(Packet::Ptr&& event) {
        runtimeAssert(connected, "Client must be connected!");
        Asio::asyncWriteToSocket(event, tcpSocket);
    }

    void Client::queueMessage(Packet::Ptr&& message) {
        runtimeAssert(connected, "Client must be connected!");
        Asio::asyncWriteToSocket(message, udpSocket, udpEndpoint);
    }
}