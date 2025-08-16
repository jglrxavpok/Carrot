//
// Created by jglrxavpok on 20/02/2021.
//

#pragma once
#include "engine/ecs/Signature.hpp"
#include "engine/ecs/EntityTypes.h"
#include "engine/render/RenderContext.h"
#include <engine/render/RenderPass.h>
#include <core/utils/Library.hpp>

namespace Carrot::Async {
    class Counter;
}

namespace Carrot::ECS {

    class World;

    enum class SystemType {
        Logic,
        Render,
    };

    class System {
    public:
        explicit System(World& world);
        explicit System(const Carrot::DocumentElement& doc, World& world): System(world) {};

        [[nodiscard]] const Signature& getSignature() const;
        std::span<const Entity> getEntities() const;

        virtual void onFrame(Carrot::Render::Context renderContext) = 0;
        virtual void setupCamera(Carrot::Render::Context renderContext) {};
        virtual void firstTick() {};
        virtual void tick(double dt) {};

        /// Called before physics updates. NOT called if the physics system is paused.
        /// Will be called at a fixed tickrate
        virtual void prePhysics() {};

        /// Called after physics updates. NOT called if the physics system is paused.
        /// Will be called at a fixed tickrate
        virtual void postPhysics() {};

        // TODO: provide a way to render even in other passes -> RenderPacket
        virtual void opaqueGBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {};
        virtual void transparentGBufferRender(const vk::RenderPass& renderPass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {};

        virtual void onEntitiesAdded(const std::vector<EntityID>& entities);
        virtual void onEntitiesRemoved(const std::vector<EntityID>& entities);

        /**
         * Called when entities have components added to or removed from them, while they were already active for at least a tick
         * @param entities
         */
        virtual void onEntitiesUpdated(const std::vector<EntityID>& entities);

        virtual void reload() {};
        virtual void unload() {};

        /**
         * Called at the end of each frame. Can be used to swap resources for the next frame.
         * TransformComponent have the transform from the previous frame which can be used for this case
         */
        virtual void swapBuffers() {};

        virtual std::unique_ptr<System> duplicate(World& newOwner) const = 0;

        virtual Carrot::DocumentElement serialise() const {
            return Carrot::DocumentElement{};
        };

        /// Is this system supposed to be serialized? This can be used for debug systems that are only present if the code says so.
        virtual bool shouldBeSerialized() const {
            return true;
        }

        virtual void broadcastStartEvent() {};
        virtual void broadcastStopEvent() {};

        virtual const char* getName() const = 0;

        virtual ~System() = default;

        World& getWorld() { return world; }
        const World& getWorld() const { return world; }

    protected:
        void parallelSubmit(const std::function<void()>& action, Async::Counter& counter);
        static std::size_t concurrency(); // avoids to include TaskScheduler

    protected:
        World& world;
        Signature signature;
        std::vector<Entity> entities;
        std::vector<EntityWithComponents> entitiesWithComponents;

        virtual void onEntityAdded(Entity& entity) {};

    private:

        void recreateEntityWithComponentsList();

        friend class World;
    };

    template<SystemType systemType, typename... RequiredComponents>
    class SignedSystem: public System {
    public:
        explicit SignedSystem(World& world);
        explicit SignedSystem(const rapidjson::Value& json, World& world): SignedSystem(world) {};

        /// Calls 'action' of each entity in this system. Immediately called, so capturing on the stack is safe.
        void forEachEntity(const std::function<void(Entity&, RequiredComponents&...)>& action);

        /// Calls 'action' of each entity in this system, using a different Task for each entity.
        ///  It is up to the user to ensure no data race arise from performing the action concurrently and on other threads.
        ///  Immediately called, so capturing on the stack is safe.
        void parallelForEachEntity(const std::function<void(Entity&, RequiredComponents&...)>& action);
    };

    template<typename... RequiredComponents>
    class LogicSystem: public SignedSystem<SystemType::Logic, RequiredComponents...> {
    public:
        explicit LogicSystem(World& world): SignedSystem<SystemType::Logic, RequiredComponents...>(world) {};
        explicit LogicSystem(const Carrot::DocumentElement& doc, World& world): LogicSystem<RequiredComponents...>(world) {};

        void onFrame(Carrot::Render::Context renderContext) override {};
    };

    template<typename... RequiredComponents>
    class RenderSystem: public SignedSystem<SystemType::Render, RequiredComponents...> {
    public:
        explicit RenderSystem(World& world): SignedSystem<SystemType::Render, RequiredComponents...>(world) {};
        explicit RenderSystem(const Carrot::DocumentElement& doc, World& world): RenderSystem<RequiredComponents...>(world) {};

        void tick(double dt) override {};
    };

    using SystemLibrary = Library<std::unique_ptr<System>, World&>;

    SystemLibrary& getSystemLibrary();

}

#include "System.ipp"
