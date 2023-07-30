//
// Created by jglrxavpok on 14/03/2023.
//

#pragma once

#include <string>
#include <unordered_map>
#include <core/io/Resource.h>
#include <core/scripting/csharp/forward.h>
#include <core/scripting/csharp/CSAppDomain.h>
#include <core/utils/Identifiable.h>
#include <mono/metadata/appdomain.h>
#include <engine/ecs/EntityTypes.h>
#include <engine/ecs/systems/System.h>
#include <engine/ecs/components/Component.h>
#include <eventpp/callbacklist.h>
#include <engine/scripting/ComponentProperty.h>
#include <engine/scripting/CSharpReflectionHelper.h>
#include <engine/scripting/CarrotCSObject.h>

namespace Carrot::Scripting {
    /// Interface to use Carrot.dll and game dlls made in C#
    /// Contrary to ScriptingEngine, does not support having multiple game assemblies loaded at once
    class CSharpBindings {
    public:
        using Callbacks = eventpp::CallbackList<void()>;

        CSProperty* SystemTypeFullNameProperty = nullptr;

        CSClass* EntityClass = nullptr;
        CSField* EntityIDField = nullptr;
        CSField* EntityUserPointerField = nullptr;

        CSClass* CarrotObjectClass = nullptr;
        CSField* CarrotObjectHandleField = nullptr;
        CSClass* CarrotReferenceClass = nullptr;
        CSField* CarrotReferenceHandleField = nullptr;

        CSClass* SystemClass = nullptr;
        CSField* SystemSignatureField = nullptr;

        CSClass* Vec2Class = nullptr;
        CSClass* Vec3Class = nullptr;

        CSClass* ComponentClass = nullptr;
        CSField* ComponentOwnerField = nullptr;

        CSClass* TransformComponentClass = nullptr;
        CSClass* TextComponentClass = nullptr;
        CSClass* RigidBodyComponentClass = nullptr;
        CSClass* ColliderClass = nullptr;
        CSClass* CharacterComponentClass = nullptr;

        CSClass* NavMeshComponentClass = nullptr;
        CSClass* NavPathClass = nullptr;
        CSField* NavPathWaypointsField = nullptr;

        CSClass* RaycastInfoClass = nullptr;
        CSField* RaycastInfoWorldPointField = nullptr;
        CSField* RaycastInfoWorldNormalField = nullptr;
        CSField* RaycastInfoTField = nullptr;
        CSField* RaycastInfoColliderField = nullptr;

        CSClass* ActionSetClass = nullptr;
        CSClass* BoolInputActionClass = nullptr;
        CSClass* FloatInputActionClass = nullptr;
        CSClass* Vec2InputActionClass = nullptr;

    public:
        CSharpBindings();

        ~CSharpBindings();

        void tick(double deltaTime);

        void loadGameAssembly(const IO::VFS::Path& gameDLL);
        void reloadGameAssembly();
        void unloadGameAssembly();

        const Carrot::IO::VFS::Path& getEngineDllPath() const;
        const Carrot::IO::VFS::Path& getEnginePdbPath() const;

    public:
        Callbacks::Handle registerGameAssemblyLoadCallback(const std::function<void()>& callback);
        Callbacks::Handle registerGameAssemblyUnloadCallback(const std::function<void()>& callback);
        void unregisterGameAssemblyLoadCallback(const Callbacks::Handle& handle);
        void unregisterGameAssemblyUnloadCallback(const Callbacks::Handle& handle);

    public: // reflection stuff
        std::vector<ComponentProperty> findAllComponentProperties(const std::string& namespaceName, const std::string& className);
        MonoDomain* getAppDomain();

    public:
        ComponentID requestComponentID(const std::string& namespaceName, const std::string& className);

        std::vector<ComponentID> getAllComponentIDs() const;

        template<typename T, typename... Args>
        Scripting::CarrotCSObject<T>& requestCarrotObject(Scripting::CSClass* csType, Args&&... args) {
            carrotObjects.push_back(std::make_unique<CarrotCSObject<T>>(csType, std::forward<Args>(args)...));
            return *((CarrotCSObject<T>*)carrotObjects.back().get());
        }

        template<typename T>
        std::shared_ptr<Scripting::CSObject> requestCarrotReference(Scripting::CSClass* csType, T* ptr) {
            verify(ptr != nullptr, "Cannot have a reference to a nullptr");

            std::uint64_t ptrValue = reinterpret_cast<std::uint64_t>(static_cast<void*>(ptr));
            void* args[] = {
                    &ptrValue
            };
            return csType->newObject(args);
        }

    public: // public because they might be useful to other parts of the engine
        struct CppComponent {
            bool isCSharp = false;
            ComponentID id = -1;
            CSClass* clazz = nullptr; // C# class used to represent this C++ component on the script side
        };

        static std::int32_t GetMaxComponentCount();

        static ComponentID GetComponentID(MonoString* namespaceStr, MonoString* classStr);

        static MonoArray* LoadEntities(MonoObject* systemObj);

        static MonoArray* _QueryECS(MonoObject* systemObj, MonoArray* componentClasses);

        static ECS::Entity convertToEntity(MonoObject* entityMonoObj);

