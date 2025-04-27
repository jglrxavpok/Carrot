//
// Created by jglrxavpok on 27/10/2022.
//

#include "AssimpLoader.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <core/utils/stringmanip.h>
#include <core/utils/Profiling.h>
#include <core/io/Logging.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <assimp/IOSystem.hpp>
#include <assimp/IOStream.hpp>
#include <core/async/Counter.h>
#include <set>

namespace Carrot::Render {
    static glm::mat4 glmMat4FromAssimp(const aiMatrix4x4& assimpMatrix) {
        return glm::transpose(glm::make_mat4(&assimpMatrix.a1));
    }

    LoadedScene&& AssimpLoader::load(const std::string& filepath, Assimp::Importer& importer) {
        const aiScene* scene = nullptr;
        {
            ZoneScopedN("Read file - Assimp");
            scene = importer.ReadFile(filepath, aiProcess_Triangulate | aiProcess_OptimizeMeshes | aiProcess_CalcTangentSpace | aiProcess_FlipUVs);
        }

        if(!scene) {
            throw std::runtime_error(Carrot::sprintf("Failed to load model "+filepath+". Error is: %s", importer.GetErrorString()).c_str());
        }

        for(std::size_t materialIndex = 0; materialIndex < scene->mNumMaterials; materialIndex++) {
            const aiMaterial* mat = scene->mMaterials[materialIndex];

            auto& material = currentScene.materials.emplace_back();

            auto setMaterialTexture = [&](Carrot::IO::VFS::Path& toSet, aiTextureType textureType) -> bool {
                int count = mat->GetTextureCount(textureType);
                if(count > 0) {
                    if(count > 1) {
                        Carrot::Log::warn("Carrot AssimpLoader does not support more than a single texture per texture type (Material: %s, TextureType: %d)", mat->GetName().C_Str(), textureType);
                    }

                    aiString texturePath;
                    if(mat->GetTexture(textureType, 0, &texturePath) == AI_SUCCESS) {
                        Carrot::IO::VFS::Path path = Carrot::IO::VFS::Path { filepath };
                        toSet = path.relative(std::string_view(texturePath.data, texturePath.length));
                    }

                    return true;
                } else {
                    return false;
                }
            };

            if(!setMaterialTexture(material.albedo, aiTextureType_BASE_COLOR)) {
                setMaterialTexture(material.albedo, aiTextureType_DIFFUSE);
            }
            if(!setMaterialTexture(material.normalMap, aiTextureType_NORMAL_CAMERA)) {
                setMaterialTexture(material.normalMap, aiTextureType_NORMALS);
            }

            // TODO: metallic + roughness
            // TODO: emissive

            aiColor4D diffuse;
            if (AI_SUCCESS == aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &diffuse)) {
                material.baseColorFactor.r = diffuse.r;
                material.baseColorFactor.g = diffuse.g;
                material.baseColorFactor.b = diffuse.b;
                material.baseColorFactor.a = diffuse.a;
            }
        }

        for(std::size_t meshIndex = 0; meshIndex < scene->mNumMeshes; meshIndex++) {
            const aiMesh* mesh = scene->mMeshes[meshIndex];

            ZoneScopedN("Loading mesh");
            ZoneText(mesh->mName.C_Str(), mesh->mName.length);

            auto& primitive = currentScene.primitives.emplace_back();
            loadMesh(primitive, meshIndex, mesh);
            bool hasBones = mesh->HasBones();
            primitive.name = filepath+" ("+std::string(mesh->mName.C_Str(), mesh->mName.length)+")";

            primitive.materialIndex = mesh->mMaterialIndex;
            primitive.isSkinned = hasBones;
        }

        aiNode* armature = scene->mRootNode;
        loadSkeleton(armature);

        if(scene->HasAnimations()) {
            loadAnimations(scene, armature);
        }

