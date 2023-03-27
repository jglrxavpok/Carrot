#include "Identifiable.h"
#include <typeinfo>
#include <mutex>

// https://gpfault.net/posts/mapping-types-to-values.txt.html
template<typename Type>
const Carrot::ComponentID Carrot::Identifiable<Type>::getID() {
    static const Carrot::ComponentID id = Carrot::requestComponentID();
    static std::once_flag flag;
    std::call_once(flag, [](Carrot::ComponentID componentID) {
        Carrot::IdentifiableNames.getOrCompute(id, []() { return getStringRepresentation(); });
        Carrot::IdentifiableIDs.getOrCompute(getStringRepresentation(), [idCopy = id]() { return idCopy; });
    }, id);
    return id;
}