
#ifndef HISSTOOLS_FFT_CORE_HPP
#define HISSTOOLS_FFT_CORE_HPP

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <functional>

#include "../SIMDSupport.hpp"

// Type definitions for Apple / HISSTools FFT

#if defined (USE_APPLE_FFT)

#include <Accelerate/Accelerate.h>

// Type specialisations for use with the Apple FFT

// Splits

template<>
struct Split<double> : DSPDoubleSplitComplex, impl::TypeBase<double>
{
    Split() {}
    Split(double *real, double *imag) : DSPDoubleSplitComplex{ real, imag } {}
};

template<>
struct Split<float> : DSPSplitComplex, impl::TypeBase<float>
{
    Split() {}
    Split(float *real, float *imag) : DSPSplitComplex{ real, imag } {}
};

// Setups

template<>
struct Setup<double> : impl::SetupBase<double, FFTSetupD>
{
    Setup() {}
    Setup(FFTSetupD setup) : SetupBase(setup) {}
};

template<>
struct Setup<float> : impl::SetupBase<float, FFTSetup>
{
    Setup() {}
    Setup(FFTSetup setup) : SetupBase(setup) {}
};

#endif

struct hisstools_fft_impl
{
    // Offset for Table
    
    static constexpr uintptr_t trig_table_offset = 3;
    
    // Setup Structures

    template <class T>
    struct SetupType
    {
        template <typename U>
        const U *get_realp(uintptr_t pass)
        {
            return reinterpret_cast<U *>(m_tables[pass - (trig_table_offset - 1)].realp);
        }
        
        template <typename U>
        const U *get_imagp(uintptr_t pass)
        {
            return reinterpret_cast<U *>(m_tables[pass - (trig_table_offset - 1)].imagp);
        }

        uintptr_t m_max_fft_log2;
        Split<T> m_tables[28];
    };
    
// Aligned Allocation
/*
 #if defined(__APPLE__) || defined (__linux__) || defined(__EMSCRIPTEN__)
 
 template <class T>
 T *allocate_aligned(size_t size)
 {
 void *mem = nullptr;
 if (!posix_memalign(&mem, alignment_size, size * sizeof(T)))
 return static_cast<T *>(mem);
 else
 return nullptr;
 }
 */

    // ******************** Basic Definitions ******************** //

    static constexpr int alignment_size = SIMDLimits<float>::max_size * sizeof(float);
    
    template <class T>
    static bool is_aligned(const T *ptr) { return !(reinterpret_cast<uintptr_t>(ptr) % alignment_size); }
    
    // Data Type Definitions
        
    template <class T, int vec_size>
    using Vector4x = SizedVector<T, vec_size, 4>;

    // ******************** Setup Creation and Destruction ******************** //

    // Creation

    template <class T>
    static void create_setup(SetupType<T> *& setup, uintptr_t max_fft_log2)
    {
        setup = new(SetupType<T>);
        
        // Set Max FFT Size
        
        setup->m_max_fft_log2 = max_fft_log2;
        
        // Create Tables
        
        for (uintptr_t i = trig_table_offset; i <= max_fft_log2; i++)
        {
            uintptr_t length = static_cast<uintptr_t>(1u) << (i - 1u);
            
            setup->m_tables[i - trig_table_offset].realp = allocate_aligned<T>(2 * length);
            setup->m_tables[i - trig_table_offset].imagp = setup->m_tables[i - trig_table_offset].realp + length;
            
            // Fill the Table
            
            T *table_real = setup->m_tables[i - trig_table_offset].realp;
            T *table_imag = setup->m_tables[i - trig_table_offset].imagp;
            
            for (uintptr_t j = 0; j < length; j++)
            {
                static const double pi = 3.14159265358979323846264338327950288;
                double angle = -(static_cast<double>(j)) * pi / static_cast<double>(length);
                
                *table_real++ = static_cast<T>(cos(angle));
                *table_imag++ = static_cast<T>(sin(angle));
            }
        }
    }

    // Destruction

    template <class T>
    static void destroy_setup(SetupType<T> *setup)
    {
        if (setup)
        {
            for (uintptr_t i = trig_table_offset; i <= setup->m_max_fft_log2; i++)
                deallocate_aligned(setup->m_tables[i - trig_table_offset].realp);
            
            delete(setup);
        }
    }

