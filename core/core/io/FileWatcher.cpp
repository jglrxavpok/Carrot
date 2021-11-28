//
// Created by jglrxavpok on 28/11/2021.
//

#include "FileWatcher.h"

namespace Carrot::IO {
    FileWatcher::FileWatcher(const Action& action, const std::vector<std::filesystem::path>& filesToWatch): action(action), files() {
        for(const auto& p : filesToWatch) {
            auto fullPath = std::filesystem::absolute(p);
            files.push_back(fullPath);
            timestamps[fullPath] = std::filesystem::last_write_time(fullPath);
        }
    }

    void FileWatcher::tick() {
        for(const auto& p : files) {
            auto timestamp = std::filesystem::last_write_time(p);
            if(timestamp > timestamps[p]) {
                timestamps[p] = timestamp;
                action(p);
            }
        }
    }
}