//
// Created by jglrxavpok on 16/04/2021.
//

#include "AnimatedInstances.h"

#include <utility>
#include "engine/render/resources/Buffer.h"
#include "engine/render/Model.h"
#include "engine/render/resources/Mesh.h"
#include "engine/render/DrawData.h"
#include "engine/render/raytracing/ASBuilder.h"
#include "engine/render/resources/LightMesh.h"

Carrot::AnimatedInstances::AnimatedInstances(Carrot::Engine& engine, std::shared_ptr<Model> animatedModel, size_t maxInstanceCount):
    engine(engine), model(std::move(animatedModel)), maxInstanceCount(maxInstanceCount) {

    instanceBuffer = std::make_unique<Buffer>(engine.getVulkanDriver(),
                                              maxInstanceCount*sizeof(AnimatedInstanceData),
                                              vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer,
                                              vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
    animatedInstances = instanceBuffer->map<AnimatedInstanceData>();
    std::fill_n(animatedInstances, maxInstanceCount, AnimatedInstanceData{});

    std::map<MeshID, std::vector<vk::DrawIndexedIndirectCommand>> indirectCommands{};
    auto meshes = model->getSkinnedMeshes();
    uint64_t maxVertexCount = 0;

    for(const auto& mesh : meshes) {
        indirectBuffers[mesh->getMeshID()] = std::make_shared<Buffer>(engine.getVulkanDriver(),
                                                                      maxInstanceCount * sizeof(vk::DrawIndexedIndirectCommand),
                                                                      vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                                                      vk::MemoryPropertyFlagBits::eDeviceLocal);
        size_t meshSize = mesh->getVertexCount();
        maxVertexCount = std::max(meshSize, maxVertexCount);

        meshOffsets[mesh->getMeshID()] = vertexCountPerInstance;
        vertexCountPerInstance += meshSize;
    }

    // TODO: swapchainlength-buffering?
    flatVertices = std::make_unique<Buffer>(engine.getVulkanDriver(),
                                            sizeof(SkinnedVertex) * vertexCountPerInstance,
                                            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                            vk::MemoryPropertyFlagBits::eDeviceLocal,
                                            engine.createGraphicsAndTransferFamiliesSet());
    flatVertices->name("flat vertices");

    // copy mesh vertices into a flat buffer to allow for easy indexing inside the compute shader
    for(const auto& mesh : meshes) {
        int32_t vertexOffset = static_cast<int32_t>(meshOffsets[mesh->getMeshID()]);

        engine.performSingleTimeTransferCommands([&](vk::CommandBuffer& commands) {
            vk::BufferCopy region {
                    .srcOffset = mesh->getVertexBuffer().getStart(),
                    .dstOffset = vertexOffset*sizeof(SkinnedVertex),
                    .size = mesh->getVertexCount()*sizeof(SkinnedVertex),
            };
            commands.copyBuffer(mesh->getVertexBuffer().getVulkanBuffer(), flatVertices->getVulkanBuffer(), region);
        });
    }

    fullySkinnedUnitVertices = std::make_unique<Buffer>(engine.getVulkanDriver(),
                                                        sizeof(Vertex) * vertexCountPerInstance * maxInstanceCount,
                                                        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
                                                        vk::MemoryPropertyFlagBits::eDeviceLocal,
                                                        engine.createGraphicsAndTransferFamiliesSet());
    fullySkinnedUnitVertices->name("full skinned unit vertices");

    raytracingBLASes.reserve(maxInstanceCount);
    raytracingInstances.reserve(maxInstanceCount);

    for(int i = 0; i < maxInstanceCount; i++) {

        std::vector<std::shared_ptr<Carrot::Mesh>> instanceMeshes;
        instanceMeshes.reserve(meshes.size());

        for(const auto& mesh : meshes) {
            std::int32_t vertexOffset = (static_cast<std::int32_t>(i * vertexCountPerInstance + meshOffsets[mesh->getMeshID()]));

            indirectCommands[mesh->getMeshID()].push_back(vk::DrawIndexedIndirectCommand {
                    .indexCount = static_cast<uint32_t>(mesh->getIndexCount()),
                    .instanceCount = 1,
                    .firstIndex = 0,
                    .vertexOffset = vertexOffset,
                    .firstInstance = static_cast<uint32_t>(i),
            });

            if(GetCapabilities().supportsRaytracing) {
                Carrot::BufferView vertexBuffer{nullptr, *fullySkinnedUnitVertices, static_cast<vk::DeviceSize>(vertexOffset * sizeof(Carrot::Vertex)), mesh->getVertexCount() * sizeof(Carrot::Vertex) };
                instanceMeshes.push_back(std::make_shared<Carrot::LightMesh>(vertexBuffer, mesh->getIndexBuffer(), sizeof(Carrot::Vertex)));
            }
        }

        if(GetCapabilities().supportsRaytracing) {
            auto& asBuilder = GetRenderer().getASBuilder();
            auto blas = asBuilder.addBottomLevel(instanceMeshes);
            raytracingBLASes.push_back(blas);
            raytracingInstances.push_back(asBuilder.addInstance(blas));
        }
    }

    for(const auto& mesh : meshes) {
        indirectBuffers[mesh->getMeshID()]->name("Indirect commands for mesh #" + std::to_string(mesh->getMeshID()));
        indirectBuffers[mesh->getMeshID()]->stageUploadWithOffsets(std::make_pair(static_cast<std::uint64_t>(0),
                                                                             indirectCommands[mesh->getMeshID()]));
    }

    createSkinningComputePipeline();
}

void Carrot::AnimatedInstances::createSkinningComputePipeline() {
    auto& computeCommandPool = engine.getComputeCommandPool();

    // command buffers which will be sent to the compute queue to compute skinning
    skinningCommandBuffers = engine.getLogicalDevice().allocateCommandBuffers(vk::CommandBufferAllocateInfo {
            .commandPool = computeCommandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = engine.getSwapchainImageCount(),
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
    constexpr std::size_t set1Size = 1;
    vk::DescriptorSetLayoutBinding bindings[set0Size] = {
            // original vertices
            {
                    .binding = 0,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .descriptorCount = engine.getSwapchainImageCount(),
                    .stageFlags = vk::ShaderStageFlagBits::eCompute,
            },

            // instance info
            {
                    .binding = 1,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .descriptorCount = engine.getSwapchainImageCount(),
                    .stageFlags = vk::ShaderStageFlagBits::eCompute,
            },

            // output buffer
            {
                    .binding = 2,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .descriptorCount = engine.getSwapchainImageCount(),
                    .stageFlags = vk::ShaderStageFlagBits::eCompute,
            },
    };

    vk::DescriptorSetLayoutBinding animationBinding {
            .binding = 0,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .descriptorCount = engine.getSwapchainImageCount(),
            .stageFlags = vk::ShaderStageFlagBits::eCompute,
    };

    computeSetLayout0 = engine.getLogicalDevice().createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo {
            .bindingCount = set0Size,
            .pBindings = bindings,
    });

    computeSetLayout1 = engine.getLogicalDevice().createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo {
            .bindingCount = set1Size,
            .pBindings = &animationBinding,
    });

    std::vector<vk::DescriptorPoolSize> poolSizes{};
    // set0
    poolSizes.push_back(vk::DescriptorPoolSize {
            .type = vk::DescriptorType::eStorageBuffer,
            .descriptorCount = static_cast<uint32_t>(set0Size * engine.getSwapchainImageCount()),
    });
    // set1
    poolSizes.push_back(vk::DescriptorPoolSize {
            .type = vk::DescriptorType::eStorageBuffer,
            .descriptorCount = engine.getSwapchainImageCount(),
    });

    for(size_t i = 0; i < engine.getSwapchainImageCount(); i++) {
        auto pool = engine.getLogicalDevice().createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo {
                .maxSets = 2*engine.getSwapchainImageCount(),
                .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
                .pPoolSizes = poolSizes.data(),
        });

        computeDescriptorSet0.push_back(engine.getLogicalDevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo {
                .descriptorPool = *pool,
                .descriptorSetCount = 1,
                .pSetLayouts = &(*computeSetLayout0)
        })[0]);

        computeDescriptorSet1.push_back(engine.getLogicalDevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo {
                .descriptorPool = *pool,
                .descriptorSetCount = 1,
                .pSetLayouts = &(*computeSetLayout1)
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

    vk::DescriptorBufferInfo outputBufferInfo {
            .buffer = fullySkinnedUnitVertices->getVulkanBuffer(),
            .offset = 0,
            .range = fullySkinnedUnitVertices->getSize(),
    };

    vk::DescriptorBufferInfo animationBufferInfo {
            .buffer = model->getAnimationDataBuffer().getVulkanBuffer(),
            .offset = 0,
            .range = model->getAnimationDataBuffer().getSize(),
    };

    // TODO: fix validation error with arrays
    for(size_t i = 0; i < engine.getSwapchainImageCount(); i++) {
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

                // set1, binding0, animation buffer
                vk::WriteDescriptorSet {
                        .dstSet = computeDescriptorSet1[i],
                        .dstBinding = 0,
                        .descriptorCount = 1,
                        .descriptorType = DT::eStorageBuffer,
                        .pBufferInfo = &animationBufferInfo,
                },
        };

        assert(writes.size() == set0Size+set1Size);

        engine.getLogicalDevice().updateDescriptorSets(writes, {});
    }

    auto computeStage = ShaderModule(engine.getVulkanDriver(), "resources/shaders/compute/skinning.compute.glsl.spv");

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
    }, engine.getAllocator());

    std::uint32_t vertexGroups = (vertexCountPerInstance + 127) / 128;
    std::uint32_t instanceGroups = (maxInstanceCount + 7)/8;
    for(std::size_t i = 0; i < engine.getSwapchainImageCount(); i++) {
        vk::CommandBuffer& commands = skinningCommandBuffers[i];
        commands.begin(vk::CommandBufferBeginInfo {
        });
        {
            // TODO: tracy zone
            commands.bindPipeline(vk::PipelineBindPoint::eCompute, *computePipeline);
            commands.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *computePipelineLayout, 0, {computeDescriptorSet0[i], computeDescriptorSet1[i]}, {});
            commands.dispatch(vertexGroups, instanceGroups, 1);
        }
        commands.end();

        skinningSemaphores.emplace_back(std::move(engine.getLogicalDevice().createSemaphoreUnique({})));
    }
}

