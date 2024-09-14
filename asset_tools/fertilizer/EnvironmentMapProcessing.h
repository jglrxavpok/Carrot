//
// Created by jglrxavpok on 04/11/2022.
//

#pragma once

#include <filesystem>
#include <Fertilizer.h>

namespace Fertilizer {
    ConversionResult processEnvironmentMap(const std::filesystem::path& inputFile, const std::filesystem::path& outputFile);
}
