
#include "HISSTools_IR_Manipulation.h"

#include <algorithm>
#include <cmath>


template <typename T>
struct BaseType
{};

template <>
struct BaseType <FFT_SPLIT_COMPLEX_D> { using type = double; };

template <>
struct BaseType <FFT_SPLIT_COMPLEX_F> { using type = float; };


template <typename T, typename Op>
void ir_real_operation(T *out, uintptr_t fft_size, Op op)
{
    typename BaseType<T>::type *r_out = out->realp;
    typename BaseType<T>::type *i_out = out->imagp;
    
    typename BaseType<T>::type temp(0);
    
    // DC and Nyquist
    
    op(r_out + 0, &temp, 0);
    op(i_out + 0, &temp, fft_size >> 1);
    
    // Other bins
    
    for (uintptr_t i = 1; i < (fft_size >> 1); i++)
        op(r_out + i, i_out + i, i);
}

template <typename T, typename Op>
void ir_real_operation(T *out, const T *in, uintptr_t fft_size, Op op)
{
    const typename BaseType<T>::type *r_in = in->realp;
    const typename BaseType<T>::type *i_in = in->imagp;
    typename BaseType<T>::type *r_out = out->realp;
    typename BaseType<T>::type *i_out = out->imagp;
    
    typename BaseType<T>::type temp1(0);
    typename BaseType<T>::type temp2(0);

    // DC and Nyquist
    
    op(r_out + 0, &temp1, r_in[0], temp1, 0);
    op(i_out + 0, &temp2, i_in[0], temp2, fft_size >> 1);
    
    // Other bins
    
    for (uintptr_t i = 1; i < (fft_size >> 1); i++)
        op(r_out + i, i_out + i, r_in[i], i_in[i], i);
}

void ir_copy(FFT_SPLIT_COMPLEX_D *out, FFT_SPLIT_COMPLEX_D *in, uintptr_t fft_size)
{
    struct copy
    {
        void operator()(double *r_out, double *i_out, double r_in, double i_in, uintptr_t i)
        {
            *r_out = r_in;
            *i_out = i_in;
        }
    };
    
    ir_real_operation(out, in, fft_size, copy());
}

void ir_spike(FFT_SPLIT_COMPLEX_D *out, uintptr_t fft_size, double spike_position)
{
    struct spike
    {
        spike(double spike_position, double fft_size)
        {
            spike_constant = ((long double) (2.0 * M_PI)) * (fft_size - spike_position) / fft_size;
        }
        
        void operator()(double *r_out, double *i_out, uintptr_t i)
        {
            long double phase = spike_constant * i;
            *r_out = std::cos(phase);
            *i_out = std::sin(phase);
        }
        
        long double spike_constant;
    };
    
    ir_real_operation(out, fft_size, spike(spike_position, fft_size));
}

void ir_delay(FFT_SPLIT_COMPLEX_D *out, FFT_SPLIT_COMPLEX_D *in, uintptr_t fft_size, double delay)
{
    struct delay_calc
    {
        delay_calc(double delay, double fft_size)
        {
            delay_constant = ((long double) (2.0 * M_PI)) * -delay / fft_size;
        }
        
        void operator()(double *r_out, double *i_out, double r_in, double i_in, uintptr_t i)
        {
            using complex = std::complex<double>;
            
            long double phase = delay_constant * i;
            complex c = complex(r_in, i_in) * complex(std::cos(phase), std::sin(phase));
            
            r_out[i] = std::real(c);
            i_out[i] = std::imag(c);
        }
        
        long double delay_constant;
    };
    
    if (delay == 0.0)
    {
        if (in != out)
            ir_copy(out, in, fft_size);
        
        return;
    }
    
    ir_real_operation(out, in, fft_size, delay_calc(delay, fft_size));
}

void ir_time_reverse(FFT_SPLIT_COMPLEX_D *out, FFT_SPLIT_COMPLEX_D *in, uintptr_t fft_size)
{
    struct conjugate
    {
        void operator()(double *r_out, double *i_out, double r_in, double i_in, uintptr_t i)
        {
            *r_out =  r_in;
            *i_out = -i_in;
        }
    };
    
    ir_real_operation(out, in, fft_size, conjugate());
}

