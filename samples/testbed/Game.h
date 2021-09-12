//
// Created by jglrxavpok on 05/12/2020.
//

#pragma once

#include <cstdint>
#include "engine/vulkan/SwapchainAware.h"
#include "engine/vulkan/includes.h"
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
    using namespace Carrot;

    class Game: public CarrotGame {
    protected:
        unique_ptr<Model> mapModel = nullptr;
        unique_ptr<Model> model = nullptr;
        unique_ptr<Buffer> mapInstanceBuffer = nullptr;

        unique_ptr<AnimatedInstances> animatedUnits = nullptr;

        World world;
        unique_ptr<ParticleBlueprint> blueprint = nullptr;
        unique_ptr<ParticleSystem> particles = nullptr;

    public:
        explicit Game(Engine& engine);

        void onFrame(Carrot::Render::Context renderContext) override;

        void tick(double frameTime) override;

        void recordGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) override;

        void onMouseMove(double dx, double dy) override {}

        void changeGraphicsWaitSemaphores(uint32_t frameIndex, vector<vk::Semaphore>& semaphores, vector<vk::PipelineStageFlags>& waitStages) override;

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
