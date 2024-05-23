//
// Created by jglrxavpok on 20/05/2024.
//

#pragma once

namespace Carrot {

    class Time {
    public:
        /// Current time (since start of engine). Recomputed on each call, can be used for profiling
        static double getExactCurrentTime();

        /// Current time (since start of engine). This is updated once per engine loop iteration, not intented for precise measurements
        /// (but should be super fast)
        static double getCurrentTime();

        /// Called by Engine to update value returned by getCurrentTime()
        static void updateTime();
    };

} // Carrot
