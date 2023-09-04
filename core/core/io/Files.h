//
// Created by jglrxavpok on 13/10/2021.
//

#pragma once

#include <filesystem>
#include <vector>
#include <span>

namespace Carrot::IO::Files {
    bool isRootFolder(const std::filesystem::path& path);

    std::vector<std::filesystem::path> enumerateRoots();

    struct NativeFileDialogFilter {
        /**
         * Name of this filter
         */
        std::string name;

        /**
         * Comma-separated list of valid extensions
         */
        std::string filters;
    };

    /**
     * Opens a native save file dialog.
     * Returns true iif the user did NOT cancel the save
     * @param savePath if the user selects a path and saves, will be filled with the user's selection
     */
    bool showSaveDialog(std::filesystem::path& savePath, std::span<NativeFileDialogFilter> filters = {}, std::optional<std::string> defaultName = {}, std::optional<std::filesystem::path> defaultPath = {});
}