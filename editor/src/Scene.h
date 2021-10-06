//
// Created by jglrxavpok on 06/10/2021.
//

#pragma once

#include <engine/ecs/World.h>
#include <engine/render/RenderContext.h>

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
        Scene& operator=(const Scene& toCopy);
    };
}
