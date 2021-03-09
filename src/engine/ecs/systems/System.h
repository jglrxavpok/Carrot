//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once
#include "engine/ecs/Signature.hpp"
#include "engine/ecs/EntityTypes.h"

namespace Carrot {

    using namespace std;

    class World;

    enum class SystemType {
        Logic,
        Render,
    };

    class System {
    protected:
        Carrot::World& world;
        Signature signature;
        vector<Entity_WeakPtr> entities;

        virtual void onEntityAdded(Entity_WeakPtr entity) {};

    public:
        explicit System(Carrot::World& world);

        [[nodiscard]] const Signature& getSignature() const;

        virtual void onFrame(size_t frameIndex) = 0;
        virtual void tick(double dt) {};

        void onEntitiesAdded(const vector<Entity_Ptr>& entities);
        void onEntitiesRemoved(const vector<Entity_Ptr>& entities);

        virtual ~System() = default;
    };

    template<SystemType systemType, typename... RequiredComponents>
    class SignedSystem: public System {
    public:
        explicit SignedSystem(Carrot::World& world);
    };

    template<typename... RequiredComponents>
    class LogicSystem: public SignedSystem<SystemType::Logic, RequiredComponents...> {
    public:
        explicit LogicSystem(Carrot::World& world): SignedSystem<SystemType::Logic, RequiredComponents...>(world) {};

        void onFrame(size_t frameIndex) override {};
    };

    template<typename... RequiredComponents>
    class RenderSystem: public SignedSystem<SystemType::Render, RequiredComponents...> {
    public:
        explicit RenderSystem(Carrot::World& world): SignedSystem<SystemType::Render, RequiredComponents...>(world) {};

        void tick(double dt) override {};
    };

}

#include "System.ipp"
