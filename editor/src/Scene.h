//
// Created by jglrxavpok on 06/10/2021.
//

#pragma once

#include <engine/ecs/World.h>
#include <engine/render/RenderContext.h>
#include <rapidjson/document.h>

namespace Peeler {
    class Scene {
    public:
        Carrot::ECS::World world;

        explicit Scene() = default;
        Scene(const Scene& toCopy) {
            *this = toCopy;
        }

    public:
        void clear();

    public:
        void tick(double frameTime);
        void onFrame(const Carrot::Render::Context& renderContext);

    public:
        void deserialise(const rapidjson::Value& src);
        rapidjson::Value serialise(rapidjson::Document& dest) const;

    public:
        Scene& operator=(const Scene& toCopy);
    };
}
