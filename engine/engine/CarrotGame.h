//
// Created by jglrxavpok on 01/05/2021.
//

#pragma once

#include "vulkan/SwapchainAware.h"
#include "Engine.h"

namespace Carrot {
    class CarrotGame: public SwapchainAware {
    protected:
        Carrot::Engine& engine;

    public:
        explicit CarrotGame(Carrot::Engine& engine): engine(engine) {};

        //! Called once per viewport per frame. Push your render calls here.
        virtual void onFrame(Carrot::Render::Context renderContext) = 0;

        //! Called once per viewport per frame per eye. Setup your camera here to work properly in VR. Also called if not in VR, just before onFrame
        virtual void setupCamera(Carrot::Render::Context renderContext) {};

        virtual void tick(double frameTime) = 0;
        virtual void prePhysics() {};
        virtual void postPhysics() {};

        [[deprecated]] virtual void onMouseMove(double dx, double dy) {};

        virtual void recordOpaqueGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {};
        virtual void recordTransparentGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {};

        virtual void changeGraphicsWaitSemaphores(std::uint32_t frameIndex, std::vector<vk::Semaphore>& semaphores, std::vector<vk::PipelineStageFlags>& waitStages) {};

        virtual void onSwapchainSizeChange(Window& window, int newWidth, int newHeight) override {};

        virtual void onSwapchainImageCountChange(std::size_t newCount) override {};

        /// Return false if you want to cancel the event
        virtual bool onCloseButtonPressed() { return true; };

        virtual ~CarrotGame() = default;

    public:
        void requestShutdown() { requestedShutdown = true; };
        bool hasRequestedShutdown() const { return requestedShutdown; }

    private:
        bool requestedShutdown = false;
    };

}
