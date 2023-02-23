
#pragma once
	
#include "MonoConvolve.hpp"
#include "ConvolveErrors.h"

#include <algorithm>
#include <cstdint>
#include <vector>

class NToMonoConvolve
{
    
public:
    
    NToMonoConvolve(uint32_t inChans, uintptr_t maxLength, LatencyMode latency)
    : mNumInChans(inChans)
    {
        for (uint32_t i = 0; i < mNumInChans; i++)
            mConvolvers.emplace_back(maxLength, latency);
    }
    
    ConvolveError resize(uint32_t inChan, uintptr_t impulse_length)
    {
        return doChannel(&MonoConvolve::resize, inChan, impulse_length);
    }
    
    ConvolveError set(uint32_t inChan, const float *input, uintptr_t impulse_length, bool resize)
    {
        return doChannel(&MonoConvolve::set, inChan, input, impulse_length, resize);
    }

    ConvolveError reset(uint32_t inChan);
    
    void process(const float * const* ins, float *out, float *temp, size_t numSamples, size_t activeInChans)
    {
        // Zero output then convolve
        
        std::fill_n(out, numSamples, 0.f);
        
        for (uint32_t i = 0; i < mNumInChans && i < activeInChans ; i++)
            mConvolvers[i].process(ins[i], temp, out, numSamples, true);
    }
    
private:
    
    template<typename Method, typename... Args>
    ConvolveError doChannel(Method method, uint32_t inChan, Args...args)
    {
        if (inChan < mNumInChans)
            return (mConvolvers[inChan].*method)(args...);
        else
            return CONVOLVE_ERR_IN_CHAN_OUT_OF_RANGE;
    }
    
    std::vector<MonoConvolve> mConvolvers;
    
    uint32_t mNumInChans;
};
