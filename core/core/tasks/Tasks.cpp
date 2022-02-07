//
// Created by jglrxavpok on 31/01/2022.
//

#include "Tasks.h"

namespace Carrot::Async {
    TaskLane TaskLane::Undefined;

    std::atomic_uint64_t TaskLane::counter;

    TaskLane::TaskLane() {
        id = ++counter;
    }
}