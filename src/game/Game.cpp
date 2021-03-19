//
// Created by jglrxavpok on 05/12/2020.
//

#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <memory>
#include "Game.h"
#include "engine/Engine.h"
#include "engine/render/Model.h"
#include "engine/render/resources/Buffer.h"
#include "engine/render/Camera.h"
#include "engine/render/resources/Mesh.h"
#include <iostream>
#include <engine/render/shaders/ShaderModule.h>
#include <engine/render/raytracing/ASBuilder.h>
#include <engine/render/raytracing/RayTracer.h>

#include <engine/ecs/components/Component.h>
#include <engine/ecs/components/Transform.h>
#include <engine/ecs/components/AnimatedModelInstance.h>
#include <engine/ecs/components/ForceSinPosition.h>
#include <engine/ecs/components/RaycastedShadowsLight.h>
#include <engine/ecs/systems/SystemUpdateAnimatedModelInstance.h>
#include <engine/ecs/systems/SystemSinPosition.h>
#include <engine/ecs/systems/SystemUpdateLightPosition.h>
#include "UnitColor.h"

constexpr static int maxInstanceCount = 100; // TODO: change

static vector<size_t> blasIndices{};

Game::Game::Game(Carrot::Engine& engine): engine(engine) {
    world.addRenderSystem<SystemUpdateAnimatedModelInstance>();
    world.addLogicSystem<SystemSinPosition>();
    world.addLogicSystem<SystemUpdateLightPosition>();

    for (int i = 0; i < maxInstanceCount * 3; ++i) {
        blasIndices.push_back(i);
    }

    mapModel = make_unique<Model>(engine, "resources/models/map/map.obj");
    model = make_unique<Model>(engine, "resources/models/unit.fbx");

    int groupSize = maxInstanceCount /3;
    instanceBuffer = make_unique<Buffer>(engine.getVulkanDriver(),
                                         maxInstanceCount*sizeof(AnimatedInstanceData),
                                         vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer,
                                         vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
    modelInstance = instanceBuffer->map<AnimatedInstanceData>();

    mapInstanceBuffer = make_unique<Buffer>(engine.getVulkanDriver(),
                                            sizeof(InstanceData),
                                            vk::BufferUsageFlagBits::eVertexBuffer,
                                            vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
    auto* mapData = mapInstanceBuffer->map<InstanceData>();
    mapData[0] = {
            {1,1,1,1},
            glm::mat4(1.0f)
    };

    // TODO: abstract
    map<MeshID, vector<vk::DrawIndexedIndirectCommand>> indirectCommands{};
    auto meshes = model->getMeshes();
    uint64_t maxVertexCount = 0;
    size_t vertexCountPerInstance = 0;
    map<MeshID, size_t> meshOffsets{};

    for(const auto& mesh : meshes) {
        indirectBuffers[mesh->getMeshID()] = make_shared<Buffer>(engine.getVulkanDriver(),
                                             maxInstanceCount * sizeof(vk::DrawIndexedIndirectCommand),
                                             vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                             vk::MemoryPropertyFlagBits::eDeviceLocal);
        size_t meshSize = mesh->getVertexCount();
        maxVertexCount = max(meshSize, maxVertexCount);

        meshOffsets[mesh->getMeshID()] = vertexCountPerInstance;
        vertexCountPerInstance += meshSize;
    }

    flatVertices = make_unique<Buffer>(engine.getVulkanDriver(),
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
                    .srcOffset = mesh->getVertexStartOffset(),
                    .dstOffset = vertexOffset*sizeof(SkinnedVertex),
                    .size = mesh->getVertexCount()*sizeof(SkinnedVertex),
            };
            commands.copyBuffer(mesh->getBackingBuffer().getVulkanBuffer(), flatVertices->getVulkanBuffer(), region);
        });
    }

    fullySkinnedUnitVertices = make_unique<Buffer>(engine.getVulkanDriver(),
                                                   sizeof(Vertex) * vertexCountPerInstance * maxInstanceCount,
                                                   vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                                   vk::MemoryPropertyFlagBits::eDeviceLocal,
                                                   engine.createGraphicsAndTransferFamiliesSet());
    fullySkinnedUnitVertices->name("full skinned unit vertices");

    auto& as = engine.getASBuilder();
    const float spacing = 0.5f;

    vector<GeometryInput*> geometries{};
    for(int i = 0; i < maxInstanceCount; i++) {
        float x = (i % (int)sqrt(maxInstanceCount)) * spacing;
        float y = (i / (int)sqrt(maxInstanceCount)) * spacing;
        auto position = glm::vec3(x, y, 0);
        auto color = static_cast<Unit::Type>((i / max(1, groupSize)) % 3);

        auto entity = world.newEntity()
                .addComponent<Transform>()
                .addComponent<UnitColor>(color)
                .addComponent<AnimatedModelInstance>(modelInstance[i]);
        if(auto transform = entity.getComponent<Transform>()) {
            transform->position = position;
        }
        switch (color) {
            case Unit::Type::Blue:
                modelInstance[i].color = {0,0,1,1};
                break;
            case Unit::Type::Red:
                modelInstance[i].color = {1,0,0,1};
                break;
            case Unit::Type::Green:
                modelInstance[i].color = {0,1,0,1};
                break;
        }

        for(const auto& mesh : meshes) {
            int32_t vertexOffset = (static_cast<int32_t>(i * vertexCountPerInstance + meshOffsets[mesh->getMeshID()]));

            indirectCommands[mesh->getMeshID()].push_back(vk::DrawIndexedIndirectCommand {
                    .indexCount = static_cast<uint32_t>(mesh->getIndexCount()),
                    .instanceCount = 1,
                    .firstIndex = 0,
                    .vertexOffset = vertexOffset,
                    .firstInstance = static_cast<uint32_t>(i),
            });

            uint64_t vertexStartAddress = vertexOffset * sizeof(Vertex);
            auto g = as.addGeometries<Vertex>(mesh->getBackingBuffer(), mesh->getIndexCount(), 0, *fullySkinnedUnitVertices, mesh->getVertexCount(), {vertexStartAddress});
            geometries.push_back(g);

            auto transform = entity.getComponent<Transform>();
            as.addInstance(InstanceInput {
                .transform = transform->toTransformMatrix(),
                .customInstanceIndex = static_cast<uint32_t>(geometries.size()-1),
                .geometryIndex = static_cast<uint32_t>(geometries.size()-1),
                .mask = 0xFF,
                .hitGroup = 0, // TODO: different materials
            });
        }
    }

    // add map to AS
    auto worldMesh = mapModel->getMeshes()[0];
    as.addGeometries<Vertex>(worldMesh->getBackingBuffer(), worldMesh->getIndexCount(), 0, worldMesh->getBackingBuffer(), worldMesh->getVertexCount(), {worldMesh->getVertexStartOffset()});
    as.addInstance(InstanceInput {
        .customInstanceIndex = maxInstanceCount*3,
        .geometryIndex = maxInstanceCount*3, // TODO: don't hardcode
        .mask = 0xFF,
        .hitGroup = 0,
    });

    as.buildBottomLevelAS();
    as.buildTopLevelAS(false);
    for(const auto& mesh : meshes) {
        indirectBuffers[mesh->getMeshID()]->name("Indirect commands for mesh #"+to_string(mesh->getMeshID()));
        indirectBuffers[mesh->getMeshID()]->stageUploadWithOffsets(make_pair(static_cast<uint64_t>(0),
                                                                             indirectCommands[mesh->getMeshID()]));
    }

    createSkinningComputePipeline(vertexCountPerInstance);

    // TODO: light ID booking system
    auto& light = engine.getRayTracer().getLightBuffer().lights[0];
    auto& dynLight = world.newEntity()
            .addComponent<Transform>()
            .addComponent<RaycastedShadowsLight>(light)
            .addComponent<ForceSinPosition>();

    auto* sinPosition = dynLight.getComponent<ForceSinPosition>();
    sinPosition->angularOffset = {0, 1, 2};
    sinPosition->angularFrequency = {1, 2, 3.14};
    sinPosition->amplitude = {1, 2, 0.2};
    sinPosition->centerPosition = {2,2,1};

    auto& sun = engine.getRayTracer().getLightBuffer().lights[1];
    sun.type = Carrot::LightType::Directional;
    sun.color = glm::vec3(1.0, 1.0, 0.995);
    sun.direction = glm::vec3(1, 1, -1);
    sun.intensity = 0.2f;

    world.newEntity()
            .addComponent<RaycastedShadowsLight>(sun);

    // prepare for first frame
    world.tick(0);
    world.onFrame(0);
}

