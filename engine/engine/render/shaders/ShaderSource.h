//
// Created by jglrxavpok on 28/11/2021.
//

#pragma once

#include <core/io/Resource.h>
#include <core/io/FileWatcher.h>

namespace Carrot::Render {
    // Allows to create shader modules
    //  In the case of file-based shaders, this does NOT keep the file open
    class ShaderSource {
    public:
        ShaderSource(const Carrot::IO::Resource& resource);
        ShaderSource(const std::string& filename);
        ShaderSource(const char* filename);
        ShaderSource();
        ShaderSource(const ShaderSource& toCopy);

        std::vector<std::uint8_t> getCode() const;
        std::string getName() const;

        ShaderSource& operator=(const ShaderSource& toCopy) = default;

        explicit operator bool() const;

    public:
        bool hasSourceBeenModified() const;
        void clearModifyFlag();

    private:
        void createWatcher();

        bool fromFile = false;
        bool sourceModified = false;
        std::filesystem::path filepath;
        std::vector<std::uint8_t> rawData;
        std::shared_ptr<IO::FileWatcher> watcher;
    };
}