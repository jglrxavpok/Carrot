//
// Created by jglrxavpok on 01/05/2024.
//

#include "ErrorComponent.h"

namespace Carrot::ECS {
    ErrorComponent::ErrorComponent(Carrot::ECS::Entity entity): IdentifiableComponent<ErrorComponent>(std::move(entity)) {}
    ErrorComponent::ErrorComponent(const Carrot::DocumentElement& doc, Carrot::ECS::Entity entity): ErrorComponent(std::move(entity)) {}

    const char* const ErrorComponent::getName() const {
        return Carrot::Identifiable<ErrorComponent>::getStringRepresentation();
    }

    std::unique_ptr<Component> ErrorComponent::duplicate(const Entity &newOwner) const {
        auto copy = std::make_unique<ErrorComponent>(newOwner);
        return copy;
    }

    Carrot::DocumentElement ErrorComponent::serialise() const {
        verify(false, "Error component should never be serialized!");
        return Carrot::DocumentElement{};
    }

    bool ErrorComponent::isSerializable() const {
        return false;
    }
} // Carrot::ECS