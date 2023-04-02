#include "CSharpComponent.h"
#include <core/scripting/csharp/CSClass.h>
#include <core/scripting/csharp/Engine.h>
#include <core/async/ParallelMap.hpp>
#include <engine/utils/Macros.h>
#include <engine/Engine.h>

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

        // TODO

        if(changed) {
            MonoObject* newObj = mono_value_box(GetCSharpScripting().getRootDomain(), mono_get_int32_class(), &value);
            property.field->set(*csComponent, Scripting::CSObject(newObj));
        }
    }

    void CSharpComponent::drawFloatProperty(Scripting::ComponentProperty& property) {
        float value = *((float*)mono_object_unbox(property.field->get(*csComponent)));

        bool changed = false;

        MAKE_ID(property);

        if(property.floatRange.has_value()) {
            changed |= ImGui::SliderFloat(id.c_str(), &value, property.floatRange->min, property.floatRange->max);
        } else {
            changed |= ImGui::DragFloat(id.c_str(), &value, 0.1f/*TODO: speed attribute*/, 0.0f, 0.0f);
        }

        if(changed) {
            //MonoObject* newObj = mono_value_box(GetCSharpBindings().getAppDomain(), mono_get_single_class(), &value);
            property.field->set(*csComponent, Scripting::CSObject((MonoObject*)&value));
        }
    }

    void CSharpComponent::drawBooleanProperty(Scripting::ComponentProperty& property) {
        // TODO
    }

    void CSharpComponent::drawEntityProperty(Scripting::ComponentProperty& property) {
        // TODO
    }

    void CSharpComponent::drawUserDefinedProperty(Scripting::ComponentProperty& property) {
        ImGui::Text("%s (%s) - User defined type", property.displayName.c_str(), property.typeStr.c_str());
    }

} // Carrot::ECS