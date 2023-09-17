
#ifndef HISSTOOLS_LIBRARY_CONVOLUTION_PARTITIONED_HPP
#define HISSTOOLS_LIBRARY_CONVOLUTION_PARTITIONED_HPP

#include <algorithm>
#include <cstdint>
#include <random>

#include "../simd_support.hpp"
#include "../namespace.hpp"
#include "../fft/fft.hpp"
#include "utilities.hpp"

HISSTOOLS_LIBRARY_NAMESPACE_START()

template <class T, class IO = T>
class convolve_partitioned
{
    // N.B. fixed_min_fft_size_log2 must take into account of the loop unrolling of vectors
    // N.B. fixed_max_fft_size_log2 is perhaps conservative right now
    
    using vector_type = simd_type<T, simd_limits<T>::max_size>;
    
    static constexpr int loop_unroll_size = 4;
    static constexpr int fixed_min_fft_size_log2 = impl::ilog2<vector_type::size * loop_unroll_size>();
    static constexpr int fixed_max_fft_size_log2 = 20;
    
public:
    
    convolve_partitioned(uintptr_t max_fft_size = 16384,
                         uintptr_t max_length = 131072,
                         uintptr_t offset = 0,
                         uintptr_t length = 0)
    : m_max_impulse_length(max_length)
    , m_fft_size_log2(0)
    , m_input_position(0)
    , m_partitions_done(0)
    , m_last_partition(0)
    , m_num_partitions(0)
    , m_valid_partitions(0)
    , m_reset_offset(-1)
    , m_reset_flag(true)
    , m_rand_gen(std::random_device()())
    {
        // Set default initial attributes and variables
        
        set_max_fft_size(max_fft_size);
        set_fft_size(get_max_fft_size());
        set_offset(offset);
        set_length(length);
        
        // Allocate impulse buffer and input buffer
        
        max_fft_size = get_max_fft_size();
        
        // This is designed to make sure we can load the max impulse length, whatever the fft size
        
        if (m_max_impulse_length % (max_fft_size >> 1))
        {
            m_max_impulse_length /= (max_fft_size >> 1);
            m_max_impulse_length++;
            m_max_impulse_length *= (max_fft_size >> 1);
        }
        
        m_impulse_buffer.realp = allocate_aligned<T>(m_max_impulse_length * 4);
        m_impulse_buffer.imagp = m_impulse_buffer.realp + m_max_impulse_length;
        m_input_buffer.realp = m_impulse_buffer.imagp + m_max_impulse_length;
        m_input_buffer.imagp = m_input_buffer.realp + m_max_impulse_length;
        
        // Allocate fft and temporary buffers
        
        m_fft_buffers[0] = allocate_aligned<T>(max_fft_size * 6);
        m_fft_buffers[1] = m_fft_buffers[0] + max_fft_size;
        m_fft_buffers[2] = m_fft_buffers[1] + max_fft_size;
        m_fft_buffers[3] = m_fft_buffers[2] + max_fft_size;
        
        m_accum_buffer.realp = m_fft_buffers[3] + max_fft_size;
        m_accum_buffer.imagp = m_accum_buffer.realp + (max_fft_size >> 1);
        m_partition_temp.realp = m_accum_buffer.imagp + (max_fft_size >> 1);
        m_partition_temp.imagp = m_partition_temp.realp + (max_fft_size >> 1);
        
        create_fft_setup(&m_fft_setup, m_max_fft_size_log2);
    }
    
    ~convolve_partitioned()
    {
        destroy_fft_setup(m_fft_setup);
                
        deallocate_aligned(m_impulse_buffer.realp);
        deallocate_aligned(m_fft_buffers[0]);
    }
    
    // Non-moveable and copyable
    
    convolve_partitioned(convolve_partitioned& obj) = delete;
    convolve_partitioned& operator = (convolve_partitioned& obj) = delete;
    convolve_partitioned(convolve_partitioned&& obj) = delete;
    convolve_partitioned& operator = (convolve_partitioned&& obj) = delete;
    
    convolve_error set_fft_size(uintptr_t fft_size)
    {
        uintptr_t fft_size_log2 = impl::ilog2(fft_size);
        
        convolve_error error = convolve_error::none;
        
        if (fft_size_log2 < fixed_min_fft_size_log2 || fft_size_log2 > m_max_fft_size_log2)
            return convolve_error::fft_size_outside_range;
        
        if (fft_size != (uintptr_t(1) << fft_size_log2))
            error = convolve_error::fft_size_not_pow2;
        
        // Set fft variables iff the fft size has actually actually changed
        
        if (fft_size_log2 != m_fft_size_log2)
        {
            m_num_partitions = 0;
            m_fft_size_log2 = fft_size_log2;
        }
        
        m_rand_dist = std::uniform_int_distribution<uintptr_t>(0, (fft_size >> 1) - 1);
        
        return error;
    }
    
