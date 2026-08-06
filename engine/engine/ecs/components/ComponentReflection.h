//
// Created by jglrxavpok on 04/08/2026.
//

#pragma once
#include <core/io/Document.h>

namespace Carrot {
    class Identifier;
}

namespace Carrot::ECS {
    class Component;
    class ComponentReflection;

    /**
     * Very basic description of a property inside a component
     */
    struct BaseComponentPropertyReflection {
        std::string name;
        std::string publicName;
        bool mandatory;
        // TODO: type

        BaseComponentPropertyReflection(std::string name, std::string publicName, bool mandatory, ComponentReflection* pReflect);
        virtual ~BaseComponentPropertyReflection() = default;

        virtual void deserialise(Carrot::ECS::Component&, const Carrot::DocumentElement& doc) const = 0;
        [[nodiscard]] virtual Carrot::DocumentElement serialise(const Carrot::ECS::Component&) const = 0;
        virtual void duplicateProperty(const Carrot::ECS::Component& src, Carrot::ECS::Component& dest) const = 0;
    };

    template<typename TProperty>
    void deserialiseElement(TProperty& out, const Carrot::DocumentElement& doc) = delete; // "Unsupported property type for reflection"

    template<typename TProperty>
    Carrot::DocumentElement serialiseElement(const TProperty& input) = delete; // "Unsupported property type for reflection"

#define DECLARE_PROPERTY_TYPE(Type) \
    template<> void deserialiseElement<Type>(Type& out, const Carrot::DocumentElement& doc);\
    template<> Carrot::DocumentElement serialiseElement<Type>(const Type& input)

    DECLARE_PROPERTY_TYPE(float);
    DECLARE_PROPERTY_TYPE(bool);
    DECLARE_PROPERTY_TYPE(glm::vec3);
    DECLARE_PROPERTY_TYPE(Carrot::Identifier);

    /**
     * Templated version of BaseComponentPropertyReflection which has a pointer-to-member to the property inside the component
     */
    template<typename TComponent, typename TProperty>
    struct ComponentPropertyReflection: BaseComponentPropertyReflection {
        TProperty TComponent::*propertyPtr;
        std::string name;

        ComponentPropertyReflection(TProperty TComponent::*ptr, std::string name, std::string publicName, bool mandatory, ComponentReflection* pReflect)
        : BaseComponentPropertyReflection(std::move(name), std::move(publicName), mandatory, pReflect)
        , propertyPtr(ptr)
        {}

        void deserialise(Carrot::ECS::Component& component, const Carrot::DocumentElement& doc) const override {
            TProperty& ref = static_cast<TComponent&>(component).*propertyPtr;
            Carrot::ECS::deserialiseElement<TProperty>(ref, doc);
        }

        [[nodiscard]] Carrot::DocumentElement serialise(const Carrot::ECS::Component& component) const override {
            const TProperty& ref = static_cast<const TComponent&>(component).*propertyPtr;
            return Carrot::ECS::serialiseElement<TProperty>(ref);
        }

        void duplicateProperty(const Carrot::ECS::Component& src, Carrot::ECS::Component& dest) const override {
            const TProperty& srcRef = static_cast<const TComponent&>(src).*propertyPtr;
            TProperty& destRef = static_cast<TComponent&>(dest).*propertyPtr;
            destRef = srcRef;
        }
    };

    /**
     * Contains the reflection information about a component.
     * Most importantly, contains the list of properties of the component.
     */
    class ComponentReflection {
    public:
        ComponentReflection() = default;
        const Carrot::Vector<BaseComponentPropertyReflection*>& getProperties() const;
        Carrot::DocumentElement serialise(const Carrot::ECS::Component& comp) const;
        void deserialise(Carrot::ECS::Component& comp, const Carrot::DocumentElement& doc) const;

    private:
        Carrot::Vector<BaseComponentPropertyReflection*> properties;

        friend struct BaseComponentPropertyReflection;
    };
}

// Expected to be included with Component.h, which has BEGIN_COMPONENT which defines TSelf

#define FIELD_NAME_CONCAT_IMPL(A, B) A ## B
#define FIELD_NAME_CONCAT(A, B) FIELD_NAME_CONCAT_IMPL(A, B)
#define FIELD_IMPL(Type, Name, PublicName, DefaultValue, Mandatory) Type Name = DefaultValue; \
static inline ::Carrot::ECS::ComponentPropertyReflection<TSelf, Type> FIELD_NAME_CONCAT(_field_, Name)\
    {&TSelf::Name, #Name, PublicName, Mandatory, &Reflection} // TODO: edit function

/// Adds a mandatory property (ie must be inside serialized version) to a component.
/// Type is the C++ type of the property.
/// Name is the C++ name of the property.
/// PublicName is the name used for serialization and display.
/// DefaultValue is the default value of the property (in C++).
#define FIELD(Type, Name, PublicName, DefaultValue) FIELD_IMPL(Type, Name, PublicName, DefaultValue, true)

/// Adds an optional property (ie can be missing inside serialized version) to a component
/// See FIELD for more information
#define OPTIONAL_FIELD(Type, Name, PublicName, DefaultValue) FIELD_IMPL(Type, Name, PublicName, DefaultValue, false)
