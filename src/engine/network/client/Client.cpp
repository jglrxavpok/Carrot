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
    Client::Client(std::string_view username): username(username), tcpSocket(ioContext), udpSocket(ioContext) {
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

        Handshake::SetUDPPort setUDPPort { udpEndpoint.port() };
        std::vector<std::uint8_t> bytes;
        setUDPPort.toBuffer().write(bytes);
        asio::write(tcpSocket, asio::buffer(bytes));

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