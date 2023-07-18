//
// Created by jglrxavpok on 08/10/2021.
//

#include "ModelComponent.h"
#include "engine/Engine.h"
#include "core/utils/ImGuiUtils.hpp"
#include "imgui.h"
#include "engine/edition/DragDropTypes.h"
#include "core/utils/JSON.h"
#include <filesystem>
#include <string>

namespace Carrot::ECS {

    ModelComponent::ModelComponent(const rapidjson::Value& json, Entity entity): ModelComponent::ModelComponent(std::move(entity)) {
        auto obj = json.GetObject();
        isTransparent = obj["isTransparent"].GetBool();
        color = JSON::read<4, float>(obj["color"]);

        if(obj.HasMember("model")) {
            auto modelData = obj["model"].GetObject();

            if(modelData.HasMember("modelPath")) {
                std::string modelPath = modelData["modelPath"].GetString();
                setFile(IO::VFS::Path(modelPath));
            } else {
                TODO // cannot load non-file models at the moment
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

        rapidjson::Value modelData(rapidjson::kObjectType);
        auto& resource = asyncModel->getOriginatingResource();

        if(resource.isFile()) {
            rapidjson::Value modelPath{resource.getName(), doc.GetAllocator()};
            modelData.AddMember("modelPath", modelPath, doc.GetAllocator());
        }


        obj.AddMember("model", modelData, doc.GetAllocator());

        return obj;
    }

    void ModelComponent::loadTLASIfPossible() {
        if(!GetCapabilities().supportsRaytracing) {
            return;
        }
        if(castsShadows && tlasIsWaitingForModel && asyncModel.isReady()) {
            Async::LockGuard l{ tlasAccess };
            if(tlasIsWaitingForModel) {
                tlasIsWaitingForModel = false;
                tlas = GetRenderer().getASBuilder().addInstance(asyncModel->getStaticBLAS());
                tlas->enabled = true;
            }
        } else if(!castsShadows && tlas) {
            tlas = nullptr;
        }
    }

    void ModelComponent::setFile(const IO::VFS::Path& path) {
        asyncModel = std::move(AsyncModelResource(GetRenderer().coloadModel(path.toString())));
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