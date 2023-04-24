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
#include <core/scene/LoadedScene.h>
#include <engine/render/GBufferDrawData.h>
#include <engine/render/RenderPacket.h>
#include <engine/utils/Profiling.h>
#include "engine/render/raytracing/ASBuilder.h"

#include "engine/render/resources/SingleMesh.h"
#include "engine/render/resources/model_loading/AssimpLoader.h"
#include <core/scene/GLTFLoader.h>
#include <engine/console/RuntimeOption.hpp>
#include <engine/render/resources/LightMesh.h>

static Carrot::RuntimeOption DrawBoundingSpheres("Debug/Draw bounding spheres", false);
static Carrot::RuntimeOption DisableFrustumCheck("Debug/Disable frustum culling", false);

Carrot::Model::Model(Carrot::Engine& engine, const Carrot::IO::Resource& file): engine(engine), resource(file) {
    ZoneScoped;
    ZoneText(file.getName().c_str(), file.getName().size());
    debugName = file.getName();
   // Profiling::PrintingScopedTimer _t(Carrot::sprintf("Model::Model(%s)", file.getName().c_str()));

    verify(file.isFile(), "In-memory models are not supported!");

    Carrot::Log::info("Loading model %s", file.getName().c_str());

    const Carrot::IO::Path filePath { file.getName() };

    Render::LoadedScene scene;
    if(filePath.getExtension() == ".gltf") {
        Render::GLTFLoader loader;
        scene = std::move(loader.load(file));
    } else {
        Render::AssimpLoader loader;
        scene = std::move(loader.load(file));
    }

    // TODO: make different pipelines based on loaded materials
    opaqueMeshesPipeline = engine.getRenderer().getOrCreatePipeline("gBuffer");

/*    if(GetEngine().getCapabilities().supportsRaytracing) {
        transparentMeshesPipeline = engine.getRenderer().getOrCreatePipeline("forward-raytracing");
    } else {
        transparentMeshesPipeline = engine.getRenderer().getOrCreatePipeline("forward-noraytracing");
    }*/
    transparentMeshesPipeline = engine.getRenderer().getOrCreatePipeline("gBuffer-transparent");

    Carrot::Async::Counter waitMaterialLoads;
    // TODO: reduce task count
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
                            toSet = defaultHandle;
                        }
                    } else {
                        toSet = defaultHandle;
                    }
                    return loadedATexture;
                };

                setMaterialTexture(handle->albedo, material.albedo, materialSystem.getWhiteTexture());
                setMaterialTexture(handle->normalMap, material.normalMap, materialSystem.getFlatNormalTexture());
                setMaterialTexture(handle->emissive, material.emissive, materialSystem.getBlackTexture());
                setMaterialTexture(handle->metallicRoughness, material.metallicRoughness, materialSystem.getBlackTexture());

                handle->isTransparent = material.blendMode != Render::LoadedMaterial::BlendMode::None;
                handle->baseColor = material.baseColorFactor;
                handle->emissiveColor = material.emissiveFactor;
                handle->roughnessFactor = material.roughnessFactor;
                handle->metallicFactor = material.metallicFactor;
            }),
            .joiner = &waitMaterialLoads,
        }, TaskScheduler::AssetLoading);
        materials.push_back(handle);
    }

    staticMeshInfo.resize(scene.primitives.size());

    std::vector<Carrot::Vertex> staticVertices;
    std::vector<std::uint32_t> staticIndices;

    std::size_t primitiveIndex = 0;

    std::size_t opaqueMeshIndex = 0;
    std::size_t transparentMeshIndex = 0;
    for(const auto& primitive : scene.primitives) {
        if(primitive.isSkinned) {
            primitiveIndex++;
            continue;
        }

        const std::size_t oldVertexCount = staticVertices.size();
        const std::size_t oldIndexCount = staticIndices.size();
        staticVertices.resize(oldVertexCount + primitive.vertices.size());
        staticIndices.resize(oldIndexCount + primitive.indices.size());

        memcpy(&staticVertices[oldVertexCount], primitive.vertices.data(), primitive.vertices.size() * sizeof(Carrot::Vertex));
        memcpy(&staticIndices[oldIndexCount], primitive.indices.data(), primitive.indices.size() * sizeof(std::uint32_t));

        auto& info = staticMeshInfo[primitiveIndex];
        info.startVertex = oldVertexCount;
        info.startIndex = oldIndexCount;
        info.vertexCount = primitive.vertices.size();
        info.indexCount = primitive.indices.size();

        primitiveIndex++;
    }

    if(!staticVertices.empty()) {
        verify(!staticIndices.empty(), "Non-indexed meshes not supported");
        staticMeshData = std::make_unique<SingleMesh>(staticVertices, staticIndices);
    }

    std::function<void(const Carrot::Render::SkeletonTreeNode&, glm::mat4)> recursivelyLoadNodes = [&](const Carrot::Render::SkeletonTreeNode& node, const glm::mat4& nodeTransform) {
        glm::mat4 transform = nodeTransform * node.bone.originalTransform;
        if(node.meshIndices.has_value()) {
            for(const std::size_t meshIndex : node.meshIndices.value()) {
                auto& primitive = scene.primitives[meshIndex];
                std::shared_ptr<Mesh> mesh;
                const auto& material = materials[primitive.materialIndex];

                Math::Sphere sphere;
                sphere.loadFromAABB(primitive.minPos, primitive.maxPos);

                // TODO: load all skinned primitive data into same buffer?
                if(primitive.isSkinned) {
                    mesh = std::make_shared<SingleMesh>(primitive.skinnedVertices, primitive.indices);
                    skinnedMeshes[material->getSlot()].emplace_back(mesh, transform, sphere, -1);
                } else {
                    StaticMeshInfo& meshInfo = staticMeshInfo[meshIndex];
                    mesh = std::make_shared<LightMesh>(std::move(staticMeshData->getSubMesh(meshInfo.startVertex, meshInfo.vertexCount, meshInfo.startIndex, meshInfo.indexCount)));

                    auto& sceneMaterial = scene.materials[primitive.materialIndex];
                    const bool isMaterialTransparent = sceneMaterial.blendMode != Render::LoadedMaterial::BlendMode::None;
                    auto& drawCommands = isMaterialTransparent ? staticTransparentDrawCommands : staticOpaqueDrawCommands;
                    auto& drawDataList = isMaterialTransparent ? staticTransparentDrawData : staticOpaqueDrawData;
                    auto& instanceDataList = isMaterialTransparent ? staticTransparentInstanceData : staticOpaqueInstanceData;

                    staticMeshes[material->getSlot()].emplace_back(mesh, transform, sphere, drawCommands.size());



                    auto& cmd = drawCommands.emplace_back();
                    cmd.instanceCount = 1;
                    cmd.indexCount = meshInfo.indexCount;
                    cmd.firstInstance = drawCommands.size()-1; // increment just before
                    cmd.vertexOffset = meshInfo.startVertex;
                    cmd.firstIndex = meshInfo.startIndex;

                    auto& drawData = drawDataList.emplace_back();
                    drawData.materialIndex = material->getSlot();

                    auto& instanceData = instanceDataList.emplace_back();
                }
                mesh->name(scene.debugName + " (" + primitive.name + ")");
            }
        }

        for(const auto& child : node.getChildren()) {
            recursivelyLoadNodes(child, transform);
        }
    };

    if(scene.nodeHierarchy) {
        recursivelyLoadNodes(scene.nodeHierarchy->hierarchy, glm::mat4{1.0f});
    }

    // upload staging buffer to GPU buffer
    auto& allAnimations = scene.animationData;

    if(!allAnimations.empty()) {
        animationData = std::make_unique<Carrot::Buffer>(GetVulkanDriver(),
                                                         sizeof(Carrot::Animation) * allAnimations.size(),
                                                         vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
                                                         vk::MemoryPropertyFlagBits::eDeviceLocal);
        animationData->setDebugNames("Animations");
        animationData->stageUploadWithOffsets(make_pair(0ull, std::span(allAnimations)));
    }

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
        std::vector<std::uint32_t> staticMeshMaterials{};

        allStaticMeshes.reserve(staticMeshes.size()); // assume 1 mesh / material
        transforms.reserve(staticMeshes.size()); // assume 1 mesh / material
        staticMeshMaterials.reserve(staticMeshes.size()); // assume 1 mesh / material
        for(const auto& [materialSlot, meshes] : staticMeshes) {
            for(const auto& meshInfo : meshes) {
                auto& mesh = meshInfo.mesh;
                auto& transform = meshInfo.transform;
                allStaticMeshes.push_back(mesh);
                transforms.push_back(transform);
                staticMeshMaterials.push_back(materialSlot);
            }
        }

        if(!allStaticMeshes.empty()) {
            staticBLAS = builder.addBottomLevel(allStaticMeshes, transforms, staticMeshMaterials);
        }
    }
    waitMaterialLoads.busyWait(); // hide latency
}

