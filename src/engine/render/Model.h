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

namespace Carrot {
    class Mesh;
    class Engine;
    class Material;
    class Buffer;

    class Model {
    public:
        using Ref = std::shared_ptr<Model>;

        explicit Model(Carrot::Engine& engine, const Carrot::IO::Resource& filename);

        void draw(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands, const Carrot::Buffer& instanceData, std::uint32_t instanceCount, const Carrot::UUID& entityID = Carrot::UUID::null());

        ///
        /// \param imageIndex
        /// \param commands
        /// \param instanceData
        /// \param indirectDrawCommands a buffer per mesh/material
        /// \param drawCount
        void indirectDraw(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands, const Carrot::Buffer& instanceData, const std::map<Carrot::MeshID, std::shared_ptr<Carrot::Buffer>>& indirectDrawCommands, std::uint32_t drawCount, const Carrot::UUID& entityID = Carrot::UUID::null());

        [[nodiscard]] std::vector<std::shared_ptr<Carrot::Mesh>> getMeshes() const;

        [[nodiscard]] const std::map<MaterialID, std::vector<std::shared_ptr<Mesh>>> getMaterialToMeshMap() const;

        Carrot::Buffer& getAnimationDataBuffer();

    public:
        const Carrot::IO::Resource& getOriginatingResource() const { return resource; }

    private:
        Carrot::Engine& engine;
        std::map<Material*, std::vector<std::shared_ptr<Mesh>>> meshes{};
        std::vector<std::shared_ptr<Material>> materials{};

        // TODO: move animations somewhere else?
        std::map<std::string, Animation*> animations{};
        std::map<std::string, std::uint32_t> animationMapping{};
        std::unique_ptr<Buffer> animationData = nullptr;
        vk::UniqueDescriptorSetLayout animationSetLayout{};
        vk::UniqueDescriptorPool animationSetPool{};
        vk::UniqueDescriptorSet animationSet{};
        std::vector<vk::DescriptorSet> animationDescriptorSets{};

        Carrot::IO::Resource resource; // resource from which this model comes. Used for serialisation

        std::shared_ptr<Mesh> loadMesh(Carrot::VertexFormat vertexFormat, const aiMesh* mesh, std::unordered_map<std::string, std::uint32_t>& boneMapping, std::unordered_map<std::string, glm::mat4>& offsetMatrices);

        void updateKeyframeRecursively(Keyframe& keyframe, const aiNode* armature, float time, const std::unordered_map<std::string, uint32_t>& boneMapping, const std::unordered_map<std::string, aiNodeAnim*>& animationNodes, const std::unordered_map<std::string, glm::mat4>& offsetMatrices, const glm::mat4& globalTransform, const glm::mat4& parentMatrix = glm::mat4{1.0f});

        void loadAnimations(Carrot::Engine& engine, const aiScene *scene,
                            const std::unordered_map<std::string, uint32_t>& boneMapping, const std::unordered_map<std::string, glm::mat4>& offsetMatrices, const aiNode *armature);

    };
}