void Game::Game::createSkinningComputePipeline(uint64_t vertexCountPerInstance) {
    // TODO: move outside of game code

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

    uint32_t specData[] = {
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

    constexpr size_t set0Size = 3;
    constexpr size_t set1Size = 1;
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

    vector<vk::DescriptorPoolSize> poolSizes{};
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

        computeDescriptorPools.emplace_back(move(pool));
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
        vector<vk::WriteDescriptorSet> writes = {
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

    auto computeStage = ShaderModule(engine.getVulkanDriver(), "resources/shaders/skinning.compute.glsl.spv");

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

    uint32_t vertexGroups = (vertexCountPerInstance + 127) / 128;
    uint32_t instanceGroups = (maxInstanceCount + 7)/8;
    for(size_t i = 0; i < engine.getSwapchainImageCount(); i++) {
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

        skinningSemaphores.emplace_back(move(engine.getLogicalDevice().createSemaphoreUnique({})));
    }
}

void Game::Game::onFrame(uint32_t frameIndex) {
    ZoneScoped;
    // TODO: optimize

    // submit skinning command buffer
    // start skinning as soon as possible, even if that means we will have a frame of delay (render before update)
    engine.getComputeQueue().submit(vk::SubmitInfo {
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = nullptr,
            .commandBufferCount = 1,
            .pCommandBuffers = &skinningCommandBuffers[frameIndex],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &(*skinningSemaphores[frameIndex]),
    });

/*    map<Unit::Type, glm::vec3> centers{};
    map<Unit::Type, uint32_t> counts{};

    for(const auto& unit : units) {
        glm::vec3& currentCenter = centers[unit->getType()];
        counts[unit->getType()]++;
        currentCenter += unit->getPosition();
    }
    centers[Unit::Type::Red] /= counts[Unit::Type::Red];
    centers[Unit::Type::Green] /= counts[Unit::Type::Green];
    centers[Unit::Type::Blue] /= counts[Unit::Type::Blue];
*/
    world.onFrame(frameIndex);
   // TracyPlot("onFrame delta time", dt*1000);


    // ensure skinning is done
    engine.getComputeQueue().waitIdle();

    // TODO: proper indexing
    engine.getASBuilder().updateBottomLevelAS(blasIndices);
    engine.getASBuilder().updateTopLevelAS();
}

void Game::Game::recordGBufferPass(uint32_t frameIndex, vk::CommandBuffer& commands) {
    {
        TracyVulkanZone(*engine.tracyCtx[frameIndex], commands, "Render map");
        mapModel->draw(frameIndex, commands, *mapInstanceBuffer, 1);
    }

    {
        TracyVulkanZone(*engine.tracyCtx[frameIndex], commands, "Render units");
        const int indirectDrawCount = maxInstanceCount; // TODO: Not all units will be always on the field, + visibility culling?
        commands.bindVertexBuffers(0, fullySkinnedUnitVertices->getVulkanBuffer(), {0});
        model->indirectDraw(frameIndex, commands, *instanceBuffer, indirectBuffers, indirectDrawCount);
    }
}

float yaw = 0.0f;
float pitch = 0.0f;

void Game::Game::onMouseMove(double dx, double dy) {
    if( ! engine.grabbingCursor())
        return;

    auto& camera = engine.getCamera();
    yaw -= dx * 0.01f;
    pitch -= dy * 0.01f;

    const float distanceFromCenter = 5.0f;
    float cosYaw = cos(yaw);
    float sinYaw = sin(yaw);

    float cosPitch = cos(pitch);
    float sinPitch = sin(pitch);
    camera.position = glm::vec3{cosYaw * cosPitch, sinYaw * cosPitch, sinPitch} * distanceFromCenter;
    camera.position += camera.target;
}

void Game::Game::changeGraphicsWaitSemaphores(uint32_t frameIndex, vector<vk::Semaphore>& semaphores, vector<vk::PipelineStageFlags>& waitStages) {
    semaphores.emplace_back(*skinningSemaphores[frameIndex]);
    waitStages.emplace_back(vk::PipelineStageFlagBits::eVertexInput);
}

static float totalTime = 0.0f;
void Game::Game::tick(double frameTime) {
    world.tick(frameTime);

    totalTime += frameTime*10.0f;
    modelInstance[99].color = {
            glm::sin(totalTime)/2.0f+0.5f,
            glm::cos(totalTime)/2.0f+0.5f,
            glm::cos(totalTime)*glm::sin(totalTime)/2.0f+0.5f,
            1.0f,
    };
}