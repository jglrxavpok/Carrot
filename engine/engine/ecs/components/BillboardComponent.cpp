#include "BillboardComponent.h"

namespace Carrot::ECS {
    BillboardComponent::BillboardComponent(Carrot::ECS::Entity entity)
            : Carrot::ECS::IdentifiableComponent<BillboardComponent>(std::move(entity)) {}

    std::unique_ptr <Carrot::ECS::Component> BillboardComponent::duplicate(const Carrot::ECS::Entity& newOwner) const {
        auto result = std::make_unique<BillboardComponent>(newOwner);
        return result;
    }

} // Carrot::ECS