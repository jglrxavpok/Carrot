//
// Created by jglrxavpok on 16/01/2022.
//

#include <engine/render/InstanceData.h>
#include <engine/render/GBufferDrawData.h>
#include "CollisionShapeRenderer.h"
#include "engine/render/VulkanRenderer.h"
#include "engine/Engine.h"

namespace Peeler::ECS {

    static glm::vec4 ColliderColor = glm::vec4(0.4f, 1.0f, 0.0f, 1.0f);

    CollisionShapeRenderer::CollisionShapeRenderer(Carrot::ECS::World& world): RenderSystem<Carrot::ECS::TransformComponent, Carrot::ECS::RigidBodyComponent>(world) {

    }

    void CollisionShapeRenderer::onFrame(Carrot::Render::Context renderContext) {
        for(const auto& selected : selectedIDs) {
            if(world.exists(selected)) {
                auto entity = world.wrap(selected);
                if((entity.getSignature() & getSignature()) == getSignature()) {
                    Carrot::ECS::RigidBodyComponent& rigidBodyComponent = entity.getComponent<Carrot::ECS::RigidBodyComponent>();
                    Carrot::ECS::TransformComponent& transformComponent = entity.getComponent<Carrot::ECS::TransformComponent>();

                    for(const auto& colliderPtr : rigidBodyComponent.rigidbody.getColliders()) {
                        const auto& collider = *colliderPtr;

                        const glm::vec3 worldScale = transformComponent.computeFinalScale(); // rigidbody size is separated from object scale
                        glm::mat4 transform = transformComponent.toTransformMatrix() * glm::scale(glm::mat4(1.0f), 1.0f / worldScale) * collider.getLocalTransform().toTransformMatrix();
                        switch(collider.getType()) {
                            case Carrot::Physics::ColliderType::Sphere: {
                                const auto& asSphere = static_cast<Carrot::Physics::SphereCollisionShape&>(collider.getShape());
                                renderContext.renderWireframeSphere(transform, asSphere.getRadius(), ColliderColor, selected);
                            } break;

                            case Carrot::Physics::ColliderType::Box: {
                                const auto& asBox = static_cast<Carrot::Physics::BoxCollisionShape&>(collider.getShape());
                                renderContext.renderWireframeCuboid(transform, asBox.getHalfExtents(), ColliderColor, selected);
                            } break;

                            case Carrot::Physics::ColliderType::Capsule: {
                                const auto& asCapsule = static_cast<Carrot::Physics::CapsuleCollisionShape&>(collider.getShape());
                                // Reactphysics 3D is Y-up while Carrot is Z-up
                                const glm::mat4 rp3dCorrection =
                                        glm::scale(
                                                glm::rotate(glm::identity<glm::mat4>(), glm::half_pi<float>(), glm::vec3(1, 0, 0)),
                                                glm::vec3(1, -1, 1)
                                        );
                                renderContext.renderWireframeCapsule(transform * rp3dCorrection, asCapsule.getRadius(), asCapsule.getHeight(), ColliderColor, selected);
                            } break;

                            default:
                                // TODO
                                break;
                        }


                    }
                }
            }
        }
    }

    void CollisionShapeRenderer::setSelected(const std::vector<Carrot::ECS::EntityID>& selection) {
        selectedIDs = selection;
    }

    std::unique_ptr<Carrot::ECS::System> CollisionShapeRenderer::duplicate(Carrot::ECS::World& newOwner) const {
        return std::make_unique<CollisionShapeRenderer>(newOwner);
    }
}