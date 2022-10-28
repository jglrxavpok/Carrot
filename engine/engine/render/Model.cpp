//
// Created by jglrxavpok on 30/11/2020.
//

#include "Model.h"
#include "engine/render/resources/Mesh.h"
#include <iostream>
#include <core/utils/stringmanip.h>
#include "engine/render/resources/Pipeline.h"
#include <glm/gtx/quaternion.hpp>
#include <engine/utils/conversions.h>
#include <core/io/Logging.hpp>
#include <engine/render/DrawData.h>
#include <engine/render/RenderPacket.h>
#include <engine/utils/Profiling.h>
#include "engine/render/raytracing/ASBuilder.h"

#include "engine/render/resources/SingleMesh.h"
#include "engine/render/resources/model_loading/AssimpLoader.h"
#include "engine/render/resources/model_loading/GLTFLoader.h"

Carrot::Model::Model(Carrot::Engine& engine, const Carrot::IO::Resource& file): engine(engine), resource(file) {
    ZoneScoped;
    ZoneText(file.getName().c_str(), file.getName().size());
    debugName = file.getName();
   // Profiling::PrintingScopedTimer _t(Carrot::sprintf("Model::Model(%s)", file.getName().c_str()));

    verify(file.isFile(), "In-memory models are not supported!");

    Carrot::Log::info("Loading model %s", file.getName().c_str());

    const Carrot::IO::Path filePath { file.getName() };

    Render::LoadedScene scene;
    /*if(filePath.getExtension() == ".gltf") {
        Render::GLTFLoader loader;
        scene = std::move(loader.load(file));
    } else */{
        Render::AssimpLoader loader;
        scene = std::move(loader.load(file));
    }

    // TODO: make different pipelines based on loaded materials
    staticMeshesPipeline = engine.getRenderer().getOrCreatePipeline("gBuffer");
    skinnedMeshesPipeline = engine.getRenderer().getOrCreatePipeline("gBuffer");

    Carrot::Async::Counter waitMaterialLoads;
    for(const auto& material : scene.materials) {
        ZoneScopedN("Loading material");
        ZoneText(material.name.c_str(), material.name.size());

        Profiling::PrintingScopedTimer _t(Carrot::sprintf("Loading material %s", material.name.c_str()));

        auto& materialSystem = GetRenderer().getMaterialSystem();
        auto handle = materialSystem.createMaterialHandle();

        GetTaskScheduler().schedule(Carrot::TaskDescription {
            .name = Carrot::sprintf("Load textures for material %s", material.name.c_str()),
            .task = Carrot::Async::AsTask<void>([material, handle, &file, &waitMaterialLoads]() -> void {
                auto& materialSystem = GetRenderer().getMaterialSystem();
                auto setMaterialTexture = [&file, material](std::shared_ptr<Render::TextureHandle>& toSet, const Carrot::IO::VFS::Path& texturePath, std::shared_ptr<Render::TextureHandle> defaultHandle) {
                    Profiling::PrintingScopedTimer _t(Carrot::sprintf("setMaterialTexture(%s)", texturePath.toString().c_str()));
                    auto& materialSystem = GetRenderer().getMaterialSystem();
                    bool loadedATexture = false;
                    if(!texturePath.isEmpty()) {
                        Carrot::IO::Resource from;
                        try {
                            from = texturePath;
                            auto texture = materialSystem.createTextureHandle(GetRenderer().getOrCreateTextureFromResource(from));
                            toSet = texture;
                            loadedATexture = true;
                        } catch (std::exception& e) {
                            Carrot::Log::error("[%s] Failed to open texture '%s', reason: %s", file.getName().c_str(), texturePath.toString().c_str(), e.what());
                        }
                    } else {
                        toSet = defaultHandle;
                    }
                    return loadedATexture;
                };

                setMaterialTexture(handle->diffuseTexture, material.albedo, materialSystem.getWhiteTexture());
                // TODO: albedo blend color

                setMaterialTexture(handle->normalMap, material.normalMap, materialSystem.getBlueTexture());
                // TODO: metalicness + roughness
            }),
            .joiner = &waitMaterialLoads,
        });
        materials.push_back(handle);
    }

    for(const auto& primitive : scene.primitives) {
        std::shared_ptr<SingleMesh> mesh;
        const auto& material = materials[primitive.materialIndex];
        const glm::mat4& nodeTransform = primitive.transform;
        if(primitive.isSkinned) {
            mesh = std::make_shared<SingleMesh>(primitive.skinnedVertices, primitive.indices);
            skinnedMeshes[material->getSlot()].emplace_back(mesh, nodeTransform);
        } else {
            mesh = std::make_shared<SingleMesh>(primitive.vertices, primitive.indices);
            staticMeshes[material->getSlot()].emplace_back(mesh, nodeTransform);
        }
    }

    animationData = std::move(scene.animationData);
    animationMapping = std::move(scene.animationMapping);
    boneMapping = std::move(scene.boneMapping);
    offsetMatrices = std::move(scene.offsetMatrices);
    skeleton = std::move(scene.nodeHierarchy);

    if(animationData != nullptr) {
        // create descriptor set for animation buffer
        vk::DescriptorSetLayoutBinding binding {
                .binding = 0,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eRaygenKHR,
        };
        animationSetLayout = engine.getLogicalDevice().createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo{
                .bindingCount = 1,
                .pBindings = &binding,
        }, engine.getAllocator());

        vk::DescriptorPoolSize size {
                .type = vk::DescriptorType::eStorageBuffer,
                .descriptorCount = engine.getSwapchainImageCount(),
        };
        animationSetPool = engine.getLogicalDevice().createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo {
                .maxSets = engine.getSwapchainImageCount(),
                .poolSizeCount = 1,
                .pPoolSizes = &size,
        }, engine.getAllocator());

        std::vector<vk::DescriptorSetLayout> layouts = {engine.getSwapchainImageCount(), *animationSetLayout};
        animationDescriptorSets = engine.getLogicalDevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo {
                .descriptorPool = *animationSetPool,
                .descriptorSetCount = engine.getSwapchainImageCount(),
                .pSetLayouts = layouts.data(),
        });

        vk::DescriptorBufferInfo bufferInfo {
                .buffer = animationData->getVulkanBuffer(),
                .offset = 0,
                .range = animationData->getSize(),
        };
        std::vector<vk::WriteDescriptorSet> writes{engine.getSwapchainImageCount()};
        for(size_t writeIndex = 0; writeIndex < writes.size(); writeIndex++) {
            auto& write = writes[writeIndex];
            write.descriptorCount = 1;
            write.descriptorType = vk::DescriptorType::eStorageBuffer;
            write.dstSet = animationDescriptorSets[writeIndex];
            write.dstBinding = 0;
            write.pBufferInfo = &bufferInfo;
        }

        engine.getLogicalDevice().updateDescriptorSets(writes, {});
    }

    if(GetCapabilities().supportsRaytracing) {
        auto& builder = GetRenderer().getASBuilder();

        std::vector<std::shared_ptr<Mesh>> allStaticMeshes{};
        std::vector<glm::mat4> transforms{};

        allStaticMeshes.reserve(staticMeshes.size()); // assume 1 mesh / material
        transforms.reserve(staticMeshes.size()); // assume 1 mesh / material
        for(const auto& [material, meshes] : staticMeshes) {
            for(const auto& [mesh, transform] : meshes) {
                allStaticMeshes.push_back(mesh);
                transforms.push_back(transform);
            }
        }

        if(!allStaticMeshes.empty()) {
            staticBLAS = builder.addBottomLevel(allStaticMeshes, transforms);
        }
    }
    waitMaterialLoads.busyWait(); // hide latency
}

