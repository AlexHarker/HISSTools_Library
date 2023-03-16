
#ifndef CONVOLVE_MONO_HPP
#define CONVOLVE_MONO_HPP

#include "ConvolveUtilities.hpp"
#include "ConvolveTimeDomain.hpp"
#include "ConvolvePartitioned.hpp"
#include "../MemorySwap.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <random>
#include <stdexcept>
#include <vector>

enum class LatencyMode
{
    Zero,
    Short,
    Medium,
} ;

template <class T, class IO = T>
class convolve_mono
{
    using CT = convolve_time_domain<T, IO>;
    using CP = convolve_partitioned<T, IO>;
    using PartPtr = typename memory_swap<CP>::Ptr;
    using PartUniquePtr = std::unique_ptr<CP>;
    
public:
    
    // Constructors
    
    convolve_mono(uintptr_t max_length, LatencyMode latency)
    : m_allocator(nullptr)
    , m_part_4(0)
    , m_length(0)
    , m_reset_offset(0)
    , m_reset(false)
    , m_rand_gen(std::random_device()())
    {
        switch (latency)
        {
            case LatencyMode::Zero:     set_partitions(max_length, true, 256, 1024, 4096, 16384);     break;
            case LatencyMode::Short:    set_partitions(max_length, false, 256, 1024, 4096, 16384);    break;
            case LatencyMode::Medium:   set_partitions(max_length, false, 1024, 4096, 16384);         break;
        }
    }
    
    convolve_mono(uintptr_t max_length, bool zero_latency, uint32_t A, uint32_t B = 0, uint32_t C = 0, uint32_t D = 0)
    : m_allocator(nullptr)
    , m_part_4(0)
    , m_length(0)
    , m_reset_offset(0)
    , m_reset(false)
    , m_rand_gen(std::random_device()())
    {
        set_partitions(max_length, zero_latency, A, B, C, D);
    }
    
    // Moveable but not copyable
    
    convolve_mono(convolve_mono& obj) = delete;
    convolve_mono& operator = (convolve_mono& obj) = delete;
    
    convolve_mono(convolve_mono&& obj)
    : m_allocator(obj.m_allocator)
    , m_sizes(std::move(obj.m_sizes))
    , m_time(std::move(obj.m_time))
    , m_part_1(std::move(obj.m_part_1))
    , m_part_2(std::move(obj.m_part_2))
    , m_part_3(std::move(obj.m_part_3))
    , m_part_4(std::move(obj.m_part_4))
    , m_length(obj.m_length)
    , m_reset_offset(obj.m_reset_offset)
    , m_reset(true)
    {}
    
    convolve_mono& operator = (convolve_mono&& obj)
    {
        m_allocator = obj.m_allocator;
        m_sizes = std::move(obj.m_sizes);
        m_time = std::move(obj.m_time);
        m_part_1 = std::move(obj.m_part_1);
        m_part_2 = std::move(obj.m_part_2);
        m_part_3 = std::move(obj.m_part_3);
        m_part_4 = std::move(obj.m_part_4);
        m_length = obj.m_length;
        m_reset_offset = obj.m_reset_offset;
        m_reset = true;
        
        return *this;
    }
    
    // Offsets / resize / set/ reset
    
    void set_reset_offset(intptr_t offset = -1)
    {
        PartPtr part_4 = m_part_4.access();
        
        set_reset_offset(part_4, offset);
    }
    
    ConvolveError resize(uintptr_t length)
    {
        m_length = 0;
        PartPtr part_4 = m_part_4.equal(m_allocator, large_free, length);
        
        if (part_4.get())
            part_4.get()->set_reset_offset(m_reset_offset);
        
        return part_4.get_size() == length ? ConvolveError::None : ConvolveError::MemUnavailable;
    }
    
    template <class U>
    ConvolveError set(const U *input, uintptr_t length, bool request_resize)
    {
        conformed_input<T, U> typed_input(input, length);

        // Lock or resize first to ensure that audio finishes processing before we replace
        
        m_length = 0;
        PartPtr part4 = request_resize ? m_part_4.equal(m_allocator, large_free, length) : m_part_4.access();
        
        if (part4.get())
        {
            for_all(&CT::template set<T>, &CP::template set<T>, part4, typed_input.get(), length);
            part4.get()->set_reset_offset(m_reset_offset);
            m_length = length;
            reset();
        }
        
        if (length && !part4.get())
            return ConvolveError::MemUnavailable;
        
        if (length > part4.get_size())
            return ConvolveError::MemAllocTooSmall;
        
        return ConvolveError::None;
    }
    
    ConvolveError reset()
    {
        m_reset = true;
        return ConvolveError::None;
    }
    
    // Process
    
