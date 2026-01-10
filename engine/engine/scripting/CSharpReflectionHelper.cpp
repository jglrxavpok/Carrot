//
// Created by jglrxavpok on 29/03/2023.
//

#include "CSharpReflectionHelper.h"
#include <core/scripting/csharp/Engine.h>
#include <core/scripting/csharp/CSClass.h>
#include <core/scripting/csharp/CSField.h>
#include <core/scripting/csharp/CSMethod.h>
#include <core/scripting/csharp/CSObject.h>
#include <engine/Engine.h>
#include <engine/utils/Macros.h>
#include <mono/metadata/object.h>

namespace Carrot::Scripting {

    void CSharpReflectionHelper::reload() {
        {
            LOAD_CLASS(ComponentProperty);
            LOAD_FIELD(ComponentProperty, FieldName);
            LOAD_FIELD(ComponentProperty, Type);
            LOAD_FIELD(ComponentProperty, SerializationName);
            LOAD_FIELD(ComponentProperty, DisplayName);
            LOAD_FIELD(ComponentProperty, IntRange);
            LOAD_FIELD(ComponentProperty, FloatRange);
        }

        {
            LOAD_CLASS(IntRange);
            LOAD_FIELD(IntRange, Min);
            LOAD_FIELD(IntRange, Max);
        }

        {
            LOAD_CLASS(FloatRange);
            LOAD_FIELD(FloatRange, Min);
            LOAD_FIELD(FloatRange, Max);
        }

        {
            LOAD_CLASS(Reflection);
            LOAD_METHOD(Reflection, FindAllComponentProperties);
            LOAD_METHOD(Reflection, IsInternalComponent);
            LOAD_METHOD(Reflection, GetEnumNames);
            LOAD_METHOD(Reflection, ParseEnum);
        }

        {
            LOAD_CLASS_NS("System", Enum);
            LOAD_METHOD(Enum, ToString);
        }
    }

    std::vector<ReflectionProperty> CSharpReflectionHelper::findAllReflectionProperties(const std::string& namespaceName, const std::string& className) {
        MonoString* namespaceStr = mono_string_new_wrapper(namespaceName.c_str());
        MonoString* classStr = mono_string_new_wrapper(className.c_str());
        CSClass* klass = GetCSharpScripting().findClass(namespaceName, className);
        if(!klass) {
            return {};
        }

        void* args[2] = {
                namespaceStr,
                classStr,
        };
        auto result = ReflectionFindAllComponentPropertiesMethod->staticInvoke(args);
        MonoArray* asArray = (MonoArray*)(MonoObject*)result;
        std::size_t propertyCount = mono_array_length(asArray);

        std::vector<ReflectionProperty> properties;
        properties.reserve(propertyCount);
        for (std::size_t j = 0; j < propertyCount; ++j) {
            MonoObject* componentProperty = mono_array_get(asArray, MonoObject*, j);

            auto fieldNameObj = ComponentPropertyFieldNameField->get(CSObject(componentProperty));
            char* fieldName = mono_string_to_utf8((MonoString*)(MonoObject*)fieldNameObj);
            CLEANUP(mono_free(fieldName));

            auto& property = properties.emplace_back();
            property.fieldName = fieldName;

            auto typeStrObj = ComponentPropertyTypeField->get(CSObject(componentProperty));
            char* typeStr = mono_string_to_utf8((MonoString*)(MonoObject*)typeStrObj);
            CLEANUP(mono_free(typeStr));

            property.typeStr = typeStr;
            if(std::string_view("System.Int") == typeStr
            || std::string_view("System.Int32") == typeStr
            ) {
                property.type = ComponentType::Int;
            } else if(std::string_view("System.Single") == typeStr) {
                property.type = ComponentType::Float;
            } else if(std::string_view("System.Double") == typeStr) {
                property.type = ComponentType::Double;
            } else if(std::string_view("System.Boolean") == typeStr) {
                property.type = ComponentType::Boolean;
            } else if(std::string_view("Carrot.Entity") == typeStr) {
                property.type = ComponentType::Entity;
            } else if(std::string_view("Carrot.Vec2") == typeStr) {
                property.type = ComponentType::Vec2;
            } else if(std::string_view("Carrot.Vec3") == typeStr) {
                property.type = ComponentType::Vec3;
            } else if(std::string_view("System.String") == typeStr) {
                property.type = ComponentType::String;
            } else {
                parseUserType(property);
            }

            property.field = klass->findField(property.fieldName);
            verify(property.field != nullptr, "Reflection helper returned a field which does not exist");

            auto loadStringIfNotNull = [&](CSField* fromField, std::string& destination) {
                MonoObject* strObj = (MonoObject*)fromField->get(CSObject(componentProperty));
                if(strObj != nullptr) {
                    char* str = mono_string_to_utf8((MonoString*)strObj);
                    CLEANUP(mono_free(str));

                    destination = str;
                }
            };

            loadStringIfNotNull(ComponentPropertyDisplayNameField, property.displayName);
            loadStringIfNotNull(ComponentPropertySerializationNameField, property.serializationName);

            if(property.type == ComponentType::Int) {
                MonoObject* intRangeObj = (MonoObject*)ComponentPropertyIntRangeField->get(CSObject(componentProperty));
                if(intRangeObj != nullptr) {
                    property.intRange = IntRange {
                            .min = IntRangeMinField->get(CSObject(intRangeObj)).unbox<std::int32_t>(),
                            .max = IntRangeMaxField->get(CSObject(intRangeObj)).unbox<std::int32_t>(),
                    };
                }
            } else if(property.type == ComponentType::Float) {
                MonoObject* floatRangeObj = (MonoObject*)ComponentPropertyFloatRangeField->get(CSObject(componentProperty));
                if(floatRangeObj != nullptr) {
                    property.floatRange = FloatRange {
                            .min = FloatRangeMinField->get(CSObject(floatRangeObj)).unbox<float>(),
                            .max = FloatRangeMaxField->get(CSObject(floatRangeObj)).unbox<float>(),
                    };
                }
            }

            // TODO: load rest of attributes

            property.validate();
        }

        return properties;
    }

