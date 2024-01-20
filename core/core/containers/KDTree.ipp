//
// Created by jglrxavpok on 02/01/2024.
//

#pragma once

#include <core/allocators/InlineAllocator.h>
#include <core/allocators/StackAllocator.h>
#include <core/containers/Vector.hpp>
#include <core/utils/RNG.h>
#include <glm/gtx/norm.hpp>

namespace Carrot {

#define KD_TREE_TEMPLATE template<typename TElement> requires HasSpatialInfo<TElement>

    KD_TREE_TEMPLATE
    KDTree<TElement>::KDTree(Allocator& allocator): allocator(allocator) {}

    KD_TREE_TEMPLATE
    KDTree<TElement>::KDTree(Allocator& allocator, std::span<const TElement> elements): KDTree(allocator) {
        build(elements);
    }

    KD_TREE_TEMPLATE
    void KDTree<TElement>::build(std::span<const TElement> elements) {
        root = {};
        elementCount = elements.size();
        Carrot::Vector<std::size_t> allPoints;
        allPoints.setCapacity(elements.size());
        for(std::size_t i = 0; i < elements.size(); i++) {
            allPoints.pushBack(i);
        }
        const glm::vec3 regionMin { -INFINITY,-INFINITY,-INFINITY };
        const glm::vec3 regionMax { +INFINITY,+INFINITY,+INFINITY };

        Carrot::StackAllocator stackAllocator { Carrot::Allocator::getDefault() };
        buildInner(&root, stackAllocator, elements, allPoints, 0, regionMin, regionMax);
    }

    KD_TREE_TEMPLATE
    std::int64_t KDTree<TElement>::closestNeighbor(const TElement& from, float maxDistance) const {
        Node* closest = findClosest(from);
        if(closest == nullptr) {
            return -1;
        }
        if(glm::distance2(from.getPosition(), closest->medianPoint) < maxDistance*maxDistance) {
            return closest->elementIndex;
        }
        return -1;
    }

    KD_TREE_TEMPLATE
    void KDTree<TElement>::getNeighbors(Vector<std::size_t>& out, const TElement& from, float maxDistance) const {
        const glm::vec3 pos = from.getPosition();
        const glm::vec3 min = pos - glm::vec3(maxDistance);
        const glm::vec3 max = pos + glm::vec3(maxDistance);
        rangeSearch(out, min, max);
        // TODO: remove all > maxDistance (range search = hyperrectangle, neighbor search = hypersphere)
    }

    KD_TREE_TEMPLATE
    void KDTree<TElement>::rangeSearch(Vector<std::size_t>& out, const glm::vec3& min, const glm::vec3& max) const {
        rangeSearchInner(out, &root, min, max);
    }

    KD_TREE_TEMPLATE
    std::size_t KDTree<TElement>::size() const {
        return this->elementCount;
    }

    KD_TREE_TEMPLATE
    bool KDTree<TElement>::empty() const {
        return size() == 0;
    }


    KD_TREE_TEMPLATE
    void KDTree<TElement>::buildInner(Node* pDestination, Carrot::Allocator& tempAllocator, std::span<const TElement> allElements, const Carrot::Vector<std::size_t>& subset, const std::size_t depth, const glm::vec3& regionMin, const glm::vec3& regionMax) {
        verify(pDestination != nullptr, "Cannot add to nullptr node");
        pDestination->regionMin = regionMin;
        pDestination->regionMax = regionMax;
        if(subset.size() == 1) {
            pDestination->elementIndex = *subset.begin();
            pDestination->medianPoint = allElements[pDestination->elementIndex].getPosition();
            return;
        }
        if(subset.empty()) {
            return;
        }

        const std::size_t axisIndex = depth % 3;

        // find median
        {
            Vector<std::size_t> points { tempAllocator };
            if(subset.size() < 512) {
                points.setCapacity(subset.size());
                for(std::size_t pointIndex : subset) {
                    points.pushBack(pointIndex);
                }
            } else {
                points.resize(512);
                for(std::size_t i = 0; i < 512; i++) {
                    std::size_t randomIndex = Carrot::RNG::randomFloat(0.0f, subset.size()-1);
                    points[i] = subset[randomIndex];
                }
            }
            points.sort([&](const std::size_t& a, const std::size_t& b) {
                const float posA = allElements[a].getPosition()[axisIndex];
                const float posB = allElements[b].getPosition()[axisIndex];
                return posA < posB;
            });

            pDestination->elementIndex = points[points.size() / 2];
            pDestination->medianPoint = allElements[pDestination->elementIndex].getPosition();
        }

        const float split = pDestination->medianPoint[axisIndex];

        Carrot::Vector<std::size_t> pointsBefore { tempAllocator };
        Carrot::Vector<std::size_t> pointsAfter { tempAllocator };
        pointsBefore.setGrowthFactor(2);
        pointsAfter.setGrowthFactor(2);

        for(std::size_t index : subset) {
            if(index == pDestination->elementIndex) {
                continue; // already in tree
            }

            const glm::vec3 position = allElements[index].getPosition();
            if(position[axisIndex] < split) {
                pointsBefore.pushBack(index);
            } else {
                pointsAfter.pushBack(index);
            }
        }

        if(!pointsBefore.empty()) {
            pDestination->pLeft = makeUnique<Node>(allocator);
            pDestination->pLeft->pParent = pDestination;
            glm::vec3 newMax = regionMax;
            newMax[axisIndex] = split;
            buildInner(pDestination->pLeft.get(), tempAllocator, allElements, pointsBefore, depth+1, regionMin, newMax);
        }
        if(!pointsAfter.empty()) {
            auto ptr = makeUnique<Node>(allocator);
            pDestination->pRight = std::move(ptr);
            pDestination->pRight->pParent = pDestination;
            glm::vec3 newMin = regionMin;
            newMin[axisIndex] = split;
            buildInner(pDestination->pRight.get(), tempAllocator, allElements, pointsAfter, depth+1, newMin, regionMax);
        }
    }

