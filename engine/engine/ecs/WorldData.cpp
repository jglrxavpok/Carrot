//
// Created by jglrxavpok on 15/07/2023.
//

#include "WorldData.h"
#include <engine/render/Model.h>

namespace Carrot::ECS {
    void WorldData::clear() {
        modelRenderers.clear();
        modelRendererLookup.clear();
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

    std::shared_ptr<Carrot::Render::ModelRenderer> WorldData::findMatchingModelRenderer(const Carrot::Model& model, const Render::MaterialOverrides& overrides) const {
        const auto& source = model.getOriginatingResource();
        if(!source.isFile()) {
            return nullptr;
        }

        Carrot::IO::VFS::Path vfsPath { source.getName() };
        auto matchingModel = modelRendererLookup.find(vfsPath);
        if(matchingModel == modelRendererLookup.end()) {
            return nullptr;
        }

        auto matchingOverrides = matchingModel->second.find(overrides);
        if(matchingOverrides == matchingModel->second.end()) {
            return nullptr;
        }

        return matchingOverrides->second.lock();
    }

    void WorldData::storeModelRenderer(std::shared_ptr<Carrot::Render::ModelRenderer> value) {
        const Carrot::UUID& id = value->uuid;
        verify(modelRenderers.find(id) == modelRenderers.end(), Carrot::sprintf("A model renderer with the id %s already exists", id.toString().c_str()));
        modelRenderers[id] = value;

        const auto& source = value->getModel().getOriginatingResource();
        if(!source.isFile()) {
            return;
        }

        Carrot::IO::VFS::Path vfsPath { source.getName() };
        modelRendererLookup[vfsPath][value->getOverrides()] = value;
    }

    void WorldData::removeModelRenderer(const Carrot::UUID& id) {
        std::shared_ptr<Render::ModelRenderer> renderer = modelRenderers[id];
        if(!renderer) {
            return;
        }
        modelRenderers.erase(id);

        const auto& source = renderer->getModel().getOriginatingResource();
        if(!source.isFile()) {
            return;
        }

        Carrot::IO::VFS::Path vfsPath { source.getName() };
        modelRendererLookup[vfsPath].erase(renderer->getOverrides());
    }

    void WorldData::loadFromJSON(const rapidjson::Value& json) {
        clear();

        if(json.HasMember("model_renderers")) {
            for(auto& [key, obj] : json["model_renderers"].GetObject()) {
                const Carrot::UUID id = Carrot::UUID::fromString(std::string_view{ key.GetString(), key.GetStringLength() });
                std::shared_ptr<Render::ModelRenderer> renderer = Render::ModelRenderer::fromJSON(obj);
                renderer->uuid = id;
                if(renderer) { // can be null if not valid
                    storeModelRenderer(renderer);
                }
            }
        }
    }

    rapidjson::Value WorldData::toJSON(rapidjson::Document::AllocatorType& allocator) const {
        rapidjson::Value result{ rapidjson::kObjectType };

        rapidjson::Value modelRenderersObj{ rapidjson::kObjectType };
        for(const auto& [id, pRenderer] : modelRenderers) {
            if(!pRenderer) {
                continue;
            }
            modelRenderersObj.AddMember(rapidjson::Value{id.toString().c_str(), allocator}, pRenderer->toJSON(allocator), allocator);
        }

        result.AddMember("model_renderers", modelRenderersObj, allocator);
        return result;
    }
} // Carrot::ECS