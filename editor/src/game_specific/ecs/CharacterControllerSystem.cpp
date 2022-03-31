//
// Created by jglrxavpok on 24/03/2022.
//

#include "CharacterControllerSystem.h"
#include <engine/ecs/components/TransformComponent.h>

namespace Game::ECS {
    CharacterControllerSystem::CharacterControllerSystem(Carrot::ECS::World& world): Carrot::ECS::LogicSystem<Carrot::ECS::RigidBodyComponent, Game::ECS::CharacterControllerComponent>(world) {
        using namespace Carrot::IO;
        movementInput.suggestBinding(GLFWGamepadVec2Binding(0, Carrot::IO::GameInputVectorType::LeftStick));
        lookInput.suggestBinding(GLFWGamepadVec2Binding(0, Carrot::IO::GameInputVectorType::RightStick));

        movementInput.suggestBinding(GLFWKeysVec2Binding(Carrot::IO::GameInputVectorType::WASD));
        lookInput.suggestBinding(GLFWMouseDeltaBinding);

        inputSet.add(movementInput);
        inputSet.add(lookInput);
        inputSet.activate();
    }

    void CharacterControllerSystem::tick(double deltaTime) {
        forEachEntity([&](Carrot::ECS::Entity& entity, Carrot::ECS::RigidBodyComponent& rigidBodyComponent, Game::ECS::CharacterControllerComponent& characterControllerComponent) {
            auto& rigidbody = rigidBodyComponent.rigidbody;

            auto headEntity = entity.getNamedChild(characterControllerComponent.headChildName);

            if(!headEntity)
                return;

            const float yawSpeed = 1.0f;
            const float pitchSpeed = 1.0f;
            float& yaw = characterControllerComponent.yaw;
            float& pitch = characterControllerComponent.pitch;
            yaw -= deltaTime * lookInput.getValue().x * yawSpeed;
            pitch -= deltaTime * lookInput.getValue().y * pitchSpeed;

            pitch = std::min(glm::pi<float>(), std::max(0.0f, pitch));

            rigidbody.setBodyType(reactphysics3d::BodyType::DYNAMIC);
            rigidbody.setRotationAxes(glm::vec3(0, 0, 0)); // only the player is allowed to rotate the entity

            glm::vec3 forward { -glm::sin(yaw), glm::cos(yaw), 0.0 };
            glm::vec3 strafe { glm::cos(yaw), glm::sin(yaw), 0.0 };

            glm::vec3 eulerAngles{ 0.0f };
            eulerAngles.x = characterControllerComponent.pitch;
            eulerAngles.z = characterControllerComponent.yaw;

            headEntity->getComponent<Carrot::ECS::TransformComponent>()->localTransform.rotation = glm::quat(eulerAngles);

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