//
// Created by jglrxavpok on 07/03/2023.
//

#include "Engine.h"

#ifdef __linux__
#include <dlfcn.h>
#endif

#include "mono/metadata/threads.h"
#include "mono/metadata/mono-debug.h"
#include <mono/metadata/mono-config.h>
#include <mono/utils/mono-logger.h>
#include <core/io/Logging.hpp>
#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/image.h>
#include <mono/metadata/environment.h>
#include <core/scripting/csharp/CSAppDomain.h>
#include <core/scripting/csharp/CSAssembly.h>
#include <mutex>

namespace fs = std::filesystem;

namespace Carrot::Scripting {

    static MonoDomain* rootDomain = nullptr;
    static MonoDomain* defaultAppDomain = nullptr;
    static std::mutex runtimeMutex;
    static std::atomic_flag runtimeReady;

    ScriptingEngine::ScriptingEngine() {
        std::lock_guard l { runtimeMutex };
        if(!runtimeReady.test_and_set()) {
            //mono_trace_set_level_string("debug");

            std::filesystem::path monoInstallPath = std::filesystem::current_path();
            monoInstallPath /= "mono-install";
            std::filesystem::path monoLibsPath = monoInstallPath / "lib/mono/4.5";
            mono_set_assemblies_path(monoLibsPath.c_str());

            static const char* options[] = {
                    "--soft-breakpoints",
                    "--debugger-agent=transport=dt_socket,address=127.0.0.1:10000,server=y,suspend=n"
            };

            mono_jit_parse_options(sizeof(options)/sizeof(char*), (char**)options);

            fs::path configPath = monoInstallPath;
            configPath /= "etc/mono/config";
            mono_set_dirs((fs::path{monoInstallPath} / "lib").c_str(), (fs::path{monoInstallPath} / "etc").c_str());
            mono_config_parse(nullptr);

#ifdef __linux__
            // Necessary for loading libmono-native, otherwise complains about undefined symbols
            std::filesystem::path dynamicLibPath = monoInstallPath / "lib/libmonosgen-2.0.so";
            libMonoDynamicLib = dlopen(dynamicLibPath.c_str(), RTLD_NOW | RTLD_GLOBAL);
            verify(libMonoDynamicLib, "Could not load 'mono-install/lib/libmonosgen-2.0.so'");
#endif

            mono_debug_init(MONO_DEBUG_FORMAT_MONO);

            rootDomain = mono_jit_init("CarrotMonoRuntime");
            if(rootDomain == nullptr) {
                Carrot::Log::error("Could not initialize Mono runtime");
                return;
            }

            mono_debug_domain_create(rootDomain);

            std::string appDomainName = "Carrot Default AppDomain";
            defaultAppDomain = mono_domain_create_appdomain(appDomainName.data(), nullptr);
            mono_domain_set(defaultAppDomain, true);

            mono_thread_set_main(mono_thread_current());
        }

        mscorlib = std::shared_ptr<CSAssembly>(new CSAssembly(defaultAppDomain, mono_get_corlib()));
        loadedAssemblies.push_back(mscorlib);
        //mono_jit_thread_attach(rootDomain);
    }

    std::unique_ptr<CSAppDomain> ScriptingEngine::makeAppDomain(const std::string& domainName) {
        std::string nameCopy = domainName;
        MonoDomain* appDomain = mono_domain_create_appdomain(nameCopy.data(), nullptr);
        mono_debug_domain_create(appDomain);
        return std::make_unique<CSAppDomain>(appDomain);
    }

    bool ScriptingEngine::unloadAssembly(std::shared_ptr<CSAssembly>&& toUnload) {
        bool found = false;
        std::erase_if(loadedAssemblies, [&](const std::weak_ptr<CSAssembly>& element) {
            if(auto e = element.lock()) {
                bool isSame = e.get() == toUnload.get();
                found |= isSame;
                return isSame;
            }
            return true;
        });
        toUnload = nullptr;
        return found;
    }

