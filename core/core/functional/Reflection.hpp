//
// Created by jglrxavpok on 14/03/2023.
//

#pragma once

/**
 * The goal is this collections of templates is to help do some "compile-time reflection" about functions.
 * For instance: getting the arguments or return type of a function
 */
// Welcome to template hell

namespace Carrot::Reflection {
    // thanks to Raymond Chen https://devblogs.microsoft.com/oldnewthing/20200713-00/?p=103978
    template<typename T>
    struct FunctionTraits;

    template<typename R, typename... Args>
    struct FunctionTraits<R(*)(Args...)>
    {
        using Pointer = R(*)(Args...);
        using RetType = R;
        using ArgTypes = std::tuple<Args...>;
        static constexpr std::size_t ArgCount = sizeof...(Args);
        template<std::size_t N>
        using NthArg = std::tuple_element_t<N, ArgTypes>;
        using FirstArg = NthArg<0>;
        using LastArg = NthArg<ArgCount - 1>;

        static constexpr bool IsMemberFunction = false;
    };

    template<typename R, typename T, typename... Args>
    struct FunctionTraits<R(T::*)(Args...)>
    {
        using Pointer = R(T::*)(Args...);
        using RetType = R;
        using ArgTypes = std::tuple<Args...>;
        static constexpr std::size_t ArgCount = sizeof...(Args);
        template<std::size_t N>
        using NthArg = std::tuple_element_t<N, ArgTypes>;
        using FirstArg = NthArg<0>;
        using LastArg = NthArg<ArgCount - 1>;

        using OwningType = T;
        static constexpr bool IsMemberFunction = true;
    };

    template<typename R>
    struct FunctionTraits<R(*)()>
    {
        using Pointer = R(*)();
        using RetType = R;
        using ArgTypes = std::tuple<>;
        static constexpr std::size_t ArgCount = 0;

        static constexpr bool IsMemberFunction = false;
    };

    template<typename R, typename T>
    struct FunctionTraits<R(T::*)()>
    {
        using Pointer = R(*)();
        using RetType = R;
        using ArgTypes = std::tuple<>;
        static constexpr std::size_t ArgCount = 0;

        static constexpr bool IsMemberFunction = true;
    };
}
