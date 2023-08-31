//
// Created by jglrxavpok on 24/03/2022.
//

#include "CharacterControllerSystem.h"
#include <engine/physics/PhysicsSystem.h>
#include <engine/utils/conversions.h>
#include <engine/ecs/components/TransformComponent.h>
#include <engine/ecs/systems/RigidBodySystem.h>
#include <core/io/Logging.hpp>
#include "PageComponent.h"
#include "engine/ecs/components/TextComponent.h"
#include "engine/Engine.h"

namespace Game::ECS {
    CharacterControllerSystem::CharacterControllerSystem(Carrot::ECS::World& world): Carrot::ECS::LogicSystem<Carrot::ECS::RigidBodyComponent, Game::ECS::CharacterControllerComponent>(world) {
        using namespace Carrot::IO;
        movementInput.suggestBinding(GLFWGamepadVec2Binding(0, Carrot::IO::GameInputVectorType::LeftStick));
        lookInput.suggestBinding(GLFWGamepadVec2Binding(0, Carrot::IO::GameInputVectorType::RightStick));
        interactInput.suggestBinding(GLFWGamepadButtonBinding(GLFW_JOYSTICK_1, GLFW_GAMEPAD_BUTTON_A));

        movementInput.suggestBinding(GLFWKeysVec2Binding(Carrot::IO::GameInputVectorType::WASD));
        ungrabCursorInput.suggestBinding(GLFWKeyBinding(GLFW_KEY_ESCAPE));
        lookInput.suggestBinding(GLFWMouseDeltaBinding);
        interactInput.suggestBinding(GLFWMouseButtonBinding(GLFW_MOUSE_BUTTON_LEFT));

        inputSet.add(movementInput);
        inputSet.add(lookInput);
        inputSet.add(interactInput);
        inputSet.add(ungrabCursorInput);
        inputSet.activate();
    }

    void CharacterControllerSystem::tick(double deltaTime) {
        forEachEntity([&](Carrot::ECS::Entity& entity, Carrot::ECS::RigidBodyComponent& rigidBodyComponent, Game::ECS::CharacterControllerComponent& characterControllerComponent) {
            if(!ungrabCursorInput.isPressed()) {
                GetEngine().grabCursor();
            } else {
                GetEngine().ungrabCursor();
            }

            auto& rigidbody = rigidBodyComponent.rigidbody;

            // Movement
            auto headEntity = entity.getNamedChild(characterControllerComponent.headChildName, Carrot::ShouldRecurse::Recursion);
            auto scoreEntity = entity.getWorld().findEntityByName(characterControllerComponent.scoreEntityName);

            if(!headEntity)
                return;

            if(!scoreEntity)
                return;

            const float dt = static_cast<float>(deltaTime);
            const float yawSpeed = 60.0f * dt;
            const float pitchSpeed = 60.0f * dt;
            float& yaw = characterControllerComponent.yaw;
            float& pitch = characterControllerComponent.pitch;

            pitch = std::min(glm::pi<float>(), std::max(0.0f, pitch));

            rigidbody.setBodyType(Carrot::Physics::BodyType::Dynamic);
            rigidbody.setRotationAxes(glm::bvec3(false, false, false)); // only the player is allowed to rotate the entity

            glm::vec3 forward { -glm::sin(yaw), glm::cos(yaw), 0.0 };
            glm::vec3 strafe { glm::cos(yaw), glm::sin(yaw), 0.0 };

            glm::vec3 eulerAngles{ 0.0f };
            eulerAngles.x = characterControllerComponent.pitch;
            eulerAngles.z = characterControllerComponent.yaw;

            Carrot::ECS::TransformComponent& headTransform = headEntity->getComponent<Carrot::ECS::TransformComponent>();
            headTransform.localTransform.rotation = glm::quat(eulerAngles);

            // updating the angles after writing to the head rotation might be weird,
            yaw -= deltaTime * lookInput.getValue().x * yawSpeed;
            pitch -= deltaTime * lookInput.getValue().y * pitchSpeed;

            forward.z = 0.0f;
            strafe.z = 0.0f;

            glm::vec3 direction = forward * -movementInput.getValue().y + strafe * movementInput.getValue().x;
            verify(direction.x == direction.x, "x");
            verify(direction.y == direction.y, "y");
            verify(direction.z == direction.z, "z");
            if(glm::length2(direction) > 0.001) {
                direction = glm::normalize(direction);
            }
            rigidbody.setVelocity(static_cast<float>(deltaTime) * direction * speedFactor);

            // Interaction
            if(interactInput.wasJustPressed()) {
                // 1. shoot ray
                // 2. find if intersection
                // 3. if intersection, find corresponding entity
                // 4. print entity name (debug, will need some specific behavior)
                glm::vec3 dir { 0, 0, -1 };
                dir = headTransform.localTransform.rotation * dir;
                const float maxDistance = 5000.0f;
                GetPhysics().raycast(headTransform.computeFinalPosition(), dir, maxDistance, [&](const Carrot::Physics::RaycastInfo& raycastInfo) -> float {
                    auto entityHit = Carrot::ECS::RigidBodySystem::entityFromBody(world, *raycastInfo.rigidBody);
                    if(entityHit) {
                        auto potentialPage = entityHit->getComponent<PageComponent>();
                        if(potentialPage) {
                            characterControllerComponent.heldPages++;
                            Carrot::ECS::TextComponent& textComponent = scoreEntity->getComponent<Carrot::ECS::TextComponent>();
                            textComponent.setText(Carrot::sprintf("Pages collected: %llu / 4", characterControllerComponent.heldPages));

                            world.removeEntity(entityHit.value());
                        }
                    }
                    return 1.0f;
                });
            }
        });
    }

    std::unique_ptr<Carrot::ECS::System> CharacterControllerSystem::duplicate(Carrot::ECS::World& newOwner) const {
        std::unique_ptr<CharacterControllerSystem> result = std::make_unique<CharacterControllerSystem>(newOwner);
        return result;
    }

    void CharacterControllerSystem::reload() {
        inputSet.activate();
    }

    void CharacterControllerSystem::unload() {
        inputSet.deactivate();
    }
}