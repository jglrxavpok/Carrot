//
// Created by jglrxavpok on 30/11/2020.
//

#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <assimp/scene.h>
#include "engine/render/resources/VertexFormat.h"
#include <core/render/Skeleton.h>
#include <core/render/Animation.h>
#include <core/math/Sphere.h>
#include "engine/Engine.h"
#include "engine/render/MaterialSystem.h"
#include "engine/render/PassEnum.h"
#include "engine/render/InstanceData.h"
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
        class ModelRenderer;
        class Packet;
    }

    /**
     * A model is a collection of meshes. To render a model, use ModelRenderer
     */
    class Model {
    public:
        using Ref = std::shared_ptr<Model>;

        struct MeshAndTransform {
            std::shared_ptr<Mesh> mesh;
            glm::mat4 transform{1.0f};
            Math::Sphere boundingSphere;

            std::size_t meshIndex = 0;
            std::size_t staticMeshIndex = 0;
        };

        struct StaticMeshInfo {
            std::size_t startVertex = 0;
            std::size_t vertexCount = 0;
            std::size_t startIndex = 0;
            std::size_t indexCount = 0;
        };

        explicit Model(Carrot::Engine& engine, const Carrot::IO::Resource& filename);
        ~Model();

        [[nodiscard]] std::vector<std::shared_ptr<Carrot::Mesh>> getStaticMeshes() const;

        // [materialSlot] = list of meshes using that material
        [[nodiscard]] std::unordered_map<std::uint32_t, std::vector<std::shared_ptr<Carrot::Mesh>>> getSkinnedMeshes() const;

        [[nodiscard]] const std::unordered_map<std::uint32_t, std::vector<MeshAndTransform>>& getStaticMeshesPerMaterial() const { return staticMeshes; }

        const StaticMeshInfo& getStaticMeshInfo(std::size_t staticMeshIndex) const;

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

        const Carrot::SingleMesh& getStaticMeshData() const;

        std::span<std::shared_ptr<Render::MaterialHandle>> getMaterials() { return materials; }
        std::span<const std::shared_ptr<Render::MaterialHandle>> getMaterials() const { return materials; }

    private:
        Carrot::Engine& engine;
        std::string debugName;
        std::shared_ptr<Carrot::Pipeline> opaqueMeshesPipeline;
        std::shared_ptr<Carrot::Pipeline> transparentMeshesPipeline;
        std::unordered_map<std::uint32_t, std::vector<MeshAndTransform>> staticMeshes{};
        std::unordered_map<std::uint32_t, std::vector<MeshAndTransform>> skinnedMeshes{};
        std::vector<std::shared_ptr<Render::MaterialHandle>> materials{};

        std::vector<StaticMeshInfo> staticMeshInfo;
        std::unique_ptr<Carrot::SingleMesh> staticMeshData;

        std::vector<vk::DrawIndexedIndirectCommand> staticOpaqueDrawCommands;
        std::vector<vk::DrawIndexedIndirectCommand> staticTransparentDrawCommands;
        std::vector<Carrot::InstanceData> staticOpaqueInstanceData;
        std::vector<Carrot::InstanceData> staticTransparentInstanceData;
        std::vector<Carrot::GBufferDrawData> staticOpaqueDrawData;
        std::vector<Carrot::GBufferDrawData> staticTransparentDrawData;

        std::size_t staticVertexCount = 0; // Count of all static vertices inside this model
        std::size_t staticVertexStart = 0; // Start of vertices inside staticMeshData
        std::size_t staticIndexCount = 0; // Count of all static indices inside this model

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

        Render::ModelRenderer* defaultRenderer = nullptr;

        friend class Carrot::AnimatedInstances;
    };
}
