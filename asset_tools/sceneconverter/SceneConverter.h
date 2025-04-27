//
// Created by jglrxavpok on 13/02/2025.
//

#pragma once
#include <filesystem>

namespace Carrot::SceneConverter {
    void convert(const std::filesystem::path& scenePath, const std::filesystem::path& outputRoot);
}