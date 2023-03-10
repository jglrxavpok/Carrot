//
// Created by jglrxavpok on 07/03/2023.
//

#pragma once

#include <mono/utils/mono-forward.h>
#include <core/io/Resource.h>

namespace Carrot::Scripting {
    class Module;

    class ScriptingEngine {
    public:
        explicit ScriptingEngine();
        ~ScriptingEngine();

    public:
        std::unique_ptr<Module> loadAssembly(const Carrot::IO::Resource& input);

    public:
        /// Runs a exe made for Mono (also works on other platforms than Windows)
        int runExecutable(const Carrot::IO::Resource& exe, std::span<std::string> arguments);

        /// Compiles the given source files with CSC.exe inside %MONO_SDK_PATH%/bin.
        bool compileFiles(const std::filesystem::path& outputAssembly, std::span<std::filesystem::path> sourceFiles);
    };

} // Carrot::Scripting
