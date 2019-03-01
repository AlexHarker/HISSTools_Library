
#include <cmath>
#include <algorithm>
#include <functional>

#ifdef __arm__
#include <arm_neon.h>
#else
#ifndef __APPLE__
#include <intrin.h>
#endif
#include <emmintrin.h>
#include <immintrin.h>
#endif

// Setup Structures

template <class T>
struct Setup
{
    unsigned long max_fft_log2;
    Split<T> tables[28];
};

struct DoubleSetup : public Setup<double> {};
struct FloatSetup : public Setup<float> {};

namespace hisstools_fft_impl{
    
    template<class T> struct SIMDLimits     { static const int max_size = 1;};
    
#if defined(__AVX512F__)
    
    template<> struct SIMDLimits<double>    { static const int max_size = 8; };
    template<> struct SIMDLimits<float>     { static const int max_size = 16; };
    
#elif defined(__AVX__)
    
    template<> struct SIMDLimits<double>    { static const int max_size = 4; };
    template<> struct SIMDLimits<float>     { static const int max_size = 8; };
    
#elif defined(__SSE__) || defined(__arm__)
    
    template<> struct SIMDLimits<double>    { static const int max_size = 2; };
    template<> struct SIMDLimits<float>     { static const int max_size = 4; };
    
#endif
    
    // Aligned Allocation
    
#ifdef __APPLE__
    
    template <class T>
    T *allocate_aligned(size_t size)
    {
        return static_cast<T *>(malloc(size * sizeof(T)));
    }
    
    template <class T>
    void deallocate_aligned(T *ptr)
    {
        free(ptr);
    }
    
#elif defined(__arm__)
    
#include <memory.h>
    
    template <class T>
    T *allocate_aligned(size_t size)
    {
        return static_cast<T *>(aligned_alloc(16, size * sizeof(T)));
    }
    
    template <class T>
    void deallocate_aligned(T *ptr)
    {
        free(ptr);
    }
    
#else
#include <malloc.h>
    
    template <class T>
    T *allocate_aligned(size_t size)
    {
        return static_cast<T *>(_aligned_malloc(size * sizeof(T), 16));
    }
    
    template <class T>
    void deallocate_aligned(T *ptr)
    {
        _aligned_free(ptr);
    }
    
#endif
    
    template <class T>
    bool isAligned(const T *ptr) { return !(reinterpret_cast<uintptr_t>(ptr) % 16); }
    
    // Offset for Table
    
    static const uintptr_t trig_table_offset = 3;
    
    // Data Type Definitions
    
    // ******************** Basic Data Type Defintions ******************** //
    
    template <class T, class U, int vec_size>
    struct SIMDVectorBase
    {
        static const int size = vec_size;
        typedef T scalar_type;
        typedef Split<scalar_type> split_type;
        typedef Setup<scalar_type> setup_type;
        
        SIMDVectorBase() {}
        SIMDVectorBase(U a) : mVal(a) {}
        
        U mVal;
    };
    
    template <class T, int vec_size>
    struct SIMDVector
    {};
    
    template<class T>
    struct SIMDVector<T, 1> : public SIMDVectorBase<T, T, 1>
    {
        static const int size = 1;
        typedef T scalar_type;
        typedef Split<scalar_type> split_type;
        typedef Setup<scalar_type> setup_type;
        
        SIMDVector() {}
        SIMDVector(T a) : mVal(a) {}
        friend SIMDVector operator + (const SIMDVector& a, const SIMDVector& b) { return a.mVal + b.mVal; }
        friend SIMDVector operator - (const SIMDVector& a, const SIMDVector& b) { return a.mVal - b.mVal; }
        friend SIMDVector operator * (const SIMDVector& a, const SIMDVector& b) { return a.mVal * b.mVal; }
        
        T mVal;
    };
    
#if defined(__AVX512F__) || defined(__AVX__) || defined(__SSE__)
    
    template <>
    struct SIMDVector<double, 2> : public SIMDVectorBase<double, __m128d, 2>
    {
        SIMDVector() {}
        SIMDVector(__m128d a) : SIMDVectorBase(a) {}
        friend SIMDVector operator + (const SIMDVector &a, const SIMDVector& b) { return _mm_add_pd(a.mVal, b.mVal); }
        friend SIMDVector operator - (const SIMDVector &a, const SIMDVector& b) { return _mm_sub_pd(a.mVal, b.mVal); }
        friend SIMDVector operator * (const SIMDVector &a, const SIMDVector& b) { return _mm_mul_pd(a.mVal, b.mVal); }
        
        template <int y, int x>
        static SIMDVector shuffle(const SIMDVector& a, const SIMDVector& b)
        {
            return _mm_shuffle_pd(a.mVal, b.mVal, (y<<1)|x);
        }
    };
    
    template <>
    struct SIMDVector<float, 4> : public SIMDVectorBase<float, __m128, 4>
    {
        SIMDVector() {}
        SIMDVector(__m128 a) : SIMDVectorBase(a) {}
        friend SIMDVector operator + (const SIMDVector& a, const SIMDVector& b) { return _mm_add_ps(a.mVal, b.mVal); }
        friend SIMDVector operator - (const SIMDVector& a, const SIMDVector& b) { return _mm_sub_ps(a.mVal, b.mVal); }
        friend SIMDVector operator * (const SIMDVector& a, const SIMDVector& b) { return _mm_mul_ps(a.mVal, b.mVal); }
        
        template <int z, int y, int x, int w>
        static SIMDVector shuffle(const SIMDVector& a, const SIMDVector& b)
        {
            return _mm_shuffle_ps(a.mVal, b.mVal, ((z<<6)|(y<<4)|(x<<2)|w));
        }
    };
    
#endif
    
#if defined(__AVX512F__) || defined(__AVX__)
    
