//
// Created by jglrxavpok on 16/01/2022.
//

#include <engine/render/InstanceData.h>
#include <engine/render/DrawData.h>
#include "CollisionShapeRenderer.h"
#include "engine/render/VulkanRenderer.h"
#include "engine/Engine.h"

namespace Peeler::ECS {

    static glm::vec4 ColliderColor = glm::vec4(0.4f, 1.0f, 0.0f, 1.0f);

    CollisionShapeRenderer::CollisionShapeRenderer(Carrot::ECS::World& world): RenderSystem<Carrot::ECS::TransformComponent, Carrot::ECS::RigidBodyComponent>(world) {

    }

    void CollisionShapeRenderer::onFrame(Carrot::Render::Context renderContext) {
        if(world.exists(selected)) {
            auto entity = world.wrap(selected);
            if((entity.getSignature() & getSignature()) == getSignature()) {
                Carrot::ECS::RigidBodyComponent& rigidBodyComponent = entity.getComponent<Carrot::ECS::RigidBodyComponent>();
                Carrot::ECS::TransformComponent& transformComponent = entity.getComponent<Carrot::ECS::TransformComponent>();

                for(const auto& colliderPtr : rigidBodyComponent.rigidbody.getColliders()) {
                    const auto& collider = *colliderPtr;

                    glm::vec4 finalPositionH = transformComponent.toTransformMatrix() * collider.getLocalTransform().toTransformMatrix() * glm::vec4 { 0, 0, 0, 1 };
                    glm::vec3 position = glm::vec3 { finalPositionH.x, finalPositionH.y, finalPositionH.z };
                    switch(collider.getType()) {
                        case Carrot::Physics::ColliderType::Sphere: {
                            const auto& asSphere = static_cast<Carrot::Physics::SphereCollisionShape&>(collider.getShape());
                            renderContext.renderWireframeSphere(position, asSphere.getRadius(), ColliderColor, selected);
                        } break;

                        case Carrot::Physics::ColliderType::Box: {
                            const auto& asBox = static_cast<Carrot::Physics::BoxCollisionShape&>(collider.getShape());
                            renderContext.renderWireframeCuboid(position, asBox.getHalfExtents(), ColliderColor, selected);
                        } break;

                        default:
                            // TODO
                            break;
                    }


                }
            }
        }
    }

    void CollisionShapeRenderer::setSelected(const std::optional<Carrot::ECS::EntityID>& id) {
        if(id.has_value()) {
            selected = id.value();
        } else {
            selected = Carrot::UUID::null();
        }
    }

    std::unique_ptr<Carrot::ECS::System> CollisionShapeRenderer::duplicate(Carrot::ECS::World& newOwner) const {
        return std::make_unique<CollisionShapeRenderer>(newOwner);
    }
}