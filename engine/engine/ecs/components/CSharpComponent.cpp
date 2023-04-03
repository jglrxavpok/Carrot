#include "CSharpComponent.h"
#include <core/scripting/csharp/CSClass.h>
#include <core/scripting/csharp/Engine.h>
#include <core/async/ParallelMap.hpp>
#include <engine/utils/Macros.h>
#include <engine/Engine.h>

#include "../../../../editor/src/Peeler.h" // TODO: this is quite ugly, find a cleaner way

namespace Carrot::ECS {

    CSharpComponent::CSharpComponent(Carrot::ECS::Entity entity, const std::string& namespaceName, const std::string& className) : Carrot::ECS::Component(
            std::move(entity)), namespaceName(namespaceName), className(className) {
        loadCallbackHandle = GetCSharpBindings().registerGameAssemblyLoadCallback([&]() { onAssemblyLoad(); });
        unloadCallbackHandle = GetCSharpBindings().registerGameAssemblyUnloadCallback([&]() { onAssemblyUnload(); });

        init();
        refresh();
    }

    CSharpComponent::CSharpComponent(const rapidjson::Value& json, Carrot::ECS::Entity entity, const std::string& namespaceName, const std::string& className) : Carrot::ECS::Component(
            std::move(entity)), namespaceName(namespaceName), className(className) {
        loadCallbackHandle = GetCSharpBindings().registerGameAssemblyLoadCallback([&]() { onAssemblyLoad(); });
        unloadCallbackHandle = GetCSharpBindings().registerGameAssemblyUnloadCallback([&]() { onAssemblyUnload(); });

        // TODO: load from JSON
        init();
        refresh();
    }

    CSharpComponent::~CSharpComponent() {
        GetCSharpBindings().unregisterGameAssemblyLoadCallback(loadCallbackHandle);
        GetCSharpBindings().unregisterGameAssemblyUnloadCallback(unloadCallbackHandle);
    }

    std::unique_ptr <Carrot::ECS::Component> CSharpComponent::duplicate(const Carrot::ECS::Entity& newOwner) const {
        auto result = std::make_unique<CSharpComponent>(newOwner, namespaceName, className);
        // TODO;
        return result;
    }

    rapidjson::Value CSharpComponent::toJSON(rapidjson::Document& doc) const {
        //TODO;
        return rapidjson::Value{};
    }

    const char *const CSharpComponent::getName() const {
        return fullName.c_str();
    }

    void CSharpComponent::drawInspectorInternals(const Carrot::Render::Context& renderContext, bool& modified) {
        if(!foundInAssemblies) {
            ImGui::Text("Not found inside assembly");
            return;
        }

        for(auto& property : componentProperties) {

            switch(property.type) {
                case Scripting::ComponentType::Int:
                    drawIntProperty(property);
                    break;

                case Scripting::ComponentType::Float:
                    drawFloatProperty(property);
                    break;

                case Scripting::ComponentType::Boolean:
                    drawBooleanProperty(property);
                    break;

                case Scripting::ComponentType::Entity:
                    drawEntityProperty(property);
                    break;

                case Scripting::ComponentType::UserDefined:
                    drawUserDefinedProperty(property);
                    break;

                default:
                    TODO; // missing case!
                    break;
            }
        }
    }

    ComponentID CSharpComponent::getComponentTypeID() const {
        return componentID;
    }

    Scripting::CSObject& CSharpComponent::getCSComponentObject() {
        verify(csComponent, "No component loaded at this point");
        return *csComponent;
    }

    void CSharpComponent::init() {
        fullName = namespaceName;
        fullName += '.';
        fullName += className;
        fullName += " (C#)";

        componentID = GetCSharpBindings().requestComponentID(namespaceName, className);
    }

    void CSharpComponent::refresh() {
        componentProperties.clear();
        Scripting::CSClass* clazz = GetCSharpScripting().findClass(namespaceName, className);
        if(!clazz) {
            foundInAssemblies = false;
            return;
        }
        foundInAssemblies = true;

        auto pEntity = GetCSharpBindings().entityToCSObject(getEntity());
        void* args[1] { (MonoObject*)(*pEntity) };
        csComponent = clazz->newObject(args);

        componentProperties = GetCSharpBindings().findAllComponentProperties(namespaceName, className);
    }

    void CSharpComponent::onAssemblyLoad() {
        refresh();
    }

    void CSharpComponent::onAssemblyUnload() {
        csComponent = nullptr;
    }

    // component drawing methods

#define MAKE_ID(PROP)               \
    std::string id;                 \
    id += PROP.displayName;         \
    id += "##";                     \
    id += PROP.fieldName;           \
    id += "-";                      \
    id += __FUNCTION__;

    void CSharpComponent::drawIntProperty(Scripting::ComponentProperty& property) {
        std::int32_t value = *((std::int32_t*)mono_object_unbox(property.field->get(*csComponent)));

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
            property.field->set(*csComponent, Scripting::CSObject((MonoObject*)&value));
        }
    }

    void CSharpComponent::drawFloatProperty(Scripting::ComponentProperty& property) {
        float value = *((float*)mono_object_unbox(property.field->get(*csComponent)));

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
            property.field->set(*csComponent, Scripting::CSObject((MonoObject*)&value));
        }
    }

    void CSharpComponent::drawBooleanProperty(Scripting::ComponentProperty& property) {
        bool value = *((bool*)mono_object_unbox(property.field->get(*csComponent)));

        bool changed = false;

        MAKE_ID(property);

        changed |= ImGui::Checkbox(id.c_str(), &value);

        if(changed) {
            property.field->set(*csComponent, Scripting::CSObject((MonoObject*)&value));
        }
    }

    void CSharpComponent::drawEntityProperty(Scripting::ComponentProperty& property) {
        ECS::Entity value = GetCSharpBindings().convertToEntity(property.field->get(*csComponent));

        bool changed = false;

        MAKE_ID(property);

        changed |= Peeler::Instance->drawPickEntityWidget(id.c_str(), value);

        if(changed) {
            property.field->set(*csComponent, *GetCSharpBindings().entityToCSObject(value));
        }
    }

    void CSharpComponent::drawUserDefinedProperty(Scripting::ComponentProperty& property) {
        ImGui::Text("%s (%s) - User defined type", property.displayName.c_str(), property.typeStr.c_str());
    }

} // Carrot::ECS