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

namespace Carrot::Scripting {
    /// Interface to use Carrot.dll and game dlls made in C#
    /// Contrary to ScriptingEngine, does not support having multiple game assemblies loaded at once
    class CSharpBindings {
    public:
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

        void loadGameAssembly(const IO::VFS::Path& gameDLL);
        void reloadGameAssembly();
        void unloadGameAssembly();

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
        std::unique_ptr<CSAppDomain> appDomain;
        std::shared_ptr<CSAssembly> baseModule;

        IO::VFS::Path gameModuleLocation{};
        std::shared_ptr<CSAssembly> gameModule;

    private:
        std::unordered_map<std::string, ComponentID> HardcodedComponentIDs;
        MonoAppDomain* gameAppDomain = nullptr;
    };
}