//
// Created by jglrxavpok on 30/11/2020.
//

#include "Model.h"
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <assimp/IOSystem.hpp>
#include <assimp/IOStream.hpp>
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

Carrot::Model::Model(Carrot::Engine& engine, const Carrot::IO::Resource& file): engine(engine), resource(file) {
    ZoneScoped;
    TracyMessageL(file.getName().c_str());
    Profiling::PrintingScopedTimer _t(Carrot::sprintf("Model::Model(%s)", file.getName().c_str()));

    Assimp::Importer importer{};
    const aiScene* scene = nullptr;
    {
        verify(file.isFile(), "In-memory models are not supported!");
        scene = importer.ReadFile(file.getName(), aiProcess_Triangulate | aiProcess_OptimizeMeshes | aiProcess_CalcTangentSpace | aiProcess_FlipUVs);
    }

    if(!scene) {
        throw std::runtime_error("Failed to load model "+file.getName());
    }

    Carrot::Log::info("Loading model %s", file.getName().c_str());

    staticMeshesPipeline = engine.getRenderer().getOrCreatePipeline("gBuffer");
    skinnedMeshesPipeline = engine.getRenderer().getOrCreatePipeline("gBuffer");//engine.getRenderer().getOrCreatePipeline("gBufferWithBoneInfo");

    std::unordered_map<std::uint32_t, std::shared_ptr<Render::MaterialHandle>> materialMap;
    auto& materialSystem = engine.getRenderer().getMaterialSystem();
    for(std::size_t materialIndex = 0; materialIndex < scene->mNumMaterials; materialIndex++) {
        const aiMaterial* mat = scene->mMaterials[materialIndex];
        std::string materialName = mat->GetName().data;

        auto handle = materialSystem.createMaterialHandle();

        auto setMaterialTexture = [&](std::shared_ptr<Render::TextureHandle>& toSet, aiTextureType textureType, std::shared_ptr<Render::TextureHandle> defaultHandle) {
            Profiling::PrintingScopedTimer _t(Carrot::sprintf("setMaterialTexture(%s)", TextureTypeToString(textureType)));
            bool loadedATexture = false;
            std::size_t count = mat->GetTextureCount(textureType);
            if(count > 0) {
                if(count > 1) {
                    Carrot::Log::warn("Model '%s' has %d textures of type '%s'. Carrot only supports 1. Only the first texture will be used", file.getName().c_str(), count, TextureTypeToString(textureType));
                }
                aiString path;
                mat->GetTexture(textureType, 0, &path);
                Carrot::IO::Resource from;
                try {
                    from = file.relative(std::string(path.C_Str()));
                    auto texture = materialSystem.createTextureHandle(GetRenderer().getOrCreateTextureFromResource(from));
                    toSet = texture;
                    loadedATexture = true;
                } catch (std::runtime_error& e) {
                    Carrot::Log::warn("Failed to open texture '%s'", path.C_Str());
                }
            } else {
                toSet = defaultHandle;
            }

            return loadedATexture;
        };
        setMaterialTexture(handle->diffuseTexture, aiTextureType_DIFFUSE, materialSystem.getWhiteTexture());
        if(!setMaterialTexture(handle->normalMap, aiTextureType_NORMALS, materialSystem.getBlueTexture())) {
            setMaterialTexture(handle->normalMap, aiTextureType_HEIGHT, materialSystem.getBlueTexture());
        }

        materialMap[materialIndex] = handle;
        materials.push_back(handle);
    }

    std::unordered_map<std::string, std::uint32_t> boneMapping{};
    std::unordered_map<std::string, glm::mat4> offsetMatrices{};
    aiNode* armature = scene->mRootNode;

    for(std::size_t meshIndex = 0; meshIndex < scene->mNumMeshes; meshIndex++) {
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        auto loadedMesh = loadMesh(mesh, boneMapping, offsetMatrices);
        auto& material = materialMap[mesh->mMaterialIndex];
        bool usesSkinning = mesh->HasBones();
        loadedMesh->name(file.getName()+", mesh #"+std::to_string(loadedMesh->getMeshID()));

        if(usesSkinning) {
            skinnedMeshes[material->getSlot()].push_back(loadedMesh);
        } else {
            staticMeshes[material->getSlot()].push_back(loadedMesh);
        }
    }

    if(scene->HasAnimations()) {
        loadAnimations(engine, scene, boneMapping, offsetMatrices, armature);
    }
}

