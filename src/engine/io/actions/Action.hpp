//
// Created by jglrxavpok on 25/07/2021.
//

#pragma once

#include <glm/vec2.hpp>
#include <string>
#include <string_view>
#include "engine/utils/stringmanip.h"

namespace Carrot::IO {
    enum class ActionType {
        FloatInput,
        BoolInput,
        Vec2Input,

        // TODO FloatOutput
    };

    template<ActionType type>
    concept IsFloatInput = type == ActionType::FloatInput;
    template<ActionType type>
    concept IsBoolInput = type == ActionType::BoolInput;
    template<ActionType type>
    concept IsVec2Input = type == ActionType::Vec2Input;

    template<ActionType type>
    class Action {


    public:
        explicit Action(std::string_view name, std::string_view suggestedBinding = ""): name(name), suggestedBinding(suggestedBinding) {
            std::memset(&state, 0, sizeof(state));
        }

        const std::string& getName() const { return name; }
        const std::string& getSuggestedBinding() const { return suggestedBinding; }

    public:
        void suggestBinding(std::string_view binding) {
            suggestedBinding = binding;
        }

    public:
        bool isPressed() const requires IsBoolInput<type> {
            return state.bValue;
        }

        bool wasJustPressed() const requires IsBoolInput<type> {
            return state.bValue && !state.bPreviousValue;
        }

        bool wasJustReleased() const requires IsBoolInput<type> {
            return !state.bValue && state.bPreviousValue;
        }

        float getValue() const requires IsFloatInput<type> {
            return state.fValue;
        }

        float getDelta() const requires IsFloatInput<type> {
            return state.fValue - state.fPreviousValue;
        }

        glm::vec2 getValue() const requires IsVec2Input<type> {
            return state.vValue;
        }

        glm::vec2 getDelta() const requires IsVec2Input<type> {
            return state.vValue - state.vPreviousValue;;
        }

    private:
        std::string name;
        std::string suggestedBinding;
        union {
            struct {
                bool bValue;
                bool bPreviousValue;
            };
            struct {
                float fValue;
                float fPreviousValue;
            };
            struct {
                glm::vec2 vValue;
                glm::vec2 vPreviousValue;
            };
        } state;

#ifdef ENABLE_VR
    private: // OpenXR compatibility
        void createXRAction(xr::ActionSet& set) {
            xr::ActionType xrActionType = xr::ActionType::FloatInput;

            if constexpr(type == ActionType::BoolInput) {
                xrActionType = xr::ActionType::BooleanInput;
            } else if constexpr(type == ActionType::Vec2Input) {
                xrActionType = xr::ActionType::Vector2FInput;
            }

            xr::ActionCreateInfo createInfo;
            std::strcpy(createInfo.actionName, name.c_str());
            std::strcpy(createInfo.localizedActionName, name.c_str());
            createInfo.actionType = xrActionType;
            xrAction = set.createActionUnique(createInfo);
        };

        xr::UniqueAction xrAction;
#endif

        friend class ActionSet;
    };

    using FloatInputAction = Action<ActionType::FloatInput>;
    using BoolInputAction = Action<ActionType::BoolInput>;
    using Vec2InputAction = Action<ActionType::Vec2Input>;

    inline std::string GLFWKeyBinding(int glfwCode) {
        return Carrot::sprintf("/user/glfw/keyboard/%d", glfwCode);
    }

    // TODO: joystick bindings
    // TODO: mouse movement bindings
    // TODO: mouse buttons bindings
    // TODO: OpenXR bindings
}
