//
// Created by jglrxavpok on 17/12/2022.
//

#pragma once

#include <core/utils/CarrotTinyGLTF.h>
#include <core/scene/LoadedScene.h>

namespace Fertilizer {
    tinygltf::Model writeAsGLTF(const std::string& modelName, const Carrot::Render::LoadedScene& scene);
}