    template <>
    struct SIMDVector<double, 4> : public SIMDVectorBase<double, __m256d, 4>
    {
        SIMDVector() {}
        SIMDVector(__m256d a) : SIMDVectorBase(a) {}
        friend SIMDVector operator + (const SIMDVector &a, const SIMDVector &b) { return _mm256_add_pd(a.mVal, b.mVal); }
        friend SIMDVector operator - (const SIMDVector &a, const SIMDVector &b) { return _mm256_sub_pd(a.mVal, b.mVal); }
        friend SIMDVector operator * (const SIMDVector &a, const SIMDVector &b) { return _mm256_mul_pd(a.mVal, b.mVal); }
    };
    
    template <>
    struct SIMDVector<float, 8> : public SIMDVectorBase<float, __m256, 8>
    {
        SIMDVector() {}
        SIMDVector(__m256 a) : SIMDVectorBase(a) {}
        friend SIMDVector operator + (const SIMDVector &a, const SIMDVector &b) { return _mm256_add_ps(a.mVal, b.mVal); }
        friend SIMDVector operator - (const SIMDVector &a, const SIMDVector &b) { return _mm256_sub_ps(a.mVal, b.mVal); }
        friend SIMDVector operator * (const SIMDVector &a, const SIMDVector &b) { return _mm256_mul_ps(a.mVal, b.mVal); }
    };
    
#endif
    
#if defined(__AVX512F__)
    
    template <>
    struct SIMDVector<double, 8> : public SIMDVectorBase<double, __m512d, 8>
    {
        SIMDVector() {}
        SIMDVector(__m512d a) : SIMDVectorBase(a) {}
        friend SIMDVector operator + (const SIMDVector &a, const SIMDVector &b) { return _mm512_add_pd(a.mVal, b.mVal); }
        friend SIMDVector operator - (const SIMDVector &a, const SIMDVector &b) { return _mm512_sub_pd(a.mVal, b.mVal); }
        friend SIMDVector operator * (const SIMDVector &a, const SIMDVector &b) { return _mm512_mul_pd(a.mVal, b.mVal); }
    };
    
    template <>
    struct SIMDVector<float, 16> : public SIMDVectorBase<float, __m512, 16>
    {
        SIMDVector() {}
        SIMDVector(__m512 a) : SIMDVectorBase(a) {}
        friend SIMDVector operator + (const SIMDVector &a, const SIMDVector &b) { return _mm512_add_ps(a.mVal, b.mVal); }
        friend SIMDVector operator - (const SIMDVector &a, const SIMDVector &b) { return _mm512_sub_ps(a.mVal, b.mVal); }
        friend SIMDVector operator * (const SIMDVector &a, const SIMDVector &b) { return _mm512_mul_ps(a.mVal, b.mVal); }
    };
    
#endif
    
#if defined(__arm__)
    
    template <>
    struct SIMDVector<double, 1> : public SIMDVectorBase<double, double, 1>
    {
        SIMDVector() {}
        SIMDVector(double a) : SIMDVectorBase(a) {}
        friend SIMDVector operator + (const SIMDVector &a, const SIMDVector& b) { return a.mVal + b.mVal; }
        friend SIMDVector operator - (const SIMDVector &a, const SIMDVector& b) { return a.mVal - b.mVal; }
        friend SIMDVector operator * (const SIMDVector &a, const SIMDVector& b) { return a.mVal * b.mVal; }
        
        template <int y, int x>
        static SIMDVector shuffle(const SIMDVector& a, const SIMDVector& b)
        {
            //return _mm_shuffle_pd(a.mVal, b.mVal, (y<<1)|x);
        }
    };
    
    template <>
    struct SIMDVector<float, 4> : public SIMDVectorBase<float, float32x4_t, 4>
    {
        SIMDVector() {}
        SIMDVector(float32x4_t a) : SIMDVectorBase(a) {}
        friend SIMDVector operator + (const SIMDVector& a, const SIMDVector& b) { return vaddq_f32(a.mVal, b.mVal); }
        friend SIMDVector operator - (const SIMDVector& a, const SIMDVector& b) { return vsubq_f32(a.mVal, b.mVal); }
        friend SIMDVector operator * (const SIMDVector& a, const SIMDVector& b) { return vmulq_f32(a.mVal, b.mVal); }
        
        static SIMDVector shuffle_lo(const SIMDVector& a, const SIMDVector& b)
        {
            return vcombine_f32(vget_low_f32(a.mVal), vget_low_f32(b.mVal));
        }
        
        static SIMDVector shuffle_hi(const SIMDVector& a, const SIMDVector& b)
        {
            return vcombine_f32(vget_high_f32(a.mVal), vget_high_f32(b.mVal));
        }
        
        static SIMDVector shuffle_interleave_lo(const SIMDVector& a, const SIMDVector& b)
        {
            return vuzpq_f32(a.mVal, b.mVal).val[0];
        }
        
        static SIMDVector shuffle_interleave_hi(const SIMDVector& a, const SIMDVector& b)
        {
            return vuzpq_f32(a.mVal, b.mVal).val[1];
        }
    };
    
#endif
    
    // ******************** A Vector of 4 Items (Made of Vectors / Scalars) ******************** //
    
    template <class T>
    struct Vector4x
    {
        static const int size = 4;
        typedef typename T::scalar_type scalar_type;
        typedef Split<scalar_type> split_type;
        typedef Setup<scalar_type> setup_type;
        static const int array_size = 4 / T::size;
        
        Vector4x() {}
        Vector4x(const Vector4x *ptr) { *this = *ptr; }
        Vector4x(const typename T::scalar_type *array) { *this = *reinterpret_cast<const Vector4x *>(array); }
        
        // This template allows a static loop
        
        template <int first, int last>
        struct static_for
        {
            template <typename Fn>
            void operator()(Vector4x &result, const Vector4x &a, const Vector4x &b, Fn const& fn) const
            {
                if (first < last)
                {
                    result.mData[first] = fn(a.mData[first], b.mData[first]);
                    static_for<first + 1, last>()(result, a, b, fn);
                }
            }
        };
        
        // This specialisation avoids infinite recursion
        
        template <int N>
        struct static_for<N, N>
        {
            template <typename Fn>
            void operator()(Vector4x &result, const Vector4x &a, const Vector4x &b, Fn const& fn) const {}
        };
        
