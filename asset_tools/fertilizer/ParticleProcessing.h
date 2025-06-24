//
// Created by jglrxavpok on 24/06/25.
//

#pragma once

#include <filesystem>
#include <Fertilizer.h>

namespace Fertilizer {
    ConversionResult processParticleFile(const std::filesystem::path& inputFile, const std::filesystem::path& outputFile);
} // Fertilizer