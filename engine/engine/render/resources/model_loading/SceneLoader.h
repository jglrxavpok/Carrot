//
// Created by jglrxavpok on 03/08/2023.
//

#pragma once

#include <core/scene/LoadedScene.h>
#include <core/io/Resource.h>

namespace Carrot::Render {

    /// Loads a CPU accessible version of a scene, from a file
    class SceneLoader {
    public:
        SceneLoader() = default;
        LoadedScene& load(const Carrot::IO::Resource& resource);

    private:
        LoadedScene scene;
    };

} // Carrot::Render
