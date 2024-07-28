//
// Created by jglrxavpok on 09/06/2024.
//

#include <cstdint>
#include <core/utils/Profiling.h>
#include <mimalloc.h>
//#include <mimalloc-new-delete.h>

#define USE_DEFAULT_ALLOCATOR 0
#define MEM_TRACKING 0

#if MEM_TRACKING == 1 and defined(TRACY_ENABLE)
    #define OnNew(ptr, count) TracyAllocS(ptr, count, 20)
    #define OnDelete(ptr) TracyFreeS(ptr, 20)
#else
    #define OnNew(ptr, count)
    #define OnDelete(ptr)
#endif

#if USE_DEFAULT_ALLOCATOR != 1
void   operator delete(void* p) {
    OnDelete(p);
    mi_free(p);
}

void   operator delete[](void* p) {
    OnDelete(p);
    mi_free(p);
}

void   operator delete(void* p, std::align_val_t align) {
    OnDelete(p);
    mi_free(p);
}

void   operator delete[](void* p, std::align_val_t align) {
    OnDelete(p);
    mi_free(p);
}


void   operator delete(void* p, std::size_t n, std::align_val_t align) {
    OnDelete(p);
    mi_free(p);
}

void   operator delete[](void* p, std::size_t n, std::align_val_t align) {
    OnDelete(p);
    mi_free(p);
}

void*  operator new(std::size_t count) noexcept(false) {
    auto ptr = mi_new(count);
    OnNew(ptr, count);
    return ptr;
}
void*  operator new[](std::size_t count) noexcept(false) {
    auto ptr = mi_new(count);
    OnNew(ptr, count);
    return ptr;
}

void*  operator new( std::size_t count, std::align_val_t align) noexcept(false) {
    auto ptr = mi_new_aligned(count, static_cast<std::size_t>(align));
    OnNew(ptr, count);
    return ptr;
}

void*  operator new[]( std::size_t count, std::align_val_t align) noexcept(false) {
    auto ptr = mi_new_aligned(count, static_cast<std::size_t>(align));
    OnNew(ptr, count);
    return ptr;
}

void*  operator new  ( std::size_t count, const std::nothrow_t& tag) {
    auto ptr = mi_new_nothrow(count);
    OnNew(ptr, count);
    return ptr;
}

void*  operator new[]( std::size_t count, const std::nothrow_t& tag) {
    auto ptr = mi_new_nothrow(count);
    OnNew(ptr, count);
    return ptr;
}

void*  operator new  ( std::size_t count, std::align_val_t al, const std::nothrow_t&) {
    auto ptr = mi_new_aligned_nothrow(count, static_cast<std::size_t>(al));
    OnNew(ptr, count);
    return ptr;
}

void*  operator new[]( std::size_t count, std::align_val_t al, const std::nothrow_t&) {
    auto ptr = mi_new_aligned_nothrow(count, static_cast<std::size_t>(al));
    OnNew(ptr, count);
    return ptr;
}
#endif