void Carrot::Model::loadAnimations(Carrot::Engine& engine, const aiScene *scene,
                                   const std::unordered_map<std::string, std::uint32_t>& boneMapping, const std::unordered_map<std::string, glm::mat4>& offsetMatrices, const aiNode *armature) {

    glm::mat4 globalInverseTransform = glm::inverse(glmMat4FromAssimp(armature->mTransformation));

    // load animations into staging buffer
    std::vector<Animation> allAnimations{};
    for(int animationIndex = 0; animationIndex < scene->mNumAnimations; animationIndex++) {
        aiAnimation* anim = scene->mAnimations[animationIndex];
        std::string name = anim->mName.data;

        // collect all timestamps
        std::set<float> timestampSet{};
        std::unordered_map<std::string, aiNodeAnim*> animationNodes{};
        // TODO: reduce timestamp count by removing duplicates
        for(int channelIndex = 0; channelIndex < anim->mNumChannels; channelIndex++) {
            aiNodeAnim* node = anim->mChannels[channelIndex];
            animationNodes[node->mNodeName.data] = node;
            for(int j = 0; j < node->mNumPositionKeys; j++) {
                timestampSet.insert(node->mPositionKeys[j].mTime);
            }
            for(int j = 0; j < node->mNumRotationKeys; j++) {
                timestampSet.insert(node->mRotationKeys[j].mTime);
            }
            for(int j = 0; j < node->mNumScalingKeys; j++) {
                timestampSet.insert(node->mScalingKeys[j].mTime);
            }
        }

        allAnimations.emplace_back();
        auto& animation = allAnimations[animationIndex];
        if(timestampSet.size() > Carrot::MAX_KEYFRAMES_PER_ANIMATION) {
            throw std::runtime_error("Too many keyframes: "+std::to_string(timestampSet.size()));
        }
        animation.keyframeCount = timestampSet.size();
        animation.duration = anim->mDuration / anim->mTicksPerSecond;

        std::vector<float> timestamps = {timestampSet.begin(), timestampSet.end()};
        for(size_t index = 0; index < timestamps.size(); index++) {
            float time = timestamps[index];
            auto& keyframe = animation.keyframes[index];
            keyframe.timestamp = time / anim->mTicksPerSecond;
            this->updateKeyframeRecursively(keyframe, armature, time, boneMapping, animationNodes, offsetMatrices, globalInverseTransform);
        }
        this->animations[name] = &allAnimations[animationIndex];

        this->animationMapping[name] = animationIndex;
    }

    // upload staging buffer to GPU buffer
    this->animationData = std::make_unique<Carrot::Buffer>(engine.getVulkanDriver(),
                                                      sizeof(Carrot::Animation) * scene->mNumAnimations,
                                                      vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
                                                      vk::MemoryPropertyFlagBits::eDeviceLocal);
    animationData->setDebugNames("Animations");
    animationData->stageUploadWithOffsets(make_pair(0ull, allAnimations));

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

void Carrot::Model::draw(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands, const Carrot::Buffer& instanceData, std::uint32_t instanceCount, const Carrot::UUID& entityID) {
    commands.bindVertexBuffers(1, instanceData.getVulkanBuffer(), {0});

    DrawData data;
    data.setUUID(entityID);
    auto draw = [&](auto meshes, auto& pipeline) {
        for(const auto& [mat, meshList] : meshes) {
            for(const auto& mesh : meshList) {
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
            for(const auto& mesh : meshList) {
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
    DrawData data;

    Render::Packet packet(renderPass);
    packet.pipeline = staticMeshesPipeline;
    packet.viewport = &renderContext.viewport;
    packet.transparentGBuffer.zOrder = 0.0f;

    Render::Packet::PushConstant& pushConstant = packet.addPushConstant();
    pushConstant.id = "drawDataPush";
    pushConstant.stages = vk::ShaderStageFlagBits::eFragment;

    packet.useInstance(instanceData);

    for (const auto&[mat, meshList]: staticMeshes) {
        for (const auto& mesh: meshList) {
            data.materialIndex = mat;
            packet.useMesh(*mesh);

            pushConstant.setData(data); // template operator=

            renderContext.renderer.render(packet);
        }
    }
}

void Carrot::Model::renderSkinned(const Carrot::Render::Context& renderContext, const Carrot::AnimatedInstanceData& instanceData, Render::PassEnum renderPass) {
    DrawData data;

    Render::Packet packet(renderPass);
    packet.pipeline = skinnedMeshesPipeline;
    packet.transparentGBuffer.zOrder = 0.0f;

    Render::Packet::PushConstant& pushConstant = packet.addPushConstant();
    pushConstant.id = "drawDataPush";
    pushConstant.stages = vk::ShaderStageFlagBits::eFragment;

    packet.useInstance(instanceData);

    for (const auto&[mat, meshList]: skinnedMeshes) {
        for (const auto& mesh: meshList) {
            data.materialIndex = mat;
            packet.useMesh(*mesh);

            pushConstant.setData(data); // template operator=

            renderContext.renderer.render(packet);
        }
    }
}

std::shared_ptr<Carrot::Mesh> Carrot::Model::loadMesh(const aiMesh* mesh, std::unordered_map<std::string, std::uint32_t>& boneMapping, std::unordered_map<std::string, glm::mat4>& offsetMatrices) {
    std::vector<Vertex> vertices{};
    std::vector<SkinnedVertex> skinnedVertices{};
    std::vector<std::uint32_t> indices{};

    bool usesSkinning = mesh->HasBones();

    for(size_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; vertexIndex++) {
        const aiVector3D vec = mesh->mVertices[vertexIndex];
        const aiColor4D* vertexColor = mesh->mColors[0];
        const aiVector3D* vertexUVW = mesh->mTextureCoords[0];
        const aiVector3D vertexNormal = mesh->mNormals[vertexIndex];
        const aiVector3D vertexTangent = mesh->mTangents[vertexIndex];
        glm::vec3 color = {1.0f, 1.0f, 1.0f};
        if(vertexColor) {
            color = {
                    vertexColor[vertexIndex].r,
                    vertexColor[vertexIndex].g,
                    vertexColor[vertexIndex].b,
            };
        }

        glm::vec2 uv = {0.0f,0.0f};
        if(vertexUVW) {
            uv = {
                    vertexUVW[vertexIndex].x,
                    vertexUVW[vertexIndex].y,
            };
        }

        glm::vec3 normal = {
                vertexNormal.x,
                vertexNormal.y,
                vertexNormal.z,
        };

        glm::vec3 tangent = {
                vertexTangent.x,
                vertexTangent.y,
                vertexTangent.z,
        };

        glm::vec4 position = {vec.x, vec.y, vec.z, 1.0f};

        if(usesSkinning) {
            skinnedVertices.push_back({
                                        position,
                                        color,
                                        normal,
                                        tangent,
                                        uv,
                                        glm::ivec4{-1, -1, -1, -1},
                                        glm::vec4{0.0f},
                               });
        } else {
            vertices.push_back({
                                        position,
                                        color,
                                        normal,
                                        tangent,
                                        uv,
                               });
        }
    }

    if(usesSkinning) {
        for(std::size_t boneIndex = 0; boneIndex < mesh->mNumBones; boneIndex++) {
            aiBone* bone = mesh->mBones[boneIndex];

            for(int i = 0; i < bone->mNumWeights; i++) {
                const aiVertexWeight& weight = bone->mWeights[i];
                auto& vertex = skinnedVertices[weight.mVertexId];
                vertex.addBoneInformation(boneIndex, weight.mWeight);
            }

            boneMapping[bone->mName.data] = boneIndex;
            offsetMatrices[bone->mName.data] = glmMat4FromAssimp(bone->mOffsetMatrix);
        }
    }

    for(std::size_t faceIndex = 0; faceIndex < mesh->mNumFaces; faceIndex++) {
        const aiFace face = mesh->mFaces[faceIndex];
        if(face.mNumIndices != 3) {
            throw std::runtime_error("Can only load triangles.");
        }

        indices.push_back(face.mIndices[0]);
        indices.push_back(face.mIndices[1]);
        indices.push_back(face.mIndices[2]);
    }

    if(usesSkinning) {
        return std::make_shared<Mesh>(engine.getVulkanDriver(), skinnedVertices, indices);
    } else {
        return std::make_shared<Mesh>(engine.getVulkanDriver(), vertices, indices);
    }
}

void Carrot::Model::updateKeyframeRecursively(Carrot::Keyframe& keyframe, const aiNode* armature, float time, const std::unordered_map<std::string, std::uint32_t>& boneMapping, const std::unordered_map<std::string, aiNodeAnim*>& animationNodes, const std::unordered_map<std::string, glm::mat4>& offsetMatrices, const glm::mat4& globalInverseTransform, const glm::mat4& parentMatrix) {
    if(armature == nullptr)
        return;
    std::string boneName = armature->mName.data;
    auto potentialMapping = boneMapping.find(boneName);
    auto potentialAnim = animationNodes.find(boneName);
    glm::mat4 nodeTransform = glmMat4FromAssimp(armature->mTransformation);
    glm::mat4 globalTransform{1.0f};
    if(potentialMapping != boneMapping.end()) {
        uint32_t boneID = potentialMapping->second;


        if(potentialAnim != animationNodes.end()) {
            aiNodeAnim* animInfo = potentialAnim->second;
            glm::vec3 translation{0.0f};
            glm::quat rotation{1.0f,0.0f,0.0f,0.0f};
            glm::vec3 scaling{1.0f};

            for(size_t index = 0; index < animInfo->mNumRotationKeys-1; index++) {
                auto key = animInfo->mRotationKeys[index+1];
                if(time <= key.mTime) {
                    rotation = {key.mValue.w, key.mValue.x, key.mValue.y, key.mValue.z};
                    break;
                }
            }

            for(size_t index = 0; index < animInfo->mNumPositionKeys-1; index++) {
                auto key = animInfo->mPositionKeys[index+1];
                if(time <= key.mTime) {
                    translation = {key.mValue.x, key.mValue.y, key.mValue.z};
                    break;
                }
            }


            for(size_t index = 0; index < animInfo->mNumScalingKeys-1; index++) {
                auto key = animInfo->mScalingKeys[index+1];
                if(time <= key.mTime) {
                    scaling = {key.mValue.x, key.mValue.y, key.mValue.z};
                    break;
                }
            }

            rotation = glm::normalize(rotation);

            glm::mat4 translationMat = glm::translate(glm::mat4{1.0f}, translation);
            glm::mat4 rotationMat = glm::toMat4(rotation);
            glm::mat4 scalingMat = glm::scale(glm::mat4{1.0f}, scaling);
            nodeTransform = translationMat * rotationMat * scalingMat;
        }

        glm::mat4 boneOffset = (offsetMatrices.find(boneName)->second);
        globalTransform = parentMatrix * nodeTransform;
        keyframe.boneTransforms[boneID] = globalInverseTransform * globalTransform * boneOffset;
    } else {
        globalTransform = parentMatrix * nodeTransform;
    }

    for(int i = 0; i < armature->mNumChildren; i++) {
        updateKeyframeRecursively(keyframe, armature->mChildren[i], time, boneMapping, animationNodes, offsetMatrices, globalInverseTransform, globalTransform);
    }
}

std::vector<std::shared_ptr<Carrot::Mesh>> Carrot::Model::getSkinnedMeshes() const {
    std::vector<std::shared_ptr<Mesh>> result{};
    for(const auto& [material, meshes] : skinnedMeshes) {
        for(const auto& mesh : meshes) {
            result.push_back(mesh);
        }
    }
    return result;
}

std::vector<std::shared_ptr<Carrot::Mesh>> Carrot::Model::getStaticMeshes() const {
    std::vector<std::shared_ptr<Mesh>> result{};
    for(const auto& [material, meshes] : staticMeshes) {
        for(const auto& mesh : meshes) {
            result.push_back(mesh);
        }
    }
    return result;
}

Carrot::Buffer& Carrot::Model::getAnimationDataBuffer() {
    return *animationData;
}

