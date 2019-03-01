
#pragma once

#ifdef __APPLE__
#include <Accelerate/Accelerate.h>
#define ALIGNED_MALLOC malloc
#define ALIGNED_FREE free
#else
#include <emmintrin.h>
#include <malloc.h>
#include <sse_mathfun.h>
#define ALIGNED_MALLOC(x)  _aligned_malloc(x, 16)
#define ALIGNED_FREE(x)  _aligned_free(x)
#endif

template <class T, class U, int vec_size> struct SIMDVector
{
    static const int size = vec_size;
    typedef T scalar_type;
    
    SIMDVector() {}
    SIMDVector(U a) : mVal(a) {}
    
    U mVal;
};

struct SSEFloat : public SIMDVector<float, __m128, 4>
{
    SSEFloat() {}
    SSEFloat(__m128 a) : SIMDVector(a) {}
    SSEFloat(float a) : SIMDVector(_mm_set1_ps(a)) {}
    
    friend SSEFloat operator + (const SSEFloat& a, const SSEFloat& b) { return _mm_add_ps(a.mVal, b.mVal); }
    friend SSEFloat operator - (const SSEFloat& a, const SSEFloat& b) { return _mm_sub_ps(a.mVal, b.mVal); }
    friend SSEFloat operator * (const SSEFloat& a, const SSEFloat& b) { return _mm_mul_ps(a.mVal, b.mVal); }
    
    static SSEFloat unaligned_load(const float* ptr) { return _mm_loadu_ps(ptr); }
    
    void unaligned_store(float* ptr)
    {
        _mm_storeu_ps(ptr, mVal);
    }
    
    float sum()
    {
        float values[4];
        unaligned_store(values);
        return values[0] + values[1] + values[2] + values[3];
    }
};

typedef SSEFloat FloatVector;