    // ******************** Shuffles for Pass 1 and 2 ******************** //
    
    struct shuffler
    {
        // Template for an SIMD Vectors With 4 Elements
        
        template <class T, int vec_size>
        static void shuffle4(const Vector4x<T, vec_size> &,
                             const Vector4x<T, vec_size> &,
                             const Vector4x<T, vec_size> &,
                             const Vector4x<T, vec_size> &,
                             Vector4x<T, vec_size> *,
                             Vector4x<T, vec_size> *,
                             Vector4x<T, vec_size> *,
                             Vector4x<T, vec_size> *)
        {
            static_assert(vec_size != vec_size, "Shuffle not implemented for this type");
        }
        
        // Template for Scalars
        
        template <class T>
        static void shuffle4(const Vector4x<T, 1> &A,
                             const Vector4x<T, 1> &B,
                             const Vector4x<T, 1> &C,
                             const Vector4x<T, 1> &D,
                             Vector4x<T, 1> *ptr1,
                             Vector4x<T, 1> *ptr2,
                             Vector4x<T, 1> *ptr3,
                             Vector4x<T, 1> *ptr4)
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
        
    #if defined(__SSE__) || defined(__AVX__) || defined(__AVX512F__)
        
        // Shuffle for an SSE Float Packed (1 SIMD Element)
        
        static void shuffle4(const Vector4x<float, 4> &A,
                             const Vector4x<float, 4> &B,
                             const Vector4x<float, 4> &C,
                             const Vector4x<float, 4> &D,
                             Vector4x<float, 4> *ptr1,
                             Vector4x<float, 4> *ptr2,
                             Vector4x<float, 4> *ptr3,
                             Vector4x<float, 4> *ptr4)
        {
            const __m128 v1 = _mm_unpacklo_ps(A.mData[0].mVal, B.mData[0].mVal);
            const __m128 v2 = _mm_unpackhi_ps(A.mData[0].mVal, B.mData[0].mVal);
            const __m128 v3 = _mm_unpacklo_ps(C.mData[0].mVal, D.mData[0].mVal);
            const __m128 v4 = _mm_unpackhi_ps(C.mData[0].mVal, D.mData[0].mVal);

            ptr1->mData[0] = _mm_unpacklo_ps(v1, v3);
            ptr2->mData[0] = _mm_unpacklo_ps(v2, v4);
            ptr3->mData[0] = _mm_unpackhi_ps(v1, v3);
            ptr4->mData[0] = _mm_unpackhi_ps(v2, v4);
        }
        
        // Shuffle for an SSE Double Packed (2 SIMD Elements)
        
        static void shuffle4(const Vector4x<double, 2> &A,
                             const Vector4x<double, 2> &B,
                             const Vector4x<double, 2> &C,
                             const Vector4x<double, 2> &D,
                             Vector4x<double, 2> *ptr1,
                             Vector4x<double, 2> *ptr2,
                             Vector4x<double, 2> *ptr3,
                             Vector4x<double, 2> *ptr4)
        {
            ptr1->mData[0] = _mm_unpacklo_pd(A.mData[0].mVal, C.mData[0].mVal);
            ptr1->mData[1] = _mm_unpacklo_pd(B.mData[0].mVal, D.mData[0].mVal);
            ptr2->mData[0] = _mm_unpacklo_pd(A.mData[1].mVal, C.mData[1].mVal);
            ptr2->mData[1] = _mm_unpacklo_pd(B.mData[1].mVal, D.mData[1].mVal);
            ptr3->mData[0] = _mm_unpackhi_pd(A.mData[0].mVal, C.mData[0].mVal);
            ptr3->mData[1] = _mm_unpackhi_pd(B.mData[0].mVal, D.mData[0].mVal);
            ptr4->mData[0] = _mm_unpackhi_pd(A.mData[1].mVal, C.mData[1].mVal);
            ptr4->mData[1] = _mm_unpackhi_pd(B.mData[1].mVal, D.mData[1].mVal);
        }
        
    #endif
        
    #if defined(__AVX__) || defined(__AVX512F__)
        
        // Shuffle for an AVX256 Double Packed (1 SIMD Element)
        
