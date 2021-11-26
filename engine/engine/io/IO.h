//
// Created by jglrxavpok on 21/11/2020.
//

#pragma once
#include <vector>
#include <string>
#include <filesystem>
#include <functional>

namespace Carrot::IO {
    /// Reads the contents of a file as a list of bytes
    std::vector<uint8_t> readFile(const std::string& filename);

    /// Reads the content of a file as a string
    std::string readFileAsText(const std::string& filename);

    void writeFile(const std::string& filename, void* ptr, size_t length);

    using WriteToFileFunction = std::function<void(std::ofstream& out)>;

    void writeFile(const std::string& filename, WriteToFileFunction function);
}
