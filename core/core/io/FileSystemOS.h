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

    /**
     * Adds executable file extension to the given filename, if the filename does not already have it.
     * For example, adds ".exe" to the filename on Windows but does nothing on Linux
     * @param filename the filename to add the extension to
     * @return the modified filename with the added extension
     */
    std::filesystem::path addExecutableExtension(const std::filesystem::path& filename);

    /// Opens the given filepath inside the editor configured by the user
    /// For example, an .sln could open Rider or Visual Studio
    /// Does not sanitize inputs, be careful
    bool openFileInDefaultEditor(const std::filesystem::path& filepath);
};