        static void shuffle4(const Vector4x<double, 4> &A,
                             const Vector4x<double, 4> &B,
                             const Vector4x<double, 4> &C,
                             const Vector4x<double, 4> &D,
                             Vector4x<double, 4> *ptr1,
                             Vector4x<double, 4> *ptr2,
                             Vector4x<double, 4> *ptr3,
                             Vector4x<double, 4> *ptr4)
        {
            const __m256d v1 = _mm256_unpacklo_pd(A.mData[0].mVal, B.mData[0].mVal);
            const __m256d v2 = _mm256_unpackhi_pd(A.mData[0].mVal, B.mData[0].mVal);
            const __m256d v3 = _mm256_unpacklo_pd(C.mData[0].mVal, D.mData[0].mVal);
            const __m256d v4 = _mm256_unpackhi_pd(C.mData[0].mVal, D.mData[0].mVal);
            
            const __m256d v5 = _mm256_permute2f128_pd(v1, v2, 0x20);
            const __m256d v6 = _mm256_permute2f128_pd(v1, v2, 0x31);
            const __m256d v7 = _mm256_permute2f128_pd(v3, v4, 0x20);
            const __m256d v8 = _mm256_permute2f128_pd(v3, v4, 0x31);
            
            const __m256d v9 = _mm256_unpacklo_pd(v5, v7);
            const __m256d vA = _mm256_unpackhi_pd(v5, v7);
            const __m256d vB = _mm256_unpacklo_pd(v6, v8);
            const __m256d vC = _mm256_unpackhi_pd(v6, v8);
            
            ptr1->mData[0] = _mm256_permute2f128_pd(v9, vA, 0x20);
            ptr2->mData[0] = _mm256_permute2f128_pd(vB, vC, 0x20);
            ptr3->mData[0] = _mm256_permute2f128_pd(v9, vA, 0x31);
            ptr4->mData[0] = _mm256_permute2f128_pd(vB, vC, 0x31) ;
        }
        
    #endif
        
    #if defined SIMD_COMPILER_SUPPORT_NEON /* Neon Intrinsics */

    #if defined(__arm64) || defined(__aarch64__)
        
        // Shuffle an ARM Double Packed (2 SIMD Elements)
        
        static void shuffle4(const Vector4x<double, 2> &A,
                             const Vector4x<double, 2> &B,
                             const Vector4x<double, 2> &C,
                             const Vector4x<double, 2> &D,
                             Vector4x<double, 2> *ptr1,
                             Vector4x<double, 2> *ptr2,
                             Vector4x<double, 2> *ptr3,
                             Vector4x<double, 2> *ptr4)
        {
            ptr1->mData[0] = vuzp1q_f64(A.mData[0].mVal, C.mData[0].mVal);
            ptr1->mData[1] = vuzp1q_f64(B.mData[0].mVal, D.mData[0].mVal);
            ptr2->mData[0] = vuzp1q_f64(A.mData[1].mVal, C.mData[1].mVal);
            ptr2->mData[1] = vuzp1q_f64(B.mData[1].mVal, D.mData[1].mVal);
            ptr3->mData[0] = vuzp2q_f64(A.mData[0].mVal, C.mData[0].mVal);
            ptr3->mData[1] = vuzp2q_f64(B.mData[0].mVal, D.mData[0].mVal);
            ptr4->mData[0] = vuzp2q_f64(A.mData[1].mVal, C.mData[1].mVal);
            ptr4->mData[1] = vuzp2q_f64(B.mData[1].mVal, D.mData[1].mVal);
        }
        
    #endif

        // Shuffle for an ARM Float Packed (1 SIMD Element)
        
