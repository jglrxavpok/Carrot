#include <cstdio>
#include <memory>
#include "engine/Engine.h"
#include <core/io/IO.h>
#include <core/io/Resource.h>
#include "engine/CarrotGame.h"
#include <core/utils/Assert.h>
#include <iostream>

namespace Game {
    class Game: public Carrot::CarrotGame {
    public:
        explicit Game(Carrot::Engine& engine): Carrot::CarrotGame(engine) {};

        void onFrame(Carrot::Render::Context renderContext) override {};

        void tick(double frameTime) override {};

        void recordOpaqueGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) override {};

        void onMouseMove(double dx, double dy) override {};

        void changeGraphicsWaitSemaphores(uint32_t frameIndex, vector<vk::Semaphore>& semaphores, vector<vk::PipelineStageFlags>& waitStages) override {};

        void onSwapchainSizeChange(int newWidth, int newHeight) override {};

        void onSwapchainImageCountChange(size_t newCount) override {};
    };
}

using namespace Carrot::IO;
int main() {
    // TODO

    try {
        Resource resource("resources/shaders/screenQuad.vertex.glsl.spv");
        verify(resource.getSize() == 1620, "");

        auto buffer = resource.readAll();
        auto readWithIfstream = Carrot::IO::readFile("resources/shaders/screenQuad.vertex.glsl.spv");
        for (int i = 0; i < resource.getSize(); ++i) {
            verify(buffer[i] == readWithIfstream[i], "mismatched input.");
        }
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}

void Carrot::Engine::initGame() {
    game = std::make_unique<Game::Game>(*this);
}