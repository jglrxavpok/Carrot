//
// Created by jglrxavpok on 27/12/2021.
//

#pragma once

#include <memory>
#include <vector>
#include <source_location>
#include <list>
#include "resources/Buffer.h"
#include "resources/BufferView.h"
#include "PassEnum.h"

namespace Carrot {
    class Pipeline;
    class Mesh;

    namespace Render {
        class Viewport;
    }
}

namespace Carrot::Render {
    class PacketContainer;

    class Packet {
    public:
        constexpr static std::size_t MAX_PUSH_CONSTANTS = 4;

        struct TransparentPassData {
            float zOrder = 0.0f;
        };

        class PushConstant {
        public:
            std::string id;
            vk::ShaderStageFlags stages = static_cast<vk::ShaderStageFlags>(0);
            std::span<std::uint8_t> pushData;

        public:
            PushConstant(PacketContainer& container): container(container) {}

            ~PushConstant();

            PushConstant& operator=(const PushConstant& other);
            PushConstant& operator=(PushConstant&& other);

        public:
            template<typename T>
            void setData(T&& data) {
                pushData = allocateGeneric(sizeof(data));
                std::memcpy(pushData.data(), &data, sizeof(data));
            }

        private:
            std::span<std::uint8_t> allocateGeneric(std::size_t size);

        private:
            PacketContainer& container;
        };

    public:
        std::shared_ptr<Carrot::Pipeline> pipeline;
        PassEnum pass = PassEnum::Undefined;
        Render::Viewport* viewport = nullptr;

        Carrot::BufferView vertexBuffer;
        Carrot::BufferView indexBuffer;
        std::uint32_t indexCount = 0;
        std::uint32_t instanceCount = 1;

        TransparentPassData transparentGBuffer;

    public:
        explicit Packet(PacketContainer& container, PassEnum pass, std::source_location location = std::source_location::current());

        Packet(const Packet&);
        Packet(Packet&& toMove);

        ~Packet();

        Packet& operator=(Packet&& toMove);
        Packet& operator=(const Packet& toCopy);

        void useMesh(Carrot::Mesh& mesh);

        template<typename T>
        void useInstance(T&& instance) {
            instancingDataBuffer = allocateGeneric(sizeof(instance));
            std::memcpy(instancingDataBuffer.data(), &instance, sizeof(instance));
        }

        PushConstant& addPushConstant(const std::string& id = "", vk::ShaderStageFlags stages = static_cast<vk::ShaderStageFlags>(0));

        bool merge(const Packet& other);

        ///
        /// \param pass
        /// \param renderContext
        /// \param commands
        /// \param skipPipelineBind if you know the proper pipeline is already bound, you can skip its bind with this parameter to save time
        void record(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands, bool skipPipelineBind = false) const;

    private:
        std::span<std::uint8_t> allocateGeneric(std::size_t size);

    private:
        PacketContainer& container;
        std::source_location source;
        std::span<std::uint8_t> instancingDataBuffer;
        std::size_t pushConstantCount = 0;
        PushConstant* pushConstants[MAX_PUSH_CONSTANTS];
    };
}