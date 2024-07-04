//
// Created by jglrxavpok on 03/06/2023.
//

#include "EditorFunctions.h"
#include <engine/Engine.h>
#include <engine/ecs/components/CSharpComponent.h>
#include <engine/scripting/CSharpReflectionHelper.h>
#include <Peeler.h>
#include <core/io/Logging.hpp>
#include <glm/gtc/type_ptr.hpp>

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

    static void drawIntProperty(EditContext& edition, const Carrot::Vector<Carrot::ECS::CSharpComponent*>& components, Scripting::ComponentProperty& property) {
        multiEditField<Carrot::ECS::CSharpComponent, std::int32_t>(edition, property.displayName.c_str(), components,
            [&](Carrot::ECS::CSharpComponent& c) {
                return *((std::int32_t*)mono_object_unbox(property.field->get(c.getCSComponentObject())));
            },
            [&](Carrot::ECS::CSharpComponent& c, const std::int32_t& v) {
                property.field->set(c.getCSComponentObject(), Scripting::CSObject((MonoObject*)&v));
            },
            Helpers::Limits<std::int32_t> {
                .min = property.intRange.has_value() ? property.intRange->min : std::numeric_limits<std::int32_t>::min(),
                .max = property.intRange.has_value() ? property.intRange->max : std::numeric_limits<std::int32_t>::max(),
            }
        );
    }

    static void drawFloatProperty(EditContext& edition, const Carrot::Vector<Carrot::ECS::CSharpComponent*>& components, Scripting::ComponentProperty& property) {
        multiEditField<Carrot::ECS::CSharpComponent, float>(edition, property.displayName.c_str(), components,
            [&](Carrot::ECS::CSharpComponent& c) {
                return *((float*)mono_object_unbox(property.field->get(c.getCSComponentObject())));
            },
            [&](Carrot::ECS::CSharpComponent& c, const float& v) {
                property.field->set(c.getCSComponentObject(), Scripting::CSObject((MonoObject*)&v));
            },
            Helpers::Limits<float> {
                .min = property.floatRange.has_value() ? property.floatRange->min : FLT_MIN,
                .max = property.floatRange.has_value() ? property.floatRange->max : FLT_MAX,
            }
        );
    }

    static void drawDoubleProperty(EditContext& edition, const Carrot::Vector<Carrot::ECS::CSharpComponent*>& components, Scripting::ComponentProperty& property) {
        multiEditField<Carrot::ECS::CSharpComponent, float>(edition, property.displayName.c_str(), components,
            [&](Carrot::ECS::CSharpComponent& c) {
                return (float)*((double*)mono_object_unbox(property.field->get(c.getCSComponentObject())));
            },
            [&](Carrot::ECS::CSharpComponent& c, const float& v) {
                double converted = v;
                property.field->set(c.getCSComponentObject(), Scripting::CSObject((MonoObject*)&converted));
            },
            Helpers::Limits<float> {
                .min = property.floatRange.has_value() ? property.floatRange->min : FLT_MIN,
                .max = property.floatRange.has_value() ? property.floatRange->max : FLT_MAX,
            }
        );
    }

    static void drawBooleanProperty(EditContext& edition, const Carrot::Vector<Carrot::ECS::CSharpComponent*>& components, Scripting::ComponentProperty& property) {
        multiEditField<Carrot::ECS::CSharpComponent, bool>(edition, property.displayName.c_str(), components,
            [&](Carrot::ECS::CSharpComponent& c) {
                return *((bool*)mono_object_unbox(property.field->get(c.getCSComponentObject())));
            },
            [&](Carrot::ECS::CSharpComponent& c, const bool& v) {
                bool tmp = v;
                property.field->set(c.getCSComponentObject(), Scripting::CSObject((MonoObject*)&tmp));
            }
        );
    }

    static void drawVec2Property(EditContext& edition, const Carrot::Vector<Carrot::ECS::CSharpComponent*>& components, Scripting::ComponentProperty& property) {
        multiEditField<Carrot::ECS::CSharpComponent, glm::vec2>(edition, property.displayName.c_str(), components,
            [&](Carrot::ECS::CSharpComponent& c) {
                return *(glm::vec2*)mono_object_unbox(property.field->get(c.getCSComponentObject()).toMono());
            },
            [&](Carrot::ECS::CSharpComponent& c, const glm::vec2& v) {
                property.field->set(c.getCSComponentObject(), Scripting::CSObject((MonoObject*)&v));
            }
        );
    }

    static void drawVec3Property(EditContext& edition, const Carrot::Vector<Carrot::ECS::CSharpComponent*>& components, Scripting::ComponentProperty& property) {
        multiEditField<Carrot::ECS::CSharpComponent, glm::vec3>(edition, property.displayName.c_str(), components,
            [&](Carrot::ECS::CSharpComponent& c) {
                return *(glm::vec3*)mono_object_unbox(property.field->get(c.getCSComponentObject()).toMono());
            },
            [&](Carrot::ECS::CSharpComponent& c, const glm::vec3& v) {
                property.field->set(c.getCSComponentObject(), Scripting::CSObject((MonoObject*)&v));
            }
        );
    }

    static void drawEntityProperty(EditContext& edition, const Carrot::Vector<Carrot::ECS::CSharpComponent*>& components, Scripting::ComponentProperty& property) {
        multiEditField<Carrot::ECS::CSharpComponent, ECS::Entity>(edition, property.displayName.c_str(), components,
            [&](Carrot::ECS::CSharpComponent& c) {
                ECS::Entity value = GetCSharpBindings().convertToEntity(property.field->get(c.getCSComponentObject()));
                return value;
            },
            [&](Carrot::ECS::CSharpComponent& c, const ECS::Entity& v) {
                ECS::Entity copy = v; // entityToCSObject takes Entity& not const Entity&
                property.field->set(c.getCSComponentObject(), *GetCSharpBindings().entityToCSObject(copy));
            }
        );
    }

    static void drawStringProperty(EditContext& edition, const Carrot::Vector<Carrot::ECS::CSharpComponent*>& components, Scripting::ComponentProperty& property) {
        multiEditField<Carrot::ECS::CSharpComponent, std::string>(edition, property.displayName.c_str(), components,
            [&](Carrot::ECS::CSharpComponent& c) {
                MonoString* text = reinterpret_cast<MonoString *>(property.field->get(c.getCSComponentObject()).toMono());
                char* textReadable = mono_string_to_utf8(text);
                CLEANUP(mono_free(textReadable));
                return std::string{textReadable};
            },
            [&](Carrot::ECS::CSharpComponent& c, const std::string& v) {
                property.field->set(c.getCSComponentObject(), Scripting::CSObject(reinterpret_cast<MonoObject*>(mono_string_new_wrapper(v.c_str()))));
            }
        );
    }

    static void drawUserDefinedProperty(EditContext& edition, const Carrot::Vector<Carrot::ECS::CSharpComponent*>& components, Scripting::ComponentProperty& property) {
        ImGui::Text("%s (%s) - User defined type", property.displayName.c_str(), property.typeStr.c_str());
    }

    static void drawUserEnumProperty(EditContext& edition, const Carrot::Vector<Carrot::ECS::CSharpComponent*>& components, Scripting::ComponentProperty& property) {
        // will refer to each enum values by its name
        multiEditEnumField<Carrot::ECS::CSharpComponent, std::string>(edition, property.displayName.c_str(), components,
            [&](Carrot::ECS::CSharpComponent& c) {
                return GetCSharpBindings().enumValueToString(property.field->get(c.getCSComponentObject()));
            },
            [&](Carrot::ECS::CSharpComponent& c, const std::string& v) {
                const Scripting::CSObject enumValue = GetCSharpBindings().stringToEnumValue(property.typeStr, v);
                property.field->set(c.getCSComponentObject(), enumValue);
            },
            +[](const std::string& s) {
                return s.c_str();
            },
            property.validUserEnumValues
        );
    }

    void editCSharpComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::CSharpComponent*>& components) {
        if(!components[0]->isLoaded()) {
            ImGui::Text("Not found inside assembly");
            return;
        }

        for(auto& property : components[0]->getProperties()) {

            switch(property.type) {
                case Scripting::ComponentType::Int:
                    drawIntProperty(edition, components, property);
                    break;


                case Scripting::ComponentType::Float:
                    drawFloatProperty(edition, components, property);
                    break;

                case Scripting::ComponentType::Double:
                    drawDoubleProperty(edition, components, property);
                    break;

                case Scripting::ComponentType::Boolean:
                    drawBooleanProperty(edition, components, property);
                    break;

                case Scripting::ComponentType::Vec2:
                    drawVec2Property(edition, components, property);
                    break;

                case Scripting::ComponentType::Vec3:
                    drawVec3Property(edition, components, property);
                    break;

                case Scripting::ComponentType::Entity:
                    drawEntityProperty(edition, components, property);
                    break;

                case Scripting::ComponentType::String:
                    drawStringProperty(edition, components, property);
                    break;

                case Scripting::ComponentType::UserEnum:
                    drawUserEnumProperty(edition, components, property);
                    break;

                case Scripting::ComponentType::UserDefined:
                    drawUserDefinedProperty(edition, components, property);
                    break;

                default:
                    TODO; // missing case!
                    break;
            }
        }
    }
}