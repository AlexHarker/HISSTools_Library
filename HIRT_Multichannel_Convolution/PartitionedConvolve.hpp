
#pragma once

#include "../HISSTools_FFT/HISSTools_FFT.h"

#include "ConvolveErrors.h"
#include "../SIMDSupport.hpp"

#include <algorithm>
#include <cstdint>
#include <random>

class PartitionedConvolve
{
    // N.B. MIN_FFT_SIZE_LOG2 should never be smaller than 4, as below code assumes loop unroll of vectors (4 vals) by 4 (== 16 or 2^4)
    // MAX_FFT_SIZE_LOG2 is perhaps conservative right now, but it is easy to increase this if necessary
    
    static constexpr int MIN_FFT_SIZE_LOG2 = 5;
    static constexpr int MAX_FFT_SIZE_LOG2 = 20;
    
public:
    
    PartitionedConvolve(uintptr_t max_fft_size, uintptr_t max_length, uintptr_t offset, uintptr_t length)
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
        
        m_impulse_buffer.realp = allocate_aligned<float>(m_max_impulse_length * 4);
        m_impulse_buffer.imagp = m_impulse_buffer.realp + m_max_impulse_length;
        m_input_buffer.realp = m_impulse_buffer.imagp + m_max_impulse_length;
        m_input_buffer.imagp = m_input_buffer.realp + m_max_impulse_length;
        
        // Allocate fft and temporary buffers
        
        m_fft_buffers[0] = allocate_aligned<float>(max_fft_size * 6);
        m_fft_buffers[1] = m_fft_buffers[0] + max_fft_size;
        m_fft_buffers[2] = m_fft_buffers[1] + max_fft_size;
        m_fft_buffers[3] = m_fft_buffers[2] + max_fft_size;
        
        m_accum_buffer.realp = m_fft_buffers[3] + max_fft_size;
        m_accum_buffer.imagp = m_accum_buffer.realp + (max_fft_size >> 1);
        m_partition_temp.realp = m_accum_buffer.imagp + (max_fft_size >> 1);
        m_partition_temp.imagp = m_partition_temp.realp + (max_fft_size >> 1);
        
