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
    class Packet {
    public:
        struct TransparentPassData {
            float zOrder = 0.0f;
        };

        class PushConstant {
        public:
            PushConstant() = default;

            std::string id;
            vk::ShaderStageFlags stages = static_cast<vk::ShaderStageFlags>(0);
            std::vector<std::uint8_t> pushData;

            template<typename T>
            void setData(T&& data) {
                pushData.resize(sizeof(data));
                std::memcpy(pushData.data(), &data, sizeof(data));
            }
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
        explicit Packet(PassEnum pass, std::source_location location = std::source_location::current());

        Packet(const Packet&) = default;
        Packet(Packet&& toMove);

        Packet& operator=(Packet&& toMove) = default;
        Packet& operator=(const Packet& toCopy) = default;

        void useMesh(Carrot::Mesh& mesh);

        template<typename T>
        void useInstance(T&& instance) {
            instancingDataBuffer.resize(sizeof(instance));
            std::memcpy(instancingDataBuffer.data(), &instance, sizeof(instance));
        }

        PushConstant& addPushConstant(const std::string& id = "", vk::ShaderStageFlags stages = static_cast<vk::ShaderStageFlags>(0));

        bool merge(const Packet& other);

        void record(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) const;

    private:
        std::source_location source;
        std::vector<std::uint8_t> instancingDataBuffer;
        std::list<PushConstant> pushConstants;
    };
}