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

        int upKey = -1;
        int leftKey = -1;
        int downKey = -1;
        int rightKey = -1;

        [[nodiscard]] bool isAxisIn(int axisID) const {
            return axisID >= 0 && (horizontalAxisID == axisID || verticalAxisID == axisID);
        }

        [[nodiscard]] bool isButtonIn(int buttonID) const {
            return buttonID >= 0 && (buttonID == upKey || buttonID == leftKey || buttonID == downKey || buttonID == rightKey);
        }
    };

    enum class GameInputVectorType: std::uint8_t {
        First,
        LeftStick = First,
        RightStick,
        Triggers,

        WASD,
        ZQSD,
        ArrowKeys,

        MouseMovement,
        MouseDeltaGrabbed, // Outputs only if the mouse cursor is grabbed by the engine (can be used to control cameras)

        Count,
    };

    constexpr static GamepadInputVector InputVectors[] = {
            { GLFW_GAMEPAD_AXIS_LEFT_X, GLFW_GAMEPAD_AXIS_LEFT_Y },
            { GLFW_GAMEPAD_AXIS_RIGHT_X, GLFW_GAMEPAD_AXIS_RIGHT_Y },
            { GLFW_GAMEPAD_AXIS_LEFT_TRIGGER, GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER },
            { -1, -1, GLFW_KEY_W, GLFW_KEY_A, GLFW_KEY_S, GLFW_KEY_D },
            { -1, -1, GLFW_KEY_Z, GLFW_KEY_Q, GLFW_KEY_S, GLFW_KEY_D },
            { -1, -1, GLFW_KEY_UP, GLFW_KEY_LEFT, GLFW_KEY_DOWN, GLFW_KEY_RIGHT },
            { -1, -1 },
            { -1, -1 },
    };

    static_assert(static_cast<std::size_t>(GameInputVectorType::Count) == sizeof(InputVectors) / sizeof(GamepadInputVector), "Both GameInputVectorType enum and inputVectors array must have the same count of members.");
}