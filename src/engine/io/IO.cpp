//
// Created by jglrxavpok on 21/11/2020.
//

#include "IO.h"
#include <fstream>
#include <iostream>

std::vector<uint8_t> IO::readFile(const std::string& filename) {
    if(!std::filesystem::exists(filename))
        throw std::runtime_error("File does not exist: " + filename);

    // start at end of file to get length directly
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    size_t filesize = static_cast<size_t>(file.tellg());

    std::vector<uint8_t> contents(filesize);
    file.seekg(0);

    file.read(reinterpret_cast<char*>(contents.data()), filesize);

    file.close();

    return contents;
}

std::string IO::readFileAsText(const std::string& filename) {
    if(!std::filesystem::exists(filename))
        throw std::runtime_error("File does not exist: " + filename);

    std::ifstream file(filename, std::ios::in);

    std::string contents;

    std::string line;
    while(getline(file, line)) {
        if(!contents.empty()) {
            contents += '\n';
        }
        contents += line;
    }
    return std::move(contents);
}

void IO::writeFile(const std::string& filename, void* ptr, size_t length) {
    std::ofstream file(filename, std::ios::out | std::ios::binary);
    file.write(static_cast<const char *>(ptr), length);
}