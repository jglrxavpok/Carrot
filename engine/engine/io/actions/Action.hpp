//
// Created by jglrxavpok on 25/07/2021.
//

#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtx/quaternion.hpp>
#include <string>
#include <string_view>
#include "core/utils/stringmanip.h"
#include "engine/io/actions/InputVectors.h"
#include "engine/vr/includes.h"
#include "core/utils/Assert.h"
#include <core/containers/Vector.hpp>
#include <core/utils/Identifier.h>

namespace Carrot::VR {
    class Session;
}

namespace Carrot::IO {
    enum class ActionType {
        FloatInput,
        BoolInput,
        Vec2Input,

        PoseInput,
        VibrationOutput,
    };

    union InputState {
        struct BoolState {
            bool value;
            bool previousValue;
        } b;
        struct FloatValue {
            float value;
            float previousValue;
        } f;
        struct Vec2Value {
            glm::vec2 value;
            glm::vec2 previousValue;
        } v2;
    };

    //! Represents a pose that an action can return (in most cases, the pose of the VR controllers)
    struct ActionPose {
        glm::vec3 position{0.0f};
        glm::quat orientation = glm::identity<glm::quat>();
    };

    //! Represents a binding for an action.
    struct ActionBinding {
        static constexpr const char* const CarrotInteractionProfile = "/carrot";

        //! Interaction profile to suggest this action for.
        //! Used for OpenXR (https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#semantic-path-interaction-profiles). Unused for non-OpenXR bindings
        std::string interactionProfile = CarrotInteractionProfile;

        //! Paths of the suggested binding (for multiple buttons)
        Carrot::Vector<Identifier> paths;

        ActionBinding() = default;

        //! Creates a non-OpenXR binding with the given path
        ActionBinding(Identifier p);

        //! Creates a non-OpenXR binding with the given paths, one for each required input (ie use this for multi-button inputs)
        ActionBinding(std::initializer_list<Identifier> paths);

        bool isOpenXR() const;

        bool operator==(const ActionBinding& other) const;

        ActionBinding operator+(const ActionBinding& other) const;
    };

    template<ActionType type>
    concept IsFloatInput = type == ActionType::FloatInput;
    template<ActionType type>
    concept IsBoolInput = type == ActionType::BoolInput;
    template<ActionType type>
    concept IsVec2Input = type == ActionType::Vec2Input;
    template<ActionType type>
    concept IsPoseInput = type == ActionType::PoseInput;
    template<ActionType type>
    concept IsVibrationOutput = type == ActionType::VibrationOutput;

    namespace Details {
        // bridge methods to avoid including more than necessary

        void internalVibrate(xr::Action& action, std::int64_t durationInNanoSeconds, float frequency, float amplitude);
        void internalStopVibration(xr::Action& action);
    }

    template<ActionType type>
    class Action {
    public:
        explicit Action(std::string_view name): name(name) {
            std::memset(&state, 0, sizeof(state));
        }

        const std::string& getName() const { return name; }
        const std::vector<ActionBinding>& getSuggestedBindings() const { return suggestedBindings; }

        xr::Action& getXRAction() {
            verify(xrAction, "Cannot call getXRAction without calling createXRAction first");
            return *xrAction;
        }

    public:
        void suggestBinding(ActionBinding binding) {
            verify(binding.paths.size() == 1 || type == ActionType::BoolInput, "Only BoolInput supports multi-inputs");
            suggestedBindings.emplace_back(std::move(binding));
        }

    public:
        bool isPressed() const requires IsBoolInput<type> {
            return state.b.value;
        }

        bool wasJustPressed() const requires IsBoolInput<type> {
            return state.b.value && !state.b.previousValue;
        }

        bool wasJustReleased() const requires IsBoolInput<type> {
            return !state.b.value && state.b.previousValue;
        }

        float getValue() const requires IsFloatInput<type> {
            return state.f.value;
        }

        float getDelta() const requires IsFloatInput<type> {
            return state.f.value - state.f.previousValue;
        }

        glm::vec2 getValue() const requires IsVec2Input<type> {
            if (glm::length2(state.v2.value) < deadzone*deadzone) {
                return glm::vec2 { 0.0f, 0.0f };
            }
            return state.v2.value;
        }

        glm::vec2 getDelta() const requires IsVec2Input<type> {
            return getValue() - state.v2.previousValue;
        }

        void setDeadzone(float deadzone) requires IsVec2Input<type> {
            this->deadzone = deadzone;
        }

        const ActionPose& getValue() const requires IsPoseInput<type> {
            return poseState.pValue;
        }

        const ActionPose& getPreviousValue() const requires IsPoseInput<type> {
            return poseState.pPreviousValue;
        }

        void forceValue(bool v) requires IsBoolInput<type> {
            state.b.value = v;
        }

        void forceValue(float v) requires IsFloatInput<type> {
            state.f.value = v;
        }

        void forceValue(const glm::vec2& v) requires IsVec2Input<type> {
            state.v2.value = v;
        }

        void forceValue(const ActionPose& v) requires IsPoseInput<type> {
            poseState.pValue = v;
        }

        //! Creates a vibration of given duration, frequency and amplitude.
        //! Backed by OpenXR, check https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XrHapticVibration for more details
        void vibrate(std::int64_t durationInNanoSeconds, float frequency, float amplitude) requires IsVibrationOutput<type> {
            Details::internalVibrate(*xrAction, durationInNanoSeconds, frequency, amplitude);
        }

