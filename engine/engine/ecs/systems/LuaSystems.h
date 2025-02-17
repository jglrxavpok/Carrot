//
// Created by jglrxavpok on 15/05/2022.
//

#pragma once
#include "engine/ecs/systems/System.h"
#include "engine/ecs/components/LuaScriptComponent.h"

namespace Carrot::ECS {
    class LuaUpdateSystem: public LogicSystem<Carrot::ECS::LuaScriptComponent>, public Identifiable<LuaUpdateSystem> {
    public:
        explicit LuaUpdateSystem(World& world): LogicSystem<Carrot::ECS::LuaScriptComponent>(world) {}
        explicit LuaUpdateSystem(const Carrot::DocumentElement& doc, World& world): LuaUpdateSystem(world) {}

        void tick(double dt) override;

        std::unique_ptr<System> duplicate(World& newOwner) const override {
            return std::make_unique<LuaUpdateSystem>(newOwner);
        }

        virtual void broadcastStartEvent() override;
        virtual void broadcastStopEvent() override;

    public:
        inline static const char* getStringRepresentation() {
            return "LuaUpdate";
        }

        virtual const char* getName() const override {
            return getStringRepresentation();
        }
    };

    class LuaRenderSystem: public RenderSystem<Carrot::ECS::LuaScriptComponent>, public Identifiable<LuaRenderSystem> {
    public:
        explicit LuaRenderSystem(World& world): RenderSystem<Carrot::ECS::LuaScriptComponent>(world) {}
        explicit LuaRenderSystem(const Carrot::DocumentElement& doc, World& world): LuaRenderSystem(world) {}

        void onFrame(Carrot::Render::Context renderContext) override;

        std::unique_ptr<System> duplicate(World& newOwner) const override {
            return std::make_unique<LuaRenderSystem>(newOwner);
        }

    public:
        inline static const char* getStringRepresentation() {
            return "LuaRender";
        }

        virtual const char* getName() const override {
            return getStringRepresentation();
        }
    };
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::LuaRenderSystem>::getStringRepresentation() {
    return Carrot::ECS::LuaRenderSystem::getStringRepresentation();
}

template<>
inline const char* Carrot::Identifiable<Carrot::ECS::LuaUpdateSystem>::getStringRepresentation() {
    return Carrot::ECS::LuaUpdateSystem::getStringRepresentation();
}