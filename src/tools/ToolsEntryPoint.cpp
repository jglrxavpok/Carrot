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

        void onFrame(uint32_t frameIndex) override;

        void tick(double frameTime) override;

        void recordGBufferPass(vk::RenderPass pass, uint32_t frameIndex, vk::CommandBuffer& commands) override;

        void onMouseMove(double dx, double dy) override;

        void changeGraphicsWaitSemaphores(uint32_t frameIndex, vector<vk::Semaphore>& semaphores, vector<vk::PipelineStageFlags>& waitStages) override;

        void onSwapchainSizeChange(int newWidth, int newHeight) override;

        void onSwapchainImageCountChange(size_t newCount) override;
    };
}

Tools::Tools::Tools(Carrot::Engine& engine): Carrot::CarrotGame(engine), particleEditor(engine) {
    engine.ungrabCursor();
    NFD_Init();
}

void Tools::Tools::onFrame(uint32_t frameIndex) {
    particleEditor.onFrame(frameIndex);
}
void Tools::Tools::tick(double frameTime) {
    particleEditor.tick(frameTime);
}
void Tools::Tools::recordGBufferPass(vk::RenderPass pass, uint32_t frameIndex, vk::CommandBuffer& commands) {}
void Tools::Tools::onMouseMove(double dx, double dy) {}
void Tools::Tools::changeGraphicsWaitSemaphores(uint32_t frameIndex, vector<vk::Semaphore>& semaphores, vector<vk::PipelineStageFlags>& waitStages) {}
void Tools::Tools::onSwapchainSizeChange(int newWidth, int newHeight) {};
void Tools::Tools::onSwapchainImageCountChange(size_t newCount) {};

int main() {
    glfwInit();

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    NakedPtr<GLFWwindow> window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, nullptr, nullptr);

    int w, h, n;
    auto* icon32Pixels = stbi_load("resources/icon32.png", &w, &h, &n, 4);
    auto* icon64Pixels = stbi_load("resources/icon64.png", &w, &h, &n, 4);
    auto* icon128Pixels = stbi_load("resources/icon128.png", &w, &h, &n, 4);
    GLFWimage icons[] = {
            {
                    .width = 32,
                    .height = 32,
                    .pixels = icon32Pixels,
            },
            {
                    .width = 64,
                    .height = 64,
                    .pixels = icon64Pixels,
            },
            {
                    .width = 128,
                    .height = 128,
                    .pixels = icon128Pixels,
            },
    };
    glfwSetWindowIcon(window.get(), 3, icons);
    stbi_image_free(icon32Pixels);
    stbi_image_free(icon64Pixels);
    stbi_image_free(icon128Pixels);

    // create new scope, as the destructor requires the window to *not* be terminated at its end
    // otherwise this creates a DEP exception when destroying the surface provided by GLFW
    try {
        {
            Carrot::Engine engine{window};
            engine.run();
        }
    } catch (std::exception& e) {
        std::cerr << "Caught exception:\n";
        std::cerr << e.what() << std::endl;
        throw e;
    }

    glfwDestroyWindow(window.get());
    glfwTerminate();

    return 0;
}

void Carrot::Engine::initGame() {
    game = make_unique<Tools::Tools>(*this);
}