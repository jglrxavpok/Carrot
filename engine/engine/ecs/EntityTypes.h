//
// Created by jglrxavpok on 27/02/2021.
//

#pragma once
#include <memory>
#include <optional>
#include <cstddef>
#include <string_view>
#include <cassert>
#include <core/containers/Vector.hpp>
#include <core/memory/OptionalRef.h>
#include <core/utils/UUID.h>
#include <core/utils/Types.h>
#include "Signature.hpp"

namespace Carrot::ECS {
    enum class EntityFlags : std::uint64_t {
        None = 0,
        Hidden = 1 << 0,
    };

    inline EntityFlags operator&(const EntityFlags& a, const EntityFlags& b) {
        return static_cast<EntityFlags>(static_cast<std::uint64_t>(a) & static_cast<std::uint64_t>(b));
    }

    inline EntityFlags operator|(const EntityFlags& a, const EntityFlags& b) {
        return static_cast<EntityFlags>(static_cast<std::uint64_t>(a) | static_cast<std::uint64_t>(b));
    }

    inline EntityFlags& operator&=(EntityFlags& self, const EntityFlags& b) {
        self = static_cast<EntityFlags>(static_cast<std::uint64_t>(self) & static_cast<std::uint64_t>(b));
        return self;
    }

    inline EntityFlags& operator|=(EntityFlags& self, const EntityFlags& b) {
        self = static_cast<EntityFlags>(static_cast<std::uint64_t>(self) | static_cast<std::uint64_t>(b));
        return self;
    }

    inline bool operator==(const EntityFlags& flags, const std::uint64_t& v) {
        return static_cast<std::uint64_t>(flags) == v;
    }

    inline bool operator!=(const EntityFlags& flags, const std::uint64_t& v) {
        return static_cast<std::uint64_t>(flags) != v;
    }

    inline EntityFlags operator~(const EntityFlags& flags) {
        return static_cast<EntityFlags>(~static_cast<std::uint64_t>(flags));
    }

    EntityFlags stringToFlags(const std::string& str);
    std::string flagsToString(const EntityFlags& flags);

    using EntityID = Carrot::UUID;

    class World;
    class Component;
    class Prefab;

    /// Wrapper struct to allow easy addition of components. This does NOT hold the components.
    struct Entity {
        Entity() = default;

        Entity(EntityID ent, World& worldRef): internalEntity(ent), worldRef(&worldRef) {}

        Entity(const Entity& toCopy): internalEntity(toCopy.internalEntity), worldRef(toCopy.worldRef) {}

        Entity& operator=(const Entity& toCopy) {
            worldRef = toCopy.worldRef;
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

        Vector<Component*> getAllComponents() const;

        template<typename Comp>
        Entity& removeComponent();

        Entity& removeComponent(Component& comp);

        Entity& removeComponent(const ComponentID& id);

        Entity& setFlags(EntityFlags tag);
        Entity& removeFlags(EntityFlags tag);

        EntityFlags getFlags() const;

        /// Shorthand for checking the flags for Hidden
        bool isVisible() const;

        /**
         * Hides this entity and potentially its children, if recursive is set to true
         * (opposite of show)
         * By default entities are visible
         */
        void hide(bool recursive);

        /**
         * Shows this entity and potentially its children, if recursive is set to true
         * (opposite of hide)
         * By default entities are visible
         */
        void show(bool recursive);

        std::optional<Entity> getNamedChild(std::string_view name, ShouldRecurse recurse = ShouldRecurse::Recursion);
        std::optional<const Entity> getNamedChild(std::string_view name, ShouldRecurse recurse = ShouldRecurse::Recursion) const;
        std::optional<Entity> getParent();
        std::optional<const Entity> getParent() const;

        /// Sets the parent of 'toSet' to 'parent'. 'parent' is allowed to be empty.
        void setParent(std::optional<Entity> parent);

        /// Sets the parent, but also modify the entity's tranform (if any) to keep the same world transform after changing the parent
        void reparent(std::optional<Entity> parent);

        /**
         * Duplicate this entity, and optionally change the parent of the duplicated entity.
         * If newParent is empty, the new entity will have the same parent as this current entity
         */
        Carrot::ECS::Entity duplicate(std::optional<Carrot::ECS::Entity> newParent = {});

        World& getWorld() { return *worldRef; }
        const World& getWorld() const { return *worldRef; }

        operator EntityID() const {
            return internalEntity;
        }

        bool operator==(const Entity& o) const {
            return internalEntity == o.internalEntity && worldRef == o.worldRef;
        }

        bool operator!=(const Entity& o) const {
            return !(*this == o);
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
        EntityID internalEntity = Carrot::UUID::null();
        World* worldRef = nullptr;

        friend class World;
    };

    struct EntityWithComponents {
        Entity entity;
        std::vector<Component*> components; // order of components is based on index of component inside Signature used to create this EntityWithComponents
    };

    struct QueryResult {
        Signature signature;
        std::vector<EntityWithComponents> matchingEntities;
    };

    /**
     * Some names of entities are not allowed, because they might clash with the scene format
     */
    bool isIllegalEntityName(std::string_view);
}