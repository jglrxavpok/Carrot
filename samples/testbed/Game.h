//
// Created by jglrxavpok on 05/12/2020.
//

#pragma once

#include <cstdint>
#include "engine/vulkan/SwapchainAware.h"
#include "engine/Engine.h"
#include <engine/ecs/World.h>
#include "engine/render/InstanceData.h"
#include "engine/render/animation/AnimatedInstances.h"
#include "engine/render/particles/Particles.h"
#include "engine/CarrotGame.h"
#include <engine/io/actions/ActionSet.h>

namespace Carrot {
    class Engine;
}

namespace Game {
    class Game: public Carrot::CarrotGame {
    protected:
        std::unique_ptr<Carrot::Model> mapModel = nullptr;
        std::unique_ptr<Carrot::Model> model = nullptr;
        std::unique_ptr<Carrot::Buffer> mapInstanceBuffer = nullptr;

        std::unique_ptr<Carrot::AnimatedInstances> animatedUnits = nullptr;

        Carrot::ECS::World world;
        std::unique_ptr<Carrot::ParticleBlueprint> blueprint = nullptr;
        std::unique_ptr<Carrot::ParticleSystem> particles = nullptr;

    public:
        explicit Game(Carrot::Engine& engine);

        void onFrame(Carrot::Render::Context renderContext) override;

        void tick(double frameTime) override;

        void recordOpaqueGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) override;
        void recordTransparentGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) override;

        void onMouseMove(double dx, double dy) override {}

        void changeGraphicsWaitSemaphores(uint32_t frameIndex, std::vector<vk::Semaphore>& semaphores, std::vector<vk::PipelineStageFlags>& waitStages) override;

        void onSwapchainSizeChange(int newWidth, int newHeight) override;

        void onSwapchainImageCountChange(size_t newCount) override;

    private:
        void moveCamera(double dx, double dy);

    private:
        Carrot::IO::ActionSet gameplayActions { "gameplay" };
        Carrot::IO::BoolInputAction grabMouse { "grab mouse" };
        Carrot::IO::BoolInputAction leftClickTest { "left click test" };
        Carrot::IO::BoolInputAction rightClickTest { "right click test" };
        Carrot::IO::BoolInputAction gamepadButtonTest { "Xbox A test" };
        Carrot::IO::BoolInputAction gamepadLeftTriggerTest { "Left trigger" };
        Carrot::IO::BoolInputAction gamepadLeftStickTest { "Left stick" };
        Carrot::IO::BoolInputAction gamepadRightStickTest { "Right stick" };
        Carrot::IO::Vec2InputAction cameraMove { "Camera move" };
    };
}
