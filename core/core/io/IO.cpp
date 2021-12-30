//
// Created by jglrxavpok on 21/11/2020.
//

#include "IO.h"
#include "core/utils/stringmanip.h"
#include <fstream>
#include <iostream>

namespace Carrot::IO {
    std::vector<uint8_t> readFile(const std::string& filename) {
        if (!std::filesystem::exists(filename))
            throw std::runtime_error("File does not exist: " + filename);

        // start at end of file to get length directly
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        size_t filesize = static_cast<size_t>(file.tellg());

        std::vector<uint8_t> contents(filesize);
        file.seekg(0);

        file.read(reinterpret_cast<char *>(contents.data()), filesize);

        file.close();

        return contents;
    }

    std::string readFileAsText(const std::string& filename) {
        if (!std::filesystem::exists(filename))
            throw std::runtime_error("File does not exist: " + filename);

        std::ifstream file(filename, std::ios::in);

        std::string contents;

        std::string line;
        while (getline(file, line)) {
            if (!contents.empty()) {
                contents += '\n';
            }
            contents += line;
        }
        return std::move(contents);
    }

    void writeFile(const std::string& filename, void *ptr, size_t length) {
        std::ofstream file(filename, std::ios::out | std::ios::binary);
        file.write(static_cast<const char *>(ptr), length);
    }

    void writeFile(const std::string& filename, IO::WriteToFileFunction function) {
        std::ofstream file(filename, std::ios::out | std::ios::binary);
        function(file);
    }

    std::string getHumanReadableFileSize(std::size_t filesize) {
        const std::size_t kiB = 1024;
        const std::size_t MiB = 1024 * kiB;
        const std::size_t GiB = 1024 * MiB;
        if(filesize < kiB) {
            return Carrot::sprintf("%llu B", static_cast<std::uint64_t>(filesize));
        }

        if(filesize < MiB) {
            return Carrot::sprintf("%0.2f kiB", static_cast<float>(filesize) / kiB);
        }

        if(filesize < GiB) {
            return Carrot::sprintf("%0.2f MiB", static_cast<float>(filesize) / MiB);
        }

        return Carrot::sprintf("%0.2f GiB", static_cast<float>(filesize) / GiB);
    }
}