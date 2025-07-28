//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once
#include <cstdint>
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <core/memory/OptionalRef.h>
#include <engine/ecs/components/Component.h>
#include <engine/ecs/systems/System.h>
#include <engine/ecs/WorldData.h>
#include <eventpp/callbacklist.h>

#include "EntityTypes.h"

namespace Carrot::ECS {
    class World {
    public:
        explicit World();
        ~World();
        World(const World& toCopy): World() {
            *this = toCopy;
        }

        Signature getSignature(const Entity& entity) const;

        template<class Comp>
        Memory::OptionalRef<Comp> getComponent(const Entity& entity) const;

        template<class Comp>
        Memory::OptionalRef<Comp> getComponent(const EntityID& entityID) const;

        Memory::OptionalRef<Component> getComponent(const Entity& entity, ComponentID component) const;

        Memory::OptionalRef<Component> getComponent(const EntityID& entityID, ComponentID component) const;

        EntityFlags getFlags(const Entity& entity) const;

        std::vector<Entity> getEntitiesWithFlags(EntityFlags tags) const;

        template<typename... Component>
        std::span<const EntityWithComponents> queryEntities();

        std::span<const EntityWithComponents> queryEntities(const std::unordered_set<Carrot::ComponentID>& componentIDs);
        std::span<const EntityWithComponents> queryEntities(const Signature& signature);

        /**
         * From the given entity list, fill 'toFill' with the components matching the given signature.
         * See documentation of EntityWithComponents for the order in which components are stored
         */
        void fillComponents(const Signature& signature, std::span<const Entity> entities, std::span<EntityWithComponents> toFill);

        std::string& getName(const EntityID& entityID);
        std::string& getName(const Entity& entity);
        const std::string& getName(const EntityID& entityID) const;
        const std::string& getName(const Entity& entity) const;

        /**
         * Ensures entities are added to the global entity list.
         * Not thread-safe, should only be called by ::tick, or when you really know no one else is using this World instance
         */
        void flushEntityCreationAndRemoval();
        void tick(double dt);
        void prePhysics();
        void postPhysics();
        void setupCamera(Carrot::Render::Context renderContext);
        void onFrame(Carrot::Render::Context renderContext);
        void recordOpaqueGBufferPass(const vk::RenderPass& pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands);
        void recordTransparentGBufferPass(const vk::RenderPass& pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands);

        Entity newEntity(std::string_view name = "<unnamed>");

        /// /!\ Unsafe! Adds an entity with the given ID, should probably only used for deserialization/networking purposes.
        Entity newEntityWithID(EntityID id, std::string_view name = "<unnamed>");

        void removeEntity(const EntityID& ent);

        bool exists(EntityID ent) const;

    public:
        Entity wrap(EntityID id) const;

        std::vector<Entity> getAllEntities() const;
        Carrot::Vector<Component*> getAllComponents(const Entity& ent) const;
        Carrot::Vector<Component*> getAllComponents(const EntityID& ent) const;

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
        RenderSystemType& addRenderSystem(Args&&... args);

        template<class LogicSystemType, typename... Args>
        LogicSystemType& addLogicSystem(Args&&... args);

        void addRenderSystem(std::unique_ptr<System>&& system);
        void addLogicSystem(std::unique_ptr<System>&& system);

        template<class RenderSystemType>
        RenderSystemType* getRenderSystem();

        template<class LogicSystemType>
        LogicSystemType* getLogicSystem();

        System* getRenderSystem(std::string_view name);
        System* getLogicSystem(std::string_view name);

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

        /// Updates the list of
        void reloadSystemEntities(System* system);

        std::vector<System*> getLogicSystems();
        std::vector<System*> getRenderSystems();

        std::vector<const System*> getLogicSystems() const;
        std::vector<const System*> getRenderSystems() const;

    public:
        void broadcastStartEvent();
        void broadcastStopEvent();

    public: // world data
        WorldData& getWorldData();
        const WorldData& getWorldData() const;

