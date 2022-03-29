//
// Created by jglrxavpok on 24/03/2022.
//

#include "CharacterControllerSystem.h"

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

            rigidbody.setBodyType(reactphysics3d::BodyType::DYNAMIC);
            rigidbody.setRotationAxes(glm::vec3(0, 0, 1)); // only rotate around UP vector

            glm::vec3 forward = rigidbody.getTransform().rotation * glm::vec3{0, 1, 0};
            glm::vec3 strafe = rigidbody.getTransform().rotation * glm::vec3{1, 0, 0};

            forward.z = 0.0f;
            strafe.z = 0.0f;

            glm::vec3 direction = forward * -movementInput.getValue().y + strafe * movementInput.getValue().x;
            verify(direction.x == direction.x, "x");
            verify(direction.y == direction.y, "y");
            verify(direction.z == direction.z, "z");
            if(glm::length2(direction) > 0.001) {
                direction = glm::normalize(direction);
            }
            rigidbody.setVelocity(direction * speedFactor);
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