//
// Created by jglrxavpok on 04/11/2022.
//

#include <TextureCompression.h>
#include <iostream>
#include <thread>
#include "core/Macros.h"
#include "core/utils/stringmanip.h"

int main(int argc, char** argv) {
    auto start = std::chrono::steady_clock::now();

    std::ios::sync_with_stdio(false);

    bool recursive = false;
    bool valid = true;

    bool hasInput = false;
    bool hasOutput = false;
    bool forceConvert = false;
    std::filesystem::path inputFile;
    std::filesystem::path outputFile;
    for (int i = 1; i < argc;) {
        const std::string_view arg = argv[i];
        if(arg == "-r" || arg == "--recursive") {
            recursive = true;
        } else if(arg == "-f" || arg == "--force") {
            forceConvert = true;
        } else {
            if(!hasInput) {
                inputFile = arg;
                hasInput = true;
            } else if(!hasOutput) {
                outputFile = arg;
                hasOutput = true;
            } else {
                std::cerr << "Unrecognized argument: " << arg << std::endl;
                valid = false;
            }
        }
        i++;
    }

    if(!valid) {
        return 1;
    }

    if(!hasInput) {
        std::cerr << "Missing input file." << std::endl;
        return 2;
    } else if(!std::filesystem::exists(inputFile)) {
        std::cerr << "Input file does not exist." << std::endl;
        return 4;
    }

    if(!hasOutput) {
        std::cerr << "Missing output file." << std::endl;
        return 4;
    }

    std::vector<std::filesystem::path> allInputs; // recursive option
    std::vector<std::filesystem::path> allOutputs; // recursive option
    if(recursive) {
        for(const auto& entry : std::filesystem::recursive_directory_iterator(inputFile)) {
            if(!Fertilizer::isSupportedFormat(entry.path())) {
                continue;
            }

            const std::filesystem::path relative = std::filesystem::relative(entry.path(), inputFile);

            allInputs.emplace_back(entry.path());
            allOutputs.emplace_back(outputFile / Fertilizer::makeOutputPath(relative));
        }
    } else {
        allInputs.push_back(inputFile);
        allOutputs.push_back(outputFile);
    }

    int errorCode = 0;
    const unsigned int maxThreads = std::thread::hardware_concurrency();
    const unsigned int stepSize = ceil(allInputs.size() / (float)maxThreads);

    std::vector<std::thread> threads;
    threads.reserve(maxThreads);
    for(std::size_t i = 0; i < allInputs.size(); i += stepSize) {
        threads.emplace_back([&, i]()
        {
            for (int j = 0; j < stepSize; ++j) {
                unsigned int index = j + i;
                if(index >= allInputs.size()) {
                    continue;
                }

                const auto& input = allInputs[index];
                const auto& output = allOutputs[index];
                std::cout << Carrot::sprintf("Converting %s (%llu / %llu)\n", input.string().c_str(), index+1, allInputs.size());
                Fertilizer::ConversionResult result = Fertilizer::convert(input, output, forceConvert);
                switch(result.errorCode) {
                    case Fertilizer::ConversionResultError::Success:
                        break;

                    default:
                        errorCode = -1;
                        std::cerr << "[" << input << "] Conversion failed: " << result.errorMessage << std::endl;
                        break;
                }
            }
        });
    }

    for(auto& t : threads) {
        t.join();
    }

    float duration = duration_cast<std::chrono::duration<float>>((std::chrono::steady_clock::now() - start)).count();
    std::cout << "Took " << duration << " seconds." << std::endl;

    return errorCode;
}