
#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <atomic>

#ifdef __APPLE__
#define ALIGNED_MALLOC(x) malloc
#else
#define ALIGNED_MALLOC(x) aligned_alloc(16, x)
#endif
#define ALIGNED_FREE free

// FIX - this locks around memory assignment - this can be made more efficient by avoiding this at which point spinlocks will be justified....
// Follow the HISSTools C++ design for this.... however use separate freeing locks so that memory is always freed in the assignment thread by waiting...
// All memory assignments are aligned in order that the memory is suitable for vector ops etc.

template<class T>
class MemorySwap
{
    
public:
    
    class Ptr
    {
        friend MemorySwap;
        
    public:
        
        Ptr(Ptr&& p) :mOwner(p.mOwner), mPtr(p.mPtr), mSize(p.mSize)
        {
            p.mOwner = nullptr;
            p.mPtr = nullptr;
            p.mSize = 0;
        }
        
        ~Ptr() { clear(); }
        
        void clear()
        {
            if (mOwner) mOwner->unlock();
            mOwner = nullptr;
            mPtr = nullptr;
            mSize = 0;
        }
        
        T *get() { return mPtr; }
        uintptr_t getSize() { return mSize; }
        
    private:
        
        Ptr() : mOwner(nullptr), mPtr(nullptr), mSize(0)
        {}
        
        Ptr(MemorySwap *owner, T *ptr, uintptr_t size)
        : mOwner(owner), mPtr(mOwner ? ptr : nullptr), mSize(mOwner ? size : 0)
        {}
        
        
        Ptr(const Ptr& p) = delete;
        Ptr operator = (const Ptr& p) = delete;
        
        
        MemorySwap *mOwner;
        T *mPtr;
        uintptr_t mSize;
    };
    
    // Alloc and free routine prototypes
    
    typedef T *(*AllocFunc) (uintptr_t size, uintptr_t nominalSize);
    typedef void (*FreeFunc) (T *);
    
    // Constructor (standard allocation)
    
    MemorySwap(uintptr_t size, uintptr_t nominalSize)
    : mLock(false), mPtr(nullptr), mSize(0), mFreeFunction(nullptr)
    {
        if (size)
            set(ALIGNED_MALLOC(size), nominalSize, ALIGNED_FREE);
    }
    
    // Constructor (custom allocation)
    
    MemorySwap(AllocFunc allocFunction, FreeFunc freeFunction, uintptr_t size, uintptr_t nominalSize)
    : mLock(false), mPtr(nullptr), mSize(0), mFreeFunction(nullptr)
    {
        if (size)
            set(allocFunction(size, nominalSize), nominalSize, freeFunction);
    }
    
    ~MemorySwap()
    {
        clear();
    }
    
    // Free - frees the memory immediately
    
    void clear()
    {
        lock();
        set(nullptr, 0, nullptr);
        unlock();
    }
    
    // Access - this routine will lock to get access to the memory struct and safely return the pointer
    
    Ptr access()
    {
        lock();
        return Ptr(this, mPtr, mSize);
    }
    
    // Attempt - This non-blocking routine will attempt to get the pointer but fail if the pointer is being altered / accessed in another thread
    
    Ptr attempt()
    {
        if (tryLock())
            return Ptr(this, mPtr, mSize);
        
        return Ptr();
    }
    
    // Swap - this routine will lock to get access to the memory struct and place a given ptr and nominal size in the new slots
    // This pointer is owned ouside the object and will *NOT* be freed
    
    Ptr swap(void *ptr, uintptr_t nominalSize)
    {
        lock();
        set(ptr, nominalSize, nullptr);
        
        return Ptr(this, mPtr, mSize);
    }
    
    // Grow - this routine will lock to get access to the memory struct and allocate new memory if required, swapping it in safely
    
    Ptr grow(uintptr_t size, uintptr_t nominalSize)
    {
        lock();
        
        if (mSize < nominalSize)
            set(ALIGNED_MALLOC(size), nominalSize, ALIGNED_FREE);
        
        return Ptr(this, mPtr, mSize);
    }
    
    // Equal - This routine will lock to get access to the memory struct and allocate new memory unless the sizes are equal, placing the memory in the new slots
    
    Ptr equal(uintptr_t size, uintptr_t nominalSize)
    {
        lock();
        
        if (mSize != nominalSize)
            set(ALIGNED_MALLOC(size), nominalSize, ALIGNED_FREE);
        
        return Ptr(this, mPtr, mSize);
    }
    
    // This routine will lock to get access to the memory struct and allocate new memory if required
    
    Ptr grow(AllocFunc allocFunction, FreeFunc freeFunction, uintptr_t size, uintptr_t nominalSize)
    {
        lock();
        
        if (mSize < nominalSize)
            set(allocFunction(size, nominalSize), nominalSize, freeFunction);
        
        return Ptr(this, mPtr, mSize);
    }
    
    // This routine will lock to get access to the memory struct and allocate new memory unless the sizes are equal
    
    Ptr equal(AllocFunc allocFunction, FreeFunc freeFunction, uintptr_t size, uintptr_t nominalSize)
    {
        lock();
        
        if (mSize != nominalSize)
            set(allocFunction(size, nominalSize), nominalSize, freeFunction);
        
        return Ptr(this, mPtr, mSize);
    }
    
private:
    
    void set(T *ptr, uintptr_t nominalSize, FreeFunc freeFunction)
    {
        if (mFreeFunction)
            mFreeFunction(mPtr);
        
        mPtr = ptr;
        mSize = ptr ? nominalSize : 0;
        mFreeFunction = freeFunction;
    }
    
    bool tryLock()
    {
        bool expected = false;
        return mLock.compare_exchange_strong(expected, true);
    }
    
    void lock()
    {
        bool expected = false;
        while (!mLock.compare_exchange_weak(expected, true));
    }
    
    void unlock()
    {
        bool expected = true;
        mLock.compare_exchange_strong(expected, false);
    }
    
    std::atomic<bool> mLock;
    
    T *mPtr;
    uintptr_t mSize;
    FreeFunc mFreeFunction;
};

