//
// Created by jglrxavpok on 07/03/2023.
//

#include "Module.h"
#include <core/utils/Assert.h>
#include <core/Macros.h>
#include <core/io/Logging.hpp>
#include <mono/jit/jit.h>
#include <mono/metadata/appdomain.h>
#include <mono/metadata/class.h>
#include <mono/metadata/metadata.h>
#include <mono/metadata/row-indexes.h>
#include <mono/metadata/blob.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>
#include <cstdint>

namespace Carrot::Scripting {
    Module::Module(MonoDomain* appDomain, MonoAssembly* assembly): appDomain(appDomain), assembly(assembly) {

    }

    Module::~Module() {
        mono_assembly_close(assembly);
    }

    int Module::executeMain(std::span<std::string> arguments) {
        std::vector<char*> argv;
        argv.reserve(arguments.size()+1);

        // From https://www.mono-project.com/docs/advanced/embedding/
        // mono_jit_exec expects the assembly file name in argv[0], so we adjust c argc and argv to respect the requirement.
        std::string n = mono_stringify_assembly_name(mono_assembly_get_name(assembly));
        argv.push_back(n.data());

        for(auto& s : arguments) {
            argv.push_back(s.data());
        }
        return mono_jit_exec(appDomain, assembly, argv.size(), argv.data());
    }

    void Module::dumpTypes() {
        MonoImage* image = mono_assembly_get_image(assembly);
        const MonoTableInfo* typeDefinitionTable = mono_image_get_table_info(image, MONO_TABLE_TYPEDEF);
        std::int32_t typeCount = mono_table_info_get_rows(typeDefinitionTable);

        for (std::int32_t i = 0; i < typeCount; ++i) {
            std::uint32_t cols[MONO_TYPEDEF_SIZE];
            mono_metadata_decode_row(typeDefinitionTable, i, cols, MONO_TYPEDEF_SIZE);

            const char* nameSpace = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAMESPACE]);
            const char* name = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAME]);

            Carrot::Log::info("- %s.%s", nameSpace, name);
        }
    }

    MonoObject* Module::staticInvoke(const std::string& namespaceName, const std::string& className,
                                     const std::string& methodName,
                                     std::span<void*> args) {
        MonoClass* clazz = findClass(namespaceName, className);
        verify(clazz, Carrot::sprintf("Could not find class %s.%s", namespaceName.c_str(), className.c_str()));

        MonoMethod* method = findMethod(clazz, methodName, args.size());

        verify(method, Carrot::sprintf("Could not find method %s with %llu arguments in class '%s.%s'", methodName.c_str(), args.size(), namespaceName.c_str(), className.c_str()));

        return mono_runtime_invoke(method, nullptr, args.data(), nullptr); // TODO: exception handling
    }

    MonoClass* Module::findClass(const std::string& namespaceName, const std::string& className) {
        ClassKey key { namespaceName, className };
        auto it = classCache.find(key);
        if(it != classCache.end()) {
            return it->second;
        }

        auto image = mono_assembly_get_image(assembly);
        MonoClass* clazz = mono_class_from_name(image, namespaceName.c_str(), className.c_str());
        classCache[key] = clazz;
        return clazz;
    }

    MonoMethod* Module::findMethod(MonoClass* clazz, const std::string& methodName) {
        return findMethod(clazz, methodName, -1);
    }

    MonoMethod* Module::findMethod(MonoClass* clazz, const std::string& methodName, int parameterCount) {
        MethodKey key { clazz, methodName, parameterCount };
        auto it = methodCache.find(key);
        if(it != methodCache.end()) {
            return it->second;
        }

        MonoMethod* method = mono_class_get_method_from_name(clazz, methodName.c_str(), parameterCount);
        methodCache[key] = method;
        return method;
    }

} // Carrot::Scripting