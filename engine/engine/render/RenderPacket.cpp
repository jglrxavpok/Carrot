//
// Created by jglrxavpok on 27/12/2021.
//

#include "RenderPacket.h"

#include <core/containers/Vector.hpp>

#include "RenderPacketContainer.h"
#include <engine/Engine.h>
#include <core/math/BasicFunctions.h>
#include "resources/Mesh.h"
#include "resources/Buffer.h"


namespace Carrot::Render {
    Packet::Packet(PacketContainer& container, PassName pass, const Render::PacketType& packetType, std::source_location sourceLocation)
    : container(container)
    , source(sourceLocation)
    , pass(pass)
    , packetType(packetType)
    {
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

        auto& cmd = commands.empty() ? commands.emplace_back().drawIndexedInstanced : commands[0].drawIndexedInstanced;
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

    void Packet::clearPerDrawData() {
        container.deallocateGeneric(std::move(perDrawData));
        perDrawData = {};
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
#if 0
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
        std::size_t newCommandsOffset = indexedDrawCommands.size();
        indexedDrawCommands.resize(newCommandsOffset + other.indexedDrawCommands.size());
        for (std::size_t j = 0; j < other.indexedDrawCommands.size(); ++j) {
            auto& newCommand = indexedDrawCommands.emplace_back();
            newCommand = other.indexedDrawCommands[j];
            newCommand.firstInstance += instanceCount;
        }
        instanceCount += other.instanceCount;

        std::size_t oldSize = instancingDataBuffer.size();
        auto newInstancingDataBuffer = allocateGeneric(oldSize + other.instancingDataBuffer.size());
        std::memcpy(newInstancingDataBuffer.data(), instancingDataBuffer.data(), instancingDataBuffer.size());
        std::memcpy(newInstancingDataBuffer.data() + instancingDataBuffer.size(), other.instancingDataBuffer.data(), other.instancingDataBuffer.size());
        instancingDataBuffer = std::move(newInstancingDataBuffer);
        return true;
#endif
        return false; // TODO: fix packet merging
    }

    void Packet::record(Carrot::Allocator& tempAllocator, vk::RenderPass pass, const Carrot::Render::Context& renderContext, vk::CommandBuffer& cmds, const Packet* previousPacket) const {
        ZoneScoped;

        if(commands.empty()) {
            return; // nothing to draw
        }

        if(false)
        {
            ZoneScopedN("Aftermath markers");
            GetVulkanDriver().setFormattedMarker(cmds, "Record RenderPacket %s %s %llu", source.file_name(), source.function_name(), (std::uint64_t)source.line());
            GetVulkanDriver().setFormattedMarker(cmds, "command count: %llu instanceCount: %%lu vertexBuffer: %x indexBuffer: %x", (std::uint64_t)commands.size(), (std::uint64_t)instanceCount, vertexBuffer.getDeviceAddress(), indexBuffer.getDeviceAddress());
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

        {
            ZoneScopedN("Change viewport");
            if(previousPacket == nullptr || previousPacket->viewportExtents != viewportExtents) {
                if(viewportExtents.has_value()) {
                    cmds.setViewport(0, viewportExtents.value());
                } else {
                    cmds.setViewport(0, vk::Viewport {
                        .x = 0,
                        .y = 0,
                        .width = static_cast<float>(viewport->getWidth()),
                        .height = static_cast<float>(viewport->getHeight()),
                        .minDepth = 0.0f,
                        .maxDepth = 1.0f,
                    });
                }
            }
        }
        {
            ZoneScopedN("Change scissor");
            if(previousPacket == nullptr || previousPacket->scissor != scissor) {
                if(scissor.has_value()) {
                    cmds.setScissor(0, scissor.value());
                } else {
                    vk::Rect2D scissorRect;
                    scissorRect.offset = vk::Offset2D {
                        .x = 0,
                        .y = 0,
                    };
                    scissorRect.extent = vk::Extent2D {
                        .width = viewport->getWidth(),
                        .height = viewport->getHeight(),
                    };
                    cmds.setScissor(0, scissorRect);
                }
            }
        }

        Carrot::BufferView instanceBuffer;
        if(!instancingDataBuffer.empty()) {
            ZoneScopedN("Upload instance buffer");
            instanceBuffer = renderer.getInstanceBuffer(instancingDataBuffer.size());
            instanceBuffer.directUpload(instancingDataBuffer.data(), instancingDataBuffer.size());
        }

        if(vertexBuffer) {
            if(instanceBuffer) {
                cmds.bindVertexBuffers(0, { vertexBuffer.getVulkanBuffer(), instanceBuffer.getVulkanBuffer() }, { vertexBuffer.getStart(), instanceBuffer.getStart() });
            } else {
                cmds.bindVertexBuffers(0, { vertexBuffer.getVulkanBuffer() }, { vertexBuffer.getStart() });
            }
        } else {
            if(instanceBuffer) {
                cmds.bindVertexBuffers(0, instanceBuffer.getVulkanBuffer(), instanceBuffer.getStart());
            }
        }

        switch(packetType) {
            case PacketType::DrawIndexedInstanced: {
                cmds.bindIndexBuffer(indexBuffer.getVulkanBuffer(), indexBuffer.getStart(), vk::IndexType::eUint32);

                Carrot::Vector<vk::DrawIndexedIndirectCommand> indexedDrawCommands { tempAllocator };
                indexedDrawCommands.ensureReserve(commands.size());
                for(const auto& cmd : commands) {
                    indexedDrawCommands.pushBack(cmd.drawIndexedInstanced);
                }

                Carrot::BufferView indexedDrawCommandBuffer = renderer.getSingleFrameHostBuffer(indexedDrawCommands.size() * sizeof(vk::DrawIndexedIndirectCommand));
                indexedDrawCommandBuffer.directUpload(std::span<const vk::DrawIndexedIndirectCommand>{ indexedDrawCommands });

                cmds.drawIndexedIndirect(indexedDrawCommandBuffer.getVulkanBuffer(), indexedDrawCommandBuffer.getStart(), indexedDrawCommands.size(), sizeof(vk::DrawIndexedIndirectCommand));
            } break;

            case PacketType::DrawUnindexedInstanced: {
                Carrot::Vector<vk::DrawIndirectCommand> unindexedDrawCommands { tempAllocator };
                unindexedDrawCommands.ensureReserve(commands.size());
                for(const auto& cmd : commands) {
                    unindexedDrawCommands.pushBack(cmd.drawUnindexedInstanced);
                }

                Carrot::BufferView unindexedDrawCommandBuffer = renderer.getSingleFrameHostBuffer(unindexedDrawCommands.size() * sizeof(vk::DrawIndirectCommand));
                unindexedDrawCommandBuffer.directUpload(std::span<const vk::DrawIndirectCommand>{ unindexedDrawCommands });

                cmds.drawIndirect(unindexedDrawCommandBuffer.getVulkanBuffer(), unindexedDrawCommandBuffer.getStart(), unindexedDrawCommands.size(), sizeof(vk::DrawIndirectCommand));
            } break;

            case PacketType::Compute: {
                Carrot::Vector<vk::DispatchIndirectCommand> unindexedDispatchCommands { tempAllocator };
                unindexedDispatchCommands.ensureReserve(commands.size());
                for(const auto& cmd : commands) {
                    unindexedDispatchCommands.pushBack(cmd.compute);
                }

                Carrot::BufferView unindexedDispatchCommandBuffer = renderer.getSingleFrameHostBuffer(unindexedDispatchCommands.size() * sizeof(vk::DispatchIndirectCommand));
                unindexedDispatchCommandBuffer.directUpload(std::span<const vk::DispatchIndirectCommand>{ unindexedDispatchCommands });

                cmds.drawIndirect(unindexedDispatchCommandBuffer.getVulkanBuffer(), unindexedDispatchCommandBuffer.getStart(), unindexedDispatchCommands.size(), sizeof(vk::DispatchIndirectCommand));
                break;
            }

            case PacketType::Mesh: {
                Carrot::Vector<vk::DrawMeshTasksIndirectCommandEXT> drawMeshTasks { tempAllocator };
                drawMeshTasks.ensureReserve(commands.size());
                for(const auto& cmd : commands) {
                    drawMeshTasks.pushBack(cmd.drawMeshTasks);
                }

                Carrot::BufferView meshDrawCommandBuffer = renderer.getSingleFrameHostBuffer(drawMeshTasks.size() * sizeof(vk::DrawMeshTasksIndirectCommandEXT));
                meshDrawCommandBuffer.directUpload(std::span<const vk::DrawMeshTasksIndirectCommandEXT>{ drawMeshTasks });

                cmds.drawMeshTasksIndirectEXT(meshDrawCommandBuffer.getVulkanBuffer(),
                    meshDrawCommandBuffer.getStart(),
                    drawMeshTasks.size(),
                    sizeof(vk::DrawMeshTasksIndirectCommandEXT));
            } break;

            default:
                verify(false, "Don't know what to do :(");
        }
    }

    std::span<std::uint8_t> Packet::allocateGeneric(std::size_t size) {
        return container.allocateGeneric(size);
    }

    void Packet::validate() const {
        verify(pipeline, "Pipeline must not be null");
        verify(pass != Render::PassEnum::Undefined, "Render pass must be defined");

        verify(commands.size() > 0, "Must have at least one draw command");
        verify(viewport, "Viewport must not be null");
        switch(packetType) {
            case PacketType::DrawIndexedInstanced: {
                verify(indexBuffer, "Index buffer must not be null if there are indexed draw commands");
                verify(((VkBuffer)indexBuffer.getVulkanBuffer()) != VK_NULL_HANDLE, "Index buffer must not be null if there are indexed draw commands");
            } break;

            case PacketType::Compute:
            case PacketType::Mesh:
                verify(!vertexBuffer, "Cannot use index buffer in compute or mesh shaders");
                verify(!indexBuffer, "Cannot use index buffer in compute or mesh shaders");
                break;

            default:
            case PacketType::Unknown:
                verify(false, "Unknown packet type");
                break;
        }

        if(!perDrawData.empty()) {
            verify(commands.size() == perDrawData.size_bytes() / sizeof(GBufferDrawData), "Must have as many commands than per draw data!");
        }
    }

    Packet& Packet::operator=(Packet&& toMove) {
        ZoneScopedN("Packet::operator= move");
        pipeline = std::move(toMove.pipeline);
        pass = std::move(toMove.pass);
        viewport = std::move(toMove.viewport);
        viewportExtents = std::move(toMove.viewportExtents);
        scissor = std::move(toMove.scissor);

        vertexBuffer = std::move(toMove.vertexBuffer);
        indexBuffer = std::move(toMove.indexBuffer);
        packetType = toMove.packetType;
        commands = std::move(toMove.commands);
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
        viewportExtents = toCopy.viewportExtents;
        scissor = toCopy.scissor;

        vertexBuffer = toCopy.vertexBuffer;
        indexBuffer = toCopy.indexBuffer;
        packetType = toCopy.packetType;
        commands = toCopy.commands;
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

    void Packet::PushConstant::freeGeneric(std::span<std::uint8_t> data) {
        return container.deallocateGeneric(std::move(data));
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