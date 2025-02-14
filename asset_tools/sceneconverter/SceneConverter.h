//
// Created by jglrxavpok on 13/02/2025.
//

#pragma once

namespace std::filesystem {
    class path;
}

namespace Carrot::SceneConverter {
    void convert(const std::filesystem::path& scenePath, const std::filesystem::path& outputRoot);
}