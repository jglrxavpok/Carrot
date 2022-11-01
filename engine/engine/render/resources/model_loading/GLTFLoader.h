//
// Created by jglrxavpok on 27/10/2022.
//

#pragma once

#include "LoadedScene.h"
#include "core/io/Resource.h"

#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_USE_RAPIDJSON
#define TINYGLTF_USE_CPP14
#define TINYGLTF_NO_INCLUDE_RAPIDJSON
#define TINYGLTF_USE_RAPIDJSON_CRTALLOCATOR
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include <tiny_gltf.h>

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