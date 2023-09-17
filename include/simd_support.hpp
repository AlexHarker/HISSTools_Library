
#ifndef HISSTOOLS_LIBRARY_SIMD_SUPPORT_HPP
#define HISSTOOLS_LIBRARY_SIMD_SUPPORT_HPP

#include <algorithm>
#include <bitset>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>

#include "namespace.hpp"

#if defined(__arm__) || defined(__arm64) || defined(__aarch64__)
#include <arm_neon.h>
#include <memory.h>
#include <fenv.h>
#define SIMD_COMPILER_SUPPORT_NEON 1
#elif defined(__APPLE__) || defined(__linux__) || defined(_WIN32)
#if defined(_WIN32)
#include <malloc.h>
#include <intrin.h>
#endif
#include <emmintrin.h>
#include <immintrin.h>
#endif

// ******************** MSVC SSE Support Detection ********************* //

// MSVC doesn't ever define __SSE__ so if needed set it from other defines

#ifndef __SSE__
#if defined _M_X64 || (defined _M_IX86_FP && _M_IX86_FP > 0)
#define __SSE__ 1
#endif
#endif

// ****************** Determine SIMD Compiler Support ****************** //

#define SIMD_COMPILER_SUPPORT_SCALAR 0
#define SIMD_COMPILER_SUPPORT_VEC128 1
#define SIMD_COMPILER_SUPPORT_VEC256 2
#define SIMD_COMPILER_SUPPORT_VEC512 3

HISSTOOLS_LIBRARY_NAMESPACE_START()

template <class T>
struct simd_limits
{
    static constexpr int max_size = 1;
    static constexpr int byte_width = alignof(T);
};

#if defined(__AVX512F__)
#define SIMD_COMPILER_SUPPORT_LEVEL SIMD_COMPILER_SUPPORT_VEC512

template<>
struct simd_limits<double>
{
    static constexpr int max_size = 8;
    static constexpr int byte_width = 64;
};

template<>
struct simd_limits<float>
{
    static constexpr int max_size = 16;
    static constexpr int byte_width = 64;
};

#elif defined(__AVX__)
#define SIMD_COMPILER_SUPPORT_LEVEL SIMD_COMPILER_SUPPORT_VEC256

template<>
struct simd_limits<double>
{
    static constexpr int max_size = 4;
    static constexpr int byte_width = 32;
};

template<>
struct simd_limits<float>
{
    static constexpr int max_size = 8;
    static constexpr int byte_width = 32;
};

#elif defined(__SSE__) || defined(__arm__) || defined(__arm64) || defined(__aarch64__)
#define SIMD_COMPILER_SUPPORT_LEVEL SIMD_COMPILER_SUPPORT_VEC128

#if defined (__SSE__) || defined(__arm64) || defined(__aarch64__)
template<>
struct simd_limits<double>
{
    static constexpr int max_size = 2;
    static constexpr int byte_width = 16;
};
#endif

template<>
struct simd_limits<float>
{
    static constexpr int max_size = 4;
    static constexpr int byte_width = 16;
};

#else
#define SIMD_COMPILER_SUPPORT_LEVEL SIMD_COMPILER_SUPPORT_SCALAR
#endif

// ********************* Aligned Memory Allocation ********************* //

#ifdef __APPLE__

template <class T>
T* allocate_aligned(size_t size)
{
    return static_cast<T*>(malloc(size * sizeof(T)));
}

template <class T>
void deallocate_aligned(T* ptr)
{
    free(ptr);
}

#elif defined(__linux__)

template <class T>
T* allocate_aligned(size_t size)
{
    void* mem = nullptr;
    
    if (posix_memalign(&mem, simd_limits<T>::byte_width, size * sizeof(T)))
        return nullptr;

    return static_cast<T*>(mem);
}

template <class T>
void deallocate_aligned(T* ptr)
{
    free(ptr);
}

#else

template <class T>
T* allocate_aligned(size_t size)
{
    return static_cast<T*>(_aligned_malloc(size * sizeof(T), simd_limits<T>::byte_width));
}

template <class T>
void deallocate_aligned(T* ptr)
{
    _aligned_free(ptr);
}

#endif

// ******************** Denormal Handling ******************** //

struct simd_denormals
{
    using denormal_flags = std::bitset<2>;
    
    static denormal_flags make_flags(bool daz, bool ftz)
    {
        denormal_flags denormal_state(0);
        denormal_state.set(0, daz);
        denormal_state.set(1, ftz);
        return denormal_state;
    }
    
    // Platform-specific get and set for flags
    
#if (SIMD_COMPILER_SUPPORT_LEVEL >= SIMD_COMPILER_SUPPORT_VEC128)
#if defined SIMD_COMPILER_SUPPORT_NEON
#if defined(__arm64) || defined(__aarch64__)
    static constexpr unsigned long long ftz()
    {
        // __fpcr_flush_to_zero on apple, but better to be portable
        
        return 0x01000000ULL;
    }
    
    static denormal_flags flags()
    {
        fenv_t env;
        fegetenv(&env);
        return make_flags(false, env.__fpcr & ftz());
    }
    
    static void set(denormal_flags flags)
    {
        fenv_t env;
        fegetenv(&env);
    
        if (flags.test(1))
            env.__fpcr |= ftz();
        else
            env.__fpcr ^= ftz();

        fesetenv(&env);
    }
#else
    static unsigned int& get_fpscr(fenv_t& env)
    {
        return env.__cw;
    }
    
    static constexpr unsigned int ftz()
    {
        // __fpscr_flush_to_zero on apple, but better to be portable
        
        return 0x01000000U;
    }
    
    static denormal_flags flags()
    {
        fenv_t env;
        fegetenv(&env);
        return make_flags(false, get_fpscr(env) & ftz());
    }
    
    static void set(denormal_flags flags)
    {
        fenv_t env;
        fegetenv(&env);
    
        if (flags.test(1))
            get_fpscr(env) |= ftz();
        else
            get_fpscr(env) ^= ftz();
            
        fesetenv(&env);
    }
#endif
#else
    static denormal_flags flags()
    {
        std::bitset<32> csr(_mm_getcsr());
        return make_flags(csr.test(6), csr.test(15));
    }
    
    static void set(denormal_flags flags)
    {
        std::bitset<32> csr(_mm_getcsr());
        csr.set(6, flags.test(0));
        csr.set(15, flags.test(1));
        _mm_setcsr(static_cast<unsigned int>(csr.to_ulong()));
    }
#endif
#else
    static denormal_flags flags() { return 0; }
    static void set(denormal_flags flags) {}
#endif
    
    // Set off
    
    void static off() { set(0x3); }
    
    // Set denomal handling using RAII
    
    simd_denormals() : m_flags(flags())
    {
        off();
    }
    
    simd_denormals(const simd_denormals&) = delete;
    simd_denormals& operator=(const simd_denormals&) = delete;
    
    ~simd_denormals()
    {
        set(m_flags);
    }

private:
    
    denormal_flags m_flags;
};

// ******************** Basic Data Type Definitions ******************** //

template <class T, class U, int vec_size>
struct simd_vector
{
    static constexpr int size = vec_size;
    using scalar_type = T;
    
    simd_vector() {}
    simd_vector(U a) : m_val(a) {}
    
    U m_val;
};

template <class T, int vec_size>
struct simd_type {};

// ************* A Vector of Given Size (Made of Vectors) ************** //

template <class T, int vec_size, int final_size>
struct sized_vector
{
    using sv = sized_vector; // to make lines shorter in operator
    using vector_type = simd_type<T, vec_size>;

    static constexpr int size = final_size;
    static constexpr int array_size = final_size / vec_size;
    
    sized_vector() {}
    sized_vector(const T& a) { static_iterate<>().set(*this, a); }
    sized_vector(const sized_vector* ptr) { *this = *ptr; }
    sized_vector(const T* array) { static_iterate<>().load(*this, array); }
    
