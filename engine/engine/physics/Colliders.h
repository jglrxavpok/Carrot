//
// Created by jglrxavpok on 12/01/2022.
//

#pragma once

#include <array>
#include <memory>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <engine/math/Transform.h>
#include <core/utils/Lookup.hpp>
#include <glm/glm.hpp>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <engine/physics/Types.h>

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <core/Macros.h>

namespace Carrot {
    class SingleMesh;
    class Mesh;
    class Model;
}

namespace Carrot::Physics {
    class RigidBody;
    class Collider;
    class CollisionShape;

    enum class ColliderType {
        Box,
        Sphere,
        Capsule,
        StaticConcaveTriangleMesh,
        DynamicConvexTriangleMesh,
        Heightmap,
    };

    inline Lookup ColliderTypeNames = std::array {
            LookupEntry<ColliderType> { ColliderType::Box, "Box" },
            LookupEntry<ColliderType> { ColliderType::Sphere, "Sphere" },
            LookupEntry<ColliderType> { ColliderType::Capsule, "Capsule" },
            LookupEntry<ColliderType> { ColliderType::StaticConcaveTriangleMesh, "StaticConcaveTriangleMesh" },
            LookupEntry<ColliderType> { ColliderType::DynamicConvexTriangleMesh, "DynamicConvexTriangleMesh" },
            LookupEntry<ColliderType> { ColliderType::Heightmap, "Heightmap" },
    };

    class CollisionShape {
    public:
        virtual ~CollisionShape() = default;

        virtual ColliderType getType() const = 0;

        virtual std::unique_ptr<CollisionShape> duplicate() const = 0;

        virtual void tick(Collider& collider, double dt) {};

        virtual void changeShapeScale(const glm::vec3& scale) { TODO("Not supported for this shape"); };

    public:
        /**
         * Performs a raycast against this collider (NOT *using* this collider!)
         */
        bool raycast(const glm::vec3& startPoint, const glm::vec3& direction, float maxLength, RaycastInfo& raycastInfo);

    public:
        virtual void fillJSON(rapidjson::Value& object, rapidjson::Document::AllocatorType& allocator) const = 0;

    protected:
        /// Used to tell the owning collider to remove & re-add itself to a rigidbody.
        /// Used when the underlying shape cannot be modified directly.
        void reattachCollider();

        JPH::ShapeRefC shape;

    private:
        void setCollider(Collider* collider);

    private:
        // Collider which uses this shape. Non-owning pointer
        Collider* owner = nullptr;

        friend class Character;
        friend class Collider;
        friend class RigidBody;
    };

    class BoxCollisionShape: public CollisionShape {
    public:
        explicit BoxCollisionShape(const rapidjson::Value& json);
        explicit BoxCollisionShape(const glm::vec3& halfExtents);
        explicit BoxCollisionShape(const BoxCollisionShape&) = delete;

        ~BoxCollisionShape() override;

    public:
        virtual ColliderType getType() const override {
            return ColliderType::Box;
        }

        virtual void fillJSON(rapidjson::Value& object, rapidjson::Document::AllocatorType& allocator) const override;

        virtual std::unique_ptr<CollisionShape> duplicate() const override {
            return std::make_unique<BoxCollisionShape>(getHalfExtents());
        }

    public:
        [[nodiscard]] glm::vec3 getHalfExtents() const;
        void setHalfExtents(const glm::vec3& halfExtents);

        virtual void changeShapeScale(const glm::vec3& scale) override;

    private:
        BoxCollisionShape();

    private:
        glm::vec3 currentHalfExtents;
    };

    class SphereCollisionShape: public CollisionShape {
    public:
        explicit SphereCollisionShape(const rapidjson::Value& json);
        explicit SphereCollisionShape(float radius);
        explicit SphereCollisionShape(const SphereCollisionShape&) = delete;

        ~SphereCollisionShape() override;

    public:
        virtual ColliderType getType() const override {
            return ColliderType::Sphere;
        }

        virtual void fillJSON(rapidjson::Value& object, rapidjson::Document::AllocatorType& allocator) const override;

        virtual std::unique_ptr<CollisionShape> duplicate() const override {
            return std::make_unique<SphereCollisionShape>(getRadius());
        }

    public:
        [[nodiscard]] float getRadius() const;
        void setRadius(float radius);

        /// Sets radius as maximum component of 'scale'
        virtual void changeShapeScale(const glm::vec3& scale) override;

    private:
        SphereCollisionShape();

    private:
        float currentRadius{0.0f};
    };

    class CapsuleCollisionShape: public CollisionShape {
    public:
        explicit CapsuleCollisionShape(const rapidjson::Value& json);
        explicit CapsuleCollisionShape(float radius, float height);
        explicit CapsuleCollisionShape(const CapsuleCollisionShape&) = delete;

        ~CapsuleCollisionShape() override;

    public:
        virtual ColliderType getType() const override {
            return ColliderType::Capsule;
        }

        virtual void fillJSON(rapidjson::Value& object, rapidjson::Document::AllocatorType& allocator) const override;

        virtual std::unique_ptr<CollisionShape> duplicate() const override {
            return std::make_unique<CapsuleCollisionShape>(getRadius(), getHeight());
        }

    public:
        [[nodiscard]] float getRadius() const;
        void setRadius(float radius);

        [[nodiscard]] float getHeight() const;
        void setHeight(float height);

        void setRadiusAndHeight(float radius, float height);

        /// Sets radius as maximum component of 'scale'
        virtual void changeShapeScale(const glm::vec3& scale) override;

    private:
        CapsuleCollisionShape();

    private:
        float currentRadius = 0.0f;
        float currentHeight = 0.0f;
    };

    class Collider {
    public:
        static std::unique_ptr<Collider> loadFromJSON(const rapidjson::Value& object);

        explicit Collider(std::unique_ptr<CollisionShape>&& shape, const Carrot::Math::Transform& localTransform);
        Collider(const Collider&) = delete;
        Collider(Collider&&);
        ~Collider() = default;

        Collider& operator=(Collider&&);

    public:
        [[nodiscard]] ColliderType getType() const;
        [[nodiscard]] CollisionShape& getShape() const;

    public:
        void setLocalTransform(const Carrot::Math::Transform& transform);
        Carrot::Math::Transform getLocalTransform() const;
        rapidjson::Value toJSON(rapidjson::Document::AllocatorType& allocator) const;

    private:
        void addToBody(RigidBody& body);
        void removeFromBody(RigidBody& body);
        void reattach();

    private:
        std::unique_ptr<CollisionShape> shape;
        Carrot::Math::Transform localTransform;
        RigidBody* rigidbody = nullptr;

        friend class RigidBody;
        friend class CollisionShape;
    };
}
