
#pragma once

#include "ConvolveUtilities.hpp"
#include "../SIMDSupport.hpp"

#include <cstdint>
#include <algorithm>

#ifdef __APPLE__
#include <Accelerate/Accelerate.h>
#endif

template <class T>
class convolve_time_domain
{
    static constexpr int vec_size = SIMDLimits<T>::max_size;

public:
    
    convolve_time_domain(uintptr_t offset, uintptr_t length)
    : m_input_position(0)
    , m_impulse_length(0)
    {
        // Set default initial variables
        
        set_offset(offset);
        set_length(length);
        
        // Allocate impulse buffer and input bufferr
        
        m_impulse_buffer = allocate_aligned<T>(2048);
        m_input_buffer = allocate_aligned<T>(8192);
        
        // Zero buffers
        
        std::fill_n(m_impulse_buffer, 2048, T(0));
        std::fill_n(m_input_buffer, 8192, T(0));
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
    
    ConvolveError set_length(uintptr_t length)
    {
        m_length = std::min(length, uintptr_t(2044));
        
        return length > 2044 ? CONVOLVE_ERR_TIME_LENGTH_OUT_OF_RANGE : CONVOLVE_ERR_NONE;
    }
    
    void set_offset(uintptr_t offset)
    {
        m_offset = offset;
    }
    
    template <class U>
    ConvolveError set(const U *input, uintptr_t length)
    {
        conformed_input<T, U> typed_input(input, length);

        m_impulse_length = 0;
        
        if (input && length > m_offset)
        {
            // Calculate impulse length and pad
            
            m_impulse_length = std::min(length - m_offset, (m_length ? m_length : 2044));
            uintptr_t pad = padded_length(m_impulse_length) - m_impulse_length;

            const T *offset_input = typed_input.get() + m_offset;
                        
            std::fill_n(m_impulse_buffer, pad, T(0));
            std::reverse_copy(offset_input, offset_input + m_impulse_length, m_impulse_buffer + pad);
        }
        
        reset();
        
        return (!m_length && (length - m_offset) > 2044) ? CONVOLVE_ERR_TIME_IMPULSE_TOO_LONG : CONVOLVE_ERR_NONE;
    }
    
    void reset()
    {
        m_reset = true;
    }
    
    bool process(const T *in, T *out, uintptr_t num_samples, bool accumulate = false)
    {
        auto loop_size = [&]()
        {
            if ((m_input_position + num_samples) > 4096)
                return 4096 - m_input_position;
            
            return std::min(uintptr_t(2048), num_samples);
        };
        
        if (m_reset)
        {
            std::fill_n(m_input_buffer, 8192, T(0));
            m_reset = false;
        }
                
        while (uintptr_t current_loop = loop_size())
        {
            // Copy input twice (allows us to read input out in one go)
            
            std::copy_n(in, current_loop, m_input_buffer + m_input_position);
            std::copy_n(in, current_loop, m_input_buffer + m_input_position + 4096);
            
            // Advance pointer
            
            m_input_position += current_loop;
            if (m_input_position >= 4096)
                m_input_position -= 4096;
            
            // Do convolution
            
            T * input_pointer = m_input_buffer + 4096 + (m_input_position - current_loop);
            convolve(input_pointer, m_impulse_buffer, out, current_loop, m_impulse_length, accumulate);
            
            // Updates
            
            in += current_loop;
            out += current_loop;
            num_samples -= current_loop;
        }
        
        return m_impulse_length;
    }
    
private:
    
#ifdef __APPLE__
    static uintptr_t padded_length(uintptr_t length)
    {
        return length;
    }
    
    static void convolve(const float *in, const float *impulse, float *output, uintptr_t N, uintptr_t L)
    {
        vDSP_conv(in + 1 - L,  1, impulse, 1, output, 1, N, L);
    }
    
    static void convolve(const double *in, const double *impulse, double *output, uintptr_t N, uintptr_t L)
    {
        vDSP_convD(in + 1 - L,  1, impulse, 1, output, 1, N, L);
    }
    
    static void convolve(const T *in, const T *impulse, T *output, uintptr_t N, uintptr_t L, bool accumulate)
    {
        if (accumulate)
        {
            T temp[N];
            
            convolve(in, impulse, temp, N, L);
            
            for (uintptr_t i = 0; i < N; i++)
                output[i] += temp[i];
        }
        else
            convolve(in, impulse, output, N, L);
    }
#else
    static uintptr_t padded_length(uintptr_t length)
    {
        return ((length + 15) >> 4) << 4;
    }
    
    static float sum(const SIMDType<float, 4>& vec)
    {
        float values[4];
        vec.store(values);
        return values[0] + values[1] + values[2] + values[3];
    }
    
    static void convolve(const T *in, const T *impulse, T *output, uintptr_t N, uintptr_t L)
    {
        using Vec = SIMDType<T, vec_size>;
        
        L = padded_length(L);
        
        const Vec *v_impulse = reinterpret_cast<const Vec *>(impulse);
        
        for (uintptr_t i = 0; i < N; i++)
        {
            Vec accum(T(0));
            
            const T *input = in - L + 1 + i - vec_size;
            
            for (uintptr_t j = 0; j < L >> 2; j += 4)
            {
                // Load vals
                
                accum += (v_impulse[j + 0] * Vec(input += vec_size));
                accum += (v_impulse[j + 1] * Vec(input += vec_size));
                accum += (v_impulse[j + 2] * Vec(input += vec_size));
                accum += (v_impulse[j + 3] * Vec(input += vec_size));
            }
            
            *output++ = sum(accum);
        }
    }
#endif
    
    // Internal buffers
    
    T *m_impulse_buffer;
    T *m_input_buffer;
    
    uintptr_t m_input_position;
    uintptr_t m_impulse_length;
    
    uintptr_t m_offset;
    uintptr_t m_length;
    
    // Flags
    
    bool m_reset;
};
