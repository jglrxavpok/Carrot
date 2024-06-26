//
// Created by jglrxavpok on 08/05/2022.
//

#pragma once

#include <filesystem>

// OS-Specific functions

namespace Carrot::IO {
    std::filesystem::path getExecutablePath();

    /// Opens the given filepath inside the editor configured by the user
    /// For example, an .sln could open Rider or Visual Studio
    bool openFileInDefaultEditor(const std::filesystem::path& filepath);
};