//
// Created by jglrxavpok on 21/11/25.
//

#pragma once
#include <functional>
#include <gtest/gtest.h>

#include <engine/CarrotGame.h>

namespace Carrot {
    struct EngineTest: public testing::Test {
        static std::function<std::unique_ptr<CarrotGame>(Carrot::Engine&)> initGameCallback;

        void SetUp() override;
        void TearDown() override;
    };

    struct TestGame: public CarrotGame {
        explicit TestGame(Carrot::Engine& engine)
            : CarrotGame(engine) {}

        void onFrame(const Carrot::Render::Context& renderContext) override {}
        void tick(double frameTime) override {}
    };
}
