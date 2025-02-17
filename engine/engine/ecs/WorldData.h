//
// Created by jglrxavpok on 15/07/2023.
//

#pragma once

#include <core/io/Document.h>
#include <rapidjson/document.h>
#include <engine/render/ModelRenderer.h>

namespace Carrot::ECS {

    /**
     * Per world data.
     * Used to share data between components & systems, avoiding unnecessary computations and memory allocations.
     * Unused data is cleared each frame
     */
    class WorldData {
    public:
        enum class Type {
            ModelRenderer,
        };

        /**
         * Remove everything inside this structure
         */
        void clear();

        /**
         * Removes unused shared data
         */
        void update();

    public: // access
        /**
         * Can return null if none found
         */
        std::shared_ptr<Carrot::Render::ModelRenderer> loadModelRenderer(const Carrot::UUID& id) const;

        std::shared_ptr<Carrot::Render::ModelRenderer> findMatchingModelRenderer(const Carrot::Model& model, const Render::MaterialOverrides& overrides) const;

        void storeModelRenderer(std::shared_ptr<Carrot::Render::ModelRenderer> value);

        void removeModelRenderer(const Carrot::UUID& id);

    public: // serialisation
        /**
         * Loads this structure from the provided JSON.
         * Previous data is deleted
         */
        void deserialise(const Carrot::DocumentElement& doc);
        Carrot::DocumentElement serialise() const;

    private:
        std::unordered_map<Carrot::UUID, std::shared_ptr<Carrot::Render::ModelRenderer>> modelRenderers;

        // model path -> which overrides -> the renderer
        std::unordered_map<Carrot::IO::VFS::Path, std::unordered_map<Render::MaterialOverrides, std::weak_ptr<Carrot::Render::ModelRenderer>>> modelRendererLookup;
    };

} // Carrot::ECS
