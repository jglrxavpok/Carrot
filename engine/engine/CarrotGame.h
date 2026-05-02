//
// Created by jglrxavpok on 01/05/2021.
//

#pragma once

#include "render/Composer.h"
#include "vulkan/SwapchainAware.h"

namespace Carrot {
    class Engine;

    namespace Render {
        class CompiledPass;
        struct Context;
    }

    class CarrotGame: public SwapchainAware {
    protected:
        Carrot::Engine& engine;

    public:
        explicit CarrotGame(Carrot::Engine& engine): engine(engine) {};

        //! Called once per viewport per frame. Push your render calls here.
        virtual void onFrame(const Carrot::Render::Context& renderContext) = 0;

        //! Called once per viewport per frame per eye. Setup your camera here to work properly in VR. Also called if not in VR, just before onFrame
        virtual void setupCamera(const Carrot::Render::Context& renderContext) {};

        virtual void tick(double frameTime) = 0;
        virtual void prePhysics() {};
        virtual void postPhysics() {};

        [[deprecated]] virtual void onMouseMove(double dx, double dy) {};

        virtual void changeGraphicsWaitSemaphores(std::uint32_t frameIndex, std::vector<vk::Semaphore>& semaphores, std::vector<vk::PipelineStageFlags>& waitStages) {};

        virtual void onSwapchainSizeChange(Window& window, int newWidth, int newHeight) override {};

        virtual void onSwapchainImageCountChange(std::size_t newCount) override {};

        /// Return false if you want to cancel the event
        virtual bool onCloseButtonPressed() { return true; };

        virtual ~CarrotGame() = default;

    public:
        void requestShutdown() { requestedShutdown = true; };
        bool hasRequestedShutdown() const { return requestedShutdown; }

        /// Allows to move a single viewport
        Render::ViewportLocation& getViewportLocation(const Identifier& viewportID);

        /// Modifies count, location, and ids of viewports used by game
        /// Returns the texture in which the game will be rendered
        Carrot::Render::FrameResource updateViewportComposition(Render::ViewportComposition&& composition);

        /// Allows game to select in which viewport it will be rendered
        /// Returns the texture in which the game will be rendered
        Carrot::Render::FrameResource setGameViewport(const Identifier& gameViewportID);

        /// Called after the game viewport has changed size, to allow subviewports to be resized
        void onGameViewportSizeChanged(u32 w, u32 h);

    private:
        Carrot::Render::ViewportComposition currentComposition; // stored without render graphs, they are moved inside the viewports
        bool requestedShutdown = false;
    };

}
