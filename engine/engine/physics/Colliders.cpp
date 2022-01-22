//
// Created by jglrxavpok on 12/01/2022.
//

#include "Colliders.h"
#include <engine/utils/Macros.h>
#include <core/utils/JSON.h>
#include <engine/utils/conversions.h>
#include <engine/physics/PhysicsSystem.h>
#include <engine/physics/RigidBody.h>

namespace Carrot::Physics {

    Collider::Collider(std::unique_ptr<CollisionShape>&& collisionShape, const Carrot::Math::Transform& localTransform): shape(std::move(collisionShape)), localTransform(localTransform) {
        shape->setCollider(this);
    }

    std::unique_ptr<Collider> Collider::loadFromJSON(const rapidjson::Value& object) {
        ColliderType colliderType = ColliderTypeNames[object["type"].GetString()];

        std::unique_ptr<CollisionShape> collisionShape;
        switch(colliderType) {
            case ColliderType::Box:
                collisionShape = std::make_unique<BoxCollisionShape>(object);
                break;

            case ColliderType::Sphere:
                collisionShape = std::make_unique<SphereCollisionShape>(object);
                break;

            case ColliderType::Capsule:
                collisionShape = std::make_unique<CapsuleCollisionShape>(object);
                break;

            case ColliderType::StaticConcaveTriangleMesh:
                collisionShape = std::make_unique<StaticConcaveMeshCollisionShape>(object);
                break;

            default:
                TODO
                verify(false, "missing case");
                break;
        }

        Carrot::Math::Transform localTransform;
        localTransform.loadJSON(object["transform"]);
        return std::make_unique<Collider>(std::move(collisionShape), localTransform);
    }

    ColliderType Collider::getType() const {
        return shape->getType();
    }

    rapidjson::Value Collider::toJSON(rapidjson::Document::AllocatorType& allocator) const {
        rapidjson::Value data{ rapidjson::kObjectType };
        data.AddMember("type", rapidjson::Value(ColliderTypeNames[getType()], allocator), allocator);
        data.AddMember("transform", getLocalTransform().toJSON(allocator), allocator);
        shape->fillJSON(data, allocator);
        return data;
    }

    void Collider::addToBody(RigidBody& body) {
        rigidbody = &body;
        collider = body.body->addCollider(shape->getReactShape(), localTransform);
    }

    void Collider::removeFromBody(RigidBody& body) {
        verify(collider, "Must already be on a body");
        body.body->removeCollider(collider);
        rigidbody = nullptr;
    }

    Carrot::Math::Transform Collider::getLocalTransform() const {
        return collider ? Carrot::Math::Transform(collider->getLocalToBodyTransform()) : localTransform;
    }

    void Collider::setLocalTransform(const Carrot::Math::Transform& transform) {
        if(collider) {
            collider->setLocalToBodyTransform(transform);
        } else {
            localTransform = transform;
        }
    }

    [[nodiscard]] CollisionShape& Collider::getShape() const {
        return *shape;
    }

    void Collider::reattach() {
        verify(collider, "Must already be attached!");
        verify(rigidbody, "Must be associated to a RigidBody");

        // modifies the reactphysics rigidbody, without modifying the engine representation
        RigidBody* body = rigidbody;
        removeFromBody(*body);
        addToBody(*body);
    }

    // ---- Collision shapes ----

    void CollisionShape::reattachCollider() {
        verify(owner, "Must be linked to a Collider!");
        owner->reattach();
    }

    void CollisionShape::setCollider(Collider* collider) {
        this->owner = collider;
    }

    BoxCollisionShape::BoxCollisionShape() {
        shape = GetPhysics().getCommons().createBoxShape(reactphysics3d::Vector3 { 0.5f, 0.5f, 0.5f });
    }

    BoxCollisionShape::BoxCollisionShape(const rapidjson::Value& json): BoxCollisionShape() {
        setHalfExtents(JSON::read<3, float>(json["half_extents"]));
    }

    BoxCollisionShape::BoxCollisionShape(const glm::vec3& halfExtents): BoxCollisionShape() {
        setHalfExtents(halfExtents);
    }

    void BoxCollisionShape::setHalfExtents(const glm::vec3& halfExtents) {
        shape->setHalfExtents(Carrot::reactPhysicsVecFromGlm(halfExtents));
    }

    glm::vec3 BoxCollisionShape::getHalfExtents() const {
        return Carrot::glmVecFromReactPhysics(shape->getHalfExtents());
    }

    void BoxCollisionShape::fillJSON(rapidjson::Value& object, rapidjson::Document::AllocatorType& allocator) const {
        object.AddMember("half_extents", JSON::write(getHalfExtents(), allocator), allocator);
    }

    BoxCollisionShape::~BoxCollisionShape() {
        GetPhysics().getCommons().destroyBoxShape(shape);
    }

    SphereCollisionShape::SphereCollisionShape() {
        shape = GetPhysics().getCommons().createSphereShape(1.0f);
    }

    SphereCollisionShape::SphereCollisionShape(const rapidjson::Value& json): SphereCollisionShape() {
        setRadius(json["radius"].GetFloat());
    }

    SphereCollisionShape::SphereCollisionShape(float radius): SphereCollisionShape() {
        setRadius(radius);
    }

    void SphereCollisionShape::setRadius(float radius) {
        verify(radius > 0, "radius must be > 0");
        shape->setRadius(radius);
    }

    float SphereCollisionShape::getRadius() const {
        return shape->getRadius();
    }

    void SphereCollisionShape::fillJSON(rapidjson::Value& object, rapidjson::Document::AllocatorType& allocator) const {
        object.AddMember("radius", getRadius(), allocator);
    }

    SphereCollisionShape::~SphereCollisionShape() {
        GetPhysics().getCommons().destroySphereShape(shape);
    }

    CapsuleCollisionShape::CapsuleCollisionShape() {
        shape = GetPhysics().getCommons().createCapsuleShape(1.0f, 1.0f);
    }

    CapsuleCollisionShape::CapsuleCollisionShape(const rapidjson::Value& json): CapsuleCollisionShape() {
        setRadius(json["radius"].GetFloat());
        setHeight(json["height"].GetFloat());
    }

    CapsuleCollisionShape::CapsuleCollisionShape(float radius, float height): CapsuleCollisionShape() {
        setRadius(radius);
        setHeight(height);
    }

    void CapsuleCollisionShape::setRadius(float radius) {
        verify(radius > 0, "radius must be > 0");
        shape->setRadius(radius);
    }

    float CapsuleCollisionShape::getRadius() const {
        return shape->getRadius();
    }
    
    void CapsuleCollisionShape::setHeight(float height) {
        verify(height > 0, "height must be > 0");
        shape->setHeight(height);
    }

    float CapsuleCollisionShape::getHeight() const {
        return shape->getHeight();
    }

    void CapsuleCollisionShape::fillJSON(rapidjson::Value& object, rapidjson::Document::AllocatorType& allocator) const {
        object.AddMember("radius", getRadius(), allocator);
        object.AddMember("height", getHeight(), allocator);
    }

    CapsuleCollisionShape::~CapsuleCollisionShape() {
        GetPhysics().getCommons().destroyCapsuleShape(shape);
    }
}