    // For scalar conversions use a constructor
    
    template <class U>
    sized_vector(const sized_vector<U, 1, final_size>& vec)
    {
        static_iterate<>().set(*this, vec);
    }
    
    // Attempt to cast types directly for conversions if casts are provided
    
    template <class U>
    sized_vector(const sized_vector<U, final_size, final_size>& v)
    : sized_vector(v.m_data[0])
    {}
    
    void store(T* a) const { static_iterate<>().store(a, *this); }

    friend sv operator + (const sv& a, const sv& b) { return op(a, b, std::plus<vector_type>()); }
    friend sv operator - (const sv& a, const sv& b) { return op(a, b, std::minus<vector_type>()); }
    friend sv operator * (const sv& a, const sv& b) { return op(a, b, std::multiplies<vector_type>()); }
    friend sv operator / (const sv& a, const sv& b) { return op(a, b, std::divides<vector_type>()); }
    
    sv& operator += (const sv& b) { return (*this = *this + b); }
    sv& operator -= (const sv& b) { return (*this = *this - b); }
    sv& operator *= (const sv& b) { return (*this = *this * b); }
    sv& operator /= (const sv& b) { return (*this = *this / b); }
    
    friend sv min(const sv& a, const sv& b) { return op(a, b, std::min<vector_type>()); }
    friend sv max(const sv& a, const sv& b) { return op(a, b, std::max<vector_type>()); }
    
    friend sv operator == (const sv& a, const sv& b) { return op(a, b, std::equal_to<vector_type>()); }
    friend sv operator != (const sv& a, const sv& b) { return op(a, b, std::not_equal_to<vector_type>()); }
    friend sv operator > (const sv& a, const sv& b) { return op(a, b, std::greater<vector_type>()); }
    friend sv operator < (const sv& a, const sv& b) { return op(a, b, std::less<vector_type>()); }
    friend sv operator >= (const sv& a, const sv& b) { return op(a, b, std::greater_equal<vector_type>()); }
    friend sv operator <= (const sv& a, const sv& b) { return op(a, b, std::less_equal<vector_type>()); }
    
    vector_type m_data[array_size];
    
private:
    
    // Helpers
    
    // This template allows static loops
    
    template <int First = 0, int Last = array_size>
    struct static_iterate
    {
        using iterate = static_iterate<First + 1, Last>;
        
        template <typename Fn>
        inline void operator()(sized_vector &result, const sized_vector& a, const sized_vector& b, Fn const& fn) const
        {
            result.m_data[First] = fn(a.m_data[First], b.m_data[First]);
            iterate()(result, a, b, fn);
        }
        
        inline void load(sized_vector &v, const T* array)
        {
            v.m_data[First] = vector_type(array + First * vec_size);
            iterate().load(v, array);
        }
        
        inline void store(T* array, const sized_vector& v)
        {
            v.m_data[First].store(array + First * vec_size);
            iterate().store(array, v);
        }
        
        inline void set(sized_vector &v, const T& a)
        {
            v.m_data[First] = a;
            iterate().set(v, a);
        }
        
        template <class U>
        inline void set(sized_vector &v, const sized_vector<U, 1, final_size>& a)
        {
            v.m_data[First] = a.m_data[First];
            iterate().set(v, a);
        }
    };
    
    // This specialisation avoids infinite recursion
    
    template <int N>
    struct static_iterate<N, N>
    {
        template <typename Fn>
        void operator()(sized_vector & /*result*/, const sized_vector& /* a */, const sized_vector& /* b */, 
                                                                                Fn const& /* fn */) const {}
        
        void load(sized_vector & /* v */, const T * /* array */) {}
        void store(T * /* array */, const sized_vector& /* v */) {}
        void set(sized_vector & /* v */, const T& /* a */) {}
        
        template <class U>
        void set(sized_vector & /* v */, const sized_vector<U, 1, final_size>& /* a */) {}
    };
    
    // Op template
    
    template <typename Op>
    friend sized_vector op(const sized_vector& a, const sized_vector& b, Op op)
    {
        sized_vector result;
        static_iterate<>()(result, a, b, op);
        return result;
    }
};

// ************** Platform-Agnostic Data Type Definitions ************** //

template<>
struct simd_type<double, 1>
{
    static constexpr int size = 1;
    using scalar_type = double;
    
    simd_type() {}
    simd_type(double a) : m_val(a) {}
    simd_type(const double* a) { m_val = *a; }
    
    void store(double* a) const { *a = m_val; }
    
    friend simd_type operator + (const simd_type& a, const simd_type& b) { return a.m_val + b.m_val; }
    friend simd_type operator - (const simd_type& a, const simd_type& b) { return a.m_val - b.m_val; }
    friend simd_type operator * (const simd_type& a, const simd_type& b) { return a.m_val * b.m_val; }
    friend simd_type operator / (const simd_type& a, const simd_type& b) { return a.m_val / b.m_val; }
    
    simd_type& operator += (const simd_type& b) { return (*this = *this + b); }
    simd_type& operator -= (const simd_type& b) { return (*this = *this - b); }
    simd_type& operator *= (const simd_type& b) { return (*this = *this * b); }
    simd_type& operator /= (const simd_type& b) { return (*this = *this / b); }
    
    friend simd_type sqrt(const simd_type& a) { return std::sqrt(a.m_val); }
    
    friend simd_type round(const simd_type& a) { return std::round(a.m_val); }
    friend simd_type trunc(const simd_type& a) { return std::trunc(a.m_val); }
    
    friend simd_type min(const simd_type& a, const simd_type& b) { return std::min(a.m_val, b.m_val); }
    friend simd_type max(const simd_type& a, const simd_type& b) { return std::max(a.m_val, b.m_val); }
    friend simd_type sel(const simd_type& a, const simd_type& b, 
                                             const simd_type& c) { return c.m_val ? b.m_val : a.m_val; }
    
    friend simd_type operator == (const simd_type& a, const simd_type& b) { return a.m_val == b.m_val; }
    friend simd_type operator != (const simd_type& a, const simd_type& b) { return a.m_val != b.m_val; }
    friend simd_type operator > (const simd_type& a, const simd_type& b) { return a.m_val > b.m_val; }
    friend simd_type operator < (const simd_type& a, const simd_type& b) { return a.m_val < b.m_val; }
    friend simd_type operator >= (const simd_type& a, const simd_type& b) { return a.m_val >= b.m_val; }
    friend simd_type operator <= (const simd_type& a, const simd_type& b) { return a.m_val <= b.m_val; }
    
    friend void unzip(simd_type& out1, simd_type& out2, const simd_type& in1, const simd_type& in2)
    {
        out1 = in1;
        out2 = in2;
    }
    
    friend void zip(simd_type& out1, simd_type& out2, const simd_type& in1, const simd_type& in2)
    {
        out1 = in1;
        out2 = in2;
    }
    
    double m_val;
};

template<>
struct simd_type<float, 1>
{
    static constexpr int size = 1;
    using scalar_type = float;
    
    simd_type() {}
    simd_type(float a) : m_val(a) {}
    simd_type(const float* a) { m_val = *a; }
    
    simd_type(const simd_type<double, 1>& a) : m_val(static_cast<float>(a.m_val)) {}
    
    void store(float* a) const { *a = m_val; }
    
    friend simd_type operator + (const simd_type& a, const simd_type& b) { return a.m_val + b.m_val; }
    friend simd_type operator - (const simd_type& a, const simd_type& b) { return a.m_val - b.m_val; }
    friend simd_type operator * (const simd_type& a, const simd_type& b) { return a.m_val * b.m_val; }
    friend simd_type operator / (const simd_type& a, const simd_type& b) { return a.m_val / b.m_val; }
    
    simd_type& operator += (const simd_type& b) { return (*this = *this + b); }
    simd_type& operator -= (const simd_type& b) { return (*this = *this - b); }
    simd_type& operator *= (const simd_type& b) { return (*this = *this * b); }
    simd_type& operator /= (const simd_type& b) { return (*this = *this / b); }
    
