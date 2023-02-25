
#pragma once
	
#include "MonoConvolve.hpp"
#include "ConvolveErrors.h"

#include <algorithm>
#include <cstdint>
#include <vector>

class NToMonoConvolve
{
public:
    
    NToMonoConvolve(uint32_t in_chans, uintptr_t max_length, LatencyMode latency)
    : m_num_in_chans(in_chans)
    {
        for (uint32_t i = 0; i < m_num_in_chans; i++)
            m_convolvers.emplace_back(max_length, latency);
    }
    
    // Resize / set / reset
    
    ConvolveError resize(uint32_t in_chan, uintptr_t length)
    {
        return do_channel(&MonoConvolve::resize, in_chan, length);
    }
    
    template <class T>
    ConvolveError set(uint32_t in_chan, const T *input, uintptr_t length, bool resize)
    {
        TypeConformedInput<float, T> typed_input(input, length);

        return do_channel(&MonoConvolve::set<T>, in_chan, typed_input.get(), length, resize);
    }

    ConvolveError reset(uint32_t in_chan)
    {
        return do_channel(&MonoConvolve::reset, in_chan);
    }
    
    // Process
    
    void process(const float * const* ins, float *out, float *temp, size_t num_samples, size_t active_in_chans)
    {
        // Zero output then convolve
        
        std::fill_n(out, num_samples, 0.f);
        
        for (uint32_t i = 0; i < m_num_in_chans && i < active_in_chans ; i++)
            m_convolvers[i].process(ins[i], temp, out, num_samples, true);
    }
    
private:
    
    // Utility to apply an operation to a channel
    
    template<typename Method, typename... Args>
    ConvolveError do_channel(Method method, uint32_t in_chan, Args...args)
    {
        if (in_chan < m_num_in_chans)
            return (m_convolvers[in_chan].*method)(args...);
        else
            return CONVOLVE_ERR_IN_CHAN_OUT_OF_RANGE;
    }
    
    std::vector<MonoConvolve> m_convolvers;
    
    uint32_t m_num_in_chans;
};
