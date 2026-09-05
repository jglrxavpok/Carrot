//
// Created by jglrxavpok on 04/08/2026.
//
#include <core/io/DocumentHelpers.h>
#include <engine/ecs/components/ComponentReflection.h>
#include <core/utils/Identifier.h>
#include <engine/math/Transform.h>

#include "Component.h"

namespace Carrot::ECS {
    BaseComponentPropertyReflection::BaseComponentPropertyReflection(std::string name, std::string publicName, bool mandatory, ComponentReflectionData* pReflect)
    : name(std::move(name))
    , publicName(std::move(publicName))
    , mandatory(mandatory)
    {
        pReflect->properties.pushBack(this);
    }

    const Carrot::Vector<BaseComponentPropertyReflection*>& ComponentReflectionData::getProperties() const {
        return properties;
    }

    Carrot::DocumentElement ComponentReflectionData::serialise(const Carrot::ECS::Component& comp) const {
        Carrot::DocumentElement doc;
        for (const auto& pProperty : properties) {
            if (pProperty->isInline) {
                doc.mergeWith(pProperty->serialise(comp));
            } else {
                doc[pProperty->publicName] = pProperty->serialise(comp);
            }
        }
        return doc;
    }

    void ComponentReflectionData::deserialise(Carrot::ECS::Component& comp, const Carrot::DocumentElement& doc) const {
        Carrot::DocumentElement::ObjectView objectView = doc.getAsObject();
        for (const BaseComponentPropertyReflection* pReflect : getProperties()) {
            if (pReflect->isInline) {
                pReflect->deserialise(comp, doc);
            } else {
                auto iter = objectView.find(pReflect->publicName);
                if (iter != objectView.end()) {
                    pReflect->deserialise(comp, iter->second);
                } else if (pReflect->mandatory) {
                    verify(false, Carrot::sprintf("Field %s is mandatory, but was not present in document", pReflect->publicName.c_str()));
                }
            }
        }
    }

    // Property types
    void ReflectedSerialisation<float>::deserialiseElement(ECS::Component& component, float& out, const Carrot::DocumentElement& doc) {
        out = static_cast<float>(doc.getAsDouble());
    }

    Carrot::DocumentElement ReflectedSerialisation<float>::serialiseElement(const ECS::Component& component, const float& input) {
        Carrot::DocumentElement elem;
        elem = input;
        return elem;
    }

    void ReflectedSerialisation<bool>::deserialiseElement(ECS::Component& component, bool& out, const Carrot::DocumentElement& doc) {
        out = doc.getAsBool();
    }

    Carrot::DocumentElement ReflectedSerialisation<bool>::serialiseElement(const ECS::Component& component, const bool& input) {
        Carrot::DocumentElement elem;
        elem = input;
        return elem;
    }

    void ReflectedSerialisation<glm::vec2>::deserialiseElement(ECS::Component& component, glm::vec2& out, const Carrot::DocumentElement& doc) {
        out = DocumentHelpers::read<2, float>(doc);
    }

    Carrot::DocumentElement ReflectedSerialisation<glm::vec2>::serialiseElement(const ECS::Component& component, const glm::vec2& input) {
        Carrot::DocumentElement elem;
        elem = DocumentHelpers::write<2, float>(input);
        return elem;
    }

    void ReflectedSerialisation<glm::vec3>::deserialiseElement(ECS::Component& component, glm::vec3& out, const Carrot::DocumentElement& doc) {
        out = DocumentHelpers::read<3, float>(doc);
    }

    Carrot::DocumentElement ReflectedSerialisation<glm::vec3>::serialiseElement(const ECS::Component& component, const glm::vec3& input) {
        Carrot::DocumentElement elem;
        elem = DocumentHelpers::write<3, float>(input);
        return elem;
    }

    void ReflectedSerialisation<glm::vec4>::deserialiseElement(ECS::Component& component, glm::vec4& out, const Carrot::DocumentElement& doc) {
        out = DocumentHelpers::read<4, float>(doc);
    }

    Carrot::DocumentElement ReflectedSerialisation<glm::vec4>::serialiseElement(const ECS::Component& component, const glm::vec4& input) {
        Carrot::DocumentElement elem;
        elem = DocumentHelpers::write<4, float>(input);
        return elem;
    }

    void ReflectedSerialisation<Carrot::Identifier>::deserialiseElement(ECS::Component& component, Carrot::Identifier& out, const Carrot::DocumentElement& doc) {
        out = Carrot::Identifier{doc.getAsString()};
    }

    Carrot::DocumentElement ReflectedSerialisation<Carrot::Identifier>::serialiseElement(const ECS::Component& component, const Carrot::Identifier& input) {
        Carrot::DocumentElement elem;
        elem = std::string{input};
        return elem;
    }

    void ReflectedSerialisation<Carrot::Math::Transform>::deserialiseElement(ECS::Component& component, Carrot::Math::Transform& out, const Carrot::DocumentElement& doc) {
        out.deserialise(doc);
    }

    Carrot::DocumentElement ReflectedSerialisation<Carrot::Math::Transform>::serialiseElement(const ECS::Component& component, const Carrot::Math::Transform& input) {
        return input.serialise();
    }

    void ReflectedSerialisation<Carrot::UUID>::deserialiseElement(ECS::Component& component, Carrot::UUID& out, const Carrot::DocumentElement& doc) {
        out = Carrot::UUID::fromString(doc.getAsString());
    }

    Carrot::DocumentElement ReflectedSerialisation<Carrot::UUID>::serialiseElement(const ECS::Component& component, const Carrot::UUID& input) {
        Carrot::DocumentElement doc;
        doc = input.toString();
        return doc;
    }
}