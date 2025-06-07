//
// Created by jglrxavpok on 04/09/2022.
//
#include "Action.hpp"
#include "engine/utils/Macros.h"
#include "engine/vr/Session.h"
#include "engine/Engine.h"

namespace Carrot::IO {

    ActionBinding::ActionBinding(Identifier p) {
        paths.emplaceBack(std::move(p));
    }

    ActionBinding::ActionBinding(std::initializer_list<Identifier> pathList) {
        paths.ensureReserve(pathList.size());
        for (auto& id : pathList) {
            paths.emplaceBack(std::move(id));
        }
    }

    bool ActionBinding::isOpenXR() const {
        return interactionProfile != CarrotInteractionProfile;
    }

    bool ActionBinding::operator==(const ActionBinding& other) const {
        return interactionProfile == other.interactionProfile && paths == other.paths;
    }

    ActionBinding ActionBinding::operator+(const ActionBinding& other) const {
        verify(interactionProfile == other.interactionProfile, "incompatible interaction profiles");
        ActionBinding result{};
        result.interactionProfile = interactionProfile;
        result.paths.setCapacity(paths.size() + other.paths.size());
        result.paths = paths;
        for (const auto& p : other.paths) {
            result.paths.pushBack(p);
        }

        return result;
    }


    namespace Details {
        void internalVibrate(xr::Action& action, std::int64_t durationInNanoSeconds, float frequency, float amplitude) {
            xr::HapticVibration feedback;
            feedback.amplitude = amplitude;
            feedback.frequency = frequency;
            feedback.duration = xr::Duration((XrDuration)durationInNanoSeconds);
            GetEngine().getVRSession().vibrate(action, feedback);
        }

        void internalStopVibration(xr::Action& action) {
            GetEngine().getVRSession().stopVibration(action);
        }
    }
}