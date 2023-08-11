
#ifndef HISSTOOLS_MEMORY_SWAP_HPP
#define HISSTOOLS_MEMORY_SWAP_HPP

#include <cstdint>
#include <cstdlib>
#include <atomic>
#include <functional>

#include "thread_locks.hpp"
#include "namespace.hpp"

#ifdef _WIN32
#include <malloc.h>
#endif

// FIX - locks around memory assignment - can be made more efficient by avoiding this
// Consider following the HISSTools C++ design for this....
// Use separate freeing locks so the memory is always freed in the assignment thread
// All memory assignments are aligned in order that the memory is suitable for vector ops etc.

HISSTOOLS_NAMESPACE_START()

template <class T>
class memory_swap
{
    
public:
    
    // Alloc and free routine prototypes
    
    typedef std::function<T *(uintptr_t size)> AllocFunc;
    typedef std::function<void (T *) > FreeFunc;
    
    class Ptr
    {
        friend memory_swap;
        
    public:
        
        Ptr(Ptr&& p)
        : m_owner(p.m_owner), m_pointer(p.m_pointer), m_size(p.m_size)
        {
            p.m_owner = nullptr;
            p.m_pointer = nullptr;
            p.m_size = 0;
        }
        
        ~Ptr() { clear(); }
        
        void clear()
        {
            if (m_owner) m_owner->unlock();
            m_owner = nullptr;
            m_pointer = nullptr;
            m_size = 0;
        }
        
        void swap(T *ptr, uintptr_t size)
        {
            update(&memory_swap::set, ptr, size, nullptr);
        }
        
        void grow(uintptr_t size)
        {
            grow(&allocate, &deallocate, size);
        }
        
        void equal(uintptr_t size)
        {
            equal(&allocate, &deallocate, size);
        }
        
        void grow(AllocFunc alloc_function, FreeFunc free_function, uintptr_t size)
        {
            update_allocate_if(alloc_function, free_function, size, std::greater<uintptr_t>());
        }
        
        void equal(AllocFunc alloc_function, FreeFunc free_function, uintptr_t size)
        {
            update_allocate_if(alloc_function, free_function, size, std::not_equal_to<uintptr_t>());
        }
        
        T *get() { return m_pointer; }
        uintptr_t size() { return m_size; }
        
    private:
        
        Ptr()
        : m_owner(nullptr), m_pointer(nullptr), m_size(0)
        {}
        
        Ptr(memory_swap *owner)
        : m_owner(owner)
        , m_pointer(m_owner ? m_owner->m_pointer : nullptr)
        , m_size(m_owner ? m_owner->m_size : 0)
        {}
        
        Ptr(const Ptr& p) = delete;
        Ptr operator = (const Ptr& p) = delete;
        
        template <typename Op>
        void update_allocate_if(AllocFunc alloc_function, FreeFunc free_function, uintptr_t size, Op op)
        {
            update(&memory_swap::allocate_locked_if<Op>, alloc_function, free_function, size, op);
        }
        
        template <typename Op, typename ...Args>
        void update(Op op, Args...args)
        {
            if (m_owner)
            {
                (m_owner->*op)(args...);
                m_pointer = m_owner->m_pointer;
                m_size = m_owner->m_size;
            }
        }
        
        memory_swap *m_owner;
        T *m_pointer;
        uintptr_t m_size;
    };
    
    // Constructor (standard allocation)
    
    memory_swap(uintptr_t size)
    : m_pointer(nullptr), m_size(0), m_free_function(nullptr)
    {
        if (size)
            set(allocate(size), size, &deallocate);
    }
    
    // Constructor (custom allocation)
    
    memory_swap(AllocFunc alloc_function, FreeFunc free_function, uintptr_t size)
    : m_pointer(nullptr), m_size(0), m_free_function(nullptr)
    {
        if (size)
            set(alloc_function(size), size, free_function);
    }
    
