//
// Created by jglrxavpok on 20/06/2021.
//

#pragma once

namespace Carrot::Render {
    enum class Eye {
        LeftEye,
        RightEye,
        NoVR = LeftEye,
    };

    struct Context {
        Eye eye = Eye::NoVR;
        size_t frameCount = -1;
        size_t swapchainIndex = -1;
        size_t lastSwapchainIndex = -1;
    };
}