void minimum_phase_components(FFT_SETUP_D setup, FFT_SPLIT_COMPLEX_D *out, FFT_SPLIT_COMPLEX_D *in, uintptr_t fft_size)
{
    // FIX - what is this value?
    
    uintptr_t fft_size_log2;
    
    // Take Log of Power Spectrum
    
    struct log_power
    {
        void operator()(double *r_out, double *i_out, double r_in, double i_in, uintptr_t i)
        {
            static double min_power = std::pow(10.0, -300.0 / 10.0);
            double power = r_in * r_in + i_in * i_in;
            
            // FIX - scaling - 0.25 is for the real FFT effect
            *r_out = 0.5 * log(0.25 * std::max(power, min_power));
            *i_out = 0.0;
        }
    };
    
    ir_real_operation(out, out, fft_size, log_power());

    // Do Real iFFT
    
    hisstools_rifft(setup, out, fft_size_log2);
    
    // Double Causal Values / Zero Non-Casual Values / Scale All Remaining
    
    // N.B. - doubling is implicit because the real FFT will double the result
    //      - (0.5 multiples needed where no doubling should take place)
    
    double scale = 1.0 / fft_size;
    
    out->realp[0] *= 0.5 * scale;
    out->imagp[0] *= scale;
    
    for (uintptr_t i = 1; i < (fft_size >> 2); i++)
    {
        out->realp[i] *= scale;
        out->imagp[i] *= scale;
    }
    
    out->realp[fft_size >> 2] *= 0.5 * scale;
    out->imagp[fft_size >> 2] = 0.0;
    
    for (unsigned long i = (fft_size >> 2) + 1; i < (fft_size >> 1); i++)
    {
        out->realp[i] = 0.0;
        out->imagp[i] = 0.0;
    }
    
    // Forward Real FFT (here there is a scaling issue to consider)
    
    hisstools_rfft(setup, out, fft_size_log2);
}

void convert_to_minimum_phase(FFT_SETUP_D setup, FFT_SPLIT_COMPLEX_D *out, FFT_SPLIT_COMPLEX_D *in, uintptr_t fft_size)
{
    minimum_phase_components(setup, out, in, fft_size);
    
    struct complex_exponential
    {
        void operator()(double *r_out, double *i_out, double r_in, double i_in, uintptr_t i)
        {
            using complex = std::complex<double>;
            const complex c = std::exp(complex(r_in, i_in));
            
            *r_out = std::real(c);
            *i_out = std::imag(c);
        }
    };
    
    ir_real_operation(out, out, fft_size, complex_exponential());
}

void convert_to_mixed_phase(FFT_SETUP_D setup, FFT_SPLIT_COMPLEX_D *out, FFT_SPLIT_COMPLEX_D *in, uintptr_t fft_size, double phase, bool zero_center)
{
    minimum_phase_components(setup, out, in, fft_size);

    phase = phase < 0 ? 0.0 : phase;
    phase = phase > 1 ? 1.0 : phase;
    
    struct phase_interpolate
    {
        
        // N.B. - induce a delay of -1 sample for anything over linear end avoid wraparound end the first sample
        
        phase_interpolate(double phase, double fft_size, bool zero_center)
        {
            min_factor = 1.0 - (2.0 * phase);
            lin_factor = zero_center ? 0.0 : (phase <= 0.5 ? -(2 * M_PI * phase) : (-2.0 * M_PI * (phase - 1.0 / fft_size)));
        }
        
        void operator()(double *r_out, double *i_out, double r_in, double i_in, uintptr_t i)
        {
            const double amp = std::exp(r_in);
            const double interp_phase = lin_factor * i + min_factor * i_in;
            
            r_out[i] = amp * std::cos(interp_phase);
            i_out[i] = amp * std::sin(interp_phase);
        }
        
        double min_factor;
        double lin_factor;
    };
    
    ir_real_operation(out, out, fft_size, phase_interpolate(phase, fft_size, zero_center));
}

void convert_to_zero_phase(FFT_SPLIT_COMPLEX_D *out, FFT_SPLIT_COMPLEX_D *in, uintptr_t fft_size)
{
    struct amplitude
    {
        void operator()(double *r_out, double *i_out, double r_in, double i_in, uintptr_t i)
        {
            *r_out = std::sqrt(r_in * r_in + i_in * i_in);
            *i_out = 0.0;
        }
    };
    
    ir_real_operation(out, in, fft_size, amplitude());
}

void ir_phase(FFT_SETUP_D setup, FFT_SPLIT_COMPLEX_D *out, FFT_SPLIT_COMPLEX_D *in, uintptr_t fft_size, double phase, bool zero_center)
{
    if (phase == 0.0)
    {
        convert_to_minimum_phase(setup, out, in, fft_size);
    }
    else if (phase == 0.5)
    {
        convert_to_zero_phase(out, in, fft_size);
        if (!zero_center)
            ir_delay(out, out, fft_size, fft_size >> 1);
    }
    else if (phase == 1.0)
    {
        convert_to_minimum_phase(setup, out, in, fft_size);
        ir_time_reverse(out, out, fft_size);
        if (!zero_center)
            ir_delay(out, out, fft_size, -1.0);
    }
    else
    {
        convert_to_mixed_phase(setup, out, in, fft_size, phase, zero_center);
    }
}
