
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
        
        setOffset(offset);
        setLength(length);
        
        // Allocate impulse buffer and input bufferr
        
        mImpulseBuffer = allocate_aligned<float>(2048);
        mInputBuffer = allocate_aligned<float>(8192);
        
        // Zero buffers
        
        std::fill_n(mImpulseBuffer, 2048, 0.f);
        std::fill_n(mInputBuffer, 8192, 0.f);
    }
    
    ~TimeDomainConvolve()
    {
        deallocate_aligned(mImpulseBuffer);
        deallocate_aligned(mInputBuffer);
    }

    
    // Non-moveable and copyable
    
    TimeDomainConvolve(TimeDomainConvolve& obj) = delete;
    TimeDomainConvolve& operator = (TimeDomainConvolve& obj) = delete;
    TimeDomainConvolve(TimeDomainConvolve&& obj) = delete;
    TimeDomainConvolve& operator = (TimeDomainConvolve&& obj) = delete;
    
    ConvolveError setLength(uintptr_t length)
    {
        mLength = std::min(length, uintptr_t(2044));
        
        return length > 2044 ? CONVOLVE_ERR_TIME_LENGTH_OUT_OF_RANGE : CONVOLVE_ERR_NONE;
    }
    
    void setOffset(uintptr_t offset)
    {
        mOffset = offset;
    }
    
    ConvolveError set(const float *input, uintptr_t length)
    {
        mImpulseLength = 0;
        
        if (input && length > mOffset)
        {
            // Calculate impulse length
            
            mImpulseLength = std::min(length - mOffset, (mLength ? mLength : 2044));
            
            uintptr_t pad = padded_length(mImpulseLength) - mImpulseLength;
            std::fill_n(mImpulseBuffer, pad, 0.f);
            std::reverse_copy(input + mOffset, input + mOffset + mImpulseLength, mImpulseBuffer + pad);
        }
        
        reset();
        
        return (!mLength && (length - mOffset) > 2044) ? CONVOLVE_ERR_TIME_IMPULSE_TOO_LONG : CONVOLVE_ERR_NONE;
    }
    
    void reset()
    {
        mReset = true;
    }
    
    bool process(const float *in, float *out, uintptr_t numSamples)
    {
        if (mReset)
        {
            std::fill_n(mInputBuffer, 8192, 0.f);
            mReset = false;
        }
        
        uintptr_t currentLoop;
        
        while ((currentLoop = (mInputPosition + numSamples) > 4096 ? (4096 - mInputPosition) : ((numSamples > 2048) ? 2048 : numSamples)))
        {
            // Copy input twice (allows us to read input out in one go)
            
            std::copy_n(in, currentLoop, mInputBuffer + mInputPosition);
            std::copy_n(in, currentLoop, mInputBuffer + mInputPosition + 4096);
            
            // Advance pointer
            
            mInputPosition += currentLoop;
            if (mInputPosition >= 4096)
                mInputPosition -= 4096;
            
            // Do convolution
            
            convolve(mInputBuffer + 4096 + (mInputPosition - currentLoop), mImpulseBuffer, out, currentLoop, mImpulseLength);
            
            // Updates
            
            in += currentLoop;
            out += currentLoop;
            numSamples -= currentLoop;
        }
        
        return mImpulseLength;
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
    
    float *mImpulseBuffer;
    float *mInputBuffer;
    
    uintptr_t mInputPosition;
    uintptr_t mImpulseLength;
    
    uintptr_t mOffset;
    uintptr_t mLength;
    
    // Flags
    
    bool mReset;
};
