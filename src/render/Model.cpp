//
// Created by jglrxavpok on 30/11/2020.
//

#include "Model.h"
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/material.h>
#include "render/Mesh.h"
#include "render/Material.h"
#include <iostream>
#include <utils/stringmanip.h>
#include "render/Pipeline.h"
#include "render/VertexFormat.h"
#include "render/Vertex.h"
#include "render/Buffer.h"
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_operation.hpp>
#include <utils/conversions.h>

Carrot::Model::Model(Carrot::Engine& engine, const string& filename): engine(engine) {
    Assimp::Importer importer{};
    const aiScene* scene = importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_OptimizeMeshes | aiProcess_FlipUVs);

    if(!scene) {
        throw runtime_error("Failed to load model "+filename);
    }

    cout << "Loading model " << filename << endl;

    map<uint32_t, shared_ptr<Material>> materialMap{};
    for(size_t materialIndex = 0; materialIndex < scene->mNumMaterials; materialIndex++) {
        const aiMaterial* mat = scene->mMaterials[materialIndex];
        string materialName = mat->GetName().data;

        // TODO: remove ugly hack, maybe due to Assimp creating a default material?
        if(materialName == "DefaultMaterial") {
            continue;
        }

        auto material = engine.getOrCreateMaterial(materialName);
        materials.emplace_back(material);
        meshes[material.get()] = {};
        materialMap[materialIndex] = material;
    }

    unordered_map<string, uint32_t> boneMapping{};
    unordered_map<string, glm::mat4> offsetMatrices{};
    aiNode* armature = scene->mRootNode;

    for(size_t meshIndex = 0; meshIndex < scene->mNumMeshes; meshIndex++) {
        const aiMesh* mesh = scene->mMeshes[meshIndex];
        auto material = materialMap[mesh->mMaterialIndex];
        auto loadedMesh = loadMesh(material->getPipeline().getVertexFormat(), mesh, boneMapping, offsetMatrices);

        meshes[material.get()].push_back(loadedMesh);
    }

    if(scene->HasAnimations()) {
        loadAnimations(engine, scene, boneMapping, offsetMatrices, armature);
    }
}

void Carrot::Model::loadAnimations(Carrot::Engine& engine, const aiScene *scene,
                           const unordered_map<string, uint32_t>& boneMapping, const unordered_map<string, glm::mat4>& offsetMatrices, const aiNode *armature) {

    glm::mat4 globalInverseTransform = glm::inverse(glmMat4FromAssimp(armature->mTransformation));

    // load animations into staging buffer
    vector<Animation> allAnimations{};
    for(int animationIndex = 0; animationIndex < scene->mNumAnimations; animationIndex++) {
        aiAnimation* anim = scene->mAnimations[animationIndex];
        string name = anim->mName.data;

        // collect all timestamps
        set<float> timestampSet{};
        unordered_map<string, aiNodeAnim*> animationNodes{};
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
            throw runtime_error("Too many keyframes: "+to_string(timestampSet.size()));
        }
        animation.keyframeCount = timestampSet.size();
        animation.duration = anim->mDuration / anim->mTicksPerSecond;

        vector<float> timestamps = {timestampSet.begin(), timestampSet.end()};
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
    this->animationData = make_unique<Carrot::Buffer>(engine,
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
        .stageFlags = vk::ShaderStageFlagBits::eVertex,
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

    vector<vk::DescriptorSetLayout> layouts = {engine.getSwapchainImageCount(), *animationSetLayout};
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
    vector<vk::WriteDescriptorSet> writes{engine.getSwapchainImageCount()};
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

void Carrot::Model::draw(const uint32_t imageIndex, vk::CommandBuffer& commands, const Carrot::Buffer& instanceData, uint32_t instanceCount) {
    commands.bindVertexBuffers(1, instanceData.getVulkanBuffer(), {0});
    for(const auto& material : materials) {
        material->bindForRender(imageIndex, commands);
        if(!animations.empty()) {
            material->getPipeline().bindDescriptorSets(commands, {animationDescriptorSets[imageIndex]}, {}, 1);
        }

        for(const auto& mesh : meshes[material.get()]) {
            mesh->bind(commands);
            mesh->draw(commands, instanceCount);
        }
    }
}

shared_ptr<Carrot::Mesh> Carrot::Model::loadMesh(Carrot::VertexFormat vertexFormat, const aiMesh* mesh, unordered_map<string, uint32_t>& boneMapping, unordered_map<string, glm::mat4>& offsetMatrices) {
    vector<Vertex> vertices{};
    vector<SkinnedVertex> skinnedVertices{};
    vector<uint32_t> indices{};

    bool usesSkinning = vertexFormat == VertexFormat::SkinnedVertex;

    for(size_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; vertexIndex++) {
        const aiVector3D vec = mesh->mVertices[vertexIndex];
        const aiColor4D* vertexColor = mesh->mColors[0];
        const aiVector3D* vertexUVW = mesh->mTextureCoords[0];
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

        glm::vec3 position = {vec.x, vec.y, vec.z};

        if(usesSkinning) {
            skinnedVertices.push_back({
                                        position,
                                        color,
                                        uv,
                                        glm::ivec4{-1, -1, -1, -1},
                                        glm::vec4{0.0f},
                               });
        } else {
            vertices.push_back({
                                        position,
                                        color,
                                        uv,
                               });
        }
    }

    if(usesSkinning) {
        for(size_t boneIndex = 0; boneIndex < mesh->mNumBones; boneIndex++) {
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

    for(size_t faceIndex = 0; faceIndex < mesh->mNumFaces; faceIndex++) {
        const aiFace face = mesh->mFaces[faceIndex];
        if(face.mNumIndices != 3) {
            throw runtime_error("Can only load triangles.");
        }

        indices.push_back(face.mIndices[0]);
        indices.push_back(face.mIndices[1]);
        indices.push_back(face.mIndices[2]);
    }

    if(usesSkinning) {
        return make_shared<Mesh>(engine, skinnedVertices, indices);
    } else {
        return make_shared<Mesh>(engine, vertices, indices);
    }
}

void Carrot::Model::updateKeyframeRecursively(Carrot::Keyframe& keyframe, const aiNode* armature, float time, const unordered_map<string, uint32_t>& boneMapping, const unordered_map<string, aiNodeAnim*>& animationNodes, const unordered_map<string, glm::mat4>& offsetMatrices, const glm::mat4& globalInverseTransform, const glm::mat4& parentMatrix) {
    if(armature == nullptr)
        return;
    string boneName = armature->mName.data;
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

