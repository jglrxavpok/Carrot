//
// Created by jglrxavpok on 07/03/2023.
//

#include "CSAssembly.h"
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
    CSAssembly::CSAssembly(MonoDomain* appDomain, MonoAssembly* assembly): appDomain(appDomain), assembly(assembly) {
        image = mono_assembly_get_image(assembly);
    }

    CSAssembly::CSAssembly(MonoDomain* appDomain, MonoImage* image): appDomain(appDomain), assembly(nullptr), image(image) {
        verify(image != nullptr, "image == nullptr!");
    }

    CSAssembly::~CSAssembly() {
        if(assembly) {
            //mono_assembly_close(assembly);// Commented because that seemed to crash Mono when unloading an appdomain ?
        }
    }

    int CSAssembly::executeMain(std::span<std::string> arguments) {
        verify(assembly, "Cannot call main on this CSAssembly: not loaded through MonoAssembly");
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

    void CSAssembly::dumpTypes() {
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

    CSClass* CSAssembly::findClass(const std::string& namespaceName, const std::string& className) {
        ClassKey key { namespaceName, className };
        auto it = classCache.find(key);
        if(it != classCache.end()) {
            return &it->second;
        }

        MonoClass* clazz = mono_class_from_name(image, namespaceName.c_str(), className.c_str());
        if(!clazz) {
            return nullptr;
        }
        auto [newIt, inserted] = classCache.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(appDomain, clazz));
        return &newIt->second;
    }

    std::vector<CSClass*> CSAssembly::findSubclasses(const CSClass& parentClass) {
        const MonoTableInfo* typeDefinitionTable = mono_image_get_table_info(image, MONO_TABLE_TYPEDEF);
        std::int32_t typeCount = mono_table_info_get_rows(typeDefinitionTable);

        std::vector<CSClass*> subclasses;
        for (std::int32_t i = 0; i < typeCount; ++i) {
            std::uint32_t cols[MONO_TYPEDEF_SIZE];
            mono_metadata_decode_row(typeDefinitionTable, i, cols, MONO_TYPEDEF_SIZE);

            const char* nameSpace = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAMESPACE]);
            const char* name = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAME]);

            CSClass* clazz = findClass(nameSpace, name);
            if(!clazz) {
                Carrot::Log::warn("Class %s.%s was in subclasses of %s, but could not be loaded via findClass?", nameSpace, name, parentClass.getName().c_str());
                continue;
            }
            if(clazz->isSubclassOf(parentClass)) {
                subclasses.push_back(clazz);
            }
        }

        return subclasses;
    }

} // Carrot::Scripting