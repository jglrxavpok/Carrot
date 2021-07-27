//
// Created by jglrxavpok on 27/07/2021.
//

#pragma once

#include <GLFW/glfw3.h>
#include <cstdint>

namespace Carrot::IO {
    /// Represents a pair of axes which combined form a Vec2 input
    struct GamepadInputVector {
        int horizontalAxisID = -1;
        int verticalAxisID = -1;

        [[nodiscard]] bool isIn(int axisID) const {
            return axisID >= 0 && (horizontalAxisID == axisID || verticalAxisID == axisID);
        }
    };

    enum class GameInputVectorType: std::uint8_t {
        First,
        LeftStick = First,
        RightStick,
        Triggers,

        // TODO: common key groups (ZQSD, WASD, Arrow keys)

        MouseMovement,
        MouseDeltaGrabbed, // Outputs only if the mouse cursor is grabbed by the engine (can be used to control cameras)

        Count,
    };

    constexpr static GamepadInputVector InputVectors[] = {
            { GLFW_GAMEPAD_AXIS_LEFT_X, GLFW_GAMEPAD_AXIS_LEFT_Y },
            { GLFW_GAMEPAD_AXIS_RIGHT_X, GLFW_GAMEPAD_AXIS_RIGHT_Y },
            { GLFW_GAMEPAD_AXIS_LEFT_TRIGGER, GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER },
            { -1, -1 },
            { -1, -1 },
    };

    static_assert(static_cast<std::size_t>(GameInputVectorType::Count) == sizeof(InputVectors) / sizeof(GamepadInputVector), "Both GameInputVectorType enum and inputVectors array must have the same count of members.");
}