        template <typename Op>
        friend Vector4x operate(const Vector4x& a, const Vector4x& b, Op op)
        {
            Vector4x result;
            
            static_for<0, array_size>()(result, a, b, op);
            
            return result;
        }
        
        friend Vector4x operator + (const Vector4x& a, const Vector4x& b)
        {
            return operate(a, b, std::plus<T>());
        }
        
        friend Vector4x operator - (const Vector4x& a, const Vector4x& b)
        {
            return operate(a, b, std::minus<T>());
        }
        
        friend Vector4x operator * (const Vector4x& a, const Vector4x& b)
        {
            return operate(a, b, std::multiplies<T>());
        }
        
        T mData[array_size];
    };
    
    // ******************** Setup Creation and Destruction ******************** //
    
    // Creation
    
    template <class T>
    Setup<T> *create_setup(uintptr_t max_fft_log2)
    {
        Setup<T> *setup = allocate_aligned<Setup<T> >(1);
        
        // Set Max FFT Size
        
        setup->max_fft_log2 = max_fft_log2;
        
        // Create Tables
        
        for (uintptr_t i = trig_table_offset; i <= max_fft_log2; i++)
        {
            uintptr_t length = (uintptr_t) 1 << (i - 1);
            
            setup->tables[i - trig_table_offset].realp = allocate_aligned<T>(2 * length);
            setup->tables[i - trig_table_offset].imagp = setup->tables[i - trig_table_offset].realp + length;
            
            // Fill the Table
            
            T *table_real = setup->tables[i - trig_table_offset].realp;
            T *table_imag = setup->tables[i - trig_table_offset].imagp;
            
            for (uintptr_t j = 0; j < length; j++)
            {
                static const double pi = 3.14159265358979323846264338327950288;
                double angle = -(static_cast<double>(j)) * pi / static_cast<double>(length);
                
                *table_real++ = static_cast<T>(cos(angle));
                *table_imag++ = static_cast<T>(sin(angle));
            }
        }
        
        return setup;
    }
    
    // Destruction
    
    template <class T>
    void destroy_setup(Setup<T> *setup)
    {
        if (setup)
        {
            for (uintptr_t i = trig_table_offset; i <= setup->max_fft_log2; i++)
                deallocate_aligned(setup->tables[i - trig_table_offset].realp);
            
            deallocate_aligned(setup);
        }
    }
    
    // ******************** Shuffles for Pass 1 and 2 ******************** //
    
    // Template for an SIMD Vectors With 4 Elements
    
    template <class T>
    void shuffle4(const Vector4x<T> &A,
                  const Vector4x<T> &B,
                  const Vector4x<T> &C,
                  const Vector4x<T> &D,
                  Vector4x<T> *ptr1,
                  Vector4x<T> *ptr2,
                  Vector4x<T> *ptr3,
                  Vector4x<T> *ptr4)
    {}
    
    // Template for Scalars
    
    template<class T>
    void shuffle4(const Vector4x<SIMDVector<T, 1>> &A,
                  const Vector4x<SIMDVector<T, 1>> &B,
                  const Vector4x<SIMDVector<T, 1>> &C,
                  const Vector4x<SIMDVector<T, 1>> &D,
                  Vector4x<SIMDVector<T, 1>> *ptr1,
                  Vector4x<SIMDVector<T, 1>> *ptr2,
                  Vector4x<SIMDVector<T, 1>> *ptr3,
                  Vector4x<SIMDVector<T, 1>> *ptr4)
    {
        ptr1->mData[0] = A.mData[0];
        ptr1->mData[1] = C.mData[0];
        ptr1->mData[2] = B.mData[0];
        ptr1->mData[3] = D.mData[0];
        ptr2->mData[0] = A.mData[2];
        ptr2->mData[1] = C.mData[2];
        ptr2->mData[2] = B.mData[2];
        ptr2->mData[3] = D.mData[2];
        ptr3->mData[0] = A.mData[1];
        ptr3->mData[1] = C.mData[1];
        ptr3->mData[2] = B.mData[1];
        ptr3->mData[3] = D.mData[1];
        ptr4->mData[0] = A.mData[3];
        ptr4->mData[1] = C.mData[3];
        ptr4->mData[2] = B.mData[3];
        ptr4->mData[3] = D.mData[3];
    }
    
#if defined(__AVX512F__) || defined(__AVX__) || defined(__SSE__)
    
    // Template Specialisation for an SSE Float Packed (1 SIMD Element)
    
    typedef SIMDVector<float, 4> SSEFloat;
    typedef SIMDVector<double, 2> SSEDouble;
    
    template<>
    void shuffle4(const Vector4x<SSEFloat> &A,
                  const Vector4x<SSEFloat> &B,
                  const Vector4x<SSEFloat> &C,
                  const Vector4x<SSEFloat> &D,
                  Vector4x<SSEFloat> *ptr1,
                  Vector4x<SSEFloat> *ptr2,
                  Vector4x<SSEFloat> *ptr3,
                  Vector4x<SSEFloat> *ptr4)
    {
        const SSEFloat v1 = SSEFloat::shuffle<1, 0, 1, 0>(A.mData[0], C.mData[0]);
        const SSEFloat v2 = SSEFloat::shuffle<3, 2, 3, 2>(A.mData[0], C.mData[0]);
        const SSEFloat v3 = SSEFloat::shuffle<1, 0, 1, 0>(B.mData[0], D.mData[0]);
        const SSEFloat v4 = SSEFloat::shuffle<3, 2, 3, 2>(B.mData[0], D.mData[0]);
        
        ptr1->mData[0] = SSEFloat::shuffle<2, 0, 2, 0>(v1, v3);
        ptr2->mData[0] = SSEFloat::shuffle<2, 0, 2, 0>(v2, v4);
        ptr3->mData[0] = SSEFloat::shuffle<3, 1, 3, 1>(v1, v3);
        ptr4->mData[0] = SSEFloat::shuffle<3, 1, 3, 1>(v2, v4);
    }
    
    // Template Specialisation for an SSE Double Packed (2 SIMD Elements)
    
