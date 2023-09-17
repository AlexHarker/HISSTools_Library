
#ifndef HISSTOOLS_LIBRARY_CONVOLUTION_TIME_DOMAIN_HPP
#define HISSTOOLS_LIBRARY_CONVOLUTION_TIME_DOMAIN_HPP

#include <array>
#include <cstdint>
#include <algorithm>

#include "../simd_support.hpp"
#include "../namespace.hpp"
#include "utilities.hpp"

#if defined __APPLE__ && !defined NO_APPLE_ACCELERATE
#include <Accelerate/Accelerate.h>
#endif

HISSTOOLS_LIBRARY_NAMESPACE_START()

template <class T, class IO = T>
class convolve_time_domain
{
    using vector_type = simd_type<T, simd_limits<T>::max_size>;

    static constexpr int loop_unroll_size = 4;
    static constexpr int vec_size_shift = impl::ilog2<vector_type::size>();
    static constexpr int padding_resolution = loop_unroll_size * vector_type::size;
    static constexpr int padding_shift = impl::ilog2<padding_resolution>();
    static constexpr int max_impulse_length = 2048;
    static constexpr int max_buffer_length = 4096;
    static constexpr int allocation_length = max_buffer_length * 2;

public:
    
    convolve_time_domain(uintptr_t offset = 0, uintptr_t length = 0)
    : m_input_position(0)
    , m_impulse_length(0)
    {
        // Set default initial variables
        
        set_offset(offset);
        set_length(length);
        
        // Allocate impulse buffer and input buffer
        
        m_impulse_buffer = allocate_aligned<T>(max_impulse_length);
        m_input_buffer = allocate_aligned<T>(allocation_length);
        
        // Zero buffers
        
        std::fill_n(m_impulse_buffer, max_impulse_length, T(0));
        std::fill_n(m_input_buffer, allocation_length, T(0));
    }
    
    ~convolve_time_domain()
    {
        deallocate_aligned(m_impulse_buffer);
        deallocate_aligned(m_input_buffer);
    }
    
    // Non-moveable and copyable
    
    convolve_time_domain(convolve_time_domain& obj) = delete;
    convolve_time_domain& operator = (convolve_time_domain& obj) = delete;
    convolve_time_domain(convolve_time_domain&& obj) = delete;
    convolve_time_domain& operator = (convolve_time_domain&& obj) = delete;
    
    convolve_error set_length(uintptr_t length)
    {
        m_length = std::min(length, uintptr_t(max_impulse_length));
        
        return length > max_impulse_length ? convolve_error::time_length_outside_range : convolve_error::none;
    }
    
    void set_offset(uintptr_t offset)
    {
        m_offset = offset;
    }
    
    template <class U>
    convolve_error set(const U* input, uintptr_t length)
    {
        conformed_input<T, U> typed_input(input, length);

        m_impulse_length = 0;
        uintptr_t new_length = 0;
        
        if (input && length > m_offset)
        {
            // Calculate impulse length and pad
            
            new_length = std::min(length - m_offset, (m_length ? m_length : max_impulse_length));
            uintptr_t pad = padded_length(new_length) - new_length;

            const T* offset_input = typed_input.get() + m_offset;
                        
            std::fill_n(m_impulse_buffer, pad, T(0));
            std::reverse_copy(offset_input, offset_input + new_length, m_impulse_buffer + pad);
        }
        
        reset();
        m_impulse_length = new_length;
        
        return (!m_length && (length - m_offset) > max_impulse_length) ? convolve_error::time_impulse_too_long 
                                                                       : convolve_error::none;
    }
    
    void reset()
    {
        m_reset = true;
    }
    
    void process(const IO* in, IO* out, uintptr_t num_samples, bool accumulate = false)
    {
        auto loop_size = [&]()
        {
            if ((m_input_position + num_samples) > max_buffer_length)
                return max_buffer_length - m_input_position;
            
            return std::min(uintptr_t(max_impulse_length), num_samples);
        };
        
        // Reset
        
        if (m_reset)
        {
            std::fill_n(m_input_buffer, allocation_length , T(0));
            m_reset = false;
        }
             
        // Early exit if we are not accumulating and there's no impulse
        
        if (!m_impulse_length && !accumulate)
        {
            std::fill_n(out, num_samples, IO(0));
            return;
        }
        
        // Main loop
        
        while (uintptr_t current_loop = loop_size())
        {
            // Copy input twice (allows us to read input out in one go)
            
            impl::copy_cast_n(in, current_loop, m_input_buffer + m_input_position);
            impl::copy_cast_n(in, current_loop, m_input_buffer + m_input_position + max_buffer_length);
            
            // Advance pointer
            
            m_input_position += current_loop;
            if (m_input_position >= max_buffer_length)
                m_input_position -= max_buffer_length;
            
            // Do convolution
            
            T * input_pointer = m_input_buffer + max_buffer_length + (m_input_position - current_loop);
            convolve(input_pointer, m_impulse_buffer, out, current_loop, m_impulse_length, accumulate);
            
            // Updates
            
            in += current_loop;
            out += current_loop;
            num_samples -= current_loop;
        }
    }
    
private:
    
#if defined __APPLE__ && !defined NO_APPLE_ACCELERATE
    static void convolve(const float* in, const float* impulse, float* output, uintptr_t N, uintptr_t L)
    {
        vDSP_conv(in + 1 - L,  1, impulse, 1, output, 1, N, L);
    }
    
