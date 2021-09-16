//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once
#include <cstdint>
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <engine/ecs/components/Component.h>
#include <engine/ecs/systems/System.h>
#include "EntityTypes.h"

namespace Carrot {

    class World {
    public:
        explicit World();

        Signature getSignature(const Entity_Ptr& entity) const;

        template<class Comp>
        Comp* getComponent(Entity_Ptr& entity) const;

        Carrot::Tags getTags(const Entity_Ptr& entity) const;

        std::vector<Entity_Ptr> getEntitiesWithTags(Carrot::Tags tags) const;

        const std::string& getName(const Entity_Ptr& entity) const;

        void tick(double dt);
        void onFrame(Carrot::Render::Context renderContext);
        void recordGBufferPass(vk::RenderPass& pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands);

        EasyEntity newEntity(std::string_view name = "<unnamed>");

        /// /!\ Unsafe! Adds an entity with the given ID, should probably only used for deserialization/networking purposes.
        EasyEntity newEntityWithID(EntityID id, std::string_view name = "<unnamed>");

        void removeEntity(const Entity_Ptr& ent);

        void setAllocationStrategy(const std::function<EntityID()>& allocationStrategy);

    public:
        template<class RenderSystemType, typename... Args>
        void addRenderSystem(Args&&... args);

        template<class LogicSystemType, typename... Args>
        void addLogicSystem(Args&&... args);

    public: // hierarchy
        /// Sets the parent of 'toSet' to 'parent'. 'parent' is allowed to be nullptr.
        void setParent(const Entity_Ptr& toSet, const Entity_Ptr& parent);

        /// Gets the parent of 'of'. Can return nullptr if no parent exists
        Entity_Ptr getParent(const Entity_Ptr& of) const;

        /// Gets the children of 'parent'. Can return an empty vector if it has no children
        const std::vector<Entity_Ptr>& getChildren(const Entity_Ptr& parent) const;

        friend class EasyEntity;

    private:
        EntityID freeEntityID = 0;
        std::vector<Entity_Ptr> entities;
        std::vector<Entity_Ptr> entitiesToAdd;
        std::vector<Entity_Ptr> entitiesToRemove;

        std::unordered_map<EntityID, std::unordered_map<ComponentID, std::unique_ptr<Component>>> entityComponents;
        std::unordered_map<EntityID, Tags> entityTags;
        std::unordered_map<EntityID, std::string> entityNames;

        std::vector<std::unique_ptr<System>> logicSystems;
        std::vector<std::unique_ptr<System>> renderSystems;

        std::function<EntityID()> allocationStrategy;

    private: // internal representation of hierarchy
        std::unordered_map<EntityID, Entity_Ptr> entityParents;
        std::unordered_map<EntityID, std::vector<Entity_Ptr>> entityChildren;
    };
}

#include "World.ipp"