        static void shuffle4(const Vector4x<float, 4> &A,
                             const Vector4x<float, 4> &B,
                             const Vector4x<float, 4> &C,
                             const Vector4x<float, 4> &D,
                             Vector4x<float, 4> *ptr1,
                             Vector4x<float, 4> *ptr2,
                             Vector4x<float, 4> *ptr3,
                             Vector4x<float, 4> *ptr4)
        {
            const float32x4_t v1 = vcombine_f32( vget_low_f32(A.mData[0].mVal),  vget_low_f32(C.mData[0].mVal));
            const float32x4_t v2 = vcombine_f32(vget_high_f32(A.mData[0].mVal), vget_high_f32(C.mData[0].mVal));
            const float32x4_t v3 = vcombine_f32( vget_low_f32(B.mData[0].mVal),  vget_low_f32(D.mData[0].mVal));
            const float32x4_t v4 = vcombine_f32(vget_high_f32(B.mData[0].mVal), vget_high_f32(D.mData[0].mVal));
            
            const float32x4x2_t v5 = vuzpq_f32(v1, v3);
            const float32x4x2_t v6 = vuzpq_f32(v2, v4);
            
            ptr1->mData[0] = v5.val[0];
            ptr2->mData[0] = v6.val[0];
            ptr3->mData[0] = v5.val[1];
            ptr4->mData[0] = v6.val[1];
        }
        
    #endif
    };
    
    // ******************** Templates (Scalar or SIMD) for FFT Passes ******************** //
    
    // Pass One and Two with Re-ordering
    
    template <class T, int vec_size>
    static void pass_1_2_reorder(Split<T> *input, uintptr_t length)
    {
        using VecType = Vector4x<T, vec_size> ;
        
        VecType *r1_ptr = reinterpret_cast<VecType *>(input->realp);
        VecType *r2_ptr = r1_ptr + (length >> 4);
        VecType *r3_ptr = r2_ptr + (length >> 4);
        VecType *r4_ptr = r3_ptr + (length >> 4);
        VecType *i1_ptr = reinterpret_cast<VecType *>(input->imagp);
        VecType *i2_ptr = i1_ptr + (length >> 4);
        VecType *i3_ptr = i2_ptr + (length >> 4);
        VecType *i4_ptr = i3_ptr + (length >> 4);
        
        for (uintptr_t i = 0; i < length >> 4; i++)
        {
            const VecType r1 = *r1_ptr;
            const VecType i1 = *i1_ptr;
            const VecType r2 = *r2_ptr;
            const VecType i2 = *i2_ptr;
            
            const VecType r3 = *r3_ptr;
            const VecType i3 = *i3_ptr;
            const VecType r4 = *r4_ptr;
            const VecType i4 = *i4_ptr;
            
            const VecType r5 = r1 + r3;
            const VecType r6 = r2 + r4;
            const VecType r7 = r1 - r3;
            const VecType r8 = r2 - r4;
            
            const VecType i5 = i1 + i3;
            const VecType i6 = i2 + i4;
            const VecType i7 = i1 - i3;
            const VecType i8 = i2 - i4;
            
            const VecType rA = r5 + r6;
            const VecType rB = r5 - r6;
            const VecType rC = r7 + i8;
            const VecType rD = r7 - i8;
            
            const VecType iA = i5 + i6;
            const VecType iB = i5 - i6;
            const VecType iC = i7 - r8;
            const VecType iD = i7 + r8;
            
            shuffler::shuffle4(rA, rB, rC, rD, r1_ptr++, r2_ptr++, r3_ptr++, r4_ptr++);
            shuffler::shuffle4(iA, iB, iC, iD, i1_ptr++, i2_ptr++, i3_ptr++, i4_ptr++);
        }
    }
    
    // Pass Three Twiddle Factors
    
    template <class T, int vec_size>
    static void pass_3_twiddle(Vector4x<T, vec_size> &tr, Vector4x<T, vec_size> &ti)
    {
        static const double SQRT_2_2 = 0.70710678118654752440084436210484904;
        
        const T _______zero = static_cast<T>(0);
        const T ________one = static_cast<T>(1);
        const T neg_____one = static_cast<T>(-1);
        const T ____sqrt2_2 = static_cast<T>(SQRT_2_2);
        const T neg_sqrt2_2 = static_cast<T>(-SQRT_2_2);
        
        const T str[4] = {________one, ____sqrt2_2, _______zero, neg_sqrt2_2};
        const T sti[4] = {_______zero, neg_sqrt2_2, neg_____one, neg_sqrt2_2};
        
        tr = Vector4x<T, vec_size>(str);
        ti = Vector4x<T, vec_size>(sti);
    }
    
    // Pass Three With Re-ordering
    
