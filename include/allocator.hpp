
#ifndef HISSTOOLS_LIBRARY_ALLOCATOR_HPP
#define HISSTOOLS_LIBRARY_ALLOCATOR_HPP

#include <cstdlib>

#include "simd_support.hpp"
#include "namespace.hpp"

HISSTOOLS_LIBRARY_NAMESPACE_START()

namespace impl
{
    using allocate_function = void* (*)(size_t);
    using free_function = void (*)(void*);
};

// A template for wrapping functions as an allocator

template <impl::allocate_function alloc, impl::free_function dealloc>
struct function_allocator
{
    template <typename T>
    T* allocate(size_t size) { return reinterpret_cast<T*>(alloc(size * sizeof(T))); }
    
    template <typename T>
    void deallocate(T* ptr) { dealloc(ptr); }
};

using malloc_allocator = function_allocator<malloc, free>;

// Aligned allocator

struct aligned_allocator
{
    template <typename T>
    T* allocate(size_t size) { return allocate_aligned<T>(size); }
    
    template <typename T>
    void deallocate(T* ptr) { deallocate_aligned(ptr); }
};

HISSTOOLS_LIBRARY_NAMESPACE_END()

#endif /* HISSTOOLS_LIBRARY_ALLOCATOR_HPP */