        hisstools_create_setup(&m_fft_setup, m_max_fft_size_log2);
    }
    
    ~PartitionedConvolve()
    {
        hisstools_destroy_setup(m_fft_setup);
        
        // FIX - try to do better here...
        
        deallocate_aligned(m_impulse_buffer.realp);
        deallocate_aligned(m_fft_buffers[0]);
    }
    
    // Non-moveable and copyable
    
    PartitionedConvolve(PartitionedConvolve& obj) = delete;
    PartitionedConvolve& operator = (PartitionedConvolve& obj) = delete;
    PartitionedConvolve(PartitionedConvolve&& obj) = delete;
    PartitionedConvolve& operator = (PartitionedConvolve&& obj) = delete;
    
    ConvolveError set_fft_size(uintptr_t fft_size)
    {
        uintptr_t fft_size_log2 = log2(fft_size);
        
        ConvolveError error = CONVOLVE_ERR_NONE;
        
        if (fft_size_log2 < MIN_FFT_SIZE_LOG2 || fft_size_log2 > m_max_fft_size_log2)
            return CONVOLVE_ERR_FFT_SIZE_OUT_OF_RANGE;
        
        if (fft_size != (uintptr_t(1) << fft_size_log2))
            error = CONVOLVE_ERR_FFT_SIZE_NON_POWER_OF_TWO;
        
        // Set fft variables iff the fft size has actually actually changed
        
        if (fft_size_log2 != m_fft_size_log2)
        {
            m_num_partitions = 0;
            m_fft_size_log2 = fft_size_log2;
        }
        
        m_rand_dist = std::uniform_int_distribution<uintptr_t>(0, (fft_size >> 1) - 1);
        
        return error;
    }
    
    ConvolveError set_length(uintptr_t length)
    {
        m_length = std::min(length, m_max_impulse_length);
        
        return (length > m_max_impulse_length) ? CONVOLVE_ERR_PARTITION_LENGTH_TOO_LARGE : CONVOLVE_ERR_NONE;
    }
    
    void set_offset(uintptr_t offset)
    {
        m_offset = offset;
    }
    
    void set_reset_offset(intptr_t offset = -1)
    {
        m_reset_offset = offset;
    }
    
    ConvolveError set(const float *input, uintptr_t length)
    {
        ConvolveError error = CONVOLVE_ERR_NONE;
        
        // FFT variables / attributes
        
        uintptr_t buffer_position;
        uintptr_t fft_size = get_fft_size();
        uintptr_t fft_size_halved = fft_size >> 1;
        
        // Partition variables
        
        float *buffer_temp_1 = (float *) m_partition_temp.realp;
        FFT_SPLIT_COMPLEX_F buffer_temp_2;
        
        uintptr_t num_partitions;
        
        // Calculate how much of the buffer to load
        
        length = (!input || length <= m_offset) ? 0 : length - m_offset;
        length = (m_length && m_length < length) ? m_length : length;
        
        if (length > m_max_impulse_length)
        {
            length = m_max_impulse_length;
            error = CONVOLVE_ERR_MEM_ALLOC_TOO_SMALL;
        }
        
        // Partition / load the impulse
        
        for (buffer_position = m_offset, buffer_temp_2 = m_impulse_buffer, num_partitions = 0; length > 0; buffer_position += fft_size_halved, num_partitions++)
        {
            // Get samples up to half the fft size
            
            uintptr_t numSamps = (length > fft_size_halved) ? fft_size_halved : length;
            length -= numSamps;
            
            // Get samples and zero pad
            
            std::copy_n(input + buffer_position, numSamps, buffer_temp_1);
            std::fill_n(buffer_temp_1 + numSamps, fft_size - numSamps, 0.f);
            
            // Do fft straight into position
            
            hisstools_rfft(m_fft_setup, buffer_temp_1, &buffer_temp_2, fft_size, m_fft_size_log2);
            offset_split_pointer(buffer_temp_2, buffer_temp_2, fft_size_halved);
        }
        
        m_num_partitions = num_partitions;
        reset();
        
        return error;
    }

    void reset()
    {
        m_reset_flag = true;
    }
    
    bool process(const float *in, float *out, uintptr_t num_samples)
    {
        FFT_SPLIT_COMPLEX_F ir_temp;
        FFT_SPLIT_COMPLEX_F in_temp;
        
        // Scheduling variables
        
        intptr_t num_partitions_to_do;
        
        // FFT variables
        
        uintptr_t fft_size = get_fft_size();
        uintptr_t fft_size_halved = fft_size >> 1;
        
        uintptr_t rw_counter = m_rw_counter;
        uintptr_t hop_mask = fft_size_halved - 1;
        
        uintptr_t samples_remaining = num_samples;
        
        if  (!m_num_partitions)
            return false;
        
        // If we need to reset everything we do that here - happens when the fft size changes, or a new buffer is loaded
        
        if (m_reset_flag)
        {
            // Reset fft buffers + accum buffer
            
            std::fill_n(m_fft_buffers[0], get_max_fft_size() * 5, 0.f);
            
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
            // Calculate how many IO samples to deal with this loop (depending on whether there is an fft to do before the end of the signal block)
            
            uintptr_t till_next_fft = (fft_size_halved - (rw_counter & hop_mask));
            uintptr_t loop_size = samples_remaining < till_next_fft ? samples_remaining : till_next_fft;
            uintptr_t hi_counter = (rw_counter + fft_size_halved) & (fft_size - 1);
            
            // Load input into buffer (twice) and output from the output buffer
            
            std::copy_n(in, loop_size, m_fft_buffers[0] + rw_counter);
            std::copy_n(in, loop_size, m_fft_buffers[1] + hi_counter);
            
            std::copy_n(m_fft_buffers[3] + rw_counter, loop_size, out);
            
            // Updates to pointers and counters
            
            samples_remaining -= loop_size;
            rw_counter += loop_size;
            in += loop_size;
            out += loop_size;
            
            bool fft_now = !(rw_counter & hop_mask);
            
            // Work loop and scheduling - this is where most of the convolution is done
            // How many partitions to do this block? (make sure that all partitions are done before we need to do the next fft)
            
            if (fft_now)
                num_partitions_to_do = (m_valid_partitions - m_partitions_done) - 1;
            else
                num_partitions_to_do = (((m_valid_partitions - 1) * (rw_counter & hop_mask)) / fft_size_halved) - m_partitions_done;
            
            while (num_partitions_to_do > 0)
            {
                // Calculate buffer wraparounds (if wraparound is in the middle of this set of partitions this loop will run again)
                
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
                using Vec = SIMDType<float, 4>;
                
                // Do the fft into the input buffer, add first partition (needed now), do ifft, scale and store (overlap-save)
                
                offset_split_pointer(in_temp, m_input_buffer, (m_input_position * fft_size_halved));
                hisstools_rfft(m_fft_setup, m_fft_buffers[(rw_counter == fft_size) ? 1 : 0], &in_temp, fft_size, m_fft_size_log2);
                process_partition(in_temp, m_impulse_buffer, m_accum_buffer, fft_size_halved);
                hisstools_rifft(m_fft_setup, &m_accum_buffer, m_fft_buffers[2], m_fft_size_log2);
                scale_store<Vec>(m_fft_buffers[3], m_fft_buffers[2], fft_size, (rw_counter != fft_size));
                
                // Clear accumulation buffer
                
                std::fill_n(m_accum_buffer.realp, fft_size_halved, 0.f);
                std::fill_n(m_accum_buffer.imagp, fft_size_halved, 0.f);
                
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
        
        return true;
    }

private:
    
    uintptr_t get_fft_size()      { return uintptr_t(1) << m_fft_size_log2; }
    uintptr_t get_max_fft_size()  { return uintptr_t(1) << m_max_fft_size_log2; }
    
    static void process_partition(FFT_SPLIT_COMPLEX_F in_1, FFT_SPLIT_COMPLEX_F in_2, FFT_SPLIT_COMPLEX_F out, uintptr_t num_bins)
    {
        using Vec = SIMDType<float, 4>;
        uintptr_t num_vecs = num_bins / Vec::size;
        
        Vec *i_real_1 = reinterpret_cast<Vec *>(in_1.realp);
        Vec *i_imag_1 = reinterpret_cast<Vec *>(in_1.imagp);
        Vec *i_real_2 = reinterpret_cast<Vec *>(in_2.realp);
        Vec *i_imag_2 = reinterpret_cast<Vec *>(in_2.imagp);
        Vec *o_real = reinterpret_cast<Vec *>(out.realp);
        Vec *o_imag = reinterpret_cast<Vec *>(out.imagp);
        
        float nyquist_1 = in_1.imagp[0];
        float nyquist_2 = in_2.imagp[0];
        
        // Do Nyquist Calculation and then zero these bins
        
        out.imagp[0] += nyquist_1 * nyquist_2;
        
        in_1.imagp[0] = 0.f;
        in_2.imagp[0] = 0.f;
        
        // Do other bins (loop unrolled)
        
        for (uintptr_t i = 0; i + 3 < num_vecs; i += 4)
        {
            *o_real++ += (i_real_1[i + 0] * i_real_2[i + 0]) - (i_imag_1[i + 0] * i_imag_2[i + 0]);
            *o_imag++ += (i_real_1[i + 0] * i_imag_2[i + 0]) + (i_imag_1[i + 0] * i_real_2[i + 0]);
            *o_real++ += (i_real_1[i + 1] * i_real_2[i + 1]) - (i_imag_1[i + 1] * i_imag_2[i + 1]);
            *o_imag++ += (i_real_1[i + 1] * i_imag_2[i + 1]) + (i_imag_1[i + 1] * i_real_2[i + 1]);
            *o_real++ += (i_real_1[i + 2] * i_real_2[i + 2]) - (i_imag_1[i + 2] * i_imag_2[i + 2]);
            *o_imag++ += (i_real_1[i + 2] * i_imag_2[i + 2]) + (i_imag_1[i + 2] * i_real_2[i + 2]);
            *o_real++ += (i_real_1[i + 3] * i_real_2[i + 3]) - (i_imag_1[i + 3] * i_imag_2[i + 3]);
            *o_imag++ += (i_real_1[i + 3] * i_imag_2[i + 3]) + (i_imag_1[i + 3] * i_real_2[i + 3]);
        }
        
        // Replace nyquist bins
        
        in_1.imagp[0] = nyquist_1;
        in_2.imagp[0] = nyquist_2;
    }

    ConvolveError set_max_fft_size(uintptr_t max_fft_size)
    {
        uintptr_t max_fft_size_log2 = log2(max_fft_size);
        
        ConvolveError error = CONVOLVE_ERR_NONE;
        
        if (max_fft_size_log2 > MAX_FFT_SIZE_LOG2)
        {
            error = CONVOLVE_ERR_FFT_SIZE_MAX_TOO_LARGE;
            max_fft_size_log2 = MAX_FFT_SIZE_LOG2;
        }
        
        if (max_fft_size_log2 && max_fft_size_log2 < MIN_FFT_SIZE_LOG2)
        {
            error = CONVOLVE_ERR_FFT_SIZE_MAX_TOO_SMALL;
            max_fft_size_log2 = MIN_FFT_SIZE_LOG2;
        }
        
        if (max_fft_size != (uintptr_t(1) << max_fft_size_log2))
            error = CONVOLVE_ERR_FFT_SIZE_MAX_NON_POWER_OF_TWO;
        
        m_max_fft_size_log2 = max_fft_size_log2;
        
        return error;
    }
    
    template <class T>
    static void scale_store(float *out, float *temp, uintptr_t fft_size, bool offset)
    {
        T *out_ptr = reinterpret_cast<T *>(out + (offset ? fft_size >> 1: 0));
        T *temp_ptr = reinterpret_cast<T *>(temp);
        T scale(1.f / static_cast<float>(fft_size << 2));
        
        for (uintptr_t i = 0; i < (fft_size / (T::size * 2)); i++)
            *(out_ptr++) = *(temp_ptr++) * scale;
    }
    
    static uintptr_t log2(uintptr_t value)
    {
        uintptr_t bit_shift = value;
        uintptr_t bit_count = 0;
        
        while (bit_shift)
        {
            bit_shift >>= 1U;
            bit_count++;
        }
        
        if (value == uintptr_t(1) << (bit_count - 1U))
            return bit_count - 1U;
        else
            return bit_count;
    }
    
    static void offset_split_pointer(FFT_SPLIT_COMPLEX_F &complex_1, const FFT_SPLIT_COMPLEX_F &complex_2, uintptr_t offset)
    {
        complex_1.realp = complex_2.realp + offset;
        complex_1.imagp = complex_2.imagp + offset;
    }
    
    // Parameters
    
    uintptr_t m_offset;
    uintptr_t m_length;
    uintptr_t m_max_impulse_length;
    
    // FFT variables
    
    FFT_SETUP_F m_fft_setup;
    
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
    
    float *m_fft_buffers[4];
    
    FFT_SPLIT_COMPLEX_F m_impulse_buffer;
    FFT_SPLIT_COMPLEX_F m_input_buffer;
    FFT_SPLIT_COMPLEX_F m_accum_buffer;
    FFT_SPLIT_COMPLEX_F m_partition_temp;
    
    // Flags
    
    intptr_t m_reset_offset;
    bool m_reset_flag;
    
    // Random number generation
    
    std::default_random_engine m_rand_gen;
    std::uniform_int_distribution<uintptr_t> m_rand_dist;
};
