//
// Created by jglrxavpok on 15/07/2023.
//

#pragma once

#include <core/io/Document.h>
#include <rapidjson/document.h>
#include <engine/render/MeshAndTransform.h>
#include <engine/render/InstanceData.h>
#include <engine/render/GBufferDrawData.h>
#include <engine/render/PassEnum.h>
#include <engine/render/RenderContext.h>
#include <engine/render/resources/Pipeline.h>
#include <engine/render/MaterialSystem.h>

#include <engine/render/RenderPacket.h>

namespace Carrot {
    class Model;
}

namespace Carrot::Render {
    struct ClusterModel;
    class ModelRenderer;

    struct MaterialOverride {
        std::size_t meshIndex = 0;

        bool virtualizedGeometry = false;

        std::shared_ptr<Carrot::Pipeline> pipeline; // can be null if only textures are changed
        std::shared_ptr<Carrot::Render::MaterialHandle> materialTextures; // can be null if only pipeline is changed
        // cannot be both null

        bool operator==(const MaterialOverride& other) const;
        std::size_t hash() const;
    };

    class MaterialOverrides {
    public:
        MaterialOverrides() = default;

        const MaterialOverride* begin() const;
        const MaterialOverride* end() const;

        MaterialOverride* begin();
        MaterialOverride* end();
        std::size_t size() const;

        /**
         * Tries to find an override with a matching meshIndex. Returns nullptr if none were found.
         * If modified, requires a call to 'recreateStructures' on ModelRenderer
         */
        MaterialOverride* findForMesh(std::size_t meshIndex);

        /**
         * Tries to find an override with a matching meshIndex. Returns nullptr if none were found
         */
        const MaterialOverride* findForMesh(std::size_t meshIndex) const;

        /**
         * Call when modifying a MaterialOverride from this object directly.
         */
        void sort();

    public:
        bool operator==(const MaterialOverrides& other) const;
        std::size_t hash() const;

    private:
        void add(const MaterialOverride& override);
        void remove(std::size_t index);

    private:
        std::vector<MaterialOverride> sortedOverrides; // sorted by ascending meshIndex

        friend class ModelRenderer;
    };

    struct MeshRenderingInfo {
        MeshAndTransform meshAndTransform;
        std::shared_ptr<Carrot::Render::MaterialHandle> materialTextures;
    };

    struct PipelineBucket {
        bool virtualizedGeometry = false;
        std::shared_ptr<Carrot::Pipeline> pipeline;

        std::vector<Render::PacketCommand> drawCommands;
        std::vector<Carrot::InstanceData> instanceData;
        std::vector<Carrot::GBufferDrawData> drawData; // contains index of material

        std::vector<MeshRenderingInfo> meshes;
    };

    struct ModelRendererStorage {
        std::unordered_map<Viewport*, std::shared_ptr<ClusterModel>> clusterModelsPerViewport;
        const ModelRenderer* pCreator = nullptr;

        ModelRendererStorage clone() const;
    };

    /**
     * Used to render a Carrot::Model. Supports overriding materials & serialisation (for use in ECS).
     * Carrot::Model are shared between components and entities, that's why there needs to be an additional layer of indirection
     */
    class ModelRenderer {
    public:
        Carrot::UUID uuid{};

        explicit ModelRenderer(Model& model);

        struct NoInitTag {};
        ModelRenderer(Model& model, NoInitTag);

        static std::shared_ptr<ModelRenderer> deserialise(const Carrot::DocumentElement& doc);
        Carrot::DocumentElement serialise();

        /**
         * Deep clone of this object. You must call "recreateStructures" again before use
         * @return
         */
        std::shared_ptr<ModelRenderer> clone() const;

    public:
        void render(ModelRendererStorage& storage, const Render::Context& renderContext, const InstanceData& instanceData, Render::PassName renderPass) const;

    public:
        void addOverride(const MaterialOverride& override);
        void removeOverride(std::size_t index);

        MaterialOverrides& getOverrides();
        const MaterialOverrides& getOverrides() const;

        Carrot::Model& getModel();
        const Carrot::Model& getModel() const;

        /// called when overriding materials, to recompute internal structures which help rendering
        void recreateStructures();

    private:
        Model& model;
        MaterialOverrides overrides;

        // default pipelines
        std::shared_ptr<Carrot::Pipeline> opaqueMeshesPipeline;
        std::shared_ptr<Carrot::Pipeline> transparentMeshesPipeline;

        std::vector<PipelineBucket> buckets;
        bool hasVirtualizedGeometry = false;
    };

} // Carrot::Render

template<>
struct std::hash<Carrot::Render::MaterialOverrides> {
    std::size_t operator()(const Carrot::Render::MaterialOverrides& o) const {
        return o.hash();
    }
};
