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
#include <reactphysics3d/reactphysics3d.h>

namespace Carrot::Physics {
    class RigidBody;

    enum class ColliderType {
        Box,
        Sphere,
        Capsule,
        TriangleMesh,
        Heightmap,
    };

    inline Lookup ColliderTypeNames = std::array {
            LookupEntry<ColliderType> { ColliderType::Box, "Box" },
            LookupEntry<ColliderType> { ColliderType::Sphere, "Sphere" },
            LookupEntry<ColliderType> { ColliderType::Capsule, "Capsule" },
            LookupEntry<ColliderType> { ColliderType::TriangleMesh, "TriangleMesh" },
            LookupEntry<ColliderType> { ColliderType::Heightmap, "Heightmap" },
    };

    class CollisionShape {
    public:
        virtual ~CollisionShape() = default;

        virtual ColliderType getType() const = 0;

        virtual std::unique_ptr<CollisionShape> duplicate() const = 0;

        virtual reactphysics3d::CollisionShape* getReactShape() const = 0;

    public:
        virtual void fillJSON(rapidjson::Value& object, rapidjson::Document::AllocatorType& allocator) const = 0;
    };

    class BoxCollisionShape: public CollisionShape {
    public:
        explicit BoxCollisionShape(const rapidjson::Value& json);
        explicit BoxCollisionShape(const glm::vec3& halfExtents);
        explicit BoxCollisionShape(const BoxCollisionShape&) = delete;

        ~BoxCollisionShape();

    public:
        virtual ColliderType getType() const override {
            return ColliderType::Box;
        }

        virtual void fillJSON(rapidjson::Value& object, rapidjson::Document::AllocatorType& allocator) const override;

        virtual std::unique_ptr<CollisionShape> duplicate() const override {
            return std::make_unique<BoxCollisionShape>(getHalfExtents());
        }

        virtual reactphysics3d::CollisionShape* getReactShape() const {
            return shape;
        }

    public:
        [[nodiscard]] glm::vec3 getHalfExtents() const;
        void setHalfExtents(const glm::vec3& halfExtents);

    private:
        BoxCollisionShape();

    private:
        reactphysics3d::BoxShape* shape = nullptr;
    };

    class SphereCollisionShape: public CollisionShape {
    public:
        explicit SphereCollisionShape(const rapidjson::Value& json);
        explicit SphereCollisionShape(float radius);
        explicit SphereCollisionShape(const SphereCollisionShape&) = delete;

        ~SphereCollisionShape();

    public:
        virtual ColliderType getType() const override {
            return ColliderType::Sphere;
        }

        virtual void fillJSON(rapidjson::Value& object, rapidjson::Document::AllocatorType& allocator) const override;

        virtual std::unique_ptr<CollisionShape> duplicate() const override {
            return std::make_unique<SphereCollisionShape>(getRadius());
        }

        virtual reactphysics3d::CollisionShape* getReactShape() const {
            return shape;
        }

    public:
        [[nodiscard]] float getRadius() const;
        void setRadius(float radius);

    private:
        SphereCollisionShape();

    private:
        reactphysics3d::SphereShape* shape = nullptr;
    };

    class CapsuleCollisionShape: public CollisionShape {
    public:
        explicit CapsuleCollisionShape(const rapidjson::Value& json);
        explicit CapsuleCollisionShape(float radius, float height);
        explicit CapsuleCollisionShape(const CapsuleCollisionShape&) = delete;

        ~CapsuleCollisionShape();

    public:
        virtual ColliderType getType() const override {
            return ColliderType::Capsule;
        }

        virtual void fillJSON(rapidjson::Value& object, rapidjson::Document::AllocatorType& allocator) const override;

        virtual std::unique_ptr<CollisionShape> duplicate() const override {
            return std::make_unique<CapsuleCollisionShape>(getRadius(), getHeight());
        }

        virtual reactphysics3d::CollisionShape* getReactShape() const {
            return shape;
        }

    public:
        [[nodiscard]] float getRadius() const;
        void setRadius(float radius);

        [[nodiscard]] float getHeight() const;
        void setHeight(float height);

    private:
        CapsuleCollisionShape();

    private:
        reactphysics3d::CapsuleShape* shape = nullptr;
    };

    class Collider {
    public:
        static std::unique_ptr<Collider> loadFromJSON(const rapidjson::Value& object);

        Collider(const Collider&) = delete;
        Collider(Collider&&) = default;
        ~Collider() = default;

    public:
        [[nodiscard]] ColliderType getType() const;
        [[nodiscard]] CollisionShape& getShape() const;

    public:
        void setLocalTransform(const Carrot::Math::Transform& transform);
        Carrot::Math::Transform getLocalTransform() const;
        rapidjson::Value toJSON(rapidjson::Document::AllocatorType& allocator) const;

        explicit Collider(std::unique_ptr<CollisionShape>&& shape, const Carrot::Math::Transform& localTransform): shape(std::move(shape)), localTransform(localTransform) {};

    private:
        void addToBody(RigidBody& body);

    private:
        std::unique_ptr<CollisionShape> shape;
        reactphysics3d::Collider* collider = nullptr;
        Carrot::Math::Transform localTransform;

        friend class RigidBody;
    };
}
