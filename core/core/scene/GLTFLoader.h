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
        LoadedScene load(const tinygltf::Model& model, const IO::VFS::Path& modelFilepath);

    private:
        struct GLTFMesh {
            // index of first primitive inside result LoadedScene
            std::size_t firstPrimitive = 0;
            std::size_t primitiveCount = 0;
        };

        void loadNodesRecursively(LoadedScene& scene, const tinygltf::Model& model, int nodeIndex, const std::span<const GLTFMesh>& meshes, SkeletonTreeNode& parentNode, glm::mat4 parentTransform);
    };
}