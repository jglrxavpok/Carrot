//
// Created by jglrxavpok on 28/11/2021.
//

#include "FileWatcher.h"
#include "core/io/Logging.hpp"

namespace Carrot::IO {
    FileWatcher::FileWatcher(const Action& action, const std::vector<std::filesystem::path>& filesToWatch): action(action), files() {
        for(const auto& p : filesToWatch) {
            std::error_code ec;
            auto fullPath = std::filesystem::absolute(p, ec);
            if(ec) {
                throw std::runtime_error(Carrot::sprintf("Got error when converting %s to absolute path: 0x%x", p.u8string().c_str(), ec.value()));
            }

            if(std::filesystem::exists(fullPath)) {
                files.push_back(fullPath);
                timestamps[fullPath] = std::filesystem::last_write_time(fullPath, ec);
                if(ec) {
                    throw std::runtime_error(Carrot::sprintf("Got error when accessing last_write_time of %s: 0x%x", fullPath.u8string().c_str(), ec.value()));
                }
            } else {
                Carrot::Log::warn("Tried to watch file '%s' but it does not exist.", fullPath.u8string().c_str());
            }
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