vk::Semaphore& Carrot::AnimatedInstances::onFrame(std::size_t frameIndex) {
    ZoneScoped;

    if(GetCapabilities().supportsRaytracing) {
        for(auto& blas : raytracingBLASes) {
            blas->setDirty();
        }

        for(std::size_t i = 0; i < raytracingInstances.size(); i++) {
            auto& instance = raytracingInstances[i];
            instance->transform = getInstance(i).transform;
        }
    }

    // submit skinning command buffer
    // start skinning as soon as possible, even if that means we will have a frame of delay (render before update)
    GetVulkanDriver().submitCompute(vk::SubmitInfo {
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = nullptr,
            .commandBufferCount = 1,
            .pCommandBuffers = &skinningCommandBuffers[frameIndex],
            .signalSemaphoreCount = 0,
            //.pSignalSemaphores = &(*skinningSemaphores[frameIndex]),
    });
    GetVulkanDriver().getComputeQueue().waitIdle();
    return *skinningSemaphores[frameIndex];
}

void Carrot::AnimatedInstances::recordGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands, std::size_t indirectDrawCount) {
    TracyVulkanZone(*engine.tracyCtx[renderContext.swapchainIndex], commands, "Render units");
    commands.bindVertexBuffers(0, fullySkinnedUnitVertices->getVulkanBuffer(), {0});
    model->indirectDraw(pass, renderContext, commands, *instanceBuffer, indirectBuffers, indirectDrawCount);
}

