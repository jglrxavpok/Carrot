//
// Created by jglrxavpok on 20/03/2025.
//

#include "core/Macros.h"
#include "core/io/Logging.hpp"
#include "engine/scripting/CSharpBindings.h"
#include "core/scripting/csharp/Engine.h"
#include "engine/assets/AssetServer.h"
#include "engine/ecs/Prefab.h"
#include "engine/scene/SceneManager.h"

namespace Carrot::Scripting {
    static CSharpBindings& instance() {
        return GetCSharpBindings();
    }

    static ECS::Prefab* convertToPrefab(MonoObject* pPrefab) {
        if(pPrefab == nullptr) {
            return nullptr;
        }
        auto entityObj = Scripting::CSObject(pPrefab);
        ECS::Prefab* prefabPtr = *((ECS::Prefab**)mono_object_unbox(instance().PrefabPointerField->get(entityObj)));
        return prefabPtr;
    }

    std::shared_ptr<Scripting::CSObject> CSharpBindings::prefabToCSObject(ECS::Prefab& e) {
        ECS::Prefab* ptr = &e;
        void* args[1] = {
            &ptr,
        };
        // TODO: track that it is used by C# ?
        return instance().PrefabClass->newObject(args);
    }

    static MonoObject* _Load(MonoString* path) {
        char* prefabPath = mono_string_to_utf8(path);
        CLEANUP(mono_free(prefabPath));

        auto pPrefab = GetAssetServer().loadPrefab(prefabPath);
        return instance().prefabToCSObject(*pPrefab)->toMono();
    }

    static MonoObject* _Instantiate(MonoObject* self) {
        ECS::Prefab* pPrefab = convertToPrefab(self);
        if (!pPrefab) {
            Carrot::Log::error("Could not convert prefab from C#");
            return nullptr;
        }

        // TODO: better way to select world?
        ECS::World& world = GetSceneManager().getMainScene().world;
        return instance().entityToCSObject(pPrefab->instantiate(world))->toMono();
    }

    void CSharpBindings::addPrefabBindingTypes() {
        LOAD_CLASS(Prefab);
        PrefabPointerField = PrefabClass->findField("_pointer");
        verify(PrefabPointerField, "Missing Carrot.Prefab::_pointer field in Carrot.dll !");
    }

    void CSharpBindings::addPrefabBindingMethods() {
        mono_add_internal_call("Carrot.Prefab::Load", _Load);
        mono_add_internal_call("Carrot.Prefab::Instantiate", _Instantiate);
    }
}
