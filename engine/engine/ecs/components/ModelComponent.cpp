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

namespace Carrot::ECS {
    ModelComponent* ModelComponent::inInspector = nullptr;

    ModelComponent::ModelComponent(const rapidjson::Value& json, Entity entity): ModelComponent::ModelComponent(std::move(entity)) {
        auto obj = json.GetObject();
        isTransparent = obj["isTransparent"].GetBool();
        color = JSON::read<4, float>(obj["color"]);

        if(obj.HasMember("model")) {
            auto modelData = obj["model"].GetObject();

            if(modelData.HasMember("modelPath")) {
                std::string modelPath = modelData["modelPath"].GetString();
                asyncModel = std::move(AsyncModelResource(GetRenderer().coloadModel(modelPath)));
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

    void ModelComponent::drawInspectorInternals(const Render::Context& renderContext, bool& modified) {
        if(!asyncModel.isReady()) {
            ImGui::Text("Model is loading...");
            return;
        }
        std::string path = asyncModel->getOriginatingResource().getName();
        if(ImGui::InputText("Filepath##ModelComponent filepath inspector", path, ImGuiInputTextFlags_EnterReturnsTrue)) {
            asyncModel = std::move(AsyncModelResource(GetRenderer().coloadModel(path)));
            modified = true;
        }

        if(ImGui::BeginDragDropTarget()) {
            if(auto* payload = ImGui::AcceptDragDropPayload(Carrot::Edition::DragDropTypes::FilePath)) {
                std::unique_ptr<char8_t[]> buffer = std::make_unique<char8_t[]>(payload->DataSize+sizeof(char8_t));
                std::memcpy(buffer.get(), static_cast<const char8_t*>(payload->Data), payload->DataSize);
                buffer.get()[payload->DataSize] = '\0';

                std::filesystem::path newPath = buffer.get();

                std::filesystem::path fsPath = std::filesystem::proximate(newPath, std::filesystem::current_path());
                if(!std::filesystem::is_directory(fsPath) && Carrot::IO::isModelFormatFromPath(fsPath)) {
                    asyncModel = std::move(AsyncModelResource(GetRenderer().coloadModel(Carrot::toString(fsPath.u8string()))));
                    inInspector = nullptr;
                    modified = true;
                }
            }

            ImGui::EndDragDropTarget();
        }

        float colorArr[4] = { color.r, color.g, color.b, color.a };
        if(ImGui::ColorPicker4("Model color", colorArr)) {
            color = glm::vec4 { colorArr[0], colorArr[1], colorArr[2], colorArr[3] };
            modified = true;
        }
    }
}