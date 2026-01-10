//
// Created by jglrxavpok on 29/03/2023.
//

#pragma once

#include <vector>
#include <core/scripting/csharp/forward.h>
#include <engine/scripting/ReflectionProperty.h>

#define LOAD_CLASS_NS(NAMESPACE_NAME, CLASS_NAME) \
    CLASS_NAME##Class = GetCSharpScripting().findClass(NAMESPACE_NAME, #CLASS_NAME); \
    verify(CLASS_NAME##Class != nullptr, "Could not find class " NAMESPACE_NAME "." #CLASS_NAME " inside Carrot.dll ?");

#define LOAD_CLASS(CLASS_NAME) LOAD_CLASS_NS("Carrot", CLASS_NAME)

#define LOAD_FIELD(CLASS_NAME, FIELD_NAME) \
    CLASS_NAME##FIELD_NAME##Field = (CLASS_NAME##Class)->findField(#FIELD_NAME); \
    verify(CLASS_NAME##FIELD_NAME##Field != nullptr, "Could not find field " #FIELD_NAME " in class " #CLASS_NAME " inside Carrot.dll ?");

#define LOAD_METHOD(CLASS_NAME, METHOD_NAME) \
    CLASS_NAME##METHOD_NAME##Method = (CLASS_NAME##Class)->findMethod(#METHOD_NAME); \
    verify(CLASS_NAME##METHOD_NAME##Method != nullptr, "Could not find method " #METHOD_NAME " in class " #CLASS_NAME " inside Carrot.dll ?");


namespace Carrot::Scripting {

    /**
     * Helper to access attributes and other reflection-based information that are not exposed by Mono
     */
    class CSharpReflectionHelper {
    public:
        /**
         * Reload internal types when assembly is reloaded. Called by CSharpBindings
         */
        void reload();

        /**
         * Finds all component properties visible in editor from the given type
         */
        std::vector<ReflectionProperty> findAllReflectionProperties(const std::string& namespaceName, const std::string& className);

        /**
         * Returns true iif the given class has the [InternalComponent] attribute
         */
        bool isInternalComponent(const std::string& namespaceName, const std::string& className);

        /**
         * Attempts to convert a given string into a enum value, for a given enum type.
         * Will throw if the enum type name is invalid, or if the value itself is invalid.
         * @param enumTypeName fully qualified name of the enum (MyNameSpace.ExampleEnum)
         * @param enumValue name of the enum value to convert to
         * @return a CSObject corresponding to the value of the enum (can be given to field or methods expecting the enum type)
         */
        CSObject stringToEnumValue(const std::string& enumTypeName, const std::string& enumValue) const;

        /**
         * Converts a given enum value to its string representation
         * @param enumValue CSObject containing the enum value
         * @return a std::string corresponding to the string representation of the enum value
         */
        std::string enumValueToString(const CSObject& enumValue) const;

    private:
        void parseUserType(ReflectionProperty& outProperty);

        CSClass* ComponentPropertyClass = nullptr;
        CSField* ComponentPropertyFieldNameField = nullptr;
        CSField* ComponentPropertyTypeField = nullptr;
        CSField* ComponentPropertySerializationNameField = nullptr;
        CSField* ComponentPropertyDisplayNameField = nullptr;
        CSField* ComponentPropertyIntRangeField = nullptr;
        CSField* ComponentPropertyFloatRangeField = nullptr;

        CSClass* IntRangeClass = nullptr;
        CSField* IntRangeMinField = nullptr;
        CSField* IntRangeMaxField = nullptr;

        CSClass* FloatRangeClass = nullptr;
        CSField* FloatRangeMinField = nullptr;
        CSField* FloatRangeMaxField = nullptr;

        CSClass* ReflectionClass = nullptr;
        CSMethod* ReflectionFindAllComponentPropertiesMethod = nullptr;
        CSMethod* ReflectionIsInternalComponentMethod = nullptr;
        CSMethod* ReflectionGetEnumNamesMethod = nullptr;
        CSMethod* ReflectionParseEnumMethod = nullptr;

        CSClass* EnumClass = nullptr;
        CSMethod* EnumToStringMethod = nullptr;
    };

} // Carrot::Scripting
