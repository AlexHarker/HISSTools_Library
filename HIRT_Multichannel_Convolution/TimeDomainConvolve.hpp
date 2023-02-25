
#pragma once

#include "ConvolveErrors.h"
#include "../SIMDSupport.hpp"

#include <cstdint>
#include <algorithm>

#ifdef __APPLE__
#include <Accelerate/Accelerate.h>
#endif

class TimeDomainConvolve
{
public:
    
    TimeDomainConvolve(uintptr_t offset, uintptr_t length)
    {
        // Set default initial variables
        
        set_offset(offset);
        set_length(length);
        
        // Allocate impulse buffer and input bufferr
        
        m_impulse_buffer = allocate_aligned<float>(2048);
        m_input_buffer = allocate_aligned<float>(8192);
        
        // Zero buffers
        
        std::fill_n(m_impulse_buffer, 2048, 0.f);
        std::fill_n(m_input_buffer, 8192, 0.f);
    }
    
    ~TimeDomainConvolve()
    {
        deallocate_aligned(m_impulse_buffer);
        deallocate_aligned(m_input_buffer);
    }
    
    // Non-moveable and copyable
    
    TimeDomainConvolve(TimeDomainConvolve& obj) = delete;
    TimeDomainConvolve& operator = (TimeDomainConvolve& obj) = delete;
    TimeDomainConvolve(TimeDomainConvolve&& obj) = delete;
    TimeDomainConvolve& operator = (TimeDomainConvolve&& obj) = delete;
    
    ConvolveError set_length(uintptr_t length)
    {
        m_length = std::min(length, uintptr_t(2044));
        
        return length > 2044 ? CONVOLVE_ERR_TIME_LENGTH_OUT_OF_RANGE : CONVOLVE_ERR_NONE;
    }
    
    void set_offset(uintptr_t offset)
    {
        m_offset = offset;
    }
    
    template <class T>
    ConvolveError set(const T *input, uintptr_t length)
    {
        TypeConformedInput<float, T> typed_input(input, length);

        m_impulse_length = 0;
        
        if (input && length > m_offset)
        {
            // Calculate impulse length
            
            m_impulse_length = std::min(length - m_offset, (m_length ? m_length : 2044));
            
            uintptr_t pad = padded_length(m_impulse_length) - m_impulse_length;
            std::fill_n(m_impulse_buffer, pad, 0.f);
            std::reverse_copy(typed_input.get() + m_offset, typed_input.get() + m_offset + m_impulse_length, m_impulse_buffer + pad);
        }
        
        reset();
        
        return (!m_length && (length - m_offset) > 2044) ? CONVOLVE_ERR_TIME_IMPULSE_TOO_LONG : CONVOLVE_ERR_NONE;
    }
    
    void reset()
    {
        m_reset = true;
    }
    
    bool process(const float *in, float *out, uintptr_t num_samples)
    {
        if (m_reset)
        {
            std::fill_n(m_input_buffer, 8192, 0.f);
            m_reset = false;
        }
        
        uintptr_t current_loop;
        
        while ((current_loop = (m_input_position + num_samples) > 4096 ? (4096 - m_input_position) : ((num_samples > 2048) ? 2048 : num_samples)))
        {
            // Copy input twice (allows us to read input out in one go)
            
            std::copy_n(in, current_loop, m_input_buffer + m_input_position);
            std::copy_n(in, current_loop, m_input_buffer + m_input_position + 4096);
            
            // Advance pointer
            
            m_input_position += current_loop;
            if (m_input_position >= 4096)
                m_input_position -= 4096;
            
            // Do convolution
            
            convolve(m_input_buffer + 4096 + (m_input_position - current_loop), m_impulse_buffer, out, current_loop, m_impulse_length);
            
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
    
    static void convolve(const float *in, const float *impulse, float *output, uintptr_t N, uintptr_t L)
    {
        using Vec = SIMDType<float, 4>;
        constexpr int size = Vec::size;
        
        L = padded_length(L);
        
        const Vec *impulse_vector = reinterpret_cast<const Vec *>(impulse);
        
        for (uintptr_t i = 0; i < N; i++)
        {
            Vec accum(0.f);
            
            const float *input = in - L + 1 + i - size;
            
            for (uintptr_t j = 0; j < L >> 2; j += 4)
            {
                // Load vals
                
                accum += (impulse_vector[j + 0] * Vec(input += size));
                accum += (impulse_vector[j + 1] * Vec(input += size));
                accum += (impulse_vector[j + 2] * Vec(input += size));
                accum += (impulse_vector[j + 3] * Vec(input += size));
            }
            
            *output++ = sum(accum);
        }
    }
#endif
    
    // Internal buffers
    
    float *m_impulse_buffer;
    float *m_input_buffer;
    
    uintptr_t m_input_position;
    uintptr_t m_impulse_length;
    
    uintptr_t m_offset;
    uintptr_t m_length;
    
    // Flags
    
    bool m_reset;
};