        static std::shared_ptr<Scripting::CSObject> entityToCSObject(ECS::Entity& e);

        static MonoObject* GetComponent(MonoObject* entityMonoObj, MonoString* namespaceStr, MonoString* classStr);

        static MonoString* GetName(MonoObject* entityMonoObj);

        static MonoObject* GetParent(MonoObject* entityMonoObj);
        static void SetParent(MonoObject* entityMonoObj, MonoObject* newParentMonoObj);
        static void ReParent(MonoObject* entityMonoObj, MonoObject* newParentMonoObj);

        static MonoObject* FindEntityByName(MonoObject* systemObj, MonoString* entityName);

    public: // input API
        static MonoObject* CreateActionSet(MonoString* nameObj);
        static void SuggestBinding(MonoObject* actionObj, MonoString* bindingStr);

        static MonoObject* CreateBoolInputAction(MonoString* nameObj);
        static bool IsBoolInputPressed(MonoObject* boolInput);
        static bool WasBoolInputJustPressed(MonoObject* boolInput);
        static bool WasBoolInputJustReleased(MonoObject* boolInput);

        static MonoObject* CreateFloatInputAction(MonoString* nameObj);
        static float GetFloatInputValue(MonoObject* input);

        static MonoObject* CreateVec2InputAction(MonoString* nameObj);

        // takes a glm::vec2* instead of returning it: Mono seems to crash when an object with 2 members of size 32 has a constructor
        // don't ask me why, i don't understand it
        static void _GetVec2InputValue(MonoObject* input, glm::vec2* out);

        static void _AddToActionSet(MonoObject* setObj, MonoObject* actionObj);
        static void _ActivateActionSet(MonoObject* setObj);

    public: // hardcoded component interfaces
        static glm::vec3 _GetLocalPosition(MonoObject* transformComp);
        static void _SetLocalPosition(MonoObject* transformComp, glm::vec3 value);
        static glm::vec3 _GetLocalScale(MonoObject* transformComp);
        static void _SetLocalScale(MonoObject* transformComp, glm::vec3 value);
        static glm::vec3 _GetEulerAngles(MonoObject* transformComp);
        static void _SetEulerAngles(MonoObject* transformComp, glm::vec3 value);
        static glm::vec3 _GetWorldPosition(MonoObject* transformComp);

        static glm::vec3 _GetCharacterVelocity(MonoObject* characterComp);
        static void _SetCharacterVelocity(MonoObject* characterComp, glm::vec3 value);
        static bool _IsCharacterOnGround(MonoObject* characterComp);
        static void EnableCharacterPhysics(MonoObject* characterComp);
        static void DisableCharacterPhysics(MonoObject* characterComp);

        static MonoString* _GetText(MonoObject* textComponent);
        static void _SetText(MonoObject* textComponent, MonoString* value);

        static glm::vec3 _GetRigidBodyVelocity(MonoObject* comp);
        static void _SetRigidBodyVelocity(MonoObject* comp, glm::vec3 value);

        static std::uint64_t GetRigidBodyColliderCount(MonoObject* comp);
        static MonoObject* GetRigidBodyCollider(MonoObject* comp, std::uint64_t index);

        static bool RaycastCollider(MonoObject* collider, glm::vec3 start, glm::vec3 direction, float maxLength, MonoObject* raycastInfo);
        static bool _DoRaycast(MonoObject* collider, glm::vec3 start, glm::vec3 direction, float maxLength, MonoObject* raycastInfo, std::uint16_t collisionMask);

        static glm::vec3 GetClosestPointInMesh(MonoObject* navMeshComponent, glm::vec3 p);
        static MonoObject* PathFind(MonoObject* navMeshComponent, glm::vec3 a, glm::vec3 b);

    private:
        void loadEngineAssembly();

        // used when triggering a game dll load for the first time: remove engine dll from old app domain. It will be reloaded in the new domain after
        void unloadOnlyEngineAssembly();

        /**
         * Returns ComponentID + C# class representing the given type.
         * Returns an object with id = -1 and clazz = nullptr if none match
         */
        CppComponent getComponentFromType(const std::string& namespaceName, const std::string& className);

    private:
        CSharpReflectionHelper reflectionHelper;
        Carrot::IO::VFS::Path engineDllPath;
        Carrot::IO::VFS::Path enginePdbPath;
        std::unique_ptr<CSAppDomain> appDomain;
        std::shared_ptr<CSAssembly> baseModule;

        IO::VFS::Path gameModuleLocation{};
        std::shared_ptr<CSAssembly> gameModule;

        Callbacks unloadCallbacks;
        Callbacks loadCallbacks;

        Carrot::Async::ParallelMap<std::string, ComponentID> csharpComponentIDs;
        std::unordered_map<std::string, CppComponent> HardcodedComponents;
        MonoAppDomain* gameAppDomain = nullptr;

        // system & component IDs found inside the game assembly
        std::vector<ECS::ComponentLibrary::ID> componentIDs;
        std::vector<ECS::SystemLibrary::ID> systemIDs;

        std::vector<std::unique_ptr<Scripting::CarrotCSObjectBase>> carrotObjects;
    };
}