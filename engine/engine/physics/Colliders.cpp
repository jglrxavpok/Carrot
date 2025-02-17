//
// Created by jglrxavpok on 12/01/2022.
//

#include "Colliders.h"

#include <core/io/DocumentHelpers.h>
#include <engine/utils/Macros.h>
#include <engine/utils/conversions.h>
#include <core/utils/JSON.h>
#include <engine/utils/conversions.h>
#include <engine/physics/PhysicsSystem.h>
#include <engine/physics/RigidBody.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

using namespace JPH;

namespace Carrot::Physics {

    Collider::Collider(std::unique_ptr<CollisionShape>&& collisionShape, const Carrot::Math::Transform& localTransform): shape(std::move(collisionShape)) {
        shape->setCollider(this);
        setLocalTransform(localTransform);
    }

    Collider::Collider(Collider&& o) {
        *this = std::move(o);
    }

    Collider& Collider::operator=(Collider&& other) {
        if(this == &other)
            return *this;

        if(rigidbody) {
            removeFromBody(*rigidbody);
        }
        verify(rigidbody == nullptr, "Programming error: rigidbody should be null at this point");

        if(other.rigidbody) {
            rigidbody = other.rigidbody; // other.rigidbody modified by line below
            other.removeFromBody(*other.rigidbody);
        }
        shape = std::move(other.shape);
        shape->setCollider(this);
        localTransform = other.getLocalTransform();

        if(rigidbody) {
            addToBody(*rigidbody);
        }

        return *this;
    }

    std::unique_ptr<Collider> Collider::load(const Carrot::DocumentElement& object) {
        ColliderType colliderType = ColliderTypeNames[std::string{ object["type"].getAsString() }];

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
                TODO;
                break;

            default:
                TODO
                verify(false, "missing case");
                break;
        }