void Carrot::Model::draw(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands, const Carrot::Buffer& instanceData, std::uint32_t instanceCount, const Carrot::UUID& entityID) {
    verify(false, "Deprecated, use Render::Packet system instead");
    commands.bindVertexBuffers(1, instanceData.getVulkanBuffer(), {0});

    GBufferDrawData data;
    data.setUUID(entityID);

    Carrot::Pipeline* boundPipeline = nullptr;
    auto& materialSystem = renderContext.renderer.getMaterialSystem();
    auto draw = [&](auto meshes) {
        for(const auto& [mat, meshList] : meshes) {
            auto weakMat = materialSystem.getMaterial(mat);
            Carrot::Pipeline* pipeline = nullptr;
            if(auto pMat = weakMat.lock()) {
                if(pMat->isTransparent) {
                    pipeline = transparentMeshesPipeline.get();
                } else {
                    pipeline = opaqueMeshesPipeline.get();
                }
            }
            if(pipeline != boundPipeline) {
                pipeline->bind(pass, renderContext, commands);
            }
            for(const auto& meshInfo : meshList) {
                auto& mesh = meshInfo.mesh;
                data.materialIndex = mat;
                renderContext.renderer.pushConstantBlock<GBufferDrawData>("drawDataPush", *pipeline, renderContext, vk::ShaderStageFlagBits::eFragment, commands, data);
                mesh->bind(commands);
                mesh->draw(commands, instanceCount);
            }
        }
    };
    if(!staticMeshes.empty()) {
        draw(staticMeshes);
    }

    if(!skinnedMeshes.empty()) {
        draw(skinnedMeshes);
    }
}

