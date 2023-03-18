//
// Created by jglrxavpok on 11/03/2023.
//

#include "CSharpBindings.h"
#include <engine/utils/Macros.h>
#include <engine/Engine.h>
#include <core/scripting/csharp/Engine.h>
#include <core/scripting/csharp/CSAppDomain.h>
#include <core/scripting/csharp/CSArray.h>
#include <core/scripting/csharp/CSAssembly.h>
#include <core/scripting/csharp/CSClass.h>
#include <core/scripting/csharp/CSObject.h>
#include <core/scripting/csharp/CSProperty.h>
#include <engine/ecs/Signature.hpp>
#include <mono/metadata/object.h>
#include <engine/ecs/components/CameraComponent.h>
#include <engine/ecs/components/TransformComponent.h>
#include <engine/ecs/systems/System.h>
#include <engine/ecs/systems/CSharpLogicSystem.h>

#include <core/functional/Reflection.hpp>

namespace Carrot::Scripting {

    static CSharpBindings& instance() {
        return GetCSharpBindings();
    }

    /* Automatic way to create void* from member function & instance().
     * But having many static methods is waaaaaaaay easier to understand
     * I am having leaving this code in case I see another use case in the future (automatic binding generation?)
     * Example usage:
     * mono_add_internal_call("Carrot.Utilities::GetMaxComponentCount", wrap<&CSharpBindings::GetMaxComponentCount>());
     * mono_add_internal_call("Carrot.Signature::GetComponentID", wrap<&CSharpBindings::GetComponentID>());
     * mono_add_internal_call("Carrot.System::LoadEntities", wrap<&CSharpBindings::LoadEntities>());
    template<typename, auto f, typename>
    struct TrampolineHelper;

    template<typename ReturnType, auto f, typename... T>
    struct TrampolineHelper<ReturnType, f, std::tuple<T...>> {
            ReturnType (*fnPtr)(T...) = [](T... args) -> ReturnType {
                return std::invoke(f, instance(), std::forward<T>(args)...);
            };
    };

    /// Creates a trampoline function that uses the current CSharpBindings instance
    template<auto f>
    static auto wrap() {
        using Traits = Reflection::FunctionTraits<decltype(f)>;
        static_assert(Traits::IsMemberFunction, "wrap expects a member function!");

        using ReturnType = Traits::RetType;
        using ArgumentTypes = Traits::ArgTypes;

        static TrampolineHelper<ReturnType, f, ArgumentTypes> t{};
        return t.fnPtr;
    }
    */

    CSharpBindings::CSharpBindings() {
        const Carrot::IO::VFS::Path dllPath = "engine://scripting/Carrot.dll";

        auto& engine = GetCSharpScripting();
        baseModule = engine.loadAssembly(dllPath);
        auto* baseClass = baseModule->findClass("Carrot", "Carrot");
        verify(baseClass, Carrot::sprintf("Missing class Carrot.Carrot inside %s", dllPath.toString().c_str()));
        auto* method = baseClass->findMethod("EngineInit");
        verify(method, Carrot::sprintf("Missing method EngineInit inside class Carrot.Carrot inside %s", dllPath.toString().c_str()));
        method->staticInvoke({});





        {
            HardcodedComponentIDs.clear();
            HardcodedComponentIDs["Carrot.TransformComponent"] = ECS::TransformComponent::getID();
            HardcodedComponentIDs["Carrot.CameraComponent"] = ECS::CameraComponent::getID();
        }

        {
            EntityClass = engine.findClass("Carrot", "Entity");
            verify(EntityClass, "Missing Carrot.Entity class in Carrot.dll !");
            EntityIDField = EntityClass->findField("_id");
            verify(EntityIDField, "Missing Carrot.Entity::_id field in Carrot.dll !");
            EntityUserPointerField = EntityClass->findField("_userPointer");
            verify(EntityUserPointerField, "Missing Carrot.Entity::_userPointer field in Carrot.dll !");
        }

        {
            SystemClass = engine.findClass("Carrot", "System");
            verify(SystemClass, "Missing Carrot.System class in Carrot.dll !");
            SystemHandleField = SystemClass->findField("_handle");
            verify(SystemHandleField, "Missing Carrot.System::_handle field in Carrot.dll !");
            SystemSignatureField = SystemClass->findField("_signature");
            verify(SystemSignatureField, "Missing Carrot.System::_signature field in Carrot.dll !");
        }

        {
            ComponentClass = engine.findClass("Carrot", "IComponent");
            verify(ComponentClass, "Missing Carrot.IComponent class in Carrot.dll !");
            ComponentOwnerField = ComponentClass->findField("owner");
            verify(ComponentOwnerField, "Missing Carrot.IComponent::owner field in Carrot.dll !");
        }

        {
            TransformComponentClass = engine.findClass("Carrot", "TransformComponent");
            verify(TransformComponentClass, "Missing Carrot.TransformComponent class in Carrot.dll !");
        }

        auto* typeClass = engine.findClass("System", "Type");
        verify(typeClass, "Something is very wrong!");
        SystemTypeFullNameProperty = typeClass->findProperty("FullName");

        mono_add_internal_call("Carrot.Utilities::GetMaxComponentCount", GetMaxComponentCount);
        mono_add_internal_call("Carrot.Signature::GetComponentID", GetComponentID);
        mono_add_internal_call("Carrot.System::LoadEntities", LoadEntities);
        mono_add_internal_call("Carrot.Entity::GetComponent", GetComponent);
        mono_add_internal_call("Carrot.Entity::GetName", GetName);
        mono_add_internal_call("Carrot.Entity::GetChildren", nullptr); // TODO
        mono_add_internal_call("Carrot.Entity::GetParent", nullptr); // TODO
        mono_add_internal_call("Carrot.TransformComponent::_GetLocalPosition", _GetLocalPosition);
        mono_add_internal_call("Carrot.TransformComponent::_SetLocalPosition", _SetLocalPosition);
    }

