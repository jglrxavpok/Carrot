//
// Created by jglrxavpok on 20/03/2023.
//

#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include <core/io/Resource.h>

namespace Carrot::Scripting {

    /**
     * Very very VERY simple parser of .csproj files
     * Basically just read which files to compile and THAT'S IT
     */
    class CSProject {
    public:
        /**
         * Loads a .csproj file represented by 'csprojFile'
         */
        explicit CSProject(Carrot::IO::Resource csprojFile);

        /**
         * Collection of files to compile
         */
        std::span<const std::filesystem::path> getSourceFiles() const;

    private:
        std::vector<std::filesystem::path> sourceFiles;
    };

} // Carrot::Scripting
