//
// Created by jglrxavpok on 08/05/2022.
//

#pragma once

#include <filesystem>

// OS-Specific functions

namespace Carrot::IO {
    /// Checks if there is an executable file named 'exeName' (or exeName.exe on Windows) in the current working directory
    bool hasExecutableInWorkingDirectory(const char* exeName);

    std::filesystem::path getExecutablePath();

    /// Opens the given filepath inside the editor configured by the user
    /// For example, an .sln could open Rider or Visual Studio
    /// Does not sanitize inputs, be careful
    bool openFileInDefaultEditor(const std::filesystem::path& filepath);
};