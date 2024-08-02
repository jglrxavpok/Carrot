//
// Created by jglrxavpok on 06/08/2022.
//

#pragma once

#include <optional>
#include <list>
#include <vector>
#include <unordered_map>
#include <string>
#include <core/data/Hashes.h>
#include <glm/glm.hpp>

namespace Carrot {
    class Model;
}

namespace Carrot::Render {
    class AssimpLoader; // forward declaration for friend class declaration
    class GLTFLoader; // forward declaration for friend class declaration

    using BoneName = std::string;

    struct Bone {
        glm::mat4 transform{1.0f}; //! Transform of this bone (relative to parent if any exists)
        glm::mat4 originalTransform{1.0f}; //! Original transform of this bone (relative to parent if any exists). Modify only if you know the consequences
        BoneName name = "NON-INITIALIZED??"; //! name used to identify this bone
    };

    // struct for type safety
    struct NodeKey {
        std::uint32_t value = 0;

        auto operator<=>(const NodeKey &) const = default;
    };

    class SkeletonTreeNode {
    public:
        Bone bone{};
        NodeKey nodeKey; //< key used for maps inside LoadedScene
        std::optional<std::vector<std::size_t>> meshIndices;
        SkeletonTreeNode* pParent = nullptr;

        explicit SkeletonTreeNode(SkeletonTreeNode* pParent): pParent(pParent) {};

    public:
        SkeletonTreeNode& operator=(const SkeletonTreeNode& other) = delete;

    public: //! Hierarchy manipulation

        //! Adds a new child to this node
        SkeletonTreeNode& newChild();

        //! Children of this node
        std::list<SkeletonTreeNode>& getChildren();

        //! Children of this node
        const std::list<SkeletonTreeNode>& getChildren() const;

    private:
        std::list<SkeletonTreeNode> children; // not a vector because newChild would invalidate previous pointers
    };

    using Pose = std::unordered_map<BoneName, glm::mat4>;

    //! Represents an armature (can be linked to a specific model, or standalone)
    //! Helps apply the transform of the entire skeleton to a mesh.
    //! If you want to read animations from a model file, use Carrot::AnimatedInstances
    class Skeleton {
    public:
        SkeletonTreeNode hierarchy { nullptr };

        explicit Skeleton(const glm::mat4& globalInverseTransform);

    public:
        // deep copy
        Skeleton& operator=(const Skeleton& other);

        //! Finds the bone which has the same name as the input. If none could be found, returns nullptr
        //! If you need to use this bone frequently, it is recommended to cache it, as this method searches the entire hierarchy each time
        Bone* findBone(const BoneName& boneName);
        SkeletonTreeNode* findNode(const BoneName& boneName);

        //! Finds the bone which has the same name as the input. If none could be found, returns nullptr
        //! If you need to use this bone frequently, it is recommended to cache it, as this method searches the entire hierarchy each time
        const Bone* findBone(const BoneName& boneName) const;
        const SkeletonTreeNode* findNode(const BoneName& boneName) const;

    public:
        const glm::mat4& getGlobalTransform() const;
        const glm::mat4& getGlobalInverseTransform() const;

    public:
        //! Fills the given keyframe with all
        void computeTransforms(std::unordered_map<std::string, glm::mat4>& transforms) const;

    private:
        glm::mat4 globalInverseTransform;
        glm::mat4 invGlobalInverseTransform;

        // this structure is meant to be loaded by Model
        friend class Carrot::Render::AssimpLoader;
        friend class Carrot::Render::GLTFLoader;
        friend class Carrot::Model;
    };
}

template<>
struct std::hash<Carrot::Render::NodeKey> {
    std::size_t operator()(const Carrot::Render::NodeKey& key) const {
        std::size_t h = 0;
        Carrot::hash_combine(h, key.value);
        return h;
    }
};