    convolve_error set_length(uintptr_t length)
    {
        m_length = std::min(length, m_max_impulse_length);
        
        return (length > m_max_impulse_length) ? convolve_error::partition_len_too_large : convolve_error::none;
    }
    
    void set_offset(uintptr_t offset)
    {
        m_offset = offset;
    }
    
    void set_reset_offset(intptr_t offset = -1)
    {
        m_reset_offset = offset;
    }
    
    template <class U>
    convolve_error set(const U* input, uintptr_t length)
    {
        conformed_input<T, U> typed_input(input, length);

        convolve_error error = convolve_error::none;
        
        // FFT variables
        
        uintptr_t fft_size = get_fft_size();
        uintptr_t fft_size_halved = fft_size >> 1;
           
        m_num_partitions = 0;
        
        // Calculate how much of the buffer to load
        
        length = (!input || length <= m_offset) ? 0 : length - m_offset;
        length = (m_length && m_length < length) ? m_length : length;
        
        if (length > m_max_impulse_length)
        {
            length = m_max_impulse_length;
            error = convolve_error::memory_alloc_too_small;
        }
        
        // Partition / load the impulse
        
        uintptr_t num_partitions = 0;
        uintptr_t buffer_position = m_offset;

        T* buffer_temp_1 = m_partition_temp.realp;
        split_type<T> buffer_temp_2 = m_impulse_buffer;

        for (; length > 0; buffer_position += fft_size_halved, num_partitions++)
        {
            // Get samples up to half the fft size
            
            uintptr_t num_samples = std::min(fft_size_halved, length);
            length -= num_samples;
            
            // Get samples and zero pad
            
            std::copy_n(typed_input.get() + buffer_position, num_samples, buffer_temp_1);
            std::fill_n(buffer_temp_1 + num_samples, fft_size - num_samples, T(0));
            
            // Do fft straight into position
            
            rfft(m_fft_setup, buffer_temp_1, &buffer_temp_2, fft_size, m_fft_size_log2);
            offset_split_pointer(buffer_temp_2, buffer_temp_2, fft_size_halved);
        }
        
        reset();
        m_num_partitions = num_partitions;

        return error;
    }

    void reset()
    {
        m_reset_flag = true;
    }
    
