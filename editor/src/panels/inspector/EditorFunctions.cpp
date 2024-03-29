//
// Created by jglrxavpok on 31/05/2023.
//

#include "EditorFunctions.h"
#include <engine/edition/DragDropTypes.h>
#include <engine/Engine.h>
#include <engine/render/VulkanRenderer.h>
#include <engine/ecs/components/AnimatedModelComponent.h>
#include <engine/ecs/components/CameraComponent.h>
#include <engine/ecs/components/CSharpComponent.h>
#include <engine/ecs/components/ForceSinPosition.h>
#include <engine/ecs/components/Kinematics.h>
#include <engine/ecs/components/LuaScriptComponent.h>
#include <engine/ecs/components/LightComponent.h>
#include <engine/ecs/components/ModelComponent.h>
#include <engine/ecs/components/NavMeshComponent.h>
#include <engine/ecs/components/RigidBodyComponent.h>
#include <engine/ecs/components/PhysicsCharacterComponent.h>
#include <engine/ecs/components/SoundListenerComponent.h>
#include <engine/ecs/components/SpriteComponent.h>
#include <engine/ecs/components/TextComponent.h>
#include <engine/ecs/components/TransformComponent.h>
#include <core/io/Logging.hpp>
#include <engine/render/ModelRenderer.h>
#include <IconsFontAwesome5.h>
#include <Peeler.h>
#include <core/allocators/StackAllocator.h>

namespace Peeler {
    namespace Helpers {
        bool any(std::span<bool> values) {
            for(const bool& b : values) {
                if(b) {
                    return true;
                }
            }
            return false;
        }

        bool all(std::span<bool> values) {
            for(const bool& b : values) {
                if(!b) {
                    return false;
                }
            }
            return true;
        }
    }

    template<typename ComponentType>
    void registerFunction(InspectorPanel& inspector, void(*editionFunction)(EditContext&, const Carrot::Vector<ComponentType*>&)) {
        inspector.registerComponentEditor(ComponentType::getID(), [editionFunction](EditContext& editContext, const Carrot::Vector<Carrot::ECS::Component*>& comps) {
            Carrot::Vector<ComponentType*> typedComponents;
            typedComponents.resize(comps.size());
            for(std::int64_t i = 0; i < typedComponents.size(); i++) {
                typedComponents[i] = dynamic_cast<ComponentType*>(comps[i]);
            }
            editionFunction(editContext, typedComponents);
        });
    }

    // instead of using ImGui::DragFloat3 & co, reimplement it. Because multiple positions are modified at once,
    //  they might not have all the same coordinates. This way, we can modify only the coordinate that has been modified
    static bool editMultipleVec3(const char* id, std::span<glm::vec3> vectors) {
        bool modified = false;
        ImGui::BeginGroup();
        ImGui::PushID(id);

        ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());

        for(std::size_t axisIndex = 0; axisIndex < 3; axisIndex++) {
            ImGui::PushID(axisIndex);
            float currentValue = (vectors[0])[axisIndex];
            bool sameValue = true;

            for(std::size_t j = 1; j < vectors.size(); j++) {
                if(currentValue != (vectors[j])[axisIndex]) {
                    sameValue = false;
                }
            }

            const char* format = sameValue ? nullptr : "<VARIOUS>";

            if (axisIndex != 0) {
                ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
            }

            if(ImGui::DragScalar("", ImGuiDataType_Float, &currentValue, 1, nullptr, nullptr, format)) {
                modified = true;
                for(glm::vec3& vec : vectors) {
                    vec[axisIndex] = currentValue;
                }
            }
            ImGui::PopItemWidth();
            ImGui::PopID();
        }

        ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::TextUnformatted(id);

