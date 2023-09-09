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
#include <engine/ecs/components/NavMeshComponent.h>
#include <engine/ecs/components/RigidBodyComponent.h>
#include <engine/ecs/components/PhysicsCharacterComponent.h>
#include <engine/ecs/components/SoundListenerComponent.h>
#include <engine/ecs/components/SpriteComponent.h>
#include <engine/ecs/components/TextComponent.h>
#include <engine/ecs/components/TransformComponent.h>
#include <core/io/Logging.hpp>
#include <engine/render/ModelRenderer.h>
#include <engine/ecs/components/ModelComponent.h>
#include <IconsFontAwesome5.h>

namespace Carrot::ECS {
    struct ModelComponent;
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

        Carrot::Render::Texture::Ref textureRef;
        if(edition.inspector.drawPickTextureWidget("Filepath##Sprite component edition", &textureRef)) {
            if(!sprite) {
                sprite = std::make_shared<Carrot::Render::Sprite>(textureRef);
            } else {
                sprite->setTexture(textureRef);
            }

            edition.hasModifications = true;
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

    void editModelComponent(EditContext& edition, Carrot::ECS::ModelComponent* component);

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

    void editNavMeshComponent(EditContext& edition, Carrot::ECS::NavMeshComponent* component) {
        std::string path = component->meshFile.isFile() ? component->meshFile.getName() : "";
        if(ImGui::InputText("Mesh##NavMesh filepath inspector", path, ImGuiInputTextFlags_EnterReturnsTrue)) {
            component->setMesh(Carrot::IO::VFS::Path(path));
            edition.hasModifications = true;
        }

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
                    component->setMesh(vfsPath);
                    edition.hasModifications = true;
                }
            }

            ImGui::EndDragDropTarget();
        }
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
        registerFunction(inspector, editNavMeshComponent);
    }

    void registerDisplayNames(InspectorPanel& inspector) {
        inspector.registerComponentDisplayName(Carrot::ECS::TransformComponent::getID(), ICON_FA_ARROWS_ALT "  Transform");
        inspector.registerComponentDisplayName(Carrot::ECS::LightComponent::getID(), ICON_FA_LIGHTBULB "  Light");
        inspector.registerComponentDisplayName(Carrot::ECS::ModelComponent::getID(), ICON_FA_CUBE "  Model");
        inspector.registerComponentDisplayName(Carrot::ECS::CameraComponent::getID(), ICON_FA_CAMERA_RETRO "  Camera");
        inspector.registerComponentDisplayName(Carrot::ECS::RigidBodyComponent::getID(), ICON_FA_CUBES "  Rigidbody");
        inspector.registerComponentDisplayName(Carrot::ECS::PhysicsCharacterComponent::getID(), ICON_FA_CHILD "  Physics Character");
        inspector.registerComponentDisplayName(Carrot::ECS::SpriteComponent::getID(), ICON_FA_IMAGE "  Sprite");
        inspector.registerComponentDisplayName(Carrot::ECS::TextComponent::getID(), ICON_FA_FONT "  Text");
        inspector.registerComponentDisplayName(Carrot::ECS::NavMeshComponent::getID(), ICON_FA_ROUTE "  NavMesh");
        inspector.registerComponentDisplayName(Carrot::ECS::SoundListenerComponent::getID(), ICON_FA_PODCAST "  SoundListener");
    }
}