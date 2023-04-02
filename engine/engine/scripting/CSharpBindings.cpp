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
#include <engine/ecs/components/CSharpComponent.h>
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
     * I am leaving this code in case I see another use case in the future (automatic binding generation?)
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
        engineDllPath = "engine://scripting/Carrot.dll";
        enginePdbPath = "engine://scripting/Carrot.pdb";

        loadEngineAssembly();

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

    CSharpBindings::~CSharpBindings() {
        unloadGameAssembly();
    }

    void CSharpBindings::loadGameAssembly(const IO::VFS::Path& gameDLL) {
        if(gameModule) {
            unloadGameAssembly();
        } else if(baseModule) {
            unloadOnlyEngineAssembly();
        }

        gameModuleLocation = gameDLL;
        loadEngineAssembly();

        std::optional<Carrot::IO::Resource> gamePDB;
        const IO::VFS::Path gamePDBLocation = gameDLL.withExtension(".pdb");
        if(GetVFS().exists(gamePDBLocation)) {
            gamePDB = gamePDBLocation;
        }
        gameModule = GetCSharpScripting().loadAssembly(gameDLL, appDomain->toMono(), gamePDB);
        gameModule->dumpTypes();

        auto allSystems = gameModule->findSubclasses(*SystemClass);
        auto& systemLibrary = ECS::getSystemLibrary();
        for(auto* systemClass : allSystems) {
            std::string id = systemClass->getNamespaceName();
            id += '.';
            id += systemClass->getName();
            id += " (C#)";

            systemLibrary.add(
                    id,
                    [className = systemClass->getName(), namespaceName = systemClass->getNamespaceName()](const rapidjson::Value& json, ECS::World& world) {
                        return std::make_unique<ECS::CSharpLogicSystem>(json, world, namespaceName, className);
                    },
                    [className = systemClass->getName(), namespaceName = systemClass->getNamespaceName()](Carrot::ECS::World& world) {
                        return std::make_unique<ECS::CSharpLogicSystem>(world, namespaceName, className);
                    }
            );

            systemIDs.emplace_back(std::move(id));
        }

        auto allComponents = gameModule->findSubclasses(*ComponentClass);
        auto& componentLibrary = ECS::getComponentLibrary();
        for(auto* componentClass : allComponents) {
            std::string fullType = componentClass->getNamespaceName();
            fullType += '.';
            fullType += componentClass->getName();
            std::string id = fullType + " (C#)";

            componentLibrary.add(
                    id,
                    [className = componentClass->getName(), namespaceName = componentClass->getNamespaceName()](const rapidjson::Value& json, ECS::Entity entity) {
                        return std::make_unique<ECS::CSharpComponent>(json, entity, namespaceName, className);
                    },
                    [className = componentClass->getName(), namespaceName = componentClass->getNamespaceName()](Carrot::ECS::Entity entity) {
                        return std::make_unique<ECS::CSharpComponent>(entity, namespaceName, className);
                    }
            );

            csharpComponentIDs.getOrCompute(fullType, []() {
                return Carrot::requestComponentID();
            });

            componentIDs.emplace_back(std::move(id));
        }

        loadCallbacks();
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

        for(const auto& id : componentIDs) {
            ECS::getComponentLibrary().remove(id);
        }

        for(const auto& id : systemIDs) {
            ECS::getSystemLibrary().remove(id);
        }

        verify(appDomain, "There is no app domain, the flow is wrong: we should have a loaded game assembly at this point!")

        // clears the assemblies from the scripting engine
        GetCSharpScripting().unloadAssembly(std::move(gameModule));
        GetCSharpScripting().unloadAssembly(std::move(baseModule));

        appDomain = nullptr; // clears the associated assembly & classes from Mono runtime
    }

    const Carrot::IO::VFS::Path& CSharpBindings::getEngineDllPath() const {
        return engineDllPath;
    }

    const Carrot::IO::VFS::Path& CSharpBindings::getEnginePdbPath() const {
        return enginePdbPath;
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

    std::vector<ComponentProperty> CSharpBindings::findAllComponentProperties(const std::string& namespaceName, const std::string& className) {
        return reflectionHelper.findAllComponentProperties(namespaceName, className);
    }

    MonoDomain* CSharpBindings::getAppDomain() {
        return appDomain->toMono();
    }

    ComponentID CSharpBindings::requestComponentID(const std::string& namespaceName, const std::string& className) {
        const std::string fullType = namespaceName + '.' + className;
        return csharpComponentIDs.getOrCompute(fullType, [&]() {
            verify(false, Carrot::sprintf("Should not happen: csharpComponentIDs does not have type %s", fullType.c_str()));
            return -1;
        });
    }

    void CSharpBindings::loadEngineAssembly() {
        verify(!appDomain, "There is already an app domain, the flow is wrong: we should never have an already loaded game assembly at this point");
        appDomain = GetCSharpScripting().makeAppDomain(gameModuleLocation.toString());

        auto& engine = GetCSharpScripting();
        baseModule = engine.loadAssembly(getEngineDllPath(), nullptr, getEnginePdbPath());
        auto* baseClass = baseModule->findClass("Carrot", "Carrot");
        verify(baseClass, Carrot::sprintf("Missing class Carrot.Carrot inside %s", getEngineDllPath().toString().c_str()));
        auto* method = baseClass->findMethod("EngineInit");
        verify(method, Carrot::sprintf("Missing method EngineInit inside class Carrot.Carrot inside %s", getEngineDllPath().toString().c_str()));
        method->staticInvoke({});

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

        reflectionHelper.reload();

        // needs to be done last: references to classes loaded above
        {
            HardcodedComponents.clear();
            HardcodedComponents["Carrot.TransformComponent"] = {
                    .id = ECS::TransformComponent::getID(),
                    .clazz = TransformComponentClass,
            };
        }
    }

    void CSharpBindings::unloadOnlyEngineAssembly() {
        verify(!gameModule, "Wrong flow: use unloadGameAssembly if there a game");

        unloadCallbacks();
        verify(appDomain, "There is no app domain, the flow is wrong: we should have a loaded engine assembly at this point!")

        // clears the assemblies from the scripting engine
        GetCSharpScripting().unloadAssembly(std::move(baseModule));

        appDomain = nullptr; // clears the associated assembly & classes from Mono runtime
    }

    //
    // Bindings
    //

    std::int32_t CSharpBindings::GetMaxComponentCount() {
        return Carrot::MAX_COMPONENTS;
    }

    ComponentID CSharpBindings::GetComponentID(MonoString* namespaceStr, MonoString* classStr) {
        char* namespaceChars = mono_string_to_utf8(namespaceStr);
        char* classChars = mono_string_to_utf8(classStr);

        CLEANUP(mono_free(namespaceChars));
        CLEANUP(mono_free(classChars));
        return instance().getComponentFromType(namespaceChars, classChars).id;
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

    CSharpBindings::CppComponent CSharpBindings::getComponentFromType(const std::string& namespaceName, const std::string& className) {
        std::string fullType;
        fullType.reserve(namespaceName.size() + className.size() + 1);
        fullType += namespaceName;
        fullType += '.';
        fullType += className;
        auto it = HardcodedComponents.find(fullType);
        if(it != HardcodedComponents.end()) {
            return it->second;
        }

        return CppComponent {
            .isCSharp = true,
            .id = csharpComponentIDs.getOrCompute(fullType, [&]() {
                verify(false, Carrot::sprintf("Should not happen: csharpComponentIDs does not have type %s", fullType.c_str()));
                return -1;
            }),
            .clazz = GetCSharpScripting().findClass(namespaceName, className),
        };
    }

    MonoObject* CSharpBindings::GetComponent(MonoObject* entityMonoObj, MonoString* namespaceStr, MonoString* classStr) {
        char* namespaceChars = mono_string_to_utf8(namespaceStr);
        char* classChars = mono_string_to_utf8(classStr);

        CLEANUP(mono_free(namespaceChars));
        CLEANUP(mono_free(classChars));

        auto component = instance().getComponentFromType(namespaceChars, classChars);
        void* args[1] = {
                entityMonoObj
        };

        if(component.isCSharp) {
            auto compRef = convertToEntity(entityMonoObj).getComponent(component.id);
            if(compRef.hasValue()) {
                auto csharpComp = dynamic_cast<ECS::CSharpComponent*>(compRef.asPtr());
                verify(csharpComp, "component isCSharp is true but not a CSharpComponent??");
                return csharpComp->getCSComponentObject();
            } else {
                return nullptr;
            }
        }

        // TODO: avoid allocations
        auto obj = component.clazz->newObject(args);
        return (MonoObject*)(*obj); // assumes the GC won't trigger before it is used
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