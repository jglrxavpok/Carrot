//
// Created by jglrxavpok on 21/11/25.
//

#include <engine/Engine.h>
#include <gtest/gtest.h>

#include "TestFramework.h"

using namespace Carrot;

TEST_F(EngineTest, Fundamentals_EngineMustBootAndCloseWithoutCrashing) {
    Configuration config{};
    Engine engine{0, nullptr, config};
}

TEST_F(EngineTest, Fundamentals_EngineMustManage2ndBootAndCloseWithoutCrashing) {
    Configuration config{};
    Engine engine{0, nullptr, config};
}

TEST_F(EngineTest, Fundamentals_MainLoop_SingleFrame) {
    struct MainLoopGame: public TestGame {
        int ticks = 2;

        explicit MainLoopGame(Carrot::Engine& engine)
            : TestGame(engine) {}

        void tick(double frameTime) override {
            if(ticks--) {
                return;
            }
            engine.stop(); // ask for shutdown immediately
        }
    };

    Configuration config{};
    EngineTest::initGameCallback = [](Carrot::Engine& engine) {
        return std::make_unique<MainLoopGame>(engine);
    };
    Engine engine{0, nullptr, config};
    engine.run();
}