    friend simd_type sqrt(const simd_type& a) { return std::sqrt(a.m_val); }
    
    friend simd_type round(const simd_type& a) { return std::round(a.m_val); }
    friend simd_type trunc(const simd_type& a) { return std::trunc(a.m_val); }
    
    friend simd_type min(const simd_type& a, const simd_type& b) { return std::min(a.m_val, b.m_val); }
    friend simd_type max(const simd_type& a, const simd_type& b) { return std::max(a.m_val, b.m_val); }
    friend simd_type sel(const simd_type& a, const simd_type& b, 
                                             const simd_type& c) { return c.m_val ? b.m_val : a.m_val; }
    
    friend simd_type operator == (const simd_type& a, const simd_type& b) { return a.m_val == b.m_val; }
    friend simd_type operator != (const simd_type& a, const simd_type& b) { return a.m_val != b.m_val; }
    friend simd_type operator > (const simd_type& a, const simd_type& b) { return a.m_val > b.m_val; }
    friend simd_type operator < (const simd_type& a, const simd_type& b) { return a.m_val < b.m_val; }
    friend simd_type operator >= (const simd_type& a, const simd_type& b) { return a.m_val >= b.m_val; }
    friend simd_type operator <= (const simd_type& a, const simd_type& b) { return a.m_val <= b.m_val; }
    
    friend void unzip(simd_type& out1, simd_type& out2, const simd_type& in1, const simd_type& in2)
    {
        out1 = in1;
        out2 = in2;
    }
    
    friend void zip(simd_type& out1, simd_type& out2, const simd_type& in1, const simd_type& in2)
    {
        out1 = in1;
        out2 = in2;
    }
    
    operator simd_type<double, 1>() { return static_cast<double>(m_val); }
    
    float m_val;
};

template<>
struct simd_type<float, 2>
{
    static constexpr int size = 1;
    using scalar_type = float;
    
    simd_type() {}
    
    simd_type(float a)
    {
        m_vals[0] = a;
        m_vals[1] = a;
    }
    
    simd_type(float a, float b)
    {
        m_vals[0] = a;
        m_vals[1] = b;
    }
    
    simd_type(const float* a)
    {
        m_vals[0] = a[0];
        m_vals[1] = a[1];
    }
    
    void store(float* a) const
    {
        a[0] = m_vals[0];
        a[1] = m_vals[1];
    }
    
    // N.B. - no ops
    
    float m_vals[2];
};

// ************** Platform-Specific Data Type Definitions ************** //

// ************************ 128-bit SIMD Types ************************* //

#if (SIMD_COMPILER_SUPPORT_LEVEL >= SIMD_COMPILER_SUPPORT_VEC128)

#ifdef SIMD_COMPILER_SUPPORT_NEON /* Neon Intrinsics */

#if defined(__arm64) || defined(__aarch64__)
template<>
struct simd_type<double, 2> : public simd_vector<double, float64x2_t, 2>
{
private:
    
    template <uint64x2_t Op(float64x2_t, float64x2_t)>
    static simd_type compare(const simd_type& a, const simd_type& b)
    {
        return vreinterpretq_f64_u64(Op(a.m_val, b.m_val));
    }
    
    template <uint64x2_t Op(uint64x2_t, uint64x2_t)>
    static simd_type bitwise(const simd_type& a, const simd_type& b)
    {
        return vreinterpretq_f64_u64(Op(vreinterpretq_u64_f64(a.m_val), vreinterpretq_u64_f64(b.m_val)));
    }
    
    static float64x2_t neq(const simd_type& a, const simd_type& b)
    {
        return vreinterpretq_f64_u32(vmvnq_u32(vreinterpretq_u32_u64(vceqq_f64(a.m_val, b.m_val))));
    }
    
public:
    
    simd_type() {}
    simd_type(const double& a) { m_val = vdupq_n_f64(a); }
    simd_type(const double* a) { m_val = vld1q_f64(a); }
    simd_type(float64x2_t a) : simd_vector(a) {}
    
    simd_type(const simd_type<float, 2> &a)
    {
        double vals[2];
        
        vals[0] = a.m_vals[0];
        vals[1] = a.m_vals[1];
        
        m_val = vld1q_f64(vals);
    }
    
    void store(double* a) const { vst1q_f64(a, m_val); }
    
    friend simd_type operator + (const simd_type& a, const simd_type& b) { return vaddq_f64(a.m_val, b.m_val); }
    friend simd_type operator - (const simd_type& a, const simd_type& b) { return vsubq_f64(a.m_val, b.m_val); }
    friend simd_type operator * (const simd_type& a, const simd_type& b) { return vmulq_f64(a.m_val, b.m_val); }
    friend simd_type operator / (const simd_type& a, const simd_type& b) { return vdivq_f64(a.m_val, b.m_val); }
    
    simd_type& operator += (const simd_type& b) { return (*this = *this + b); }
    simd_type& operator -= (const simd_type& b) { return (*this = *this - b); }
    simd_type& operator *= (const simd_type& b) { return (*this = *this * b); }
    simd_type& operator /= (const simd_type& b) { return (*this = *this / b); }
    
    friend simd_type sqrt(const simd_type& a) { return vsqrtq_f64(a.m_val); }
    
    // N.B. - ties issue (this matches intel, but not the scalar)
    //friend simd_type round(const simd_type& a) { return vrndnq_f64(a.m_val, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC); }
    friend simd_type trunc(const simd_type& a) { return vrndq_f64(a.m_val); }
    
    friend simd_type min(const simd_type& a, const simd_type& b) { return vminq_f64(a.m_val, b.m_val); }
    friend simd_type max(const simd_type& a, const simd_type& b) { return vmaxq_f64(a.m_val, b.m_val); }
    friend simd_type sel(const simd_type& a, const simd_type& b, const simd_type& c) { return and_not(c, a) | (b & c); }
    
    // N.B. - operand swap for and_not
    friend simd_type and_not(const simd_type& a, const simd_type& b) { return bitwise<vbicq_u64>(b, a); }
    friend simd_type operator & (const simd_type& a, const simd_type& b) { return bitwise<vandq_u64>(a, b); }
    friend simd_type operator | (const simd_type& a, const simd_type& b) { return bitwise<vorrq_u64>(a, b); }
    friend simd_type operator ^ (const simd_type& a, const simd_type& b) { return bitwise<veorq_u64>(a, b); }
    
    friend simd_type operator == (const simd_type& a, const simd_type& b) { return compare<vceqq_f64>(a, b); }
    friend simd_type operator != (const simd_type& a, const simd_type& b) { return neq(a, b); }
    friend simd_type operator > (const simd_type& a, const simd_type& b) { return compare<vcgtq_f64>(a, b); }
    friend simd_type operator < (const simd_type& a, const simd_type& b) { return compare<vcltq_f64>(a, b); }
    friend simd_type operator >= (const simd_type& a, const simd_type& b) { return compare<vcgeq_f64>(a, b); }
    friend simd_type operator <= (const simd_type& a, const simd_type& b) { return compare<vcleq_f64>(a, b); }
    
    friend void unzip(simd_type& out1, simd_type& out2, const simd_type& in1, const simd_type& in2)
    {
        out1 = vuzp1q_f64(in1.m_val, in2.m_val);
        out2 = vuzp2q_f64(in1.m_val, in2.m_val);
    }
    
    friend void zip(simd_type& out1, simd_type& out2, const simd_type& in1, const simd_type& in2)
    {
        out1 = vzip1q_f64(in1.m_val, in2.m_val);
        out2 = vzip2q_f64(in1.m_val, in2.m_val);
    }
    
    operator simd_type<float, 2>()
    {
        double vals[2];
        
        store(vals);
        
        return simd_type<float, 2>(static_cast<float>(vals[0]), static_cast<float>(vals[1]));
    }
};
#endif /* defined (__arm64) || defined(__aarch64__) */

