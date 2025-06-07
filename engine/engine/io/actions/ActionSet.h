//
// Created by jglrxavpok on 25/07/2021.
//

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include "Action.hpp"
#include "core/utils/UUID.h"


namespace Carrot {
    class Engine;
}

namespace Carrot::IO {
    /// Represents a group of Action which can be toggled on or off at once.
    /// For OpenXR compatibility, ALL action sets must be created and filled before the first frame of your game (in your
    /// CarrotGame constructor for instance). Also the name must respect https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#well-formed-path-strings for OpenXR compatibility
    class ActionSet {
    public:
        explicit ActionSet(std::string_view name);
        ~ActionSet();

        bool isActive() const { return active; }
        const std::string& getName() const { return name; }

        void activate();
        void deactivate();

        void add(BoolInputAction& input);

        void add(FloatInputAction& input);

        void add(Vec2InputAction& input);

        void add(PoseInputAction& input);

        void add(VibrationOutputAction& output);

        xr::ActionSet& getXRActionSet();

    public:
        const std::vector<BoolInputAction*>& getBoolInputs() const { return boolInputs; }
        const std::vector<FloatInputAction*>& getFloatInputs() const { return floatInputs; }
        const std::vector<Vec2InputAction*>& getVec2Inputs() const { return vec2Inputs; }
        const std::vector<PoseInputAction*>& getPoseInputs() const { return poseInputs; }
        const std::vector<VibrationOutputAction*>& getVibrationOutputs() const { return vibrationOutputs; }

    public:
        static void updatePrePollAllSets(Carrot::Engine& engine);
        static void updatePostPollAllSets();
        static void resetAllDeltas();
        static void syncXRActions();
        static std::vector<ActionSet*>& getSetList();

    private:
        // TODO: actually allow remapping, for the moment only use suggested binding
        const std::vector<ActionBinding>& getMappedBindings(auto& action) {
            return action->getSuggestedBindings();
        };

        void updatePrePoll();
        void updatePostPoll();
        void resetDeltas();
        void prepareForUse(Carrot::Engine& engine);
        void pollXRActions();

    private:
        bool readyForUse = false;
        bool active = false;
        std::string name;
        std::vector<FloatInputAction*> floatInputs;
        std::vector<BoolInputAction*> boolInputs;
        std::vector<Vec2InputAction*> vec2Inputs;
        std::vector<PoseInputAction*> poseInputs;
        std::vector<VibrationOutputAction*> vibrationOutputs;

        // action "path" to state. Needs this indirection for multi-key inputs
        std::unordered_map<Identifier, InputState> inputStates;

        Carrot::UUID keyCallback;
        Carrot::UUID gamepadButtonCallback;
        Carrot::UUID gamepadAxisCallback;
        Carrot::UUID gamepadVec2Callback;
        Carrot::UUID keysVec2Callback;
        Carrot::UUID mouseButtonCallback;
        Carrot::UUID mousePositionCallback;
        Carrot::UUID mouseDeltaCallback;
        Carrot::UUID mouseDeltaGrabbedCallback;
        Carrot::UUID mouseWheelCallback;

    private: // OpenXR compatibility
        xr::UniqueActionSet xrActionSet;
    };
}