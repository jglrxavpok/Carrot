//
// Created by jglrxavpok on 07/03/2023.
//

#pragma once

#include <core/io/Resource.h>
#include <core/scripting/csharp/forward.h>
#include "mono/utils/mono-forward.h"

namespace Carrot::Scripting {

    class ScriptingEngine {
    public:
        explicit ScriptingEngine();
        ~ScriptingEngine();

    public:
        /**
         * Creates a new app domain to help with unloading
         */
        std::unique_ptr<CSAppDomain> makeAppDomain(const std::string& domainName);

        /// Loads a given assembly
        std::shared_ptr<CSAssembly> loadAssembly(const Carrot::IO::Resource& input, MonoDomain* appDomain = nullptr);

        bool unloadAssembly(std::shared_ptr<CSAssembly>&& toUnload);

    public:
        /// Runs a exe made for Mono (also works on other platforms than Windows)
        int runExecutable(const Carrot::IO::Resource& exe, std::span<std::string> arguments);

        /// Compiles the given source files with CSC.exe inside %MONO_SDK_PATH%/bin.
        bool compileFiles(const std::filesystem::path& outputAssembly, std::span<std::filesystem::path> sourceFiles, std::span<std::filesystem::path> referenceAssemblies);

        /// Finds a given class inside all currently loaded assemblies
        CSClass* findClass(const std::string& namespaceName, const std::string& className);

        MonoDomain* getRootDomain() const;

    private:
        std::vector<std::weak_ptr<CSAssembly>> loadedAssemblies;
        std::shared_ptr<CSAssembly> mscorlib;
    };

} // Carrot::Scripting
