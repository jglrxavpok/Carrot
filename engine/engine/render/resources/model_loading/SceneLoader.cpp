//
// Created by jglrxavpok on 03/08/2023.
//

#include "SceneLoader.h"
#include <engine/render/resources/model_loading/AssimpLoader.h>
#include <core/scene/GLTFLoader.h>

namespace Carrot::Render {
    LoadedScene& SceneLoader::load(const Carrot::IO::Resource& file) {
        verify(file.isFile(), "In-memory models are not supported!");
        const Carrot::IO::Path filePath { Carrot::toString(file.getFilepath().u8string()).c_str() };

        if(filePath.getExtension() == ".gltf") {
            Render::GLTFLoader loader;
            scene = std::move(loader.load(file));
        } else {
            Render::AssimpLoader loader;
            scene = std::move(loader.load(file));
        }
        return scene;
    }
} // Carrot::Render