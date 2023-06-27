//
// Created by jglrxavpok on 27/12/2021.
//

#pragma once

namespace Carrot::Render {
    enum class PassEnum {
        Undefined = 0,
        OpaqueGBuffer,
        TransparentGBuffer,
        Lighting,
        Unlit, // rendered after lighting but before tonemapping
        UI, // TODO: not implemented yet
    };
}