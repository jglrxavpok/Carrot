//
// Created by jglrxavpok on 04/08/2026.
//
#include <core/io/DocumentHelpers.h>
#include <engine/ecs/components/ComponentReflection.h>
#include <core/utils/Identifier.h>

namespace Carrot::ECS {
    BaseComponentPropertyReflection::BaseComponentPropertyReflection(std::string name, std::string publicName, bool mandatory, ComponentReflection* pReflect)
    : name(std::move(name))
    , publicName(std::move(publicName))
    , mandatory(mandatory)
    {
        pReflect->properties.pushBack(this);
    }

    const Carrot::Vector<BaseComponentPropertyReflection*>& ComponentReflection::getProperties() const {
        return properties;
    }

    // Property types
    template<> void deserialiseElement<float>(float& out, const Carrot::DocumentElement& doc) {
        out = static_cast<float>(doc.getAsDouble());
    }

    template<> Carrot::DocumentElement serialiseElement<float>(const float& input) {
        Carrot::DocumentElement elem;
        elem = input;
        return elem;
    }

    template<> void deserialiseElement<bool>(bool& out, const Carrot::DocumentElement& doc) {
        out = doc.getAsBool();
    }

    template<> Carrot::DocumentElement serialiseElement<bool>(const bool& input) {
        Carrot::DocumentElement elem;
        elem = input;
        return elem;
    }

    template<> void deserialiseElement<glm::vec3>(glm::vec3& out, const Carrot::DocumentElement& doc) {
        out = DocumentHelpers::read<3, float>(doc);
    }

    template<> Carrot::DocumentElement serialiseElement<glm::vec3>(const glm::vec3& input) {
        Carrot::DocumentElement elem;
        elem = DocumentHelpers::write<3, float>(input);
        return elem;
    }

    template<> void deserialiseElement<Carrot::Identifier>(Carrot::Identifier& out, const Carrot::DocumentElement& doc) {
        out = Carrot::Identifier{doc.getAsString()};
    }

    template<> Carrot::DocumentElement serialiseElement<Carrot::Identifier>(const Carrot::Identifier& input) {
        Carrot::DocumentElement elem;
        elem = std::string{input};
        return elem;
    }
}