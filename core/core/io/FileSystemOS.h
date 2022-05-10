//
// Created by jglrxavpok on 08/05/2022.
//

#pragma once

#include <filesystem>

// OS-Specific functions

namespace Carrot::IO {
    std::filesystem::path getExecutablePath();
};