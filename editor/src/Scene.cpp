//
// Created by jglrxavpok on 06/10/2021.
//

#include "Scene.h"

namespace Peeler {
    void Scene::tick(double frameTime) {
        world.tick(frameTime);
    }

    void Scene::onFrame(const Carrot::Render::Context& renderContext) {
        world.onFrame(renderContext);
    }

    Scene& Scene::operator=(const Scene& toCopy) {
        world = toCopy.world;
        return *this;
    }

    void Scene::clear() {
        *this = Scene();
    }
}