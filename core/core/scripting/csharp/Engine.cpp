//
// Created by jglrxavpok on 07/03/2023.
//

#include "Engine.h"
#include "mono/metadata/threads.h"
#include <core/io/Logging.hpp>
#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/image.h>
#include <mono/metadata/environment.h>
#include <core/scripting/csharp/Module.h>
#include <mutex>

namespace fs = std::filesystem;

namespace Carrot::Scripting {

    static MonoDomain* rootDomain = nullptr;
    static MonoDomain* appDomain = nullptr;
    static std::mutex runtimeMutex;
    static std::atomic_flag runtimeReady;

    void SomeCppFunction(MonoString* str) {
        char* s = mono_string_to_utf8(str);
        Carrot::Log::info("C# called function with arg= %s", s);
    }

    ScriptingEngine::ScriptingEngine() {
        std::lock_guard l { runtimeMutex };
        if(!runtimeReady.test_and_set()) {
            mono_set_assemblies_path("monolib");

            rootDomain = mono_jit_init("CarrotMonoRuntime");
            if(rootDomain == nullptr) {
                Carrot::Log::error("Could not initialize Mono runtime");
                return;
            }

            std::string domainName = "CarrotAppDomain";
            appDomain = mono_domain_create_appdomain(domainName.data(), nullptr);
            mono_domain_set(appDomain, true);

            // TODO: register internal calls somewhere else
            mono_add_internal_call("DotNetCpp_DotNetProject.Test::SomeCppFunction", SomeCppFunction);

            mono_thread_set_main(mono_thread_current());
        }
        //mono_jit_thread_attach(rootDomain);
    }

    ScriptingEngine::~ScriptingEngine() {
        // From the docs: https://www.mono-project.com/docs/advanced/embedding/
        // Note that for current versions of Mono, the mono runtime can’t be reloaded into the same process, so call mono_jit_cleanup() only if you’re never going to initialize it again.

        // Therefore we don't cleanup anything

/*        mono_domain_unload(appDomain);
        if(auto* rootDomain = mono_get_root_domain()) {
            mono_domain_set(rootDomain, false);
        }*/
        //mono_jit_cleanup(rootDomain);
    }

    std::unique_ptr<Module> ScriptingEngine::loadAssembly(const Carrot::IO::Resource& input) {
        auto bytes = input.readAll();

        MonoImageOpenStatus status;
        MonoImage* image = mono_image_open_from_data_full(reinterpret_cast<char *>(bytes.get()), input.getSize(), true, &status, false);
        if(status != MONO_IMAGE_OK) {
            Carrot::Log::error("Could not open assembly '%s': %s", input.getName().c_str(), mono_image_strerror(status));
            return nullptr;
        }

        CLEANUP(mono_image_close(image));
        MonoAssembly* assembly = mono_assembly_load_from_full(image, input.getName().c_str(), &status, false);

        if(status != MONO_IMAGE_OK) {
            Carrot::Log::error("Could not load assembly '%s': %s", input.getName().c_str(), mono_image_strerror(status));
            return nullptr;
        }

        return std::unique_ptr<Module>(new Module(appDomain, assembly));
    }

    int ScriptingEngine::runExecutable(const Carrot::IO::Resource& exe, std::span<std::string> arguments) {
        auto module = loadAssembly(exe);

        if(!module) {
            return -1;
        }

        return module->executeMain(arguments);
    }

    bool ScriptingEngine::compileFiles(const fs::path& outputAssembly, std::span<fs::path> sourceFiles) {
        const char* sdkPath = std::getenv("MONO_SDK_PATH");
        if(sdkPath == nullptr) {
            Carrot::Log::error("No Mono SDK found, make sure the 'MONO_SDK_PATH' environment variable is set");
            return false;
        }

#ifdef _WIN64
        const fs::path compilerPath = fs::path{sdkPath} / "bin/csc.bat";
#else
        const fs::path compilerPath = fs::path{sdkPath} / "bin/csc";
#endif

        if(!fs::exists(compilerPath)) {
            Carrot::Log::error("No Mono compiler found at %s", compilerPath.string().c_str());
            return false;
        }

#ifdef _WIN64
        // thanks to Windows' system implementation, we have to surround our command line with additional double-quotes
        // https://stackoverflow.com/questions/9964865/c-system-not-working-when-there-are-spaces-in-two-different-parameters
        std::string command = "cmd /S /C \"\"";
#else
        std::string command = "\"";
#endif
        command += Carrot::toString(compilerPath.u8string());
        command += "\"";

        for(const auto& p : sourceFiles) {
            command += " \"";
            command += Carrot::toString(p.u8string());
            command += "\"";
        }
        command += " -target:library";

        command += " -out:";
        command += "\"";
        command += Carrot::toString(outputAssembly.u8string());
        command += "\"";
#ifdef _WIN64
        command += "\"";
#endif

        int ret = system(command.c_str());

        if(ret != 0) {
            Carrot::Log::error("Compiler returned non-zero exit code %d", ret);
            return false;
        }

        return true;
    }
} // Carrot::Scripting