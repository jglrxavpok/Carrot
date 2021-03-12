//
// Created by jglrxavpok on 30/11/2020.
//

#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include "engine/vulkan/includes.h"
#include <assimp/scene.h>
#include "engine/render/resources/VertexFormat.h"
#include "engine/render/animation/Animation.h"
#include "engine/Engine.h"
#include "IDTypes.h"

using namespace std;

namespace Carrot {
    class Mesh;
    class Engine;
    class Material;
    class Buffer;

    class Model {
    private:
        Carrot::Engine& engine;
        map<Material*, vector<shared_ptr<Mesh>>> meshes{};
        vector<shared_ptr<Material>> materials{};

        // TODO: move animations somewhere else?
        map<string, Animation*> animations{};
        map<string, uint32_t> animationMapping{};
        unique_ptr<Buffer> animationData = nullptr;
        vk::UniqueDescriptorSetLayout animationSetLayout{};
        vk::UniqueDescriptorPool animationSetPool{};
        vk::UniqueDescriptorSet animationSet{};
        vector<vk::DescriptorSet> animationDescriptorSets{};

        shared_ptr<Mesh> loadMesh(Carrot::VertexFormat vertexFormat, const aiMesh* mesh, unordered_map<string, uint32_t>& boneMapping, unordered_map<string, glm::mat4>& offsetMatrices);

        void updateKeyframeRecursively(Keyframe& keyframe, const aiNode* armature, float time, const unordered_map<string, uint32_t>& boneMapping, const unordered_map<string, aiNodeAnim*>& animationNodes, const unordered_map<string, glm::mat4>& offsetMatrices, const glm::mat4& globalTransform, const glm::mat4& parentMatrix = glm::mat4{1.0f});

        void loadAnimations(Carrot::Engine& engine, const aiScene *scene,
                                   const unordered_map<string, uint32_t>& boneMapping, const unordered_map<string, glm::mat4>& offsetMatrices, const aiNode *armature);

    public:
        explicit Model(Carrot::Engine& engine, const string& filename);

        void draw(const uint32_t imageIndex, vk::CommandBuffer& commands, const Carrot::Buffer& instanceData, uint32_t instanceCount);

        ///
        /// \param imageIndex
        /// \param commands
        /// \param instanceData
        /// \param indirectDrawCommands a buffer per mesh/material
        /// \param drawCount
        void indirectDraw(const uint32_t imageIndex, vk::CommandBuffer& commands, const Carrot::Buffer& instanceData, const map<Carrot::MeshID, shared_ptr<Carrot::Buffer>>& indirectDrawCommands, uint32_t drawCount);

        [[nodiscard]] vector<shared_ptr<Carrot::Mesh>> getMeshes() const;

        [[nodiscard]] const map<MaterialID, vector<shared_ptr<Mesh>>> getMaterialToMeshMap() const;

        Carrot::Buffer& getAnimationDataBuffer();
    };
}
