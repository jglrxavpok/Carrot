//
// Created by jglrxavpok on 06/07/2023.
//

#include <core/Macros.h>
#include "CollisionLayers.h"

namespace Carrot::Physics {
    CollisionLayers::CollisionLayers() {
        reset();
    }

    CollisionLayerID CollisionLayers::newLayer(const std::string_view& name, bool isStatic) {
        auto newLayer = [&](CollisionLayerID id) {
            collisionLayersStorage[id].isStatic = isStatic;
            collisionLayersStorage[id].name = name;
            collisionLayersStorage[id].layerID = id;
            collisionLayersStorage[id].isValid = true;
            return id;
        };
        if(!freeIndices.empty()) {
            CollisionLayerID id = freeIndices.back();
            freeIndices.pop_back();
            usedIndices.emplace_back(id);
        } else {
            // no free indices, need to grow storage & collision matrix
            usedIndices.emplace_back(usedIndices.size());

            collisionLayersStorage.resize(collisionLayersStorage.size()+1);
            collisionMatrix.resize(collisionLayersStorage.size());

            for (auto& row : collisionMatrix) {
                row.resize(collisionLayersStorage.size(), true); // add new layer to collision matrix, with collision enabled with everything else by default
            }

            verify(collisionMatrix.size() == collisionLayersStorage.size(), "Programming error: mismatch between size of collision matrix and layer storage");
        }
        return newLayer(usedIndices.back());
    }

    void CollisionLayers::removeLayer(CollisionLayerID id) {
        verify(std::find(usedIndices.begin(), usedIndices.end(), id) != usedIndices.end(), "CollisionLayerID does not exist");
        freeIndices.push_back(id);

        usedIndices.erase(std::remove(usedIndices.begin(), usedIndices.end(), id), usedIndices.end());

        collisionLayersStorage[id].name = "!!REMOVED!!";
        collisionLayersStorage[id].isValid = false;

        // TODO: shrink layer storage if possible
    }

    bool CollisionLayers::canCollide(CollisionLayerID me, CollisionLayerID other) const {
        assert(me < collisionLayersStorage.size());
        assert(other < collisionLayersStorage.size());
        return collisionMatrix[me][other];
    }

    void CollisionLayers::setCanCollide(CollisionLayerID me, CollisionLayerID other, bool can) {
        assert(me < collisionLayersStorage.size());
        assert(other < collisionLayersStorage.size());
        collisionMatrix[me][other] = can;
    }

    void CollisionLayers::reset() {
        collisionMatrix.clear();
        collisionLayersStorage.clear();
        usedIndices.clear();
        freeIndices.clear();

        // create default layers for convenience
        staticLayer = newLayer("Static", true);
        movingLayer = newLayer("Default moving", false);
    }

    std::vector<CollisionLayer> CollisionLayers::getLayers() const {
        std::vector<CollisionLayer> layers;
        layers.reserve(getCollisionLayersCount());
        for (const auto& id : usedIndices) {
            layers.emplace_back(collisionLayersStorage[id]);
        }
        return layers;
    }

    std::uint16_t CollisionLayers::getCollisionLayersCount() const {
        return usedIndices.size();
    }

    /**
     * Returns a collision layer based on its ID.
     */
    const CollisionLayer& CollisionLayers::getLayer(CollisionLayerID id) const {
        verify(id < getCollisionLayersCount(), "out-of-bounds CollisionLayerID");
        return collisionLayersStorage[id];
    }

    bool CollisionLayers::isValid(CollisionLayerID id) const {
        if(id < getCollisionLayersCount()) {
            return collisionLayersStorage[id].isValid;
        }
        return false;
    }

    CollisionLayerID CollisionLayers::getStaticLayer() const {
        return staticLayer;
    }

    CollisionLayerID CollisionLayers::getMovingLayer() const {
        return movingLayer;
    }

} // Carrot::Physics