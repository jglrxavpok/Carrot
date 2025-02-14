//
// Created by jglrxavpok on 12/02/2025.
//

#pragma once

#define ENUM_FLAGS_OPERATORS(Type)                                                                                                      \
inline Type operator|(Type a, Type b) {                                                                                                 \
    return static_cast<Type>(static_cast<std::underlying_type_t<Type>>(a) | static_cast<std::underlying_type_t<Type>>(b));              \
}                                                                                                                                       \
inline Type operator&(Type a, Type b) {                                                                                                 \
    return static_cast<Type>(static_cast<std::underlying_type_t<Type>>(a) & static_cast<std::underlying_type_t<Type>>(b));              \
}                                                                                                                                       \
inline Type operator~(Type a) {                                                                                                         \
    return static_cast<Type>(~static_cast<std::underlying_type_t<Type>>(a));                                                            \
}                                                                                                                                       \
inline std::underlying_type_t<Type> operator+(Type a) {                                                                                 \
    return static_cast<std::underlying_type_t<Type>>(a);                                                                                \
}                                                                                                                                       \
