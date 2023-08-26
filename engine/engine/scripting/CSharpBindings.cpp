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
#include <engine/ecs/components/NavMeshComponent.h>
#include <engine/ecs/components/PhysicsCharacterComponent.h>
#include <engine/ecs/components/RigidBodyComponent.h>
#include <engine/ecs/components/TextComponent.h>
#include <engine/ecs/components/TransformComponent.h>
#include <engine/ecs/systems/System.h>
#include <engine/ecs/systems/CSharpLogicSystem.h>

#include <core/functional/Reflection.hpp>

#include <engine/io/actions/Action.hpp>
#include <engine/io/actions/ActionSet.h>

#include <engine/physics/PhysicsSystem.h>

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
        mono_add_internal_call("Carrot.System::_Query", _QueryECS);
        mono_add_internal_call("Carrot.System::FindEntityByName", FindEntityByName);
        mono_add_internal_call("Carrot.Entity::GetComponent", GetComponent);
        mono_add_internal_call("Carrot.Entity::GetName", GetName);
        mono_add_internal_call("Carrot.Entity::GetChildren", nullptr); // TODO
        mono_add_internal_call("Carrot.Entity::GetParent", GetParent);
        mono_add_internal_call("Carrot.Entity::SetParent", SetParent);
        mono_add_internal_call("Carrot.Entity::ReParent", ReParent);

        mono_add_internal_call("Carrot.TransformComponent::_GetLocalPosition", _GetLocalPosition);
        mono_add_internal_call("Carrot.TransformComponent::_SetLocalPosition", _SetLocalPosition);
        mono_add_internal_call("Carrot.TransformComponent::_GetLocalScale", _GetLocalScale);
        mono_add_internal_call("Carrot.TransformComponent::_SetLocalScale", _SetLocalScale);
        mono_add_internal_call("Carrot.TransformComponent::_GetEulerAngles", _GetEulerAngles);
        mono_add_internal_call("Carrot.TransformComponent::_SetEulerAngles", _SetEulerAngles);
        mono_add_internal_call("Carrot.TransformComponent::_GetWorldPosition", _GetWorldPosition);

        mono_add_internal_call("Carrot.CharacterComponent::_GetVelocity", _GetCharacterVelocity);
        mono_add_internal_call("Carrot.CharacterComponent::_SetVelocity", _SetCharacterVelocity);
        mono_add_internal_call("Carrot.CharacterComponent::IsOnGround", _IsCharacterOnGround);
        mono_add_internal_call("Carrot.CharacterComponent::EnablePhysics", EnableCharacterPhysics);
        mono_add_internal_call("Carrot.CharacterComponent::DisablePhysics", DisableCharacterPhysics);

        mono_add_internal_call("Carrot.TextComponent::_GetText", _GetText);
        mono_add_internal_call("Carrot.TextComponent::_SetText", _SetText);

        mono_add_internal_call("Carrot.RigidBodyComponent::GetColliderCount", GetRigidBodyColliderCount);
        mono_add_internal_call("Carrot.RigidBodyComponent::GetCollider", GetRigidBodyCollider);
        mono_add_internal_call("Carrot.RigidBodyComponent::_GetVelocity", _GetRigidBodyVelocity);
        mono_add_internal_call("Carrot.RigidBodyComponent::_SetVelocity", _SetRigidBodyVelocity);
        mono_add_internal_call("Carrot.RigidBodyComponent::_DoRaycast", _DoRaycast);

        mono_add_internal_call("Carrot.NavMeshComponent::GetClosestPointInMesh", GetClosestPointInMesh);
        mono_add_internal_call("Carrot.NavMeshComponent::PathFind", PathFind);

        {
            mono_add_internal_call("Carrot.Physics.Collider::Raycast", RaycastCollider);
        }

        mono_add_internal_call("Carrot.Input.ActionSet::Create", CreateActionSet);
        mono_add_internal_call("Carrot.Input.ActionSet::_ActivateActionSet", _ActivateActionSet);
        mono_add_internal_call("Carrot.Input.ActionSet::_AddToActionSet", _AddToActionSet);

        mono_add_internal_call("Carrot.Input.Action::SuggestBinding", SuggestBinding);

        mono_add_internal_call("Carrot.Input.BoolInputAction::Create", CreateBoolInputAction);
        mono_add_internal_call("Carrot.Input.BoolInputAction::IsPressed", IsBoolInputPressed);
        mono_add_internal_call("Carrot.Input.BoolInputAction::WasJustPressed", WasBoolInputJustPressed);
        mono_add_internal_call("Carrot.Input.BoolInputAction::WasJustReleased", WasBoolInputJustReleased);

        mono_add_internal_call("Carrot.Input.FloatInputAction::Create", CreateFloatInputAction);
        mono_add_internal_call("Carrot.Input.FloatInputAction::GetValue", GetFloatInputValue);

        mono_add_internal_call("Carrot.Input.Vec2InputAction::Create", CreateVec2InputAction);
        mono_add_internal_call("Carrot.Input.Vec2InputAction::_GetValue", _GetVec2InputValue);
    }

    CSharpBindings::~CSharpBindings() {
        unloadGameAssembly();
    }

    void CSharpBindings::tick(double deltaTime) {
        // clear C++ objects that are no longer referenced by C#
        Carrot::removeIf(carrotObjects, [&](auto& pObj) {
            return !pObj->isAlive();
        });
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

    std::vector<ComponentID> CSharpBindings::getAllComponentIDs() const {
        std::vector<ComponentID> components;
        auto snapshot = csharpComponentIDs.snapshot();
        components.reserve(snapshot.size());
        for(auto& [fullType, pComponentID] : snapshot) {
            components.emplace_back(*pComponentID);
        }
        return components;
    }

    Carrot::Async::ParallelMap<std::string, ComponentID>::ConstSnapshot CSharpBindings::getAllComponents() const {
        return csharpComponentIDs.snapshot();
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

        LOAD_CLASS(Vec2);
        LOAD_CLASS(Vec3);
        {
            LOAD_CLASS(Entity);
            EntityIDField = EntityClass->findField("_id");
            verify(EntityIDField, "Missing Carrot.Entity::_id field in Carrot.dll !");
            EntityUserPointerField = EntityClass->findField("_userPointer");
            verify(EntityUserPointerField, "Missing Carrot.Entity::_userPointer field in Carrot.dll !");
        }

        {
            CarrotObjectClass = engine.findClass("Carrot", "Object");
            verify(CarrotObjectClass, "Missing Carrot.Object class in Carrot.dll !");
            CarrotObjectHandleField = CarrotObjectClass->findField("_handle");
            verify(CarrotObjectHandleField, "Missing Carrot.Object::_handle field in Carrot.dll !");
        }

        {
            CarrotReferenceClass = engine.findClass("Carrot", "Reference");
            verify(CarrotReferenceClass, "Missing Carrot.Reference class in Carrot.dll !");
            CarrotReferenceHandleField = CarrotReferenceClass->findField("_handle");
            verify(CarrotReferenceHandleField, "Missing Carrot.Reference::_handle field in Carrot.dll !");
        }

        {
            LOAD_CLASS(System);
            SystemSignatureField = SystemClass->findField("_signature");
            verify(SystemSignatureField, "Missing Carrot.System::_signature field in Carrot.dll !");
        }

        {
            LOAD_CLASS_NS("Carrot.Input", ActionSet);
            LOAD_CLASS_NS("Carrot.Input", BoolInputAction);
            LOAD_CLASS_NS("Carrot.Input", FloatInputAction);
            LOAD_CLASS_NS("Carrot.Input", Vec2InputAction);
        }

        {
            ComponentClass = engine.findClass("Carrot", "IComponent");
            verify(ComponentClass, "Missing Carrot.IComponent class in Carrot.dll !");
            ComponentOwnerField = ComponentClass->findField("owner");
            verify(ComponentOwnerField, "Missing Carrot.IComponent::owner field in Carrot.dll !");
        }

        LOAD_CLASS(TransformComponent);
        LOAD_CLASS(TextComponent);
        LOAD_CLASS(RigidBodyComponent);
        LOAD_CLASS(CharacterComponent);
        LOAD_CLASS(NavMeshComponent);

        {
            LOAD_CLASS(NavPath);
            LOAD_FIELD(NavPath, Waypoints);
        }

        {
            LOAD_CLASS_NS("Carrot.Physics", Collider);

            LOAD_CLASS_NS("Carrot.Physics", RaycastInfo);
            LOAD_FIELD(RaycastInfo, WorldPoint);
            LOAD_FIELD(RaycastInfo, WorldNormal);
            LOAD_FIELD(RaycastInfo, T);
            LOAD_FIELD(RaycastInfo, Collider);
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
            HardcodedComponents["Carrot.TextComponent"] = {
                    .id = ECS::TextComponent::getID(),
                    .clazz = TextComponentClass,
            };
            HardcodedComponents["Carrot.RigidBodyComponent"] = {
                    .id = ECS::RigidBodyComponent::getID(),
                    .clazz = RigidBodyComponentClass,
            };
            HardcodedComponents["Carrot.CharacterComponent"] = {
                    .id = ECS::PhysicsCharacterComponent::getID(),
                    .clazz = CharacterComponentClass,
            };
            HardcodedComponents["Carrot.NavMeshComponent"] = {
                    .id = ECS::NavMeshComponent::getID(),
                    .clazz = NavMeshComponentClass,
            };
        }
    }

    template<typename T>
    T& getObject(MonoObject* obj) {
        verify(mono_object_isinst(obj, instance().CSharpBindings::CarrotObjectClass->toMono()), "Input object is not a Carrot.Object instance!");

        Scripting::CSObject handleObj = instance().CarrotObjectHandleField->get(Scripting::CSObject(obj));
        std::uint64_t handle = *((std::uint64_t*)mono_object_unbox(handleObj));
        auto* ptr = reinterpret_cast<T*>(handle);
        return *ptr;
    }

    template<typename T>
    T& getReference(MonoObject* obj) {
        verify(mono_object_isinst(obj, instance().CSharpBindings::CarrotReferenceClass->toMono()), "Input object is not a Carrot.Reference instance!");

        Scripting::CSObject handleObj = instance().CarrotReferenceHandleField->get(Scripting::CSObject(obj));
        std::uint64_t handle = *((std::uint64_t*)mono_object_unbox(handleObj));
        auto* ptr = reinterpret_cast<T*>(handle);
        return *ptr;
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
        Scripting::CSObject handleObj = instance().CarrotObjectHandleField->get(Scripting::CSObject(systemObj));
        std::uint64_t handle = *((std::uint64_t*)mono_object_unbox(handleObj));
        auto* systemPtr = reinterpret_cast<ECS::CSharpLogicSystem*>(handle);

        return systemPtr->getEntityList()->toMono();
    }

    MonoArray* CSharpBindings::_QueryECS(MonoObject* systemObj, MonoArray* componentClasses) {
        Scripting::CSObject handleObj = instance().CarrotObjectHandleField->get(Scripting::CSObject(systemObj));
        std::uint64_t handle = *((std::uint64_t*)mono_object_unbox(handleObj));
        auto* systemPtr = reinterpret_cast<ECS::CSharpLogicSystem*>(handle);

        std::unordered_set<Carrot::ComponentID> components;
        std::size_t componentCount = mono_array_length(componentClasses);

        for(std::size_t i = 0; i < componentCount/2; ++i) {
            auto* namespaceString = mono_array_get(componentClasses, MonoString*, i * 2);
            auto* classNameString = mono_array_get(componentClasses, MonoString*, i*2 + 1);

            ComponentID componentID = GetComponentID(namespaceString, classNameString);
            components.insert(componentID);
        }

        components.reserve(componentCount/2);
        auto queryResult = systemPtr->getWorld().queryEntities(components);

        auto csQueryResult = instance().EntityClass->newArray(queryResult.size());
        auto* csQueryResultArray = csQueryResult->toMono();

        std::vector<std::shared_ptr<CSObject>> csEntities;
        csEntities.resize(queryResult.size());

        for(std::size_t i = 0; i < queryResult.size(); i++) {
            csEntities[i] = entityToCSObject(queryResult[i]);
            mono_array_set(csQueryResultArray, MonoObject*, i, csEntities[i]->toMono());
        }

        return csQueryResult->toMono();
    }

    ECS::Entity CSharpBindings::convertToEntity(MonoObject* entityMonoObj) {
        if(entityMonoObj == nullptr) {
            return ECS::Entity{};
        }
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

    glm::vec3 CSharpBindings::_GetLocalScale(MonoObject* transformComp) {
        auto ownerEntity = instance().ComponentOwnerField->get(Scripting::CSObject(transformComp));
        ECS::Entity entity = convertToEntity(ownerEntity);
        return entity.getComponent<ECS::TransformComponent>()->localTransform.scale;
    }

    void CSharpBindings::_SetLocalScale(MonoObject* transformComp, glm::vec3 value) {
        auto ownerEntity = instance().ComponentOwnerField->get(Scripting::CSObject(transformComp));
        ECS::Entity entity = convertToEntity(ownerEntity);
        entity.getComponent<ECS::TransformComponent>()->localTransform.scale = value;
    }

    glm::vec3 CSharpBindings::_GetEulerAngles(MonoObject* transformComp) {
        auto ownerEntity = instance().ComponentOwnerField->get(Scripting::CSObject(transformComp));
        ECS::Entity entity = convertToEntity(ownerEntity);
        return glm::eulerAngles(entity.getComponent<ECS::TransformComponent>()->localTransform.rotation);
    }

    void CSharpBindings::_SetEulerAngles(MonoObject* transformComp, glm::vec3 value) {
        auto ownerEntity = instance().ComponentOwnerField->get(Scripting::CSObject(transformComp));
        ECS::Entity entity = convertToEntity(ownerEntity);
        entity.getComponent<ECS::TransformComponent>()->localTransform.rotation = glm::quat(value);
    }

    glm::vec3 CSharpBindings::_GetWorldPosition(MonoObject* transformComp) {
        auto ownerEntity = instance().ComponentOwnerField->get(Scripting::CSObject(transformComp));
        ECS::Entity entity = convertToEntity(ownerEntity);
        return entity.getComponent<ECS::TransformComponent>()->computeFinalPosition();
    }

    glm::vec3 CSharpBindings::_GetCharacterVelocity(MonoObject* characterComp) {
        auto ownerEntity = instance().ComponentOwnerField->get(Scripting::CSObject(characterComp));
        ECS::Entity entity = convertToEntity(ownerEntity);
        return entity.getComponent<ECS::PhysicsCharacterComponent>()->character.getVelocity();
    }

    void CSharpBindings::_SetCharacterVelocity(MonoObject* characterComp, glm::vec3 value) {
        auto ownerEntity = instance().ComponentOwnerField->get(Scripting::CSObject(characterComp));
        ECS::Entity entity = convertToEntity(ownerEntity);
        entity.getComponent<ECS::PhysicsCharacterComponent>()->character.setVelocity(value);
    }

    bool CSharpBindings::_IsCharacterOnGround(MonoObject* characterComp) {
        auto ownerEntity = instance().ComponentOwnerField->get(Scripting::CSObject(characterComp));
        ECS::Entity entity = convertToEntity(ownerEntity);
        return entity.getComponent<ECS::PhysicsCharacterComponent>()->character.isOnGround();
    }

    void CSharpBindings::EnableCharacterPhysics(MonoObject* characterComp) {
        auto ownerEntity = instance().ComponentOwnerField->get(Scripting::CSObject(characterComp));
        ECS::Entity entity = convertToEntity(ownerEntity);
        return entity.getComponent<ECS::PhysicsCharacterComponent>()->character.addToWorld();
    }

    void CSharpBindings::DisableCharacterPhysics(MonoObject* characterComp) {
        auto ownerEntity = instance().ComponentOwnerField->get(Scripting::CSObject(characterComp));
        ECS::Entity entity = convertToEntity(ownerEntity);
        return entity.getComponent<ECS::PhysicsCharacterComponent>()->character.removeFromWorld();
    }


    MonoString* CSharpBindings::_GetText(MonoObject* textComp) {
        auto ownerEntity = instance().ComponentOwnerField->get(Scripting::CSObject(textComp));
        ECS::Entity entity = convertToEntity(ownerEntity);
        return mono_string_new_wrapper(entity.getComponent<ECS::TextComponent>()->getText().data());
    }

    void CSharpBindings::_SetText(MonoObject* textComp, MonoString* value) {
        auto ownerEntity = instance().ComponentOwnerField->get(Scripting::CSObject(textComp));
        ECS::Entity entity = convertToEntity(ownerEntity);

        char* valueStr = mono_string_to_utf8(value);
        CLEANUP(mono_free(valueStr));
        entity.getComponent<ECS::TextComponent>()->setText(valueStr);
    }

    glm::vec3 CSharpBindings::_GetRigidBodyVelocity(MonoObject* comp) {
        auto ownerEntity = instance().ComponentOwnerField->get(Scripting::CSObject(comp));
        ECS::Entity entity = convertToEntity(ownerEntity);
        return entity.getComponent<ECS::RigidBodyComponent>()->rigidbody.getVelocity();
    }

    void CSharpBindings::_SetRigidBodyVelocity(MonoObject* comp, glm::vec3 value) {
        auto ownerEntity = instance().ComponentOwnerField->get(Scripting::CSObject(comp));
        ECS::Entity entity = convertToEntity(ownerEntity);
        entity.getComponent<ECS::RigidBodyComponent>()->rigidbody.setVelocity(value);
    }

    std::uint64_t CSharpBindings::GetRigidBodyColliderCount(MonoObject* comp) {
        auto ownerEntity = instance().ComponentOwnerField->get(Scripting::CSObject(comp));
        ECS::Entity entity = convertToEntity(ownerEntity);
        return entity.getComponent<ECS::RigidBodyComponent>()->rigidbody.getColliderCount();
    }

    MonoObject* CSharpBindings::GetRigidBodyCollider(MonoObject* comp, std::uint64_t index) {
        auto ownerEntity = instance().ComponentOwnerField->get(Scripting::CSObject(comp));
        ECS::Entity entity = convertToEntity(ownerEntity);
        auto& rigidbody = entity.getComponent<ECS::RigidBodyComponent>()->rigidbody;
        return instance().requestCarrotReference(instance().ColliderClass, &rigidbody.getCollider(index))->toMono();
    }

    bool CSharpBindings::RaycastCollider(MonoObject* collider, glm::vec3 start, glm::vec3 direction, float maxLength, MonoObject* pCSRaycastInfo) {
        auto& colliderRef = getReference<Carrot::Physics::Collider>(collider);

        Carrot::Physics::RaycastInfo raycastInfo;
        bool hasHit = colliderRef.getShape().raycast(start, direction, maxLength, raycastInfo);

        Scripting::CSObject csRaycastInfo{ pCSRaycastInfo };
        if(hasHit) {
            instance().RaycastInfoWorldPointField->set(csRaycastInfo, Scripting::CSObject((MonoObject*)&raycastInfo.worldPoint));
            instance().RaycastInfoWorldNormalField->set(csRaycastInfo, Scripting::CSObject((MonoObject*)&raycastInfo.worldNormal));
            instance().RaycastInfoTField->set(csRaycastInfo, Scripting::CSObject((MonoObject*)&raycastInfo.t));
            instance().RaycastInfoColliderField->set(csRaycastInfo, *instance().requestCarrotReference(instance().ColliderClass, raycastInfo.collider));
        }

        return hasHit;
    }

    bool CSharpBindings::_DoRaycast(MonoObject* rigidbodyComp, glm::vec3 start, glm::vec3 direction, float maxLength, MonoObject* pCSRaycastInfo, std::uint16_t collisionMask) {
        if(true) // TODO
            return false;
        auto ownerEntity = instance().ComponentOwnerField->get(Scripting::CSObject(rigidbodyComp));
        ECS::Entity entity = convertToEntity(ownerEntity);
        auto& rigidbody = entity.getComponent<ECS::RigidBodyComponent>()->rigidbody;
        DISCARD(rigidbody); // unused

        Carrot::Physics::RaycastInfo raycastInfo;
        bool hasHit = GetPhysics().raycast(start, direction, maxLength, raycastInfo, collisionMask);

        Scripting::CSObject csRaycastInfo{ pCSRaycastInfo };
        if(hasHit) {
            instance().RaycastInfoWorldPointField->set(csRaycastInfo, Scripting::CSObject((MonoObject*)&raycastInfo.worldPoint));
            instance().RaycastInfoWorldNormalField->set(csRaycastInfo, Scripting::CSObject((MonoObject*)&raycastInfo.worldNormal));
            instance().RaycastInfoTField->set(csRaycastInfo, Scripting::CSObject((MonoObject*)&raycastInfo.t));
            instance().RaycastInfoColliderField->set(csRaycastInfo, *instance().requestCarrotReference(instance().ColliderClass, raycastInfo.collider));
        }

        return hasHit;
    }

    glm::vec3 CSharpBindings::GetClosestPointInMesh(MonoObject* navMeshComponent, glm::vec3 p) {
        auto ownerEntity = instance().ComponentOwnerField->get(Scripting::CSObject(navMeshComponent));
        ECS::Entity entity = convertToEntity(ownerEntity);
        auto& navMesh = entity.getComponent<ECS::NavMeshComponent>()->navMesh;

        return navMesh.getClosestPointInMesh(p);
    }

    MonoObject* CSharpBindings::PathFind(MonoObject* navMeshComponent, glm::vec3 a, glm::vec3 b) {
        auto ownerEntity = instance().ComponentOwnerField->get(Scripting::CSObject(navMeshComponent));
        ECS::Entity entity = convertToEntity(ownerEntity);
        auto& navMesh = entity.getComponent<ECS::NavMeshComponent>()->navMesh;

        AI::NavPath navPath = navMesh.computePath(a, b);
        auto navPathCSObj = instance().NavPathClass->newObject({});
        auto waypointsArray = instance().Vec3Class->newArray(navPath.waypoints.size());
        for (std::size_t i = 0; i < navPath.waypoints.size(); ++i) {
            void* xyz[3] = {
                    (void*)&navPath.waypoints[i].x,
                    (void*)&navPath.waypoints[i].y,
                    (void*)&navPath.waypoints[i].z
            };
            //waypointsArray->set(i, *instance().Vec3Class->newObject(xyz));
            // mono_array_set(csQueryResultArray, MonoObject*, i, csEntities[i]->toMono());
            mono_array_set(waypointsArray->toMono(), glm::vec3, i, navPath.waypoints[i]);
        }

        instance().NavPathWaypointsField->set(*navPathCSObj, Scripting::CSObject((MonoObject*)waypointsArray->toMono()));
        return navPathCSObj->toMono();
    }

    MonoString* CSharpBindings::GetName(MonoObject* entityMonoObj) {
        auto entity = convertToEntity(entityMonoObj);
        return mono_string_new_wrapper(entity.getName().c_str());
    }

    MonoObject* CSharpBindings::GetParent(MonoObject* entityMonoObj) {
        auto entity = convertToEntity(entityMonoObj);
        auto optParent = entity.getParent();
        if(optParent.has_value()) {
            return entityToCSObject(optParent.value())->toMono();
        }
        return nullptr;
    }

    void CSharpBindings::SetParent(MonoObject* entityMonoObj, MonoObject* newParentMonoObj) {
        auto entity = convertToEntity(entityMonoObj);

        if(newParentMonoObj) {
            entity.setParent(convertToEntity(newParentMonoObj));
        } else {
            entity.setParent(std::optional<ECS::Entity>{});
        }
    }

    void CSharpBindings::ReParent(MonoObject* entityMonoObj, MonoObject* newParentMonoObj) {
        auto entity = convertToEntity(entityMonoObj);

        if(newParentMonoObj) {
            entity.reparent(convertToEntity(newParentMonoObj));
        } else {
            entity.reparent(std::optional<ECS::Entity>{});
        }
    }

    MonoObject* CSharpBindings::FindEntityByName(MonoObject* systemObj, MonoString* entityName) {
        char* entityNameStr = mono_string_to_utf8(entityName);
        CLEANUP(mono_free(entityNameStr));

        Scripting::CSObject handleObj = instance().CarrotObjectHandleField->get(Scripting::CSObject(systemObj));
        std::uint64_t handle = *((std::uint64_t*)mono_object_unbox(handleObj));
        auto* systemPtr = reinterpret_cast<ECS::CSharpLogicSystem*>(handle);

        auto potentialEntity = systemPtr->getWorld().findEntityByName(entityNameStr);
        if(potentialEntity.has_value()) {
            return (MonoObject*) (*entityToCSObject(potentialEntity.value()));
        }
        return nullptr;
    }

    MonoObject* CSharpBindings::CreateActionSet(MonoString* nameObj) {
        char* nameStr = mono_string_to_utf8(nameObj);
        CLEANUP(mono_free(nameStr));

        return instance().requestCarrotObject<Carrot::IO::ActionSet>(instance().ActionSetClass, nameStr).toMono();
    }

    void CSharpBindings::SuggestBinding(MonoObject* actionObj, MonoString* bindingObj) {
        char* bindingStr = mono_string_to_utf8(bindingObj);
        CLEANUP(mono_free(bindingStr));
        if(mono_object_isinst(actionObj, instance().BoolInputActionClass->toMono())) {
            auto& action = getObject<Carrot::IO::BoolInputAction>(actionObj);
            action.suggestBinding(bindingStr);
        } else if(mono_object_isinst(actionObj, instance().FloatInputActionClass->toMono())) {
            auto& action = getObject<Carrot::IO::FloatInputAction>(actionObj);
            action.suggestBinding(bindingStr);
        } else if(mono_object_isinst(actionObj, instance().Vec2InputActionClass->toMono())) {
            auto& action = getObject<Carrot::IO::Vec2InputAction>(actionObj);
            action.suggestBinding(bindingStr);
        } else {
            MonoClass* objClass = mono_object_get_class(actionObj);
            CSClass c{ instance().appDomain->toMono(), objClass };
            verify(false, Carrot::sprintf("Unhandled type: %s.%s", c.getNamespaceName().c_str(), c.getName().c_str())); // TODO
        }
    }

    MonoObject* CSharpBindings::CreateBoolInputAction(MonoString* nameObj) {
        char* nameStr = mono_string_to_utf8(nameObj);
        CLEANUP(mono_free(nameStr));

        return instance().requestCarrotObject<Carrot::IO::BoolInputAction>(instance().BoolInputActionClass, nameStr).toMono();
    }

    bool CSharpBindings::IsBoolInputPressed(MonoObject* boolInput) {
        auto& action = getObject<Carrot::IO::BoolInputAction>(boolInput);
        return action.isPressed();
    }

    bool CSharpBindings::WasBoolInputJustPressed(MonoObject* boolInput) {
        auto& action = getObject<Carrot::IO::BoolInputAction>(boolInput);
        return action.wasJustPressed();
    }

    bool CSharpBindings::WasBoolInputJustReleased(MonoObject* boolInput) {
        auto& action = getObject<Carrot::IO::BoolInputAction>(boolInput);
        return action.wasJustReleased();
    }

    MonoObject* CSharpBindings::CreateFloatInputAction(MonoString* nameObj) {
        char* nameStr = mono_string_to_utf8(nameObj);
        CLEANUP(mono_free(nameStr));

        return instance().requestCarrotObject<Carrot::IO::FloatInputAction>(instance().FloatInputActionClass, nameStr).toMono();
    }

    float CSharpBindings::GetFloatInputValue(MonoObject* input) {
        auto& action = getObject<Carrot::IO::FloatInputAction>(input);
        return action.getValue();
    }

    MonoObject* CSharpBindings::CreateVec2InputAction(MonoString* nameObj) {
        char* nameStr = mono_string_to_utf8(nameObj);
        CLEANUP(mono_free(nameStr));

        return instance().requestCarrotObject<Carrot::IO::Vec2InputAction>(instance().Vec2InputActionClass, nameStr).toMono();
    }

    void CSharpBindings::_GetVec2InputValue(MonoObject* input, glm::vec2* out) {
        auto& action = getObject<Carrot::IO::Vec2InputAction>(input);
        *out = glm::vec2{ action.getValue().x, action.getValue().y };
    }

    void CSharpBindings::_AddToActionSet(MonoObject* setObj, MonoObject* actionObj) {
        auto& set = getObject<Carrot::IO::ActionSet>(setObj);
        if(mono_object_isinst(actionObj, instance().BoolInputActionClass->toMono())) {
            auto& action = getObject<Carrot::IO::BoolInputAction>(actionObj);
            set.add(action);
        } else if(mono_object_isinst(actionObj, instance().FloatInputActionClass->toMono())) {
            auto& action = getObject<Carrot::IO::FloatInputAction>(actionObj);
            set.add(action);
        } else if(mono_object_isinst(actionObj, instance().Vec2InputActionClass->toMono())) {
            auto& action = getObject<Carrot::IO::Vec2InputAction>(actionObj);
            set.add(action);
        } else {
            verify(false, "Unhandled type!"); // TODO
        }
    }

    void CSharpBindings::_ActivateActionSet(MonoObject* setObj) {
        auto& set = getObject<Carrot::IO::ActionSet>(setObj);
        set.activate();
    }
}