        Carrot::Math::Transform localTransform;
        localTransform.deserialise(object["transform"]);
        return std::make_unique<Collider>(std::move(collisionShape), localTransform);
    }

    ColliderType Collider::getType() const {
        return shape->getType();
    }

    Carrot::DocumentElement Collider::serialise() const {
        Carrot::DocumentElement data;
        data["type"] = ColliderTypeNames[getType()];
        data["transform"] = getLocalTransform().serialise();
        shape->serialiseTo(data);
        return data;
    }

    void Collider::addToBody(RigidBody& body) {
        rigidbody = &body;
    }

    void Collider::removeFromBody(RigidBody& body) {
        verify(rigidbody, "Must already be on a body");
        rigidbody = nullptr;
    }

    Carrot::Math::Transform Collider::getLocalTransform() const {
        return localTransform;
    }

    void Collider::setLocalTransform(const Carrot::Math::Transform& transform) {
        localTransform = transform;
        verify(shape->shape, "Must already have a shape");
        auto oldShapeRef = shape->shape;
        const JPH::Shape* oldShape = oldShapeRef.GetPtr();
        if(oldShapeRef->GetSubType() == JPH::EShapeSubType::RotatedTranslated) {
            oldShape = reinterpret_cast<const RotatedTranslatedShape*>(oldShapeRef.GetPtr())->GetInnerShape();
        }
        shape->shape = JPH::RotatedTranslatedShapeSettings{
            carrotToJolt(transform.position),
            carrotToJolt(transform.rotation).Normalized(),
            oldShape
        }.Create().Get();
        reattach();
    }

    [[nodiscard]] CollisionShape& Collider::getShape() const {
        return *shape;
    }

    void Collider::reattach() {
        verify(shape, "Must have a shape!");

        if(rigidbody) {
            rigidbody->recreateBodyIfNeeded();
        }
    }

    // ---- Collision shapes ----

    bool CollisionShape::raycast(const glm::vec3& startPoint, const glm::vec3& direction, float maxLength, RaycastInfo& raycastInfo) {
        TODO;
    }

    void CollisionShape::reattachCollider() {
        if(owner) {
            owner->reattach();
        }
    }

    void CollisionShape::setCollider(Collider* collider) {
        this->owner = collider;
    }

    BoxCollisionShape::BoxCollisionShape() {
        setHalfExtents({1,1,1});
    }

    BoxCollisionShape::BoxCollisionShape(const Carrot::DocumentElement& doc): BoxCollisionShape() {
        setHalfExtents(DocumentHelpers::read<3, float>(doc["half_extents"]));
    }

    BoxCollisionShape::BoxCollisionShape(const glm::vec3& halfExtents): BoxCollisionShape() {
        setHalfExtents(halfExtents);
    }

    void BoxCollisionShape::setHalfExtents(const glm::vec3& halfExtents) {
        currentHalfExtents = halfExtents;
        shape = BoxShapeSettings{carrotToJolt(halfExtents), 0.0f}.Create().Get();
        reattachCollider();
    }

    void BoxCollisionShape::changeShapeScale(const glm::vec3& scale) {
        setHalfExtents(scale/2.0f);
    }

    glm::vec3 BoxCollisionShape::getHalfExtents() const {
        return currentHalfExtents;
    }

    void BoxCollisionShape::serialiseTo(Carrot::DocumentElement& target) const {
        target["half_extents"] = DocumentHelpers::write(getHalfExtents());
    }

    BoxCollisionShape::~BoxCollisionShape() {

    }

    SphereCollisionShape::SphereCollisionShape() {
        setRadius(1.0f);
    }

    SphereCollisionShape::SphereCollisionShape(const Carrot::DocumentElement& doc): SphereCollisionShape() {
        setRadius(doc["radius"].getAsDouble());
    }

    SphereCollisionShape::SphereCollisionShape(float radius): SphereCollisionShape() {
        setRadius(radius);
    }

    void SphereCollisionShape::setRadius(float radius) {
        verify(radius > 0, "radius must be > 0");
        currentRadius = radius;
        shape = SphereShapeSettings{radius}.Create().Get();
        reattachCollider();
    }

    float SphereCollisionShape::getRadius() const {
        return currentRadius;
    }

    void SphereCollisionShape::changeShapeScale(const glm::vec3& r) {
        setRadius(glm::compMax(r));
    }

    void SphereCollisionShape::serialiseTo(Carrot::DocumentElement& target) const {
        target["radius"] = getRadius();
    }

    SphereCollisionShape::~SphereCollisionShape() {}

    CapsuleCollisionShape::CapsuleCollisionShape() {
        setRadiusAndHeight(1.0f, 1.0f);
    }

    CapsuleCollisionShape::CapsuleCollisionShape(const Carrot::DocumentElement& doc): CapsuleCollisionShape() {
        setRadiusAndHeight(doc["radius"].getAsDouble(), doc["height"].getAsDouble());
    }

    CapsuleCollisionShape::CapsuleCollisionShape(float radius, float height): CapsuleCollisionShape() {
        setRadiusAndHeight(radius, height);
    }

    void CapsuleCollisionShape::setRadius(float radius) {
        setRadiusAndHeight(radius, currentHeight);
    }

    float CapsuleCollisionShape::getRadius() const {
        return currentRadius;
    }
    
    void CapsuleCollisionShape::setHeight(float height) {
        setRadiusAndHeight(currentRadius, height);
    }

    float CapsuleCollisionShape::getHeight() const {
        return currentHeight;
    }

    void CapsuleCollisionShape::setRadiusAndHeight(float radius, float height) {
        currentRadius = radius;
        currentHeight = height;
        shape = CapsuleShapeSettings{height/2, radius}.Create().Get();
        reattachCollider();
    }

    void CapsuleCollisionShape::changeShapeScale(const glm::vec3& scale) {
        setRadiusAndHeight(glm::max(scale.x, scale.z), scale[1]);
    }

    void CapsuleCollisionShape::serialiseTo(Carrot::DocumentElement& target) const {
        target["radius"] = getRadius();
        target["height"] = getHeight();
    }

    CapsuleCollisionShape::~CapsuleCollisionShape() {
    }
}