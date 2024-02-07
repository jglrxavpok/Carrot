//
// Created by jglrxavpok on 27/10/2022.
//

#pragma once

#include <core/scene/LoadedScene.h>
#include <core/io/Resource.h>
#include <core/render/Animation.h>
#include <core/render/Skeleton.h>

struct aiMesh;
struct aiNode;
struct aiNodeAnim;
struct aiScene;

namespace Assimp {
    class Importer;
}

namespace Carrot::Render {
    class AssimpLoader {
    public:
        LoadedScene&& load(const std::string& filepath, Assimp::Importer& importer);

    private:
        void loadMesh(LoadedPrimitive& primitive, std::size_t meshIndex, const aiMesh* mesh);

        void updateKeyframeRecursively(Keyframe& keyframe, const aiNode* armature, float time,
                                       const std::unordered_map<std::string, aiNodeAnim*>& animationNodes,
                                       const glm::mat4& globalTransform,
                                       const glm::mat4& parentMatrix = glm::mat4{1.0f});

        void loadAnimations(const aiScene* scene, const aiNode* armature);

        void loadSubSkeleton(aiNode* subSkeleton, Render::SkeletonTreeNode& parent);
        void loadSkeleton(aiNode* armature);

    private:
        LoadedScene currentScene;

    };
}
