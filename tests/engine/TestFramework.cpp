#include "TestFramework.h"

#include <engine/Engine.h>

namespace Carrot {
    /*static*/ std::function<std::unique_ptr<CarrotGame>(Carrot::Engine&)> EngineTest::initGameCallback;

    void EngineTest::SetUp() {
        tracy::GetGpuCtxCounter() = 0; // reset tracy counter to avoid crashes just because we run multiple tests
        initGameCallback = {};
    }

    void EngineTest::TearDown() {
        initGameCallback = {};
    }

    void Engine::initGame() {
        if (EngineTest::initGameCallback) {
            game = EngineTest::initGameCallback(*this);
        }
    }
}