void Carrot::Model::draw(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands, const Carrot::Buffer& instanceData, std::uint32_t instanceCount, const Carrot::UUID& entityID) {
    commands.bindVertexBuffers(1, instanceData.getVulkanBuffer(), {0});

    DrawData data;
    data.setUUID(entityID);
    auto draw = [&](auto meshes, auto& pipeline) {
        for(const auto& [mat, meshList] : meshes) {
            for(const auto& [mesh, transform] : meshList) {
                data.materialIndex = mat;
                renderContext.renderer.pushConstantBlock<DrawData>("drawDataPush", pipeline, renderContext, vk::ShaderStageFlagBits::eFragment, commands, data);
                mesh->bind(commands);
                mesh->draw(commands, instanceCount);
            }
        }
    };
    if(!staticMeshes.empty()) {
        staticMeshesPipeline->bind(pass, renderContext, commands);
        draw(staticMeshes, *staticMeshesPipeline);
    }

    if(!skinnedMeshes.empty()) {
        skinnedMeshesPipeline->bind(pass, renderContext, commands);
        draw(skinnedMeshes, *skinnedMeshesPipeline);
    }
}

void Carrot::Model::indirectDraw(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands, const Carrot::Buffer& instanceData, const std::map<MeshID, std::shared_ptr<Carrot::Buffer>>& indirectDrawCommands, std::uint32_t drawCount, const Carrot::UUID& entityID) {
    commands.bindVertexBuffers(1, instanceData.getVulkanBuffer(), {0});

    DrawData data;
    data.setUUID(entityID);

    auto draw = [&](auto meshes, auto& pipeline) {
        for(const auto& [mat, meshList] : meshes) {
            for(const auto& [mesh, transform] : meshList) {
                data.materialIndex = mat;
                renderContext.renderer.pushConstantBlock<DrawData>("drawDataPush", pipeline, renderContext, vk::ShaderStageFlagBits::eFragment, commands, data);
                mesh->bindForIndirect(commands);
                mesh->indirectDraw(commands, *indirectDrawCommands.at(mesh->getMeshID()), drawCount);
            }
        }
    };
    if(!staticMeshes.empty()) {
        staticMeshesPipeline->bind(pass, renderContext, commands);
        draw(staticMeshes, *staticMeshesPipeline);
    }

    if(!skinnedMeshes.empty()) {
        skinnedMeshesPipeline->bind(pass, renderContext, commands);
        draw(skinnedMeshes, *skinnedMeshesPipeline);
    }
}

