//
// Created by jglrxavpok on 30/08/2023.
//

#pragma once

#include <list>
#include <engine/scene/Scene.h>
#include <core/io/vfs/VirtualFileSystem.h>

namespace Carrot {

    struct Bindings;

    class SceneManager {
    public: // scene loading
        ~SceneManager();

        /**
         * Loads the scene at the given path and add it to the scene list.
         * Scenes loaded via loadScene are completely independent.
         * Returns the new scene
         */
        Scene& loadScene(const Carrot::IO::VFS::Path& path);

        /**
         * Loads scene data from the given path, and add it to the 'addTo' scene.
         *
         */
        Scene& loadSceneAdditive(const Carrot::IO::VFS::Path& path, Scene& addTo);

        /**
         * Call 'clear' + 'deserialize' on the main scene.
         * Note: because this does not change viewport bindings, binding the main scene, and calling 'changeScene'
         *  will ensure the main scene is still drawn to its bound viewports
         */
        Scene& changeScene(const Carrot::IO::VFS::Path& scenePath);

        /**
         * Deletes the given scene from memory.
         * This call MUST be the last access to 'scene', because the SceneManager WILL delete the instance
         */
        void deleteScene(Scene&& scene);

    public: // main scene
        /**
         * Gets the main scene.
         */
        Scene& getMainScene();

    public: // iteration
        struct SceneIterator {
            struct End{};

            Scene* operator*();
            SceneIterator& operator++();
            bool operator==(const SceneIterator& other) const;
            bool operator==(const SceneIterator::End&) const;

        private:
            SceneManager* pManager = nullptr;
            std::size_t index = 0; // by convention, 0 = main scene, >0 is index inside 'scenes'+1

            friend class SceneManager;
        };

        SceneIterator begin();

        SceneIterator::End end();

    public: // C# bindings
        // called by engine once C# scripting engine is ready. is NOT called once an assembly is reloaded
        void initScripting();

    private:
        void* bindingsImpl = nullptr;

        Scene mainScene{};
        std::list<Scene> scenes;
        friend struct Bindings;
    };

} // Carrot
