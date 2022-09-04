//
// Created by jglrxavpok on 04/09/2022.
//
#include "Action.hpp"
#include "engine/utils/Macros.h"
#include "engine/vr/Session.h"
#include "engine/Engine.h"

namespace Carrot::IO {

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