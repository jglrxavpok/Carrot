//
// Created by jglrxavpok on 19/03/2025.
//

#include "MissingComponent.h"

namespace Carrot::ECS {
    static Async::SpinLock componentIDsAccess{};
    static std::unordered_map<std::string, ComponentID> componentIDs{};

    MissingComponent::MissingComponent(Entity entity, const std::string& originalComponent, const Carrot::DocumentElement& doc)
    : IdentifiableComponent<Carrot::ECS::MissingComponent>(entity)
    , originalComponent(originalComponent)
    , serializedVersion(doc)
    {}

    Carrot::DocumentElement MissingComponent::serialise() const {
        return serializedVersion;
    }

    const char *const MissingComponent::getName() const {
        return originalComponent.c_str();
    }

    std::unique_ptr<Component> MissingComponent::duplicate(const Entity& newOwner) const {
        return std::make_unique<MissingComponent>(newOwner, originalComponent, serializedVersion);
    }

    ComponentID MissingComponent::getComponentTypeID() const {
        Async::LockGuard g { componentIDsAccess };
        auto [iter, wasNew] = componentIDs.try_emplace(originalComponent);
        if (wasNew) {
            iter->second = requestComponentID();
        }
        return iter->second;
    }
}


template<>
inline const char* Carrot::Identifiable<Carrot::ECS::MissingComponent>::getStringRepresentation() {
    TODO;
    return "no no no no";
}
