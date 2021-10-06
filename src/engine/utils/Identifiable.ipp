#include "Identifiable.h"
#include <typeinfo>

// https://gpfault.net/posts/mapping-types-to-values.txt.html
template<typename Type>
const Carrot::ComponentID Carrot::Identifiable<Type>::getID() {
    static const Carrot::ComponentID id = LastComponentID++;
    return id;
}