//
// Created by jglrxavpok on 06/07/2023.
//

#pragma once

#include <Jolt/Physics/PhysicsSystem.h>
#include <engine/physics/Types.h>

namespace Carrot::Physics {

    class CollisionLayers {
    public:
        /**
         * Initializes & creates default static and default moving layers for convenience
         */
        CollisionLayers();

        /**
         * Creates a new layer and returns the assigned ID
         */
        CollisionLayerID newLayer(const std::string_view& name, bool isStatic);

        /**
         * Removes the given layer from the physics engine
         */
        void removeLayer(CollisionLayerID id);

        /**
         * Checks whether a layer can collide against another
         */
        bool canCollide(CollisionLayerID me, CollisionLayerID other) const;

        /**
         * Sets whether a layer can collide against anotehr
         */
        void setCanCollide(CollisionLayerID me, CollisionLayerID other, bool can);

        /**
         * Removes all layers, except default static and default moving
         */
        void reset();

    public:
        /**
         * Creates a copy of the currently available layer list.
         * Order and index are not guaranteed
         */
        std::vector<CollisionLayer> getLayers() const;

        /**
         * How many layers currently exist?
         */
        std::uint16_t getCollisionLayersCount() const;

        /**
         * Returns a collision layer based on its ID.
         */
        const CollisionLayer& getLayer(CollisionLayerID id) const;

        /**
         * Returns true iif the given ID is a valid collision layer
         */
        bool isValid(CollisionLayerID id) const;

        CollisionLayerID getStaticLayer() const;
        CollisionLayerID getMovingLayer() const;

    private:
        CollisionLayerID staticLayer = -1;
        CollisionLayerID movingLayer = -1;
        std::vector<CollisionLayer> collisionLayersStorage; // all collision layers, with holes for removed layers
        std::vector<JPH::ObjectLayer> usedIndices;
        std::vector<JPH::ObjectLayer> freeIndices;

        std::vector<std::vector<bool>> collisionMatrix; // matrix representing if a layer can collide with another => collisionMatrix[me][other]
    };

} // Carrot::Physics
