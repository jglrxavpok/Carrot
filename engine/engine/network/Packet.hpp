//
// Created by jglrxavpok on 28/08/2021.
//

#pragma once

#include <memory>
#include <span>
#include <vector>
#include <unordered_map>
#include <core/utils/Assert.h>
#include <core/io/Serialisation.h>

namespace Carrot::Network {
    using PacketID = std::uint32_t;

    struct PacketBuffer {
    public:
        PacketID packetType = -1;
        std::vector<std::uint8_t> data;

        explicit PacketBuffer() = default;
        PacketBuffer(const PacketBuffer&) = default;
        PacketBuffer(PacketBuffer&&) = default;

        explicit PacketBuffer(const std::span<std::uint8_t>& input) {
            verify(input.size() >= 2*sizeof(std::uint32_t), "Not enough data inside packet buffer!");

            auto* header = reinterpret_cast<uint32_t *>(input.data());
            packetType = header[0];
            std::uint32_t dataSize = header[1];
            verify(input.size() >= dataSize+2*sizeof(std::uint32_t), "Not enough data inside packet buffer!");
            data.resize(dataSize);
            std::memcpy(data.data(), input.data()+ 2*sizeof(std::uint32_t), dataSize);
        }

        void write(std::vector<std::uint8_t>& destination) {
            destination << packetType;
            destination << static_cast<std::uint32_t>(data.size());

            std::size_t prevSize = destination.size();
            destination.resize(prevSize + data.size());
            std::memcpy(destination.data() + prevSize, data.data(), data.size());
        }

        std::size_t sizeOf() const {
            return sizeof(std::uint32_t) * 2 /* packet ID + data length */ + data.size();
        }
    };

    class Packet {
    public:
        using Ptr = std::shared_ptr<Packet>;

        explicit Packet(PacketID id): packetType(id) {};

        virtual ~Packet() = default;

    public: // serialisation
        [[nodiscard]] PacketBuffer toBuffer() const {
            PacketBuffer buffer;
            buffer.packetType = packetType;
            writeAdditional(buffer.data);
            return std::move(buffer);
        }

        virtual void writeAdditional(std::vector<std::uint8_t>& data) const = 0;
        virtual void readAdditional(const std::vector<std::uint8_t>& data) = 0;

        PacketID getPacketID() const { return packetType; }

    protected:
        PacketID packetType = -1;

    private:
    };

    class Protocol {
        struct PacketGen {
            virtual Packet::Ptr operator()() const = 0;
            virtual std::unique_ptr<PacketGen> copy() const = 0;
        };

        template<typename PacketType> requires std::is_base_of_v<Packet, PacketType>
        struct DefaultPacketGen: public PacketGen {
            typename PacketType::Ptr operator()() const override {
                return std::make_shared<PacketType>();
            }

            std::unique_ptr<PacketGen> copy() const override {
                return std::make_unique<DefaultPacketGen<PacketType>>();
            }
        };

    public:
        explicit Protocol() = default;
        Protocol(Protocol&&) = default;

        Protocol(const Protocol& toCopy) {
            *this = toCopy;
        }

        template<PacketID ID, typename PacketType>
        Protocol& with() {
            if(entries[ID])
                throw std::runtime_error("A packet with ID " + std::to_string(ID) + " already exists.");
            entries[ID] = std::make_unique<DefaultPacketGen<PacketType>>();
            return *this;
        }

        [[nodiscard]] Packet::Ptr make(PacketID packetID) const {
            auto generatorLoc = entries.find(packetID);
            if(generatorLoc == entries.end()) {
                throw std::runtime_error("Unknown packet ID: " + std::to_string(packetID));
            }
            return generatorLoc->second->operator()();
        }

        Protocol& operator=(const Protocol& toCopy) {
            entries.clear();
            for(const auto& [key, value] : toCopy.entries) {
                entries[key] = value->copy();
            }
            return *this;
        }

    private:
        std::unordered_map<PacketID, std::unique_ptr<PacketGen>> entries;
    };
}