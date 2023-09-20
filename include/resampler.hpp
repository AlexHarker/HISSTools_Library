
#ifndef HISSTOOLS_LIBRARY_RESAMPLER_HPP
#define HISSTOOLS_LIBRARY_RESAMPLER_HPP

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>

#include "simd_support.hpp"
#include "window.hpp"
#include "namespace.hpp"

// N.B. - clean up for other usage (different phases / consider other windows etc.)

HISSTOOLS_LIBRARY_NAMESPACE_START()

template <class T = double, class IO = float, bool Approx = true>
class resampler
{
    
public:
    
    resampler()
    {
        set_filter(10, 16384, 0.455, 11.0);
    }
    
    template <bool B = Approx, typename std::enable_if<B, int>::type = 0>
    std::vector<IO> process(IO* input, uintptr_t in_length, double in_sr, double out_sr, double transpose_ratio = 1.0)
    {
        // Calculate overall rate change
        
        const double rate = std::fabs(transpose_ratio * in_sr / out_sr);
        
        // Resample
        
        uint32_t numerator, denominator;
        rate_as_ratio(rate, numerator, denominator);
        
        uintptr_t out_length = std::ceil(divide(in_length * denominator, numerator));
        
        std::vector<IO> output(out_length);

        if (numerator == 1 && denominator == 1)
            std::copy_n(input, out_length, output.data());
        else
            resample_ratio(output, input, in_length, out_length, numerator, denominator);
        
        return output;
    }
    
    template <bool B = Approx, typename std::enable_if<!B, int>::type = 0>
    std::vector<IO> process(IO* input, uintptr_t in_length, double in_sr, double out_sr, double transpose_ratio = 1.0)
    {
        // Calculate overall rate change
        
        const double rate = std::fabs(transpose_ratio * in_sr / out_sr);
        
        // Resample
        
        uintptr_t out_length = std::ceil(static_cast<double>(in_length) / rate);
        
        std::vector<IO> output(out_length);

        if (rate == 1.0)
            std::copy_n(input, out_length, output.data());
        else
            resample_rate(output, input, in_length, out_length, rate);
        
        return output;
    }
    
    // Make filter
    
    void set_filter(uint32_t num_zeros, uint32_t num_points, double cf, double alpha)
    {
        auto izero = [](double x)
        {
            return window::izero(x);
        };
        
        assert(num_zeros != 0 && "resampler: number of zeros cannot be zero");
        assert(num_points != 0 && "resampler: number of points per zero cannot be zero");
        
        m_num_zeros = num_zeros;
        m_num_points = num_points;
        
        alpha = alpha <= 0.0 ? 1.0 : alpha;
        
        uint32_t half_filter_length = num_zeros * num_points;
        m_filter.resize(half_filter_length + 2);
        
        // Calculate second half of filter only
        // First find the reciprocal of the bessel function of alpha
        
        const double bessel_recip = 1.0 / izero(alpha * alpha);
        
        // Limit Value
        
        m_filter[0] = static_cast<T>(2.0 * cf);
        
        for (uint32_t i = 1; i < half_filter_length + 1; i++)
        {
            // Kaiser Window multiplied by sinc filter
            
            const double x = divide(i, half_filter_length);
            const double k = izero((1.0 - x * x) * alpha * alpha) * bessel_recip;
            
            m_filter[i] = static_cast<T>(k * sinc_filter(divide(i, num_points), cf));
        }
        
        // Guard sample for linear interpolation (N.B. - max read index should be half_filter_length)
        
        m_filter[half_filter_length + 1] = T(0);
    }

private:

    template <bool B = Approx, typename std::enable_if<B, int>::type = 0>
    void resample_ratio(std::vector<IO>& output, IO* input, uintptr_t in_length, uintptr_t nsamps, uint32_t num, 
                                                                                                   uint32_t denom)
    {
        uint32_t filter_offset;
        uint32_t filter_length;
        
        // Create relevant temporaries for resampling
        
        double* filter_set = create_filter_set(num, denom, filter_length, filter_offset);
        auto padded_input = copy_input_padded(input, in_length, filter_offset, filter_length - filter_offset);
        
        // Resample
        
        for (uintptr_t i = 0, offset = 0; i < nsamps; offset += num)
        {
            const double* filter = filter_set;
            
            for (uint32_t j = 0; i < nsamps && j < denom; i++, j++, filter += filter_length)
            {
                uintptr_t input_offset = offset + (j * num / denom);
                output[i] = apply_filter(filter, padded_input.data() + input_offset, filter_length);
            }
        }
        
        // Deallocate temporary memory
        
        deallocate_aligned(filter_set);
    }
    
    template <bool B = Approx, typename std::enable_if<!B, int>::type = 0>
    void resample_rate(std::vector<IO>& output, IO* input, uintptr_t in_length, uintptr_t nsamps, double rate)
    {
        const double per_sample_recip = rate > 1.0 ? m_num_zeros * rate : m_num_zeros;
        const double mul = rate < 1.0 ? rate : 1.0;
        
        const uint32_t pad_length = static_cast<uint32_t>(std::ceil(per_sample_recip)) + 1;

        auto padded_input = copy_input_padded(input, in_length, pad_length, pad_length);

        // Resample
        
        for (uintptr_t i = 0; i < nsamps; i++)
            output[i] = mul * calculate_sample(padded_input.data(), (i * rate) + pad_length, per_sample_recip);
    }
    
