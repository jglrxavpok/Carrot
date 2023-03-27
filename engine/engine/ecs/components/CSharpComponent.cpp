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
        //TODO;
        ImGui::Text("TODO!!");
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
        Scripting::CSClass* clazz = GetCSharpScripting().findClass(namespaceName, className);
        if(!clazz) {
            foundInAssemblies = false;
            return;
        }
        foundInAssemblies = true;

        auto pEntity = GetCSharpBindings().entityToCSObject(getEntity());
        void* args[1] { (MonoObject*)(*pEntity) };
        csComponent = clazz->newObject(args);
    }

    void CSharpComponent::onAssemblyLoad() {
        refresh();
    }

    void CSharpComponent::onAssemblyUnload() {
        csComponent = nullptr;
    }

} // Carrot::ECS