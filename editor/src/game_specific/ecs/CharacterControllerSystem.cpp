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