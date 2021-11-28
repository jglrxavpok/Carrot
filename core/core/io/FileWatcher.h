//
// Created by jglrxavpok on 28/11/2021.
//

#pragma once

#include <filesystem>
#include <string>
#include <functional>
#include <vector>
#include "core/data/Hashes.h"

namespace Carrot::IO {
    class FileWatcher {
    public:
        using Action = std::function<void(const std::filesystem::path&)>;

        // Creates a new file watcher which will react to modifications inside files in 'filesToWatch'
        explicit FileWatcher(const Action& action, const std::vector<std::filesystem::path>& filesToWatch);

        // Checks if files have been modified, and calls the action if it is the case
        void tick();

    private:
        std::vector<std::filesystem::path> files;
        std::unordered_map<std::filesystem::path, std::filesystem::file_time_type> timestamps;
        Action action;
    };
}
