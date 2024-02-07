//
// Created by jglrxavpok on 03/08/2023.
//

#include "SceneLoader.h"
#include <core/scene/AssimpLoader.h>
#include <core/scene/GLTFLoader.h>
#include <core/utils/stringmanip.h>
#include <engine/io/AssimpCompatibilityLayer.h>

#include <assimp/Importer.hpp>

namespace Carrot::Render {
    LoadedScene& SceneLoader::load(const Carrot::IO::Resource& file) {
        verify(file.isFile(), "In-memory models are not supported!");
        const Carrot::IO::Path filePath { Carrot::toString(file.getFilepath().u8string()).c_str() };

        if(filePath.getExtension() == ".gltf") {
            Render::GLTFLoader loader;
            scene = std::move(loader.load(file));
        } else {
            Render::AssimpLoader loader;
            Assimp::Importer importer{};

            importer.SetIOHandler(new Carrot::IO::CarrotIOSystem(file));
            scene = std::move(loader.load(file.getName(), importer));
            scene.debugName = file.getName();
        }
        return scene;
    }
} // Carrot::Render