    static void convolve(const double* in, const double* impulse, double* output, uintptr_t N, uintptr_t L)
    {
        vDSP_convD(in + 1 - L,  1, impulse, 1, output, 1, N, L);
    }
    
    template <void Func(IO&, IO), class U>
    static void convolve(const T* in, const T* impulse, U* output, uintptr_t N, uintptr_t L)
    {
        T temp[N];
        
        convolve(in, impulse, temp, N, L);
        impl::loop_cast_n<IO, Func>(temp, N, output);
    }
    
    template <>
    static void convolve<impl::copy_to_result<IO>, T>(const T* in, const T* impulse, T* output, uintptr_t N, 
                                                                                                uintptr_t L)
    {
        convolve(in, impulse, output, N, L);
    }
#endif
  
    // Struct to deal with loop unrolling (across multiplications / samples / storage)
    
    template <int N, int M = 0>
    struct loop_unroll
    {
        using recurse = loop_unroll<N - 1, M>;
        
        inline void multiply(vector_type* accum, const T* input, const vector_type& impulse)
        {
            *accum += vector_type(input) * impulse;
            recurse().multiply(++accum, ++input, impulse);
        }
        
        inline void unroll(vector_type* accum, const T*& input, const vector_type* impulse, uintptr_t idx)
        {
            loop_unroll<M, M>().multiply(accum, input, impulse[idx]);
            recurse().unroll(accum, input += vector_type::size, impulse, ++idx);
        }
        
        template <void Func(IO&, IO)>
        inline void store(IO*& output, vector_type* accum)
        {
            Func(*output, static_cast<IO>(sum(*accum)));
            recurse().template store<Func>(++output, ++accum);
        }
    };
    
    template <int M>
    struct loop_unroll<0, M>
    {
        template <typename...Args>
        void multiply(Args...) {}
        
        template <typename...Args>
        void unroll(Args...) {}
        
        template <void Func(IO&, IO), typename...Args>
        void store(Args...) {}
    };

    // Convolution functions
    
    template <void Func(IO&, IO), int UR>
    static void convolve_unrolled(const T* in,
                                       const vector_type* impulse,
                                       IO* output,
                                       uintptr_t& idx,
                                       uintptr_t N,
                                       uintptr_t L)
    {
        for (; (idx + (UR - 1)) < N; idx += UR)
        {
            std::array<vector_type, UR> accum;
            accum.fill(T(0));
            
            const T* input = in - L + 1 + idx;
                        
            for (uintptr_t j = 0; j < (L >> vec_size_shift); j += loop_unroll_size)
                loop_unroll<loop_unroll_size, UR>().unroll(accum.data(), input, impulse, j);
            
            loop_unroll<UR>().template store<Func>(output, accum.data());
        }
    }
    
    // U and V will always evaluate to T and IO, but templating solves clashes with more explicit apple versions above
    
    template <void Func(IO&, IO), class U, class V>
    static void convolve(const U* in, const U* impulse, V* output, uintptr_t N, uintptr_t L)
    {
        const vector_type* v_impulse = reinterpret_cast<const vector_type*>(impulse);

        uintptr_t idx = 0;
                       
        // Unroll against samples for efficiency (by 4 then cleanup by 2 and 1)
        
        convolve_unrolled<Func, 4>(in, v_impulse, output, idx, N, L);
        convolve_unrolled<Func, 2>(in, v_impulse, output, idx, N, L);
        convolve_unrolled<Func, 1>(in, v_impulse, output, idx, N, L);
    }
    
    static void convolve(const T* in, const T* impulse, IO* output, uintptr_t N, uintptr_t L, bool accumulate)
    {
        L = padded_length(L);

        if (accumulate)
            convolve<impl::add_to_result<IO>>(in, impulse, output, N, L);
        else
            convolve<impl::copy_to_result<IO>>(in, impulse, output, N, L);
    }
    
    static uintptr_t padded_length(uintptr_t length)
    {
        return ((length + (padding_resolution - 1)) >> padding_shift) << padding_shift;
    }
    
    // Internal buffers
    
    T* m_impulse_buffer;
    T* m_input_buffer;
    
    uintptr_t m_input_position;
    uintptr_t m_impulse_length;
    
    uintptr_t m_offset;
    uintptr_t m_length;
    
    // Flags
    
    bool m_reset;
};

HISSTOOLS_LIBRARY_NAMESPACE_END()

#endif /* HISSTOOLS_LIBRARY_CONVOLUTION_TIME_DOMAIN_HPP */
