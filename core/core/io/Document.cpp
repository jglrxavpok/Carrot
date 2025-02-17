//
// Created by jglrxavpok on 05/02/2025.
//

#include "Document.h"

#include <core/utils/TOML.h>

namespace Carrot {

    DocumentElement::DocumentElement(DocumentType type): type(type) {
        reset(type);
    }

    DocumentElement::DocumentElement(const DocumentElement& other) {
        *this = other;
    }

    DocumentElement& DocumentElement::operator=(const DocumentElement& other) {
        reset(other.type);
        primitive = other.primitive;

        string = other.string;
        array.resize(other.array.size());
        for (i32 i = 0; i < array.size(); i++) {
            array[i] = std::make_unique<DocumentElement>(*other.array[i]);
        }
        elements = other.elements;
        return *this;
    }


#pragma region SetContent
    void DocumentElement::reset(DocumentType type) {
        this->type = type;
        memset(&primitive, 0, sizeof(primitive));
        elements.clear();
        array.clear();
        string.clear();
    }

    DocumentElement& DocumentElement::operator=(bool b) {
        reset(DocumentType::Bool);
        primitive.b = b;
        return *this;
    }

    DocumentElement& DocumentElement::operator=(const std::string& s) {
        reset(DocumentType::String);
        string = s;
        return *this;
    }

    DocumentElement& DocumentElement::operator=(const char* s) {
        reset(DocumentType::String);
        string = s;
        return *this;
    }

    DocumentElement& DocumentElement::operator=(i64 i) {
        reset(DocumentType::Int64);
        primitive.i = i;
        return *this;
    }

    DocumentElement& DocumentElement::operator=(double d) {
        reset(DocumentType::Double);
        primitive.d = d;
        return *this;
    }
#pragma endregion SetContent

#pragma region VectorLike
    void DocumentElement::setReserve(i64 newSize) {
        verify(type == DocumentType::Array, "wrong type!");
        array.ensureReserve(newSize);
    }

    void DocumentElement::setSize(i64 newSize) {
        verify(type == DocumentType::Array, "wrong type!");
        i32 oldSize = array.size();
        array.resize(newSize);
        for(i32 i = oldSize; i < newSize; i++) {
            array[i] = std::make_unique<DocumentElement>();
        }
    }

    DocumentElement& DocumentElement::pushBack() {
        verify(type == DocumentType::Array, "wrong type!");
        return *array.emplaceBack(std::make_unique<DocumentElement>());
    }

    DocumentElement& DocumentElement::pushBack(bool b) {
        verify(type == DocumentType::Array, "wrong type!");
        return pushBack() = b;
    }

    DocumentElement& DocumentElement::pushBack(const std::string& s) {
        verify(type == DocumentType::Array, "wrong type!");
        return pushBack() = s;
    }
    DocumentElement& DocumentElement::pushBack(const char* s) {
        verify(type == DocumentType::Array, "wrong type!");
        return pushBack() = s;
    }

    DocumentElement& DocumentElement::pushBack(i64 i) {
        verify(type == DocumentType::Array, "wrong type!");
        return pushBack() = i;
    }

    DocumentElement& DocumentElement::pushBack(double d) {
        verify(type == DocumentType::Array, "wrong type!");
        return pushBack() = d;
    }
#pragma endregion VectorLike

#pragma region GetContent
    bool DocumentElement::getAsBool() const {
        verify(type == DocumentType::Bool, "wrong type!");
        return primitive.b;
    }

    std::string_view DocumentElement::getAsString() const {
        verify(type == DocumentType::String, "wrong type!");
        return string;
    }

    i64 DocumentElement::getAsInt64() const {
        verify(type == DocumentType::Int64, "wrong type!");
        return primitive.i;
    }

    double DocumentElement::getAsDouble() const {
        verify(type == DocumentType::Double, "wrong type!");
        return primitive.d;
    }

    DocumentElement::ArrayView DocumentElement::getAsArray() const {
        verify(type == DocumentType::Array, "wrong type!");
        return ArrayView{*this};
    }

    DocumentElement::ObjectView DocumentElement::getAsObject() const {
        verify(type == DocumentType::Object, "wrong type!");
        return ObjectView{*this};
    }
#pragma endregion GetContent

#pragma region Getters
    bool DocumentElement::contains(const std::string& key) const {
        verify(isObject(), "wrong type!");
        return elements.contains(key);
    }