        return std::move(currentScene);
    }

    void AssimpLoader::loadMesh(LoadedPrimitive& primitive, std::size_t meshIndex, const aiMesh* mesh) {
        std::vector<Vertex>& vertices = primitive.vertices;
        std::vector<SkinnedVertex>& skinnedVertices = primitive.skinnedVertices;
        std::vector<std::uint32_t>& indices = primitive.indices;

        bool usesSkinning = mesh->HasBones();

        if(usesSkinning) {
            skinnedVertices.reserve(mesh->mNumVertices);
        } else {
            vertices.reserve(mesh->mNumVertices);
        }
        indices.reserve(mesh->mNumFaces * 3);

        primitive.minPos = glm::vec3{INFINITY};
        primitive.maxPos = glm::vec3{-INFINITY};
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
                primitive.hadTexCoords = true;
            }

            glm::vec3 normal = {
                    vertexNormal.x,
                    vertexNormal.y,
                    vertexNormal.z,
            };
            primitive.hadNormals = true;

            glm::vec4 tangent = {
                    vertexTangent.x,
                    vertexTangent.y,
                    vertexTangent.z,
                    1.0f, // TODO: compute proper sign, use mBitangents
            };
            primitive.hadTangents = true;

            glm::vec4 position = {vec.x, vec.y, vec.z, 1.0f};
            primitive.minPos = glm::min(primitive.minPos, glm::vec3{ position.xyz() });
            primitive.maxPos = glm::max(primitive.maxPos, glm::vec3{ position.xyz() });

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

                currentScene.boneMapping[meshIndex][bone->mName.data] = boneIndex;
                currentScene.offsetMatrices[meshIndex][bone->mName.data] = glmMat4FromAssimp(bone->mOffsetMatrix);
            }

            const std::size_t vertexCount = skinnedVertices.size();
            for(std::size_t vertexIndex = 0; vertexIndex < vertexCount; vertexIndex++) {
                skinnedVertices[vertexIndex].normalizeWeights();
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
    }

    void AssimpLoader::updateKeyframeRecursively(Carrot::Keyframe& keyframe, const aiNode* armature, float time, const std::unordered_map<std::string, aiNodeAnim*>& animationNodes, const glm::mat4& globalInverseTransform, const glm::mat4& parentMatrix) {
        if(currentScene.nodeHierarchy == nullptr)
            return;
        const int meshIndex = 0; // TODO: rework to work with multiple meshes & skeletons


        std::string boneName = armature->mName.data;
        auto potentialMapping = currentScene.boneMapping[meshIndex].find(boneName);
        auto potentialAnim = animationNodes.find(boneName);
        glm::mat4 nodeTransform = glmMat4FromAssimp(armature->mTransformation);
        glm::mat4 globalTransform{1.0f};
        if(potentialMapping != currentScene.boneMapping[meshIndex].end()) {
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

            const glm::mat4& boneOffset = (currentScene.offsetMatrices[meshIndex].at(boneName));
            globalTransform = parentMatrix * nodeTransform;
            keyframe.boneTransforms[boneID] = globalInverseTransform * globalTransform * boneOffset;
        } else {
            globalTransform = parentMatrix * nodeTransform;
        }

        for(int i = 0; i < armature->mNumChildren; i++) {
            updateKeyframeRecursively(keyframe, armature->mChildren[i], time, animationNodes, globalInverseTransform, globalTransform);
        }
    }

    void AssimpLoader::loadAnimations(const aiScene *scene, const aiNode *armature) {
        glm::mat4 globalInverseTransform = glm::inverse(glmMat4FromAssimp(armature->mTransformation));
        auto& allAnimations = currentScene.animationData;

        unsigned int sceneCount = scene->mNumAnimations;
        allAnimations.reserve(sceneCount);
        for(int animationIndex = 0; animationIndex < sceneCount; animationIndex++) {
            aiAnimation* anim = scene->mAnimations[animationIndex];
            std::string name = anim->mName.data;

            ZoneScopedN("Loading animation");
            ZoneText(name.c_str(), name.size());

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
            animation.keyframeCount = timestampSet.size();
            animation.keyframes.resize(animation.keyframeCount);
            for(auto& keyframe : animation.keyframes) {
                keyframe.boneTransforms.resize(anim->mNumChannels, glm::mat4{1.0f});
            }

            animation.duration = anim->mDuration / anim->mTicksPerSecond;

            std::vector<float> timestamps = {timestampSet.begin(), timestampSet.end()};
            for(size_t index = 0; index < timestamps.size(); index++) {
                float time = timestamps[index];
                auto& keyframe = animation.keyframes[index];
                keyframe.timestamp = time / anim->mTicksPerSecond;
                this->updateKeyframeRecursively(keyframe, armature, time, animationNodes, globalInverseTransform);
            }
            currentScene.animationMapping[name] = animationIndex;
        }
    }

    void AssimpLoader::loadSubSkeleton(aiNode* subSkeleton, Render::SkeletonTreeNode& parent) {
        parent.bone.name = std::string(subSkeleton->mName.data, subSkeleton->mName.length);
        parent.bone.transform = glmMat4FromAssimp(subSkeleton->mTransformation);
        parent.bone.originalTransform = parent.bone.transform;

        std::size_t meshCount = subSkeleton->mNumMeshes;
        if(meshCount > 0) {
            parent.meshIndices = std::vector<std::size_t>{};
            parent.meshIndices->resize(subSkeleton->mNumMeshes);
            for(std::size_t i = 0; i < subSkeleton->mNumMeshes; i++) {
                parent.meshIndices.value()[i] = subSkeleton->mMeshes[i];
            }
        }

        for(std::size_t i = 0; i < subSkeleton->mNumChildren; i++) {
            loadSubSkeleton(subSkeleton->mChildren[i], parent.newChild());
        }
    }

    void AssimpLoader::loadSkeleton(aiNode* armature) {
        verify(armature != nullptr, Carrot::sprintf("Cannot load non-existent skeleton (%s)", currentScene.debugName.c_str()));
        currentScene.nodeHierarchy = std::make_unique<Render::Skeleton>(glm::inverse(glmMat4FromAssimp(armature->mTransformation)));
        loadSubSkeleton(armature, currentScene.nodeHierarchy->hierarchy);
    }
}