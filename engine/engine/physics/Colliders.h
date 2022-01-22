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

namespace Carrot {
    class Mesh;
    class Model;
}

namespace Carrot::Physics {
    class RigidBody;
    class Collider;

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

        virtual reactphysics3d::CollisionShape* getReactShape() const = 0;

        virtual void tick(Collider& collider, double dt) {};

    public:
        virtual void fillJSON(rapidjson::Value& object, rapidjson::Document::AllocatorType& allocator) const = 0;

    protected:
        /// Used to tell the owning collider to remove & re-add itself to a rigidbody.
        /// Used when the ReactPhysics3D CollisionShape cannot be modified directly.
        void reattachCollider();

    private:
        void setCollider(Collider* collider);

    private:
        // Collider which uses this shape. Non-owning pointer
        Collider* owner = nullptr;

        friend class Collider;
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

        ~SphereCollisionShape() override;

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

        ~CapsuleCollisionShape() override;

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

    /// Concave mesh. Can only be used for static bodies.
    class StaticConcaveMeshCollisionShape: public CollisionShape {
    public:
        explicit StaticConcaveMeshCollisionShape(const rapidjson::Value& json);
        explicit StaticConcaveMeshCollisionShape(const std::shared_ptr<Carrot::Model>& model);
        explicit StaticConcaveMeshCollisionShape(const StaticConcaveMeshCollisionShape&) = delete;

        ~StaticConcaveMeshCollisionShape();

    public:
        virtual ColliderType getType() const override {
            return ColliderType::StaticConcaveTriangleMesh;
        }

        virtual void fillJSON(rapidjson::Value& object, rapidjson::Document::AllocatorType& allocator) const override;

        virtual std::unique_ptr<CollisionShape> duplicate() const override {
            return std::make_unique<StaticConcaveMeshCollisionShape>(gpuModel);
        }

        virtual reactphysics3d::CollisionShape* getReactShape() const {
            return shape;
        }

    public:
        Carrot::Model& getModel() const;

        /// Changes the model used by this collision shape.
        void setModel(const std::shared_ptr<Carrot::Model>& mesh);
        void setScale(const glm::vec3& scaling);

    private:
        StaticConcaveMeshCollisionShape();

    private:
        std::shared_ptr<Carrot::Model> gpuModel;

        struct BasicCPUMesh {
            std::vector<glm::vec3> positions;
            std::vector<glm::vec3> normals;
            std::vector<std::uint32_t> indices;

            std::unique_ptr<reactphysics3d::TriangleVertexArray> vertexArray = nullptr;
            std::unique_ptr<reactphysics3d::TriangleVertexArray>& makeReactphysicsVertexArray();
        };

        std::vector<BasicCPUMesh> meshes;
        reactphysics3d::TriangleMesh* triangleMesh = nullptr;
        reactphysics3d::ConcaveMeshShape* shape = nullptr;
    };

    class Collider {
    public:
        static std::unique_ptr<Collider> loadFromJSON(const rapidjson::Value& object);

        explicit Collider(std::unique_ptr<CollisionShape>&& shape, const Carrot::Math::Transform& localTransform);
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

    private:
        void addToBody(RigidBody& body);
        void removeFromBody(RigidBody& body);
        void reattach();

    private:
        std::unique_ptr<CollisionShape> shape;
        reactphysics3d::Collider* collider = nullptr;
        Carrot::Math::Transform localTransform;
        RigidBody* rigidbody = nullptr;

        friend class RigidBody;
        friend class CollisionShape;
    };
}