    DocumentElement& DocumentElement::get(i64 index) {
        return *array[index];
    }
    const DocumentElement& DocumentElement::get(i64 index) const {
        return *array[index];
    }
    DocumentElement& DocumentElement::operator[](i64 index) {
        return *array[index];
    }
    const DocumentElement& DocumentElement::operator[](i64 index) const {
        return *array[index];
    }

    DocumentElement& DocumentElement::get(const std::string& key) {
        return elements[key];
    }
    DocumentElement& DocumentElement::at(const std::string& key) {
        return elements.at(key);
    }
    const DocumentElement& DocumentElement::at(const std::string& key) const {
        return elements.at(key);
    }
    const DocumentElement& DocumentElement::get(const std::string& key) const {
        return elements.at(key);
    }
    DocumentElement& DocumentElement::operator[](const std::string& key) {
        return elements[key];
    }
    const DocumentElement& DocumentElement::operator[](const std::string& key) const {
        return elements.at(key);
    }
#pragma endregion Getters

#pragma region Introspection
    DocumentType DocumentElement::getType() const {
        return type;
    }
    bool DocumentElement::isBool() const {
        return type == DocumentType::Bool;
    }
    bool DocumentElement::isString() const {
        return type == DocumentType::String;
    }
    bool DocumentElement::isInt64() const {
        return type == DocumentType::Int64;
    }
    bool DocumentElement::isDouble() const {
        return type == DocumentType::Double;
    }
    bool DocumentElement::isArray() const {
        return type == DocumentType::Array;
    }
    bool DocumentElement::isObject() const {
        return type == DocumentType::Object;
    }
    bool DocumentElement::is(DocumentType type) const {
        return this->type == type;
    }

    i64 DocumentElement::getSubElementCount() const {
        verify(isArray() || isObject(), "wrong type!");
        if (isArray()) {
            return array.size();
        } else {
            return elements.size();
        }
    }
#pragma endregion Introspection

#pragma region Views
    // ================================
    // Views
    // ================================

    DocumentElement::ArrayView::ArrayView(const DocumentElement& elem): doc(&elem) {}
    DocumentElement::ArrayView::Iterator::Iterator(ArrayView* pView): pView(pView) {}

    const DocumentElement& DocumentElement::ArrayView::operator[](i64 index) const {
        return (*doc)[index];
    }

    DocumentElement::ArrayView::Iterator DocumentElement::ArrayView::begin() {
        auto it = Iterator{this};
        it.currentIndex = 0;
        return it;
    }

    DocumentElement::ArrayView::Iterator DocumentElement::ArrayView::end() {
        auto it = Iterator{this};
        it.currentIndex = doc->array.size();
        return it;
    }

    i64 DocumentElement::ArrayView::getSize() const {
        return doc->array.size();
    }

    bool DocumentElement::ArrayView::Iterator::operator==(const Iterator& o) const {
        return pView == o.pView && currentIndex == o.currentIndex;
    }
    bool DocumentElement::ArrayView::Iterator::operator!=(const Iterator& o) const {
        return !(*this == o);
    }

    DocumentElement::ArrayView::Iterator& DocumentElement::ArrayView::Iterator::operator++() {
        currentIndex++;
        return *this;
    }

    const DocumentElement& DocumentElement::ArrayView::Iterator::operator*() const {
        return pView->doc->get(currentIndex);
    }

    const DocumentElement* DocumentElement::ArrayView::Iterator::operator->() const {
        return &pView->doc->get(currentIndex);
    }

    DocumentElement::ObjectView::ObjectView(const DocumentElement& elem): doc(&elem) {}
    DocumentElement::ObjectView::Iterator::Iterator(const ObjectView* pView): pView(pView) {}

    DocumentElement::ObjectView::Iterator DocumentElement::ObjectView::begin() {
        auto it = Iterator{this};
        it.currentIt = doc->elements.begin();
        return it;
    }

    DocumentElement::ObjectView::Iterator DocumentElement::ObjectView::end() {
        auto it = Iterator{this};
        it.currentIt = doc->elements.end();
        return it;
    }

    DocumentElement::ObjectView::Iterator DocumentElement::ObjectView::begin() const {
        auto it = Iterator{this};
        it.currentIt = doc->elements.begin();
        return it;
    }

