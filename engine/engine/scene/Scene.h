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
        Scene(const Scene& toCopy) {
            *this = toCopy;
        }

    public:
        void clear();

    public:
        void setupCamera(const Carrot::Render::Context& renderContext);
        void tick(double frameTime);
        void prePhysics();
        void postPhysics();
        void onFrame(const Carrot::Render::Context& renderContext);

    public:
        void deserialise(const rapidjson::Value& src);
        void serialise(rapidjson::Document& dest) const;

    public:
        /// Unload the systems of this scene, freeing engine resources (eg lights, rigidbodies)
        void unload();

        /// Loads the systems of this scene, allocating engine resources (eg lights, rigidbodies). Automatically in this state when constructing or deserialising the scene
        void load();

    public:
        Scene& operator=(const Scene& toCopy) = default;
    };
}
