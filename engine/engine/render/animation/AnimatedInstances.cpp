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

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        fullySkinnedVertexBuffers[i] = GetResourceAllocator().allocateDeviceBuffer(sizeof(Vertex) * vertexCountPerInstance * maxInstanceCount, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR);
        fullySkinnedVertexBuffers[i].name(Carrot::sprintf("full skinned vertices %s", model->debugName.c_str()));
    }

    raytracingBLASes.reserve(maxInstanceCount * MAX_FRAMES_IN_FLIGHT);
    raytracingInstances.reserve(maxInstanceCount * MAX_FRAMES_IN_FLIGHT);

    for(int i = 0; i < maxInstanceCount; i++) {
        std::array<std::vector<std::shared_ptr<Carrot::Mesh>>, MAX_FRAMES_IN_FLIGHT> instanceMeshes;
        std::array<std::vector<glm::mat4>, MAX_FRAMES_IN_FLIGHT> meshTransforms;
        std::array<std::vector<std::uint32_t>, MAX_FRAMES_IN_FLIGHT> meshMaterialSlots;

        for (i32 bufferingIndex = 0; bufferingIndex < MAX_FRAMES_IN_FLIGHT; bufferingIndex++) {
            instanceMeshes[bufferingIndex].reserve(meshes.size());
            meshTransforms[bufferingIndex].reserve(meshes.size());
            meshMaterialSlots[bufferingIndex].reserve(meshes.size());
        }

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
                for (i32 bufferingIndex = 0; bufferingIndex < MAX_FRAMES_IN_FLIGHT; bufferingIndex++) {
                    Carrot::BufferView vertexBuffer = fullySkinnedVertexBuffers[bufferingIndex].view.subView(static_cast<vk::DeviceSize>(vertexOffset * sizeof(Carrot::Vertex)), mesh->getVertexCount() * sizeof(Carrot::Vertex));
                    instanceMeshes[bufferingIndex].push_back(std::make_shared<Carrot::LightMesh>(vertexBuffer, mesh->getIndexBuffer(), sizeof(Carrot::Vertex), sizeof(std::uint32_t)));
                    meshTransforms[bufferingIndex].push_back(glm::mat4(1.0f)); // TODO: use actual mesh transform
                    meshMaterialSlots[bufferingIndex].push_back(materialSlot);
                }
            }
        });

        if(GetCapabilities().supportsRaytracing) {
            for (i32 bufferingIndex = 0; bufferingIndex < MAX_FRAMES_IN_FLIGHT; bufferingIndex++) {
                auto& asBuilder = GetRenderer().getASBuilder();
                auto blas = asBuilder.addBottomLevel(instanceMeshes[bufferingIndex], meshTransforms[bufferingIndex], meshMaterialSlots[bufferingIndex], BLASGeometryFormat::Default);
                blas->dynamicGeometry = true;
                raytracingBLASes.push_back(blas);
                raytracingInstances.push_back(asBuilder.addInstance(blas));
            }
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
    auto& computeCommandPool = engine.getComputeCommandPool();

    // command buffers which will be sent to the compute queue to compute skinning
    skinningCommandBuffers = engine.getLogicalDevice().allocateCommandBuffers(vk::CommandBufferAllocateInfo {
            .commandPool = computeCommandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
    });

    // sets the mesh count (per instance) for this skinning pipeline
    vk::SpecializationMapEntry specEntries[] = {
            {
                    .constantID = 0,
                    .offset = 0,
                    .size = sizeof(uint32_t),
            },

            {
                    .constantID = 1,
                    .offset = 1*sizeof(uint32_t),
                    .size = sizeof(uint32_t),
            },
    };

    std::uint32_t specData[] = {
            static_cast<uint32_t>(vertexCountPerInstance),
            static_cast<uint32_t>(maxInstanceCount),
    };
    vk::SpecializationInfo specialization {
            .mapEntryCount = 2,
            .pMapEntries = specEntries,

            .dataSize = 2*sizeof(uint32_t),
            .pData = specData,
    };

    // Describe descriptors used by compute shader

    constexpr std::size_t set0Size = 3;
    constexpr std::size_t set1Size = 2;
    vk::DescriptorSetLayoutBinding bindings[set0Size] = {
            // original vertices
            {
                    .binding = 0,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .descriptorCount = MAX_FRAMES_IN_FLIGHT,
                    .stageFlags = vk::ShaderStageFlagBits::eCompute,
            },

            // instance info
            {
                    .binding = 1,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .descriptorCount = MAX_FRAMES_IN_FLIGHT,
                    .stageFlags = vk::ShaderStageFlagBits::eCompute,
            },

            // output buffer
            {
                    .binding = 2,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .descriptorCount = MAX_FRAMES_IN_FLIGHT,
                    .stageFlags = vk::ShaderStageFlagBits::eCompute,
            },
    };

    // TODO: share with Model.cpp
    std::array animationBindings = {
            vk::DescriptorSetLayoutBinding {
                    .binding = 0,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .descriptorCount = 1,
                    .stageFlags = vk::ShaderStageFlagBits::eCompute,
            },
            vk::DescriptorSetLayoutBinding {
                    .binding = 1,
                    .descriptorType = vk::DescriptorType::eStorageImage,
                    .descriptorCount = static_cast<uint32_t>(model->getAnimationMetadata().size()),
                    .stageFlags = vk::ShaderStageFlagBits::eCompute,
            }
    };

    computeSetLayout0 = engine.getLogicalDevice().createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo {
            .bindingCount = set0Size,
            .pBindings = bindings,
    });

    computeSetLayout1 = engine.getLogicalDevice().createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo {
            .bindingCount = animationBindings.size(),
            .pBindings = animationBindings.data(),
    });

    std::vector<vk::DescriptorPoolSize> poolSizes{};
    // set0
    poolSizes.push_back(vk::DescriptorPoolSize {
            .type = vk::DescriptorType::eStorageBuffer,
            .descriptorCount = static_cast<uint32_t>(set0Size * MAX_FRAMES_IN_FLIGHT),
    });

    for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        auto pool = engine.getLogicalDevice().createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo {
                .maxSets = 2*MAX_FRAMES_IN_FLIGHT,
                .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
                .pPoolSizes = poolSizes.data(),
        });

        computeDescriptorSet0.push_back(engine.getLogicalDevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo {
                .descriptorPool = *pool,
                .descriptorSetCount = 1,
                .pSetLayouts = &(*computeSetLayout0)
        })[0]);

        computeDescriptorPools.emplace_back(std::move(pool));
    }

    // write to descriptor sets
    vk::DescriptorBufferInfo originalVertexBuffer {
            .buffer = flatVertices->getVulkanBuffer(),
            .offset = 0,
            .range = flatVertices->getSize(),
    };

    vk::DescriptorBufferInfo instanceBufferInfo {
            .buffer = instanceBuffer->getVulkanBuffer(),
            .offset = 0,
            .range = instanceBuffer->getSize(),
    };

    // TODO: fix validation error with arrays
    for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vk::DescriptorBufferInfo outputBufferInfo = fullySkinnedVertexBuffers[i].view.asBufferInfo();

        using DT = vk::DescriptorType;
        std::vector<vk::WriteDescriptorSet> writes = {
                // set0, binding0, original vertex buffer
                vk::WriteDescriptorSet {
                        .dstSet = computeDescriptorSet0[i],
                        .dstBinding = 0,
                        .descriptorCount = 1,
                        .descriptorType = DT::eStorageBuffer,
                        .pBufferInfo = &originalVertexBuffer,
                },

                // set0, binding1, instance buffer
                vk::WriteDescriptorSet {
                        .dstSet = computeDescriptorSet0[i],
                        .dstBinding = 1,
                        .descriptorCount = 1,
                        .descriptorType = DT::eStorageBuffer,
                        .pBufferInfo = &instanceBufferInfo,
                },

                // set0, binding2, output buffer
                vk::WriteDescriptorSet {
                        .dstSet = computeDescriptorSet0[i],
                        .dstBinding = 2,
                        .descriptorCount = 1,
                        .descriptorType = DT::eStorageBuffer,
                        .pBufferInfo = &outputBufferInfo,
                },
        };

        assert(writes.size() == set0Size);

        engine.getLogicalDevice().updateDescriptorSets(writes, {});
    }

    auto computeStage = ShaderModule("resources/shaders/compute/animation-skinning.compute.glsl.spv", "main");

    // create the pipeline
    vk::DescriptorSetLayout setLayouts[] = {
            *computeSetLayout0,
            *computeSetLayout1,
    };
    computePipelineLayout = engine.getLogicalDevice().createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo {
            .setLayoutCount = 2,
            .pSetLayouts = setLayouts,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr,
    }, engine.getAllocator());
    computePipeline = engine.getLogicalDevice().createComputePipelineUnique(nullptr, vk::ComputePipelineCreateInfo {
            .stage = computeStage.createPipelineShaderStage(vk::ShaderStageFlagBits::eCompute, &specialization),
            .layout = *computePipelineLayout,
    }, engine.getAllocator()).value;

    std::uint32_t vertexGroups = (vertexCountPerInstance + 127) / 128;
    std::uint32_t instanceGroups = (maxInstanceCount + 7)/8;
    for(std::size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vk::CommandBuffer& commands = skinningCommandBuffers[i];
        commands.begin(vk::CommandBufferBeginInfo {
        });
        {
            // TODO: tracy zone
            commands.bindPipeline(vk::PipelineBindPoint::eCompute, *computePipeline);
            commands.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *computePipelineLayout, 0, {computeDescriptorSet0[i], model->getAnimationDataDescriptorSet()}, {});
            commands.dispatch(vertexGroups, instanceGroups, 1);
        }
        commands.end();

        skinningSemaphores.emplace_back(std::move(engine.getLogicalDevice().createSemaphoreUnique({})));
        DebugNameable::nameSingle(Carrot::sprintf("Skinning semaphore %s", this->getModel().getOriginatingResource().getName().c_str()), *skinningSemaphores[i]);
    }
}

