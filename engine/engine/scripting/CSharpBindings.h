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

    public: // public because they might be useful to other parts of the engine
        static std::int32_t GetMaxComponentCount();

        static ComponentID getComponentIDFromTypeName(const std::string& name);

        static ComponentID GetComponentID(MonoString* str);

        static MonoArray* LoadEntities(MonoObject* systemObj);

        static ECS::Entity convertToEntity(MonoObject* entityMonoObj);

        static std::shared_ptr<Scripting::CSObject> entityToCSObject(ECS::Entity& e);

        static MonoObject* GetComponent(MonoObject* entityMonoObj, MonoString* type);

        static glm::vec3 _GetLocalPosition(MonoObject* transformComp);

        static void _SetLocalPosition(MonoObject* transformComp, glm::vec3 value);

        static MonoString* GetName(MonoObject* entityMonoObj);

    private:
        void loadEngineAssembly();

        // used when triggering a game dll load for the first time: remove engine dll from old app domain. It will be reloaded in the new domain after
        void unloadOnlyEngineAssembly();

    private:
        Carrot::IO::VFS::Path engineDllPath = "engine://scripting/Carrot.dll";
        std::unique_ptr<CSAppDomain> appDomain;
        std::shared_ptr<CSAssembly> baseModule;

        IO::VFS::Path gameModuleLocation{};
        std::shared_ptr<CSAssembly> gameModule;

        Callbacks unloadCallbacks;
        Callbacks loadCallbacks;

        std::unordered_map<std::string, ComponentID> HardcodedComponentIDs;
        MonoAppDomain* gameAppDomain = nullptr;
    };
}