    template <class T, int vec_size>
    static void pass_3_reorder(Split<T> *input, uintptr_t length)
    {
        using VecType = Vector4x<T, vec_size>;
        
        uintptr_t offset = length >> 5;
        uintptr_t outerLoop = length >> 6;
        
        VecType tr;
        VecType ti;
        
        pass_3_twiddle(tr, ti);
        
        VecType *r1_ptr = reinterpret_cast<VecType *>(input->realp);
        VecType *i1_ptr = reinterpret_cast<VecType *>(input->imagp);
        VecType *r2_ptr = r1_ptr + offset;
        VecType *i2_ptr = i1_ptr + offset;
        
        for (uintptr_t i = 0, j = 0; i < length >> 1; i += 8)
        {
            // Get input
            
            const VecType r1(r1_ptr);
            const VecType r2(r1_ptr + 1);
            const VecType i1(i1_ptr);
            const VecType i2(i1_ptr + 1);
            
            const VecType r3(r2_ptr);
            const VecType r4(r2_ptr + 1);
            const VecType i3(i2_ptr);
            const VecType i4(i2_ptr + 1);
            
            // Multiply by twiddle
            
            const VecType r5 = (r3 * tr) - (i3 * ti);
            const VecType i5 = (r3 * ti) + (i3 * tr);
            const VecType r6 = (r4 * tr) - (i4 * ti);
            const VecType i6 = (r4 * ti) + (i4 * tr);
            
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
    
    template <class T, int vec_size>
    static void pass_3(Split<T> *input, uintptr_t length)
    {
        using VecType = Vector4x<T, vec_size>;

        VecType tr;
        VecType ti;
        
        pass_3_twiddle(tr, ti);
        
        VecType *r_ptr = reinterpret_cast<VecType *>(input->realp);
        VecType *i_ptr = reinterpret_cast<VecType *>(input->imagp);
        
        for (uintptr_t i = 0; i < length >> 3; i++)
        {
            // Get input
            
            const VecType r1(r_ptr);
            const VecType r2(r_ptr + 1);
            const VecType i1(i_ptr);
            const VecType i2(i_ptr + 1);
            
            // Multiply by twiddle
            
            const VecType r3 = (r2 * tr) - (i2 * ti);
            const VecType i3 = (r2 * ti) + (i2 * tr);
            
            // Store output
            
            *r_ptr++ = r1 + r3;
            *r_ptr++ = r1 - r3;
            *i_ptr++ = i1 + i3;
            *i_ptr++ = i1 - i3;
            
        }
    }
    
    // A Pass Requiring Tables With Re-ordering
    
    template <class T, int vec_size>
    static void pass_trig_table_reorder(Split<T> *input, SetupType<T> *setup, uintptr_t length, uintptr_t pass)
    {
        using VecType = SIMDType<T, vec_size>;

        uintptr_t size = static_cast<uintptr_t>(2u) << pass;
        uintptr_t incr = size / (vec_size << 1);
        uintptr_t loop = size;
        uintptr_t offset = (length >> pass) / (vec_size << 1);
        uintptr_t outerLoop = ((length >> 1) / size) / (static_cast<uintptr_t>(1u) << pass);
        
        VecType *r1_ptr = reinterpret_cast<VecType *>(input->realp);
        VecType *i1_ptr = reinterpret_cast<VecType *>(input->imagp);
        VecType *r2_ptr = r1_ptr + offset;
        VecType *i2_ptr = i1_ptr + offset;
        
        for (uintptr_t i = 0, j = 0; i < (length >> 1); loop += size)
        {
            const VecType *tr_ptr = setup->template get_realp<VecType>(pass);
            const VecType *ti_ptr = setup->template get_imagp<VecType>(pass);
            
            for (; i < loop; i += (vec_size << 1))
            {
                // Get input and twiddle
                
                const VecType tr = *tr_ptr++;
                const VecType ti = *ti_ptr++;
                
                const VecType r1 = *r1_ptr;
                const VecType i1 = *i1_ptr;
                const VecType r2 = *r2_ptr;
                const VecType i2 = *i2_ptr;
                
                const VecType r3 = *(r1_ptr + incr);
                const VecType i3 = *(i1_ptr + incr);
                const VecType r4 = *(r2_ptr + incr);
                const VecType i4 = *(i2_ptr + incr);
                
                // Multiply by twiddle
                
                const VecType r5 = (r2 * tr) - (i2 * ti);
                const VecType i5 = (r2 * ti) + (i2 * tr);
                const VecType r6 = (r4 * tr) - (i4 * ti);
                const VecType i6 = (r4 * ti) + (i4 * tr);
                
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
    
    template <class T, int vec_size>
    static void pass_trig_table(Split<T> *input, SetupType<T> *setup, uintptr_t length, uintptr_t pass)
    {
        using VecType = SIMDType<T, vec_size>;

        uintptr_t size = static_cast<uintptr_t>(2u) << pass;
        uintptr_t incr = size / (vec_size << 1);
        uintptr_t loop = size;
        
        VecType *r1_ptr = reinterpret_cast<VecType *>(input->realp);
        VecType *i1_ptr = reinterpret_cast<VecType *>(input->imagp);
        VecType *r2_ptr = r1_ptr + (size >> 1) / vec_size;
        VecType *i2_ptr = i1_ptr + (size >> 1) / vec_size;
        
        for (uintptr_t i = 0; i < length; loop += size)
        {
            const VecType *tr_ptr = setup->template get_realp<VecType>(pass);
            const VecType *ti_ptr = setup->template get_imagp<VecType>(pass);
            
            for (; i < loop; i += (vec_size << 1))
            {
                // Get input and twiddle factors
                
                const VecType tr = *tr_ptr++;
                const VecType ti = *ti_ptr++;
                
                const VecType r1 = *r1_ptr;
                const VecType i1 = *i1_ptr;
                const VecType r2 = *r2_ptr;
                const VecType i2 = *i2_ptr;
                
                // Multiply by twiddle
                
                const VecType r3 = (r2 * tr) - (i2 * ti);
                const VecType i3 = (r2 * ti) + (i2 * tr);
                
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
    static void pass_real_trig_table(Split<T> *input, SetupType<T> *setup, uintptr_t fft_log2)
    {
        uintptr_t length = static_cast<uintptr_t>(1u) << (fft_log2 - 1u);
        uintptr_t lengthM1 = length - 1;
        
        T *r1_ptr = input->realp;
        T *i1_ptr = input->imagp;
        T *r2_ptr = r1_ptr + lengthM1;
        T *i2_ptr = i1_ptr + lengthM1;
        const T *tr_ptr = setup->template get_realp<T>(fft_log2 - 1);
        const T *ti_ptr = setup->template get_imagp<T>(fft_log2 - 1);
        
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
    static void small_fft(Split<T> *input, uintptr_t fft_log2)
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
            
            pass_3<T, 1>(input, 8);
        }
    }
    
    // Small Real FFTs (2 or 4 points)
    
    template <bool ifft, class T>
    static void small_real_fft(Split<T> *input, uintptr_t fft_log2)
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

    // Unzip
    
    template <class T, int vec_size>
    static void unzip_impl(const T *input, T *real, T *imag, uintptr_t half_length, uintptr_t offset)
    {
        using VecType = SIMDType<T, vec_size>;

        const VecType *in_ptr = reinterpret_cast<const VecType*>(input + (2 * offset));
        
        VecType *realp = reinterpret_cast<VecType*>(real + offset);
        VecType *imagp = reinterpret_cast<VecType*>(imag + offset);
        
        for (uintptr_t i = 0; i < (half_length / vec_size); i++, in_ptr += 2)
            unzip(*realp++, *imagp++, *in_ptr, *(in_ptr + 1));
    }
    
    template <class T, class U>
    static void unzip_complex(const U *input, Split<T> *output, uintptr_t half_length)
    {
        T *realp = output->realp;
        T *imagp = output->imagp;
        
        for (uintptr_t i = 0; i < half_length; i++)
        {
            *realp++ = static_cast<T>(*input++);
            *imagp++ = static_cast<T>(*input++);
        }
    }
    
    template <class T>
    static void unzip_complex(const T *input, Split<T> *output, uintptr_t half_length)
    {
        constexpr int v_size = SIMDLimits<T>::max_size;
        
        if (is_aligned(input) && is_aligned(output->realp) && is_aligned(output->imagp))
        {
            uintptr_t v_length = (half_length / v_size) * v_size;
            unzip_impl<T, v_size>(input, output->realp, output->imagp, v_length, 0);
            unzip_impl<T, 1>(input, output->realp, output->imagp, half_length - v_length, v_length);
        }
        else
            unzip_impl<T, 1>(input, output->realp, output->imagp, half_length, 0);
    }
    
    // Zip
    
    template <class T, int vec_size>
    static void zip_impl(const T *real, const T *imag, T *output, uintptr_t half_length, uintptr_t offset)
    {
        using VecType = SIMDType<T, vec_size>;

        const VecType *realp = reinterpret_cast<const VecType*>(real + offset);
        const VecType *imagp = reinterpret_cast<const VecType*>(imag + offset);
        
        VecType *out_ptr = reinterpret_cast<VecType*>(output + (2 * offset));
        
        for (uintptr_t i = 0; i < (half_length / vec_size); i++, out_ptr += 2)
            zip(*out_ptr, *(out_ptr + 1), *realp++, *imagp++);
    }
    
    template <class T>
    static void zip_complex(const Split<T> *input, T *output, uintptr_t half_length)
    {
        constexpr int v_size = SIMDLimits<T>::max_size;
        
        if (is_aligned(output) && is_aligned(input->realp) && is_aligned(input->imagp))
        {
            uintptr_t v_length = (half_length / v_size) * v_size;
            zip_impl<T, v_size>(input->realp, input->imagp, output, v_length, 0);
            zip_impl<T, 1>(input->realp, input->imagp, output, half_length - v_length, v_length);
        }
        else
            zip_impl<T, 1>(input->realp, input->imagp, output, half_length, 0);
    }
    
    // Unzip With Zero Padding
    
    template <class T, class U>
    static void unzip_zero(const U *input, Split<T> *output, uintptr_t in_length, uintptr_t log2n)
    {
        T odd_sample = static_cast<T>(input[in_length - 1]);
        T *realp = output->realp;
        T *imagp = output->imagp;
        
        // Check input length is not longer than the FFT size and unzip an even number of samples
        
        uintptr_t fft_size = static_cast<uintptr_t>(1u) << log2n;
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
    
    // Utility Helper
    
    static constexpr int min(int a, int b) { return a < b ? a : b; }
    
    // FFT Passes Template
    
    template <class T, int max_vec_size>
    static void fft_passes(Split<T> *input, SetupType<T> *setup, uintptr_t fft_log2)
    {
        constexpr int A = min(max_vec_size,  4);
        constexpr int B = min(max_vec_size,  8);
        constexpr int C = min(max_vec_size, 16);
        const uintptr_t length = static_cast<uintptr_t>(1u) << fft_log2;
        uintptr_t i;
        
        pass_1_2_reorder<T, A>(input, length);
        
        if (fft_log2 > 5)
            pass_3_reorder<T, A>(input, length);
        else
            pass_3<T, A>(input, length);
        
        if (3 < (fft_log2 >> 1))
            pass_trig_table_reorder<T, B>(input, setup, length, 3);
        else
            pass_trig_table<T, B>(input, setup, length, 3);
        
        for (i = 4; i < (fft_log2 >> 1); i++)
            pass_trig_table_reorder<T, C>(input, setup, length, i);
        
        for (; i < fft_log2; i++)
            pass_trig_table<T, C>(input, setup, length, i);
    }
    
    // ******************** Main Calls ******************** //
    
    // A Complex FFT
    
    template <class T>
    static void hisstools_fft(Split<T> *input, SetupType<T> *setup, uintptr_t fft_log2)
    {
        if (fft_log2 >= 4)
        {
            if (!is_aligned(input->realp) || !is_aligned(input->imagp))
                fft_passes<T, 1>(input, setup, fft_log2);
            else
                fft_passes<T, SIMDLimits<T>::max_size>(input, setup, fft_log2);
        }
        else
            small_fft(input, fft_log2);
    }
    
    // A Complex iFFT
    
    template <class T>
    static void hisstools_ifft(Split<T> *input, SetupType<T> *setup, uintptr_t fft_log2)
    {
        Split<T> swap(input->imagp, input->realp);
        hisstools_fft(&swap, setup, fft_log2);
    }
    
    // A Real FFT
    
    template <class T>
    static void hisstools_rfft(Split<T> *input, SetupType<T> *setup, uintptr_t fft_log2)
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
    static void hisstools_rifft(Split<T> *input, SetupType<T> *setup, uintptr_t fft_log2)
    {
        if (fft_log2 >= 3)
        {
            pass_real_trig_table<true>(input, setup, fft_log2);
            hisstools_ifft(input, setup, fft_log2 - 1);
        }
        else
            small_real_fft<true>(input, fft_log2);
    }
    
    // ******************** Apple Explicit Overloads ******************** //

#if defined (USE_APPLE_FFT)
    
    // Setup Create / Destroy
    
    static void create_setup(FFTSetupD& setup, uintptr_t max_fft_log_2)
    {
        setup = vDSP_create_fftsetupD(max_fft_log_2, FFT_RADIX2);
    }
    
    static void create_setup(FFTSetup& setup, uintptr_t max_fft_log_2)
    {
        setup = vDSP_create_fftsetup(max_fft_log_2, FFT_RADIX2);
    }
    
    static void destroy_setup(FFTSetupD setup)
    {
        if (setup)
            vDSP_destroy_fftsetupD(setup);
    }
    
    static void destroy_setup(FFTSetup setup)
    {
        if (setup)
            vDSP_destroy_fftsetup(setup);
    }
    
    // FFT and iFFT Routines
    
    static void hisstools_fft(Split<double> *input, Setup<double> setup, uintptr_t log2n)
    {
        vDSP_fft_zipD(setup, input, 1, log2n, FFT_FORWARD);
    }
    
    static void hisstools_fft(Split<float> *input, Setup<float> setup, uintptr_t log2n)
    {
        vDSP_fft_zip(setup, input, 1, log2n, FFT_FORWARD);
    }
    
    static void hisstools_rfft(Split<double> *input, Setup<double> setup, uintptr_t log2n)
    {
        vDSP_fft_zripD(setup, input, 1, log2n, FFT_FORWARD);
    }
    
    static void hisstools_rfft(Split<float> *input, Setup<float> setup, uintptr_t log2n)
    {
        vDSP_fft_zrip(setup, input, 1, log2n, FFT_FORWARD);
    }
    
    static void hisstools_ifft(Split<double> *input, Setup<double> setup, uintptr_t log2n)
    {
        vDSP_fft_zipD(setup, input, 1, log2n, FFT_INVERSE);
    }
    
    static void hisstools_ifft(Split<float> *input, Setup<float> setup, uintptr_t log2n)
    {
        vDSP_fft_zip(setup, input, 1, log2n, FFT_INVERSE);
    }
    
    static void hisstools_rifft(Split<double> *input, Setup<double> setup, uintptr_t log2n)
    {
        vDSP_fft_zripD(setup, input, 1, log2n, FFT_INVERSE);
    }
    
    static void hisstools_rifft(Split<float> *input, Setup<float> setup, uintptr_t log2n)
    {
        vDSP_fft_zrip(setup, input, 1, log2n, FFT_INVERSE);
    }
    
    // Zip and Unzip
    
    static void zip_complex(const Split<double> *input, double *output, uintptr_t half_length)
    {
        vDSP_ztocD(input, 1, reinterpret_cast<DOUBLE_COMPLEX *>(output), 2, half_length);
    }
    
    static void zip_complex(const Split<float> *input, float *output, uintptr_t half_length)
    {
        vDSP_ztoc(input, 1, reinterpret_cast<COMPLEX *>(output), 2, half_length);
    }
    
    static void unzip_complex(const double *input, Split<double> *output, uintptr_t half_length)
    {
        vDSP_ctozD(reinterpret_cast<const DOUBLE_COMPLEX *>(input), 2, output, 1, half_length);
    }
    
    static void unzip_complex(const float *input, Split<float> *output, uintptr_t half_length)
    {
        vDSP_ctoz(reinterpret_cast<const COMPLEX *>(input), 2, output, 1, half_length);
    }
    
#endif
    
};

#if !defined (USE_APPLE_FFT)

// Setup definition when using the HISSTools codepath

template <class T>
struct Setup : impl::SetupBase<T, hisstools_fft_impl::SetupType<T> *>
{
    Setup() {}
    Setup(hisstools_fft_impl::SetupType<T> *setup)
    : impl::SetupBase<T, hisstools_fft_impl::SetupType<T> *>(setup)
    {}
};

#endif

#endif /* HISSTOOLS_FFT_CORE_HPP */
