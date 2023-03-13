//
// Created by jglrxavpok on 11/03/2023.
//

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <core/scripting/csharp/forward.h>
#include <core/scripting/csharp/CSField.h>
#include <core/scripting/csharp/CSMethod.h>
#include <core/scripting/csharp/CSProperty.h>
#include <mono/metadata/object-forward.h>
#include <mono/utils/mono-forward.h>

namespace Carrot::Scripting {
    /**
     * Simple C++ wrapper for C# classes
     */
    class CSClass {
    public:
        explicit CSClass(MonoDomain* appDomain, MonoClass* clazz);

    public:
        std::shared_ptr<CSObject> newObject(std::span<void*> args);
        std::shared_ptr<CSArray> newArray(int count);

    public: // methods
        /**
         * Finds the first occurrence of the given method inside the class, or nullptr if none. Cached. Not thread-safe
         */
        CSMethod* findMethod(const std::string& methodName);

        /**
         * Finds the given method inside the class, or nullptr if none. Cached. Not thread-safe.
         * No guarantee what this returns if multiple methods have the same name and same parameter count.
         */
        CSMethod* findMethod(const std::string& methodName, int parameterCount);

    public:
        CSProperty* findProperty(const std::string& propertyName);

    public:
        CSField* findField(const std::string& fieldName);

    public: // reflection
        const std::string& getNamespaceName() const;
        const std::string& getName() const;

        /**
         * Returns true iif this class is a subclass of "parent". Is this class is equal to "parent", this method returns false
         */
        bool isSubclassOf(const CSClass& parent) const;

    public:
        bool operator==(const CSClass& other) const;

    public:
        MonoClass* toMono();

    private:
        struct MethodKey {
            std::string methodName;
            int parameterCount = 0;

            auto operator<=>(const MethodKey&) const = default;
        };

        struct MethodKeyHasher {
            std::hash<std::string> stringHasher;

            std::size_t operator()(const MethodKey& k) const {
                return stringHasher(k.methodName) * 31 + k.parameterCount;
            }
        };

        using MemberKey = std::string;

    private:
        MonoDomain* appDomain = nullptr;
        MonoClass* clazz = nullptr;
        std::unordered_map<MethodKey, CSMethod, MethodKeyHasher> methodCache;
        std::unordered_map<MemberKey, CSProperty> propertyCache;
        std::unordered_map<MemberKey, CSField> fieldCache;

        std::string namespaceName;
        std::string className;

        friend class ScriptingEngine;
    };
} // Carrot::Scripting