    void process(const IO* in, IO* out, uintptr_t num_samples, bool accumulate = false)
    {
        split_type<T> ir_temp;
        split_type<T> in_temp;
        
        // Scheduling variables
        
        intptr_t num_partitions_to_do;
        
        // FFT variables
        
        uintptr_t fft_size = get_fft_size();
        uintptr_t fft_size_halved = fft_size >> 1;
        
        uintptr_t rw_counter = m_rw_counter;
        uintptr_t hop_mask = fft_size_halved - 1;
        
        uintptr_t samples_remaining = num_samples;
        
        if  (!m_num_partitions)
        {
            std::fill_n(out, accumulate ? 0 : num_samples, IO(0));
            return;
        }
        // Reset everything here if needed - happens when the fft size changes, or a new buffer is loaded
        
        if (m_reset_flag)
        {
            // Reset fft buffers + accum buffer
            
            std::fill_n(m_fft_buffers[0], get_max_fft_size() * 5, T(0));
            
            // Reset fft rw_counter (randomly or by fixed amount)
            
            if (m_reset_offset < 0)
                rw_counter = m_rand_dist(m_rand_gen);
            else
                rw_counter = m_reset_offset % fft_size_halved;
            
            // Reset scheduling variables
            
            m_input_position = 0;
            m_partitions_done = 0;
            m_last_partition = 0;
            m_valid_partitions = 1;
            
            // Set reset flag off
            
            m_reset_flag = false;
        }
        
        // Main loop
        
        while (samples_remaining > 0)
        {
            // Calculate how many IO samples to deal with this loop (depending on when the next fft is due)
            
            uintptr_t till_next_fft = (fft_size_halved - (rw_counter & hop_mask));
            uintptr_t loop_size = samples_remaining < till_next_fft ? samples_remaining : till_next_fft;
            uintptr_t hi_counter = (rw_counter + fft_size_halved) & (fft_size - 1);
            
            // Load input into buffer (twice) and output from the output buffer
            
            impl::copy_cast_n(in, loop_size, m_fft_buffers[0] + rw_counter);
            impl::copy_cast_n(in, loop_size, m_fft_buffers[1] + hi_counter);
            
            if (accumulate)
                impl::add_cast_n(m_fft_buffers[3] + rw_counter, loop_size, out);
            else
                impl::copy_cast_n(m_fft_buffers[3] + rw_counter, loop_size, out);
            
            // Updates to pointers and counters
            
            samples_remaining -= loop_size;
            rw_counter += loop_size;
            in += loop_size;
            out += loop_size;
            
            uintptr_t fft_counter = rw_counter & hop_mask;
            bool fft_now = !fft_counter;
            
            // Work loop and scheduling - this is where most of the convolution is done
            // How many partitions to do this block? (make sure all partitions are done before the next fft)
            
            if (fft_now)
                num_partitions_to_do = (m_valid_partitions - m_partitions_done) - 1;
            else
                num_partitions_to_do = (((m_valid_partitions - 1) * fft_counter) 
                                       / fft_size_halved) - m_partitions_done;
            
            while (num_partitions_to_do > 0)
            {
                // Calculate wraparounds (if wraparound is within this set of partitions this loop will run again)
                
                uintptr_t next_partition = (m_last_partition < m_num_partitions) ? m_last_partition : 0;
                m_last_partition = std::min(m_num_partitions, next_partition + num_partitions_to_do);
                num_partitions_to_do -= m_last_partition - next_partition;
                
                // Calculate offsets and pointers
                
                offset_split_pointer(ir_temp, m_impulse_buffer, ((m_partitions_done + 1) * fft_size_halved));
                offset_split_pointer(in_temp, m_input_buffer, (next_partition * fft_size_halved));
                
                // Do processing
                
                for (uintptr_t i = next_partition; i < m_last_partition; i++)
                {
                    process_partition(in_temp, ir_temp, m_accum_buffer, fft_size_halved);
                    offset_split_pointer(ir_temp, ir_temp, fft_size_halved);
                    offset_split_pointer(in_temp, in_temp, fft_size_halved);
                    m_partitions_done++;
                }
            }
            
            // FFT processing
            
            if (fft_now)
            {
                // Do the fft into the input buffer and add first partition (needed now)
                // Then do ifft, scale and store (overlap-save)
                
                T* fft_input = m_fft_buffers[(rw_counter == fft_size) ? 1 : 0];
                
                offset_split_pointer(in_temp, m_input_buffer, (m_input_position * fft_size_halved));
                rfft(m_fft_setup, fft_input, &in_temp, fft_size, m_fft_size_log2);
                process_partition(in_temp, m_impulse_buffer, m_accum_buffer, fft_size_halved);
                rifft(m_fft_setup, &m_accum_buffer, m_fft_buffers[2], m_fft_size_log2);
                scale_store<vector_type>(m_fft_buffers[3], m_fft_buffers[2], fft_size, (rw_counter != fft_size));
                
                // Clear accumulation buffer
                
                std::fill_n(m_accum_buffer.realp, fft_size_halved, T(0));
                std::fill_n(m_accum_buffer.imagp, fft_size_halved, T(0));
                
                // Update RWCounter
                
                rw_counter = rw_counter & (fft_size - 1);
                
                // Set scheduling variables
                
                m_valid_partitions = std::min(m_num_partitions, m_valid_partitions + 1);
                m_input_position = m_input_position ? m_input_position - 1 : m_num_partitions - 1;
                m_last_partition = m_input_position + 1;
                m_partitions_done = 0;
            }
        }
        
        // Write counter back into the object
        
        m_rw_counter = rw_counter;
    }

private:
    
    uintptr_t get_fft_size()      { return uintptr_t(1) << m_fft_size_log2; }
    uintptr_t get_max_fft_size()  { return uintptr_t(1) << m_max_fft_size_log2; }
    
    // Struct to deal with loop unrolling of complex muliplication
    
    template <int N>
    struct loop_unroll
    {
        using vt = vector_type;
        using cvt = const vector_type;
        using recurse = loop_unroll<N - 1>;
        
        inline void multiply(vt*& out_r, vt*& out_i, cvt* in_r1, cvt* in_i1, cvt* in_r2, cvt* in_i2, uintptr_t i)
        {
            *out_r++ += (in_r1[i] * in_r2[i]) - (in_i1[i] * in_i2[i]);
            *out_i++ += (in_r1[i] * in_i2[i]) + (in_i1[i] * in_r2[i]);
            
            recurse().multiply(out_r, out_i, in_r1, in_i1, in_r2, in_i2, ++i);
        }
    };
    
