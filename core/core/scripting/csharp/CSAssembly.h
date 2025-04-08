//
// Created by jglrxavpok on 07/03/2023.
//

#pragma once

#include <mono/utils/mono-forward.h>
#include <mono/metadata/image.h>
#include <string>
#include <span>
#include <unordered_map>
#include <core/scripting/csharp/forward.h>
#include <core/scripting/csharp/CSClass.h>

namespace Carrot::Scripting {
    /// Represents a C# assembly
    class CSAssembly {
    public:
        /// Calls the Main function of this module. Throws if none exists.
        int executeMain(std::span<std::string> arguments);

        /**
         * Finds the given class inside the module, or nullptr if none. Cached. Not thread-safe
         */
        CSClass* findClass(const std::string& namespaceName, const std::string& className);

        /**
         * Attemps to find the parent class of the input. Not thread-safe
         * Returns nullptr if there is no parent class
         */
        CSClass* getParentClass(CSClass& child);

        /**
         * Finds the subclasses of a given parent class. 'parentClass' is NOT considered to be a subclass
         * @param parentClass the class to find subclasses of. Must not be nullptr
         * @return a vector containing the subclasses. Empty if none.
         */
        std::vector<CSClass*> findSubclasses(const CSClass& parentClass);

    public: // test
        void dumpTypes();

    public:
        ~CSAssembly();

    private:
        explicit CSAssembly(MonoDomain* appDomain, MonoAssembly* assembly);

        /// Load from an image, intended only for mscorlib
        explicit CSAssembly(MonoDomain* appDomain, MonoImage* assembly);

    private:
        struct ClassKey {
            std::string namespaceName;
            std::string className;

            auto operator<=>(const ClassKey&) const = default;
        };

        struct ClassKeyHasher {
            std::hash<std::string> stringHasher;

            std::size_t operator()(const ClassKey& k) const {
                return stringHasher(k.namespaceName) * 31 + stringHasher(k.className);
            }
        };

    private:
        MonoAssembly* assembly = nullptr;
        MonoImage* image = nullptr;
        MonoDomain* appDomain = nullptr;
        std::unordered_map<ClassKey, CSClass, ClassKeyHasher> classCache;

        friend class ::Carrot::Scripting::ScriptingEngine;
    };
} // Carrot::Scripting
