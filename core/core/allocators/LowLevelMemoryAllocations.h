//
// Created by jglrxavpok on 04/08/2024.
//

#pragma once

namespace Carrot {
    constexpr std::size_t DefaultAlign = 16;

    void* alloc(std::size_t size, std::size_t alignment = DefaultAlign);
    void free(void* p);
}