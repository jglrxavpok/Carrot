//
// Created by jglrxavpok on 27/12/2021.
//

#include "RenderPacket.h"
#include "RenderPacketContainer.h"
#include <engine/Engine.h>
#include <core/math/BasicFunctions.h>
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
        container.deallocateGeneric(std::move(perDrawData));
    }

    void Packet::useMesh(Carrot::Mesh& mesh) {
        vertexBuffer = mesh.getVertexBuffer();
        indexBuffer = mesh.getIndexBuffer();

        auto& cmd = drawCommands.empty() ? drawCommands.emplace_back() : drawCommands[0];
        cmd.indexCount = mesh.getIndexCount();
        cmd.instanceCount = 1;
    }

    void Packet::addPerDrawData(const std::span<const GBufferDrawData>& data) {
        std::span<std::uint8_t> newPerDrawData = allocateGeneric(perDrawData.size() + data.size_bytes());
        if(!perDrawData.empty()) {
            std::memcpy(newPerDrawData.data(), perDrawData.data(), perDrawData.size_bytes());
        }
        std::memcpy(newPerDrawData.data() + perDrawData.size_bytes(), data.data(), data.size_bytes());
        container.deallocateGeneric(std::move(perDrawData));
        perDrawData = newPerDrawData;
    }

    Packet::PushConstant& Packet::addPushConstant(const std::string& id, vk::ShaderStageFlags stages) {
        verify(pushConstantCount < MAX_PUSH_CONSTANTS, "Too many push constants. Lower your usage, or update the engine");
        std::size_t pushConstantIndex = pushConstantCount++;
        pushConstants[pushConstantIndex] = &container.makePushConstant();
        auto& result = pushConstants[pushConstantIndex];
        result->id = id;
        result->stages = stages;
        return *result;
    }

    bool Packet::merge(const Packet& other) {
        if(true)
            return false; // TODO: fix packet merging

        // verify if packets are merge-able
        if(viewport != other.viewport) return false;
        if(pass != other.pass) return false;
        if(pipeline != other.pipeline) return false;
        if(vertexBuffer != other.vertexBuffer) return false;
        if(indexBuffer != other.indexBuffer) return false;

        if(pushConstantCount != other.pushConstantCount) return false;

        for(std::size_t pushConstantIndex = 0; pushConstantIndex < pushConstantCount; pushConstantIndex++) {
            const auto& pushConstant = *pushConstants[pushConstantIndex];
            const auto& otherPushConstant = *other.pushConstants[pushConstantIndex];
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
        }

        // everything verified, merge packets
        // TODO: merge with another drawcommand if possible
        std::size_t newCommandsOffset = drawCommands.size();
        drawCommands.resize(newCommandsOffset + other.drawCommands.size());
        for (std::size_t j = 0; j < other.drawCommands.size(); ++j) {
            auto& newCommand = drawCommands.emplace_back();
            newCommand = other.drawCommands[j];
            newCommand.firstInstance += instanceCount;
        }
        instanceCount += other.instanceCount;

        std::size_t oldSize = instancingDataBuffer.size();
        auto newInstancingDataBuffer = allocateGeneric(oldSize + other.instancingDataBuffer.size());
        std::memcpy(newInstancingDataBuffer.data(), instancingDataBuffer.data(), instancingDataBuffer.size());
        std::memcpy(newInstancingDataBuffer.data() + instancingDataBuffer.size(), other.instancingDataBuffer.data(), other.instancingDataBuffer.size());
        instancingDataBuffer = std::move(newInstancingDataBuffer);
        return true;
    }

    void Packet::record(vk::RenderPass pass, const Carrot::Render::Context& renderContext, vk::CommandBuffer& cmds, const Packet* previousPacket) const {
        ZoneScoped;

        if(false)
        {
            ZoneScopedN("Aftermath markers");
            GetVulkanDriver().setFormattedMarker(cmds, "Record RenderPacket %s %s %llu", source.file_name(), source.function_name(), (std::uint64_t)source.line());
            GetVulkanDriver().setFormattedMarker(cmds, "command count: %llu instanceCount: %%lu vertexBuffer: %x indexBuffer: %x", (std::uint64_t)drawCommands.size(), (std::uint64_t)instanceCount, vertexBuffer.getDeviceAddress(), indexBuffer.getDeviceAddress());
        }

        auto& renderer = renderContext.renderer;

        const bool skipPipelineBind = previousPacket != nullptr
                && previousPacket->pipeline == pipeline;
        std::vector<std::uint32_t> dynamicOffsets;
        if(!perDrawData.empty())
            dynamicOffsets.push_back(renderer.uploadPerDrawData(std::span{ (GBufferDrawData*)perDrawData.data(), perDrawData.size_bytes() / sizeof(GBufferDrawData) }));
        if(!skipPipelineBind) {
            ZoneScopedN("Change pipeline");
            pipeline->bind(pass, renderContext, cmds, vk::PipelineBindPoint::eGraphics, dynamicOffsets);
        } else {
            pipeline->bindOnlyDescriptorSets(renderContext, cmds, vk::PipelineBindPoint::eGraphics, dynamicOffsets);
        }
        {
            ZoneScopedN("Add push constants");
            for(std::size_t index = 0; index < pushConstantCount; index++) {
                const auto& layout = pipeline->getPipelineLayout();
                const auto& range = pipeline->getPushConstant(pushConstants[index]->id);
                cmds.pushConstants(layout, pushConstants[index]->stages, range.offset, pushConstants[index]->pushData.size(), pushConstants[index]->pushData.data());
            }
        }

        if(!instancingDataBuffer.empty()) {
            ZoneScopedN("Upload instance buffer");
            Carrot::BufferView instanceBuffer = renderer.getInstanceBuffer(instancingDataBuffer.size());
            instanceBuffer.directUpload(instancingDataBuffer.data(), instancingDataBuffer.size());

            cmds.bindVertexBuffers(0, { vertexBuffer.getVulkanBuffer(), instanceBuffer.getVulkanBuffer() }, { vertexBuffer.getStart(), instanceBuffer.getStart() });
        } else {
            cmds.bindVertexBuffers(0, vertexBuffer.getVulkanBuffer(), vertexBuffer.getStart());
        }
        cmds.bindIndexBuffer(indexBuffer.getVulkanBuffer(), indexBuffer.getStart(), vk::IndexType::eUint32);

        // TODO: vkCmdDrawIndexedIndirect
        Carrot::BufferView drawCommandBuffer = renderer.getSingleFrameBuffer(drawCommands.size() * sizeof(vk::DrawIndexedIndirectCommand));
        drawCommandBuffer.directUpload(std::span { drawCommands });

        cmds.drawIndexedIndirect(drawCommandBuffer.getVulkanBuffer(), drawCommandBuffer.getStart(), drawCommands.size(), sizeof(vk::DrawIndexedIndirectCommand));
    }

    std::span<std::uint8_t> Packet::allocateGeneric(std::size_t size) {
        return container.allocateGeneric(size);
    }

    void Packet::validate() const {
        verify(pipeline, "Pipeline must not be null");
        verify(pass != Render::PassEnum::Undefined, "Render pass must be defined");
        verify(vertexBuffer, "Vertex buffer must not be null");
        verify(((VkBuffer)vertexBuffer.getVulkanBuffer()) != VK_NULL_HANDLE, "Vertex buffer must not be null");
        verify(indexBuffer, "Index buffer must not be null");
        verify(((VkBuffer)indexBuffer.getVulkanBuffer()) != VK_NULL_HANDLE, "Index buffer must not be null");
        verify(viewport, "Viewport must not be null");

        if(!perDrawData.empty()) {
            verify(drawCommands.size() == perDrawData.size_bytes() / sizeof(GBufferDrawData), "Must have as many drawCommands than per draw data!");
        }
    }

    Packet& Packet::operator=(Packet&& toMove) {
        ZoneScopedN("Packet::operator= move");
        pipeline = std::move(toMove.pipeline);
        pass = std::move(toMove.pass);
        viewport = std::move(toMove.viewport);

        vertexBuffer = std::move(toMove.vertexBuffer);
        indexBuffer = std::move(toMove.indexBuffer);
        drawCommands = std::move(toMove.drawCommands);
        perDrawData = std::move(toMove.perDrawData);
        instanceCount = std::move(toMove.instanceCount);

        transparentGBuffer = std::move(toMove.transparentGBuffer);

        source = std::move(toMove.source);
        instancingDataBuffer = std::move(toMove.instancingDataBuffer);
        pushConstantCount = std::exchange(toMove.pushConstantCount, 0);
        for(std::size_t i = 0; i < pushConstantCount; i++) {
            pushConstants[i] = std::exchange(toMove.pushConstants[i], nullptr);
        }
        return *this;
    }

    Packet& Packet::operator=(const Packet& toCopy) {
        ZoneScopedN("Packet::operator= copy");
        if(this == &toCopy)
            return *this;

        pipeline = toCopy.pipeline;
        pass = toCopy.pass;
        viewport = toCopy.viewport;

        vertexBuffer = toCopy.vertexBuffer;
        indexBuffer = toCopy.indexBuffer;
        drawCommands = toCopy.drawCommands;
        perDrawData = toCopy.perDrawData;
        instanceCount = toCopy.instanceCount;

        transparentGBuffer = toCopy.transparentGBuffer;

        source = toCopy.source;

        instancingDataBuffer = toCopy.instancingDataBuffer;
        // deep copies
        pushConstantCount = toCopy.pushConstantCount;
        for(std::size_t pushConstantIndex = 0; pushConstantIndex < pushConstantCount; pushConstantIndex++) {
            auto& pushConstant = container.makePushConstant();
            pushConstant = *toCopy.pushConstants[pushConstantIndex];
            pushConstants[pushConstantIndex] = &pushConstant;
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