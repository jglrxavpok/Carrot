//
// Created by jglrxavpok on 05/02/2025.
//

#pragma once
#include <core/containers/Pair.hpp>
#include <core/containers/Vector.hpp>
#include <core/utils/Types.h>

namespace toml {
    inline namespace v3 {
        class table;
    }
}

namespace Carrot {

    enum class DocumentType {
        Bool,
        String,
        Int64,
        Double,
        Array,
        Object,
    };

    /// A document is a tree-like structure that aims to store generic data.
    /// It is intended to be an abstraction over TOML, JSON and a custom binary format.
    ///
    /// Elements are stored by keys, and keys are not guaranteed to be in order.
    /// The data types are:
    /// - bool
    /// - string
    /// - int64
    /// - double
    /// - array
    /// - object (a Document is always an object), basically a hashmap
    ///
    /// DocumentElement represents a generic element of a document.
    /// Use the isX functions to determine the actual type of the element.
    class DocumentElement {
        using ElementMap = std::unordered_map<std::string, DocumentElement>;

    public:
        class ArrayView;
        class ObjectView;

        /// Creates an element with the given type.
        explicit DocumentElement(DocumentType type = DocumentType::Object);
        DocumentElement(const DocumentElement&) = default;
        DocumentElement(DocumentElement&&) noexcept = default;
        DocumentElement& operator=(const DocumentElement&) = default;
        DocumentElement& operator=(DocumentElement&&) noexcept = default;

    public:
        /// Changes the type and resets this element to its default value (empty array, empty object, false, 0).
        void reset(DocumentType type);

        /// Changes the type of this element to Bool, and sets its value
        DocumentElement& operator=(bool b);

        /// Changes the type of this element to String, and sets its value
        DocumentElement& operator=(const std::string&);
        /// Changes the type of this element to String, and sets its value
        DocumentElement& operator=(const char*);

        /// Changes the type of this element to Int64, and sets its value
        DocumentElement& operator=(i64);

        /// Changes the type of this element to Double, and sets its value
        DocumentElement& operator=(double);

    public: // vector like methods
        /// Sets the reserved size of this element. If this element is not an Array, throws
        /// See Carrot::Vector::ensureReserve
        void setReserve(i64 newSize);

        /// Sets the size of this element. If this element is not an Array, throws
        void setSize(i64 newSize);

        /// Adds an empty element at the end of this array. Throws if this element is not an array
        DocumentElement& pushBack();

        /// Adds an element with the type Bool at the end of this array. Throws if this element is not an array
        DocumentElement& pushBack(bool b);

        /// Adds an element with the type String at the end of this array. Throws if this element is not an array
        DocumentElement& pushBack(const std::string& s);
        /// Adds an element with the type String at the end of this array. Throws if this element is not an array
        DocumentElement& pushBack(const char* s);

        /// Adds an element with the type Int64 at the end of this array. Throws if this element is not an array
        DocumentElement& pushBack(i64 i);

        /// Adds an element with the type Double at the end of this array. Throws if this element is not an array
        DocumentElement& pushBack(double d);

    public:
        /// Gets the content of this element. Throws if type is not correct
        bool getAsBool() const;

        /// Gets the content of this element. Throws if type is not correct
        std::string_view getAsString() const;

        /// Gets the content of this element. Throws if type is not correct
        i64 getAsInt64() const;

        /// Gets the content of this element. Throws if type is not correct
        double getAsDouble() const;

        /// Gets the content of this element. Throws if type is not correct
        ArrayView getAsArray() const;

        /// Gets the content of this element. Throws if type is not correct
        ObjectView getAsObject() const;

    public:
        /// Returns true if this element contains the given key. Throws if this element is not an Object
        bool contains(const std::string& key) const;

        /// Gets the element at the given index, if this element is an array. If not, throws
        DocumentElement& get(i64 index);
        /// Gets the element at the given index, if this element is an array. If not, throws
        const DocumentElement& get(i64 index) const;