    ScriptingEngine::~ScriptingEngine() {
#ifdef __linux__
        dlclose(libMonoDynamicLib);
#endif
        // From the docs: https://www.mono-project.com/docs/advanced/embedding/
        // Note that for current versions of Mono, the mono runtime can’t be reloaded into the same process, so call mono_jit_cleanup() only if you’re never going to initialize it again.

        // Therefore we don't cleanup anything

/*        mono_domain_unload(appDomain);
        if(auto* rootDomain = mono_get_root_domain()) {
            mono_domain_set(rootDomain, false);
        }*/
        //mono_jit_cleanup(rootDomain);
    }

    void ScriptingEngine::setRootDomainAsMain() {
        mono_domain_set(rootDomain, false);
    }

    std::shared_ptr<CSAssembly> ScriptingEngine::loadAssembly(const Carrot::IO::Resource& input,
                                                              MonoDomain* appDomain,
                                                              std::optional<Carrot::IO::Resource> symbolInput) {
        if(appDomain == nullptr) {
            appDomain = defaultAppDomain;
        }

        auto bytes = input.readAll();

        MonoImageOpenStatus status;
        MonoImage* image = mono_image_open_from_data_full(reinterpret_cast<char *>(bytes.get()), input.getSize(), true, &status, false);
        if(status != MONO_IMAGE_OK) {
            Carrot::Log::error("Could not open assembly '%s': %s", input.getName().c_str(), mono_image_strerror(status));
            return nullptr;
        }

        if(symbolInput.has_value()) {
            auto pdbBytes = symbolInput->readAll();
            mono_debug_open_image_from_memory(image, pdbBytes.get(), symbolInput->getSize());
        }

        //CLEANUP(mono_image_close(image));

        const std::filesystem::path fullPath = GetVFS().resolve(IO::VFS::Path{ input.getName() });
        MonoAssembly* assembly = mono_assembly_load_from_full(image, input.getName().c_str(), &status, false);
        if(assembly == nullptr) {
            return nullptr;
        }

        if(status != MONO_IMAGE_OK) {
            Carrot::Log::error("Could not load assembly '%s': %s", input.getName().c_str(), mono_image_strerror(status));
            return nullptr;
        }

        auto ptr = std::shared_ptr<CSAssembly>(new CSAssembly(appDomain, assembly));
        loadedAssemblies.push_back(ptr);
        return ptr;
    }

    int ScriptingEngine::runExecutable(const Carrot::IO::Resource& exe, std::span<std::string> arguments) {
        auto module = loadAssembly(exe);

        if(!module) {
            return -1;
        }

        return module->executeMain(arguments);
    }

    bool ScriptingEngine::compileFiles(const fs::path& outputAssembly,
                                       std::span<fs::path> sourceFiles,
                                       std::span<fs::path> referenceAssemblies,
                                       std::optional<std::filesystem::path> outputPDB) {
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

        for(const auto& p : referenceAssemblies) {
            command += " -reference:";
            command += "\"";
            command += Carrot::toString(p.u8string());
            command += "\"";
        }
        command += " -target:library";

        if(outputPDB.has_value()) {
            command += " -debug:portable";
            command += " -pdb:";
            command += "\"";
            command += Carrot::toString(outputPDB->u8string());
            command += "\"";
        }

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

    CSClass* ScriptingEngine::findClass(const std::string& namespaceName, const std::string& className) {
        for(auto& pModule : loadedAssemblies) {
            if(auto m = pModule.lock()) {
                auto* clazz = m->findClass(namespaceName, className);
                if(clazz) {
                    return clazz;
                }
            }
        }
        return nullptr;
    }

    CSClass* ScriptingEngine::getParentClass(CSClass& child) {
        for(auto& pModule : loadedAssemblies) {
            if(auto m = pModule.lock()) {
                auto* clazz = m->getParentClass(child);
                if(clazz) {
                    return clazz;
                }
            }
        }
        return nullptr;
    }

    MonoDomain* ScriptingEngine::getRootDomain() const {
        return rootDomain;
    }
} // Carrot::Scripting