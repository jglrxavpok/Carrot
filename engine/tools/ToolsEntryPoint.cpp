//
// Created by jglrxavpok on 01/05/2021.
//

#include <iostream>
#include "engine/Engine.h"
#include "stb_image.h"
#include "engine/render/raytracing/ASBuilder.h"
#include "engine/CarrotGame.h"
#include "ParticleEditor.h"
#include <nfd.h>

namespace Tools {
    class Tools: public Carrot::CarrotGame {
    private:
        ParticleEditor particleEditor;

    public:
        explicit Tools(Carrot::Engine& engine);

        void onFrame(Carrot::Render::Context renderContext) override;

        void tick(double frameTime) override;

        void recordOpaqueGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) override;

        void onMouseMove(double dx, double dy) override;

        void changeGraphicsWaitSemaphores(uint32_t frameIndex, std::vector<vk::Semaphore>& semaphores, std::vector<vk::PipelineStageFlags>& waitStages) override;

        void onSwapchainSizeChange(int newWidth, int newHeight) override;

        void onSwapchainImageCountChange(size_t newCount) override;
    };
}

Tools::Tools::Tools(Carrot::Engine& engine): Carrot::CarrotGame(engine), particleEditor(engine) {
    engine.ungrabCursor();
    NFD_Init();
}

void Tools::Tools::onFrame(Carrot::Render::Context renderContext) {
    particleEditor.onFrame(renderContext);
}
void Tools::Tools::tick(double frameTime) {
    particleEditor.tick(frameTime);
}
void Tools::Tools::recordOpaqueGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) {}
void Tools::Tools::onMouseMove(double dx, double dy) {}
void Tools::Tools::changeGraphicsWaitSemaphores(uint32_t frameIndex, std::vector<vk::Semaphore>& semaphores, std::vector<vk::PipelineStageFlags>& waitStages) {}

void Tools::Tools::onSwapchainSizeChange(int newWidth, int newHeight) {
    particleEditor.onSwapchainSizeChange(newWidth, newHeight);
}

void Tools::Tools::onSwapchainImageCountChange(size_t newCount) {
    particleEditor.onSwapchainImageCountChange(newCount);
}

int main() {
    Carrot::Configuration config;
    config.useRaytracing = false;
    config.icon32 = "resources/icon32.png";
    config.icon64 = "resources/icon64.png";
    config.icon128 = "resources/icon128.png";
    Carrot::Engine engine{config};
    engine.run();

    return 0;
}

void Carrot::Engine::initGame() {
    game = std::make_unique<Tools::Tools>(*this);
}