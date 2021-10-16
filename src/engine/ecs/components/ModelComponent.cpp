//
// Created by jglrxavpok on 08/10/2021.
//

#include "ModelComponent.h"
#include "engine/Engine.h"
#include "engine/utils/ImGuiUtils.hpp"
#include "imgui.h"
#include "engine/edition/DragDropTypes.h"
#include "engine/utils/JSON.h"
#include <filesystem>

namespace Carrot::ECS {
    ModelComponent* ModelComponent::inInspector = nullptr;

    ModelComponent::ModelComponent(const rapidjson::Value& json, Entity entity): ModelComponent::ModelComponent(std::move(entity)) {
        auto obj = json.GetObject();
        isTransparent = obj["isTransparent"].GetBool();
        color = JSON::read<4, float>(obj["color"]);

        if(obj.HasMember("model")) {
            auto modelData = obj["model"].GetObject();

            if(modelData.HasMember("path")) {
                std::string modelPath = modelData["path"].GetString();
                model = Engine::getInstance().getRenderer().getOrCreateModel(modelPath);
            } else {
                TODO // cannot load non-file models at the moment
            }
        }
    }

    rapidjson::Value ModelComponent::toJSON(rapidjson::Document& doc) const {
        rapidjson::Value obj{rapidjson::kObjectType};

        obj.AddMember("isTransparent", isTransparent, doc.GetAllocator());
        obj.AddMember("color", JSON::write(color, doc), doc.GetAllocator());
        if(model) {
            rapidjson::Value modelData(rapidjson::kObjectType);
            auto& resource = model->getOriginatingResource();

            if(resource.isFile()) {
                rapidjson::Value modelPath{resource.getName(), doc.GetAllocator()};
                modelData.AddMember("modelPath", modelPath, doc.GetAllocator());
            }


            obj.AddMember("model", modelData, doc.GetAllocator());
        }

        return obj;
    }

    void ModelComponent::drawInspectorInternals(const Render::Context& renderContext, bool& modified) {
        static std::string path = "<<path>>";
        if(inInspector != this) {
            inInspector = this;
            if(model) {
                path = model->getOriginatingResource().getName();
            } else {
                path = "";
            }
        }
        if(Carrot::ImGui::InputText("Filepath##ModelComponent filepath inspector", path, ImGuiInputTextFlags_EnterReturnsTrue)) {
            model = Engine::getInstance().getRenderer().getOrCreateModel(path);
            modified = true;
        }

        if(::ImGui::BeginDragDropTarget()) {
            if(auto* payload = ::ImGui::AcceptDragDropPayload(Carrot::Edition::DragDropTypes::FilePath)) {
                std::unique_ptr<char[]> buffer = std::make_unique<char[]>(payload->DataSize+1);
                std::memcpy(buffer.get(), static_cast<const char *>(payload->Data), payload->DataSize);
                buffer.get()[payload->DataSize] = '\0';

                std::string newPath = buffer.get();

                std::filesystem::path fsPath = std::filesystem::relative(newPath, std::filesystem::current_path());
                if(!std::filesystem::is_directory(fsPath)/* && Carrot::IO::isImageFormatFromPath(fsPath) TODO: isModelFormat */) {
                    model = Engine::getInstance().getRenderer().getOrCreateModel(fsPath.string());
                    inInspector = nullptr;
                    modified = true;
                }
            }

            ::ImGui::EndDragDropTarget();
        }

        float colorArr[4] = { color.r, color.g, color.b, color.a };
        if(::ImGui::ColorPicker4("Model color", colorArr)) {
            color = glm::vec4 { colorArr[0], colorArr[1], colorArr[2], colorArr[3] };
            modified = true;
        }
    }
}