    void process(const IO *in, IO *out, uintptr_t num_samples, bool accumulate = false)
    {
        PartPtr part_4 = m_part_4.attempt();
        
        if (m_length && m_length <= part_4.get_size())
        {
            if (m_reset)
            {
                for_all(&CT::reset, &CP::reset, part_4);
                m_reset = false;
            }
                        
            if (m_time) m_time->process(in, out, num_samples, accumulate);
            if (m_part_1) m_part_1->process(in, out, num_samples, accumulate || m_time);
            if (m_part_2) m_part_2->process(in, out, num_samples, accumulate || m_part_1);
            if (m_part_3) m_part_3->process(in, out, num_samples, accumulate || m_part_2);
            if (part_4.get()) part_4.get()->process(in, out, num_samples, accumulate || m_part_3);
        }
        else if (!accumulate)
            std::fill_n(out, num_samples, IO(0));
            
    }
    
    // Set partiioning
    
    void set_partitions(uintptr_t max_length,
                        bool zero_latency,
                        uint32_t A,
                        uint32_t B = 0,
                        uint32_t C = 0,
                        uint32_t D = 0)
    {
        // Utilities
        
        auto check_and_store_fft_size = [this](int size, int prev)
        {
            if ((size >= (1 << 5)) && (size <= (1 << 20)) && size > prev)
                m_sizes.push_back(size);
            else if (size)
                throw std::runtime_error("invalid FFT size or order");
        };
        
        auto create_part = [](PartUniquePtr& obj, uint32_t& offset, uint32_t size, uint32_t next)
        {
            obj.reset(new CP(size, (next - size) >> 1, offset, (next - size) >> 1));
            offset += (next - size) >> 1;
        };
        
        // Sanity checks
        
        check_and_store_fft_size(A, 0);
        check_and_store_fft_size(B, A);
        check_and_store_fft_size(C, B);
        check_and_store_fft_size(D, C);
        
        if (!num_sizes())
            throw std::runtime_error("no valid FFT sizes given");
        
        // Lock to ensure we have exclusive access
        
        PartPtr part_4 = m_part_4.access();
        
        uint32_t offset = zero_latency ? m_sizes[0] >> 1 : 0;
        uint32_t final_size = m_sizes[num_sizes() - 1];
        
        // Allocate paritions in unique pointers
        
        if (zero_latency) m_time.reset(new CT(0, m_sizes[0] >> 1));
        if (num_sizes() == 4) create_part(m_part_1, offset, m_sizes[0], m_sizes[1]);
        if (num_sizes() > 2) create_part(m_part_2, offset, m_sizes[num_sizes() - 3], m_sizes[num_sizes() - 2]);
        if (num_sizes() > 1) create_part(m_part_3, offset, m_sizes[num_sizes() - 2], m_sizes[num_sizes() - 1]);
        
        // Allocate the final resizeable partition
        
        m_allocator = [offset, final_size](uintptr_t size)
        {
            return new CP(final_size, std::max(size, uintptr_t(final_size)) - offset, offset, 0);
        };
        
        part_4.equal(m_allocator, large_free, max_length);
        
        // Set offsets
        
        m_rand_dist = std::uniform_int_distribution<uintptr_t>(0, (m_sizes.back() >> 1) - 1);
        set_reset_offset(part_4);
    }
    
private:
    
    size_t num_sizes() { return m_sizes.size(); }

    void set_reset_offset(PartPtr &part4, intptr_t offset = -1)
    {
        if (offset < 0)
            offset = m_rand_dist(m_rand_gen);
        
        if (m_part_1) m_part_1.get()->set_reset_offset(offset + (m_sizes[num_sizes() - 3] >> 3));
        if (m_part_2) m_part_2.get()->set_reset_offset(offset + (m_sizes[num_sizes() - 2] >> 3));
        if (m_part_3) m_part_3.get()->set_reset_offset(offset + (m_sizes[num_sizes() - 1] >> 3));
        
        if (part4.get()) part4.get()->set_reset_offset(offset);
        
        m_reset_offset = offset;
    }
    
    // Utilities
    
    template <typename TimeMethod, typename PartMethod, typename ...Args>
    void for_all(TimeMethod time_method, PartMethod part_method, PartPtr& part_4, Args... args)
    {
        if (m_time) (m_time.get()->*time_method)(args...);
        if (m_part_1) (m_part_1.get()->*part_method)(args...);
        if (m_part_2) (m_part_2.get()->*part_method)(args...);
        if (m_part_3) (m_part_3.get()->*part_method)(args...);
        if (part_4.get()) (part_4.get()->*part_method)(args...);
    }
    
    static void large_free(CP *large_partition)
    {
        delete large_partition;
    }
    
    typename memory_swap<CP>::AllocFunc m_allocator;
    
    std::vector<uint32_t> m_sizes;
    
    std::unique_ptr<CT> m_time;
    std::unique_ptr<CP> m_part_1;
    std::unique_ptr<CP> m_part_2;
    std::unique_ptr<CP> m_part_3;
    
    memory_swap<CP> m_part_4;
    
    uintptr_t m_length;
    
    intptr_t m_reset_offset;
    bool m_reset;
    
    // Random Number Generation
    
    std::default_random_engine m_rand_gen;
    std::uniform_int_distribution<uintptr_t> m_rand_dist;
};

#endif /* CONVOLVE_MONO_HPP */