    template <>
    struct loop_unroll<0>
    {
        template <typename...Args>
        void multiply(Args...) {}
    };

    // Process a partition
    
    static void process_partition(split_type<T> in_1, split_type<T> in_2, split_type<T> out, uintptr_t num_bins)
    {
        uintptr_t num_vecs = num_bins / vector_type::size;
        
        const vector_type* in_r1 = reinterpret_cast<const vector_type*>(in_1.realp);
        const vector_type* in_i1 = reinterpret_cast<const vector_type*>(in_1.imagp);
        const vector_type* in_r2 = reinterpret_cast<const vector_type*>(in_2.realp);
        const vector_type* in_i2 = reinterpret_cast<const vector_type*>(in_2.imagp);
        vector_type* out_r = reinterpret_cast<vector_type*>(out.realp);
        vector_type* out_i = reinterpret_cast<vector_type*>(out.imagp);
        
        T nyquist_1 = in_1.imagp[0];
        T nyquist_2 = in_2.imagp[0];
        
        // Do Nyquist Calculation and then zero these bins
        
        out.imagp[0] += nyquist_1 * nyquist_2;
        
        in_1.imagp[0] = T(0);
        in_2.imagp[0] = T(0);
        
        // Do other bins (loop unrolled)
        
        for (uintptr_t i = 0; i + (loop_unroll_size - 1) < num_vecs; i += loop_unroll_size)
            loop_unroll<loop_unroll_size>().multiply(out_r, out_i, in_r1, in_i1, in_r2, in_i2, i);
        
        // Replace nyquist bins
        
        in_1.imagp[0] = nyquist_1;
        in_2.imagp[0] = nyquist_2;
    }

    convolve_error set_max_fft_size(uintptr_t max_fft_size)
    {
        uintptr_t max_fft_size_log2 = impl::ilog2(max_fft_size);
        
        convolve_error error = convolve_error::none;
        
        if (max_fft_size_log2 > fixed_max_fft_size_log2)
        {
            error = convolve_error::fft_size_outside_range;
            max_fft_size_log2 = fixed_max_fft_size_log2;
        }
        
        if (max_fft_size_log2 && max_fft_size_log2 < fixed_min_fft_size_log2)
        {
            error = convolve_error::fft_size_outside_range;
            max_fft_size_log2 = fixed_min_fft_size_log2;
        }
        
        if (max_fft_size != (uintptr_t(1) << max_fft_size_log2))
            error = convolve_error::fft_size_not_pow2;
        
        m_max_fft_size_log2 = max_fft_size_log2;
        
        return error;
    }
    
    template <class U>
    static void scale_store(T* out, T* temp, uintptr_t fft_size, bool offset)
    {
        U* out_ptr = reinterpret_cast<U*>(out + (offset ? fft_size >> 1: 0));
        U* temp_ptr = reinterpret_cast<U*>(temp);
        U scale(T(1) / static_cast<T>(fft_size << 2));
        
        for (uintptr_t i = 0; i < (fft_size / (U::size * 2)); i++)
            *(out_ptr++) = *(temp_ptr++) * scale;
    }
    
    static void offset_split_pointer(split_type<T> &complex_1, const split_type<T> &complex_2, uintptr_t offset)
    {
        complex_1.realp = complex_2.realp + offset;
        complex_1.imagp = complex_2.imagp + offset;
    }
    
    // Parameters
    
    uintptr_t m_offset;
    uintptr_t m_length;
    uintptr_t m_max_impulse_length;
    
    // FFT variables
    
    setup_type<T> m_fft_setup;
    
    uintptr_t m_max_fft_size_log2;
    uintptr_t m_fft_size_log2;
    uintptr_t m_rw_counter;
    
    // Scheduling variables
    
    uintptr_t m_input_position;
    uintptr_t m_partitions_done;
    uintptr_t m_last_partition;
    uintptr_t m_num_partitions;
    uintptr_t m_valid_partitions;
    
    // Internal buffers
    
    T* m_fft_buffers[4];
    
    split_type<T> m_impulse_buffer;
    split_type<T> m_input_buffer;
    split_type<T> m_accum_buffer;
    split_type<T> m_partition_temp;
    
    // Flags
    
    intptr_t m_reset_offset;
    bool m_reset_flag;
    
    // Random number generation
    
    std::default_random_engine m_rand_gen;
    std::uniform_int_distribution<uintptr_t> m_rand_dist;
};

HISSTOOLS_LIBRARY_NAMESPACE_END()

#endif /* HISSTOOLS_LIBRARY_CONVOLUTION_PARTITIONED_HPP */
