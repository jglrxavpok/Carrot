//
// Created by jglrxavpok on 01/05/2024.
//

#pragma once

#include <engine/ecs/components/Component.h>

namespace Carrot::ECS {

    /// Component added to entities when something went wrong while loading the entity, mostly to show it in the editor.
    /// This component is not serialized
    class ErrorComponent: public IdentifiableComponent<ErrorComponent> {
    public:
        std::string message;

        explicit ErrorComponent(Carrot::ECS::Entity entity);
        ErrorComponent(const Carrot::DocumentElement& doc, Carrot::ECS::Entity entity);

        const char * const getName() const override;

        std::unique_ptr<Component> duplicate(const Entity &newOwner) const override;

        Carrot::DocumentElement serialise() const override;

        virtual bool isSerializable() const override;
    };

} // Carrot::ECS

template<>
inline const char *Carrot::Identifiable<Carrot::ECS::ErrorComponent>::getStringRepresentation() {
    return "ErrorComponent";
}
