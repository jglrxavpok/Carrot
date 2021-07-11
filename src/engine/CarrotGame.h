//
// Created by jglrxavpok on 01/05/2021.
//

#pragma once

#include "vulkan/SwapchainAware.h"
#include "Engine.h"

namespace Carrot {
    class CarrotGame: public SwapchainAware {
    protected:
        Engine& engine;

    public:
        explicit CarrotGame(Engine& engine): engine(engine) {};

        virtual void onFrame(uint32_t frameIndex) = 0;

        virtual void tick(double frameTime) = 0;

        virtual void recordGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) = 0;

        virtual void onMouseMove(double dx, double dy) = 0;

        virtual void changeGraphicsWaitSemaphores(uint32_t frameIndex, vector<vk::Semaphore>& semaphores, vector<vk::PipelineStageFlags>& waitStages) {};

        virtual void onSwapchainSizeChange(int newWidth, int newHeight) override {};

        virtual void onSwapchainImageCountChange(size_t newCount) override {};

        virtual ~CarrotGame() = default;
    };

}