void Carrot::Model::indirectDraw(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands, const Carrot::Buffer& instanceData, const std::map<MeshID, std::shared_ptr<Carrot::Buffer>>& indirectDrawCommands, std::uint32_t drawCount, const Carrot::UUID& entityID) {
    verify(false, "Deprecated, use Render::Packet system instead");
    commands.bindVertexBuffers(1, instanceData.getVulkanBuffer(), {0});

    GBufferDrawData data;
    data.setUUID(entityID);

    Carrot::Pipeline* boundPipeline = nullptr;
    auto& materialSystem = renderContext.renderer.getMaterialSystem();
    auto draw = [&](auto meshes) {
        for(const auto& [mat, meshList] : meshes) {
            auto weakMat = materialSystem.getMaterial(mat);
            Carrot::Pipeline* pipeline = nullptr;
            if(auto pMat = weakMat.lock()) {
                if(pMat->isTransparent) {
                    pipeline = transparentMeshesPipeline.get();
                } else {
                    pipeline = opaqueMeshesPipeline.get();
                }
            }
            if(pipeline != boundPipeline) {
                pipeline->bind(pass, renderContext, commands);
            }
            for(const auto& meshInfo : meshList) {
                auto& mesh = meshInfo.mesh;
                data.materialIndex = mat;
                renderContext.renderer.pushConstantBlock<GBufferDrawData>("drawDataPush", *pipeline, renderContext, vk::ShaderStageFlagBits::eFragment, commands, data);
                mesh->bindForIndirect(commands);
                mesh->indirectDraw(commands, *indirectDrawCommands.at(mesh->getMeshID()), drawCount);
            }
        }
    };
    if(!staticMeshes.empty()) {
        draw(staticMeshes);
    }

    if(!skinnedMeshes.empty()) {
        draw(skinnedMeshes);
    }
}

