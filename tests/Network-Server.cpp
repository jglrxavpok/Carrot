//
// Created by jglrxavpok on 25/08/2021.
//
#include <ctime>
#include <iostream>
#include <string>
#include <asio.hpp>
#include <engine/io/Serialisation.h>
#include <engine/network/client/Client.h>
#include <engine/network/server/Server.h>
#include <engine/utils/Macros.h>
#include <engine/io/Logging.hpp>

struct TestPacket: public Carrot::Network::Packet {
public:
    float someVal = 42.0f;

    explicit TestPacket(): Carrot::Network::Packet(42) {}
    explicit TestPacket(float value): Carrot::Network::Packet(42), someVal(value) {}

protected:
    void writeAdditional(std::vector<std::uint8_t>& data) const override {
        data << someVal;
    }

    void readAdditional(const std::vector<std::uint8_t>& data) override {
        Carrot::IO::VectorReader r{data};
        r >> someVal;
    }
};

int main() {
    using namespace Carrot;

    Network::Server server(25565);
    Network::Protocol protocol = Carrot::Network::Protocol().
            with<42, TestPacket>()
    ;
    server.setPlayProtocol(protocol);

    struct ServerConsumer : public Carrot::Network::Server::IPacketConsumer {
        explicit ServerConsumer(Network::Server& server): server(server) {}

        void consumePacket(const UUID& clientID, const Network::Packet::Ptr& packet) override {
            std::string str = Carrot::toString(clientID);
            Carrot::Log::info("Received a packet from client %s", str.c_str());
            switch(packet->getPacketID()) {
                case 42: {
                    auto testPacket = std::reinterpret_pointer_cast<const TestPacket>(packet);
                    Carrot::Log::info("TestPacket, value is %f", testPacket->someVal);
                    server.broadcastMessage(std::make_shared<TestPacket>(-50.0f));
                } break;

                default: TODO
            }
        }

        Carrot::Network::Server& server;
    } serverConsumer{server};

    struct ClientConsumer : public Carrot::Network::Client::IPacketConsumer {
        void consumePacket(const Network::Packet::Ptr& packet) override {
            Carrot::Log::info("Received a packet on client!");
            switch(packet->getPacketID()) {
                case 42: {
                    auto testPacket = std::reinterpret_pointer_cast<const TestPacket>(packet);
                    Carrot::Log::info("(Client) TestPacket, value is %f", testPacket->someVal);
                } break;

                default: TODO
            }
        }
    } clientConsumer;

    server.setPacketConsumer(&serverConsumer);

    Network::Client client(U"username"s);
    Network::Client client2(U"username2");
    client.setPacketConsumer(&clientConsumer);
    client2.setPacketConsumer(&clientConsumer);

    client.connect("localhost", 25565);

    client.setPlayProtocol(protocol);
    client2.setPlayProtocol(protocol);
    client2.connect("localhost", 25565);
//    client.queueEvent(std::move(std::make_unique<TestPacket>()));
    for (int i = 0; i < 10; ++i) {
        client.queueMessage(std::move(std::make_unique<TestPacket>(i)));
        client2.queueMessage(std::move(std::make_unique<TestPacket>(i)));
    }

    std::this_thread::sleep_for(std::chrono::seconds(10));
    return 0;
}