template<>
struct simd_type<float, 4> : public simd_vector<float, float32x4_t, 4>
{
private:
    
    template <uint32x4_t Op(float32x4_t, float32x4_t)>
    static simd_type compare(const simd_type& a, const simd_type& b)
    {
        return vreinterpretq_f32_u32(Op(a.m_val, b.m_val));
    }
    
    template <uint32x4_t Op(uint32x4_t, uint32x4_t)>
    static simd_type bitwise(const simd_type& a, const simd_type& b)
    {
        return vreinterpretq_f32_u32(Op(vreinterpretq_u32_f32(a.m_val), vreinterpretq_u32_f32(b.m_val)));
    }
    
    static float32x4_t neq(const simd_type& a, const simd_type& b)
    {
        return vreinterpretq_f32_u32(vmvnq_u32(vceqq_f32(a.m_val, b.m_val)));
    }
    
#if !defined(__arm64) && !defined(__aarch64__)
    
    // Helpers for single value iteration
    
    template <typename U, U Op(float), typename V>
    static void iterate(V out[4], float temp[4], const float32x4_t& a)
    {
        vst1q_f32(temp, a);
        
        out[0] = Op(temp[0]);
        out[1] = Op(temp[1]);
        out[2] = Op(temp[2]);
        out[3] = Op(temp[3]);
    }
    
    template <float Op(float)>
    static float32x4_t unary(const float32x4_t& a)
    {
        float vals[4];

        iterate<float, Op>(vals, vals, a);
        
        return vld1q_f32(vals);
    }
    
    static double cast_f64_f2(float a)                      { return static_cast<double>(a); }
    
    // Emulate these for 32 bit
    
    static float32x4_t vsqrtq_f32(const float32x4_t& a)     { return unary<std::sqrt>(a); }
    static float32x4_t vrndq_f32(const float32x4_t& a)      { return unary<std::trunc>(a); }
    
    static float32x4_t vdivq_f32(const float32x4_t& a, const float32x4_t& b)
    {
        float vals_a[4], vals_b[4];
        
        vst1q_f32(vals_a, a);
        vst1q_f32(vals_b, b);

        vals_a[0] /= vals_b[0];
        vals_a[1] /= vals_b[1];
        vals_a[2] /= vals_b[2];
        vals_a[3] /= vals_b[3];
        
        return vld1q_f32(vals_a);
    }
    
#endif
    
public:
    
    simd_type() {}
    simd_type(const float& a) { m_val = vdupq_n_f32(a); }
    simd_type(const float* a) { m_val = vld1q_f32(a); }
    simd_type(float32x4_t a) : simd_vector(a) {}
    
    void store(float* a) const { vst1q_f32(a, m_val); }
    
    friend simd_type operator + (const simd_type& a, const simd_type& b) { return vaddq_f32(a.m_val, b.m_val); }
    friend simd_type operator - (const simd_type& a, const simd_type& b) { return vsubq_f32(a.m_val, b.m_val); }
    friend simd_type operator * (const simd_type& a, const simd_type& b) { return vmulq_f32(a.m_val, b.m_val); }
    friend simd_type operator / (const simd_type& a, const simd_type& b) { return vdivq_f32(a.m_val, b.m_val); }
    
    simd_type& operator += (const simd_type& b) { return (*this = *this + b); }
    simd_type& operator -= (const simd_type& b) { return (*this = *this - b); }
    simd_type& operator *= (const simd_type& b) { return (*this = *this * b); }
    simd_type& operator /= (const simd_type& b) { return (*this = *this / b); }
    
    friend simd_type sqrt(const simd_type& a) { return vsqrtq_f32(a.m_val); }
    
    // N.B. - ties issue (this matches intel, but not the scalar)
    //friend simd_type round(const simd_type& a) { return vrndnq_f32(a.m_val); }
    friend simd_type trunc(const simd_type& a) { return vrndq_f32(a.m_val); }
    
    friend simd_type min(const simd_type& a, const simd_type& b) { return vminq_f32(a.m_val, b.m_val); }
    friend simd_type max(const simd_type& a, const simd_type& b) { return vmaxq_f32(a.m_val, b.m_val); }
    friend simd_type sel(const simd_type& a, const simd_type& b, 
                                             const simd_type& c) { return and_not(c, a) | (b & c); }
    
    // N.B. - operand swap for and_not
    friend simd_type and_not(const simd_type& a, const simd_type& b) { return bitwise<vbicq_u32>(b, a); }
    friend simd_type operator & (const simd_type& a, const simd_type& b) { return bitwise<vandq_u32>(a, b); }
    friend simd_type operator | (const simd_type& a, const simd_type& b) { return bitwise<vorrq_u32>(a, b); }
    friend simd_type operator ^ (const simd_type& a, const simd_type& b) { return bitwise<veorq_u32>(a, b); }
    
    friend simd_type operator == (const simd_type& a, const simd_type& b) { return compare<vceqq_f32>(a, b); }
    friend simd_type operator != (const simd_type& a, const simd_type& b) { return neq(a, b); }
    friend simd_type operator > (const simd_type& a, const simd_type& b) { return compare<vcgtq_f32>(a, b); }
    friend simd_type operator < (const simd_type& a, const simd_type& b) { return compare<vcltq_f32>(a, b); }
    friend simd_type operator >= (const simd_type& a, const simd_type& b) { return compare<vcgeq_f32>(a, b); }
    friend simd_type operator <= (const simd_type& a, const simd_type& b) { return compare<vcleq_f32>(a, b); }
    
    friend void unzip(simd_type& out1, simd_type& out2, const simd_type& in1, const simd_type& in2)
    {
        float32x4x2_t v = vuzpq_f32(in1.m_val, in2.m_val);
        
        out1 = v.val[0];
        out2 = v.val[1];
    }
    
    friend void zip(simd_type& out1, simd_type& out2, const simd_type& in1, const simd_type& in2)
    {
        float32x4x2_t v = vzipq_f32(in1.m_val, in2.m_val);
        
        out1 = v.val[0];
        out2 = v.val[1];
    }
    
#if defined(__arm64) || defined(__aarch64__)
    operator sized_vector<double, 2, 4>() const
    {
        sized_vector<double, 2, 4> vec;
        
        vec.m_data[0] = vcvt_f64_f32(vget_low_f32(m_val));
        vec.m_data[1] = vcvt_f64_f32(vget_high_f32(m_val));
        
        return vec;
    }
#else
    operator sized_vector<double, 1, 4>() const
    {
        float vals[4];
        sized_vector<double, 1, 4> vec;
        
        iterate<double, cast_f64_f2>(vec.m_data, vals, m_val);
        
        return vec;
    }
#endif
};

template<>
struct simd_type<int32_t, 4> : public simd_vector<int32_t, int32x4_t, 4>
{
    simd_type() {}
    simd_type(const int32_t& a) { m_val = vdupq_n_s32(a); }
    simd_type(const int32_t* a) { m_val = vld1q_s32(a); }
    simd_type(int32x4_t a) : simd_vector(a) {}
    
    void store(int32_t* a) const { vst1q_s32(a, m_val); }
    
    friend simd_type operator + (const simd_type& a, const simd_type& b) { return vaddq_s32(a.m_val, b.m_val); }
    friend simd_type operator - (const simd_type& a, const simd_type& b) { return vsubq_s32(a.m_val, b.m_val); }
    friend simd_type operator * (const simd_type& a, const simd_type& b) { return vmulq_s32(a.m_val, b.m_val); }
    
    simd_type& operator += (const simd_type& b) { return (*this = *this + b); }
    simd_type& operator -= (const simd_type& b) { return (*this = *this - b); }
    simd_type& operator *= (const simd_type& b) { return (*this = *this * b); }
    
