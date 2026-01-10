//
// Created by jglrxavpok on 03/06/2023.
//

#include "EditorFunctions.h"
#include <engine/Engine.h>
#include <engine/ecs/components/CSharpComponent.h>
#include <engine/scripting/CSharpReflectionHelper.h>
#include <Peeler.h>
#include <core/io/Logging.hpp>
#include <engine/ecs/systems/CSharpLogicSystem.h>
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

    template<typename TThing>
    static void drawIntProperty(EditContext& edition, const Carrot::Vector<TThing*>& components, Scripting::ReflectionProperty& property) {
        multiEditField<TThing, std::int32_t>(edition, property.displayName.c_str(), components,
            [&](TThing& c) {
                return *((std::int32_t*)mono_object_unbox(property.field->get(c.getCSObject())));
            },
            [&](TThing& c, const std::int32_t& v) {
                property.field->set(c.getCSObject(), Scripting::CSObject((MonoObject*)&v));
            },
            Helpers::Limits<std::int32_t> {
                .min = property.intRange.has_value() ? property.intRange->min : std::numeric_limits<std::int32_t>::min(),
                .max = property.intRange.has_value() ? property.intRange->max : std::numeric_limits<std::int32_t>::max(),
            }
        );
    }

    template<typename TThing>
    static void drawFloatProperty(EditContext& edition, const Carrot::Vector<TThing*>& components, Scripting::ReflectionProperty& property) {
        multiEditField<TThing, float>(edition, property.displayName.c_str(), components,
            [&](TThing& c) {
                return *((float*)mono_object_unbox(property.field->get(c.getCSObject())));
            },
            [&](TThing& c, const float& v) {
                property.field->set(c.getCSObject(), Scripting::CSObject((MonoObject*)&v));
            },
            Helpers::Limits<float> {
                .min = property.floatRange.has_value() ? property.floatRange->min : FLT_MIN,
                .max = property.floatRange.has_value() ? property.floatRange->max : FLT_MAX,
            }
        );
    }

    template<typename TThing>
    static void drawDoubleProperty(EditContext& edition, const Carrot::Vector<TThing*>& components, Scripting::ReflectionProperty& property) {
        multiEditField<TThing, float>(edition, property.displayName.c_str(), components,
            [&](TThing& c) {
                return (float)*((double*)mono_object_unbox(property.field->get(c.getCSObject())));
            },
            [&](TThing& c, const float& v) {
                double converted = v;
                property.field->set(c.getCSObject(), Scripting::CSObject((MonoObject*)&converted));
            },
            Helpers::Limits<float> {
                .min = property.floatRange.has_value() ? property.floatRange->min : FLT_MIN,
                .max = property.floatRange.has_value() ? property.floatRange->max : FLT_MAX,
            }
        );
    }

    template<typename TThing>
    static void drawBooleanProperty(EditContext& edition, const Carrot::Vector<TThing*>& components, Scripting::ReflectionProperty& property) {
        multiEditField<TThing, bool>(edition, property.displayName.c_str(), components,
            [&](TThing& c) {
                return *((bool*)mono_object_unbox(property.field->get(c.getCSObject())));
            },
            [&](TThing& c, const bool& v) {
                bool tmp = v;
                property.field->set(c.getCSObject(), Scripting::CSObject((MonoObject*)&tmp));
            }
        );
    }

    template<typename TThing>
    static void drawVec2Property(EditContext& edition, const Carrot::Vector<TThing*>& components, Scripting::ReflectionProperty& property) {
        multiEditField<TThing, glm::vec2>(edition, property.displayName.c_str(), components,
            [&](TThing& c) {
                return *(glm::vec2*)mono_object_unbox(property.field->get(c.getCSObject()).toMono());
            },
            [&](TThing& c, const glm::vec2& v) {
                property.field->set(c.getCSObject(), Scripting::CSObject((MonoObject*)&v));
            }
        );
    }

    template<typename TThing>
    static void drawVec3Property(EditContext& edition, const Carrot::Vector<TThing*>& components, Scripting::ReflectionProperty& property) {
        multiEditField<TThing, glm::vec3>(edition, property.displayName.c_str(), components,
            [&](TThing& c) {
                return *(glm::vec3*)mono_object_unbox(property.field->get(c.getCSObject()).toMono());
            },
            [&](TThing& c, const glm::vec3& v) {
                property.field->set(c.getCSObject(), Scripting::CSObject((MonoObject*)&v));
            }
        );
    }

    template<typename TThing>
    static void drawEntityProperty(EditContext& edition, const Carrot::Vector<TThing*>& components, Scripting::ReflectionProperty& property) {
        multiEditField<TThing, ECS::Entity>(edition, property.displayName.c_str(), components,
            [&](TThing& c) {
                ECS::Entity value = GetCSharpBindings().convertToEntity(property.field->get(c.getCSObject()));
                return value;
            },
            [&](TThing& c, const ECS::Entity& v) {
                ECS::Entity copy = v; // entityToCSObject takes Entity& not const Entity&
                property.field->set(c.getCSObject(), *GetCSharpBindings().entityToCSObject(copy));
            }
        );
    }

    template<typename TThing>
    static void drawStringProperty(EditContext& edition, const Carrot::Vector<TThing*>& components, Scripting::ReflectionProperty& property) {
        multiEditField<TThing, std::string>(edition, property.displayName.c_str(), components,
            [&](TThing& c) {
                MonoString* text = reinterpret_cast<MonoString *>(property.field->get(c.getCSObject()).toMono());
                char* textReadable = mono_string_to_utf8(text);
                CLEANUP(mono_free(textReadable));
                return std::string{textReadable};
            },
            [&](TThing& c, const std::string& v) {
                property.field->set(c.getCSObject(), Scripting::CSObject(reinterpret_cast<MonoObject*>(mono_string_new_wrapper(v.c_str()))));
            }
        );
    }

    template<typename TThing>
    static void drawUserDefinedProperty(EditContext& edition, const Carrot::Vector<TThing*>& components, Scripting::ReflectionProperty& property) {
        ImGui::Text("%s (%s) - User defined type", property.displayName.c_str(), property.typeStr.c_str());
    }

    template<typename TThing>
    static void drawUserEnumProperty(EditContext& edition, const Carrot::Vector<TThing*>& components, Scripting::ReflectionProperty& property) {
        // will refer to each enum values by its name
        multiEditEnumField<TThing, std::string>(edition, property.displayName.c_str(), components,
            [&](TThing& c) {
                return GetCSharpBindings().enumValueToString(property.field->get(c.getCSObject()));
            },
            [&](TThing& c, const std::string& v) {
                const Scripting::CSObject enumValue = GetCSharpBindings().stringToEnumValue(property.typeStr, v);
                property.field->set(c.getCSObject(), enumValue);
            },
            +[](const std::string& s) {
                return s.c_str();
            },
            property.validUserEnumValues
        );
    }

    /**
     * Edit a CSharp system or component
     * @tparam TThing CSharpSystem or CSharpComponent
     * @param edition
     * @param things
     */
    template<typename TThing>
    static void editCSharpThing(EditContext& edition, const Carrot::Vector<TThing*>& things) {
        for(auto& property : things[0]->getProperties()) {

            switch(property.type) {
                case Scripting::ComponentType::Int:
                    drawIntProperty(edition, things, property);
                    break;


                case Scripting::ComponentType::Float:
                    drawFloatProperty(edition, things, property);
                    break;

                case Scripting::ComponentType::Double:
                    drawDoubleProperty(edition, things, property);
                    break;

                case Scripting::ComponentType::Boolean:
                    drawBooleanProperty(edition, things, property);
                    break;

                case Scripting::ComponentType::Vec2:
                    drawVec2Property(edition, things, property);
                    break;

                case Scripting::ComponentType::Vec3:
                    drawVec3Property(edition, things, property);
                    break;

                case Scripting::ComponentType::Entity:
                    drawEntityProperty(edition, things, property);
                    break;

                case Scripting::ComponentType::String:
                    drawStringProperty(edition, things, property);
                    break;

                case Scripting::ComponentType::UserEnum:
                    drawUserEnumProperty(edition, things, property);
                    break;

                case Scripting::ComponentType::UserDefined:
                    drawUserDefinedProperty(edition, things, property);
                    break;

                default:
                    TODO; // missing case!
                    break;
            }
        }
    }

    void editCSharpComponent(EditContext& edition, const Carrot::Vector<Carrot::ECS::CSharpComponent*>& components) {
        if(!components[0]->isLoaded()) {
            ImGui::Text("Not found inside assembly");
            return;
        }

        editCSharpThing(edition, components);
    }

    void editCSharpSystem(EditContext& edition, Carrot::ECS::CSharpLogicSystem& system) {
        if(!system.isLoaded()) {
            ImGui::Text("Not found inside assembly");
            return;
        }

        editCSharpThing(edition, Vector{&system});
    }
}