void Carrot::Model::renderStatic(const Carrot::Render::Context& renderContext, const Carrot::InstanceData& instanceData, Render::PassEnum renderPass) {
    ZoneScoped;

    Render::Packet& opaquePacket = GetRenderer().makeRenderPacket(renderPass, renderContext.viewport);
    Render::Packet& transparentPacket = GetRenderer().makeRenderPacket(renderPass, renderContext.viewport);
    opaquePacket.pipeline = opaqueMeshesPipeline;
    transparentPacket.pipeline = transparentMeshesPipeline;
    transparentPacket.transparentGBuffer.zOrder = 0.0f; // TODO

    opaquePacket.drawCommands = staticOpaqueDrawCommands;
    transparentPacket.drawCommands = staticTransparentDrawCommands;

    opaquePacket.vertexBuffer = staticMeshData->getVertexBuffer();
    opaquePacket.indexBuffer = staticMeshData->getIndexBuffer();
    transparentPacket.vertexBuffer = staticMeshData->getVertexBuffer();
    transparentPacket.indexBuffer = staticMeshData->getIndexBuffer();

    opaquePacket.addPerDrawData(staticOpaqueDrawData);
    transparentPacket.addPerDrawData(staticTransparentDrawData);

    // TODO: per-draw buffer to replace push constants
    // push constant contains first index for first draw, then access with gl_DrawID

    auto& materialSystem = renderContext.renderer.getMaterialSystem();
    for (const auto&[mat, meshList]: staticMeshes) {
        auto weakMat = materialSystem.getMaterial(mat);
        if(auto pMat = weakMat.lock()) {
            for (const auto& meshInfo: meshList) {
                auto& mesh = meshInfo.mesh;
                auto& transform = meshInfo.transform;
                auto& sphere = meshInfo.boundingSphere;
                auto& meshIndex = meshInfo.meshIndex;
                ZoneScopedN("mesh use");

                InstanceData* pInstanceData = nullptr;
                vk::DrawIndexedIndirectCommand* pDrawCommand = nullptr;
                if(!pMat->isTransparent) {
                    pInstanceData = &staticOpaqueInstanceData[meshIndex];
                    pDrawCommand = &opaquePacket.drawCommands[meshIndex];
                } else {
                    pInstanceData = &staticTransparentInstanceData[meshIndex];
                    pDrawCommand = &transparentPacket.drawCommands[meshIndex];
                }
                *pInstanceData = instanceData;

                pInstanceData->transform = instanceData.transform * transform;

                Math::Sphere s = sphere;
                s.transform(pInstanceData->transform);

                bool frustumCheck = renderContext.getCamera().isInFrustum(s);
                if(DisableFrustumCheck) {
                    frustumCheck = true;
                }

                pDrawCommand->instanceCount = frustumCheck ? 1 : 0;
                if(!frustumCheck) {
                    continue;
                }

                if(DrawBoundingSpheres) {
                    if(this != renderContext.renderer.getUnitSphere().get()) {
                        glm::mat4 sphereTransform = glm::translate(glm::mat4{1.0f}, s.center) * glm::scale(glm::mat4{1.0f}, glm::vec3{s.radius*2 /*unit sphere model has a radius of 0.5*/});
                        renderContext.renderer.renderWireframeSphere(renderContext, sphereTransform, 1.0f, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), instanceData.uuid);
                    }
                }

                pInstanceData->lastFrameTransform = instanceData.lastFrameTransform * transform;
            }
        }
    }

    opaquePacket.useInstances(std::span(staticOpaqueInstanceData));
    transparentPacket.useInstances(std::span(staticTransparentInstanceData));

    if(!opaquePacket.drawCommands.empty()) {
        renderContext.renderer.render(opaquePacket);
    }
    if(!transparentPacket.drawCommands.empty()) {
        renderContext.renderer.render(transparentPacket);
    }
}

void Carrot::Model::renderSkinned(const Carrot::Render::Context& renderContext, const Carrot::AnimatedInstanceData& instanceData, Render::PassEnum renderPass) {
/* TODO
    GBufferDrawData data;

    const bool inTransparentPass = renderPass == Render::PassEnum::TransparentGBuffer;

    Render::Packet& packet = GetRenderer().makeRenderPacket(renderPass, renderContext.viewport);
    packet.pipeline = inTransparentPass ? transparentMeshesPipeline : opaqueMeshesPipeline;
    packet.transparentGBuffer.zOrder = 0.0f;

    Render::Packet::PushConstant& pushConstant = packet.addPushConstant();
    pushConstant.id = "drawDataPush";
    pushConstant.stages = vk::ShaderStageFlagBits::eFragment;

    Carrot::AnimatedInstanceData meshInstanceData = instanceData;
    for (const auto&[mat, meshList]: skinnedMeshes) {
        for (const auto& [mesh, transform, sphere]: meshList) {
            meshInstanceData.transform = instanceData.transform * transform;

            Math::Sphere s = sphere;
            s.transform(meshInstanceData.transform);
            if(!renderContext.getCamera().isInFrustum(s)) {
                continue;
            }

            data.materialIndex = mat;
            packet.useMesh(*mesh);

            packet.useInstance(meshInstanceData);

            pushConstant.setData(data); // template operator=

            renderContext.renderer.render(packet);
        }
    }*/
}

std::unordered_map<std::uint32_t, std::vector<Carrot::Mesh::Ref>> Carrot::Model::getSkinnedMeshes() const {
    std::unordered_map<std::uint32_t, std::vector<Carrot::Mesh::Ref>> result;
    for(const auto& [material, meshes] : skinnedMeshes) {
        for(const auto& meshInfo : meshes) {
            result[material].push_back(meshInfo.mesh);
        }
    }
    return result;
}

std::vector<std::shared_ptr<Carrot::Mesh>> Carrot::Model::getStaticMeshes() const {
    std::vector<std::shared_ptr<Mesh>> result{};
    for(const auto& [material, meshes] : staticMeshes) {
        for(const auto& meshInfo : meshes) {
            result.push_back(meshInfo.mesh);
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
