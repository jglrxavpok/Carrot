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
        static constexpr const char* const CARROT_MESHLETS_EXTENSION_NAME = "CARROT_meshlets";
        static constexpr const char* const CARROT_PRECOMPUTED_MESHLETS_BLAS_EXTENSION_NAME = "CARROT_precomputed_meshlets_blas";
        static constexpr const char* const CARROT_NODE_KEY_EXTENSION_NAME = "CARROT_node_key";

        LoadedScene load(const Carrot::IO::Resource& resource);
        LoadedScene load(const tinygltf::Model& model, const IO::VFS::Path& modelFilepath);

    private:
        struct GLTFMesh {
            // index of first primitive inside result LoadedScene
            std::size_t firstPrimitive = 0;
            std::size_t primitiveCount = 0;
        };

        using NodeMapping = std::unordered_map<SkeletonTreeNode*, int>;

        void loadAnimations(LoadedScene& result, const tinygltf::Model& model, const NodeMapping& nodeMapping);
        void loadNodesRecursively(LoadedScene& scene, const tinygltf::Model& model, int nodeIndex, const std::span<const GLTFMesh>& meshes, SkeletonTreeNode& parentNode, NodeMapping& nodeMapping, const glm::mat4& parentTransform);
    };
}