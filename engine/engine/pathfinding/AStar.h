//
// Created by jglrxavpok on 29/07/2023.
//

#pragma once

namespace Carrot::AI {

    /// Unidirectional (ie not equivalent to Edge{ indexB, indexA })
    struct Edge {
        // indices of vertices making this edge
        std::size_t indexA = ~0ull;
        std::size_t indexB = ~0ull;
    };

    class AStarImpl {
    public:
        std::span<const Edge> getEdges() const {
            return edges;
        }

    protected:
        std::vector<Edge> edges;

        // vertex -> edges starting with this vertex
        std::unordered_map<std::size_t, std::vector<const Edge*>> adjacency;

        std::vector<std::size_t> findPath(std::size_t pointA, std::size_t pointB,
                                          const std::function<float(std::size_t a, std::size_t b)>& distanceFunction,
                                          const std::function<float(std::size_t v)>& costEstimation);
    };

    template<typename VertexType>
    class AStar: public AStarImpl {
    public:
        explicit AStar() {};

        explicit AStar(std::vector<VertexType> _vertices, std::vector<Edge> _edges) {
            setGraph(std::move(_vertices), std::move(_edges));
        }

        void setGraph(std::vector<VertexType> _vertices, std::vector<Edge> _edges) {
            vertices = std::move(_vertices);
            edges = std::move(_edges);

            adjacency.clear();
            for(const auto& edge : edges) {
                adjacency[edge.indexA].push_back(&edge);
            }
        }

        /// Attempts a short path from pointA to pointB
        /// Can return an empty result, if there are no such path
        std::vector<std::size_t> findPath(std::size_t pointA, std::size_t pointB,
                                          const std::function<float(const VertexType& a, const VertexType& b)>& distanceFunction,
                                          const std::function<float(const VertexType& a)>& costEstimation) {
            return AStarImpl::findPath(pointA, pointB, [&](std::size_t a, std::size_t b) -> float {
                return distanceFunction(vertices[a], vertices[b]);
            }, [&](std::size_t v) -> float {
                return costEstimation(vertices[v]);
            });
        }

        std::span<const VertexType> getVertices() const {
            return vertices;
        }

    private:
        std::vector<VertexType> vertices;
    };

} // Carrot::AI
