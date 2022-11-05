//
// Created by jglrxavpok on 27/10/2022.
//

#pragma once

#include "LoadedScene.h"
#include "core/io/Resource.h"

#include <core/utils/CarrotTinyGLTF.h>
#include <glm/glm.hpp>


namespace Carrot::Render {
    class GLTFLoader {
    public:
        LoadedScene load(const Carrot::IO::Resource& resource);

    private:
        struct GLTFMesh {
            std::vector<LoadedPrimitive> primitives;
        };

        void loadNodesRecursively(LoadedScene& scene, tinygltf::Model& model, int nodeIndex, const std::span<const GLTFMesh>& meshes, glm::mat4 parentTransform);
    };
}