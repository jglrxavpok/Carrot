//
// Created by jglrxavpok on 21/11/2020.
//

#include "IO.h"
#include <fstream>

std::vector<uint8_t> IO::readFile(const std::string& filename) {
    // start at end of file to get length directly
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    size_t filesize = static_cast<size_t>(file.tellg());

    std::vector<uint8_t> contents(filesize);
    file.seekg(0);

    file.read(reinterpret_cast<char*>(contents.data()), filesize);

    file.close();

    return contents;
}