    void CSharpBindings::loadGameAssembly(const IO::VFS::Path& gameDLL) {
        if(gameModule) {
            unloadGameAssembly();
        }
        gameModuleLocation = gameDLL;

        verify(!appDomain, "There is already an app domain, the flow is wrong: we should never have an already loaded game assembly at this point");
        appDomain = GetCSharpScripting().makeAppDomain(gameDLL.toString());
        gameModule = GetCSharpScripting().loadAssembly(gameDLL, appDomain->toMono());
        gameModule->dumpTypes();

        loadCallbacks();
        // TODO: load systems, components
    }

    void CSharpBindings::reloadGameAssembly() {
        unloadGameAssembly();
        loadGameAssembly(gameModuleLocation);
    }

    void CSharpBindings::unloadGameAssembly() {
        if(!gameModule) {
            return;
        }

        unloadCallbacks();
        verify(appDomain, "There is no app domain, the flow is wrong: we should have a loaded game assembly at this point!")
        GetCSharpScripting().unloadAssembly(std::move(gameModule)); // clears the assembly from the scripting engine
        appDomain = nullptr; // clears the associated assembly & classes from Mono runtime
    }

    CSharpBindings::Callbacks::Handle CSharpBindings::registerGameAssemblyLoadCallback(const std::function<void()>& callback) {
        return loadCallbacks.append(callback);
    }

    CSharpBindings::Callbacks::Handle CSharpBindings::registerGameAssemblyUnloadCallback(const std::function<void()>& callback) {
        return unloadCallbacks.append(callback);
    }

    void CSharpBindings::unregisterGameAssemblyLoadCallback(const CSharpBindings::Callbacks::Handle& handle) {
        loadCallbacks.remove(handle);
    }

    void CSharpBindings::unregisterGameAssemblyUnloadCallback(const CSharpBindings::Callbacks::Handle& handle) {
        unloadCallbacks.remove(handle);
    }


    //
    // Bindings
    //

    std::int32_t CSharpBindings::GetMaxComponentCount() {
        return Carrot::MAX_COMPONENTS;
    }

    ComponentID CSharpBindings::getComponentIDFromTypeName(const std::string& name) {
        auto it = instance().HardcodedComponentIDs.find(name);
        if(it != instance().HardcodedComponentIDs.end()) {
            return it->second;
        }

        // TODO: handle non-hardcoded
        return -1;
    }

    ComponentID CSharpBindings::GetComponentID(MonoString* str) {
        char* chars = mono_string_to_utf8(str);

        CLEANUP(mono_free(chars));
        return getComponentIDFromTypeName(chars);
    }

    MonoArray* CSharpBindings::LoadEntities(MonoObject* systemObj) {
        Scripting::CSObject handleObj = instance().SystemHandleField->get(Scripting::CSObject(systemObj));
        std::uint64_t handle = *((std::uint64_t*)mono_object_unbox(handleObj));
        auto* systemPtr = reinterpret_cast<ECS::CSharpLogicSystem*>(handle);

        return systemPtr->getEntityList()->toMono();
    }

    ECS::Entity CSharpBindings::convertToEntity(MonoObject* entityMonoObj) {
        auto entityObj = Scripting::CSObject(entityMonoObj);
        ECS::EntityID entityID = *((ECS::EntityID*)mono_object_unbox(instance().EntityIDField->get(entityObj)));
        ECS::World* pWorld = *((ECS::World**)mono_object_unbox(instance().EntityUserPointerField->get(entityObj)));

        return pWorld->wrap(entityID);
    }

    std::shared_ptr<Scripting::CSObject> CSharpBindings::entityToCSObject(ECS::Entity& e) {
        ECS::EntityID uuid = e.getID();
        ECS::World* worldPtr = &e.getWorld();
        void* args[2] = {
                &uuid,
                &worldPtr,
        };
        return instance().EntityClass->newObject(args);
    }

    MonoObject* CSharpBindings::GetComponent(MonoObject* entityMonoObj, MonoString* type) {
        ECS::Entity entity = convertToEntity(entityMonoObj);
        ComponentID componentId = GetComponentID(type);

        // TODO: avoid allocations

        if(componentId == ECS::TransformComponent::getID()) {
            void* args[1] = {
                    entityMonoObj
            };
            auto obj = instance().TransformComponentClass->newObject(args);
            return (MonoObject*)(*obj); // assumes the GC won't trigger before it is used
        } else {
            TODO;
        }
    }

    glm::vec3 CSharpBindings::_GetLocalPosition(MonoObject* transformComp) {
        auto ownerEntity = instance().ComponentOwnerField->get(Scripting::CSObject(transformComp));
        ECS::Entity entity = convertToEntity(ownerEntity);
        return entity.getComponent<ECS::TransformComponent>()->localTransform.position;
    }

    void CSharpBindings::_SetLocalPosition(MonoObject* transformComp, glm::vec3 value) {
        auto ownerEntity = instance().ComponentOwnerField->get(Scripting::CSObject(transformComp));
        ECS::Entity entity = convertToEntity(ownerEntity);
        entity.getComponent<ECS::TransformComponent>()->localTransform.position = value;
    }

    MonoString* CSharpBindings::GetName(MonoObject* entityMonoObj) {
        auto entity = convertToEntity(entityMonoObj);
        return mono_string_new_wrapper(entity.getName().c_str());
    }
}