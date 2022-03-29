//
// Created by jglrxavpok on 27/12/2021.
//

#include "RenderPacket.h"
#include "RenderPacketContainer.h"
#include <engine/Engine.h>
#include "resources/Mesh.h"
#include "resources/Buffer.h"


namespace Carrot::Render {
    Packet::Packet(PacketContainer& container, PassEnum pass, std::source_location sourceLocation): container(container), source(sourceLocation), pass(pass) {
        viewport = &GetEngine().getMainViewport();
    };

    Packet::Packet(const Packet& toCopy): container(toCopy.container) {
        *this = toCopy;
    }

    Packet::Packet(Packet&& toMove): container(toMove.container) {
        *this = std::move(toMove);
    }

    Packet::~Packet() {
        container.deallocateGeneric(std::move(instancingDataBuffer));
    }

    void Packet::useMesh(Carrot::Mesh& mesh) {
        vertexBuffer = BufferView(nullptr, mesh.getBackingBuffer(), mesh.getVertexStartOffset(), mesh.getVertexSize());
        indexBuffer = BufferView(nullptr, mesh.getBackingBuffer(), mesh.getIndexStartOffset(), mesh.getIndexSize());
        indexCount = mesh.getIndexCount();
    }

    Packet::PushConstant& Packet::addPushConstant(const std::string& id, vk::ShaderStageFlags stages) {
        pushConstants.emplace_back(&container.makePushConstant());
        auto& result = pushConstants.back();
        result->id = id;
        result->stages = stages;
        return *result;
    }

    bool Packet::merge(const Packet& other) {
        // verify if packets are merge-able
        if(viewport != other.viewport) return false;
        if(pass != other.pass) return false;
        if(pipeline != other.pipeline) return false;
        if(vertexBuffer != other.vertexBuffer) return false;
        if(indexBuffer != other.indexBuffer) return false;
        if(indexCount != other.indexCount) return false;

        if(pushConstants.size() != other.pushConstants.size()) return false;

        auto it = pushConstants.begin();
        auto otherIt = other.pushConstants.begin();
        auto end = pushConstants.end();
        while(it != end) {
            verify(otherIt != other.pushConstants.end(), "check that pushConstants.size() == other.pushConstants.size()");

            const auto& pushConstant = *(*it);
            const auto& otherPushConstant = *(*otherIt);
            if(pushConstant.stages != otherPushConstant.stages) {
                return false;
            }

            if(pushConstant.id != otherPushConstant.id) {
                return false;
            }

            const auto& data = pushConstant.pushData;
            const auto& otherData = otherPushConstant.pushData;

            if(data.size() != otherData.size()) {
                return false;
            }

            for (std::size_t j = 0; j < data.size(); ++j) {
                if (data[j] != otherData[j]) {
                    return false;
                }
            }

            it++;
            otherIt++;
        }

        // everything verified, merge packets
        instanceCount += other.instanceCount;

        std::size_t oldSize = instancingDataBuffer.size();
        auto newInstancingDataBuffer = allocateGeneric(oldSize + other.instancingDataBuffer.size());
        std::memcpy(newInstancingDataBuffer.data(), instancingDataBuffer.data(), instancingDataBuffer.size());
        std::memcpy(newInstancingDataBuffer.data() + instancingDataBuffer.size(), other.instancingDataBuffer.data(), other.instancingDataBuffer.size());
        instancingDataBuffer = std::move(newInstancingDataBuffer);
        return true;
    }

    void Packet::record(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& cmds) const {
        ZoneScoped;
        auto& renderer = renderContext.renderer;

        pipeline->bind(pass, renderContext, cmds);
        for(const auto& pushConstant : pushConstants) {
            const auto& layout = pipeline->getPipelineLayout();
            const auto& range = pipeline->getPushConstant(pushConstant->id);
            cmds.pushConstants(layout, pushConstant->stages, range.offset, pushConstant->pushData.size(), pushConstant->pushData.data());
        }

        if(!instancingDataBuffer.empty()) {
            Carrot::BufferView instanceBuffer = renderer.getInstanceBuffer(instancingDataBuffer.size());
            instanceBuffer.directUpload(instancingDataBuffer.data(), instancingDataBuffer.size());

            cmds.bindVertexBuffers(0, { vertexBuffer.getVulkanBuffer(), instanceBuffer.getVulkanBuffer() }, { vertexBuffer.getStart(), instanceBuffer.getStart() });
        } else {
            cmds.bindVertexBuffers(0, vertexBuffer.getVulkanBuffer(), vertexBuffer.getStart());
        }
        cmds.bindIndexBuffer(indexBuffer.getVulkanBuffer(), indexBuffer.getStart(), vk::IndexType::eUint32);

        cmds.drawIndexed(indexCount, instanceCount, 0, 0, 0);
    }

    std::span<std::uint8_t> Packet::allocateGeneric(std::size_t size) {
        return container.allocateGeneric(size);
    }

    Packet& Packet::operator=(Packet&& toMove) {
        ZoneScopedN("Packet::operator= move");
        pipeline = std::move(toMove.pipeline);
        pass = std::move(toMove.pass);
        viewport = std::move(toMove.viewport);

        vertexBuffer = std::move(toMove.vertexBuffer);
        indexBuffer = std::move(toMove.indexBuffer);
        indexCount = std::move(toMove.indexCount);
        instanceCount = std::move(toMove.instanceCount);

        transparentGBuffer = std::move(toMove.transparentGBuffer);

        source = std::move(toMove.source);
        instancingDataBuffer = std::move(toMove.instancingDataBuffer);
        pushConstants = std::move(toMove.pushConstants);
        return *this;
    }

    Packet& Packet::operator=(const Packet& toCopy) {
        ZoneScopedN("Packet::operator= copy");
        pipeline = toCopy.pipeline;
        pass = toCopy.pass;
        viewport = toCopy.viewport;

        vertexBuffer = toCopy.vertexBuffer;
        indexBuffer = toCopy.indexBuffer;
        indexCount = toCopy.indexCount;
        instanceCount = toCopy.instanceCount;

        transparentGBuffer = toCopy.transparentGBuffer;

        source = toCopy.source;

        instancingDataBuffer = toCopy.instancingDataBuffer;
        // deep copies
        pushConstants.clear();
        for(const auto& pPushConstant : toCopy.pushConstants) {
            auto& pushConstant = container.makePushConstant();
            pushConstant = *pPushConstant;
            pushConstants.emplace_back(&pushConstant);
        }
        return *this;
    }

    Packet::PushConstant::~PushConstant() {
        container.deallocateGeneric(std::move(pushData));
    }

    std::span<std::uint8_t> Packet::PushConstant::allocateGeneric(std::size_t size) {
        return container.allocateGeneric(size);
    }

    Packet::PushConstant& Packet::PushConstant::operator=(const Packet::PushConstant& other) {
        id = other.id;
        stages = other.stages;
        pushData = container.copyGeneric(other.pushData);
        return *this;
    }

    Packet::PushConstant& Packet::PushConstant::operator=(Packet::PushConstant&& other) {
        id = std::move(other.id);
        stages = other.stages;
        pushData = std::move(other.pushData);
        return *this;
    }

}