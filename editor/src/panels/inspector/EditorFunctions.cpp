//
// Created by jglrxavpok on 31/05/2023.
//

#include "EditorFunctions.h"
#include <engine/edition/DragDropTypes.h>
#include <engine/Engine.h>
#include <engine/render/VulkanRenderer.h>
#include <engine/ecs/components/CameraComponent.h>
#include <engine/ecs/components/CSharpComponent.h>
#include <engine/ecs/components/ForceSinPosition.h>
#include <engine/ecs/components/Kinematics.h>
#include <engine/ecs/components/LuaScriptComponent.h>
#include <engine/ecs/components/LightComponent.h>
#include <engine/ecs/components/ModelComponent.h>
#include <engine/ecs/components/RigidBodyComponent.h>
#include <engine/ecs/components/PhysicsCharacterComponent.h>
#include <engine/ecs/components/SpriteComponent.h>
#include <engine/ecs/components/TextComponent.h>
#include <engine/ecs/components/TransformComponent.h>
#include <core/io/Logging.hpp>
#include <engine/render/ModelRenderer.h>

namespace Carrot::ECS {
    struct RigidBodyComponent;
}

namespace Peeler {
    template<typename ComponentType>
    void registerFunction(InspectorPanel& inspector, void(*editionFunction)(EditContext&, ComponentType*)) {
        inspector.registerComponentEditor(ComponentType::getID(), [editionFunction](EditContext& editContext, Carrot::ECS::Component* comp) {
            editionFunction(editContext, dynamic_cast<ComponentType*>(comp));
        });
    }

    static void editTransformComponent(EditContext& edition, Carrot::ECS::TransformComponent* component) {
        Carrot::Math::Transform& localTransform = component->localTransform;
        {
            float arr[] = { localTransform.position.x, localTransform.position.y, localTransform.position.z };
            if (ImGui::DragFloat3("Position", arr)) {
                localTransform.position = { arr[0], arr[1], arr[2] };
                edition.hasModifications = true;
            }
        }

        {
            float arr[] = { localTransform.scale.x, localTransform.scale.y, localTransform.scale.z };
            if (ImGui::DragFloat3("Scale", arr)) {
                localTransform.scale = { arr[0], arr[1], arr[2] };
                edition.hasModifications = true;
            }
        }

        {
            float arr[] = { localTransform.rotation.x, localTransform.rotation.y, localTransform.rotation.z, localTransform.rotation.w };
            if (ImGui::DragFloat4("Rotation", arr)) {
                localTransform.rotation = { arr[3], arr[0], arr[1], arr[2] };
                edition.hasModifications = true;
            }
        }
    }

    static void editLightComponent(EditContext& edition, Carrot::ECS::LightComponent* component) {
        if(component->lightRef) {
            auto& light = component->lightRef->light;

            bool enabled = light.enabled;
            if(ImGui::Checkbox("Enabled##inspector lightcomponent", &enabled)) {
                light.enabled = enabled;
            }

            if(ImGui::BeginCombo("Light type##inspector lightcomponent", Carrot::Render::Light::nameOf(light.type))) {
                auto selectable = [&](Carrot::Render::LightType type) {
                    std::string id = Carrot::Render::Light::nameOf(type);
                    id += "##inspector lightcomponent";
                    bool selected = type == light.type;
                    if(ImGui::Selectable(id.c_str(), &selected)) {
                        light.type = type;
                    }
                };

                selectable(Carrot::Render::LightType::Point);
                selectable(Carrot::Render::LightType::Directional);
                selectable(Carrot::Render::LightType::Spot);

                ImGui::EndCombo();
            }

            ImGui::BeginDisabled(!enabled);
            ImGui::DragFloat("Intensity##inspector lightcomponent", &light.intensity);

            if(ImGui::CollapsingHeader("Parameters")) {
                switch (light.type) {
                    case Carrot::Render::LightType::Spot: {
                        float cutoffAngle = glm::acos(light.cutoffCosAngle);
                        float outerCutoffAngle = glm::acos(light.outerCutoffCosAngle);
                        if(ImGui::DragAngle("Cutoff angle", &cutoffAngle)) {
                            light.cutoffCosAngle = glm::cos(cutoffAngle);
                        }
                        if(ImGui::DragAngle("Outer cutoff angle", &outerCutoffAngle)) {
                            light.outerCutoffCosAngle = glm::cos(outerCutoffAngle);
                        }
                    } break;

                    case Carrot::Render::LightType::Point: {
                        ImGui::DragFloat("Constant attenuation", &light.constantAttenuation);
                        ImGui::DragFloat("Linear attenuation", &light.linearAttenuation);
                        ImGui::DragFloat("Quadratic attenuation", &light.quadraticAttenuation);
                    } break;
                }
            }

            float colorArr[3] = { light.color.r, light.color.g, light.color.b };
            glm::vec3& color = light.color;
            if(ImGui::ColorPicker3("Light color##inspector lightcomponent", colorArr)) {
                color = glm::vec3 { colorArr[0], colorArr[1], colorArr[2] };
                edition.hasModifications = true;
            }

            ImGui::EndDisabled();
        } else {
            ImGui::BeginDisabled();
            ImGui::TextWrapped("%s", "No light reference in this component. It must have been disabled explicitly.");
            ImGui::EndDisabled();
        }
    }

