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
#include "engine/render/resources/Skeleton.h"
#include "engine/render/animation/Animation.h"
#include "engine/Engine.h"
#include "engine/render/MaterialSystem.h"
#include "engine/render/PassEnum.h"
#include "engine/render/InstanceData.h"
#include "engine/render/resources/model_loading/LoadedScene.h"
#include "IDTypes.h"

namespace Carrot {
    class Mesh;
    class SingleMesh;
    class Engine;
    class Material;
    class Buffer;
    class AnimatedInstances;

    class BLASHandle;

    namespace Render {
        class Packet;
    }

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

        [[nodiscard]] std::vector<std::shared_ptr<Carrot::Mesh>> getStaticMeshes() const;

        // [materialSlot] = list of meshes using that material
        [[nodiscard]] std::unordered_map<std::uint32_t, std::vector<std::shared_ptr<Carrot::Mesh>>> getSkinnedMeshes() const;

        Carrot::Buffer& getAnimationDataBuffer();

        std::shared_ptr<BLASHandle> getStaticBLAS();
        std::shared_ptr<BLASHandle> getSkinnedBLAS();

    public:
        bool hasSkeleton() const;

        //! Gets the skeleton inside this model. Due to how Assimp works, don't expect only bone nodes, the entire scene hierarchy will be present.
        Render::Skeleton& getSkeleton();

        //! Gets the skeleton inside this model. Due to how Assimp works, don't expect only bone nodes, the entire scene hierarchy will be present.
        const Render::Skeleton& getSkeleton() const;

        //! MeshIndex -> Name -> Bone Index mapping
        const std::unordered_map<int, std::unordered_map<std::string, std::uint32_t>>& getBoneMapping() const;

        //! MeshIndex -> Name -> Bone Offset matrix
        const std::unordered_map<int, std::unordered_map<std::string, glm::mat4>>& getBoneOffsetMatrices() const;

    public:
        void renderStatic(const Render::Context& renderContext, const InstanceData& instanceData = {}, Render::PassEnum renderPass = Render::PassEnum::OpaqueGBuffer);
        void renderSkinned(const Render::Context& renderContext, const AnimatedInstanceData& instanceData = {}, Render::PassEnum renderPass = Render::PassEnum::OpaqueGBuffer);

    public:
        const Carrot::IO::Resource& getOriginatingResource() const { return resource; }

    private:
        struct MeshAndTransform {
            std::shared_ptr<Mesh> mesh;
            glm::mat4 transform{1.0f};
        };

        Carrot::Engine& engine;
        std::string debugName;
        std::shared_ptr<Carrot::Pipeline> staticMeshesPipeline;
        std::shared_ptr<Carrot::Pipeline> skinnedMeshesPipeline;
        std::unordered_map<std::uint32_t, std::vector<MeshAndTransform>> staticMeshes{};
        std::unordered_map<std::uint32_t, std::vector<MeshAndTransform>> skinnedMeshes{};
        std::vector<std::shared_ptr<Render::MaterialHandle>> materials{};

        std::shared_ptr<BLASHandle> staticBLAS;
        std::shared_ptr<BLASHandle> skinnedBLAS;

        // TODO: move animations somewhere else?
        std::unique_ptr<Render::Skeleton> skeleton;
        std::unordered_map<int, std::unordered_map<std::string, std::uint32_t>> boneMapping;
        std::unordered_map<int, std::unordered_map<std::string, glm::mat4>> offsetMatrices;

        std::map<std::string, Animation*> animations{};
        std::map<std::string, std::uint32_t> animationMapping{};
        std::unique_ptr<Buffer> animationData = nullptr;
        vk::UniqueDescriptorSetLayout animationSetLayout{};
        vk::UniqueDescriptorPool animationSetPool{};
        vk::UniqueDescriptorSet animationSet{};
        std::vector<vk::DescriptorSet> animationDescriptorSets{};

        Carrot::IO::Resource resource; // resource from which this model comes. Used for serialisation

        friend class Carrot::AnimatedInstances;
    };
}