    KD_TREE_TEMPLATE
    typename KDTree<TElement>::Node* KDTree<TElement>::findClosest(const TElement& from) const {
        if(empty()) {
            return nullptr;
        }

        // 1. find leaf with the closest element
        Node* currentNode = &root;
        std::size_t depth = 0;

        const float nodePositions[3] = {
            from.getPosition()[0],
            from.getPosition()[1],
            from.getPosition()[2],
        };

        const std::size_t maxIterations = size();
        std::size_t iteration = 0;
        bool found = false;
        while(iteration++ < maxIterations) {
            const std::size_t axisIndex = depth % 3;
            depth++;

            const float split = currentNode->medianPoint[axisIndex];
            if(nodePositions[axisIndex] < split) {
                Node* nextNode = currentNode->pLeft.get();
                if(nextNode == nullptr) {
                    found = true;
                    break; // closest is 'currentNode'
                } else {
                    currentNode = nextNode;
                }
            } else {
                Node* nextNode = currentNode->pRight.get();
                if(nextNode == nullptr) {
                    found = true;
                    break; // closest is 'currentNode'
                } else {
                    currentNode = nextNode;
                }
            }
        }

        verify(found, "Potential infinite loop!");

        return currentNode;
    }

    KD_TREE_TEMPLATE
    void KDTree<TElement>::rangeSearchInner(Vector<std::size_t>& out, const Node* pRoot, const glm::vec3& min, const glm::vec3& max) const {
        const bool isLeaf = pRoot->pLeft == nullptr && pRoot->pRight == nullptr;
        if(isLeaf) {
            if(glm::all(glm::greaterThanEqual(pRoot->medianPoint, min))
                && glm::all(glm::lessThan(pRoot->medianPoint, max))) {
                out.pushBack(pRoot->elementIndex);
            }
        } else {
            auto regionFullyContained = [&](const Node& node) -> bool {
                return glm::all(glm::greaterThanEqual(node.regionMin, min))
                    && glm::all(glm::lessThan(node.regionMax, max));
            };
            auto regionIntersects = [&](const Node& node) -> bool {
                return glm::all(glm::lessThanEqual(node.regionMin, max)) && glm::all(glm::greaterThan(node.regionMax, min));
            };

            std::function<void(const Node&)> reportSubTree = [&](const Node& node) {
                out.pushBack(node.elementIndex);
                if(node.pLeft) {
                    reportSubTree(*node.pLeft);
                }
                if(node.pRight) {
                    reportSubTree(*node.pRight);
                }
            };

            if(pRoot->pLeft) {
                if(regionFullyContained(*pRoot->pLeft)) {
                    reportSubTree(*pRoot->pLeft);
                } else if(regionIntersects(*pRoot->pLeft)) {
                    rangeSearchInner(out, pRoot->pLeft, min, max);
                }
            }
            if(pRoot->pRight) {
                if(regionFullyContained(*pRoot->pRight)) {
                    reportSubTree(*pRoot->pRight);
                } else if(regionIntersects(*pRoot->pRight)) {
                    rangeSearchInner(out, pRoot->pRight, min, max);
                }
            }
        }
    }
}