    template<>
    void shuffle4(const Vector4x<SSEDouble> &A,
                  const Vector4x<SSEDouble> &B,
                  const Vector4x<SSEDouble> &C,
                  const Vector4x<SSEDouble> &D,
                  Vector4x<SSEDouble> *ptr1,
                  Vector4x<SSEDouble> *ptr2,
                  Vector4x<SSEDouble> *ptr3,
                  Vector4x<SSEDouble> *ptr4)
    {
        ptr1->mData[0] = SSEDouble::shuffle<0, 0>(A.mData[0], C.mData[0]);
        ptr1->mData[1] = SSEDouble::shuffle<0, 0>(B.mData[0], D.mData[0]);
        ptr2->mData[0] = SSEDouble::shuffle<0, 0>(A.mData[1], C.mData[1]);
        ptr2->mData[1] = SSEDouble::shuffle<0, 0>(B.mData[1], D.mData[1]);
        ptr3->mData[0] = SSEDouble::shuffle<1, 1>(A.mData[0], C.mData[0]);
        ptr3->mData[1] = SSEDouble::shuffle<1, 1>(B.mData[0], D.mData[0]);
        ptr4->mData[0] = SSEDouble::shuffle<1, 1>(A.mData[1], C.mData[1]);
        ptr4->mData[1] = SSEDouble::shuffle<1, 1>(B.mData[1], D.mData[1]);
    }
    
#endif
    
#if defined (__arm__)
    
    typedef SIMDVector<float, 4> ARMFloat;
    
    template<>
    void shuffle4(const Vector4x<ARMFloat> &A,
                  const Vector4x<ARMFloat> &B,
                  const Vector4x<ARMFloat> &C,
                  const Vector4x<ARMFloat> &D,
                  Vector4x<ARMFloat> *ptr1,
                  Vector4x<ARMFloat> *ptr2,
                  Vector4x<ARMFloat> *ptr3,
                  Vector4x<ARMFloat> *ptr4)
    {
        const ARMFloat v1 = ARMFloat::shuffle_lo(A.mData[0], C.mData[0]);
        const ARMFloat v2 = ARMFloat::shuffle_hi(A.mData[0], C.mData[0]);
        const ARMFloat v3 = ARMFloat::shuffle_lo(B.mData[0], D.mData[0]);
        const ARMFloat v4 = ARMFloat::shuffle_hi(B.mData[0], D.mData[0]);
        
        ptr1->mData[0] = ARMFloat::shuffle_interleave_lo(v1, v3);
        ptr2->mData[0] = ARMFloat::shuffle_interleave_lo(v2, v4);
        ptr3->mData[0] = ARMFloat::shuffle_interleave_hi(v1, v3);
        ptr4->mData[0] = ARMFloat::shuffle_interleave_hi(v2, v4);
    }
    
#endif
    
    // ******************** Templates (Scalar or SIMD) for FFT Passes ******************** //
    
    // Pass One and Two with Re-ordering
    
    template <class T>
    void pass_1_2_reorder(Split<typename T::scalar_type> *input, uintptr_t length)
    {
        typedef Vector4x<T> Vector;
        
        Vector *r1_ptr = reinterpret_cast<Vector *>(input->realp);
        Vector *r2_ptr = r1_ptr + (length >> 4);
        Vector *r3_ptr = r2_ptr + (length >> 4);
        Vector *r4_ptr = r3_ptr + (length >> 4);
        Vector *i1_ptr = reinterpret_cast<Vector *>(input->imagp);
        Vector *i2_ptr = i1_ptr + (length >> 4);
        Vector *i3_ptr = i2_ptr + (length >> 4);
        Vector *i4_ptr = i3_ptr + (length >> 4);
        
        for (uintptr_t i = 0; i < length >> 4; i++)
        {
            const Vector r1 = *r1_ptr;
            const Vector i1 = *i1_ptr;
            const Vector r2 = *r2_ptr;
            const Vector i2 = *i2_ptr;
            
            const Vector r3 = *r3_ptr;
            const Vector i3 = *i3_ptr;
            const Vector r4 = *r4_ptr;
            const Vector i4 = *i4_ptr;
            
            const Vector r5 = r1 + r3;
            const Vector r6 = r2 + r4;
            const Vector r7 = r1 - r3;
            const Vector r8 = r2 - r4;
            
            const Vector i5 = i1 + i3;
            const Vector i6 = i2 + i4;
            const Vector i7 = i1 - i3;
            const Vector i8 = i2 - i4;
            
            const Vector rA = r5 + r6;
            const Vector rB = r5 - r6;
            const Vector rC = r7 + i8;
            const Vector rD = r7 - i8;
            
            const Vector iA = i5 + i6;
            const Vector iB = i5 - i6;
            const Vector iC = i7 - r8;
            const Vector iD = i7 + r8;
            
            shuffle4(rA, rB, rC, rD, r1_ptr++, r2_ptr++, r3_ptr++, r4_ptr++);
            shuffle4(iA, iB, iC, iD, i1_ptr++, i2_ptr++, i3_ptr++, i4_ptr++);
        }
    }
    
    // Pass Three Twiddle Factors
    
    template <class T>
    void pass_3_twiddle(Vector4x<T> &tr, Vector4x<T> &ti)
    {
        static const double SQRT_2_2 = 0.70710678118654752440084436210484904;
        
        typename T::scalar_type _______zero = static_cast<typename T::scalar_type>(0);
        typename T::scalar_type ________one = static_cast<typename T::scalar_type>(1);
        typename T::scalar_type neg_____one = static_cast<typename T::scalar_type>(-1);
        typename T::scalar_type ____sqrt2_2 = static_cast<typename T::scalar_type>(SQRT_2_2);
        typename T::scalar_type neg_sqrt2_2 = static_cast<typename T::scalar_type>(-SQRT_2_2);
        
        typename T::scalar_type str[4] = {________one, ____sqrt2_2, _______zero, neg_sqrt2_2};
        typename T::scalar_type sti[4] = {_______zero, neg_sqrt2_2, neg_____one, neg_sqrt2_2};
        
        tr = Vector4x<T>(str);
        ti = Vector4x<T>(sti);
    }
    
    // Pass Three With Re-ordering
    