vk::Semaphore& Carrot::AnimatedInstances::onFrame(const Render::Context& renderContext) {
    ZoneScoped;

    const std::size_t modFrameIndex = renderContext.frameNumber % MAX_FRAMES_IN_FLIGHT;
    if(GetCapabilities().supportsRaytracing) {
        verify(raytracingBLASes.size() == raytracingInstances.size(), "There must be as many BLASes as there are RT instances!");
        for(std::size_t instanceIndex = 0; instanceIndex < maxInstanceCount; instanceIndex ++) {
            for (i32 bufferingIndex = 0; bufferingIndex < MAX_FRAMES_IN_FLIGHT; bufferingIndex++) {
                const i32 globalIndex = instanceIndex * MAX_FRAMES_IN_FLIGHT + bufferingIndex;
                auto& instance = raytracingInstances[globalIndex];
                if (bufferingIndex == modFrameIndex % MAX_FRAMES_IN_FLIGHT) {
                    auto& blas = raytracingBLASes[globalIndex];

                    const AnimatedInstanceData& animatedInstanceData = getInstance(instanceIndex);
                    instance->transform = animatedInstanceData.transform;

                    const bool isEnabled = instanceIndex < currentInstanceCount && animatedInstanceData.raytraced;
                    instance->enabled = isEnabled;
                    if(isEnabled) {
                        blas->setDirty();
                    }
                } else {
                    instance->enabled = false;
                }
            }
        }
    }

    // submit skinning command buffer
    // start skinning as soon as possible, even if that means we will have a frame of delay (render before update)
    vk::CommandBufferSubmitInfo commandBufferInfo {
        .commandBuffer = skinningCommandBuffers[modFrameIndex],
    };
    vk::SemaphoreSubmitInfo signalInfo {
        .semaphore = *skinningSemaphores[modFrameIndex],
        .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
    };

    // do not bind the semaphore earlier, it is possible the creation of the AnimatedInstances finishes and ASBuilder kicks off, without the skinning semaphore ever signaled
    if (!submitAtLeastOneSkinningCompute)
    {
        submitAtLeastOneSkinningCompute = true;
        Render::PerFrame<vk::Semaphore> semaphores;
        semaphores.reserve(skinningSemaphores.size());
        for(auto& pSemaphore : skinningSemaphores) {
            semaphores.emplace_back(*pSemaphore);
        }

        for(auto& blas : raytracingBLASes) {
            blas->bindSemaphores(semaphores);
        }
    }

    GetVulkanDriver().submitCompute(vk::SubmitInfo2 {
            .waitSemaphoreInfoCount = 0,
            .pWaitSemaphoreInfos = nullptr,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &commandBufferInfo,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &signalInfo,
    });

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
        GetEngine().addWaitSemaphoreBeforeRendering(renderContext, vk::PipelineStageFlagBits::eVertexInput, *skinningSemaphores[modFrameIndex]);
    }
    return *skinningSemaphores[modFrameIndex];
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

    Carrot::BufferView skinnedVertices = fullySkinnedVertexBuffers[renderContext.frameNumber % MAX_FRAMES_IN_FLIGHT].view;
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
