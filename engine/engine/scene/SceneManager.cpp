//
// Created by jglrxavpok on 30/08/2023.
//

#include <core/io/Logging.hpp>
#include "SceneManager.h"

#include <core/scripting/csharp/Engine.h>
#include <engine/scripting/CSharpBindings.h>
#include <engine/scripting/CSharpReflectionHelper.h>
#include <engine/scripting/CSharpHelpers.ipp>
#include <core/scripting/csharp/CSClass.h>
#include <core/scripting/csharp/CSObject.h>

namespace Carrot {

    Scene& SceneManager::getMainScene() {
        verify(pMainScene, "Main scene")
        return *pMainScene;
    }

    Scene& SceneManager::loadScene(const Carrot::IO::VFS::Path& path) {
        auto& scene = scenes.emplace_back();
        try {
            scene.deserialise(path);
        } catch (std::exception& e) {
            Carrot::Log::error("Failed to open scene: %s", e.what());
            scene.clear();
        }
        return scene;
    }

    Scene& SceneManager::loadSceneAdditive(const Carrot::IO::VFS::Path& path, Scene& addTo) {
        rapidjson::Document sceneDoc;
        try {
            addTo.deserialise(path);
        } catch (std::exception& e) {
            Carrot::Log::error("Failed to open scene: %s", e.what());
        }
        return addTo;
    }

    Scene& SceneManager::queueChangeScene(const Carrot::IO::VFS::Path& scenePath) {
        pMainSceneReplacement = std::make_unique<Scene>();
        try {
            pMainSceneReplacement->clear();
            pMainSceneReplacement->deserialise(scenePath);
        } catch (std::exception& e) {
            Carrot::Log::error("Failed to open scene: %s", e.what());
            pMainSceneReplacement->clear();
        }
        return *pMainSceneReplacement;
    }

    void SceneManager::deleteScene(Scene&& scene) {
        for(auto it = scenes.begin(); it != scenes.end(); it++) {
            if(&(it.operator*()) == &scene) {
                scenes.erase(it);
                break;
            }
        }
    }

    void SceneManager::swapMainScene() {
        if (pMainSceneReplacement) {
            pMainScene->copyFrom(*pMainSceneReplacement);
            pMainScene->world.unfreezeLogic();
            pMainScene->world.broadcastStartEvent();
            pMainScene->load();

            pMainSceneReplacement = nullptr;
        }
    }


    Scene* SceneManager::SceneIterator::operator*() {
        verify(index <= pManager->scenes.size(), "Dereferencing an invalid iterator");
        if(index == 0) {
            return pManager->pMainScene.get();
        } else {
            std::size_t n = index-1;
            auto iter = pManager->scenes.begin();
            for (int i = 0; i < n; ++i) {
                iter++;
            }
            return &(iter.operator*());
        }
    }

    SceneManager::SceneIterator& SceneManager::SceneIterator::operator++() {
        index++;
        return *this;
    }

    bool SceneManager::SceneIterator::operator==(const SceneIterator& other) const {
        return index == other.index && pManager == other.pManager;
    }

    bool SceneManager::SceneIterator::operator==(const SceneIterator::End&) const {
        return index > pManager->scenes.size();
    }

    SceneManager::SceneIterator SceneManager::begin() {
        SceneIterator it;
        it.index = 0;
        it.pManager = this;
        return it;
    }

    SceneManager::SceneIterator::End SceneManager::end() {
        return SceneIterator::End{};
    }

    using namespace Scripting;
    struct Bindings {
        CSClass* SceneClass;

        static Bindings& self() {
            return *((Bindings*)GetSceneManager().bindingsImpl);
        }

        static MonoObject* GetMainScene() {
            return GetCSharpBindings().requestCarrotReference(self().SceneClass, &GetSceneManager().getMainScene())->toMono();
        }

        static MonoObject* QueueChangeScene(MonoString* vfsPathObj) {
            char* vfsPath = mono_string_to_utf8(vfsPathObj);
            CLEANUP(mono_free(vfsPath));

            return GetCSharpBindings().requestCarrotReference(self().SceneClass, &GetSceneManager().queueChangeScene(vfsPath))->toMono();
        }

        static MonoObject* LoadScene(MonoString* vfsPathObj) {
            char* vfsPath = mono_string_to_utf8(vfsPathObj);
            CLEANUP(mono_free(vfsPath));

            return GetCSharpBindings().requestCarrotReference(self().SceneClass, &GetSceneManager().loadScene(vfsPath))->toMono();
        }

        static MonoObject* LoadSceneAdditive(MonoString* vfsPathObj, MonoObject* addToHandle) {
            char* vfsPath = mono_string_to_utf8(vfsPathObj);
            CLEANUP(mono_free(vfsPath));

            Scene& addTo = getReference<Scene>(addToHandle);
            return GetCSharpBindings().requestCarrotReference(self().SceneClass, &GetSceneManager().loadSceneAdditive(vfsPath, addTo))->toMono();
        }

        static void Delete(MonoObject* sceneHandle) {
            Scene& scene = getReference<Scene>(sceneHandle);
            GetSceneManager().deleteScene(std::move(scene));
        }

        Bindings() {
            GetCSharpBindings().registerEngineAssemblyLoadCallback([&]() {
                LOAD_CLASS(Scene);
            });

            mono_add_internal_call("Carrot.SceneManager::GetMainScene", (void*)GetMainScene);
            mono_add_internal_call("Carrot.SceneManager::QueueChangeScene", (void*)QueueChangeScene);
            mono_add_internal_call("Carrot.SceneManager::LoadScene", (void*)LoadScene);
            mono_add_internal_call("Carrot.SceneManager::LoadSceneAdditive", (void*)LoadSceneAdditive);
            mono_add_internal_call("Carrot.SceneManager::Delete", (void*)Delete);
        }
    };

    SceneManager::~SceneManager() {
        delete (Bindings*)bindingsImpl;
    }

    void SceneManager::initScripting() {
        bindingsImpl = new Bindings;

        pMainScene = std::make_unique<Scene>();
    }
} // Carrot