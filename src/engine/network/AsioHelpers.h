//
// Created by jglrxavpok on 04/09/2021.
//

#pragma once

#include "Packet.hpp"
#include <asio.hpp>

namespace Carrot::Asio {
    inline void asyncWriteToSocket(const Carrot::Network::Packet::Ptr& packet, asio::ip::tcp::socket& socket) {
        auto bytes = std::make_shared<std::vector<std::uint8_t>>();
        packet->toBuffer().write(*bytes);
        asio::async_write(socket, asio::buffer(*bytes), [&socket, bytes /* keep data alive for write */](const asio::error_code& error, std::size_t bytesTransferred) {
            if(error) {
                const auto& endpoint = socket.remote_endpoint();
                std::string endpointStr = endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
                Carrot::Log::error("Error while writing to TCP socket at endpoint %s: %s", endpointStr.c_str(), error.message().c_str());
            }
        });
    }

    inline void asyncWriteToSocket(const Carrot::Network::Packet::Ptr& packet, asio::ip::udp::socket& socket, const asio::ip::udp::endpoint& endpoint) {
        auto bytes = std::make_shared<std::vector<std::uint8_t>>();
        packet->toBuffer().write(*bytes);
        socket.async_send_to(asio::buffer(*bytes), endpoint, [&endpoint, bytes /* keep data alive for write */](const asio::error_code& error, std::size_t bytesTransferred) {
            if(error) {
                std::string endpointStr = endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
                Carrot::Log::error("Error while writing to UDP endpoint %s: %s", endpointStr.c_str(), error.message().c_str());
            }
        });
    }
}