    template <class T>
    void pass_3_reorder(Split<typename T::scalar_type> *input, uintptr_t length)
    {
        typedef Vector4x<T> Vector;
        
        uintptr_t offset = length >> 5;
        uintptr_t outerLoop = length >> 6;
        
        Vector tr;
        Vector ti;
        
        pass_3_twiddle(tr, ti);
        
        Vector *r1_ptr = reinterpret_cast<Vector *>(input->realp);
        Vector *i1_ptr = reinterpret_cast<Vector *>(input->imagp);
        Vector *r2_ptr = r1_ptr + offset;
        Vector *i2_ptr = i1_ptr + offset;
        
        for (uintptr_t i = 0, j = 0; i < length >> 1; i += 8)
        {
            // Get input
            
            const Vector r1(r1_ptr);
            const Vector r2(r1_ptr + 1);
            const Vector i1(i1_ptr);
            const Vector i2(i1_ptr + 1);
            
            const Vector r3(r2_ptr);
            const Vector r4(r2_ptr + 1);
            const Vector i3(i2_ptr);
            const Vector i4(i2_ptr + 1);
            
            // Multiply by twiddle
            
            const Vector r5 = (r3 * tr) - (i3 * ti);
            const Vector i5 = (r3 * ti) + (i3 * tr);
            const Vector r6 = (r4 * tr) - (i4 * ti);
            const Vector i6 = (r4 * ti) + (i4 * tr);
            
            // Store output (swapping as necessary)
            
            *r1_ptr++ = r1 + r5;
            *r1_ptr++ = r1 - r5;
            *i1_ptr++ = i1 + i5;
            *i1_ptr++ = i1 - i5;
            
            *r2_ptr++ = r2 + r6;
            *r2_ptr++ = r2 - r6;
            *i2_ptr++ = i2 + i6;
            *i2_ptr++ = i2 - i6;
            
            if (!(++j % outerLoop))
            {
                r1_ptr += offset;
                r2_ptr += offset;
                i1_ptr += offset;
                i2_ptr += offset;
            }
        }
    }
    
    // Pass Three Without Re-ordering
    
    template <class T>
    void pass_3(Split<typename T::scalar_type> *input, uintptr_t length)
    {
        typedef Vector4x<T> Vector;
        
        Vector tr;
        Vector ti;
        
        pass_3_twiddle(tr, ti);
        
        Vector *r_ptr = reinterpret_cast<Vector *>(input->realp);
        Vector *i_ptr = reinterpret_cast<Vector *>(input->imagp);
        
        for (uintptr_t i = 0; i < length >> 3; i++)
        {
            // Get input
            
            const Vector r1(r_ptr);
            const Vector r2(r_ptr + 1);
            const Vector i1(i_ptr);
            const Vector i2(i_ptr + 1);
            
            // Multiply by twiddle
            
            const Vector r3 = (r2 * tr) - (i2 * ti);
            const Vector i3 = (r2 * ti) + (i2 * tr);
            
            // Store output
            
            *r_ptr++ = r1 + r3;
            *r_ptr++ = r1 - r3;
            *i_ptr++ = i1 + i3;
            *i_ptr++ = i1 - i3;
            
        }
    }
    
    // A Pass Requiring Tables With Re-ordering
    
    template <class T>
    void pass_trig_table_reorder(typename T::split_type *input, typename T::setup_type *setup, uintptr_t length, uintptr_t pass)
    {
        uintptr_t size = 2 << pass;
        uintptr_t incr = size / (T::size << 1);
        uintptr_t loop = size;
        uintptr_t offset = (length >> pass) / (T::size << 1);
        uintptr_t outerLoop = ((length >> 1) / size) / ((uintptr_t) 1 << pass);
        
        T *r1_ptr = reinterpret_cast<T *>(input->realp);
        T *i1_ptr = reinterpret_cast<T *>(input->imagp);
        T *r2_ptr = r1_ptr + offset;
        T *i2_ptr = i1_ptr + offset;
        
        for (uintptr_t i = 0, j = 0; i < (length >> 1); loop += size)
        {
            T *tr_ptr = reinterpret_cast<T *>(setup->tables[pass - (trig_table_offset - 1)].realp);
            T *ti_ptr = reinterpret_cast<T *>(setup->tables[pass - (trig_table_offset - 1)].imagp);
            
            for (; i < loop; i += (T::size << 1))
            {
                // Get input and twiddle
                
                const T tr = *tr_ptr++;
                const T ti = *ti_ptr++;
                
                const T r1 = *r1_ptr;
                const T i1 = *i1_ptr;
                const T r2 = *r2_ptr;
                const T i2 = *i2_ptr;
                
                const T r3 = *(r1_ptr + incr);
                const T i3 = *(i1_ptr + incr);
                const T r4 = *(r2_ptr + incr);
                const T i4 = *(i2_ptr + incr);
                
                // Multiply by twiddle
                
                const T r5 = (r2 * tr) - (i2 * ti);
                const T i5 = (r2 * ti) + (i2 * tr);
                const T r6 = (r4 * tr) - (i4 * ti);
                const T i6 = (r4 * ti) + (i4 * tr);
                
                // Store output (swapping as necessary)
                
                *r1_ptr = r1 + r5;
                *(r1_ptr++ + incr) = r1 - r5;
                *i1_ptr = i1 + i5;
                *(i1_ptr++ + incr) = i1 - i5;
                
                *r2_ptr = r3 + r6;
                *(r2_ptr++ + incr) = r3 - r6;
                *i2_ptr = i3 + i6;
                *(i2_ptr++ + incr) = i3 - i6;
            }
            
            r1_ptr += incr;
            r2_ptr += incr;
            i1_ptr += incr;
            i2_ptr += incr;
            
            if (!(++j % outerLoop))
            {
                r1_ptr += offset;
                r2_ptr += offset;
                i1_ptr += offset;
                i2_ptr += offset;
            }
        }
    }
    
    // A Pass Requiring Tables Without Re-ordering
    
