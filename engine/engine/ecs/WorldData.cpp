//
// Created by jglrxavpok on 15/07/2023.
//

#include "WorldData.h"
#include <engine/render/Model.h>

namespace Carrot::ECS {
    void WorldData::clear() {
        modelRenderers.clear();
    }

    void WorldData::update() {
        for(auto it = modelRenderers.begin(); it != modelRenderers.end();) {
            if(it->second.use_count() <= 1) {
                it = modelRenderers.erase(it);
            } else {
                it++;
            }
        }
    }

    std::shared_ptr<Carrot::Render::ModelRenderer> WorldData::loadModelRenderer(const Carrot::UUID& id) const {
        auto it = modelRenderers.find(id);
        if(it == modelRenderers.end()) {
            return nullptr;
        }
        return it->second;
    }

    void WorldData::storeModelRenderer(std::shared_ptr<Carrot::Render::ModelRenderer> value) {
        const Carrot::UUID& id = value->uuid;
        verify(modelRenderers.find(id) == modelRenderers.end(), Carrot::sprintf("A model renderer with the id %s already exists", id.toString().c_str()));
        modelRenderers[id] = value;
    }

    void WorldData::removeModelRenderer(const Carrot::UUID& id) {
        std::shared_ptr<Render::ModelRenderer> renderer = modelRenderers[id];
        if(!renderer) {
            return;
        }
        modelRenderers.erase(id);
    }

    void WorldData::deserialiseAndQueueLoading(const Carrot::DocumentElement& doc, Async::Counter& loadingCounter) {
        clear();

        if(doc.contains("model_renderers")) {
            for(auto& [key, obj] : doc["model_renderers"].getAsObject()) {
                const Carrot::UUID id = Carrot::UUID::fromString(key);
                std::shared_ptr<Render::ModelRenderer> renderer = Render::ModelRenderer::asyncDeserialise(obj, loadingCounter);
                renderer->uuid = id;
                if(renderer) { // can be null if not valid
                    storeModelRenderer(renderer);
                }
            }
        }
    }

    Carrot::DocumentElement WorldData::serialise() const {
        Carrot::DocumentElement result;

        Carrot::DocumentElement modelRenderersObj;
        for(const auto& [id, pRenderer] : modelRenderers) {
            if(!pRenderer) {
                continue;
            }
            modelRenderersObj[id.toString()] = pRenderer->serialise();
        }

        result["model_renderers"] = modelRenderersObj;
        return result;
    }
} // Carrot::ECS