    DocumentElement::ObjectView::Iterator DocumentElement::ObjectView::end() const {
        auto it = Iterator{this};
        it.currentIt = doc->elements.end();
        return it;
    }

    DocumentElement::ObjectView::Iterator DocumentElement::ObjectView::find(const std::string& key) const {
        auto it = Iterator{this};
        it.currentIt = doc->elements.find(key);
        return it;
    }

    i64 DocumentElement::ObjectView::getSize() const {
        return doc->elements.size();
    }

    bool DocumentElement::ObjectView::Iterator::operator==(const Iterator& o) const {
        return pView == o.pView && currentIt == o.currentIt;
    }
    bool DocumentElement::ObjectView::Iterator::operator!=(const Iterator& o) const {
        return !(*this == o);
    }
    bool DocumentElement::ObjectView::Iterator::isValid() const {
        return currentIt != pView->doc->elements.end();
    }


    DocumentElement::ObjectView::Iterator& DocumentElement::ObjectView::Iterator::operator++() {
        ++currentIt;
        return *this;
    }

    const DocumentElement::ElementMap::const_iterator::value_type& DocumentElement::ObjectView::Iterator::operator*() const {
        return currentIt.operator*();
    }

    const DocumentElement::ElementMap::const_iterator::value_type* DocumentElement::ObjectView::Iterator::operator->() const {
        return currentIt.operator->();
    }

#pragma endregion Views

#pragma region Serialisation


    toml::table& operator<<(toml::table& out, const DocumentElement& doc) {
        verify(doc.isObject(), "wrong type, only objects can be written to a TOML table!");
        std::function<std::unique_ptr<toml::node>(const DocumentElement& value)> toTOML = [&](const DocumentElement& value) -> std::unique_ptr<toml::node> {
            switch (value.getType()) {
                case DocumentType::Bool:
                    return std::make_unique<toml::value<bool>>(value.getAsBool());
                case DocumentType::String:
                    return std::make_unique<toml::value<std::string>>(value.getAsString());
                case DocumentType::Int64:
                    return std::make_unique<toml::value<i64>>(value.getAsInt64());
                case DocumentType::Double:
                    return std::make_unique<toml::value<double>>(value.getAsDouble());
                case DocumentType::Array: {
                    auto pArray = std::make_unique<toml::array>();
                    DocumentElement::ArrayView view = value.getAsArray();
                    pArray->reserve(view.getSize());
                    for (const auto& v : view) {
                        pArray->push_back(*toTOML(v));
                    }

                    return pArray;
                }
                case DocumentType::Object: {
                    auto pNested = std::make_unique<toml::table>();
                    *pNested << value;
                    return pNested;
                }

                default:
                    verify(false, "missing case");
                    return nullptr;
            }
        };
        for (const auto& [key, value] : doc.getAsObject()) {
            out.insert(key, *toTOML(value));
        }
        return out;
    }

    DocumentElement& operator>>(const toml::table& in, DocumentElement& doc) {
        doc.type = DocumentType::Object;
        std::function<void(DocumentElement& value, const toml::node& tomlNode)> load = [&](DocumentElement& value, const toml::node& tomlNode) {
            if (tomlNode.is_boolean()) {
                value.reset(DocumentType::Bool);
                value = tomlNode.as_boolean()->get();
            } else if (tomlNode.is_string()) {
                value.reset(DocumentType::String);
                value = tomlNode.as_string()->get();
            } else if (tomlNode.is_integer()) {
                value.reset(DocumentType::Int64);
                value = tomlNode.as_integer()->get();
            } else if (tomlNode.is_floating_point()) {
                value.reset(DocumentType::Double);
                value = tomlNode.as_floating_point()->get();
            } else if (tomlNode.is_array()) {
                value.reset(DocumentType::Array);
                auto* pArray = tomlNode.as_array();
                value.setSize(pArray->size());
                for (i32 i = 0; i < value.getSubElementCount(); i++) {
                    load(value[i], (*pArray)[i]);
                }
            } else if (tomlNode.is_table()) {
                value.reset(DocumentType::Object);
                for (const auto& [key, node] : *tomlNode.as_table()) {
                    load(value[std::string{ key.str() }], node);
                }
            } else {
                verify(false, "unsupported type");
            }
        };
        load(doc, in);
        return doc;
    }

#pragma endregion Serialisation
} // Carrot