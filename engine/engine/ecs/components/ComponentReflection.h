//
// Created by jglrxavpok on 04/08/2026.
//

#pragma once
#include <core/io/Document.h>
#include <engine/ecs/Prefab.h>
#include <engine/ecs/components/Component.h>
#include <engine/render/AsyncResource.hpp>

namespace Carrot {
    namespace Math {
        struct Transform;
    }

    class Identifier;
}

namespace Carrot::ECS {
    class Component;
    class ComponentReflectionData;

    /**
     * Very basic description of a property inside a component
     */
    struct BaseComponentPropertyReflection {
        std::string name;
        std::string publicName;
        bool mandatory;
        // TODO: type

        // config
        bool isInline = false; /// True if property should be serialised directly to the Carrot::Document representing the object, instead of as a sub-object (see TransformComponent for an example)

        BaseComponentPropertyReflection(std::string name, std::string publicName, bool mandatory, ComponentReflectionData* pReflect);
        virtual ~BaseComponentPropertyReflection() = default;

        virtual void deserialise(Carrot::ECS::Component&, const Carrot::DocumentElement& doc) const = 0;
        [[nodiscard]] virtual Carrot::DocumentElement serialise(const Carrot::ECS::Component&) const = 0;
        virtual void duplicateProperty(const Carrot::ECS::Component& src, Carrot::ECS::Component& dest) const = 0;
    };

    /**
     * Used to wrap a field with a getter and setter to allow user code to run when modified
     * Can also be used to reroute read/write to the field to another variable
     * @tparam T Type of element to wrap
     */
    template<typename TOwningType, typename T>
    struct PropertyWrapper {
        TOwningType& owner;
        std::function<T(TOwningType& self)> getter;
        std::function<void(TOwningType& self, T&& newValue)> setter;

        explicit PropertyWrapper(TOwningType& owner, T&& defaultValue, const std::function<T(TOwningType& self)>& getter, const std::function<void(TOwningType& self, T&& newValue)>& setter)
            : owner(owner)
            , getter(getter)
            , setter(setter)
        {
            setter(owner, std::move(defaultValue));
        }

        operator T() const {
            return getter(owner);
        }

        T get() const {
            return getter(owner);
        }

        void set(const T& v) {
            *this = v;
        }

        void set(const T&& v) {
            *this = std::move(v);
        }

        PropertyWrapper<TOwningType, T>& operator=(T&& v) {
            setter(owner, std::move(v));
            return *this;
        }

        PropertyWrapper<TOwningType, T>& operator=(const T& v) {
            T copy = v;
            setter(owner, std::move(copy));
            return *this;
        }

        PropertyWrapper<TOwningType, T>& operator=(const PropertyWrapper<TOwningType, T>& other) {
            if (&other == this)
                return *this;
            setter(owner, other.get());
            return *this;
        }
    };

    template<typename TElement>
    struct ReflectedSerialisation {
        static void deserialiseElement(ECS::Component& component, TElement& out, const Carrot::DocumentElement& doc) = delete; // "Unsupported property type for reflection"
        static Carrot::DocumentElement serialiseElement(const ECS::Component& component, const TElement& input) = delete; // "Unsupported property type for reflection"
    };

#define DECLARE_PROPERTY_TYPE(Type) \
    template<> struct ReflectedSerialisation<Type> {\
        static void deserialiseElement(ECS::Component& component, Type& out, const Carrot::DocumentElement& doc);\
        static Carrot::DocumentElement serialiseElement(const ECS::Component& component, const Type& input);\
    }

    DECLARE_PROPERTY_TYPE(float);
    DECLARE_PROPERTY_TYPE(bool);
    DECLARE_PROPERTY_TYPE(glm::vec2);
    DECLARE_PROPERTY_TYPE(glm::vec3);
    DECLARE_PROPERTY_TYPE(glm::vec4);
    DECLARE_PROPERTY_TYPE(Carrot::Identifier);
    DECLARE_PROPERTY_TYPE(Carrot::Math::Transform);
    DECLARE_PROPERTY_TYPE(Carrot::UUID);

    template<typename TOwningType, typename TElement>
    struct ReflectedSerialisation<PropertyWrapper<TOwningType, TElement>> {
        static void deserialiseElement(ECS::Component& component, PropertyWrapper<TOwningType, TElement>& out, const Carrot::DocumentElement& doc) {
            TElement tempValue;
            ReflectedSerialisation<TElement>::deserialiseElement(component, tempValue, doc);
            out = std::move(tempValue);
        }

