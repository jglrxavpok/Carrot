//
// Created by jglrxavpok on 08/10/2021.
//

#include "ModelComponent.h"
#include "engine/Engine.h"
#include "core/utils/ImGuiUtils.hpp"
#include "imgui.h"
#include "engine/edition/DragDropTypes.h"
#include "engine/render/ModelRenderer.h"
#include "engine/render/ClusterManager.h"
#include "engine/ecs/World.h"
#include "engine/ecs/WorldData.h"
#include "core/utils/JSON.h"
#include <filesystem>
#include <string>
#include <core/io/DocumentHelpers.h>

namespace Carrot::ECS {

    ModelComponent::ModelComponent(const Carrot::DocumentElement& doc, Entity entity): ModelComponent::ModelComponent(std::move(entity)) {
        isTransparent = doc["isTransparent"].getAsBool();
        color = DocumentHelpers::read<4, float>(doc["color"]);

        if(doc.contains("castsShadows")) {
            castsShadows = doc["castsShadows"].getAsBool();
        }

        if(doc.contains("model")) {
            auto& modelData = doc["model"];

            if(modelData.contains("modelPath")) {
                std::string modelPath { modelData["modelPath"].getAsString() };
                setFile(IO::VFS::Path(modelPath));
            } else {
                TODO // cannot load non-file models at the moment
            }

            if(modelData.contains("model_renderer")) {
                Carrot::UUID rendererID = Carrot::UUID::fromString(modelData["model_renderer"].getAsString());
                WorldData& worldData = entity.getWorld().getWorldData();
                std::shared_ptr<Render::ModelRenderer> renderer = worldData.loadModelRenderer(rendererID);
                if(renderer != nullptr) {
                    modelRenderer = renderer;
                }
            }
        } else {
            TODO // missing model
        }
    }

    Carrot::DocumentElement ModelComponent::serialise() const {
        asyncModel.forceWait();

        Carrot::DocumentElement obj;

        obj["isTransparent"] = isTransparent;
        obj["color"] = DocumentHelpers::write(color);
        obj["castsShadows"] = castsShadows;

        Carrot::DocumentElement modelData;
        auto& resource = asyncModel->getOriginatingResource();

        if(resource.isFile()) {
            modelData["modelPath"] = resource.getName();
        }

        if(modelRenderer) {
            modelData["model_renderer"] = modelRenderer->uuid.toString();
        }

        obj["model"] = modelData;

        return obj;
    }

    void ModelComponent::loadTLASIfPossible() {
        if(!GetCapabilities().supportsRaytracing) {
            return;
        }
        if(tlasIsWaitingForModel && asyncModel.isReady()) {
            Async::LockGuard l{ tlasAccess };
            if(tlasIsWaitingForModel) {
                tlasIsWaitingForModel = false;
                tlas = GetRenderer().getRaytracingScene().addInstance(asyncModel->getStaticBLAS());
                tlas->enabled = true;
            }
        }
    }

    std::unique_ptr<Component> ModelComponent::duplicate(const Entity& newOwner) const {
        auto result = std::make_unique<ModelComponent>(newOwner);
        asyncModel.forceWait();
        result->asyncModel = std::move(AsyncModelResource(GetAssetServer().loadModelTask(Carrot::IO::VFS::Path { asyncModel->getOriginatingResource().getName() })));
        result->isTransparent = isTransparent;
        result->color = color;
        result->modelRenderer = modelRenderer;
        result->castsShadows = castsShadows;
        result->rendererStorage = rendererStorage.clone();
        return result;
    }

    void ModelComponent::setFile(const IO::VFS::Path& path) {
        asyncModel = std::move(AsyncModelResource(GetAssetServer().loadModelTask(path)));
        modelRenderer = nullptr;
        if(GetCapabilities().supportsRaytracing) {
            Async::LockGuard l{ tlasAccess };
            tlasIsWaitingForModel = true;
        }
    }

    void ModelComponent::enableTLAS() {
        Async::LockGuard l{ tlasAccess };
        if(tlas) {
            tlas->enabled = true;
        }
    }

    void ModelComponent::disableTLAS() {
        Async::LockGuard l{ tlasAccess };
        if(tlas) {
            tlas->enabled = false;
        }
    }
}
