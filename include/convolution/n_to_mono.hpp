
#ifndef HISSTOOLS_LIBRARY_CONVOLUTION_N_TO_MONO_HPP
#define HISSTOOLS_LIBRARY_CONVOLUTION_N_TO_MONO_HPP

#include <algorithm>
#include <cstdint>
#include <vector>

#include "../namespace.hpp"
#include "mono.hpp"
#include "utilities.hpp"

HISSTOOLS_LIBRARY_NAMESPACE_START()

template <class T, class IO = T>
class convolve_n_to_mono
{
    using cn = convolve_n_to_mono;

public:
    
    // Constructor
    
    convolve_n_to_mono(uint32_t in_chans, uintptr_t max_length, latency_mode latency)
    {
        for (uint32_t i = 0; i < in_chans; i++)
            m_convolvers.emplace_back(max_length, latency);
    }
    
    // Channels
    
    uint32_t get_num_ins() const { return static_cast<uint32_t>(m_convolvers.size()); }
    
    // Clear IRs
    
    void clear(bool resize)
    {
        for_all(static_cast<void (cn::*)(uint32_t, uint32_t, bool)>(&cn::clear), resize);
    }
    
    void clear(uint32_t in_chan, bool resize)
    {
        set<T>(in_chan, nullptr, 0, resize);
    }
    
    // Reset
    
    void reset()
    {
        for_all(static_cast<void (cn::*)(uint32_t, uint32_t, bool)>(&cn::clear));
    }
    
    convolve_error reset(uint32_t in_chan)
    {
        return do_channel(&convolve_mono<T, IO>::reset, in_chan);
    }
    
    // Resize and set IR
    
    convolve_error resize(uint32_t in_chan, uintptr_t length)
    {
        return do_channel(&convolve_mono<T, IO>::resize, in_chan, length);
    }
    
    template <class U>
    convolve_error set(uint32_t in_chan, const U* input, uintptr_t length, bool resize)
    {
        conformed_input<T, U> typed_input(input, length);

        return do_channel(&convolve_mono<T, IO>::template set<T>, in_chan, typed_input.get(), length, resize);
    }

    // Process
    
    void process(const IO * const* ins, IO* out, uintptr_t num_samples, uint32_t num_ins, bool accumulate = false)
    {
        num_ins = std::min(num_ins, get_num_ins());

        for (uint32_t i = 0; i < num_ins; i++)
            m_convolvers[i].process(ins[i], out, num_samples, accumulate || i);
    }
    
private:
    
    // Utility to apply an operation to a channel
    
    template <typename Method, typename... Args>
    convolve_error do_channel(Method method, uint32_t in_chan, Args...args)
    {
        if (in_chan < get_num_ins())
            return (m_convolvers[in_chan].*method)(args...);
        else
            return convolve_error::in_channel_outside_range;
    }
    
    // Utility to apply an operation to all convolvers
    
    template <typename Method, typename... Args>
    void for_all(Method method, Args...args)
    {
        for (uint32_t i = 0; i < get_num_ins(); i++)
            (this->*method)(i, args...);
    }
    
    std::vector<convolve_mono<T, IO>> m_convolvers;
};

HISSTOOLS_LIBRARY_NAMESPACE_END()

#endif /* HISSTOOLS_LIBRARY_CONVOLUTION_N_TO_MONO_HPP */
