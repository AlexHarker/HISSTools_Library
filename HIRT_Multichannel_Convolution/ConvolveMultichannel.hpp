
#pragma once

#include "../MemorySwap.hpp"
#include "../SIMDSupport.hpp"
#include "ConvolveUtilities.hpp"
#include "ConvolveNToMono.hpp"

#include <cstdint>
#include <vector>

template <class T>
class convolve_multichannel
{
    using CM = convolve_multichannel;
    
public:
    
    convolve_multichannel(uint32_t num_ins, uint32_t num_outs, LatencyMode latency)
    : m_num_ins(std::max(num_ins, 1U))
    , m_num_outs(std::max(num_outs, 1U))
    , m_parallel(false)
    , m_temp_1(nullptr)
    , m_temp_2(nullptr)
    , m_temporary_memory(0)
    {
        for (uint32_t i = 0; i < m_num_ins; i++)
            m_in_temps.push_back(nullptr);
        
        for (uint32_t i = 0; i < m_num_outs; i++)
            m_convolvers.push_back(new convolve_n_to_mono<T>(m_num_ins, 16384, latency));
    }
    
    convolve_multichannel(uint32_t num_io, LatencyMode latency)
    : m_num_ins(std::max(num_io, 1U))
    , m_num_outs(m_num_ins)
    , m_parallel(true)
    , m_temp_1(nullptr)
    , m_temp_2(nullptr)
    , m_temporary_memory(0)
    {
        for (uint32_t i = 0; i < m_num_ins; i++)
        {
            m_convolvers.push_back(new convolve_n_to_mono<T>(1, 16384, latency));
            m_in_temps.push_back(nullptr);
        }
    }
    
    virtual ~convolve_multichannel() throw()
    {
        for (uint32_t i = 0; i < m_num_outs; i++)
            delete m_convolvers[i];
    }
    
    // Clear IRs
    
    void clear(bool resize)
    {
        do_all(static_cast<void (CM::*)(uint32_t, uint32_t, bool)>(&CM::clear), resize);
    }
    
    void clear(uint32_t in_chan, uint32_t out_chan, bool resize)
    {
        set(in_chan, out_chan, static_cast<T *>(nullptr), 0, resize);
    }
    
    // DSP Engine Reset
    
    void reset()
    {
        do_all(static_cast<ConvolveError (CM::*)(uint32_t, uint32_t)>(&CM::reset));
    }
    
    ConvolveError reset(uint32_t in_chan, uint32_t out_chan)
    {
        if (out_chan < m_num_outs)
            return m_convolvers[out_chan]->reset(offset_input(in_chan, out_chan));
        else
            return CONVOLVE_ERR_OUT_CHAN_OUT_OF_RANGE;
    }
    
    // Resize and set IR
    
    ConvolveError resize(uint32_t in_chan, uint32_t out_chan, uintptr_t length)
    {
        if (out_chan < m_num_outs)
            return m_convolvers[out_chan]->resize(offset_input(in_chan, out_chan), length);
        else
            return CONVOLVE_ERR_IN_CHAN_OUT_OF_RANGE;
    }
    
    template <class U>
    ConvolveError set(uint32_t in_chan, uint32_t out_chan, const U* input, uintptr_t length, bool resize)
    {
        TypeConformedInput<T, U> typed_input(input, length);
        
        if (out_chan < m_num_outs)
            return m_convolvers[out_chan]->set(offset_input(in_chan, out_chan), typed_input.get(), length, resize);
        else
            return CONVOLVE_ERR_OUT_CHAN_OUT_OF_RANGE;
    }
    
    // DSP
    
    void process(const T * const* ins, T** outs, size_t num_ins, size_t num_outs, size_t num_samples)
    {
        auto mem_pointer = m_temporary_memory.grow((m_num_ins + 2) * num_samples);
        temp_setup(mem_pointer.get(), mem_pointer.get_size());
        
        if (!mem_pointer.get())
            num_ins = num_outs = 0;
        
        SIMDDenormals denormal_handler;

        for (size_t i = 0; i < num_outs; i++)
        {
            const T *parallel_in[1] = { ins[i] };
            const T * const *use_ins = m_parallel ? parallel_in : ins;
            
            m_convolvers[i]->process(use_ins, outs[i], num_samples, m_parallel ? 1 : num_ins);
        }
    }
    
    void process(const double * const* ins, double** outs, size_t num_ins, size_t num_outs, size_t num_samples)
    {
        auto mem_pointer = m_temporary_memory.grow((m_num_ins + 2) * num_samples);
        temp_setup(mem_pointer.get(), mem_pointer.get_size());
        
        if (!mem_pointer.get())
            num_ins = num_outs = 0;
        
        num_ins = num_ins > m_num_ins ? m_num_ins : num_ins;
        num_outs = num_outs > m_num_outs ? m_num_outs : num_outs;
        
        for (uintptr_t i = 0; i < num_ins; i++)
            for (uintptr_t j = 0; j < num_samples; j++)
                m_in_temps[i][j] = static_cast<T>(ins[i][j]);
        
        SIMDDenormals denormal_handler;
        
        for (uintptr_t i = 0; i < num_outs; i++)
        {
            const T *parallel_in[1] = { m_in_temps[i] };
            const T * const *in_temps = m_in_temps.data();
            const T * const *use_ins = m_parallel ? parallel_in : in_temps;
            
            m_convolvers[i]->process(use_ins, m_temp_2, num_samples, m_parallel ? 1 : num_ins);
            
            for (uintptr_t j = 0; j < num_samples; j++)
                outs[i][j] = m_temp_2[j];
        }
    }

private:
    
    void temp_setup(T* mem_pointer, uintptr_t max_num_samples)
    {
        max_num_samples /= (m_num_ins + 2);
        m_in_temps[0] = mem_pointer;
        
        for (uint32_t i = 1; i < m_num_ins; i++)
            m_in_temps[i] = m_in_temps[0] + (i * max_num_samples);
        
        m_temp_1 = m_in_temps[m_num_ins - 1] + max_num_samples;
        m_temp_2 = m_temp_1 + max_num_samples;
    }
    
    uint32_t offset_input(uint32_t in_chan, uint32_t out_chan)
    {
        // For Parallel operation you must pass the same in/out channel
    
        return m_parallel ? in_chan - out_chan : in_chan;
    }
    
    // Utility to apply an operation to a channel
        
    template<typename Method, typename... Args>
    void do_all(Method method, Args...args)
    {
        if (m_parallel)
        {
            for (uint32_t i = 0; i < m_num_outs; i++)
                (this->*method)(i, i, args...);
        }
        else
        {
            for (uint32_t i = 0; i < m_num_outs; i++)
                for (uint32_t j = 0; j < m_num_ins; j++)
                    (this->*method)(j, i, args...);
        }
    }
    
    // Data
    
    const uint32_t m_num_ins;
    const uint32_t m_num_outs;
    const bool m_parallel;
    
    std::vector<T*> m_in_temps;
    T* m_temp_1;
    T* m_temp_2;
    
    memory_swap<T> m_temporary_memory;
    
    std::vector<convolve_n_to_mono<T>*> m_convolvers;
};