void Carrot::Model::renderStatic(const Carrot::Render::Context& renderContext, const Carrot::InstanceData& instanceData, Render::PassEnum renderPass) {
    ZoneScoped;
    DrawData data;

    Render::Packet& packet = GetRenderer().makeRenderPacket(renderPass, renderContext.viewport);
    packet.pipeline = staticMeshesPipeline;
    packet.transparentGBuffer.zOrder = 0.0f;

    Render::Packet::PushConstant& pushConstant = packet.addPushConstant();
    pushConstant.id = "drawDataPush";
    pushConstant.stages = vk::ShaderStageFlagBits::eFragment;

    {
        ZoneScopedN("instance use");
        packet.useInstance(instanceData);
    }

    Carrot::InstanceData meshInstanceData = instanceData;
    for (const auto&[mat, meshList]: staticMeshes) {
        for (const auto& [mesh, transform]: meshList) {
            ZoneScopedN("mesh use");
            data.materialIndex = mat;
            packet.useMesh(*mesh);

            meshInstanceData.transform = instanceData.transform * transform;
            packet.useInstance(meshInstanceData);


            pushConstant.setData(data); // template operator=

            renderContext.renderer.render(packet);
        }
    }
}

void Carrot::Model::renderSkinned(const Carrot::Render::Context& renderContext, const Carrot::AnimatedInstanceData& instanceData, Render::PassEnum renderPass) {
    DrawData data;

    Render::Packet& packet = GetRenderer().makeRenderPacket(renderPass, renderContext.viewport);
    packet.pipeline = skinnedMeshesPipeline;
    packet.transparentGBuffer.zOrder = 0.0f;

    Render::Packet::PushConstant& pushConstant = packet.addPushConstant();
    pushConstant.id = "drawDataPush";
    pushConstant.stages = vk::ShaderStageFlagBits::eFragment;

    Carrot::AnimatedInstanceData meshInstanceData = instanceData;
    for (const auto&[mat, meshList]: skinnedMeshes) {
        for (const auto& [mesh, transform]: meshList) {
            data.materialIndex = mat;
            packet.useMesh(*mesh);

            meshInstanceData.transform = instanceData.transform * transform;
            packet.useInstance(meshInstanceData);

            pushConstant.setData(data); // template operator=

            renderContext.renderer.render(packet);
        }
    }
}

std::unordered_map<std::uint32_t, std::vector<Carrot::Mesh::Ref>> Carrot::Model::getSkinnedMeshes() const {
    std::unordered_map<std::uint32_t, std::vector<Carrot::Mesh::Ref>> result;
    for(const auto& [material, meshes] : skinnedMeshes) {
        for(const auto& [mesh, transform] : meshes) {
            result[material].push_back(mesh);
        }
    }
    return result;
}

std::vector<std::shared_ptr<Carrot::Mesh>> Carrot::Model::getStaticMeshes() const {
    std::vector<std::shared_ptr<Mesh>> result{};
    for(const auto& [material, meshes] : staticMeshes) {
        for(const auto& [mesh, transform] : meshes) {
            result.push_back(mesh);
        }
    }
    return result;
}

Carrot::Buffer& Carrot::Model::getAnimationDataBuffer() {
    return *animationData;
}

std::shared_ptr<Carrot::BLASHandle> Carrot::Model::getStaticBLAS() {
    return staticBLAS;
}

std::shared_ptr<Carrot::BLASHandle> Carrot::Model::getSkinnedBLAS() {
    return skinnedBLAS;
}

bool Carrot::Model::hasSkeleton() const {
    return skeleton != nullptr;
}

Carrot::Render::Skeleton& Carrot::Model::getSkeleton() {
    verify(skeleton, Carrot::sprintf("No skeleton in model %s", debugName.c_str()));
    return *skeleton;
}

const Carrot::Render::Skeleton& Carrot::Model::getSkeleton() const {
    verify(skeleton, Carrot::sprintf("No skeleton in model %s", debugName.c_str()));
    return *skeleton;
}

const std::unordered_map<int, std::unordered_map<std::string, std::uint32_t>>& Carrot::Model::getBoneMapping() const {
    return boneMapping;
}

const std::unordered_map<int, std::unordered_map<std::string, glm::mat4>>& Carrot::Model::getBoneOffsetMatrices() const {
    return offsetMatrices;
}
