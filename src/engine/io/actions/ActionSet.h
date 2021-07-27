//
// Created by jglrxavpok on 25/07/2021.
//

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include "Action.hpp"

#ifdef ENABLE_VR
#include "engine/vr/includes.h"
#endif

namespace Carrot {
    class Engine;
}

namespace Carrot::IO {
    /// Represents a group of Action which can be toggled on or off at once.
    class ActionSet {
    public:
        explicit ActionSet(std::string_view name, bool isXRSet = false);
        ~ActionSet();

        bool isActive() const { return active; }
        const std::string& getName() const { return name; }

        void activate();
        void deactivate();

        void add(BoolInputAction& input) {
            boolInputs.push_back(&input);
        }

        void add(FloatInputAction& input) {
            floatInputs.push_back(&input);
        }

        void add(Vec2InputAction& input) {
            vec2Inputs.push_back(&input);
        }

    public:
        const std::vector<BoolInputAction*>& getBoolInputs() const { return boolInputs; }
        const std::vector<FloatInputAction*>& getFloatInputs() const { return floatInputs; }
        const std::vector<Vec2InputAction*>& getVec2Inputs() const { return vec2Inputs; }

    public:
        static void updatePrePollAllSets(Carrot::Engine& engine, bool setPreviousValues);
        static std::vector<ActionSet*>& getSetList();

    private:
        void updatePrePoll(bool setPreviousValues);
        void prepareForUse(Carrot::Engine& engine);

    private:

        bool readyForUse = false;
        bool isXRSet = false;
        bool active = false;
        std::string name;
        std::vector<FloatInputAction*> floatInputs;
        std::vector<BoolInputAction*> boolInputs;
        std::vector<Vec2InputAction*> vec2Inputs;

#ifdef ENABLE_VR
    private: // OpenXR compatibility
        xr::UniqueActionSet xrActionSet;
#endif
    };
}