        ImGui::PopID();
        ImGui::EndGroup();
        return modified;
    }

    // maybe merge with function editMultipleVec3, but for now copy paste will do
    static bool editMultipleQuat(const char* id, std::span<glm::quat> values) {
        bool modified = false;
        ImGui::BeginGroup();
        ImGui::PushID(id);

        ImGui::PushMultiItemsWidths(4, ImGui::CalcItemWidth());

        for(std::size_t axisIndex = 0; axisIndex < 4; axisIndex++) {
            ImGui::PushID(axisIndex);
            float currentValue = (values[0])[axisIndex];
            bool sameValue = true;

            for(std::size_t j = 1; j < values.size(); j++) {
                if(currentValue != (values[j])[axisIndex]) {
                    sameValue = false;
                }
            }

            const char* format = sameValue ? nullptr : "<VARIOUS>";

            if (axisIndex != 0) {
                ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
            }

            if(ImGui::DragScalar("", ImGuiDataType_Float, &currentValue, 1, nullptr, nullptr, format)) {
                modified = true;
                for(glm::quat& q : values) {
                    q[axisIndex] = currentValue;
                }
            }
            ImGui::PopItemWidth();
            ImGui::PopID();
        }

        ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::TextUnformatted(id);

        ImGui::PopID();
        ImGui::EndGroup();
        return modified;
    }

    static bool editMultipleCheckbox(const char* id, std::span<bool> values) {
        int tristate = -1;
        if(Helpers::all(values)) {
            tristate = 1;
        } else if(!Helpers::any(values)) {
            tristate = 0;
        }
        if(ImGui::CheckBoxTristate(id, &tristate)) {
            for(bool& b : values) {
                b = tristate == 1;
            }
            return true;
        }
        return false;
    }

    template<typename TComponent, typename TValue>
    struct UpdateComponentValues: Peeler::ICommand {
        Carrot::Vector<Carrot::ECS::EntityID> entityList;
        Carrot::Vector<TValue> newValues;
        Carrot::Vector<TValue> oldValues;
        std::function<TValue&(TComponent& comp)> accessor;

        UpdateComponentValues(Application &app, const std::string& desc, std::span<Carrot::ECS::EntityID> _entityList, std::span<TValue> _newValues,
            std::function<TValue&(TComponent& comp)> _accessor)
            : ICommand(app, desc)
            , entityList(_entityList)
            , newValues(_newValues)
            , accessor(_accessor)
        {
            oldValues.ensureReserve(newValues.size());
            for(const auto& entityID : _entityList) {
                oldValues.pushBack(accessor(app.currentScene.world.getComponent<TComponent>(entityID)));
            }
        }

        void undo() override {
            for(std::size_t i = 0; i < entityList.size(); i++) {
                accessor(editor.currentScene.world.getComponent<TComponent>(entityList[i])) = oldValues[i];
            }
        }

        void redo() override {
            for(std::size_t i = 0; i < entityList.size(); i++) {
                accessor(editor.currentScene.world.getComponent<TComponent>(entityList[i])) = newValues[i];
            }
        }
    };

    static void editTransformComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::TransformComponent*>& components) {
        Carrot::StackAllocator allocator { Carrot::Allocator::getDefault(), sizeof(void*) * (3 * components.size()) };
        Carrot::Vector<glm::vec3> positions;
        Carrot::Vector<glm::quat> rotations;
        Carrot::Vector<glm::vec3> scales;
        positions.resize(components.size());
        rotations.resize(components.size());
        scales.resize(components.size());

        for(std::int64_t i = 0; i < components.size(); i++) {
            positions[i] = components[i]->localTransform.position;
            rotations[i] = components[i]->localTransform.rotation;
            scales[i] = components[i]->localTransform.scale;
        }

        struct UpdateEntityPositions: UpdateComponentValues<Carrot::ECS::TransformComponent, glm::vec3> {
            UpdateEntityPositions(Application &app, std::span<Carrot::ECS::EntityID> _entityList, std::span<glm::vec3> _positions)
                : UpdateComponentValues(app, "Update entities position", _entityList, _positions, [](Carrot::ECS::TransformComponent& t) -> glm::vec3& { return t.localTransform.position; })
            {}
        };

        struct UpdateEntityRotations: UpdateComponentValues<Carrot::ECS::TransformComponent, glm::quat> {
            UpdateEntityRotations(Application &app, std::span<Carrot::ECS::EntityID> _entityList, std::span<glm::quat> _scales)
                : UpdateComponentValues(app, "Update entities rotations", _entityList, _scales, [](Carrot::ECS::TransformComponent& t) -> glm::quat& { return t.localTransform.rotation; })
            {}
        };

        struct UpdateEntityScales: UpdateComponentValues<Carrot::ECS::TransformComponent, glm::vec3> {
            UpdateEntityScales(Application &app, std::span<Carrot::ECS::EntityID> _entityList, std::span<glm::vec3> _scales)
                : UpdateComponentValues(app, "Update entities scale", _entityList, _scales, [](Carrot::ECS::TransformComponent& t) -> glm::vec3& { return t.localTransform.scale; })
            {}
        };

        if(editMultipleVec3("Position", positions)) {
            edition.editor.undoStack.push<UpdateEntityPositions>(edition.editor.selectedIDs, positions);
            edition.hasModifications = true;
        }
        if(editMultipleQuat("Rotation", rotations)) {
            edition.editor.undoStack.push<UpdateEntityRotations>(edition.editor.selectedIDs, rotations);
            edition.hasModifications = true;
        }
        if(editMultipleVec3("Scale", scales)) {
            edition.editor.undoStack.push<UpdateEntityScales>(edition.editor.selectedIDs, scales);
            edition.hasModifications = true;
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
    void editAnimatedModelComponent(EditContext& edition, Carrot::ECS::AnimatedModelComponent* component);

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

    void editCameraComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::CameraComponent*>& components) {
        const bool hasMultipleCamerasSelected = components.size() > 1;
        ImGui::BeginDisabled(hasMultipleCamerasSelected);

        bool isPrimary = components[0]->isPrimary;
        if(ImGui::Checkbox("Primary", &isPrimary)) {
            TODO;
        }
        if(hasMultipleCamerasSelected) {
            if(ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Only one camera can be the primary camera. Please deselect the others.");
                ImGui::EndTooltip();
            }
        }
        ImGui::EndDisabled();


        Carrot::Vector<bool> isOrthoFields;
        isOrthoFields.resize(components.size());
        for(std::int64_t i = 0; i < components.size(); i++) {
            isOrthoFields[i] = components[i]->isOrthographic;
        }

        if(editMultipleCheckbox("Orthographic", isOrthoFields)) {
            struct UpdateOrthographic: UpdateComponentValues<Carrot::ECS::CameraComponent, bool> {
                UpdateOrthographic(Application &app, std::span<Carrot::ECS::EntityID> _entityList, std::span<bool> _positions)
                : UpdateComponentValues(app, "Update camera ortho flag", _entityList, _positions,
                    [](Carrot::ECS::CameraComponent& t) -> bool& { return t.isOrthographic; })
                {}
            };

            edition.editor.undoStack.push<UpdateOrthographic>(edition.editor.selectedIDs, isOrthoFields);
        }

        if(Helpers::all(isOrthoFields)) {
            Carrot::Vector<glm::vec3> bounds;
            bounds.resize(components.size());
            for(std::int64_t i = 0; i < components.size(); i++) {
                bounds[i] = components[i]->orthoSize;
            }

            struct UpdateBounds: UpdateComponentValues<Carrot::ECS::CameraComponent, glm::vec3> {
                UpdateBounds(Application &app, std::span<Carrot::ECS::EntityID> _entityList, std::span<glm::vec3> _positions)
                : UpdateComponentValues(app, "Update camera ortho bounds", _entityList, _positions,
                    [](Carrot::ECS::CameraComponent& t) -> glm::vec3& { return t.orthoSize; })
                {}
            };

            if(editMultipleVec3("Bounds", bounds)) {
                edition.editor.undoStack.push<UpdateBounds>(edition.editor.selectedIDs, bounds);
            }
        } else if(!Helpers::any(isOrthoFields)) {
            /* TODO ImGui::DragFloatRange2("Z Range", &component->perspectiveNear, &component->perspectiveFar, 0.0f, 10000.0f);
            ImGui::SliderAngle("FOV", &component->perspectiveFov, 0.001f, 360.0f);*/
        } else {
            ImGui::Text("Cannot edit mix of orthographic and perspective cameras at the same time.");
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
        /*registerFunction(inspector, editForceSinPositionComponent);
        registerFunction(inspector, editKinematicsComponent);
        registerFunction(inspector, editLightComponent);
        registerFunction(inspector, editLuaScriptComponent);
        registerFunction(inspector, editModelComponent);
        registerFunction(inspector, editAnimatedModelComponent);
        registerFunction(inspector, editRigidBodyComponent);
        registerFunction(inspector, editSpriteComponent);
        registerFunction(inspector, editTextComponent);*/
        registerFunction(inspector, editTransformComponent);
        /*registerFunction(inspector, editPhysicsCharacterComponent);
        registerFunction(inspector, editNavMeshComponent);*/
    }

    void registerDisplayNames(InspectorPanel& inspector) {
        inspector.registerComponentDisplayName(Carrot::ECS::TransformComponent::getID(), ICON_FA_ARROWS_ALT "  Transform");
        inspector.registerComponentDisplayName(Carrot::ECS::LightComponent::getID(), ICON_FA_LIGHTBULB "  Light");
        inspector.registerComponentDisplayName(Carrot::ECS::ModelComponent::getID(), ICON_FA_CUBE "  Model");
        inspector.registerComponentDisplayName(Carrot::ECS::AnimatedModelComponent::getID(), ICON_FA_CUBE "  AnimatedModel");
        inspector.registerComponentDisplayName(Carrot::ECS::CameraComponent::getID(), ICON_FA_CAMERA_RETRO "  Camera");
        inspector.registerComponentDisplayName(Carrot::ECS::RigidBodyComponent::getID(), ICON_FA_CUBES "  Rigidbody");
        inspector.registerComponentDisplayName(Carrot::ECS::PhysicsCharacterComponent::getID(), ICON_FA_CHILD "  Physics Character");
        inspector.registerComponentDisplayName(Carrot::ECS::SpriteComponent::getID(), ICON_FA_IMAGE "  Sprite");
        inspector.registerComponentDisplayName(Carrot::ECS::TextComponent::getID(), ICON_FA_FONT "  Text");
        inspector.registerComponentDisplayName(Carrot::ECS::NavMeshComponent::getID(), ICON_FA_ROUTE "  NavMesh");
        inspector.registerComponentDisplayName(Carrot::ECS::SoundListenerComponent::getID(), ICON_FA_PODCAST "  SoundListener");
    }
}
