//
// Created by jglrxavpok on 29/03/2023.
//

#pragma once

#include <vector>
#include <core/scripting/csharp/forward.h>
#include <engine/scripting/ComponentProperty.h>

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
        std::vector<ComponentProperty> findAllComponentProperties(const std::string& namespaceName, const std::string& className);

    private:
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
    };

} // Carrot::Scripting
