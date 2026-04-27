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
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

#include <stb_image.h>
#include <engine/render/resources/model_loading/SceneLoader.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>

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

            case ColliderType::Heightmap:
                collisionShape = std::make_unique<HeightmapCollisionShape>(object);
                break;

            case ColliderType::StaticConcaveTriangleMesh:
                collisionShape = std::make_unique<StaticConcaveTriangleMeshShape>(object);
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

    RigidBody* Collider::getRigidBody() const {
        return rigidbody;
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

    bool Collider::containsJoltShape(const JPH::Shape* pJoltShape) const {
        const JPH::Shape* leafShape = shape->shape.GetPtr();
        while (leafShape->GetSubType() == JPH::EShapeSubType::RotatedTranslated) {
            leafShape = static_cast<const JPH::RotatedTranslatedShape*>(leafShape)->GetInnerShape();
        }

        return leafShape == pJoltShape;
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

    HeightmapCollisionShape::HeightmapCollisionShape() {
        samples.resize(16);
        samples.fill(0.0f);
    }

    HeightmapCollisionShape::HeightmapCollisionShape(const Carrot::DocumentElement& element) {
        sourceMap = element["path"].getAsString();
        if (element.contains("scale")) {
            scale = DocumentHelpers::read<3, float>(element["scale"]);
        }
        recreateShape();
        reattachCollider();
    }

    HeightmapCollisionShape::HeightmapCollisionShape(const Carrot::IO::VFS::Path& sourceMapPath, const glm::vec3& scale): samples(Carrot::Allocator::getDefault()), scale(scale) {}

    HeightmapCollisionShape::HeightmapCollisionShape(const Carrot::IO::VFS::Path& sourceMapPath, const glm::vec3& scale, const Carrot::Vector<float>& samples) {
        sourceMap = sourceMapPath;
        this->scale = scale;
        this->samples = samples;
        recreateShape();
        reattachCollider();
    }

    HeightmapCollisionShape::~HeightmapCollisionShape() {

    }

    void HeightmapCollisionShape::recreateShape() {
        if (samples.empty() && !sourceMap.isEmpty()) {
            // TODO: share with Image.cpp?
            int width;
            int height;
            int channels;
            Carrot::IO::Resource resource { sourceMap };
            auto buffer = resource.readAll();
            stbi_us* pixels = stbi_load_16_from_memory(buffer.get(), resource.getSize(), &width, &height, &channels, STBI_grey);
            if(!pixels) {
                throw std::runtime_error("Failed to load image "+resource.getName());
            }
            verify(channels == 1, "Expected grey image");

            samples.resize(width * height);
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    samples[y * width + x] = (float)(pixels[y * width + x] / (double)0xFFFF);
                }
            }

            stbi_image_free(pixels);
        }
        shape = HeightFieldShapeSettings{ samples.data(), JPH::Vec3::sZero(), carrotToJolt(scale), (u32)sqrt(samples.size()) }.Create().Get();
    }

    ColliderType HeightmapCollisionShape::getType() const {
        return ColliderType::Heightmap;
    }

    std::unique_ptr<CollisionShape> HeightmapCollisionShape::duplicate() const {
        std::unique_ptr<HeightmapCollisionShape> cloned = std::make_unique<HeightmapCollisionShape>(sourceMap, scale, samples);
        return cloned;
    }

    void HeightmapCollisionShape::serialiseTo(Carrot::DocumentElement& target) const {
        target["path"] = sourceMap.toString();
        target["scale"] = DocumentHelpers::write(scale);
    }

    void HeightmapCollisionShape::changeShapeScale(const glm::vec3& scale) {
        this->scale = scale;
        recreateShape();
        reattachCollider();
    }

    const Carrot::IO::VFS::Path& HeightmapCollisionShape::getSourcePath() const {
        return sourceMap;
    }

    const glm::vec3& HeightmapCollisionShape::getScale() const {
        return scale;
    }

    void HeightmapCollisionShape::setSourcePath(const Carrot::IO::VFS::Path& sourcePath) {
        samples.clear();
        sourceMap = sourcePath;
        recreateShape();
        reattachCollider();
    }

    StaticConcaveTriangleMeshShape::StaticConcaveTriangleMeshShape(): StaticConcaveTriangleMeshShape({}, glm::vec3(1.0f)) {}

    StaticConcaveTriangleMeshShape::StaticConcaveTriangleMeshShape(const Carrot::DocumentElement& element) {
        this->path = element["path"].getAsString();
        this->scale = DocumentHelpers::read<3, float>(element["scale"]);
        recreateShape();
        reattachCollider();
    }

    StaticConcaveTriangleMeshShape::StaticConcaveTriangleMeshShape(const Carrot::IO::VFS::Path& sourceModelPath, const glm::vec3& scale) {
        this->path = sourceModelPath;
        this->scale = scale;
        recreateShape();
        reattachCollider();
    }

    StaticConcaveTriangleMeshShape::~StaticConcaveTriangleMeshShape() {
    }

    void StaticConcaveTriangleMeshShape::recreateShape() {
        shape = nullptr;
        VertexList vertices;
        IndexedTriangleList triangles;
        if (path.isEmpty()) {
            vertices.resize(3);
            triangles.resize(1);
            vertices[0] = {0,0,0};
            vertices[1] = {1,0,0};
            vertices[2] = {0,1,0};
            triangles[0].mIdx[0] = 0;
            triangles[0].mIdx[1] = 1;
            triangles[0].mIdx[2] = 2;
            shape = MeshShapeSettings{vertices, triangles}.Create().Get();
            return;
        }
        Render::SceneLoader loader;
        const Render::LoadedScene& scene = loader.load(path);

        std::function<void(const Render::SkeletonTreeNode&, glm::mat4)> iterateOverHierarchy = [&](const Render::SkeletonTreeNode& node, glm::mat4 parentTransform) {
            const glm::mat4 transform = parentTransform * node.bone.originalTransform;
            if (node.meshIndices.has_value()) {
                for (const std::size_t& meshIndex : *node.meshIndices) {
                    const Render::LoadedPrimitive& primitive = scene.primitives[meshIndex];
                    if (primitive.isSkinned) {
                        continue;
                    }

                    const u32 vertexOffset = vertices.size();
                    const u32 triangleOffset = triangles.size();
                    vertices.resize(vertexOffset + primitive.vertices.size());
                    triangles.resize(triangleOffset + primitive.indices.size()/3);

                    for (u32 vertexIndex = 0; vertexIndex < primitive.vertices.size(); vertexIndex++) {
                        Float3& pos = vertices[vertexOffset + vertexIndex];
                        glm::vec4 transformedPos = transform * primitive.vertices[vertexIndex].pos;
                        pos.x = transformedPos.x;
                        pos.y = transformedPos.y;
                        pos.z = transformedPos.z;
                    }
                    for (u32 triangleIndex = 0; triangleIndex < primitive.indices.size() / 3; triangleIndex++) {
                        IndexedTriangle& triangle = triangles[triangleIndex + triangleOffset];
                        triangle.mIdx[0] = vertexOffset + primitive.indices[triangleIndex * 3 + 0];
                        triangle.mIdx[1] = vertexOffset + primitive.indices[triangleIndex * 3 + 1];
                        triangle.mIdx[2] = vertexOffset + primitive.indices[triangleIndex * 3 + 2];
                    }
                }
            }

            for (const Render::SkeletonTreeNode& child : node.getChildren()) {
                iterateOverHierarchy(child, transform);
            }
        };
        iterateOverHierarchy(scene.nodeHierarchy->hierarchy, glm::scale(glm::mat4(1.0f), scale));
        shape = MeshShapeSettings{vertices, triangles}.Create().Get();
    }

    std::unique_ptr<CollisionShape> StaticConcaveTriangleMeshShape::duplicate() const {
        return std::make_unique<StaticConcaveTriangleMeshShape>(path, scale);
    }

    ColliderType StaticConcaveTriangleMeshShape::getType() const {
        return ColliderType::StaticConcaveTriangleMesh;
    }

    void StaticConcaveTriangleMeshShape::serialiseTo(Carrot::DocumentElement& target) const {
        target["path"] = path.toString();
        target["scale"] = DocumentHelpers::write(scale);
    }

    const glm::vec3& StaticConcaveTriangleMeshShape::getScale() const {
        return scale;
    }

    void StaticConcaveTriangleMeshShape::changeShapeScale(const glm::vec3& scale) {
        this->scale = scale;
        recreateShape();
        reattachCollider();
    }

    const Carrot::IO::VFS::Path& StaticConcaveTriangleMeshShape::getSourcePath() const {
        return path;
    }

    void StaticConcaveTriangleMeshShape::updateSourcePath(const Carrot::IO::VFS::Path& path) {
        this->path = path;
        recreateShape();
        reattachCollider();
    }

}