    template <class T>
    void pass_trig_table(typename T::split_type *input, typename T::setup_type *setup, uintptr_t length, uintptr_t pass)
    {
        uintptr_t size = 2 << pass;
        uintptr_t incr = size / (T::size << 1);
        uintptr_t loop = size;
        
        T *r1_ptr = reinterpret_cast<T *>(input->realp);
        T *i1_ptr = reinterpret_cast<T *>(input->imagp);
        T *r2_ptr = r1_ptr + (size >> 1) / T::size;
        T *i2_ptr = i1_ptr + (size >> 1) / T::size;
        
        for (uintptr_t i = 0; i < length; loop += size)
        {
            T *tr_ptr = reinterpret_cast<T *>(setup->tables[pass - (trig_table_offset - 1)].realp);
            T *ti_ptr = reinterpret_cast<T *>(setup->tables[pass - (trig_table_offset - 1)].imagp);
            
            for (; i < loop; i += (T::size << 1))
            {
                // Get input and twiddle factors
                
                const T tr = *tr_ptr++;
                const T ti = *ti_ptr++;
                
                const T r1 = *r1_ptr;
                const T i1 = *i1_ptr;
                const T r2 = *r2_ptr;
                const T i2 = *i2_ptr;
                
                // Multiply by twiddle
                
                const T r3 = (r2 * tr) - (i2 * ti);
                const T i3 = (r2 * ti) + (i2 * tr);
                
                // Store output
                
                *r1_ptr++ = r1 + r3;
                *i1_ptr++ = i1 + i3;
                *r2_ptr++ = r1 - r3;
                *i2_ptr++ = i1 - i3;
            }
            
            r1_ptr += incr;
            r2_ptr += incr;
            i1_ptr += incr;
            i2_ptr += incr;
        }
    }
    
    // A Real Pass Requiring Trig Tables (Never Reorders)
    
    template <bool ifft, class T>
    void pass_real_trig_table(Split<T> *input, Setup<T> *setup, uintptr_t fft_log2)
    {
        uintptr_t length = (uintptr_t) 1 << (fft_log2 - 1);
        uintptr_t lengthM1 = length - 1;
        
        T *r1_ptr = input->realp;
        T *i1_ptr = input->imagp;
        T *r2_ptr = r1_ptr + lengthM1;
        T *i2_ptr = i1_ptr + lengthM1;
        T *tr_ptr = setup->tables[fft_log2 - trig_table_offset].realp;
        T *ti_ptr = setup->tables[fft_log2 - trig_table_offset].imagp;
        
        // Do DC and Nyquist (note that the complex values can be considered periodic)
        
        const T t1 = r1_ptr[0] + i1_ptr[0];
        const T t2 = r1_ptr[0] - i1_ptr[0];
        
        *r1_ptr++ = ifft ? t1 : t1 + t1;
        *i1_ptr++ = ifft ? t2 : t2 + t2;
        
        tr_ptr++;
        ti_ptr++;
        
        // N.B. - The last time through this loop will write the same values twice to the same places
        // N.B. - In this case: t1 == 0, i4 == 0, r1_ptr == r2_ptr, i1_ptr == i2_ptr
        
        for (uintptr_t i = 0; i < (length >> 1); i++)
        {
            const T tr = ifft ? -*tr_ptr++ : *tr_ptr++;
            const T ti = *ti_ptr++;
            
            // Get input
            
            const T r1 = *r1_ptr;
            const T i1 = *i1_ptr;
            const T r2 = *r2_ptr;
            const T i2 = *i2_ptr;
            
            const T r3 = r1 + r2;
            const T i3 = i1 + i2;
            const T r4 = r1 - r2;
            const T i4 = i1 - i2;
            
            const T t1 = (tr * i3) + (ti * r4);
            const T t2 = (ti * i3) - (tr * r4);
            
            // Store output
            
            *r1_ptr++ = r3 + t1;
            *i1_ptr++ = t2 + i4;
            *r2_ptr-- = r3 - t1;
            *i2_ptr-- = t2 - i4;
        }
    }
    
    // ******************** Scalar-Only Small FFTs ******************** //
    
    // Small Complex FFTs (2, 4 or 8 points)
    
    template <class T>
    void small_fft(Split<T> *input, uintptr_t fft_log2)
    {
        T *r1_ptr = input->realp;
        T *i1_ptr = input->imagp;
        
        if (fft_log2 == 1)
        {
            const T r1 = r1_ptr[0];
            const T r2 = r1_ptr[1];
            const T i1 = i1_ptr[0];
            const T i2 = i1_ptr[1];
            
            r1_ptr[0] = r1 + r2;
            r1_ptr[1] = r1 - r2;
            i1_ptr[0] = i1 + i2;
            i1_ptr[1] = i1 - i2;
        }
        else if (fft_log2 == 2)
        {
            const T r5 = r1_ptr[0];
            const T r6 = r1_ptr[1];
            const T r7 = r1_ptr[2];
            const T r8 = r1_ptr[3];
            const T i5 = i1_ptr[0];
            const T i6 = i1_ptr[1];
            const T i7 = i1_ptr[2];
            const T i8 = i1_ptr[3];
            
            // Pass One
            
            const T r1 = r5 + r7;
            const T r2 = r5 - r7;
            const T r3 = r6 + r8;
            const T r4 = r6 - r8;
            const T i1 = i5 + i7;
            const T i2 = i5 - i7;
            const T i3 = i6 + i8;
            const T i4 = i6 - i8;
            
            // Pass Two
            
            r1_ptr[0] = r1 + r3;
            r1_ptr[1] = r2 + i4;
            r1_ptr[2] = r1 - r3;
            r1_ptr[3] = r2 - i4;
            i1_ptr[0] = i1 + i3;
            i1_ptr[1] = i2 - r4;
            i1_ptr[2] = i1 - i3;
            i1_ptr[3] = i2 + r4;
        }
        else if (fft_log2 == 3)
        {
            // Pass One
            
            const T r1 = r1_ptr[0] + r1_ptr[4];
            const T r2 = r1_ptr[0] - r1_ptr[4];
            const T r3 = r1_ptr[2] + r1_ptr[6];
            const T r4 = r1_ptr[2] - r1_ptr[6];
            const T r5 = r1_ptr[1] + r1_ptr[5];
            const T r6 = r1_ptr[1] - r1_ptr[5];
            const T r7 = r1_ptr[3] + r1_ptr[7];
            const T r8 = r1_ptr[3] - r1_ptr[7];
            
            const T i1 = i1_ptr[0] + i1_ptr[4];
            const T i2 = i1_ptr[0] - i1_ptr[4];
            const T i3 = i1_ptr[2] + i1_ptr[6];
            const T i4 = i1_ptr[2] - i1_ptr[6];
            const T i5 = i1_ptr[1] + i1_ptr[5];
            const T i6 = i1_ptr[1] - i1_ptr[5];
            const T i7 = i1_ptr[3] + i1_ptr[7];
            const T i8 = i1_ptr[3] - i1_ptr[7];
            
            // Pass Two
            
            r1_ptr[0] = r1 + r3;
            r1_ptr[1] = r2 + i4;
            r1_ptr[2] = r1 - r3;
            r1_ptr[3] = r2 - i4;
            r1_ptr[4] = r5 + r7;
            r1_ptr[5] = r6 + i8;
            r1_ptr[6] = r5 - r7;
            r1_ptr[7] = r6 - i8;
            
            i1_ptr[0] = i1 + i3;
            i1_ptr[1] = i2 - r4;
            i1_ptr[2] = i1 - i3;
            i1_ptr[3] = i2 + r4;
            i1_ptr[4] = i5 + i7;
            i1_ptr[5] = i6 - r8;
            i1_ptr[6] = i5 - i7;
            i1_ptr[7] = i6 + r8;
            
            // Pass Three
            
            pass_3<SIMDVector<T, 1>>(input, 8);
        }
    }
    
