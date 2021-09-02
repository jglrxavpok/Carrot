//
// Created by jglrxavpok on 25/08/2021.
//
#include <ctime>
#include <iostream>
#include <string>
#include <asio.hpp>
#include <engine/network/client/Client.h>
#include <engine/network/server/Server.h>

struct TestPacket: public Carrot::Network::Packet {
    float someVal = 42.0f;

public:
    explicit TestPacket(): Carrot::Network::Packet(42) {}

protected:
    void writeAdditional(std::vector<std::uint8_t>& data) const override {
        data << someVal;
    }

    void readAdditional(const std::vector<std::uint8_t>& data) override {
        // TODO
    }
};

int main() {
    using namespace Carrot;

    Network::Server server(25565);
    Network::Client client("username");
    client.connect("localhost", 25565);
    //client.queueEvent(std::move(std::make_unique<TestPacket>()));
    client.queueMessage(std::move(std::make_unique<TestPacket>()));

    std::this_thread::sleep_for(std::chrono::seconds(10));


    return 0;
}