    static void editSpriteComponent(EditContext& edition, Carrot::ECS::SpriteComponent* component) {
        static std::string path = "<<path>>";
        static Carrot::ECS::SpriteComponent* inInspector = nullptr;
        auto& sprite = component->sprite;
        if(inInspector != component) {
            inInspector = component;
            if(sprite) {
                path = sprite->getTexture().getOriginatingResource().getName();
            } else {
                path = "";
            }
        }
        if(ImGui::InputText("Filepath##SpriteComponent filepath inspector", path, ImGuiInputTextFlags_EnterReturnsTrue)) {
            auto textureRef = GetRenderer().getOrCreateTextureFullPath(path);
            if(!sprite) {
                sprite = std::make_unique<Carrot::Render::Sprite>(textureRef);
            } else {
                sprite->setTexture(textureRef);
            }
            edition.hasModifications = true;
        }
        if(ImGui::BeginDragDropTarget()) {
            if(auto* payload = ImGui::AcceptDragDropPayload(Carrot::Edition::DragDropTypes::FilePath)) {
                std::unique_ptr<char8_t[]> buffer = std::make_unique<char8_t[]>(payload->DataSize+1);
                std::memcpy(buffer.get(), static_cast<const void*>(payload->Data), payload->DataSize);
                buffer.get()[payload->DataSize] = '\0';

                std::u8string newPath = buffer.get();

                std::filesystem::path fsPath = std::filesystem::proximate(newPath, std::filesystem::current_path());
                if(!std::filesystem::is_directory(fsPath) && Carrot::IO::isImageFormatFromPath(fsPath)) {
                    auto textureRef = GetRenderer().getOrCreateTextureFullPath(fsPath.string().c_str());
                    if(!sprite) {
                        sprite = std::make_unique<Carrot::Render::Sprite>(textureRef);
                    } else {
                        sprite->setTexture(textureRef);
                    }
                    inInspector = nullptr;
                    edition.hasModifications = true;
                }
            }

            ImGui::EndDragDropTarget();
        }

        if(sprite) {
            auto& region = sprite->getTextureRegion();
            bool recompute = false;
            float minU = region.getMinX();
            float minV = region.getMinY();
            float maxU = region.getMaxX();
            float maxV = region.getMaxY();
            if(ImGui::InputFloat("Min U##SpriteComponent minU inspector", &minU)
               |  ImGui::InputFloat("Min V##SpriteComponent minV inspector", &minV)
               |  ImGui::InputFloat("Max U##SpriteComponent maxU inspector", &maxU)
               |  ImGui::InputFloat("Max V##SpriteComponent maxV inspector", &maxV)) {
                recompute = true;
            }

            if(recompute) {
                edition.hasModifications = true;
                region = Carrot::Math::Rect2Df(minU, minV, maxU, maxV);
            }
        }
    }

