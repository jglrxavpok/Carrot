#include "Identifiable.h"
#include <typeinfo>

template<typename Type>
const Carrot::ComponentID Carrot::Identifiable<Type>::id = typeid(Type).hash_code();