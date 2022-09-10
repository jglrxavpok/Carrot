//
// Created by jglrxavpok on 27/02/2021.
//

#pragma once
#include <memory>
#include <optional>
#include <cstddef>
#include <string_view>
#include <cassert>
#include <core/memory/OptionalRef.h>
#include <core/utils/UUID.h>
#include <core/utils/Types.h>
#include "Signature.hpp"

namespace Carrot::ECS {
    using Tags = std::uint64_t;
    using EntityID = Carrot::UUID;

    class World;
    class Component;

    /// Wrapper struct to allow easy addition of components. This does NOT hold the components.
    struct Entity {
        Entity(EntityID ent, World& worldRef): internalEntity(ent), worldRef(worldRef) {}

        Entity(const Entity& toCopy): internalEntity(toCopy.internalEntity), worldRef(toCopy.worldRef) {}

        Entity& operator=(const Entity& toCopy) {
            assert(&worldRef == &toCopy.worldRef);
            internalEntity = toCopy.internalEntity;
            return *this;
        }

        /// Converts to true iff the world has this entity
        explicit operator bool() const;

        bool exists() const;

        template<typename Comp>
        Entity& addComponent(std::unique_ptr<Comp>&& component);

        template<typename Comp, typename... Arg>
        Entity& addComponent(Arg&&... args);

        template<typename Comp, typename... Arg>
        Entity& addComponentIf(bool condition, Arg&&... args);

        template<typename Comp>
        Memory::OptionalRef<Comp> getComponent();

        Memory::OptionalRef<Component> getComponent(ComponentID component) const;

        std::vector<Component*> getAllComponents() const;

        template<typename Comp>
        Entity& removeComponent();

        Entity& removeComponent(Component& comp);

        Entity& removeComponent(const ComponentID& id);

        Entity& addTag(Tags tag);

        Tags getTags() const;

        std::optional<Entity> getNamedChild(std::string_view name, ShouldRecurse recurse = ShouldRecurse::Recursion);
        std::optional<const Entity> getNamedChild(std::string_view name, ShouldRecurse recurse = ShouldRecurse::Recursion) const;
        std::optional<Entity> getParent();
        std::optional<const Entity> getParent() const;

        void setParent(std::optional<Entity> parent);

        World& getWorld() { return worldRef; }
        const World& getWorld() const { return worldRef; }

        operator EntityID() const {
            return internalEntity;
        }

        const EntityID& getID() const {
            return internalEntity;
        }

        Signature getSignature() const;

        std::string_view getName() const;
        std::string& getName();

        void updateName(std::string_view name);

    public:
        //! Schedule this entity and its children for removal in the next tick
        void remove();

    private:
        EntityID internalEntity;
        World& worldRef;

        friend class World;
    };
}