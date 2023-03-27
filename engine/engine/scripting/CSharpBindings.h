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

        CSClass* SystemClass = nullptr;
        CSField* SystemHandleField = nullptr;
        CSField* SystemSignatureField = nullptr;

        CSClass* ComponentClass = nullptr;
        CSField* ComponentOwnerField = nullptr;

        CSClass* TransformComponentClass = nullptr;

    public:
        CSharpBindings();

        ~CSharpBindings();

        void loadGameAssembly(const IO::VFS::Path& gameDLL);
        void reloadGameAssembly();
        void unloadGameAssembly();

        const Carrot::IO::VFS::Path& getEngineDllPath() const;

    public:
        Callbacks::Handle registerGameAssemblyLoadCallback(const std::function<void()>& callback);
        Callbacks::Handle registerGameAssemblyUnloadCallback(const std::function<void()>& callback);
        void unregisterGameAssemblyLoadCallback(const Callbacks::Handle& handle);
        void unregisterGameAssemblyUnloadCallback(const Callbacks::Handle& handle);

    public:
        ComponentID requestComponentID(const std::string& namespaceName, const std::string& className);

    public: // public because they might be useful to other parts of the engine
        struct CppComponent {
            bool isCSharp = false;
            ComponentID id = -1;
            CSClass* clazz = nullptr; // C# class used to represent this C++ component on the script side
        };

        static std::int32_t GetMaxComponentCount();

        static ComponentID GetComponentID(MonoString* namespaceStr, MonoString* classStr);

        static MonoArray* LoadEntities(MonoObject* systemObj);

        static ECS::Entity convertToEntity(MonoObject* entityMonoObj);

        static std::shared_ptr<Scripting::CSObject> entityToCSObject(ECS::Entity& e);

        static MonoObject* GetComponent(MonoObject* entityMonoObj, MonoString* namespaceStr, MonoString* classStr);

        static glm::vec3 _GetLocalPosition(MonoObject* transformComp);

        static void _SetLocalPosition(MonoObject* transformComp, glm::vec3 value);

        static MonoString* GetName(MonoObject* entityMonoObj);

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
        Carrot::IO::VFS::Path engineDllPath = "engine://scripting/Carrot.dll";
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
    };
}