void Carrot::AnimatedInstances::render(const Carrot::Render::Context& renderContext, Carrot::Render::PassEnum renderPass) {
    render(renderContext, renderPass, maxInstanceCount);
}

void Carrot::AnimatedInstances::render(const Carrot::Render::Context& renderContext, Carrot::Render::PassEnum renderPass, std::size_t instanceCount) {
    verify(instanceCount <= maxInstanceCount, "instanceCount > maxInstanceCount !");
    onFrame(renderContext.swapchainIndex); // TODO: don't yolo it, wait for semaphore

    Carrot::DrawData data;

    Render::Packet& packet = GetRenderer().makeRenderPacket(renderPass, renderContext.viewport);
    packet.pipeline = model->skinnedMeshesPipeline;
    packet.transparentGBuffer.zOrder = 0.0f;

    Render::Packet::PushConstant& pushConstant = packet.addPushConstant();
    pushConstant.id = "drawDataPush";
    pushConstant.stages = vk::ShaderStageFlagBits::eFragment;

    packet.instanceCount = 1;

    for (const auto&[mat, meshList]: model->skinnedMeshes) {
        data.materialIndex = mat;
        pushConstant.setData(data); // template operator=

        for (const auto& mesh: meshList) {
            for(size_t index = 0; index < maxInstanceCount; index++) {
                packet.useInstance(getInstance(index));

                std::int32_t vertexOffset = (static_cast<std::int32_t>(index * vertexCountPerInstance + meshOffsets[mesh->getMeshID()]));

                packet.vertexBuffer = Carrot::BufferView(nullptr, *fullySkinnedUnitVertices, sizeof(Carrot::Vertex) * vertexOffset, sizeof(Carrot::Vertex) * mesh->getVertexCount());
                packet.indexBuffer = mesh->getIndexBuffer();
                packet.indexCount = mesh->getIndexCount();

                renderContext.renderer.render(packet);
            }
        }
    }
}
