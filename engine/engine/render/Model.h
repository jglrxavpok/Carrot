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
#include <core/scene/LoadedScene.h>

#include "engine/render/MaterialSystem.h"
#include "engine/render/PassEnum.h"
#include "engine/render/InstanceData.h"
#include "engine/render/MeshAndTransform.h"
#include "engine/render/GBufferDrawData.h"
#include "engine/render/resources/Pipeline.h"
#include "core/render/Meshlet.h"
#include "IDTypes.h"

namespace Carrot {
    class Mesh;
    class SingleMesh;
    class Engine;
    class Material;
    class Buffer;
    class AnimatedInstances;

    class BLASHandle;
    class TaskHandle;

    namespace Render {
        class MaterialHandle;
        struct ClustersTemplate;
        class ModelRenderer;
        struct ModelRendererStorage;
        class Packet;
    }

    /**
     * A model is a collection of meshes. To render a model, use ModelRenderer
     */
    class Model {
    public:
        using Ref = std::shared_ptr<Model>;

        struct StaticMeshInfo {
            std::vector<Render::Meshlet> meshlets;
            std::vector<std::uint32_t> meshletVertexIndices;
            std::vector<std::uint32_t> meshletIndices;
            std::size_t startVertex = 0;
            std::size_t vertexCount = 0;
            std::size_t startIndex = 0;
            std::size_t indexCount = 0;
            glm::mat4 transform{1.0f}; // copy of data inside MeshAndTransform
        };

        static std::shared_ptr<Model> load(TaskHandle& task, Carrot::Engine& engine, const Carrot::IO::Resource& filename);
        ~Model();

        [[nodiscard]] std::vector<std::shared_ptr<Carrot::Mesh>> getStaticMeshes() const;

        // [materialSlot] = list of meshes using that material
        [[nodiscard]] std::unordered_map<std::uint32_t, std::vector<std::shared_ptr<Carrot::Mesh>>> getSkinnedMeshes() const;

        [[nodiscard]] const std::unordered_map<std::uint32_t, std::vector<MeshAndTransform>>& getStaticMeshesPerMaterial() const { return staticMeshes; }

        const StaticMeshInfo& getStaticMeshInfo(std::size_t staticMeshIndex) const;

        Carrot::Buffer& getAnimationDataBuffer();

        std::shared_ptr<BLASHandle> getStaticBLAS();
        std::shared_ptr<BLASHandle> getSkinnedBLAS();

        std::shared_ptr<Render::ClustersTemplate> lazyLoadMeshletTemplate(std::size_t staticMeshIndex, const glm::mat4& transform);
        const Render::LoadedScene::PrecomputedBLASes& getPrecomputedBLASes(const Render::NodeKey& nodeKey) const;

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

        /**
         * Gets the corresponding animation data. Or nullptr if none match the given name.
         * Pointer is valid for entire lifetime of this Model instance
         */
        const AnimationMetadata* getAnimationMetadata(const std::string& animationName) const;
        const std::map<std::string, AnimationMetadata>& getAnimationMetadata() const;
        vk::DescriptorSet getAnimationDataDescriptorSet() const;

    public:
        void renderStatic(Render::ModelRendererStorage& rendererStorage, const Render::Context& renderContext, const InstanceData& instanceData = {}, Render::PassName renderPass = Render::PassEnum::OpaqueGBuffer);
        void renderSkinned(const Render::Context& renderContext, const AnimatedInstanceData& instanceData = {}, Render::PassName renderPass = Render::PassEnum::OpaqueGBuffer);

    public:
        const Carrot::IO::Resource& getOriginatingResource() const { return resource; }

        const Carrot::SingleMesh& getStaticMeshData() const;

        std::span<std::shared_ptr<Render::MaterialHandle>> getMaterials() { return materials; }
        std::span<const std::shared_ptr<Render::MaterialHandle>> getMaterials() const { return materials; }

    private:
        explicit Model(Carrot::Engine& engine, const Carrot::IO::Resource& filename);
        void loadInner(TaskHandle& task, Carrot::Engine& engine, const Carrot::IO::Resource& filename);

        /**
         * Generate an image with the bone transforms of the given animation.
         * See "engine/resources/shaders/compute/animation-skinning.compute.glsl" for more details
         */
        std::unique_ptr<Carrot::Render::Texture> generateBoneTransformsStorageImage(const Animation& animation);

        Carrot::Engine& engine;
        std::string debugName;
        std::shared_ptr<Carrot::Pipeline> opaqueMeshesPipeline;
        std::shared_ptr<Carrot::Pipeline> transparentMeshesPipeline;
        std::unordered_map<std::uint32_t, std::vector<MeshAndTransform>> staticMeshes{};
        std::unordered_map<std::uint32_t, std::vector<MeshAndTransform>> skinnedMeshes{};
        std::vector<std::shared_ptr<Render::MaterialHandle>> materials{};

        std::vector<Carrot::Vertex> staticVertices;
        std::vector<std::uint32_t> staticIndices;

        std::vector<StaticMeshInfo> staticMeshInfo;
        std::unique_ptr<Carrot::SingleMesh> staticMeshData;

        std::vector<vk::DrawIndexedIndirectCommand> staticOpaqueDrawCommands;
        std::vector<vk::DrawIndexedIndirectCommand> staticTransparentDrawCommands;
        std::vector<Carrot::InstanceData> staticOpaqueInstanceData;
        std::vector<Carrot::InstanceData> staticTransparentInstanceData;
        std::vector<Carrot::GBufferDrawData> staticOpaqueDrawData;
        std::vector<Carrot::GBufferDrawData> staticTransparentDrawData;

        std::shared_ptr<BLASHandle> staticBLAS;
        std::shared_ptr<BLASHandle> skinnedBLAS;

        // indexed by mesh's static mesh index
        Async::SpinLock meshletsLoading;
        Async::ParallelMap<std::size_t, std::shared_ptr<Render::ClustersTemplate>> meshletsPerStaticMesh;
        std::unordered_map<Render::NodeKey, Render::LoadedScene::PrecomputedBLASes> precomputedBLASes;

        // TODO: move animations somewhere else?
        std::unique_ptr<Render::Skeleton> skeleton;
        std::unordered_map<int, std::unordered_map<std::string, std::uint32_t>> boneMapping;
        std::unordered_map<int, std::unordered_map<std::string, glm::mat4>> offsetMatrices;

        std::map<std::string, AnimationMetadata> animationMapping{};
        std::vector<std::unique_ptr<Carrot::Render::Texture>> animationBoneTransformData;
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
