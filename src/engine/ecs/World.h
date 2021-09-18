//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once
#include <cstdint>
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <engine/memory/OptionalRef.h>
#include <engine/ecs/components/Component.h>
#include <engine/ecs/systems/System.h>
#include "EntityTypes.h"

namespace Carrot::ECS {

    class World {
    public:
        explicit World();

        Signature getSignature(const Entity& entity) const;

        template<class Comp>
        Memory::OptionalRef<Comp> getComponent(const Entity& entity) const;

        Tags getTags(const Entity& entity) const;

        std::vector<Entity> getEntitiesWithTags(Tags tags) const;

        const std::string& getName(const Entity& entity) const;

        void tick(double dt);
        void onFrame(Carrot::Render::Context renderContext);
        void recordGBufferPass(vk::RenderPass& pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands);

        Entity newEntity(std::string_view name = "<unnamed>");

        /// /!\ Unsafe! Adds an entity with the given ID, should probably only used for deserialization/networking purposes.
        Entity newEntityWithID(EntityID id, std::string_view name = "<unnamed>");

        void removeEntity(const Entity& ent);

        void setAllocationStrategy(const std::function<EntityID()>& allocationStrategy);

        bool exists(EntityID ent) const;

    public:
        template<class RenderSystemType, typename... Args>
        void addRenderSystem(Args&&... args);

        template<class LogicSystemType, typename... Args>
        void addLogicSystem(Args&&... args);

    public: // hierarchy
        /// Sets the parent of 'toSet' to 'parent'. 'parent' is allowed to be empty.
        void setParent(const Entity& toSet, std::optional<Entity> parent);

        /// Gets the parent of 'of'. Can return nullptr if no parent exists
        std::optional<Entity> getParent(const Entity& of) const;

        /// Gets the children of 'parent'. Can return an empty vector if it has no children
        const std::vector<Entity> getChildren(const Entity& parent) const;

        friend class EasyEntity;

    private:
        std::vector<EntityID> entities;
        std::vector<EntityID> entitiesToAdd;
        std::vector<EntityID> entitiesToRemove;

        std::unordered_map<EntityID, std::unordered_map<ComponentID, std::unique_ptr<Component>>> entityComponents;
        std::unordered_map<EntityID, Tags> entityTags;
        std::unordered_map<EntityID, std::string> entityNames;

        std::vector<std::unique_ptr<System>> logicSystems;
        std::vector<std::unique_ptr<System>> renderSystems;

        std::function<EntityID()> allocationStrategy;

    private: // internal representation of hierarchy
        std::unordered_map<EntityID, EntityID> entityParents;
        std::unordered_map<EntityID, std::vector<EntityID>> entityChildren;

        friend class Entity;
    };
}

#include "World.ipp"
