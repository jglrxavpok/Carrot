//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once
#include <cstdint>
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <core/memory/OptionalRef.h>
#include <engine/ecs/components/Component.h>
#include <engine/ecs/systems/System.h>
#include "EntityTypes.h"

namespace Carrot::ECS {

    class World {
    public:
        explicit World();
        World(const World& toCopy) {
            *this = toCopy;
        }

        Signature getSignature(const Entity& entity) const;

        template<class Comp>
        Memory::OptionalRef<Comp> getComponent(const Entity& entity) const;

        template<class Comp>
        Memory::OptionalRef<Comp> getComponent(const EntityID& entityID) const;

        Tags getTags(const Entity& entity) const;

        std::vector<Entity> getEntitiesWithTags(Tags tags) const;

        std::string& getName(const EntityID& entityID);
        std::string& getName(const Entity& entity);
        const std::string& getName(const EntityID& entityID) const;
        const std::string& getName(const Entity& entity) const;

        void tick(double dt);
        void onFrame(Carrot::Render::Context renderContext);
        void recordOpaqueGBufferPass(const vk::RenderPass& pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands);
        void recordTransparentGBufferPass(const vk::RenderPass& pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands);

        Entity newEntity(std::string_view name = "<unnamed>");

        /// /!\ Unsafe! Adds an entity with the given ID, should probably only used for deserialization/networking purposes.
        Entity newEntityWithID(EntityID id, std::string_view name = "<unnamed>");

        void removeEntity(const Entity& ent);

        bool exists(EntityID ent) const;

    public:
        Entity wrap(EntityID id) const;

        std::vector<Entity> getAllEntities() const;
        std::vector<Component*> getAllComponents(const Entity& ent) const;
        std::vector<Component*> getAllComponents(const EntityID& ent) const;

    public:
        /// Stops the processing of components (no longer calls tick), but still processes added/removed entities
        void freezeLogic() { frozenLogic = true; }
        void unfreezeLogic() { frozenLogic = false; }

        /// Loads the systems of this world, allocating engine resources (eg lights, rigidbodies). Automatically in this state when constructing
        void reloadSystems();

        /// Unloads the systems of this world, freeing engine resources (eg lights, rigidbodies).
        void unloadSystems();

    public:
        template<class RenderSystemType, typename... Args>
        void addRenderSystem(Args&&... args);

        template<class LogicSystemType, typename... Args>
        void addLogicSystem(Args&&... args);

        void addRenderSystem(std::unique_ptr<System>&& system);
        void addLogicSystem(std::unique_ptr<System>&& system);

        template<class RenderSystemType>
        RenderSystemType* getRenderSystem();

        template<class LogicSystemType>
        LogicSystemType* getLogicSystem();

        /// Removes the given RenderSystem. Does nothing if it was not inside this world
        template<class RenderSystemType>
        void removeRenderSystem();

        /// Removes the given LogicSystem. Does nothing if it was not inside this world
        template<class LogicSystemType>
        void removeLogicSystem();

        /// Removes the given RenderSystem. Does nothing if it was not inside this world
        void removeRenderSystem(System* system);

        /// Removes the given LogicSystem. Does nothing if it was not inside this world
        void removeLogicSystem(System* system);

        std::vector<System*> getLogicSystems();
        std::vector<System*> getRenderSystems();

        std::vector<const System*> getLogicSystems() const;
        std::vector<const System*> getRenderSystems() const;

    public: // hierarchy
        /// Sets the parent of 'toSet' to 'parent'. 'parent' is allowed to be empty.
        void setParent(const Entity& toSet, std::optional<Entity> parent);

        /// Gets the parent of 'of'. Can return nullptr if no parent exists
        std::optional<Entity> getParent(const Entity& of) const;

        /// Gets the children of 'parent'. Can return an empty vector if it has no children
        const std::vector<Entity> getChildren(const Entity& parent) const;

    public:
        World& operator=(const World& toCopy);

    private:
        /// updates the systems entity list (based on entity signatures) called each tick and each frame
        /// (because components can be modified during a tick)
        void updateEntityLists();

    private:
        std::vector<EntityID> entities;
        std::vector<EntityID> entitiesToAdd;
        std::vector<EntityID> entitiesToRemove;
        std::vector<EntityID> entitiesUpdated;

        std::unordered_map<EntityID, std::unordered_map<ComponentID, std::unique_ptr<Component>>> entityComponents;
        std::unordered_map<EntityID, Tags> entityTags;
        std::unordered_map<EntityID, std::string> entityNames;

        std::vector<std::unique_ptr<System>> logicSystems;
        std::vector<std::unique_ptr<System>> renderSystems;

        bool frozenLogic = false;

    private: // internal representation of hierarchy
        std::unordered_map<EntityID, EntityID> entityParents;
        std::unordered_map<EntityID, std::vector<EntityID>> entityChildren;

        friend class Entity;
    };
}

#include "World.ipp"
