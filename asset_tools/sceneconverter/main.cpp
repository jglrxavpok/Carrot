//
// Created by jglrxavpok on 13/02/2025.
//

#include "SceneConverter.h"

#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Invalid usage, expected scene folder" << std::endl;
        return 1;
    }

    fs::path sceneFolder { argv[1] };
    for (auto file : fs::directory_iterator{ sceneFolder }) {
        if (!file.is_regular_file()) {
            continue;
        }

        const fs::path& scene = file.path();
        if (scene.extension() == ".json") {
            convert(scene, sceneFolder);
        }
    }
    return 0;
}