    // Small Real FFTs (2 or 4 points)
    
    template <bool ifft, class T>
    void small_real_fft(Split<T> *input, uintptr_t fft_log2)
    {
        T *r1_ptr = input->realp;
        T *i1_ptr = input->imagp;
        
        if (fft_log2 == 1)
        {
            const T r1 = ifft ? r1_ptr[0] : r1_ptr[0] + r1_ptr[0];
            const T r2 = ifft ? i1_ptr[0] : i1_ptr[0] + i1_ptr[0];
            
            r1_ptr[0] = (r1 + r2);
            i1_ptr[0] = (r1 - r2);
        }
        else if (fft_log2 == 2)
        {
            if (!ifft)
            {
                // Pass One
                
                const T r1 = r1_ptr[0] + r1_ptr[1];
                const T r2 = r1_ptr[0] - r1_ptr[1];
                const T i1 = i1_ptr[0] + i1_ptr[1];
                const T i2 = i1_ptr[1] - i1_ptr[0];
                
                // Pass Two
                
                const T r3 = r1 + i1;
                const T i3 = r1 - i1;
                
                r1_ptr[0] = r3 + r3;
                r1_ptr[1] = r2 + r2;
                i1_ptr[0] = i3 + i3;
                i1_ptr[1] = i2 + i2;
            }
            else
            {
                const T i1 = r1_ptr[0];
                const T r2 = r1_ptr[1] + r1_ptr[1];
                const T i2 = i1_ptr[0];
                const T r4 = i1_ptr[1] + i1_ptr[1];
                
                // Pass One
                
                const T r1 = i1 + i2;
                const T r3 = i1 - i2;
                
                // Pass Two
                
                r1_ptr[0] = r1 + r2;
                r1_ptr[1] = r1 - r2;
                i1_ptr[0] = r3 - r4;
                i1_ptr[1] = r3 + r4;
            }
        }
    }
    
    // ******************** Unzip and Zip ******************** //
    
#ifdef USE_APPLE_FFT
    
    template<class T>
    void unzip_complex(const T *input, DSPSplitComplex *output, uintptr_t half_length)
    {
        vDSP_ctoz((COMPLEX *) input, (vDSP_Stride) 2, output, (vDSP_Stride) 1, (vDSP_Length) half_length);
    }
    
    template<class T>
    void unzip_complex(const T *input, DSPDoubleSplitComplex *output, uintptr_t half_length)
    {
        vDSP_ctozD((DOUBLE_COMPLEX *) input, (vDSP_Stride) 2, output, (vDSP_Stride) 1, (vDSP_Length) half_length);
    }
    
    template<>
    void unzip_complex(const float *input, DSPDoubleSplitComplex *output, uintptr_t half_length)
    {
        double *realp = output->realp;
        double *imagp = output->imagp;
        
        for (uintptr_t i = 0; i < half_length; i++)
        {
            *realp++ = static_cast<double>(*input++);
            *imagp++ = static_cast<double>(*input++);
        }
    }
    
#endif
    
    // Unzip
    
    template <class T, class U>
    void unzip_complex(const U *input, Split<T> *output, uintptr_t half_length)
    {
        T *realp = output->realp;
        T *imagp = output->imagp;
        
        for (uintptr_t i = 0; i < half_length; i++)
        {
            *realp++ = static_cast<T>(*input++);
            *imagp++ = static_cast<T>(*input++);
        }
    }
    
    template<>
    void unzip_complex(const float *input, Split<float> *output, uintptr_t half_length)
    {
        /*
         if (isAligned(input) && isAligned(output->realp) && isAligned(output->imagp))
         {
         const ARMFloat *inp = reinterpret_cast<const ARMFloat*>(input);
         ARMFloat *realp = reinterpret_cast<ARMFloat*>(output->realp);
         ARMFloat *imagp = reinterpret_cast<ARMFloat*>(output->imagp);
         
         for (uintptr_t i = 0; i < (half_length >> 2); i++, inp += 2)
         {
         float32x4x2_t v = vuzpq_f32(inp[0].mVal, inp[1].mVal);
         *realp++ = v.val[0];
         *imagp++ = v.val[1];
         }
         }
         else*/
        {
            float *realp = output->realp;
            float *imagp = output->imagp;
            
            for (uintptr_t i = 0; i < half_length; i++)
            {
                *realp++ = *input++;
                *imagp++ = *input++;
            }
        }
    }
    
