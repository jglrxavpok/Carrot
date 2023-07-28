//
// Created by jglrxavpok on 16/01/2022.
//

#include "CharacterPositionSetterSystem.h"

namespace Peeler::ECS {

    static glm::vec4 ColliderColor = glm::vec4(0.4f, 1.0f, 0.0f, 1.0f);

    CharacterPositionSetterSystem::CharacterPositionSetterSystem(Carrot::ECS::World& world): RenderSystem<Carrot::ECS::TransformComponent, Carrot::ECS::PhysicsCharacterComponent>(world) {

    }

    void CharacterPositionSetterSystem::onFrame(Carrot::Render::Context renderContext) {
        // make sure physics debug show collisions at the proper place
        forEachEntity([&](Carrot::ECS::Entity& entity, Carrot::ECS::TransformComponent& transformComponent, Carrot::ECS::PhysicsCharacterComponent& characterComponent) {
            characterComponent.character.setWorldTransform(transformComponent.computeGlobalPhysicsTransform());
            characterComponent.character.prePhysics();
        });
    }

    std::unique_ptr<Carrot::ECS::System> CharacterPositionSetterSystem::duplicate(Carrot::ECS::World& newOwner) const {
        return std::make_unique<CharacterPositionSetterSystem>(newOwner);
    }
}