    friend simd_type min(const simd_type& a, const simd_type& b) { return vminq_s32(a.m_val, b.m_val); }
    friend simd_type max(const simd_type& a, const simd_type& b) { return vmaxq_s32(a.m_val, b.m_val); }
    
    operator simd_type<float, 4>() { return simd_type<float, 4>( vcvtq_f32_s32(m_val)); }
    /*
    operator sized_vector<double, 2, 4>() const
    {
        sized_vector<double, 2, 4> vec;
        
        vec.m_data[0] = _mm_cvtepi32_pd(m_val);
        vec.m_data[1] = _mm_cvtepi32_pd(_mm_shuffle_epi32(m_val, 0xE));
        
        return vec;
    }*/
};

#else /* Intel Instrinsics */

template<>
struct simd_type<double, 2> : public simd_vector<double, __m128d, 2>
{
    simd_type() {}
    simd_type(const double& a) { m_val = _mm_set1_pd(a); }
    simd_type(const double* a) { m_val = _mm_loadu_pd(a); }
    simd_type(__m128d a) : simd_vector(a) {}
    
    simd_type(const simd_type<float, 2> &a)
    {
        double vals[2];
        
        vals[0] = a.m_vals[0];
        vals[1] = a.m_vals[1];
        
        m_val = _mm_loadu_pd(vals);
    }
    
    void store(double* a) const { _mm_storeu_pd(a, m_val); }
    
    friend simd_type operator + (const simd_type& a, const simd_type& b) { return _mm_add_pd(a.m_val, b.m_val); }
    friend simd_type operator - (const simd_type& a, const simd_type& b) { return _mm_sub_pd(a.m_val, b.m_val); }
    friend simd_type operator * (const simd_type& a, const simd_type& b) { return _mm_mul_pd(a.m_val, b.m_val); }
    friend simd_type operator / (const simd_type& a, const simd_type& b) { return _mm_div_pd(a.m_val, b.m_val); }
    
    simd_type& operator += (const simd_type& b) { return (*this = *this + b); }
    simd_type& operator -= (const simd_type& b) { return (*this = *this - b); }
    simd_type& operator *= (const simd_type& b) { return (*this = *this * b); }
    simd_type& operator /= (const simd_type& b) { return (*this = *this / b); }
    
    friend simd_type sqrt(const simd_type& a) { return _mm_sqrt_pd(a.m_val); }
    
    // N.B. - ties issue
    friend simd_type trunc(const simd_type& a) { return _mm_round_pd(a.m_val, _MM_FROUND_TO_ZERO |_MM_FROUND_NO_EXC); }
    friend simd_type round(const simd_type& a) { return _mm_round_pd(a.m_val, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC); }
    
    friend simd_type min(const simd_type& a, const simd_type& b) { return _mm_min_pd(a.m_val, b.m_val); }
    friend simd_type max(const simd_type& a, const simd_type& b) { return _mm_max_pd(a.m_val, b.m_val); }
    friend simd_type sel(const simd_type& a, const simd_type& b, const simd_type& c) { return and_not(c, a) | (b & c); }
    
    friend simd_type and_not(const simd_type& a, const simd_type& b) { return _mm_andnot_pd(a.m_val, b.m_val); }
    friend simd_type operator & (const simd_type& a, const simd_type& b) { return _mm_and_pd(a.m_val, b.m_val); }
    friend simd_type operator | (const simd_type& a, const simd_type& b) { return _mm_or_pd(a.m_val, b.m_val); }
    friend simd_type operator ^ (const simd_type& a, const simd_type& b) { return _mm_xor_pd(a.m_val, b.m_val); }
    
    friend simd_type operator == (const simd_type& a, const simd_type& b) { return _mm_cmpeq_pd(a.m_val, b.m_val); }
    friend simd_type operator != (const simd_type& a, const simd_type& b) { return _mm_cmpneq_pd(a.m_val, b.m_val); }
    friend simd_type operator > (const simd_type& a, const simd_type& b) { return _mm_cmplt_pd(a.m_val, b.m_val); }
    friend simd_type operator < (const simd_type& a, const simd_type& b) { return _mm_cmpgt_pd(a.m_val, b.m_val); }
    friend simd_type operator >= (const simd_type& a, const simd_type& b) { return _mm_cmple_pd(a.m_val, b.m_val); }
    friend simd_type operator <= (const simd_type& a, const simd_type& b) { return _mm_cmpge_pd(a.m_val, b.m_val); }
    
    friend void unzip(simd_type& out1, simd_type& out2, const simd_type& in1, const simd_type& in2)
    {
        out1 = _mm_unpacklo_pd(in1.m_val, in2.m_val);
        out2 = _mm_unpackhi_pd(in1.m_val, in2.m_val);
    }
    
    friend void zip(simd_type& out1, simd_type& out2, const simd_type& in1, const simd_type& in2)
    {
        out1 = _mm_unpacklo_pd(in1.m_val, in2.m_val);
        out2 = _mm_unpackhi_pd(in1.m_val, in2.m_val);
    }
    
    operator simd_type<float, 2>()
    {
        double vals[2];
        
        store(vals);
        
        return simd_type<float, 2>(static_cast<float>(vals[0]), static_cast<float>(vals[1]));
    }
};

template<>
struct simd_type<float, 4> : public simd_vector<float, __m128, 4>
{
    simd_type() {}
    simd_type(const float& a) { m_val = _mm_set1_ps(a); }
    simd_type(const float* a) { m_val = _mm_loadu_ps(a); }
    simd_type(__m128 a) : simd_vector(a) {}
    
    void store(float* a) const { _mm_storeu_ps(a, m_val); }
    
    friend simd_type operator + (const simd_type& a, const simd_type& b) { return _mm_add_ps(a.m_val, b.m_val); }
    friend simd_type operator - (const simd_type& a, const simd_type& b) { return _mm_sub_ps(a.m_val, b.m_val); }
    friend simd_type operator * (const simd_type& a, const simd_type& b) { return _mm_mul_ps(a.m_val, b.m_val); }
    friend simd_type operator / (const simd_type& a, const simd_type& b) { return _mm_div_ps(a.m_val, b.m_val); }
    
    simd_type& operator += (const simd_type& b) { return (*this = *this + b); }
    simd_type& operator -= (const simd_type& b) { return (*this = *this - b); }
    simd_type& operator *= (const simd_type& b) { return (*this = *this * b); }
    simd_type& operator /= (const simd_type& b) { return (*this = *this / b); }
    
    friend simd_type sqrt(const simd_type& a) { return _mm_sqrt_ps(a.m_val); }
    
    // N.B. - ties issue
    friend simd_type round(const simd_type& a) { return _mm_round_ps(a.m_val, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC); }
    friend simd_type trunc(const simd_type& a) { return _mm_round_ps(a.m_val, _MM_FROUND_TO_ZERO |_MM_FROUND_NO_EXC); }
    
    friend simd_type min(const simd_type& a, const simd_type& b) { return _mm_min_ps(a.m_val, b.m_val); }
    friend simd_type max(const simd_type& a, const simd_type& b) { return _mm_max_ps(a.m_val, b.m_val); }
    friend simd_type sel(const simd_type& a, const simd_type& b, const simd_type& c) { return and_not(c, a) | (b & c); }
    
    friend simd_type and_not(const simd_type& a, const simd_type& b) { return _mm_andnot_ps(a.m_val, b.m_val); }
    friend simd_type operator & (const simd_type& a, const simd_type& b) { return _mm_and_ps(a.m_val, b.m_val); }
    friend simd_type operator | (const simd_type& a, const simd_type& b) { return _mm_or_ps(a.m_val, b.m_val); }
    friend simd_type operator ^ (const simd_type& a, const simd_type& b) { return _mm_xor_ps(a.m_val, b.m_val); }
    
