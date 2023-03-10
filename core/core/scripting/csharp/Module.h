//
// Created by jglrxavpok on 07/03/2023.
//

#pragma once

#include <mono/utils/mono-forward.h>
#include <mono/metadata/image.h>
#include <string>
#include <span>
#include <unordered_map>

namespace Carrot::Scripting {
    class ScriptingEngine;

    /// Represents a C# "module" (ie dll)
    class Module {
    public:
        /// Calls the Main function of this module. Throws if none exists.
        int executeMain(std::span<std::string> arguments);

        /**
         * Calls the static method 'methodName' inside 'namespaceName'.'className' with the given arguments
         * @param namespaceName namespace in which the class should be found
         * @param className class in which the method should be found
         * @param methodName name of method to call
         * @param args list of pointers to the arguments. The size of this span is used to find the correct method!
         * @return the result of the execution
         * @throws if the class does not exist, or the function does not exist
         */
        MonoObject* staticInvoke(const std::string& namespaceName, const std::string& className, const std::string& methodName, std::span<void*> args);

        /**
         * Finds the given class inside the module, or nullptr if none. Cached. Not thread-safe
         */
        MonoClass* findClass(const std::string& namespaceName, const std::string& className);

        /**
         * Finds the first occurrence of the given method inside the given class, or nullptr if none. Cached. Not thread-safe
         */
        MonoMethod* findMethod(MonoClass* clazz, const std::string& methodName);

        /**
         * Finds the given method inside the given class, or nullptr if none. Cached. Not thread-safe.
         * No guarantee what this returns if multiple methods have the same name and same parameter count.
         */
        MonoMethod* findMethod(MonoClass* clazz, const std::string& methodName, int parameterCount);

    public: // test
        void dumpTypes();

    public:
        ~Module();

    private:
        explicit Module(MonoDomain* appDomain, MonoAssembly* assembly);

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

        struct MethodKey {
            MonoClass* clazz { nullptr };
            std::string methodName;
            int parameterCount = 0;

            auto operator<=>(const MethodKey&) const = default;
        };

        struct MethodKeyHasher {
            std::hash<std::string> stringHasher;

            std::size_t operator()(const MethodKey& k) const {
                return (stringHasher(k.methodName) * 31 + k.parameterCount) * 31 + (std::size_t)k.clazz;
            }
        };

    private:
        MonoAssembly* assembly = nullptr;
        MonoDomain* appDomain = nullptr;
        std::unordered_map<ClassKey, MonoClass*, ClassKeyHasher> classCache;
        std::unordered_map<MethodKey, MonoMethod*, MethodKeyHasher> methodCache;

        friend class ::Carrot::Scripting::ScriptingEngine;
    };
} // Carrot::Scripting
