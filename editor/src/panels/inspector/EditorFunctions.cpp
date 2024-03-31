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
        /**
         * Wrapper of angles to use a different slider than "regular" floats
         */
        struct AngleWrapper {
            float radianValue;
        };

        /**
         * Wrapper of angles to use a different slider than "regular" floats, but described as their cos value
         */
        struct CosAngleWrapper {
            float cosRadianValue;
        };

        /**
         * Wrapper for vec3 used as RGB color values
         */
        struct RGBColorWrapper {
            glm::vec3 rgb;
        };

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

        template<typename T>
        struct Limits {};

        template<>
        struct Limits<AngleWrapper> {
            float min = -2 * glm::pi<float>();
            float max = 2 * glm::pi<float>();
        };

        template<>
        struct Limits<std::string_view> {
            bool multiline = false;
        };
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

    template<typename T>
    static bool editMultiple(const char* id, std::span<T> values, const Helpers::Limits<T>& limits = {}) = delete;

    // instead of using ImGui::DragFloat3 & co, reimplement it. Because multiple positions are modified at once,
    //  they might not have all the same coordinates. This way, we can modify only the coordinate that has been modified
    template<int ComponentCount>
    static bool editMultipleVec(const char* id, std::span<glm::vec<ComponentCount, float>> vectors, const Helpers::Limits<glm::vec<ComponentCount, float>>& limits) {
        using VecType = glm::vec<ComponentCount, float>;
        bool modified = false;
        ImGui::BeginGroup();
        ImGui::PushID(id);

        ImGui::PushMultiItemsWidths(ComponentCount, ImGui::CalcItemWidth());

        for(std::size_t axisIndex = 0; axisIndex < ComponentCount; axisIndex++) {
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
                for(VecType& vec : vectors) {
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

    template<>
    static bool editMultiple<glm::vec3>(const char* id, std::span<glm::vec3> vectors, const Helpers::Limits<glm::vec3>& limits) {
        return editMultipleVec<3>(id, vectors, limits);
    }

    // maybe merge with function editMultipleVec3, but for now copy paste will do
    template<>
    static bool editMultiple<glm::quat>(const char* id, std::span<glm::quat> values, const Helpers::Limits<glm::quat>& limits) {
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

    template<>
    static bool editMultiple<bool>(const char* id, std::span<bool> values, const Helpers::Limits<bool>& limits) {
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

    template<>
    static bool editMultiple<Carrot::Render::bool32>(const char* id, std::span<Carrot::Render::bool32> values, const Helpers::Limits<Carrot::Render::bool32>& limits) {
        Carrot::Vector<bool> asBools;
        asBools.resize(values.size());
        for(std::int64_t i = 0; i < asBools.size(); i++) {
            asBools[i] = values[i];
        }
        if(editMultiple(id, std::span<bool>(asBools))) {
            for(auto& v : values) {
                v = asBools[0];
            }
            return true;
        }
        return false;
    }

    template<>
    static bool editMultiple<float>(const char* id, std::span<float> values, const Helpers::Limits<float>& limits) {
        float sameValue = values[0];
        bool areSame = true;
        for(std::size_t i = 1; i < values.size(); i++) {
            if(values[i] != sameValue) {
                areSame = false;
                break;
            }
        }

        const char* format = areSame ? "%.3f" : "<VARIOUS>";
        if(ImGui::DragFloat(id, &sameValue, 1, 0, 0, format)) {
            for(float& v : values) {
                v = sameValue;
            }
            return true;
        }

        return false;
    }

    template<>
    static bool editMultiple<Helpers::AngleWrapper>(const char* id, std::span<Helpers::AngleWrapper> values, const Helpers::Limits<Helpers::AngleWrapper>& limits) {
        float sameValue = values[0].radianValue;
        bool areSame = true;
        for(std::size_t i = 1; i < values.size(); i++) {
            if(values[i].radianValue != sameValue) {
                areSame = false;
                break;
            }
        }

        const char* format = areSame ? "%.0f deg" : "<VARIOUS>";
        if(ImGui::SliderAngle(id, &sameValue, Carrot::Math::Rad2Degrees * limits.min, Carrot::Math::Rad2Degrees * limits.max, format)) {
            for(Helpers::AngleWrapper& angle : values) {
                angle.radianValue = sameValue;
            }
            return true;
        }

        return false;
    }

    template<>
    static bool editMultiple<Helpers::CosAngleWrapper>(const char* id, std::span<Helpers::CosAngleWrapper> values, const Helpers::Limits<Helpers::CosAngleWrapper>& limits) {
        Carrot::Vector<Helpers::AngleWrapper> asAngles;
        asAngles.resize(values.size());
        for(std::int64_t i = 0; i < asAngles.size(); i++) {
            asAngles[i].radianValue = glm::acos(values[i].cosRadianValue);
        }
        if(editMultiple(id, std::span<Helpers::AngleWrapper>(asAngles))) {
            for(auto& v : values) {
                v.cosRadianValue = glm::cos(asAngles[0].radianValue);
            }
            return true;
        }
        return false;
    }

    template<>
    static bool editMultiple<Helpers::RGBColorWrapper>(const char* id, std::span<Helpers::RGBColorWrapper> values, const Helpers::Limits<Helpers::RGBColorWrapper>& limits) {
        const glm::vec3& colorValue = values[0].rgb;
        float colorArr[3] { colorValue.r, colorValue.g, colorValue.b };
        if(ImGui::ColorPicker3(id, colorArr)) {
            for(Helpers::RGBColorWrapper& v : values) {
                v.rgb = glm::vec3 { colorArr[0], colorArr[1], colorArr[2] };
            }
            return true;
        }
        return false;
    }

    template<>
    static bool editMultiple<std::string_view>(const char* id, std::span<std::string_view> values, const Helpers::Limits<std::string_view>& limits) {
        static std::string sameValue; // expected to have its contents copied immediatly after returning out of this function
        sameValue = std::string { values[0] };
        for(std::size_t i = 1; i < values.size(); i++) {
            if(values[i] != sameValue) {
                sameValue = "<VARIOUS>";
                break;
            }
        }

        bool modified = false;
        if(limits.multiline) {
            modified = ImGui::InputTextMultiline(id, sameValue);
        } else {
            modified = ImGui::InputText(id, sameValue);
        }

        if(modified) {
            for(std::string_view& v : values) {
                v = sameValue;
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
        std::function<TValue(TComponent& comp)> getter;
        std::function<void(TComponent& comp, const TValue& value)> setter;

        UpdateComponentValues(Application &app, const std::string& desc, std::span<Carrot::ECS::EntityID> _entityList, std::span<TValue> _newValues,
            std::function<TValue(TComponent& comp)> _getter, std::function<void(TComponent& comp, const TValue& value)> _setter)
            : ICommand(app, desc)
            , entityList(_entityList)
            , newValues(_newValues)
            , getter(_getter)
            , setter(_setter)
        {
            oldValues.ensureReserve(newValues.size());
            for(const auto& entityID : _entityList) {
                oldValues.pushBack(getter(app.currentScene.world.getComponent<TComponent>(entityID)));
            }
        }

        void undo() override {
            for(std::size_t i = 0; i < entityList.size(); i++) {
                setter(editor.currentScene.world.getComponent<TComponent>(entityList[i]), oldValues[i]);
            }
        }

        void redo() override {
            for(std::size_t i = 0; i < entityList.size(); i++) {
                setter(editor.currentScene.world.getComponent<TComponent>(entityList[i]), newValues[i]);
            }
        }
    };

    /**
     * Helper function for editing the same property in multiple entities at once. Handles undo stack automatically
     * @tparam TComponent Type of component to edit
     * @tparam TPropertyType Type of property to edit (underlying type, for positions this would be vec3)
     * @param id label used to show user which property they are editing
     * @param components list of components to edit
     * @param getterFunc function to get the property's value
     * @param setterFunc function to set the property's value
     */
    template<typename TComponent, typename TPropertyType>
    static void multiEditField(EditContext& edition, const char* id, const Carrot::Vector<TComponent*> components,
        std::function<TPropertyType(TComponent&)> getterFunc,
        std::function<void(TComponent&, const TPropertyType&)> setterFunc,
        const Helpers::Limits<TPropertyType>& limits = {}
    ) {
        Carrot::Vector<TPropertyType> values;
        values.ensureReserve(components.size());
        for(TComponent* pComponent : components) {
            values.emplaceBack(getterFunc(*pComponent));
        }

        bool modified = editMultiple<TPropertyType>(id, values, limits);

        if(modified) {
            using UpdateProperty = UpdateComponentValues<TComponent, TPropertyType>;
            edition.editor.undoStack.push<UpdateProperty>(Carrot::sprintf("Update %s", id), edition.editor.selectedIDs, values, getterFunc, setterFunc);
            edition.hasModifications = true;
        }
    }

    /**
     * Helper function for editing the same property in multiple entities at once. Handles undo stack automatically
     * @tparam TComponent Type of component to edit
     * @tparam TPropertyType Type of property to edit (underlying type, for positions this would be vec3)
     * @param id label used to show user which property they are editing
     * @param components list of components to edit
     * @param getter function to get the property's value
     * @param setter function to set the property's value
     */
    template<typename TComponent, typename TPropertyType>
    static void multiEditField(EditContext& edition, const char* id, const Carrot::Vector<TComponent*> components,
        TPropertyType(getter)(TComponent&),
        void(setter)(TComponent&, const TPropertyType&),
        const Helpers::Limits<TPropertyType>& limits = {}
    ) {
        multiEditField(edition, id, components, std::function(getter), std::function(setter), limits);
    }

    /**
     * Helper function for editing the same property in multiple entities at once. Handles undo stack automatically
     * @tparam TComponent Type of component to edit
     * @tparam TPropertyType Type of property to edit (underlying type, for positions this would be vec3)
     * @param id label used to show user which property they are editing
     * @param components list of components to edit
     * @param accessor function to access the property's value
     */
    template<typename TComponent, typename TPropertyType>
    static void multiEditField(EditContext& edition, const char* id, const Carrot::Vector<TComponent*> components, TPropertyType&(accessor)(TComponent&), const Helpers::Limits<TPropertyType>& limits = {}) {
        multiEditField(edition, id, components,
            std::function([&](TComponent& c) -> TPropertyType { return accessor(c); }),
            std::function([&](TComponent& c, const TPropertyType& v) { accessor(c) = v; }),
            limits);
    }

    static void editTransformComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::TransformComponent*>& components) {
        multiEditField(edition, "Position", components,
            +[](Carrot::ECS::TransformComponent& t) -> glm::vec3& { return t.localTransform.position; });
        multiEditField(edition, "Rotation", components,
            +[](Carrot::ECS::TransformComponent& t) -> glm::quat& { return t.localTransform.rotation; });
        multiEditField(edition, "Scale", components,
            +[](Carrot::ECS::TransformComponent& t) -> glm::vec3& { return t.localTransform.scale; });
    }

    template<typename TComponent, typename TEnum>
    static void multiEditEnumField(EditContext& edition, const char* id, const Carrot::Vector<TComponent*>& components, TEnum(getter)(TComponent&), void(setter)(TComponent&, const TEnum&),
        const char*(nameGetter)(TEnum),
        const Carrot::Vector<TEnum>& validValues,
        const Helpers::Limits<TEnum>& limits = {})
    {
        Carrot::Vector<TEnum> values;
        values.ensureReserve(components.size());
        TEnum currentlySelectedValue = getter(*components[0]);
        bool allSame = true;
        for(TComponent* pComponent : components) {
            values.emplaceBack(getter(*pComponent));
            allSame = currentlySelectedValue == values[values.size()-1];
        }

        bool modified = false;
        const char* previewValue = allSame ? Carrot::Render::Light::nameOf(currentlySelectedValue) : "<VARIOUS>";
        if(ImGui::BeginCombo(id, previewValue)) {
            auto selectable = [&](TEnum entry) {
                bool selected = entry == currentlySelectedValue;
                if(ImGui::Selectable(nameGetter(entry), &selected)) {
                    modified = true;
                    for(auto& v : values) {
                        v = entry;
                    }
                }
            };

            for(const auto& validValue : validValues) {
                selectable(validValue);
            }

            ImGui::EndCombo();
        }

        if(modified) {
            using UpdateProperty = UpdateComponentValues<TComponent, TEnum>;
            edition.editor.undoStack.push<UpdateProperty>(Carrot::sprintf("Update %s", id), edition.editor.selectedIDs, values, getter, setter);
            edition.hasModifications = true;
        }
    }

    static void editLightComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::LightComponent*>& components) {
        multiEditField(edition, "Enabled", components,
            +[](Carrot::ECS::LightComponent& c) -> Carrot::Render::bool32& { return c.lightRef->light.enabled; });

        multiEditEnumField(edition, "Light type", components,
            +[](Carrot::ECS::LightComponent& c) { return c.lightRef->light.type ; },
            +[](Carrot::ECS::LightComponent& c, const Carrot::Render::LightType& v) { c.lightRef->light.type = v; },
            Carrot::Render::Light::nameOf, { Carrot::Render::LightType::Point, Carrot::Render::LightType::Directional, Carrot::Render::LightType::Spot });

        multiEditField(edition, "Intensity", components,
            +[](Carrot::ECS::LightComponent& c) -> float& { return c.lightRef->light.intensity; });

        if(ImGui::CollapsingHeader("Parameters")) {
            bool allSameType = true;
            Carrot::Render::LightType lightType = components[0]->lightRef->light.type;
            for(std::size_t i = 1; i < components.size(); i++) {
                if(lightType != components[i]->lightRef->light.type) {
                    allSameType = false;
                    break;
                }
            }
            if(allSameType) {
                switch (lightType) {
                    case Carrot::Render::LightType::Spot: {
                        multiEditField(edition, "Cutoff angle", components,
                            +[](Carrot::ECS::LightComponent& c) { return Helpers::CosAngleWrapper { c.lightRef->light.cutoffCosAngle }; },
                            +[](Carrot::ECS::LightComponent& c, const Helpers::CosAngleWrapper& v) { c.lightRef->light.cutoffCosAngle = v.cosRadianValue; });
                        multiEditField(edition, "Outer cutoff angle", components,
                            +[](Carrot::ECS::LightComponent& c) { return Helpers::CosAngleWrapper { c.lightRef->light.outerCutoffCosAngle }; },
                            +[](Carrot::ECS::LightComponent& c, const Helpers::CosAngleWrapper& v) { c.lightRef->light.outerCutoffCosAngle = v.cosRadianValue; });
                    } break;

                    case Carrot::Render::LightType::Point: {
                        multiEditField(edition, "Constant attenuation", components,
                            +[](Carrot::ECS::LightComponent& c) -> float& { return c.lightRef->light.constantAttenuation; });
                        multiEditField(edition, "Linear attenuation", components,
                            +[](Carrot::ECS::LightComponent& c) -> float& { return c.lightRef->light.linearAttenuation; });
                        multiEditField(edition, "Quadratic attenuation", components,
                            +[](Carrot::ECS::LightComponent& c) -> float& { return c.lightRef->light.quadraticAttenuation; });
                    } break;

                    default: {
                        ImGui::Text("Unhandled light type :(");
                    } break;
                }
            } else {
                ImGui::Text("Lights are not all of same type, cannot edit them at the same time.");
            }
        }

        multiEditField(edition, "Light color", components,
            +[](Carrot::ECS::LightComponent& c) { return Helpers::RGBColorWrapper { .rgb = c.lightRef->light.color }; },
            +[](Carrot::ECS::LightComponent& c, const Helpers::RGBColorWrapper& v) { c.lightRef->light.color = v.rgb; });
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

    void editKinematicsComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::Kinematics*>& components) {
        multiEditField(edition, "Velocity", components,
            +[](Carrot::ECS::Kinematics& c) -> glm::vec3& { return c.velocity; });
    }

    void editForceSinPositionComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::ForceSinPosition*>& components) {
        multiEditField(edition, "Angular Frequency", components,
            +[](Carrot::ECS::ForceSinPosition& c) -> glm::vec3& { return c.angularFrequency; });
        multiEditField(edition, "Amplitude", components,
            +[](Carrot::ECS::ForceSinPosition& c) -> glm::vec3& { return c.amplitude; });
        multiEditField(edition, "Angular Offset", components,
            +[](Carrot::ECS::ForceSinPosition& c) -> glm::vec3& { return c.angularOffset; });
        multiEditField(edition, "Center Position", components,
            +[](Carrot::ECS::ForceSinPosition& c) -> glm::vec3& { return c.centerPosition; });
    }

    void editCameraComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::CameraComponent*>& components) {
        multiEditField(edition, "Primary", components,
            +[](Carrot::ECS::CameraComponent& c) -> bool& { return c.isPrimary; });

        Carrot::Vector<bool> isOrthoFields;
        isOrthoFields.resize(components.size());
        for(std::int64_t i = 0; i < components.size(); i++) {
            isOrthoFields[i] = components[i]->isOrthographic;
        }

        multiEditField(edition, "Orthographic", components,
            +[](Carrot::ECS::CameraComponent& c) -> bool& { return c.isOrthographic; });

        if(Helpers::all(isOrthoFields)) {
            multiEditField(edition, "Bounds", components,
                +[](Carrot::ECS::CameraComponent& c) -> glm::vec3& { return c.orthoSize; });
        } else if(!Helpers::any(isOrthoFields)) {
            multiEditField(edition, "FOV", components,
                +[](Carrot::ECS::CameraComponent& c) { return Helpers::AngleWrapper{ c.perspectiveFov }; },
                +[](Carrot::ECS::CameraComponent& c, const Helpers::AngleWrapper& v) { c.perspectiveFov = v.radianValue; },
                Helpers::Limits<Helpers::AngleWrapper> { .min = 0.0001f, .max = 360.0f });
            multiEditField(edition, "Z Near", components, +[](Carrot::ECS::CameraComponent& c) -> float& { return c.perspectiveNear; });
            multiEditField(edition, "Z Far", components, +[](Carrot::ECS::CameraComponent& c) -> float& { return c.perspectiveFar; });
        } else {
            ImGui::Text("Cannot edit mix of orthographic and perspective cameras at the same time.");
        }
    }

    void editTextComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::TextComponent*>& components) {
        multiEditField(edition, "Content", components,
            +[](Carrot::ECS::TextComponent& c) { return c.getText(); },
            +[](Carrot::ECS::TextComponent& c, const std::string_view& v) { c.setText(v); },
            Helpers::Limits<std::string_view> { .multiline = true });
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
        /*registerFunction(inspector, editLuaScriptComponent);
        registerFunction(inspector, editModelComponent);
        registerFunction(inspector, editAnimatedModelComponent);
        registerFunction(inspector, editRigidBodyComponent);
        registerFunction(inspector, editSpriteComponent);*/
        registerFunction(inspector, editTextComponent);
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