    friend simd_type operator == (const simd_type& a, const simd_type& b) { return _mm_cmpeq_ps(a.m_val, b.m_val); }
    friend simd_type operator != (const simd_type& a, const simd_type& b) { return _mm_cmpneq_ps(a.m_val, b.m_val); }
    friend simd_type operator > (const simd_type& a, const simd_type& b) { return _mm_cmplt_ps(a.m_val, b.m_val); }
    friend simd_type operator < (const simd_type& a, const simd_type& b) { return _mm_cmpgt_ps(a.m_val, b.m_val); }
    friend simd_type operator >= (const simd_type& a, const simd_type& b) { return _mm_cmple_ps(a.m_val, b.m_val); }
    friend simd_type operator <= (const simd_type& a, const simd_type& b) { return _mm_cmpge_ps(a.m_val, b.m_val); }
    
    friend void unzip(simd_type& out1, simd_type& out2, const simd_type& in1, const simd_type& in2)
    {
        out1 = _mm_shuffle_ps(in1.m_val, in2.m_val, 0x88);
        out2 = _mm_shuffle_ps(in1.m_val, in2.m_val, 0xDD);
    }
    
    friend void zip(simd_type& out1, simd_type& out2, const simd_type& in1, const simd_type& in2)
    {
        out1 = _mm_unpacklo_ps(in1.m_val, in2.m_val);
        out2 = _mm_unpackhi_ps(in1.m_val, in2.m_val);
    }
    
    operator sized_vector<double, 2, 4>() const
    {
        sized_vector<double, 2, 4> vec;
        
        vec.m_data[0] = _mm_cvtps_pd(m_val);
        vec.m_data[1] = _mm_cvtps_pd(_mm_movehl_ps(m_val, m_val));
        
        return vec;
    }
};

template<>
struct simd_type<int32_t, 4> : public simd_vector<int32_t, __m128i, 4>
{
    simd_type() {}
    simd_type(const int32_t& a) { m_val = _mm_set1_epi32(a); }
    simd_type(const int32_t* a) { m_val = _mm_loadu_si128(reinterpret_cast<const __m128i*>(a)); }
    simd_type(__m128i a) : simd_vector(a) {}
    
    void store(int32_t* a) const { _mm_storeu_si128(reinterpret_cast<__m128i*>(a), m_val); }
    
    friend simd_type operator + (const simd_type& a, const simd_type& b) { return _mm_add_epi32(a.m_val, b.m_val); }
    friend simd_type operator - (const simd_type& a, const simd_type& b) { return _mm_sub_epi32(a.m_val, b.m_val); }
    friend simd_type operator * (const simd_type& a, const simd_type& b) { return _mm_mul_epi32(a.m_val, b.m_val); }
    
    simd_type& operator += (const simd_type& b) { return (*this = *this + b); }
    simd_type& operator -= (const simd_type& b) { return (*this = *this - b); }
    simd_type& operator *= (const simd_type& b) { return (*this = *this * b); }
    
    friend simd_type min(const simd_type& a, const simd_type& b) { return _mm_min_epi32(a.m_val, b.m_val); }
    friend simd_type max(const simd_type& a, const simd_type& b) { return _mm_max_epi32(a.m_val, b.m_val); }
    
    operator simd_type<float, 4>() { return simd_type<float, 4>( _mm_cvtepi32_ps(m_val)); }
    
    operator sized_vector<double, 2, 4>() const
    {
        sized_vector<double, 2, 4> vec;
        
        vec.m_data[0] = _mm_cvtepi32_pd(m_val);
        vec.m_data[1] = _mm_cvtepi32_pd(_mm_shuffle_epi32(m_val, 0xE));
        
        return vec;
    }
};

#endif /* SIMD_COMPILER_SUPPORT_NEON - End Intel Intrinsics */

#endif /* SIMD_COMPILER_SUPPORT_LEVEL >= SIMD_COMPILER_SUPPORT_VEC128 */

// ************************ 256-bit SIMD Types ************************* //

#if (SIMD_COMPILER_SUPPORT_LEVEL >= SIMD_COMPILER_SUPPORT_VEC256)

template<>
struct simd_type<double, 4> : public simd_vector<double, __m256d, 4>
{
    simd_type() {}
    simd_type(const double& a) { m_val = _mm256_set1_pd(a); }
    simd_type(const double* a) { m_val = _mm256_loadu_pd(a); }
    simd_type(__m256d a) : simd_vector(a) {}
    
    simd_type(const simd_type<float, 4> &a) { m_val = _mm256_cvtps_pd(a.m_val); }
    simd_type(const simd_type<int32_t, 4> &a) { m_val = _mm256_cvtepi32_pd(a.m_val); }
    
    void store(double* a) const { _mm256_storeu_pd(a, m_val); }
    
    friend simd_type operator + (const simd_type& a, const simd_type& b) { return _mm256_add_pd(a.m_val, b.m_val); }
    friend simd_type operator - (const simd_type& a, const simd_type& b) { return _mm256_sub_pd(a.m_val, b.m_val); }
    friend simd_type operator * (const simd_type& a, const simd_type& b) { return _mm256_mul_pd(a.m_val, b.m_val); }
    friend simd_type operator / (const simd_type& a, const simd_type& b) { return _mm256_div_pd(a.m_val, b.m_val); }
    
    simd_type& operator += (const simd_type& b) { return (*this = *this + b); }
    simd_type& operator -= (const simd_type& b) { return (*this = *this - b); }
    simd_type& operator *= (const simd_type& b) { return (*this = *this * b); }
    simd_type& operator /= (const simd_type& b) { return (*this = *this / b); }
    
    friend simd_type sqrt(const simd_type& a) { return _mm256_sqrt_pd(a.m_val); }
    
    // N.B. - ties issue
    friend simd_type round(const simd_type& a) { return _mm256_round_pd(a.m_val, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC); }
    friend simd_type trunc(const simd_type& a) { return _mm256_round_pd(a.m_val, _MM_FROUND_TO_ZERO |_MM_FROUND_NO_EXC); }
    
    friend simd_type min(const simd_type& a, const simd_type& b) { return _mm256_min_pd(a.m_val, b.m_val); }
    friend simd_type max(const simd_type& a, const simd_type& b) { return _mm256_max_pd(a.m_val, b.m_val); }
    friend simd_type sel(const simd_type& a, const simd_type& b, const simd_type& c) { return and_not(c, a) | (b & c); }
    
    friend simd_type and_not(const simd_type& a, const simd_type& b) { return _mm256_andnot_pd(a.m_val, b.m_val); }
    friend simd_type operator & (const simd_type& a, const simd_type& b) { return _mm256_and_pd(a.m_val, b.m_val); }
    friend simd_type operator | (const simd_type& a, const simd_type& b) { return _mm256_or_pd(a.m_val, b.m_val); }
    friend simd_type operator ^ (const simd_type& a, const simd_type& b) { return _mm256_xor_pd(a.m_val, b.m_val); }
    
    friend simd_type operator == (const simd_type& a, const simd_type& b) { return _mm256_cmp_pd(a.m_val, b.m_val, _CMP_EQ_OQ); }
    friend simd_type operator != (const simd_type& a, const simd_type& b) { return _mm256_cmp_pd(a.m_val, b.m_val, _CMP_NEQ_UQ); }
    friend simd_type operator > (const simd_type& a, const simd_type& b) { return _mm256_cmp_pd(a.m_val, b.m_val, _CMP_GT_OQ); }
    friend simd_type operator < (const simd_type& a, const simd_type& b) { return _mm256_cmp_pd(a.m_val, b.m_val, _CMP_LT_OQ); }
    friend simd_type operator >= (const simd_type& a, const simd_type& b) { return _mm256_cmp_pd(a.m_val, b.m_val, _CMP_GE_OQ); }
    friend simd_type operator <= (const simd_type& a, const simd_type& b) { return _mm256_cmp_pd(a.m_val, b.m_val, _CMP_LE_OQ); }
    