    // Zip
    
    template <class T>
    void zip_complex(const Split<T> *input, T *output, uintptr_t half_length)
    {
        const T *realp = input->realp;
        const T *imagp = input->imagp;
        
        for (uintptr_t i = 0; i < half_length; i++)
        {
            *output++ = *realp++;
            *output++ = *imagp++;
        }
    }
    
    template<>
    void zip_complex(const Split<float> *input, float *output, uintptr_t half_length)
    {/*
      if (isAligned(output) && isAligned(input->realp) && isAligned(input->imagp))
      {
      ARMFloat *outp = reinterpret_cast<ARMFloat*>(output);
      const ARMFloat *realp = reinterpret_cast<const ARMFloat*>(input->realp);
      const ARMFloat *imagp = reinterpret_cast<const ARMFloat*>(input->imagp);
      
      for (uintptr_t i = 0; i < (half_length >> 2); i++, realp++, imagp++)
      {
      float32x4x2_t v = vzipq_f32(realp->mVal, imagp->mVal);
      *outp++ = v.val[0];
      *outp++ = v.val[1];
      }
      }
      else*/
        {
            float *realp = input->realp;
            float *imagp = input->imagp;
            
            for (uintptr_t i = 0; i < half_length; i++)
            {
                *output++ = *realp++;
                *output++ = *imagp++;
            }
        }
    }
    
    // Unzip With Zero Padding
    
    template <class T, class U, class V>
    void unzip_zero(const U *input, V *output, uintptr_t in_length, uintptr_t log2n)
    {
        T odd_sample = static_cast<T>(input[in_length - 1]);
        T *realp = output->realp;
        T *imagp = output->imagp;
        
        // Check input length is not longer than the FFT size and unzip an even number of samples
        
        uintptr_t fft_size = static_cast<uintptr_t>(1 << log2n);
        in_length = std::min(fft_size, in_length);
        unzip_complex(input, output, in_length >> 1);
        
        // If necessary replace the odd sample, and zero pad the input
        
        if (fft_size > in_length)
        {
            uintptr_t end_point1 = in_length >> 1;
            uintptr_t end_point2 = fft_size >> 1;
            
            realp[end_point1] = (in_length & 1) ? odd_sample : static_cast<T>(0);
            imagp[end_point1] = static_cast<T>(0);
            
            for (uintptr_t i = end_point1 + 1; i < end_point2; i++)
            {
                realp[i] = static_cast<T>(0);
                imagp[i] = static_cast<T>(0);
            }
        }
    }
    
    // ******************** FFT Pass Control ******************** //
    
    // FFT Passes Template
    
    template <class T, class U, class V, class W>
    void fft_passes(Split<typename T::scalar_type> *input, Setup<typename T::scalar_type> *setup, uintptr_t fft_log2)
    {
        const uintptr_t length = (uintptr_t) 1 << fft_log2;
        uintptr_t i;
        
        pass_1_2_reorder<T>(input, length);
        
        if (fft_log2 > 5)
            pass_3_reorder<U>(input, length);
        else
            pass_3<U>(input, length);
        
        if (3 < (fft_log2 >> 1))
            pass_trig_table_reorder<V>(input, setup, length, 3);
        else
            pass_trig_table<V>(input, setup, length, 3);
        
        for (i = 4; i < (fft_log2 >> 1); i++)
            pass_trig_table_reorder<W>(input, setup, length, i);
        
        for (; i < fft_log2; i++)
            pass_trig_table<W>(input, setup, length, i);
    }
    
    // SIMD Options
    
    template <class T>
    void fft_passes_simd(Split<T> *input, Setup<T> *setup, uintptr_t fft_log2)
    {
        typedef SIMDVector<T, SIMDLimits<T>::max_size <  4 ? SIMDLimits<T>::max_size :  4> Type1;
        typedef SIMDVector<T, SIMDLimits<T>::max_size <  4 ? SIMDLimits<T>::max_size :  4> Type2;
        typedef SIMDVector<T, SIMDLimits<T>::max_size <  8 ? SIMDLimits<T>::max_size :  8> Type3;
        typedef SIMDVector<T, SIMDLimits<T>::max_size < 16 ? SIMDLimits<T>::max_size : 16> Type4;
        
        fft_passes<Type1, Type2, Type3, Type4>(input, setup, fft_log2);
    }
    
    // ******************** Main Calls ******************** //
    
    // A Complex FFT
    
    template <class T>
    void hisstools_fft(Split<T> *input, Setup<T> *setup, uintptr_t fft_log2)
    {
        typedef SIMDVector<T, 1> Scalar;
        
        if (fft_log2 >= 4)
        {
            if (!isAligned(input->realp) || !isAligned(input->imagp))
                fft_passes<Scalar, Scalar, Scalar, Scalar>(input, setup, fft_log2);
            else
                fft_passes_simd(input, setup, fft_log2);
        }
        else
            small_fft(input, fft_log2);
    }
    
    // A Complex iFFT
    
    template <class T>
    void hisstools_ifft(Split<T> *input, Setup<T> *setup, uintptr_t fft_log2)
    {
        Split<T> swap(input->imagp, input->realp);
        hisstools_fft(&swap, setup, fft_log2);
    }
    
    // A Real FFT
    
    template <class T>
    void hisstools_rfft(Split<T> *input, Setup<T> *setup, uintptr_t fft_log2)
    {
        if (fft_log2 >= 3)
        {
            hisstools_fft(input, setup, fft_log2 - 1);
            pass_real_trig_table<false>(input, setup, fft_log2);
        }
        else
            small_real_fft<false>(input, fft_log2);
    }
    
    // A Real iFFT
    
    template <class T>
    void hisstools_rifft(Split<T> *input, Setup<T> *setup, uintptr_t fft_log2)
    {
        if (fft_log2 >= 3)
        {
            pass_real_trig_table<true>(input, setup, fft_log2);
            hisstools_ifft(input, setup, fft_log2 - 1);
        }
        else
            small_real_fft<true>(input, fft_log2);
    }
    
} /* hisstools_fft_impl */

