//
// Created by jglrxavpok on 02/01/2024.
//

#pragma once

#include <core/Allocator.h>
#include <core/containers/Vector.hpp>
#include <core/UniquePtr.hpp>

namespace Carrot {
    template<typename T>
    concept HasSpatialInfo = requires(T t)
    {
        { t.getPosition() } -> std::convertible_to<glm::vec3>;
    };

    /**
     * \brief Binary Space Partitioning tree mostly used for neighbor searches. Elements are expected to outlive this tree!
     * Also, all methods return indices to the input elements
     * \tparam TElement Element type to store inside this tree. Must match concept 'HasSpatialInfo'
     */
    template<typename TElement>
    requires HasSpatialInfo<TElement>
    class KDTree {
    public:
        /**
         * \brief Creates an empty K-d tree
         * \param allocator the allocator which will be used for this tree
         */
        explicit KDTree(Allocator& allocator);

        /**
         * \brief Constructs a K-d tree with the given elements. Equivalent to empty constructor + build
         * \param allocator the allocator which will be used for this tree
         * \param elements elements to build the K-d tree with
         */
        explicit KDTree(Allocator& allocator, std::span<const TElement> elements);

        /**
         * \brief Builds this tree to reference the given elements. Previous content of this tree are lost!
         * \param elements the elements to store in this tree
         */
        void build(std::span<const TElement> elements);

        /**
         * \brief Finds the closest neighbor of a given element
         * \param from element to get closest neighbor of. Does not have to be inside the tree
         * \param maxDistance elements further than 'maxDistance' will not be taken into account. Defaults to INFINITY
         * \return index of the closest neighbor, or -1 if there are no such neighbor
         */
        std::int64_t closestNeighbor(const TElement& from, float maxDistance = INFINITY) const;

        /**
         * \brief Finds all elements in this tree that are less than 'maxDistance' in distance to 'from'.
         * \param out vector where to store the neighbors, not cleared when filled
         * \param from element to search neighbors of
         * \param maxDistance max distance to 'from'
         */
        void getNeighbors(Vector<std::size_t>& out, const TElement& from, float maxDistance) const;

        /**
         * \brief Finds all elements in this tree that are inside the region defined by min and max.
         * \param out vector where to store the elements, not cleared when filled
         */
        void rangeSearch(Vector<std::size_t>& out, const glm::vec3& min, const glm::vec3& max) const;

    public:
        /**
         * How many elements are in this tree
         */
        std::size_t size() const;

        /**
         * Is this tree empty? (size() == 0)
         */
        bool empty() const;

    private:
        struct Node {
            std::size_t elementIndex;
            glm::vec3 medianPoint;
            glm::vec3 regionMin;
            glm::vec3 regionMax;
            UniquePtr<Node> pLeft = nullptr;
            UniquePtr<Node> pRight = nullptr;
            Node* pParent = nullptr;
        };

        void buildInner(Node* pDestination, Carrot::Allocator& tempAllocator, std::span<const TElement> allElements, const Carrot::Vector<std::size_t>& subset, std::size_t depth, const glm::vec3& regionMin, const glm::vec3& regionMax);
        Node* findClosest(const TElement& element) const;

        template<typename TFunctor>
        void rangeSearchInner(Vector<std::size_t>& out, const Node* pRoot, const glm::vec3& min, const glm::vec3& max, const TFunctor& checkResult) const;

        Allocator& allocator;
        Node root;
        std::size_t elementCount = 0;
    };
}

#include "KDTree.ipp"