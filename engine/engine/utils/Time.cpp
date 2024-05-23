//
// Created by jglrxavpok on 20/05/2024.
//

#include "Time.h"

namespace Carrot {
    static std::chrono::steady_clock::time_point startTime;
    static double currentTime = 0.0;

    double Time::getCurrentTime() {
        return currentTime;
    }

    double Time::getExactCurrentTime() {
        return std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();
    }

    void Time::updateTime() {
        currentTime = getExactCurrentTime();
    }


} // Carrot