    template <bool B = Approx, typename std::enable_if<B, int>::type = 0>
    double* create_filter_set(uint32_t numerator, uint32_t denominator, uint32_t& length, uint32_t& offset)
    {
        const double per_sample = numerator > denominator ? divide(denominator, numerator) : 1.0;
        const double mul = numerator > denominator ? 1.0 : divide(numerator, denominator);

        length = static_cast<uint32_t>((m_num_zeros << 1) / per_sample) + 1;
        offset = length >> 1;
        length += (4 - (length % 4));
        
        double* filter_set = allocate_aligned<double>(denominator * length);
        
        for (uint32_t i = 0, n = 0; i < denominator; i++, n += numerator)
        {
            double* filter = filter_set + i * length;

            while (n >= denominator)
                n -= denominator;
            
            for (uint32_t j = 0; j < length; j++)
            {
                const double position = std::fabs(per_sample * (j - (divide(n, denominator) + offset)));
                *filter++ = position <= m_num_zeros ? mul * get_filter_value(position) : 0.0;
            }
        }
        
        return filter_set;
    }
    
    template <bool B = Approx, typename std::enable_if<B, int>::type = 0>
    static inline double apply_filter(const T* a, const IO* b, uintptr_t N)
    {
        constexpr int vec_size_o = (simd_limits<T>::max_size > 4) ? 4 : simd_limits<T>::max_size;
        constexpr int vec_size_i = (simd_limits<IO>::max_size >= 4) ? 4 : 1;
        
        using out_vector_type = sized_vector<T , vec_size_o, 4>;
        using in_vector_type = sized_vector<IO, vec_size_i, 4>;
        
        out_vector_type sum(0.0);
        T results[4];
        uintptr_t i;
        
        for (i = 0; i + 7 < N; i += 8)
        {
            sum += out_vector_type(a + i + 0) * out_vector_type(in_vector_type(b + i + 0));
            sum += out_vector_type(a + i + 4) * out_vector_type(in_vector_type(b + i + 4));
        }
        for (; i + 3 < N; i += 4)
            sum += out_vector_type(a + i + 0) * out_vector_type(in_vector_type(b + i + 0));
        
        sum.store(results);
        
        return results[0] + results[1] + results[2] + results[3];
    }
    
    /*
        // N.B. - The code above is equivalent to the following scalar code
     
        double apply_filter(const T* a, const IO* b, uintptr_t N)
        {
            T sum = 0.0;
     
            for (uintptr_t i = 0; i < N; i++)
                sum += a[i] * static_cast<T>(b[i]);
     
            return sum;
        }
     */
    
    template <bool B = Approx, typename std::enable_if<B, int>::type = 0>
    void rate_as_ratio(double rate, uint32_t& numerator, uint32_t& denominator)
    {
        uint32_t cf[256];
        
        double npart = std::fabs(rate);
        
        numerator = 1;
        denominator = 1;
        long length = 0;
        
        for (; npart < 1000 && npart > 0 && length < 256; length++)
        {
            const double ipart = floor(npart);
            npart = npart - ipart;
            npart = npart ? 1 / npart : 0;
            
            cf[length] = static_cast<uint32_t>(ipart);
        }
        
        for (long i = length - 1; i >= 0; i--)
        {
            numerator = cf[i];
            denominator = 1;
            
            for (long j = i - 1; j >= 0 && denominator < 1000; j--)
            {
                std::swap(numerator, denominator);
                numerator = numerator + (denominator * cf[j]);
            }
            
            if (denominator < 1000)
                break;
        }
    }
    
    // Calculate one sample 

    template <bool B = Approx, typename std::enable_if<!B, int>::type = 0>
    double calculate_sample(const IO* input, double offset, double per_sample_recip)
    {
        double per_sample = 1.0 / per_sample_recip;
        
        uint32_t i_offset = std::floor(offset);
        
        const IO* samples = input + i_offset;
                
        double position = (offset - i_offset) * per_sample;
        double sum = 0.0;

        // Get to first relevant sample

        for (; (position + per_sample) < 1.0; position += per_sample)
            samples--;
        
        // Do left wing of the filter
                
        for (; position >= 0.0; position -= per_sample)
            sum += *samples++ * get_filter_value(position * m_num_zeros);
        
        // Do right wing of the filter
        
        for (position = -position; position <= 1.0; position += per_sample)
            sum += *samples++ * get_filter_value(position * m_num_zeros);
        
        return sum;
    }
    
    // Get a filter value from a position 0-nzero on the RHS wing (translate for LHS)
    // N.B. position **MUST** be in the range 0 to nzero inclusive
    
    double get_filter_value(double position)
    {
        const double index = m_num_points * position;
        const int32_t idx = static_cast<int32_t>(index);
        const double fract = index - idx;
        
        const double lo = m_filter[idx];
        const double hi = m_filter[idx + 1];
        
        return lo + fract * (hi - lo);
    }
    
    static inline double sinc_filter(double position, double cf)
    {
        const double sinc_arg = M_PI * position;
        return std::sin(2.0 * cf * sinc_arg) / sinc_arg;
    }
    
    template <class U, class V>
    static double divide(U x, V N)
    {
        return static_cast<double>(x) / static_cast<double>(N);
    }
    
    std::vector<IO> copy_input_padded(const IO* input, uintptr_t in_length, uint32_t pad_start, uint32_t pad_end)
    {
        std::vector<IO> padded_input(in_length + pad_start + pad_end);
        
        std::fill_n(padded_input.data() + 0, pad_start, IO(0));
        std::copy_n(input, in_length, padded_input.data() + pad_start);
        std::fill_n(padded_input.data() + pad_start + in_length, pad_end, IO(0));
        
        return padded_input;
    }
    
    std::vector<T> m_filter;
    uint32_t m_num_zeros;
    uint32_t m_num_points;
};

HISSTOOLS_LIBRARY_NAMESPACE_END()

#endif /* HISSTOOLS_LIBRARY_RESAMPLER_HPP */
