//
// Created by jglrxavpok on 27/10/2022.
//

#pragma once

#include "LoadedScene.h"
#include "core/io/Resource.h"

namespace Carrot::Render {
    class GLTFLoader {
    public:
        LoadedScene load(const Carrot::IO::Resource& resource);

    private:

    };
}