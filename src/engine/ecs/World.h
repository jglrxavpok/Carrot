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

using namespace std;

namespace Carrot {

    class World {
    private:
        EntityID freeEntityID = 0;
        vector<Entity_Ptr> entities;
        vector<Entity_Ptr> entitiesToAdd;
        vector<Entity_Ptr> entitiesToRemove;

        unordered_map<EntityID, unordered_map<ComponentID, unique_ptr<Component>>> entityComponents;
        unordered_map<EntityID, Tags> entityTags;
        unordered_map<EntityID, std::string> entityNames;

        vector<unique_ptr<System>> logicSystems;
        vector<unique_ptr<System>> renderSystems;

    private: // internal representation of hierarchy
        unordered_map<EntityID, Entity_Ptr> entityParents;
        unordered_map<EntityID, std::vector<Entity_Ptr>> entityChildren;

    public:
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

        void removeEntity(const Entity_Ptr& ent);

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
    };
}

#include "World.ipp"