    ~memory_swap()
    {
        clear();
    }
    
    memory_swap(const memory_swap&) = delete;
    memory_swap& operator = (const memory_swap&) = delete;

    memory_swap(memory_swap&& obj)
    : m_pointer(nullptr), m_size(0), m_free_function(nullptr)
    {
        *this = std::move(obj);
        obj.m_pointer = nullptr;
        obj.m_free_function = nullptr;
    }
    
    memory_swap& operator = (memory_swap&& obj)
    {
        clear();
        obj.lock();
        m_pointer = obj.m_pointer;
        m_size = obj.m_size;
        m_free_function = obj.m_free_function;
        obj.m_pointer = nullptr;
        obj.m_free_function = nullptr;
        obj.unlock();
        
        return *this;
    }
    
    // frees the memory immediately
    
    void clear()
    {
        swap(nullptr, 0);
    }
    
    // lock to get access to the memory struct and safely return the pointer
    
    Ptr access()
    {
        lock();
        return Ptr(this);
    }
    
    // This non-blocking routine attempts to lock the pointer but fails if it is in another thread
    
    Ptr attempt()
    {
        return try_lock() ? Ptr(this) : Ptr();
    }
    
    Ptr swap(T *ptr, uintptr_t size)
    {
        lock();
        set(ptr, size, nullptr);
        return Ptr(this);
    }
    
    Ptr grow(uintptr_t size)
    {
        return grow(&allocate, &deallocate, size);
    }
    
    Ptr equal(uintptr_t size)
    {
        return equal(&allocate, &deallocate, size);
    }
    
    Ptr grow(AllocFunc alloc_function, FreeFunc free_function, uintptr_t size)
    {
        return allocate_if(alloc_function, free_function, size, std::greater<uintptr_t>());
    }
    
    Ptr equal(AllocFunc alloc_function, FreeFunc free_function, uintptr_t size)
    {
        return allocate_if(alloc_function, free_function, size, std::not_equal_to<uintptr_t>());
    }
    
private:
    
    template <typename Op>
    Ptr allocate_if(AllocFunc alloc_function, FreeFunc free_function, uintptr_t size, Op op)
    {
        lock();
        allocate_locked_if(alloc_function, free_function, size, op);
        return Ptr(this);
    }
    
    template <typename Op>
    void allocate_locked_if(AllocFunc alloc_function, FreeFunc free_function, uintptr_t size, Op op)
    {
        if (op(size, m_size))
            set(alloc_function(size), size, free_function);
    }
    
    void set(T *ptr, uintptr_t size, FreeFunc free_function)
    {
        if (m_free_function)
            m_free_function(m_pointer);
        
        m_pointer = ptr;
        m_size = ptr ? size : 0;
        m_free_function = free_function;
    }
    
    bool try_lock()
    {
        return m_lock.attempt();
    }
    
    void lock()
    {
        m_lock.acquire();
    }
    
    void unlock()
    {
        m_lock.release();
    }
    
#ifdef _WIN32
    static T* allocate(size_t size)
    {
        return static_cast<T *>(_aligned_malloc(size * sizeof(T), 16));
    }
    
    static void deallocate(T *ptr)
    {
        _aligned_free(ptr);
    }
#else
#ifdef __APPLE__
    static T* allocate(size_t size)
    {
        return reinterpret_cast<T *>(malloc(size * sizeof(T)));
    }
#else
    static T* allocate(size_t size)
    {
        return reinterpret_cast<T *>(aligned_alloc(16, size * sizeof(T)));
    }
#endif

    static void deallocate(T *ptr)
    {
        free(ptr);
    }
#endif
    
    thread_lock m_lock;
    
    T *m_pointer;
    uintptr_t m_size;
    FreeFunc m_free_function;
};

HISSTOOLS_NAMESPACE_END()

#endif /* HISSTOOLS_MEMORY_SWAP_HPP */
