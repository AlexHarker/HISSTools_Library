
#ifndef CONVOLVE_MULTICHANNEL_HPP
#define CONVOLVE_MULTICHANNEL_HPP

#include "../MemorySwap.hpp"
#include "../SIMDSupport.hpp"
#include "ConvolveUtilities.hpp"
#include "ConvolveNToMono.hpp"

#include <cstdint>
#include <vector>

template <class T, class IO = T>
class convolve_multichannel
{
    using CM = convolve_multichannel;
    using CN = convolve_n_to_mono<T, IO>;

public:
    
    // Constructors / Destructor
    
    convolve_multichannel(uint32_t num_ins, uint32_t num_outs, LatencyMode latency)
    : m_parallel(false)
    {
        for (uint32_t i = 0; i < std::max(num_outs, 1U); i++)
            m_convolvers.emplace_back(std::max(num_ins, 1U), 16384, latency);
    }
    
    convolve_multichannel(uint32_t num_io, LatencyMode latency)
    : m_parallel(true)
    {
        for (uint32_t i = 0; i < std::max(num_io, 1U); i++)
            m_convolvers.emplace_back(1, 16384, latency);
    }
    
    // Channels
    
    uint32_t get_num_ins() const { return m_convolvers[0].get_num_ins(); }
    uint32_t get_num_outs() const { return static_cast<uint32_t>(m_convolvers.size()); }
    
    // Clear IRs
    
    void clear(bool resize)
    {
        for_all(static_cast<void (CM::*)(uint32_t, uint32_t, bool)>(&CM::clear), resize);
    }
    
    void clear(uint32_t in_chan, uint32_t out_chan, bool resize)
    {
        set<T>(in_chan, out_chan, nullptr, 0, resize);
    }
    
    // Reset
    
    void reset()
    {
        for_all(static_cast<ConvolveError (CM::*)(uint32_t, uint32_t)>(&CM::reset));
    }
    
    ConvolveError reset(uint32_t in_chan, uint32_t out_chan)
    {
        auto method = static_cast<ConvolveError (CN::*)(uint32_t)>(&CN::reset);
        
        return do_channel(method, out_chan, offset_input(in_chan, out_chan));
    }
    
    // Resize and set IR
    
    ConvolveError resize(uint32_t in_chan, uint32_t out_chan, uintptr_t length)
    {
        return do_channel(&CN::resize, out_chan, offset_input(in_chan, out_chan), length);
    }
    
    template <class U>
    ConvolveError set(uint32_t in_chan, uint32_t out_chan, const U* input, uintptr_t length, bool resize)
    {
        conformed_input<T, U> typed_input(input, length);
        
        return do_channel(&CN::template set<T>, out_chan, offset_input(in_chan, out_chan), typed_input.get(), length, resize);
    }
    
    // Process
    
    void process(const IO * const* ins, IO** outs, uint32_t num_ins, uint32_t num_outs, uintptr_t num_samples)
    {
        SIMDDenormals denormal_handler;
        
        num_ins = std::min(num_ins, get_num_ins());
        num_outs = std::min(num_outs, get_num_outs());
        
        for (uint32_t i = 0; i < num_outs; i++)
        {
            const IO *parallel_in[1] = { ins[i] };
            const IO * const *use_ins = m_parallel ? parallel_in : ins;
            
            m_convolvers[i].process(use_ins, outs[i], num_samples, m_parallel ? 1 : num_ins);
        }
    }

private:

    // For parallel operation you must pass the same in/out channel

    uint32_t offset_input(uint32_t in_chan, uint32_t out_chan)
    {
        return m_parallel ? in_chan - out_chan : in_chan;
    }
    
    // Utility to do one output channel
    
    template <typename Method, typename... Args>
    ConvolveError do_channel(Method method, uint32_t out_chan, Args...args)
    {
        if (out_chan < get_num_outs())
            return (m_convolvers[out_chan].*method)(args...);
        else
            return ConvolveError::OutChanOutOfRange;
    }
    
    // Utility to apply an operation to all convolvers
        
    template <typename Method, typename... Args>
    void for_all(Method method, Args...args)
    {
        if (m_parallel)
        {
            for (uint32_t i = 0; i < get_num_outs(); i++)
                (this->*method)(i, i, args...);
        }
        else
        {
            for (uint32_t i = 0; i < get_num_outs(); i++)
                for (uint32_t j = 0; j < get_num_ins(); j++)
                    (this->*method)(j, i, args...);
        }
    }
    
    // Data
    
    const bool m_parallel;
        
    std::vector<CN> m_convolvers;
};

#endif /* CONVOLVE_MULTICHANNEL_HPP */