        static Carrot::DocumentElement serialiseElement(const ECS::Component& component, const PropertyWrapper<TOwningType, TElement>& input) {
            return ReflectedSerialisation<TElement>::serialiseElement(component, input.get());
        }
    };

    template<typename T, bool WaitOnAccess, typename ValueContainer>
    struct ReflectedSerialisation<AsyncResource<T, WaitOnAccess, ValueContainer>> {
        using ResourceType = AsyncResource<T, WaitOnAccess, ValueContainer>;

        static void deserialiseElement(ECS::Component& component, ResourceType& out, const Carrot::DocumentElement& doc) requires IsResourceSerialisable<T, ValueContainer> {
            out.startLoad(doc);
        }

        static Carrot::DocumentElement serialiseElement(const ECS::Component& component, const ResourceType& input) requires IsResourceSerialisable<T, ValueContainer> {
            return input.serialise();
        }
    };

    /**
     * Templated version of BaseComponentPropertyReflection which has a pointer-to-member to the property inside the component
     */
    template<typename TComponent, typename TProperty>
    struct ComponentPropertyReflection: BaseComponentPropertyReflection {
        TProperty TComponent::*propertyPtr;
        std::string name;

        ComponentPropertyReflection(TProperty TComponent::*ptr, std::string name, std::string publicName, bool mandatory, ComponentReflectionData* pReflect)
        : BaseComponentPropertyReflection(std::move(name), std::move(publicName), mandatory, pReflect)
        , propertyPtr(ptr)
        {}

        void deserialise(Carrot::ECS::Component& component, const Carrot::DocumentElement& doc) const override {
            TProperty& ref = static_cast<TComponent&>(component).*propertyPtr;
            Carrot::ECS::ReflectedSerialisation<TProperty>::deserialiseElement(component, ref, doc);
        }

        [[nodiscard]] Carrot::DocumentElement serialise(const Carrot::ECS::Component& component) const override {
            const TProperty& ref = static_cast<const TComponent&>(component).*propertyPtr;
            return Carrot::ECS::ReflectedSerialisation<TProperty>::serialiseElement(component, ref);
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
    class ComponentReflectionData {
    public:
        ComponentReflectionData() = default;
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

#define COMMA , // wtf C++
#define PROPERTY(Type, Name, PublicName, DefaultValue, Getter, Setter) using FIELD_NAME_CONCAT(PropertyWrapperType, Name) = PropertyWrapper<TSelf COMMA Type>;\
    FIELD(FIELD_NAME_CONCAT(PropertyWrapperType, Name), Name, PublicName, FIELD_NAME_CONCAT(PropertyWrapperType, Name)(*this, DefaultValue, Getter, Setter))

#define FIELD_CONFIG(Name, ConfigLambda) \
    static inline int FIELD_NAME_CONCAT(_field_config, FIELD_NAME_CONCAT(Name, __COUNTER__)) = \
        []() { ConfigLambda(FIELD_NAME_CONCAT(_field_, Name)); return 0; }()

namespace Carrot::ECS {
    template<typename TComponent>
    struct ReflectionComponent: public IdentifiableComponent<TComponent> {
        using TSelf = TComponent;
        static inline ::Carrot::ECS::ComponentReflectionData Reflection{};
        explicit ReflectionComponent(Carrot::ECS::Entity entity): IdentifiableComponent<TComponent>(std::move(entity)) {};

        explicit ReflectionComponent(const Carrot::DocumentElement& doc, Carrot::ECS::Entity entity): ReflectionComponent(std::move(entity)) { }

        void deserialise(const Carrot::DocumentElement& doc) override {
            Reflection.deserialise(*this, doc);
        }

        Carrot::DocumentElement serialise() const override { return Reflection.serialise(*this); }

        const char *const getName() const override {
            return Carrot::Identifiable<TComponent>::getStringRepresentation();
        }

        std::unique_ptr<Component> duplicate(const Carrot::ECS::Entity& newOwner) const override {
            auto result = std::make_unique<TComponent>(newOwner);
            for (const ::Carrot::ECS::BaseComponentPropertyReflection* pReflect : Reflection.getProperties()) {
                pReflect->duplicateProperty(*this, *result);
            }
            return result;
        }
    };
}
