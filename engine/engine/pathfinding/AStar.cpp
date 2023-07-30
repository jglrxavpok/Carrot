//
// Created by jglrxavpok on 29/07/2023.
//

#include "AStar.h"
#include <core/Macros.h>
#include <unordered_set>

namespace Carrot::AI {
    static std::vector<std::size_t> reconstructPath(const std::unordered_map<std::size_t, std::size_t>& cameFrom, std::size_t current) {
        std::vector<std::size_t> totalPath;
        totalPath.push_back(current);

        while(true) {
            auto it = cameFrom.find(current);
            if(it == cameFrom.end()) {
                break; // reached the end (which is actually the start of our path)
            }

            current = it->second;
            totalPath.push_back(current);
        }

        std::reverse(totalPath.begin(), totalPath.end());

        return totalPath;
    }

    // TODO: separate h & d functions from costEstimation
    std::vector<std::size_t> AStarImpl::findPath(std::size_t pointA, std::size_t pointB,
                                                 const std::function<float(std::size_t a, std::size_t b)>& distanceFunction,
                                                 const std::function<float(std::size_t v)>& costEstimation) {
        std::vector<std::size_t> openSet;
        openSet.push_back(pointA);

        std::unordered_map<std::size_t, std::size_t> cameFrom;

        std::unordered_map<std::size_t, float> gScore;
        std::unordered_map<std::size_t, float> fScore;

        gScore[pointA] = 0.0f;
        fScore[pointA] = costEstimation(pointA);

        auto getGScore = [&](std::size_t node) {
            auto it = gScore.find(node);
            if(it != gScore.end()) {
                return it->second;
            }
            return INFINITY;
        };

        auto getFScore = [&](std::size_t node) {
            auto it = fScore.find(node);
            if(it != fScore.end()) {
                return it->second;
            }
            return INFINITY;
        };

        while(!openSet.empty()) {
            // find node in openSet with lowest fscore
            std::size_t current = openSet[0];
            std::size_t currentIndex = 0;
            float lowestFScore = getFScore(current);

            for (std::size_t i = 1; i < openSet.size(); ++i) {
                float otherFScore = getFScore(openSet[i]);
                if(otherFScore < lowestFScore) {
                    lowestFScore = otherFScore;
                    current = openSet[i];
                    currentIndex = i;
                }
            }

            if(current == pointB) {
                return reconstructPath(cameFrom, current);
            }

            openSet.erase(openSet.begin() + currentIndex);
            for(const auto& edge : adjacency[current]) {
                std::size_t neighbor = edge->indexB;

                float tentativeGScore = getGScore(current) + distanceFunction(current, neighbor);
                if(tentativeGScore < getGScore(neighbor)) {
                    cameFrom[neighbor] = current;
                    gScore[neighbor] = tentativeGScore;
                    fScore[neighbor] = tentativeGScore + costEstimation(neighbor);

                    if(std::find(WHOLE_CONTAINER(openSet), neighbor) == openSet.end()) {
                        openSet.push_back(neighbor);
                    }
                }
            }
        }

        return {};
    }

} // Carrot::AI