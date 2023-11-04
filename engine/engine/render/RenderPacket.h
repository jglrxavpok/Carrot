//
// Created by jglrxavpok on 27/12/2021.
//

#pragma once

#include <memory>
#include <vector>
#include <source_location>
#include <list>
#include <span>
#include "resources/Buffer.h"
#include "resources/BufferView.h"
#include "PassEnum.h"
#include "GBufferDrawData.h"

namespace Carrot {
    class Pipeline;
    class Mesh;
    class VulkanRenderer;

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

        std::uint32_t instanceCount = 1; // Total number of instances for this packet, used to offset firstInstance when merging packets

        std::vector<vk::DrawIndexedIndirectCommand> drawCommands;

        TransparentPassData transparentGBuffer;

        /// Setting this value will change the viewport property of the packet.
        /// ie similar to calling vkCmdSetViewport
        std::optional<vk::Viewport> viewportExtents;
        std::optional<vk::Rect2D> scissor;

    public:
        explicit Packet(PacketContainer& container, PassEnum pass, std::source_location location = std::source_location::current());

        Packet(const Packet&);
        Packet(Packet&& toMove);

        ~Packet();

        Packet& operator=(Packet&& toMove);
        Packet& operator=(const Packet& toCopy);

        void useMesh(Carrot::Mesh& mesh);

        template<typename T>
        void useInstances(const std::span<T>& instance) {
            instancingDataBuffer = allocateGeneric(instance.size_bytes());
            std::memcpy(instancingDataBuffer.data(), instance.data(), instance.size_bytes());
        }

        template<typename T>
        void useInstance(T& instance) {
            useInstances(std::span<T>{&instance, 1});
        }

        void addPerDrawData(const std::span<const GBufferDrawData>& data);
        void clearPerDrawData();

        PushConstant& addPushConstant(const std::string& id = "", vk::ShaderStageFlags stages = static_cast<vk::ShaderStageFlags>(0));

        bool merge(const Packet& other);

        ///
        /// \param pass
        /// \param renderContext
        /// \param commands
        /// \param previousRenderPacket if you know the proper state is already bound, you can skip its bind thanks to this parameter to save time
        void record(vk::RenderPass pass, const Carrot::Render::Context& renderContext, vk::CommandBuffer& commands, const Packet* previousRenderPacket) const;

    private:
        std::span<std::uint8_t> allocateGeneric(std::size_t size);

        void validate() const;

    private:
        PacketContainer& container;
        std::source_location source;
        std::span<std::uint8_t> instancingDataBuffer;
        std::span<std::uint8_t> perDrawData;
        std::size_t pushConstantCount = 0;
        PushConstant* pushConstants[MAX_PUSH_CONSTANTS];

        friend class VulkanRenderer;
    };
}