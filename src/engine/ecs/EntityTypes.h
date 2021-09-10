//
// Created by jglrxavpok on 27/02/2021.
//

#pragma once
#include <memory>
#include <cstddef>
#include <string_view>

typedef std::uint32_t EntityID;
typedef EntityID Entity;
typedef std::shared_ptr<EntityID> Entity_Ptr;
typedef std::weak_ptr<Entity> Entity_WeakPtr;

namespace Carrot {
    using Tags = std::uint64_t;

    class World;

    /// Wrapper struct to allow easy addition of components
    struct EasyEntity {
        EasyEntity(Entity_Ptr ent, World& worldRef): internalEntity(ent), worldRef(worldRef) {}

        template<typename Comp>
        EasyEntity& addComponent(std::unique_ptr<Comp>&& component);

        template<typename Comp, typename... Arg>
        EasyEntity& addComponent(Arg&&... args);

        template<typename Comp, typename... Arg>
        EasyEntity& addComponentIf(bool condition, Arg&&... args);

        template<typename Comp>
        Comp* getComponent();

        EasyEntity& addTag(Tags tag);

        Tags getTags() const;

        Entity_Ptr getParent();
        const Entity_Ptr getParent() const;

        void setParent(const Entity_Ptr& parent);

        World& getWorld() { return worldRef; }
        const World& getWorld() const { return worldRef; }

        operator EntityID() const {
            return *internalEntity;
        }

        operator Entity_Ptr() {
            return internalEntity;
        }

        operator Entity_Ptr() const {
            return internalEntity;
        }

        std::string_view getName() const;

        void updateName(std::string_view name);

    private:
        Entity_Ptr internalEntity;
        World& worldRef;

        friend class World;
    };
}