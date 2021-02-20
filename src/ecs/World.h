//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once
#include <cstdint>
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <ecs/components/Component.h>

using namespace std;

namespace Carrot {


    using EntityID = uint32_t;
    using Entity = EntityID;
    using Entity_Ptr = shared_ptr<uint32_t>;
    using Entity_WeakPtr = weak_ptr<Entity>;

    class World {
    private:

        /// Wrapper struct to allow easy addition of components
        struct EasyEntity {
            EasyEntity(Entity_Ptr ent, World& worldRef): internalEntity(ent), worldRef(worldRef) {}

        public:
            template<typename Comp>
            EasyEntity& addComponent(unique_ptr<Comp>&& component);

            template<typename Comp, typename... Arg>
            EasyEntity& addComponent(Arg... args);

            template<typename Comp>
            Comp* getComponent();

        private:
            Entity_Ptr internalEntity;
            World& worldRef;

        };

        EntityID freeEntityID = 0;
        vector<Entity_Ptr> entities;
        vector<Entity_Ptr> entitiesToAdd;
        vector<Entity_Ptr> entitiesToRemove;

        unordered_map<Entity_Ptr, unordered_map<ComponentID, unique_ptr<Component>>> entityComponents;

    public:
        template<class Comp>
        Comp* getComponent(Entity_Ptr& entity) const;

        void tick(double dt);

        EasyEntity newEntity();
    };
}

#include "World.ipp"
