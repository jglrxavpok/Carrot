#include <cstdio>
#include "engine/Engine.h"
#include "engine/io/IO.h"
#include "engine/io/Resource.h"
#include "engine/CarrotGame.h"
#include "engine/utils/Assert.h"
#include <iostream>

namespace Game {
    class Game: public Carrot::CarrotGame {
    public:
        explicit Game(Carrot::Engine& engine): Carrot::CarrotGame(engine) {};

        void onFrame(uint32_t frameIndex) override {};

        void tick(double frameTime) override {};

        void recordGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands) override {};

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
        runtimeAssert(resource.getSize() == 1620, "");

        auto buffer = resource.readAll();
        auto readWithIfstream = readFile("resources/shaders/screenQuad.vertex.glsl.spv");
        for (int i = 0; i < resource.getSize(); ++i) {
            runtimeAssert(buffer[i] == readWithIfstream[i], "mismatched input.");
        }
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}

void Carrot::Engine::initGame() {
    game = make_unique<Game::Game>(*this);
}