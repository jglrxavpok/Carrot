//
// Created by jglrxavpok on 11/03/2023.
//

#include "CSClass.h"
#include "mono/metadata/class.h"
#include "mono/metadata/object.h"
#include "core/utils/stringmanip.h"
#include "core/utils/Assert.h"
#include "core/Macros.h"
#include <core/scripting/csharp/CSArray.h>
#include <core/scripting/csharp/CSObject.h>

namespace Carrot::Scripting {

    CSClass::CSClass(MonoDomain* appDomain, MonoClass* clazz): appDomain(appDomain), clazz(clazz) {
        verify(clazz, "this class does not exist");
        namespaceName = mono_class_get_namespace(clazz);
        className = mono_class_get_name(clazz);
    }

    std::shared_ptr<CSObject> CSClass::newObject(std::span<void*> args) {
        auto* obj = mono_object_new(appDomain, clazz);
        verify(obj, Carrot::sprintf("Could not create object of type %s.%s", namespaceName.c_str(), className.c_str()));

        auto result = std::make_shared<CSObject>(obj);

        if(args.size() == 0) {
            mono_runtime_object_init(obj);
        } else {
            CSMethod* method = findMethod(".ctor", args.size());
            verify(method, Carrot::sprintf("No constructor with %llu arguments!", args.size()));
            method->invoke(*result, args);
        }

        return result;
    }

    std::shared_ptr<CSArray> CSClass::newArray(int count) {
        auto* arr = mono_array_new(appDomain, clazz, count);
        return std::make_shared<CSArray>(*this, appDomain, arr);
    }

    CSMethod* CSClass::findMethod(const std::string& methodName) {
        return findMethod(methodName, -1);
    }

    CSMethod* CSClass::findMethod(const std::string& methodName, int parameterCount) {
        MethodKey key { methodName, parameterCount };
        auto it = methodCache.find(key);
        if(it != methodCache.end()) {
            return &it->second;
        }

        MonoMethod* method = mono_class_get_method_from_name(clazz, methodName.c_str(), parameterCount);
        if(!method) {
            return nullptr;
        }
        auto [newIt, inserted] = methodCache.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(method));
        return &newIt->second;
    }

    CSProperty* CSClass::findProperty(const std::string& propertyName) {
        MemberKey key { propertyName };
        auto it = propertyCache.find(key);
        if(it != propertyCache.end()) {
            return &it->second;
        }

        MonoProperty* prop = mono_class_get_property_from_name(clazz, propertyName.c_str());
        if(!prop) {
            return nullptr;
        }
        auto [newIt, inserted] = propertyCache.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(*this, prop));
        return &newIt->second;
    }

    CSField* CSClass::findField(const std::string& fieldName) {
        MemberKey key { fieldName };
        auto it = fieldCache.find(key);
        if(it != fieldCache.end()) {
            return &it->second;
        }

        MonoClassField* field = mono_class_get_field_from_name(clazz, fieldName.c_str());
        if(!field) {
            return nullptr;
        }
        auto [newIt, inserted] = fieldCache.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(*this, appDomain, field));
        return &newIt->second;
    }

    const std::string& CSClass::getNamespaceName() const {
        return namespaceName;
    }

    const std::string& CSClass::getName() const {
        return className;
    }

    bool CSClass::isSubclassOf(const CSClass& other) const {
        if(*this == other) {
            return false;
        }
        return mono_class_is_subclass_of(clazz, other.clazz, true);
    }

    bool CSClass::operator==(const CSClass& other) const {
        return clazz == other.clazz;
    }

    MonoClass* CSClass::toMono() {
        return clazz;
    }
} // Carrot::Scripting