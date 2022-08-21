//
// Created by jglrxavpok on 06/08/2022.
//

#include "Skeleton.h"
#include <core/Macros.h>
#include <functional>

namespace Carrot::Render {
    SkeletonTreeNode& SkeletonTreeNode::newChild() {
        return children.emplace_back();
    }

    std::list<SkeletonTreeNode>& SkeletonTreeNode::getChildren() {
        return children;
    }

    const std::list<SkeletonTreeNode>& SkeletonTreeNode::getChildren() const {
        return children;
    }

    // --

    Skeleton::Skeleton(const glm::mat4& globalInverseTransform): globalInverseTransform(globalInverseTransform) {
        invGlobalInverseTransform = glm::inverse(globalInverseTransform);
    }

    Bone* Skeleton::findBone(const BoneName& boneName) {
        std::function<Bone* (SkeletonTreeNode&)> recurse = [&](SkeletonTreeNode& node) -> Bone* {
            if(node.bone.name == boneName) {
                return &node.bone;
            }
            for(auto& child : node.getChildren()) {
                Bone* found = recurse(child);
                if(found) {
                    return found;
                }
            }
            return nullptr;
        };
        return recurse(hierarchy);
    }

    const Bone* Skeleton::findBone(const BoneName& boneName) const {
        return const_cast<Skeleton*>(this)->findBone(boneName);
    }

    const glm::mat4& Skeleton::getGlobalTransform() const {
        return invGlobalInverseTransform;
    }

    const glm::mat4& Skeleton::getGlobalInverseTransform() const {
        return globalInverseTransform;
    }

    void Skeleton::computeTransforms(std::unordered_map<std::string, glm::mat4>& transforms) const {
        std::function<void(const glm::mat4&, const SkeletonTreeNode&)> recurse = [&](const glm::mat4& parentTransform, const SkeletonTreeNode& node) {
            glm::mat4 transform = parentTransform * node.bone.transform;
            transforms[node.bone.name] = transform;

            for(const auto& child : node.getChildren()) {
                recurse(transform, child);
            }
        };

        recurse(globalInverseTransform, hierarchy);
    }
}