//
// Created by jglrxavpok on 03/06/2023.
//

#include "EditorFunctions.h"
#include <engine/Engine.h>
#include <engine/ecs/components/CSharpComponent.h>
#include <engine/scripting/CSharpReflectionHelper.h>
#include <Peeler.h>

namespace Peeler {
    using namespace Carrot;

    // component drawing methods

#define MAKE_ID(PROP)               \
    std::string id;                 \
    id += (PROP).displayName;       \
    id += "##";                     \
    id += (PROP).fieldName;         \
    id += "-";                      \
    id += __FUNCTION__;

    static void drawIntProperty(EditContext& edition, Carrot::Scripting::CSObject& csComponent, Scripting::ComponentProperty& property) {
        std::int32_t value = *((std::int32_t*)mono_object_unbox(property.field->get(csComponent)));

        bool changed = false;

        MAKE_ID(property);

        if(property.intRange.has_value()) {
            std::int32_t min = std::min(property.intRange->min, property.intRange->max);
            std::int32_t max = std::max(property.intRange->min, property.intRange->max);

            // ImGui limits
            bool useSlider = true;
            if(min <= -INT_MAX) {
                useSlider = false;
            }
            if(max >= INT_MAX) {
                useSlider = false;
            }

            if(useSlider) {
                changed |= ImGui::SliderInt(id.c_str(), &value, min, max);
            } else {
                changed |= ImGui::DragInt(id.c_str(), &value, 0.1f, min, max);
            }
        } else {
            changed |= ImGui::DragInt(id.c_str(), &value, 0.1f/*TODO: speed attribute*/, 0.0f, 0.0f);
        }

        if(changed) {
            property.field->set(csComponent, Scripting::CSObject((MonoObject*)&value));
            edition.hasModifications = true;
        }
    }

    static void drawFloatProperty(EditContext& edition, Carrot::Scripting::CSObject& csComponent, Scripting::ComponentProperty& property) {
        float value = *((float*)mono_object_unbox(property.field->get(csComponent)));

        bool changed = false;

        MAKE_ID(property);

        if(property.floatRange.has_value()) {
            float min = std::min(property.floatRange->min, property.floatRange->max);
            float max = std::max(property.floatRange->min, property.floatRange->max);

            // ImGui limits
            bool useSlider = true;
            if(min < -FLT_MAX/2.0f) {
                min = -FLT_MAX/2.0f;
                useSlider = false;
            }
            if(max > FLT_MAX/2.0f) {
                max = FLT_MAX/2.0f;
                useSlider = false;
            }

            if(useSlider) {
                changed |= ImGui::SliderFloat(id.c_str(), &value, min, max);
            } else {
                changed |= ImGui::DragFloat(id.c_str(), &value, 0.1f, min, max);
            }
        } else {
            changed |= ImGui::DragFloat(id.c_str(), &value, 0.1f/*TODO: speed attribute*/, 0.0f, 0.0f);
        }

        if(changed) {
            property.field->set(csComponent, Scripting::CSObject((MonoObject*)&value));
            edition.hasModifications = true;
        }
    }

    static void drawBooleanProperty(EditContext& edition, Carrot::Scripting::CSObject& csComponent, Scripting::ComponentProperty& property) {
        bool value = *((bool*)mono_object_unbox(property.field->get(csComponent)));

        bool changed = false;

        MAKE_ID(property);

        changed |= ImGui::Checkbox(id.c_str(), &value);

        if(changed) {
            property.field->set(csComponent, Scripting::CSObject((MonoObject*)&value));
            edition.hasModifications = true;
        }
    }

    static void drawEntityProperty(EditContext& edition, Carrot::Scripting::CSObject& csComponent, Scripting::ComponentProperty& property) {
        ECS::Entity value = GetCSharpBindings().convertToEntity(property.field->get(csComponent));

        bool changed = false;

        MAKE_ID(property);

        changed |= edition.inspector.drawPickEntityWidget(id.c_str(), &value);

        if(changed) {
            property.field->set(csComponent, *GetCSharpBindings().entityToCSObject(value));
            edition.hasModifications = true;
        }
    }

    static void drawUserDefinedProperty(EditContext& edition, Carrot::Scripting::CSObject& csComponent, Scripting::ComponentProperty& property) {
        ImGui::Text("%s (%s) - User defined type", property.displayName.c_str(), property.typeStr.c_str());
    }

    void editCSharpComponent(EditContext& edition, Carrot::ECS::CSharpComponent* component) {
        if(!component->isLoaded()) {
            ImGui::Text("Not found inside assembly");
            return;
        }

        for(auto& property : component->getProperties()) {

            switch(property.type) {
                case Scripting::ComponentType::Int:
                    drawIntProperty(edition, component->getCSComponentObject(), property);
                    break;

                case Scripting::ComponentType::Float:
                    drawFloatProperty(edition, component->getCSComponentObject(), property);
                    break;

                case Scripting::ComponentType::Boolean:
                    drawBooleanProperty(edition, component->getCSComponentObject(), property);
                    break;

                case Scripting::ComponentType::Entity:
                    drawEntityProperty(edition, component->getCSComponentObject(), property);
                    break;

                case Scripting::ComponentType::UserDefined:
                    drawUserDefinedProperty(edition, component->getCSComponentObject(), property);
                    break;

                default:
                    TODO; // missing case!
                    break;
            }
        }
    }
}