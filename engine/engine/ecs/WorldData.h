//
// Created by jglrxavpok on 15/07/2023.
//

#pragma once

#include <core/async/Counter.h>
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

        void storeModelRenderer(std::shared_ptr<Carrot::Render::ModelRenderer> value);

        void removeModelRenderer(const Carrot::UUID& id);

    public: // serialisation
        /**
         * Loads this structure from the provided JSON.
         * Previous data is deleted
         *
         * @param loadingCounter counter incremented when starting loading, will be decremented when finished
         */
        void deserialiseAndQueueLoading(const Carrot::DocumentElement& doc, Carrot::Async::Counter& loadingCounter);
        Carrot::DocumentElement serialise() const;

    private:
        std::unordered_map<Carrot::UUID, std::shared_ptr<Carrot::Render::ModelRenderer>> modelRenderers;
    };

} // Carrot::ECS
