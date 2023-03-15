
#pragma once
	
#include "ConvolveUtilities.hpp"
#include "ConvolveMono.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

template <class T>
class convolve_n_to_mono
{
public:
    
    convolve_n_to_mono(uint32_t in_chans, uintptr_t max_length, LatencyMode latency)
    : m_num_in_chans(in_chans)
    {
        for (uint32_t i = 0; i < m_num_in_chans; i++)
            m_convolvers.emplace_back(max_length, latency);
    }
    
    // Resize / set / reset
    
    ConvolveError resize(uint32_t in_chan, uintptr_t length)
    {
        return do_channel(&convolve_mono<T>::resize, in_chan, length);
    }
    
    template <class U>
    ConvolveError set(uint32_t in_chan, const U *input, uintptr_t length, bool resize)
    {
        conformed_input<T, U> typed_input(input, length);

        return do_channel(&convolve_mono<T>::template set<T>, in_chan, typed_input.get(), length, resize);
    }

    ConvolveError reset(uint32_t in_chan)
    {
        return do_channel(&convolve_mono<T>::reset, in_chan);
    }
    
    // Process
    
    void process(const T * const* ins, T *out, size_t num_samples, size_t active_in_chans, bool accumulate = false)
    {
        for (uint32_t i = 0; i < m_num_in_chans && i < active_in_chans ; i++)
            m_convolvers[i].process(ins[i], out, num_samples, accumulate || i);
    }
    
private:
    
    // Utility to apply an operation to a channel
    
    template <typename Method, typename... Args>
    ConvolveError do_channel(Method method, uint32_t in_chan, Args...args)
    {
        if (in_chan < m_num_in_chans)
            return (m_convolvers[in_chan].*method)(args...);
        else
            return CONVOLVE_ERR_IN_CHAN_OUT_OF_RANGE;
    }
    
    std::vector<convolve_mono<T>> m_convolvers;
    
    uint32_t m_num_in_chans;
};
