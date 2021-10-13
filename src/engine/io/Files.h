//
// Created by jglrxavpok on 13/10/2021.
//

#pragma once

#include <filesystem>
#include <vector>

namespace Carrot::IO::Files {
    bool isRootFolder(const std::filesystem::path& path);

    std::vector<std::filesystem::path> enumerateRoots();
}