    bool CSharpReflectionHelper::isInternalComponent(const std::string& namespaceName, const std::string& className) {
        MonoString* namespaceStr = mono_string_new_wrapper(namespaceName.c_str());
        MonoString* classStr = mono_string_new_wrapper(className.c_str());
        CSClass* klass = GetCSharpScripting().findClass(namespaceName, className);
        if(!klass) {
            return false;
        }

        void* args[2] = {
                namespaceStr,
                classStr,
        };
        auto result = ReflectionIsInternalComponentMethod->staticInvoke(args);
        return result.unbox<bool>();
    }

    CSObject CSharpReflectionHelper::stringToEnumValue(const std::string& enumTypeName, const std::string& enumValue) const {
        MonoString* enumTypeNameObj = mono_string_new_wrapper(enumTypeName.c_str());
        MonoString* enumValueObj = mono_string_new_wrapper(enumValue.c_str());
        void* args[2] = {
            enumTypeNameObj,
            enumValueObj
        };
        CSObject result = ReflectionParseEnumMethod->staticInvoke(args);
        void* v = mono_object_unbox(result); // enums are integral types, so need an unbox/box to properly pass to Mono...
        return Scripting::CSObject((MonoObject*)v);
    }

    std::string CSharpReflectionHelper::enumValueToString(const CSObject& enumValue) const {
        return EnumToStringMethod->invoke(enumValue, {}).toString();
    }

    void CSharpReflectionHelper::parseUserType(ReflectionProperty& outProperty) {
        const std::string& typeName = outProperty.typeStr;
        std::size_t lastDotPosition = typeName.find_last_of('.');
        std::string typeNamespace = lastDotPosition == std::string::npos ? "" : typeName.substr(0, lastDotPosition);
        std::string typeClassName = lastDotPosition == std::string::npos ? typeName : typeName.substr(lastDotPosition+1);

        CSClass* userTypeClass = GetCSharpScripting().findClass(typeNamespace, typeClassName);
        if(mono_class_is_enum(userTypeClass->toMono())) {
            outProperty.type = ComponentType::UserEnum;
            MonoString* fullTypeStr = mono_string_new_wrapper(typeName.c_str());

            void* args[1] = {
                fullTypeStr
            };
            CSObject enumNamesResult = ReflectionGetEnumNamesMethod->staticInvoke(args);
            MonoArray* enumNamesAsArray = (MonoArray*)(MonoObject*)enumNamesResult;
            std::size_t nameCount = mono_array_length(enumNamesAsArray);
            outProperty.validUserEnumValues.ensureReserve(nameCount);
            for(std::size_t index = 0; index < nameCount; index++) {
                MonoString* entryNameObj = (MonoString*)mono_array_get(enumNamesAsArray, MonoObject*, index);
                char* entryNameStr = mono_string_to_utf8(entryNameObj);
                outProperty.validUserEnumValues.pushBack(entryNameStr);
                CLEANUP(mono_free(entryNameStr));
            }
        } else {
            outProperty.type = ComponentType::UserDefined;
        }
    }
} // Carrot::Scripting