    void editModelComponent(EditContext& edition, Carrot::ECS::ModelComponent* component) {
        auto& asyncModel = component->asyncModel;
        if(!asyncModel.isReady() && !asyncModel.isEmpty()) {
            ImGui::Text("Model is loading...");
            return;
        }
        std::string path = asyncModel.isEmpty() ? "" : asyncModel->getOriginatingResource().getName();
        if(ImGui::InputText("Filepath##ModelComponent filepath inspector", path, ImGuiInputTextFlags_EnterReturnsTrue)) {
            component->setFile(Carrot::IO::VFS::Path(path));
            edition.hasModifications = true;
        }

        ImGui::Checkbox("Transparent##ModelComponent transparent inspector", &component->isTransparent);

        if(ImGui::BeginDragDropTarget()) {
            if(auto* payload = ImGui::AcceptDragDropPayload(Carrot::Edition::DragDropTypes::FilePath)) {
                std::unique_ptr<char8_t[]> buffer = std::make_unique<char8_t[]>(payload->DataSize+sizeof(char8_t));
                std::memcpy(buffer.get(), static_cast<const char8_t*>(payload->Data), payload->DataSize);
                buffer.get()[payload->DataSize] = '\0';

                std::u8string str = buffer.get();
                std::string s = Carrot::toString(str);

                auto vfsPath = Carrot::IO::VFS::Path(s);

                // TODO: no need to go through disk again
                std::filesystem::path fsPath = GetVFS().resolve(vfsPath);
                if(!std::filesystem::is_directory(fsPath) && Carrot::IO::isModelFormatFromPath(s.c_str())) {
                    component->setFile(vfsPath);
                    edition.hasModifications = true;
                }
            }

            ImGui::EndDragDropTarget();
        }

        float colorArr[4] = { component->color.r, component->color.g, component->color.b, component->color.a };
        if(ImGui::ColorPicker4("Model color", colorArr)) {
            component->color = glm::vec4 { colorArr[0], colorArr[1], colorArr[2], colorArr[3] };
            edition.hasModifications = true;
        }

        if(asyncModel.isReady() && ImGui::CollapsingHeader("Material overrides")) {
            auto& worldData = component->getEntity().getWorld().getWorldData();
            auto cloneRenderer = [&]() {
                if(component->modelRenderer) {
                    return component->modelRenderer->clone();
                }

                return std::make_shared<Carrot::Render::ModelRenderer>(*asyncModel);
            };

            // handle rendering overrides (replacing pipeline and/or material textures)
            if(component->modelRenderer) {
                // TODO
                std::shared_ptr<Carrot::Render::ModelRenderer> clonedRenderer = nullptr;
                auto cloneIfNeeded = [&]() {
                    if(!clonedRenderer) {
                        clonedRenderer = cloneRenderer();
                    }

                    return clonedRenderer;
                };

                static std::string albedoPath = "<<path>>";

                Carrot::Render::MaterialSystem& materialSystem = GetRenderer().getMaterialSystem();
                for(const auto& override : component->modelRenderer->getOverrides()) {
                    ImGui::Text("%llu", override.meshIndex);

                    auto modifyTextures = [&]() {
                        auto renderer = cloneIfNeeded();
                        Carrot::Render::MaterialOverride* pNewOverride = renderer->getOverrides().findForMesh(override.meshIndex);
                        if(!pNewOverride->materialTextures) {
                            pNewOverride->materialTextures = materialSystem.createMaterialHandle();
                        }

                        return pNewOverride->materialTextures;
                    };

                    if(ImGui::InputText("Albedo##editModelComponent", albedoPath, ImGuiInputTextFlags_EnterReturnsTrue)) {
                        auto textureRef = GetRenderer().getOrCreateTextureFullPath(albedoPath);
                        modifyTextures()->albedo = materialSystem.createTextureHandle(textureRef);
                        edition.hasModifications = true;
                    }
                }

                if(clonedRenderer) {
                    component->modelRenderer = clonedRenderer;
                    clonedRenderer->recreateStructures();
                }
            }

            if(ImGui::Button("+")) {
                component->modelRenderer = cloneRenderer();
                component->modelRenderer->addOverride({});
                worldData.storeModelRenderer(component->modelRenderer);
            }
        }
    }

    void editKinematicsComponent(EditContext& edition, Carrot::ECS::Kinematics* component) {
        float arr[] = { component->velocity.x, component->velocity.y, component->velocity.z };
        if (ImGui::DragFloat3("Velocity", arr)) {
            component->velocity = { arr[0], arr[1], arr[2] };
            edition.hasModifications = true;
        }
    }

    void editForceSinPositionComponent(EditContext& edition, Carrot::ECS::ForceSinPosition* component) {
        auto line = [&](const char* id, glm::vec3& vec) {
            float arr[] = { vec.x, vec.y, vec.z };
            if (ImGui::DragFloat3(id, arr)) {
                vec = { arr[0], arr[1], arr[2] };
                edition.hasModifications = true;
            }
        };
        line("Angular Frequency", component->angularFrequency);
        line("Amplitude", component->amplitude);
        line("Angular Offset", component->angularOffset);
        line("Center Position", component->centerPosition);
    }

    void editCameraComponent(EditContext& edition, Carrot::ECS::CameraComponent* component) {
        ImGui::Checkbox("Primary", &component->isPrimary);
        ImGui::BeginGroup();
        if(ImGui::RadioButton("Perspective", !component->isOrthographic)) {
            component->isOrthographic = false;
        }
        ImGui::SameLine();
        if(ImGui::RadioButton("Orthographic", component->isOrthographic)) {
            component->isOrthographic = true;
        }
        ImGui::EndGroup();

        if(component->isOrthographic) {
            float arr[3] = { component->orthoSize.x, component->orthoSize.y, component->orthoSize.z };
            if(ImGui::DragFloat3("Bounds", arr, 0.5f)) {
                component->orthoSize = { arr[0], arr[1], arr[2] };
            }
        } else {
            ImGui::DragFloatRange2("Z Range", &component->perspectiveNear, &component->perspectiveFar, 0.0f, 10000.0f);
            ImGui::SliderAngle("FOV", &component->perspectiveFov, 0.001f, 360.0f);
        }
    }