        /// Gets the element at the given index, if this element is an array. If not, throws
        DocumentElement& operator[](i64 index);

        /// Gets the element at the given index, if this element is an array. If not, throws
        const DocumentElement& operator[](i64 index) const;

        /// Gets the element at the given key, if this element is an object. If not, adds it
        DocumentElement& get(const std::string& key);

        /// Gets the element at the given key, if this element is an object. If not, throws
        DocumentElement& at(const std::string& key);
        /// Gets the element at the given key, if this element is an object. If not, throws
        const DocumentElement& at(const std::string& key) const;

        /// Gets the element at the given key, if this element is an object. If not, throws
        const DocumentElement& get(const std::string& key) const;

        /// Gets the element at the given key, if this element is an object. If not, adds it
        DocumentElement& operator[](const std::string& key);

        /// Gets the element at the given key, if this element is an object. If not, throws
        const DocumentElement& operator[](const std::string& key) const;

    public:
        DocumentType getType() const;
        bool isBool() const;
        bool isString() const;
        bool isInt64() const;
        bool isDouble() const;
        bool isArray() const;
        bool isObject() const;
        bool is(DocumentType type) const;

        /// Returns the number of children of this element.
        /// If this element is an array, returns the size of the array.
        /// If this element is an object, returns the number of entries inside it.
        /// Otherwise, throws.
        i64 getSubElementCount() const;

    public:
        /// View of the sub-elements of a given element.
        /// Intended for usage in for each
        class ArrayView {
        public:
            struct Iterator {
                Iterator(ArrayView* view);

                bool operator==(const Iterator& o) const;
                bool operator!=(const Iterator& o) const;

                Iterator& operator++();
                const DocumentElement& operator*() const;
                const DocumentElement* operator->() const;

            private:
                i64 currentIndex = 0;
                ArrayView* pView = nullptr;

                friend class ::Carrot::DocumentElement::ArrayView;
            };

        public:
            const DocumentElement& operator[](i64 index) const;
            Iterator begin();
            Iterator end();
            i64 getSize() const;

        private:
            ArrayView(const DocumentElement& doc);
            const DocumentElement* doc = nullptr;

            friend class DocumentElement;
        };

        /// View of the sub-elements of a given element.
        /// Intended for usage in for each
        class ObjectView {
        public:
            struct Iterator {
                Iterator(const ObjectView* view);

                bool operator==(const Iterator& o) const;
                bool operator!=(const Iterator& o) const;

                Iterator& operator++();
                const ElementMap::const_iterator::value_type& operator*() const;
                const ElementMap::const_iterator::value_type* operator->() const;

            private:
                const ObjectView* pView = nullptr;
                ElementMap::const_iterator currentIt;

                friend class ::Carrot::DocumentElement::ObjectView;
            };

        public:
            [[nodiscard]] Iterator begin();
            [[nodiscard]] Iterator end();
            [[nodiscard]] Iterator begin() const;
            [[nodiscard]] Iterator end() const;
            i64 getSize() const;

        private:
            ObjectView(const DocumentElement& doc);
            const DocumentElement* doc = nullptr;

            friend class DocumentElement;
        };

    private:
        // storage, depends on type
        union {
            bool b;
            i64 i;
            double d;
        } primitive{};

        std::string string;
        Vector<std::unique_ptr<DocumentElement>> array;
        ElementMap elements;

        // end of storage
        DocumentType type { DocumentType::Object };

        friend toml::table& operator<<(toml::table& out, const DocumentElement& doc);
        friend DocumentElement& operator>>(const toml::table& in, DocumentElement& doc);
    };

    toml::table& operator<<(toml::table& out, const DocumentElement& doc);
    DocumentElement& operator>>(const toml::table& in, DocumentElement& doc);
    // TODO: binary format

} // Carrot
