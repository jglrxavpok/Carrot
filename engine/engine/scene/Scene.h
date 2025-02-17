//
// Created by jglrxavpok on 06/10/2021.
//

#pragma once

#include <engine/ecs/World.h>
#include <engine/render/RenderContext.h>
#include <rapidjson/document.h>

namespace Carrot {
    class Scene {
    public:
        Carrot::ECS::World world;

        struct Lighting {
            glm::vec3 ambient{1.0f};
            bool raytracedShadows = true;
        } lighting;

        Carrot::Skybox::Type skybox = Carrot::Skybox::Type::None;

        explicit Scene() = default;
        Scene(const Scene& toCopy) = delete;

        ~Scene();

    public:
        /**
         * Removes world data (entities & systems) and resets configuration.
         * Does not change viewport bindings
         */
        void clear();

    public:
        void setupCamera(const Carrot::Render::Context& renderContext);
        void tick(double frameTime);
        void prePhysics();
        void postPhysics();
        void onFrame(const Carrot::Render::Context& renderContext);

    public:
        void deserialise(const Carrot::IO::VFS::Path& sceneFolder);
        void serialise(const std::filesystem::path& sceneFolder) const;

    public:
        /// Unload the systems of this scene, freeing engine resources (eg lights, rigidbodies)
        void unload();

        /// Loads the systems of this scene, allocating engine resources (eg lights, rigidbodies). Automatically in this state when constructing or deserialising the scene
        void load();

    public:
        /**
         * Binds to the given viewport. Bound scenes will have their 'onFrame' function called automatically with the proper context.
         */
        void bindToViewport(Carrot::Render::Viewport& viewport);

        /**
         * Unbinds from the given viewport.
         */
        void unbindFromViewport(Carrot::Render::Viewport& viewport);

        std::span<Carrot::Render::Viewport*> getViewports();

    public:
        /**
         * Copies settings and world from 'toCopy'.
         * Does NOT copy viewport bindings
         */
        void copyFrom(const Scene& toCopy);
        Scene& operator=(const Scene& toCopy) = delete;

    private:
        std::vector<Carrot::Render::Viewport*> viewports;
    };
}
