
#ifndef HISSTOOLS_LIBRARY_CONVOLUTION_MONO_HPP
#define HISSTOOLS_LIBRARY_CONVOLUTION_MONO_HPP

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <random>
#include <stdexcept>
#include <vector>

#include "../memory_swap.hpp"
#include "../namespace.hpp"
#include "partitioned.hpp"
#include "time_domain.hpp"
#include "utilities.hpp"

HISSTOOLS_LIBRARY_NAMESPACE_START()

enum class latency_mode
{
    zero,
    low,
    medium
};

template <class T, class IO = T>
class convolve_mono
{
    using ct = convolve_time_domain<T, IO>;
    using cp = convolve_partitioned<T, IO>;
    using partition_pointer = typename memory_swap<cp>::pointer_type;
    using partition_unique_pointer = std::unique_ptr<cp>;
    
public:
    
    // Constructors
    
    convolve_mono(uintptr_t max_length, latency_mode latency)
    : m_allocator(nullptr)
    , m_part_4(0)
    , m_length(0)
    , m_reset_offset(0)
    , m_reset(false)
    , m_rand_gen(std::random_device()())
    {
        switch (latency)
        {
            case latency_mode::zero:        set_partitions(max_length, true, 256, 1024, 4096, 16384);     break;
            case latency_mode::low:         set_partitions(max_length, false, 256, 1024, 4096, 16384);    break;
            case latency_mode::medium:      set_partitions(max_length, false, 1024, 4096, 16384);         break;
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
        partition_pointer part_4 = m_part_4.access();
        
        set_reset_offset(part_4, offset);
    }
    
    convolve_error resize(uintptr_t length)
    {
        m_length = 0;
        partition_pointer part_4 = m_part_4.equal(m_allocator, large_free, length);
        
        if (part_4.get())
            part_4.get()->set_reset_offset(m_reset_offset);
        
        return part_4.size() == length ? convolve_error::none : convolve_error::memory_unavailable;
    }
    
    template <class U>
    convolve_error set(const U* input, uintptr_t length, bool request_resize)
    {
        conformed_input<T, U> typed_input(input, length);

        // Lock or resize first to ensure that audio finishes processing before we replace
        
        m_length = 0;
        partition_pointer part4 = request_resize ? m_part_4.equal(m_allocator, large_free, length) : m_part_4.access();
        
        if (part4.get())
        {
            for_all(&ct::template set<T>, &cp::template set<T>, part4, typed_input.get(), length);
            part4.get()->set_reset_offset(m_reset_offset);
            m_length = length;
            reset();
        }
        
        if (length && !part4.get())
            return convolve_error::memory_unavailable;
        
        if (length > part4.size())
            return convolve_error::memory_alloc_too_small;
        
        return convolve_error::none;
    }
    
    convolve_error reset()
    {
        m_reset = true;
        return convolve_error::none;
    }
    
    // Process
    
    void process(const IO* in, IO* out, uintptr_t num_samples, bool accumulate = false)
    {
        partition_pointer part_4 = m_part_4.attempt();
        
        if (m_length && m_length <= part_4.size())
        {
            if (m_reset)
            {
                for_all(&ct::reset, &cp::reset, part_4);
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
                        uint32_t a,
                        uint32_t b = 0,
                        uint32_t c = 0,
                        uint32_t d = 0)
    {
        // Utilities
        
        auto check_and_store_fft_size = [this](int size, int prev)
        {
            if ((size >= (1 << 5)) && (size <= (1 << 20)) && size > prev)
                m_sizes.push_back(size);
            else if (size)
                throw std::runtime_error("invalid FFT size or order");
        };
        
        auto create_part = [](partition_unique_pointer& obj, uint32_t& offset, uint32_t size, uint32_t next)
        {
            obj.reset(new cp(size, (next - size) >> 1, offset, (next - size) >> 1));
            offset += (next - size) >> 1;
        };
        
        // Sanity checks
        
        check_and_store_fft_size(a, 0);
        check_and_store_fft_size(b, a);
        check_and_store_fft_size(c, b);
        check_and_store_fft_size(d, c);
        
        if (!num_sizes())
            throw std::runtime_error("no valid FFT sizes given");
        
        // Lock to ensure we have exclusive access
        
        partition_pointer part_4 = m_part_4.access();
        
        uint32_t offset = zero_latency ? m_sizes[0] >> 1 : 0;
        uint32_t final_size = m_sizes[num_sizes() - 1];
        
        // Allocate paritions in unique pointers
        
        if (zero_latency) m_time.reset(new ct(0, m_sizes[0] >> 1));
        if (num_sizes() == 4) create_part(m_part_1, offset, m_sizes[0], m_sizes[1]);
        if (num_sizes() > 2) create_part(m_part_2, offset, m_sizes[num_sizes() - 3], m_sizes[num_sizes() - 2]);
        if (num_sizes() > 1) create_part(m_part_3, offset, m_sizes[num_sizes() - 2], m_sizes[num_sizes() - 1]);
        
        // Allocate the final resizeable partition
        
        m_allocator = [offset, final_size](uintptr_t size)
        {
            return new cp(final_size, std::max(size, uintptr_t(final_size)) - offset, offset, 0);
        };
        
        part_4.equal(m_allocator, large_free, max_length);
        
        // Set offsets
        
        m_rand_dist = std::uniform_int_distribution<uintptr_t>(0, (m_sizes.back() >> 1) - 1);
        set_reset_offset(part_4);
    }
    
private:
    
    size_t num_sizes() { return m_sizes.size(); }

    void set_reset_offset(partition_pointer &part4, intptr_t offset = -1)
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
    
    template <typename U, typename V, typename ...Args>
    void for_all(U time_method, V part_method, partition_pointer& part_4, Args... args)
    {
        if (m_time) (m_time.get()->*time_method)(args...);
        if (m_part_1) (m_part_1.get()->*part_method)(args...);
        if (m_part_2) (m_part_2.get()->*part_method)(args...);
        if (m_part_3) (m_part_3.get()->*part_method)(args...);
        if (part_4.get()) (part_4.get()->*part_method)(args...);
    }
    
    static void large_free(cp* large_partition)
    {
        delete large_partition;
    }
    
    typename memory_swap<cp>::alloc_func m_allocator;
    
    std::vector<uint32_t> m_sizes;
    
    std::unique_ptr<ct> m_time;
    std::unique_ptr<cp> m_part_1;
    std::unique_ptr<cp> m_part_2;
    std::unique_ptr<cp> m_part_3;
    
    memory_swap<cp> m_part_4;
    
    uintptr_t m_length;
    
    intptr_t m_reset_offset;
    bool m_reset;
    
    // Random Number Generation
    
    std::default_random_engine m_rand_gen;
    std::uniform_int_distribution<uintptr_t> m_rand_dist;
};

HISSTOOLS_LIBRARY_NAMESPACE_END()

#endif /* HISSTOOLS_LIBRARY_CONVOLUTION_MONO_HPP */
