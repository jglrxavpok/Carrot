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
        CSClass* EntityWithComponentsClass = nullptr;

        CSClass* PrefabClass = nullptr;
        CSField* PrefabPointerField = nullptr;

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
        CSMethod* RigidBodyOnContactAddedMethod = nullptr;

        CSClass* ColliderClass = nullptr;
        CSClass* CharacterComponentClass = nullptr;

        CSClass* NavMeshComponentClass = nullptr;
        CSClass* NavPathClass = nullptr;
        CSField* NavPathWaypointsField = nullptr;

        CSClass* CameraComponentClass = nullptr;
        CSClass* KinematicsComponentClass = nullptr;
        CSClass* LightComponentClass = nullptr;
        CSClass* AnimatedModelComponentClass = nullptr;
        CSClass* ModelComponentClass = nullptr;

        CSClass* RaycastInfoClass = nullptr;
        CSField* RaycastInfoWorldPointField = nullptr;
        CSField* RaycastInfoWorldNormalField = nullptr;
        CSField* RaycastInfoTField = nullptr;
        CSField* RaycastInfoColliderField = nullptr;

        CSClass* RayCastSettingsClass = nullptr;
        CSField* RayCastSettingsOriginField = nullptr;
        CSField* RayCastSettingsDirField = nullptr;
        CSField* RayCastSettingsMaxLengthField = nullptr;
        CSField* RayCastSettingsIgnoreCharactersField = nullptr;
        CSField* RayCastSettingsIgnoreBodiesField = nullptr;
        CSField* RayCastSettingsIgnoreLayersField = nullptr;

        CSClass* ActionSetClass = nullptr;
        CSClass* BoolInputActionClass = nullptr;
        CSClass* FloatInputActionClass = nullptr;
        CSClass* Vec2InputActionClass = nullptr;

        CSClass* DebugClass = nullptr;
        CSMethod* DebugGetDrawnLinesMethod = nullptr;

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
        Callbacks::Handle registerGameAssemblyLoadCallback(const std::function<void()>& callback, bool prepend = false);
        Callbacks::Handle registerGameAssemblyUnloadCallback(const std::function<void()>& callback);
        void unregisterGameAssemblyLoadCallback(const Callbacks::Handle& handle);
        void unregisterGameAssemblyUnloadCallback(const Callbacks::Handle& handle);

        Callbacks::Handle registerEngineAssemblyLoadCallback(const std::function<void()>& callback);
        Callbacks::Handle registerEngineAssemblyUnloadCallback(const std::function<void()>& callback);
        void unregisterEngineAssemblyLoadCallback(const Callbacks::Handle& handle);
        void unregisterEngineAssemblyUnloadCallback(const Callbacks::Handle& handle);

    public: // reflection stuff
        std::vector<ComponentProperty> findAllComponentProperties(const std::string& namespaceName, const std::string& className);
        MonoDomain* getAppDomain();

    public:
        ComponentID requestComponentID(const std::string& namespaceName, const std::string& className);

        std::vector<ComponentID> getAllComponentIDs() const;
        Carrot::Async::ParallelMap<std::string, ComponentID>::ConstSnapshot getAllComponents() const;

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

        std::shared_ptr<Scripting::CSArray> entityListToCSharp(std::span<const ECS::EntityWithComponents> entities);

        /**
         * Attempts to convert a given string into a enum value, for a given enum type.
         * Will throw if the enum type name is invalid, or if the value itself is invalid.
         * @param enumTypeName fully qualified name of the enum (MyNameSpace.ExampleEnum)
         * @param enumValue name of the enum value to convert to
         * @return a CSObject corresponding to the value of the enum (can be given to field or methods expecting the enum type)
         */
        CSObject stringToEnumValue(const std::string& enumTypeName, const std::string& enumValue) const;
        std::string enumValueToString(const CSObject& enumValue) const;

        std::shared_ptr<Scripting::CSArray> createArrayFromEntityList(std::span<const Carrot::ECS::Entity> entities) const;

    public: // public because they might be useful to other parts of the engine
        struct CppComponent {
            bool isCSharp = false;
            ComponentID id = -1;
            CSClass* clazz = nullptr; // C# class used to represent this C++ component on the script side
        };

        static std::int32_t _GetMaxComponentCountUncached();
        static void BeginProfilingZone(MonoString* zoneName);
        static void EndProfilingZone(MonoString* zoneName);
        static glm::vec3 EulerToForwardVector(float pitch, float yaw, float roll);
        static void ShutdownGame();

        static ComponentID GetComponentID(MonoString* namespaceStr, MonoString* classStr);
        static ComponentID GetComponentIndex(MonoString* namespaceStr, MonoString* classStr);

        static MonoArray* LoadEntities(MonoObject* systemObj);

        static MonoArray* _QueryECS(MonoObject* systemObj, std::uint64_t componentsBitset);

        static ECS::Entity convertToEntity(MonoObject* entityMonoObj);

        static std::shared_ptr<Scripting::CSObject> entityToCSObject(ECS::Entity e);
        static std::shared_ptr<Scripting::CSObject> prefabToCSObject(ECS::Prefab& e);

        static MonoObject* GetComponent(MonoObject* entityMonoObj, MonoString* namespaceStr, MonoString* classStr);

        static MonoString* GetName(MonoObject* entityMonoObj);

        static void Remove(MonoObject* entityMonoObj);
        static MonoObject* GetParent(MonoObject* entityMonoObj);
        static MonoArray* GetEntityChildren(MonoObject* entityMonoObj);
        static void SetParent(MonoObject* entityMonoObj, MonoObject* newParentMonoObj);
        static void ReParent(MonoObject* entityMonoObj, MonoObject* newParentMonoObj);
        static MonoObject* Duplicate(MonoObject* entityMonoObj);
        static bool EntityExists(MonoObject* entityMonoObj);
        static bool IsEntityVisible(MonoObject* entityMonoObj);
        static void HideEntity(MonoObject* entityMonoObj, bool recursive);
        static void ShowEntity(MonoObject* entityMonoObj, bool recursive);

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
        static void _AddRotationAroundX(MonoObject* transformComp, float value);
        static void _AddRotationAroundY(MonoObject* transformComp, float value);
        static void _AddRotationAroundZ(MonoObject* transformComp, float value);
        static glm::vec3 _GetWorldPosition(MonoObject* transformComp);
        static MonoArray* _GetWorldUpForwardVectors(MonoObject* transformComp);

        static glm::vec3 _GetKinematicsLocalVelocity(MonoObject* comp);
        static void _SetKinematicsLocalVelocity(MonoObject* comp, glm::vec3 value);

        static bool _GetLightEnabled(MonoObject* comp);
        static void _SetLightEnabled(MonoObject* comp, bool value);
        static float _GetLightIntensity(MonoObject* comp);
        static void _SetLightIntensity(MonoObject* comp, float value);

        static void TeleportCharacter(MonoObject* characterComp, glm::vec3 newPos);
        static glm::vec3 _GetCharacterVelocity(MonoObject* characterComp);
        static void _SetCharacterVelocity(MonoObject* characterComp, glm::vec3 value);
        static bool _IsCharacterOnGround(MonoObject* characterComp);
        static void EnableCharacterPhysics(MonoObject* characterComp);
        static void DisableCharacterPhysics(MonoObject* characterComp);

        static MonoString* _GetText(MonoObject* textComponent);
        static void _SetText(MonoObject* textComponent, MonoString* value);
        static glm::vec4 _GetTextColor(MonoObject* textComponent);
        static void _SetTextColor(MonoObject* textComponent, glm::vec4 value);

        static glm::vec3 _GetRigidBodyAngularVelocityInLocalSpace(MonoObject* comp);
        static glm::vec3 _GetRigidBodyVelocity(MonoObject* comp);
        static void _SetRigidBodyVelocity(MonoObject* comp, glm::vec3 value);
        static void _RigidBodyRegisterForContacts(MonoObject* comp);

        static bool _RigidBodyGetRegisteredForContacts(MonoObject* comp);
        static void _RigidBodySetRegisteredForContacts(MonoObject* comp, bool);

        static MonoObject* _RigidBodyGetCallbacksHolder(MonoObject* comp);
        static void _RigidBodySetCallbacksHolder(MonoObject* comp, MonoObject* holder);

        static glm::vec3 _RigidBodyGetPointVelocityInLocalSpace(MonoObject* comp, glm::vec3 point);
        static void _RigidBodyAddForceAtPointInLocalSpace(MonoObject* comp, glm::vec3 force, glm::vec3 point);
        static void _RigidBodyAddRelativeForceInLocalSpace(MonoObject* comp, glm::vec3 force);

        static MonoObject* _ColliderGetEntity(MonoObject* pColliderWrapper);

        static std::uint64_t GetRigidBodyColliderCount(MonoObject* comp);
        static MonoObject* GetRigidBodyCollider(MonoObject* comp, std::uint64_t index);

        static bool RaycastCollider(MonoObject* collider, glm::vec3 start, glm::vec3 direction, float maxLength, MonoObject* raycastInfo);

        static bool _RaycastHelper(MonoObject* raycastSettings, MonoObject* pCSRaycastInfo);
        static bool RaycastRigidbody(MonoObject* rigidbodyComp, MonoObject* raycastSettings, MonoObject* pCSRaycastInfo);
        static bool RaycastCharacter(MonoObject* characterComp, MonoObject* raycastSettings, MonoObject* pCSRaycastInfo);

        static glm::vec3 GetClosestPointInMesh(MonoObject* navMeshComponent, glm::vec3 p);
        static MonoObject* PathFind(MonoObject* navMeshComponent, glm::vec3 a, glm::vec3 b);

        static glm::vec3 GetAimDirectionFromScreen(MonoObject* cameraComponent, glm::vec2 screenPosition);
        static void GrabCursor();
        static void UngrabCursor();

        static float _GetAnimatedModelAnimationTime(MonoObject* comp);
        static void _SetAnimatedModelAnimationTime(MonoObject* comp, float newTime);
        static std::uint32_t _GetAnimatedModelAnimationIndex(MonoObject* comp);
        static void _SetAnimatedModelAnimationIndex(MonoObject* comp, std::uint32_t newValue);
        static bool SelectAnimatedModelAnimation(MonoObject* comp, MonoString* animationName);

        /**
         * From a given component ID, returns the C# class used to represent that component type.
         * Can return null if there is none.
         */
        CSClass* getHardcodedComponentClass(const ComponentID& componentID);

    private:
        void addPrefabBindingTypes();
        void addPrefabBindingMethods();
        void addModelBindingTypes();
        void addModelBindingMethods();

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
        Callbacks unloadEngineCallbacks;
        Callbacks loadEngineCallbacks;

        Carrot::Async::ParallelMap<std::string, ComponentID> csharpComponentIDs;
        std::unordered_map<std::string, CppComponent> hardcodedComponents;
        MonoAppDomain* gameAppDomain = nullptr;

        // system & component IDs found inside the game assembly
        std::vector<ECS::ComponentLibrary::ID> componentIDs;
        std::vector<ECS::SystemLibrary::ID> systemIDs;

        std::vector<std::unique_ptr<Scripting::CarrotCSObjectBase>> carrotObjects;
    };
}