    friend void unzip(simd_type& out1, simd_type& out2, const simd_type& in1, const simd_type& in2)
    {
        const __m256d v1 = _mm256_permute2f128_pd(in1.m_val, in2.m_val, 0x20);
        const __m256d v2 = _mm256_permute2f128_pd(in1.m_val, in2.m_val, 0x31);
        
        out1 = _mm256_unpacklo_pd(v1, v2);
        out2 = _mm256_unpackhi_pd(v1, v2);
    }
    
    friend void zip(simd_type& out1, simd_type& out2, const simd_type& in1, const simd_type& in2)
    {
        const __m256d v1 = _mm256_unpacklo_pd(in1.m_val, in2.m_val);
        const __m256d v2 = _mm256_unpackhi_pd(in1.m_val, in2.m_val);
        
        out1 = _mm256_permute2f128_pd(v1, v2, 0x20);
        out2 = _mm256_permute2f128_pd(v1, v2, 0x31);
    }
    
    operator simd_type<float, 4>() { return _mm256_cvtpd_ps(m_val); }
    operator simd_type<int32_t, 4>() { return _mm256_cvtpd_epi32(m_val); }
};

template<>
struct simd_type<float, 8> : public simd_vector<float, __m256, 8>
{
    simd_type() {}
    simd_type(const float& a) { m_val = _mm256_set1_ps(a); }
    simd_type(const float* a) { m_val = _mm256_loadu_ps(a); }
    simd_type(__m256 a) : simd_vector(a) {}
    
    void store(float* a) const { _mm256_storeu_ps(a, m_val); }
    
    friend simd_type operator + (const simd_type& a, const simd_type& b) { return _mm256_add_ps(a.m_val, b.m_val); }
    friend simd_type operator - (const simd_type& a, const simd_type& b) { return _mm256_sub_ps(a.m_val, b.m_val); }
    friend simd_type operator * (const simd_type& a, const simd_type& b) { return _mm256_mul_ps(a.m_val, b.m_val); }
    friend simd_type operator / (const simd_type& a, const simd_type& b) { return _mm256_div_ps(a.m_val, b.m_val); }
    
    simd_type& operator += (const simd_type& b) { return (*this = *this + b); }
    simd_type& operator -= (const simd_type& b) { return (*this = *this - b); }
    simd_type& operator *= (const simd_type& b) { return (*this = *this * b); }
    simd_type& operator /= (const simd_type& b) { return (*this = *this / b); }
    
    friend simd_type sqrt(const simd_type& a) { return _mm256_sqrt_ps(a.m_val); }
    
    // N.B. - ties issue
    friend simd_type round(const simd_type& a) { return _mm256_round_ps(a.m_val, _MM_FROUND_TO_NEAREST_INT |_MM_FROUND_NO_EXC); }
    friend simd_type trunc(const simd_type& a) { return _mm256_round_ps(a.m_val, _MM_FROUND_TO_ZERO |_MM_FROUND_NO_EXC); }
    
    friend simd_type min(const simd_type& a, const simd_type& b) { return _mm256_min_ps(a.m_val, b.m_val); }
    friend simd_type max(const simd_type& a, const simd_type& b) { return _mm256_max_ps(a.m_val, b.m_val); }
    friend simd_type sel(const simd_type& a, const simd_type& b, const simd_type& c) { return and_not(c, a) | (b & c); }
    
    friend simd_type and_not(const simd_type& a, const simd_type& b) { return _mm256_andnot_ps(a.m_val, b.m_val); }
    friend simd_type operator & (const simd_type& a, const simd_type& b) { return _mm256_and_ps(a.m_val, b.m_val); }
    friend simd_type operator | (const simd_type& a, const simd_type& b) { return _mm256_or_ps(a.m_val, b.m_val); }
    friend simd_type operator ^ (const simd_type& a, const simd_type& b) { return _mm256_xor_ps(a.m_val, b.m_val); }
    
    friend simd_type operator == (const simd_type& a, const simd_type& b) { return _mm256_cmp_ps(a.m_val, b.m_val, _CMP_EQ_OQ); }
    friend simd_type operator != (const simd_type& a, const simd_type& b) { return _mm256_cmp_ps(a.m_val, b.m_val, _CMP_NEQ_UQ); }
    friend simd_type operator > (const simd_type& a, const simd_type& b) { return _mm256_cmp_ps(a.m_val, b.m_val, _CMP_GT_OQ); }
    friend simd_type operator < (const simd_type& a, const simd_type& b) { return _mm256_cmp_ps(a.m_val, b.m_val, _CMP_LT_OQ); }
    friend simd_type operator >= (const simd_type& a, const simd_type& b) { return _mm256_cmp_ps(a.m_val, b.m_val, _CMP_GE_OQ); }
    friend simd_type operator <= (const simd_type& a, const simd_type& b) { return _mm256_cmp_ps(a.m_val, b.m_val, _CMP_LE_OQ); }
    
    friend void unzip(simd_type& out1, simd_type& out2, const simd_type& in1, const simd_type& in2)
    {
        const __m256 v1 = _mm256_permute2f128_ps(in1.m_val, in2.m_val, 0x20);
        const __m256 v2 = _mm256_permute2f128_ps(in1.m_val, in2.m_val, 0x31);
        
        out1 = _mm256_shuffle_ps(v1, v2, 0x88);
        out2 = _mm256_shuffle_ps(v1, v2, 0xDD);
    }
    
    friend void zip(simd_type& out1, simd_type& out2, const simd_type& in1, const simd_type& in2)
    {
        const __m256 v1 = _mm256_unpacklo_ps(in1.m_val, in2.m_val);
        const __m256 v2 = _mm256_unpackhi_ps(in1.m_val, in2.m_val);
        
        out1 = _mm256_permute2f128_ps(v1, v2, 0x20);
        out2 = _mm256_permute2f128_ps(v1, v2, 0x31);
    }
    
    operator sized_vector<double, 4, 8>() const
    {
        sized_vector<double, 4, 8> vec;
        
        vec.m_data[0] = _mm256_cvtps_pd(_mm256_extractf128_ps(m_val, 0));
        vec.m_data[1] = _mm256_cvtps_pd(_mm256_extractf128_ps(m_val, 1));
        
        return vec;
    }
};

#endif

// ************************ 512-bit SIMD Types ************************* //

#if (SIMD_COMPILER_SUPPORT_LEVEL >= SIMD_COMPILER_SUPPORT_VEC512)

template<>
struct simd_type<double, 8> : public simd_vector<double, __m512d, 8>
{
    simd_type() {}
    simd_type(const double& a) { m_val = _mm512_set1_pd(a); }
    simd_type(const double* a) { m_val = _mm512_loadu_pd(a); }
    simd_type(__m512d a) : simd_vector(a) {}
    
    simd_type(const simd_type<float, 8> &a) { m_val = _mm512_cvtps_pd(a.m_val); }
    
    void store(double* a) const { _mm512_storeu_pd(a, m_val); }
    
    friend simd_type operator + (const simd_type& a, const simd_type& b) { return _mm512_add_pd(a.m_val, b.m_val); }
    friend simd_type operator - (const simd_type& a, const simd_type& b) { return _mm512_sub_pd(a.m_val, b.m_val); }
    friend simd_type operator * (const simd_type& a, const simd_type& b) { return _mm512_mul_pd(a.m_val, b.m_val); }
    friend simd_type operator / (const simd_type& a, const simd_type& b) { return _mm512_div_pd(a.m_val, b.m_val); }
    
    simd_type& operator += (const simd_type& b) { return (*this = *this + b); }
    simd_type& operator -= (const simd_type& b) { return (*this = *this - b); }
    simd_type& operator *= (const simd_type& b) { return (*this = *this * b); }
    simd_type& operator /= (const simd_type& b) { return (*this = *this / b); }
    
    friend simd_type sqrt(const simd_type& a) { return _mm512_sqrt_pd(a.m_val); }
    
