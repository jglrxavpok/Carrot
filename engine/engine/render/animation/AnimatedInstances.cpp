//
// Created by jglrxavpok on 16/04/2021.
//

#include "AnimatedInstances.h"

#include <utility>
#include <core/io/Logging.hpp>
#include <engine/console/RuntimeOption.hpp>
#include <engine/render/resources/ResourceAllocator.h>

#include "engine/render/resources/Buffer.h"
#include "engine/render/Model.h"
#include "engine/render/resources/Mesh.h"
#include "engine/render/GBufferDrawData.h"
#include "engine/render/raytracing/ASBuilder.h"
#include "engine/render/resources/LightMesh.h"
#include "engine/vulkan/VulkanDriver.h"
#include "engine/Engine.h"

extern Carrot::RuntimeOption DrawBoundingSpheres;

Carrot::AnimatedInstances::AnimatedInstances(Carrot::Engine& engine, std::shared_ptr<Model> animatedModel, size_t maxInstanceCount):
    engine(engine), model(std::move(animatedModel)), maxInstanceCount(maxInstanceCount) {

    // TODO: don't crash if there are no skinned meshes inside model
    instanceBuffer = std::make_unique<Buffer>(GetVulkanDriver(),
                                              maxInstanceCount*sizeof(AnimatedInstanceData),
                                              vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer,
                                              vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
    animatedInstances = instanceBuffer->map<AnimatedInstanceData>();
    std::fill_n(animatedInstances, maxInstanceCount, AnimatedInstanceData{});

    std::map<MeshID, std::vector<vk::DrawIndexedIndirectCommand>> indirectCommands{};
    auto meshes = model->getSkinnedMeshes();
    uint64_t maxVertexCount = 0;

    forEachMesh([&](std::uint32_t meshIndex, std::uint32_t materialSlot, Carrot::Mesh::Ref& mesh) {
        indirectBuffers[mesh->getMeshID()] = std::make_shared<Buffer>(GetVulkanDriver(),
                                                                      maxInstanceCount * sizeof(vk::DrawIndexedIndirectCommand),
                                                                      vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                                                      vk::MemoryPropertyFlagBits::eDeviceLocal);
        size_t meshSize = mesh->getVertexCount();
        maxVertexCount = std::max(meshSize, maxVertexCount);

        meshOffsets[mesh->getMeshID()] = vertexCountPerInstance;
        vertexCountPerInstance += meshSize;
    });

    // TODO: swapchainlength-buffering?
    flatVertices = std::make_unique<Buffer>(GetVulkanDriver(),
                                            sizeof(SkinnedVertex) * vertexCountPerInstance,
                                            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                            vk::MemoryPropertyFlagBits::eDeviceLocal,
                                            engine.createGraphicsAndTransferFamiliesSet());
    flatVertices->name("flat vertices");

    // copy mesh vertices into a flat buffer to allow for easy indexing inside the compute shader
    engine.getVulkanDriver().performSingleTimeTransferCommands([&](vk::CommandBuffer& commands) {
        forEachMesh([&](std::uint32_t meshIndex, std::uint32_t materialSlot, Carrot::Mesh::Ref& mesh) {
            int32_t vertexOffset = static_cast<int32_t>(meshOffsets[mesh->getMeshID()]);

            vk::BufferCopy region {
                    .srcOffset = mesh->getVertexBuffer().getStart(),
                    .dstOffset = vertexOffset*sizeof(SkinnedVertex),
                    .size = mesh->getVertexCount()*sizeof(SkinnedVertex),
            };
            commands.copyBuffer(mesh->getVertexBuffer().getVulkanBuffer(), flatVertices->getVulkanBuffer(), region);
        });
    });

    fullySkinnedVertexBuffer = GetResourceAllocator().allocateDeviceBuffer(sizeof(Vertex) * vertexCountPerInstance * maxInstanceCount, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR);
    fullySkinnedVertexBuffer.name(Carrot::sprintf("full skinned vertices %s", model->debugName.c_str()));

    raytracingBLASes.reserve(maxInstanceCount);
    raytracingInstances.reserve(maxInstanceCount);

    for(int i = 0; i < maxInstanceCount; i++) {
        std::vector<std::shared_ptr<Carrot::Mesh>> instanceMeshes;
        std::vector<glm::mat4> meshTransforms;
        std::vector<std::uint32_t> meshMaterialSlots;

        instanceMeshes.reserve(meshes.size());
        meshTransforms.reserve(meshes.size());
        meshMaterialSlots.reserve(meshes.size());

        forEachMesh([&](std::uint32_t meshIndex, std::uint32_t materialSlot, Carrot::Mesh::Ref& mesh) {
            std::int32_t vertexOffset = (static_cast<std::int32_t>(i * vertexCountPerInstance + meshOffsets[mesh->getMeshID()]));

            indirectCommands[mesh->getMeshID()].push_back(vk::DrawIndexedIndirectCommand {
                    .indexCount = static_cast<uint32_t>(mesh->getIndexCount()),
                    .instanceCount = 1,
                    .firstIndex = 0,
                    .vertexOffset = vertexOffset,
                    .firstInstance = static_cast<uint32_t>(i),
            });

            if(GetCapabilities().supportsRaytracing) {
                Carrot::BufferView vertexBuffer = fullySkinnedVertexBuffer.view.subView(static_cast<vk::DeviceSize>(vertexOffset * sizeof(Carrot::Vertex)), mesh->getVertexCount() * sizeof(Carrot::Vertex));
                instanceMeshes.push_back(std::make_shared<Carrot::LightMesh>(vertexBuffer, mesh->getIndexBuffer(), sizeof(Carrot::Vertex), sizeof(std::uint32_t)));
                meshTransforms.emplace_back(1.0f); // TODO: use actual mesh transform
                meshMaterialSlots.push_back(materialSlot);
            }
        });

        if(GetCapabilities().supportsRaytracing) {
            auto& asBuilder = GetRenderer().getASBuilder();
            auto blas = asBuilder.addBottomLevel(instanceMeshes, meshTransforms, meshMaterialSlots, BLASGeometryFormat::Default);
            blas->dynamicGeometry = true;
            raytracingBLASes.push_back(blas);
            raytracingInstances.push_back(asBuilder.addInstance(blas));
        }
    }

    forEachMesh([&](std::uint32_t meshIndex, std::uint32_t materialSlot, Carrot::Mesh::Ref& mesh) {
        indirectBuffers[mesh->getMeshID()]->name("Indirect commands for mesh #" + std::to_string(mesh->getMeshID()));
        indirectBuffers[mesh->getMeshID()]->stageUploadWithOffsets(std::make_pair(static_cast<std::uint64_t>(0),
                                                                             std::span(indirectCommands[mesh->getMeshID()])));
    });

    createSkinningComputePipeline();
}

void Carrot::AnimatedInstances::createSkinningComputePipeline() {
    skinningPipeline = engine.getRenderer().getOrCreatePipelineFullPath("resources/pipelines/compute/animation-skinning.pipeline", reinterpret_cast<u64>(this));

    vk::SemaphoreTypeCreateInfo timelineCreateInfo {
        .semaphoreType = vk::SemaphoreType::eTimeline,
        .initialValue = 0,
    };
    skinningSemaphore = engine.getLogicalDevice().createSemaphoreUnique(vk::SemaphoreCreateInfo {
        .pNext = &timelineCreateInfo
    });
    DebugNameable::nameSingle(Carrot::sprintf("Skinning semaphore %s", this->getModel().getOriginatingResource().getName().c_str()), *skinningSemaphore);
}

void Carrot::AnimatedInstances::onFrame(const Render::Context& renderContext) {
    ZoneScoped;

    if(GetCapabilities().supportsRaytracing) {
        verify(raytracingBLASes.size() == raytracingInstances.size(), "There must be as many BLASes as there are RT instances!");
        for(std::size_t instanceIndex = 0; instanceIndex < maxInstanceCount; instanceIndex ++) {
            auto& instance = raytracingInstances[instanceIndex];
            auto& blas = raytracingBLASes[instanceIndex];

            const AnimatedInstanceData& animatedInstanceData = getInstance(instanceIndex);
            instance->transform = animatedInstanceData.transform;

            const bool isEnabled = instanceIndex < currentInstanceCount && animatedInstanceData.raytraced;
            instance->enabled = isEnabled;
            if(isEnabled) {
                blas->setDirty();
            }
        }
    }

    const std::uint32_t vertexGroups = (vertexCountPerInstance + 127) / 128;
    const std::uint32_t instanceGroups = (maxInstanceCount + 7)/8;

    // submit skinning command buffer
    // start skinning as soon as possible, even if that means we will have a frame of delay (render before update)
    Carrot::Render::Packet& packet = renderContext.renderer.makeAsyncPacket();
    auto& pushConstant = packet.addPushConstant("push", vk::ShaderStageFlagBits::eCompute);
    struct PushConstantData {
        u32 vertexCount;
        u32 instanceCount;
    } push;
    push.vertexCount = vertexCountPerInstance;
    push.instanceCount = maxInstanceCount;
    pushConstant.setData(push);
    packet.pipeline = skinningPipeline;
    auto& command = packet.commands.emplace_back();
    command.compute.x = vertexGroups;
    command.compute.y = instanceGroups;
    command.compute.z = 1;

    // Set 0
    renderContext.renderer.bindBuffer(*skinningPipeline, renderContext, flatVertices->getWholeView(), 0, 0);
    renderContext.renderer.bindBuffer(*skinningPipeline, renderContext, instanceBuffer->getWholeView(), 0, 1);
    renderContext.renderer.bindBuffer(*skinningPipeline, renderContext, fullySkinnedVertexBuffer.view, 0, 2);

    // Set 1
    renderContext.renderer.bindBuffer(*skinningPipeline, renderContext, model->getAnimationDataBuffer().getWholeView(), 1, 0);
    for (const auto& [_, binding] : model->getAnimationMetadata()) {
        renderContext.renderer.bindStorageImage(*skinningPipeline, renderContext, model->getAnimationDataTexture(binding.index), 1, 1,
            vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, binding.index /*array index*/, vk::ImageLayout::eGeneral);
    }

    if (renderContext.frameNumber != 0) {
        vk::SemaphoreSubmitInfo waitRender {
            .semaphore = engine.getRenderFinishedTimelineSemaphore(),
            .value = renderContext.getPreviousFrameNumber(),
            .stageMask = vk::PipelineStageFlagBits2::eComputeShader,
        };
        packet.waitSemaphores.emplaceBack(waitRender);
    }

    vk::SemaphoreSubmitInfo signalInfo {
        .semaphore = *skinningSemaphore,
        .value = renderContext.frameNumber,
        .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
    };
    packet.signalSemaphores.emplaceBack(signalInfo);

    renderContext.render(packet); // submits because this is an async packet

    // do not bind the semaphore earlier, it is possible the creation of the AnimatedInstances finishes and ASBuilder kicks off, without the skinning semaphore ever signaled
    if (!submitAtLeastOneSkinningCompute)
    {
        submitAtLeastOneSkinningCompute = true;
        Render::PerFrame<vk::Semaphore> semaphores;

        // TODO: don't use perframe?
        for(i32 index = 0; index < semaphores.size(); index++) {
            semaphores[index] = *skinningSemaphore;
        }

        for(auto& blas : raytracingBLASes) {
            blas->bindSemaphores(semaphores);
        }
    }

    bool hasRaytracedInstances = false;
    if (GetCapabilities().supportsRaytracing) {
        for(std::size_t instanceIndex = 0; instanceIndex < maxInstanceCount; instanceIndex ++) {
            if (getInstance(instanceIndex).raytraced) {
                hasRaytracedInstances = true;
                break;
            }
        }
    }

    if (!hasRaytracedInstances) { // otherwise, the raytracing code will wait on the semaphore
        GetEngine().addWaitSemaphoreBeforeRendering(renderContext, vk::PipelineStageFlagBits::eVertexInput, *skinningSemaphore, renderContext.frameNumber);
    }
}

void Carrot::AnimatedInstances::render(const Carrot::Render::Context& renderContext, Carrot::Render::PassName renderPass) {
    render(renderContext, renderPass, maxInstanceCount);
}

void Carrot::AnimatedInstances::render(const Carrot::Render::Context& renderContext, Carrot::Render::PassName renderPass, std::size_t instanceCount) {
    verify(instanceCount <= maxInstanceCount, "instanceCount > maxInstanceCount !");
    currentInstanceCount = instanceCount;
    onFrame(renderContext);

    Carrot::GBufferDrawData data;

    bool inTransparentPass = renderPass == Render::PassEnum::TransparentGBuffer;
    Render::Packet& packet = GetRenderer().makeRenderPacket(renderPass, Render::PacketType::DrawIndexedInstanced, renderContext);
    packet.pipeline = inTransparentPass ? model->transparentMeshesPipeline : model->opaqueMeshesPipeline;
    packet.transparentGBuffer.zOrder = 0.0f;

    packet.instanceCount = 1;
    packet.commands.resize(1);

    Carrot::BufferView skinnedVertices = fullySkinnedVertexBuffer.view;
    for (const auto&[mat, meshList]: model->skinnedMeshes) {
        data.materialIndex = mat;
        packet.clearPerDrawData();
        data.materialIndex = mat;
        packet.addPerDrawData(std::span(&data, 1));

        for (const auto& meshInfo: meshList) {
            auto& mesh = meshInfo.mesh;
            auto& meshTransform = meshInfo.transform;
            auto& sphere = meshInfo.boundingSphere;
            for(size_t index = 0; index < currentInstanceCount; index++) {
                Carrot::AnimatedInstanceData meshInstanceData = getInstance(index);
                meshInstanceData.transform = meshInstanceData.transform;

                // TODO: bounding sphere should follow animation
                Math::Sphere s = sphere;
                s.transform(meshInstanceData.transform * meshTransform);
                if(!renderContext.getCamera().isInFrustum(s)) {
                    continue;
                }

                if(DrawBoundingSpheres) {
                    if(model.get() != renderContext.renderer.getUnitSphere().get()) {
                        glm::mat4 sphereTransform = glm::translate(glm::mat4{1.0f}, s.center) * glm::scale(glm::mat4{1.0f}, glm::vec3{s.radius*2 /*unit sphere model has a radius of 0.5*/});
                        renderContext.renderer.renderWireframeSphere(renderContext, sphereTransform, 1.0f, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), meshInstanceData.uuid);
                    }
                }

                packet.useInstance(meshInstanceData);

                std::int32_t vertexOffset = (static_cast<std::int32_t>(index * vertexCountPerInstance + meshOffsets[mesh->getMeshID()]));

                packet.vertexBuffer = skinnedVertices.subView(sizeof(Carrot::Vertex) * vertexOffset, sizeof(Carrot::Vertex) * mesh->getVertexCount());
                packet.indexBuffer = mesh->getIndexBuffer();
                auto& cmd = packet.commands[0].drawIndexedInstanced;
                cmd.instanceCount = 1;
                cmd.indexCount = mesh->getIndexCount();

                renderContext.renderer.render(packet);
            }
        }
    }
}

void Carrot::AnimatedInstances::forEachMesh(const std::function<void(std::uint32_t meshIndex, std::uint32_t materialSlot, Mesh::Ref& mesh)>& action) {
    std::size_t meshIndex = 0;
    for(auto& [materialSlot, meshList] : model->getSkinnedMeshes()) {
        for(auto& mesh : meshList) {
            action(meshIndex, materialSlot, mesh);
            meshIndex++;
        }
    }
}