    void editTextComponent(EditContext& edition, Carrot::ECS::TextComponent* component) {
        std::string text { component->getText() };
        if(ImGui::InputTextMultiline("Content##TextComponent input field", text)) {
            component->setText(text);
        }
    }

    void editLuaScriptComponent(EditContext& edition, Carrot::ECS::LuaScriptComponent* component) {
        ImGui::PushID("LuaScriptComponent");
        std::vector<Carrot::IO::VFS::Path> toRemove;
        std::size_t index = 0;
        for(auto& [path, script] : component->scripts) {
            ImGui::PushID(index);

            ImGui::Text("[%llu]", index);
            ImGui::SameLine();

            std::string pathStr = path.toString();
            if(ImGui::InputText("Filepath", pathStr, ImGuiInputTextFlags_EnterReturnsTrue)) {
                script = nullptr;
                Carrot::IO::VFS::Path vfsPath = Carrot::IO::VFS::Path(pathStr);
                if(GetVFS().exists(vfsPath)) {
                    script = std::make_unique<Carrot::Lua::Script>(vfsPath);
                }
                script = std::move(script);
                path = vfsPath;
            }

            if(ImGui::BeginDragDropTarget()) {
                if(auto* payload = ImGui::AcceptDragDropPayload(Carrot::Edition::DragDropTypes::FilePath)) {
                    std::unique_ptr<char8_t[]> buffer = std::make_unique<char8_t[]>(payload->DataSize+sizeof(char8_t));
                    std::memcpy(buffer.get(), static_cast<const char8_t*>(payload->Data), payload->DataSize);
                    buffer.get()[payload->DataSize] = '\0';

                    std::filesystem::path newPath = buffer.get();

                    std::filesystem::path fsPath = std::filesystem::proximate(newPath, std::filesystem::current_path());
                    if(!std::filesystem::is_directory(fsPath) && Carrot::IO::isScriptFormatFromPath(fsPath)) {
                        script = nullptr;
                        auto inVFS = GetVFS().represent(fsPath);
                        if(inVFS.has_value()) {
                            const auto& vfsPath = inVFS.value();
                            if(GetVFS().exists(vfsPath)) {
                                script = std::make_unique<Carrot::Lua::Script>(vfsPath);
                            }
                            script = std::move(script);
                            path = vfsPath;
                            edition.hasModifications = true;
                        } else {
                            Carrot::Log::error("Not inside VFS: %s", fsPath.u8string().c_str());
                        }
                    }
                }

                ImGui::EndDragDropTarget();
            }

            ImGui::SameLine();
            if(ImGui::Button("x")) {
                toRemove.push_back(path);
            }

            ImGui::SameLine();
            if(ImGui::Button("Reload")) {
                script = nullptr;
                if(GetVFS().exists(path)) {
                    script = std::make_unique<Carrot::Lua::Script>(path);
                }
                script = std::move(script);
            }

            ImGui::PopID();
            index++;
        }

        for(const auto& p : toRemove) {
            std::erase_if(component->scripts, [&](const auto& pair) { return pair.first == p; });
        }

        if(ImGui::Button("+")) {
            component->scripts.emplace_back("", nullptr);
        }
        ImGui::PopID();
    }

    void editRigidBodyComponent(EditContext& edition, Carrot::ECS::RigidBodyComponent* component);
    void editPhysicsCharacterComponent(EditContext& edition, Carrot::ECS::PhysicsCharacterComponent* component);

    void registerEditionFunctions(InspectorPanel& inspector) {
        registerFunction(inspector, editCameraComponent);
        registerFunction(inspector, editForceSinPositionComponent);
        registerFunction(inspector, editKinematicsComponent);
        registerFunction(inspector, editLightComponent);
        registerFunction(inspector, editLuaScriptComponent);
        registerFunction(inspector, editModelComponent);
        registerFunction(inspector, editRigidBodyComponent);
        registerFunction(inspector, editSpriteComponent);
        registerFunction(inspector, editTextComponent);
        registerFunction(inspector, editTransformComponent);
        registerFunction(inspector, editPhysicsCharacterComponent);
    }
}