    friend simd_type min(const simd_type& a, const simd_type& b) { return _mm512_min_pd(a.m_val, b.m_val); }
    friend simd_type max(const simd_type& a, const simd_type& b) { return _mm512_max_pd(a.m_val, b.m_val); }
    friend simd_type sel(const simd_type& a, const simd_type& b, const simd_type& c) { return and_not(c, a) | (b & c); }
    
    friend simd_type and_not(const simd_type& a, const simd_type& b) { return _mm512_andnot_pd(a.m_val, b.m_val); }
    friend simd_type operator & (const simd_type& a, const simd_type& b) { return _mm512_and_pd(a.m_val, b.m_val); }
    friend simd_type operator | (const simd_type& a, const simd_type& b) { return _mm512_or_pd(a.m_val, b.m_val); }
    friend simd_type operator ^ (const simd_type& a, const simd_type& b) { return _mm512_xor_pd(a.m_val, b.m_val); }
    
    friend simd_type operator == (const simd_type& a, const simd_type& b) { return _mm512_cmp_pd_mask(a.m_val, b.m_val, _CMP_EQ_OQ); }
    friend simd_type operator != (const simd_type& a, const simd_type& b) { return _mm512_cmp_pd_mask(a.m_val, b.m_val, _CMP_NEQ_UQ); }
    friend simd_type operator > (const simd_type& a, const simd_type& b) { return _mm512_cmp_pd_mask(a.m_val, b.m_val, _CMP_GT_OQ); }
    friend simd_type operator < (const simd_type& a, const simd_type& b) { return _mm512_cmp_pd_mask(a.m_val, b.m_val, _CMP_LT_OQ); }
    friend simd_type operator >= (const simd_type& a, const simd_type& b) { return _mm512_cmp_pd_mask(a.m_val, b.m_val, _CMP_GE_OQ); }
    friend simd_type operator <= (const simd_type& a, const simd_type& b) { return _mm512_cmp_pd_mask(a.m_val, b.m_val, _CMP_LE_OQ); }
    
    operator simd_type<float, 8>() { return _mm512_cvtpd_ps(m_val); }
};

template<>
struct simd_type<float, 16> : public simd_vector<float, __m512, 16>
{
    simd_type() {}
    simd_type(const float& a) { m_val = _mm512_set1_ps(a); }
    simd_type(const float* a) { m_val = _mm512_loadu_ps(a); }
    simd_type(__m512 a) : simd_vector(a) {}
    
    void store(float* a) const { _mm512_storeu_ps(a, m_val); }
    
    friend simd_type operator + (const simd_type& a, const simd_type& b) { return _mm512_add_ps(a.m_val, b.m_val); }
    friend simd_type operator - (const simd_type& a, const simd_type& b) { return _mm512_sub_ps(a.m_val, b.m_val); }
    friend simd_type operator * (const simd_type& a, const simd_type& b) { return _mm512_mul_ps(a.m_val, b.m_val); }
    friend simd_type operator / (const simd_type& a, const simd_type& b) { return _mm512_div_ps(a.m_val, b.m_val); }
    
    simd_type& operator += (const simd_type& b) { return (*this = *this + b); }
    simd_type& operator -= (const simd_type& b) { return (*this = *this - b); }
    simd_type& operator *= (const simd_type& b) { return (*this = *this * b); }
    simd_type& operator /= (const simd_type& b) { return (*this = *this / b); }
    
    friend simd_type sqrt(const simd_type& a) { return _mm512_sqrt_ps(a.m_val); }
    
    friend simd_type min(const simd_type& a, const simd_type& b) { return _mm512_min_ps(a.m_val, b.m_val); }
    friend simd_type max(const simd_type& a, const simd_type& b) { return _mm512_max_ps(a.m_val, b.m_val); }
    friend simd_type sel(const simd_type& a, const simd_type& b, const simd_type& c) { return and_not(c, a) | (b & c); }
    
    friend simd_type and_not(const simd_type& a, const simd_type& b) { return _mm512_andnot_ps(a.m_val, b.m_val); }
    friend simd_type operator & (const simd_type& a, const simd_type& b) { return _mm512_and_ps(a.m_val, b.m_val); }
    friend simd_type operator | (const simd_type& a, const simd_type& b) { return _mm512_or_ps(a.m_val, b.m_val); }
    friend simd_type operator ^ (const simd_type& a, const simd_type& b) { return _mm512_xor_ps(a.m_val, b.m_val); }
    
    friend simd_type operator == (const simd_type& a, const simd_type& b) { return _mm512_cmp_ps_mask(a.m_val, b.m_val, _CMP_EQ_OQ); }
    friend simd_type operator != (const simd_type& a, const simd_type& b) { return _mm512_cmp_ps_mask(a.m_val, b.m_val, _CMP_NEQ_UQ); }
    friend simd_type operator > (const simd_type& a, const simd_type& b) { return _mm512_cmp_ps_mask(a.m_val, b.m_val, _CMP_GT_OQ); }
    friend simd_type operator < (const simd_type& a, const simd_type& b) { return _mm512_cmp_ps_mask(a.m_val, b.m_val, _CMP_LT_OQ); }
    friend simd_type operator >= (const simd_type& a, const simd_type& b) { return _mm512_cmp_ps_mask(a.m_val, b.m_val, _CMP_GE_OQ); }
    friend simd_type operator <= (const simd_type& a, const simd_type& b) { return _mm512_cmp_ps_mask(a.m_val, b.m_val, _CMP_LE_OQ); }
};

#endif

// ********************** Common Functionality ********************** //

// Vector Summing for all types

namespace impl
{
    template <class ST, int N, int M>
    struct partial_sum
    {
        using T = typename ST::scalar_type;
        
        T operator()(const T values[ST::size])
        {
            constexpr int O = N / 2;
            
            return partial_sum<ST, O, M>()(values) + partial_sum<ST, O, M + O>()(values);
        }
    };

    template <class ST, int M>
    struct partial_sum<ST, 1, M>
    {
        using T = typename ST::scalar_type;

        T operator()(const T values[ST::size])
        {
            return values[M];
        }
    };
}

template <class T, int  N>
T sum(const simd_type<T,  N>& vec)
{
    T values[N];
    
    vec.store(values);
    
    return impl::partial_sum<simd_type<T,  N>, N, 0>()(values);
}

// Select Functionality for all types

template <class T, int N>
T select(const simd_type<T, N>& a, const simd_type<T, N>& b, const simd_type<T, N>& mask)
{
    return (b & mask) | and_not(mask, a);
}

// Abs functionality

static inline simd_type<double, 1> abs(const simd_type<double, 1> a)
{
    constexpr uint64_t bit_mask_64 = 0x7FFFFFFFFFFFFFFFU;
    
    uint64_t temp = *(reinterpret_cast<const uint64_t*>(&a)) & bit_mask_64;
    return *(reinterpret_cast<double*>(&temp));
}

static inline simd_type<float, 1> abs(const simd_type<float, 1> a)
{
    constexpr uint32_t bit_mask_32 = 0x7FFFFFFFU;
    
    uint32_t temp = *(reinterpret_cast<const uint32_t*>(&a)) & bit_mask_32;
    return *(reinterpret_cast<float*>(&temp));
}

template <int N>
simd_type<double, N> abs(const simd_type<double, N> a)
{
    constexpr uint64_t bit_mask_64 = 0x7FFFFFFFFFFFFFFFU;
    const double bit_mask_64d = *(reinterpret_cast<const double*>(&bit_mask_64));
    
    return a & simd_type<double, N>(bit_mask_64d);
}

template <int N>
simd_type<float, N> abs(const simd_type<float, N> a)
{
    constexpr uint32_t bit_mask_32 = 0x7FFFFFFFU;
    const float bit_mask_32f = *(reinterpret_cast<const float*>(&bit_mask_32));
    
    return a & simd_type<float, N>(bit_mask_32f);
}

HISSTOOLS_LIBRARY_NAMESPACE_END()

#endif /* HISSTOOLS_LIBRARY_SIMD_SUPPORT_HPP */