        //! Stops a vibration.
        //! Backed by OpenXR, check https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XrHapticVibration for more details
        void stopVibration() requires IsVibrationOutput<type> {
            Details::internalStopVibration(*xrAction);
        }

    private:
        std::string name;
        std::vector<ActionBinding> suggestedBindings;
        InputState state{};
        float deadzone = 0.0f;

        // non-trivial constructor so keeping it inside the union is not great
        struct {
            ActionPose pValue;
            ActionPose pPreviousValue;
        } poseState;

    private: // OpenXR compatibility
        void createXRAction(xr::ActionSet& set) {
            xr::ActionType xrActionType = xr::ActionType::FloatInput;

            if constexpr(type == ActionType::BoolInput) {
                xrActionType = xr::ActionType::BooleanInput;
            } else if constexpr(type == ActionType::Vec2Input) {
                xrActionType = xr::ActionType::Vector2FInput;
            } else if constexpr(type == ActionType::FloatInput) {
                xrActionType = xr::ActionType::FloatInput;
            } else if constexpr(type == ActionType::PoseInput) {
                xrActionType = xr::ActionType::PoseInput;
            } else if constexpr(type == ActionType::VibrationOutput) {
                xrActionType = xr::ActionType::VibrationOutput;
            } else {
                verify(false, "Missing case");
            }

            xr::ActionCreateInfo createInfo;
            std::strcpy(createInfo.actionName, name.c_str());
            std::strcpy(createInfo.localizedActionName, name.c_str());
            createInfo.actionType = xrActionType;
            xrAction = set.createActionUnique(createInfo);
        };

        void setXRSpace(xr::UniqueSpace&& space) {
            xrSpace = std::move(space);
        }

        xr::UniqueAction xrAction;
        xr::UniqueSpace xrSpace; // only used for Pose inputs

        friend class ActionSet;
        friend class Carrot::VR::Session;
    };

    using FloatInputAction = Action<ActionType::FloatInput>;
    using BoolInputAction = Action<ActionType::BoolInput>;
    using Vec2InputAction = Action<ActionType::Vec2Input>;
    using PoseInputAction = Action<ActionType::PoseInput>;
    using VibrationOutputAction = Action<ActionType::VibrationOutput>;

    inline ActionBinding GLFWKeyBinding(int glfwCode) {
        return Identifier { Carrot::sprintf("/user/glfw/keyboard/%d", glfwCode) };
    }

    inline ActionBinding GLFWMouseButtonBinding(int buttonID) {
        return Identifier { Carrot::sprintf("/user/glfw/mouse/%d", buttonID) };
    }

    inline ActionBinding GLFWGamepadButtonBinding(int gamepadID, int buttonID) {
        return Identifier { Carrot::sprintf("/user/glfw/gamepad/%d/button/%d", gamepadID, buttonID) };
    }

    inline ActionBinding GLFWGamepadAxisBinding(int gamepadID, int axisID) {
        return Identifier { Carrot::sprintf("/user/glfw/gamepad/%d/axis/%d", gamepadID, axisID) };
    }

    inline ActionBinding GLFWGamepadVec2Binding(int gamepadID, Carrot::IO::GameInputVectorType vectorType) {
        return Identifier { Carrot::sprintf("/user/glfw/gamepad/%d/vec2/%d", gamepadID, vectorType) };
    }

    inline ActionBinding GLFWKeysVec2Binding(Carrot::IO::GameInputVectorType vectorType) {
        return Identifier { Carrot::sprintf("/user/glfw/keys/vec2/%d", vectorType) };
    }

    inline ActionBinding GLFWMousePositionBinding() { return Identifier { "/user/glfw/mouse/pos" }; };
    inline ActionBinding GLFWMouseDeltaBinding() { return Identifier { "/user/glfw/mouse/delta" }; };
    inline ActionBinding GLFWMouseWheel() { return Identifier { "/user/glfw/mouse/wheel" }; };
    inline ActionBinding GLFWGrabbedMouseDeltaBinding() { return Identifier { "/user/glfw/mouse/delta_grabbed" }; };

    // Input profiles
    constexpr static const char* const SimpleController = "/interaction_profiles/khr/simple_controller";
    constexpr static const char* const DaydreamController = "/interaction_profiles/google/daydream_controller";
    constexpr static const char* const ViveController = "/interaction_profiles/htc/vive_controller";
    constexpr static const char* const VivePro = "/interaction_profiles/htc/vive_pro";
    constexpr static const char* const MotionController = "/interaction_profiles/microsoft/motion_controller";
    constexpr static const char* const XboxController = "/interaction_profiles/microsoft/xbox_controller";
    constexpr static const char* const GoController = "/interaction_profiles/oculus/go_controller";
    constexpr static const char* const TouchController = "/interaction_profiles/oculus/touch_controller";
    constexpr static const char* const IndexController = "/interaction_profiles/valve/index_controller";

    inline ActionBinding OpenXRBinding(const std::string& interactionProfile, const std::string& path) {
        ActionBinding binding;
        binding.interactionProfile = interactionProfile;
        binding.paths.emplaceBack(path);
        return binding;
    }
}
