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

namespace Carrot::ECS {

    ModelComponent::ModelComponent(const rapidjson::Value& json, Entity entity): ModelComponent::ModelComponent(std::move(entity)) {
        auto obj = json.GetObject();
        isTransparent = obj["isTransparent"].GetBool();
        color = JSON::read<4, float>(obj["color"]);

        if(obj.HasMember("castsShadows")) {
            castsShadows = obj["castsShadows"].GetBool();
        }

        if(obj.HasMember("model")) {
            auto modelData = obj["model"].GetObject();

            if(modelData.HasMember("modelPath")) {
                std::string modelPath = modelData["modelPath"].GetString();
                setFile(IO::VFS::Path(modelPath));
            } else {
                TODO // cannot load non-file models at the moment
            }

            if(modelData.HasMember("model_renderer")) {
                Carrot::UUID rendererID = Carrot::UUID::fromString(std::string_view { modelData["model_renderer"].GetString(), modelData["model_renderer"].GetStringLength() });
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

    rapidjson::Value ModelComponent::toJSON(rapidjson::Document& doc) const {
        asyncModel.forceWait();

        rapidjson::Value obj{rapidjson::kObjectType};

        obj.AddMember("isTransparent", isTransparent, doc.GetAllocator());
        obj.AddMember("color", JSON::write(color, doc), doc.GetAllocator());
        obj.AddMember("castsShadows", castsShadows, doc.GetAllocator());

        rapidjson::Value modelData(rapidjson::kObjectType);
        auto& resource = asyncModel->getOriginatingResource();

        if(resource.isFile()) {
            rapidjson::Value modelPath{resource.getName(), doc.GetAllocator()};
            modelData.AddMember("modelPath", modelPath, doc.GetAllocator());
        }

        if(modelRenderer) {
            modelData.AddMember("model_renderer", rapidjson::Value { modelRenderer->uuid.toString().c_str(), doc.GetAllocator() }, doc.GetAllocator());
        }

        obj.AddMember("model", modelData, doc.GetAllocator());

        return obj;
    }

    void ModelComponent::loadTLASIfPossible() {
        if(!GetCapabilities().supportsRaytracing) {
            return;
        }
        // TODO: recreating nanite rt stuff
        if(true) {
            return;
        }
        if(tlasIsWaitingForModel && asyncModel.isReady()) {
            Async::LockGuard l{ tlasAccess };
            if(tlasIsWaitingForModel) {
                tlasIsWaitingForModel = false;
                tlas = GetRenderer().getASBuilder().addInstance(asyncModel->getStaticBLAS());
                tlas->enabled = true;
            }
        }
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