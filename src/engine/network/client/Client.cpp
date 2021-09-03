//
// Created by jglrxavpok on 28/08/2021.
//

#include "Client.h"
#include <engine/utils/Macros.h>
#include <coroutine>
#include <engine/utils/stringmanip.h>
#include <engine/io/Logging.hpp>
#include <engine/network/packets/HandshakePackets.h>

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

        //asio::connect(udpSocket, udpEndpoint);
        connected = true;
    }

    void Client::queueEvent(Packet::Ptr&& event) {
        runtimeAssert(connected, "Client must be connected!");
        auto bytes = std::make_shared<std::vector<std::uint8_t>>();
        event->toBuffer().write(*bytes);
        asio::async_write(tcpSocket, asio::buffer(*bytes), [bytes /* keep data alive for write */](const asio::error_code& error, std::size_t bytesTransferred) {});
    }

    void Client::queueMessage(Packet::Ptr&& message) {
        runtimeAssert(connected, "Client must be connected!");
        auto bytes = std::make_shared<std::vector<std::uint8_t>>();
        message->toBuffer().write(*bytes);
        udpSocket.async_send_to(asio::buffer(*bytes), udpEndpoint, [bytes /* keep data alive for write */](const asio::error_code& error, std::size_t bytesTransferred) {
            Carrot::Log::info("queueMessage, write done");
        });
    }
}