    public: // hierarchy
        /// Sets the parent of 'toSet' to 'parent'. 'parent' is allowed to be empty.
        void setParent(const Entity& toSet, std::optional<Entity> parent);

        /// Sets the parent, but also modify the entity's tranform (if any) to keep the same world transform after changing the parent
        void reparent(Entity& toSet, std::optional<Entity> parent);

        /// Duplicates this entity and its children
        /// Returns the clone of this entity
        Carrot::ECS::Entity duplicate(const Carrot::ECS::Entity& entity, std::optional<Carrot::ECS::Entity> newParent = {});

        /// Duplicates this entity, using 'remap' to find the new ID of each clone (this entity and its children, recursively).
        /// If no ID exists for an entity, a new one will be generated and added to 'remap'.
        /// Returns the clone of this entity
        Carrot::ECS::Entity duplicateWithRemap(const Carrot::ECS::Entity& entity, std::optional<Carrot::ECS::Entity> newParent, std::unordered_map<Carrot::ECS::EntityID, Carrot::ECS::EntityID>& remap);

        /// Gets the parent of 'of'. Can return nullptr if no parent exists
        std::optional<Entity> getParent(const Entity& of) const;

        /// Gets the children of 'parent'. Can return an empty vector if it has no children
        std::vector<Entity> getChildren(const Entity& parent, ShouldRecurse recurse = ShouldRecurse::Recursion) const;

        /// Gets the children of 'parent'. Can return an empty vector if it has no children
        std::optional<Entity> getNamedChild(std::string_view name, const Entity& parent, ShouldRecurse recurse = ShouldRecurse::Recursion) const;

        /// Gets the first entity with the given name
        std::optional<Entity> findEntityByName(std::string_view name) const;

        /// Go through all entities, and change the components references to entities based on 'remap'.
        /// Used when duplicating entities to ensure components of duplicated entities don't reference the original entities.
        void repairLinks(const std::unordered_map<Carrot::ECS::EntityID, Carrot::ECS::EntityID>& remap);

        /// Go through the entire hierarchy starting from 'root', and change the components references to entities based on 'remap'.
        /// Used when duplicating entities to ensure components of duplicated entities don't reference the original entities.
        void repairLinks(const Carrot::ECS::Entity& root, const std::unordered_map<Carrot::ECS::EntityID, Carrot::ECS::EntityID>& remap);

    public:
        World& operator=(const World& toCopy);

    private:
        /// updates the systems entity list (based on entity signatures) called each tick and each frame
        /// (because components can be modified during a tick)
        void updateEntityLists();

        /// Based on entities added, removed and updated (components added/removed), removes cached queries which are impacted
        ///  by these changes.
        /// The next call to queryEntities will therefore recompute the proper list of entities matching a signature
        /// Called *before* changes are applied, because we need to get the signature of entities which are being removed
        void invalidateQueries();

    private:
        WorldData worldData;
        std::vector<EntityID> entities;
        std::vector<EntityID> entitiesToAdd;
        std::vector<EntityID> entitiesToRemove;
        std::vector<EntityID> entitiesUpdated;

        std::unordered_map<EntityID, std::unordered_map<ComponentID, std::unique_ptr<Component>>> entityComponents;
        std::unordered_map<EntityID, EntityFlags> entityFlags;
        std::unordered_map<EntityID, std::string> entityNames;

        std::vector<QueryResult> queries; //< cache result of queries to avoid recomputing the list on each call of queryEntities

        std::vector<std::unique_ptr<System>> logicSystems;
        std::vector<std::unique_ptr<System>> renderSystems;

        bool frozenLogic = false;

        // used to invalidate structures that hold csharp components
        eventpp::CallbackList<void()>::Handle csharpLoadCallbackHandle;
        eventpp::CallbackList<void()>::Handle csharpUnloadCallbackHandle;

    private: // internal representation of hierarchy
        std::unordered_map<EntityID, EntityID> entityParents;
        std::unordered_map<EntityID, std::vector<EntityID>> entityChildren;

        friend class Entity;
    };
}

#include "World.ipp"
