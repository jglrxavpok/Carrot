//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once
#include "engine/ecs/Signature.hpp"
#include "engine/ecs/EntityTypes.h"
#include "engine/render/RenderContext.h"
#include <engine/render/RenderPass.h>

namespace Carrot {

    class World;

    enum class SystemType {
        Logic,
        Render,
    };

    class System {
    protected:
        Carrot::World& world;
        Signature signature;
        std::vector<Entity_WeakPtr> entities;

        virtual void onEntityAdded(Entity_WeakPtr entity) {};

    public:
        explicit System(Carrot::World& world);

        [[nodiscard]] const Signature& getSignature() const;

        virtual void onFrame(Carrot::Render::Context renderContext) = 0;
        virtual void tick(double dt) {};

        // TODO: provide a way to render even in other passes
        virtual void gBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {};

        void onEntitiesAdded(const std::vector<Entity_Ptr>& entities);
        void onEntitiesRemoved(const std::vector<Entity_Ptr>& entities);

        virtual ~System() = default;
    };

    template<SystemType systemType, typename... RequiredComponents>
    class SignedSystem: public System {
    public:
        explicit SignedSystem(Carrot::World& world);

        /// Calls 'action' of each entity in this system. Immediately called, so capturing on the stack is safe.
        void forEachEntity(const std::function<void(Entity_Ptr, RequiredComponents&...)>& action);
    };

    template<typename... RequiredComponents>
    class LogicSystem: public SignedSystem<SystemType::Logic, RequiredComponents...> {
    public:
        explicit LogicSystem(Carrot::World& world): SignedSystem<SystemType::Logic, RequiredComponents...>(world) {};

        void onFrame(Carrot::Render::Context renderContext) override {};
    };

    template<typename... RequiredComponents>
    class RenderSystem: public SignedSystem<SystemType::Render, RequiredComponents...> {
    public:
        explicit RenderSystem(Carrot::World& world): SignedSystem<SystemType::Render, RequiredComponents...>(world) {};

        void tick(double dt) override {};
    };

}

#include "System.ipp"
