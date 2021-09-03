//
// Created by jglrxavpok on 28/08/2021.
//

#pragma once

#include <string>
#include <string_view>
#include <engine/network/Packet.hpp>
#include <thread>
#include <queue>
#include <semaphore>
#include <asio.hpp>

namespace Carrot::Network {
    /// Client which can connect to a game server
    class Client {
    public:
        /// Starts a client and its backing thread.
        Client(std::u32string_view username);
        ~Client();

    public:
        /// Attempts to connect to the server at the given address, running on the given port. Will throw if connecting fails
        void connect(std::string_view address, std::uint16_t port);

    public:
        /// Asynchronously sends the event via UDP (can be lost, but fast)
        void queueMessage(Packet::Ptr&& message);

        /// Asynchronously sends the event via TCP (will not be lost, but slow)
        void queueEvent(Packet::Ptr&& event);

    public:
        bool isConnected() const { return connected; }

    private:
        void threadFunction();

    private:
        std::u32string_view username;
        std::thread networkThread;
        bool connected = false;
        asio::io_context ioContext;
        // go through TCP, important messages
        std::queue<Packet::Ptr> events;
        // go through UDP, deemed less important
        std::queue<Packet::Ptr> messages;

        asio::ip::udp::socket udpSocket;
        asio::ip::tcp::socket tcpSocket;

        asio::ip::tcp::endpoint tcpEndpoint;
        asio::ip::udp::endpoint udpEndpoint;

    };
}
