
#ifndef HISSTOOLS_LIBRARY_WINDOW_HPP
#define HISSTOOLS_LIBRARY_WINDOW_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "namespace.hpp"

HISSTOOLS_LIBRARY_NAMESPACE_START()

// Coefficients (and the basis for naming) can largely be found in:
//
// Nuttall, A. (1981). Some windows with very good sidelobe behavior.
// IEEE Transactions on Acoustics, Speech, and Signal Processing, 29(1), 84-91.
//
// Similar windows / additional flat-top windows from:
//
// Heinzel, G., Rüdiger, A., & Schilling, R. (2002).
// Spectrum and spectral density estimation by the Discrete Fourier transform (DFT),
// including a comprehensive list of window functions and some new flat-top windows.

class window
{
    // Allow the rasampler class access
    
    template <class, class, bool> friend class resampler;
    
public:

    // Parameter struct
    
    struct params
    {
        constexpr params(double A0 = 0, double A1 = 0, double A2 = 0, double A3 = 0, double A4 = 0, double exp = 1)
        : a0(A0), a1(A1), a2(A2), a3(A3), a4(A4), exponent(exp) {}
        
        constexpr params(const double* param_array, int N, double exp)
        : a0(N > 0 ? param_array[0] : 0.0)
        , a1(N > 1 ? param_array[1] : 0.0)
        , a2(N > 2 ? param_array[2] : 0.0)
        , a3(N > 3 ? param_array[3] : 0.0)
        , a4(N > 4 ? param_array[4] : 0.0)
        , exponent(exp) {}
     
        double a0;
        double a1;
        double a2;
        double a3;
        double a4;
        
        double exponent;
    };
    
    using window_func = double(uint32_t, uint32_t, const params&);

    template <class T>
    using window_generator = void(T*, uint32_t, uint32_t, uint32_t, const params&);

    // Specific window generators
    
    template <class T>
    static void rect(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<rect, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void triangle(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<triangle, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void trapezoid(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<trapezoid, false>(window, n, begin, end, p);
    }
    
    template <class T>
    static void welch(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<welch, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void parzen(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<parzen, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void sine(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<sine, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void sine_taper(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        params p1(std::round(p.a0));
        p1.exponent = p.exponent;
        
        generate<sine_taper, false>(window, n, begin, end, p1);
    }
    
    template <class T>
    static void tukey(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        params p1(p.a0 * 0.5, 1.0 - (p.a0 * 0.5));
        p1.exponent = p.exponent;
        
        generate<tukey, true>(window, n, begin, end, p1);
    }
    
    template <class T>
    static void kaiser(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        params p1(p.a0, 1.0 / izero(p.a0 * p.a0));
        p1.exponent = p.exponent;
        
        generate<kaiser, true>(window, n, begin, end, p1);
    }
    
    template <class T>
    static void cosine_2_term(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<cosine_2_term, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void cosine_3_term(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<cosine_3_term, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void cosine_4_term(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<cosine_4_term, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void cosine_5_term(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<cosine_5_term, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void hann(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<hann, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void hamming(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<hamming, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void blackman(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<blackman, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void exact_blackman(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<exact_blackman, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void blackman_harris_62dB(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<blackman_harris_62dB, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void blackman_harris_71dB(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<blackman_harris_71dB, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void blackman_harris_74dB(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<blackman_harris_74dB, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void blackman_harris_92dB(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<blackman_harris_92dB, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void nuttall_1st_64dB(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<nuttall_1st_64dB, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void nuttall_1st_93dB(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<nuttall_1st_93dB, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void nuttall_3rd_47dB(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<nuttall_3rd_47dB, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void nuttall_3rd_83dB(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<nuttall_3rd_83dB, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void nuttall_5th_61dB(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<nuttall_5th_61dB, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void nuttall_minimal_71dB(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<nuttall_minimal_71dB, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void nuttall_minimal_98dB(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<nuttall_minimal_98dB, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void ni_flat_top(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<ni_flat_top, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void hp_flat_top(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<hp_flat_top, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void stanford_flat_top(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<stanford_flat_top, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void heinzel_flat_top_70dB(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<heinzel_flat_top_70dB, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void heinzel_flat_top_90dB(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<heinzel_flat_top_90dB, true>(window, n, begin, end, p);
    }
    
    template <class T>
    static void heinzel_flat_top_95dB(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        generate<heinzel_flat_top_95dB, true>(window, n, begin, end, p);
    }
    
    template <class T, window_generator<T> ...Gens>
    struct indexed_generator
    {
        void operator()(size_t type, T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
        {
            return generators[type](window, n, begin, end, p);
        }
        
        window_generator<T>* get(size_t type) { return generators[type]; }
        
        window_generator<T>* generators[sizeof...(Gens)] = { Gens... };
    };

private:
    
    // Constexpr functions for convenience
    
    static constexpr double pi()   { return M_PI; }
    static constexpr double pi2()  { return M_PI * 2.0; }
    static constexpr double pi4()  { return M_PI * 4.0; }
    static constexpr double pi6()  { return M_PI * 6.0; }
    static constexpr double pi8()  { return M_PI * 8.0; }
    
    static constexpr double div(int x, int y)
    {
        return static_cast<double>(x) / static_cast<double>(y);
    }
    
    // Operators for cosine sum windows
    
    struct constant
    {
        constant(double v) : value(v) {}
        
        inline double operator()(double x) { return value; }
        
        const double value;
    };
    
    struct cosx
    {
        cosx(double c, double mul)
        : coefficient(c), multiplier(mul) {}
        
        inline double operator()(double x)
        {
            return coefficient * cos(x * multiplier);
        }
        
        const double coefficient;
        const double multiplier;
    };
    
    // Normalisation helper
    
    static inline double normalise(uint32_t x, uint32_t N)
    {
        return static_cast<double>(x) / static_cast<double>(N);
    }
    
    // Summing functions for cosine sum windows
    
    template <typename T>
    static inline double sum(double x, T op)
    {
        return op(x);
    }
    
    template <typename T, typename ...Ts>
    static inline double sum(double x, T op, Ts... ops)
    {
        return sum(x, op) + sum(x, ops...);
    }
    
    template <typename ...Ts>
    static inline double sum(uint32_t i, uint32_t n, Ts... ops)
    {
        return sum(normalise(i, n), ops...);
    }
    
    // Specific window calculations
    
    static inline double rect(uint32_t i, uint32_t n, const params& p)
    {
        return 1.0;
    }
    
    static inline double triangle(uint32_t i, uint32_t n, const params& p)
    {
        return 1.0 - fabs(normalise(i, n) * 2.0 - 1.0);
    }
    
    static inline double trapezoid(uint32_t i, uint32_t n, const params& p)
    {
        double a = p.a0;
        double b = p.a1;
        
        if (b < a)
            std::swap(a, b);
        
        const double x = normalise(i, n);
        
        if (x < a)
            return x / a;
        
        if (x > b)
            return 1.0 - ((x - b) / (1.0 - b));
        
        return 1.0;
    }
    
    static inline double welch(uint32_t i, uint32_t n, const params& p)
    {
        const double x = 2.0 * normalise(i, n) - 1.0;
        return 1.0 - x * x;
    }
    
    static inline double parzen(uint32_t i, uint32_t n, const params& p)
    {
        const double n2 = static_cast<double>(n) * 0.5;
        
        auto w0 = [&](double x)
        {
            x = fabs(x) / n2;
            
            if (x > 0.5)
            {
                double v = (1.0 - x);
                return 2.0 * v * v * v;
            }
            
            return 1.0 - 6.0 * x * x * (1.0 - x);
        };
        
        return w0(static_cast<double>(i) - n2);
    }
    
    static inline double sine(uint32_t i, uint32_t n, const params& p)
    {
        return sin(pi() * normalise(i, n));
    }
    
    static inline double sine_taper(uint32_t i, uint32_t n, const params& p)
    {
        return sin(p.a0 * pi() * normalise(i, n));
    }
    
    static inline double tukey(uint32_t i, uint32_t n, const params& p)
    {
        return 0.5 - 0.5 * cos(trapezoid(i, n, p) * pi());
    }
    
    static inline double izero(double x2)
    {
        double term = 1.0;
        double bessel = 1.0;
        
        // N.B. - loop based on epsilon for maximum accuracy
        
        for (unsigned long i = 1; term > std::numeric_limits<double>::epsilon(); i++)
        {
            const double i2 = static_cast<double>(i * i);
            term = term * x2 * (1.0 / (4.0 * i2));
            bessel += term;
        }
        
        return bessel;
    }
    
    static inline double kaiser(uint32_t i, uint32_t n, const params& p)
    {
        double x = 2.0 * normalise(i, n) - 1.0;
        return izero((1.0 - x * x) * p.a0 * p.a0) * p.a1;
    }
    
    static inline double cosine_2_term(uint32_t i, uint32_t n, const params& p)
    {
        return sum(i, n, constant(p.a0), cosx(-(1.0 - p.a0), pi2()));
    }
    
    static inline double cosine_3_term(uint32_t i, uint32_t n, const params& p)
    {
        return sum(i, n, constant(p.a0), cosx(-p.a1, pi2()), cosx(p.a2, pi4()));
    }
    
    static inline double cosine_4_term(uint32_t i, uint32_t n, const params& p)
    {
        return sum(i, n, constant(p.a0), cosx(-p.a1, pi2()), cosx(p.a2, pi4()), cosx(-p.a3, pi6()));
    }
    
    static inline double cosine_5_term(uint32_t i, uint32_t n, const params& p)
    {
        return sum(i, n, constant(p.a0),
                    cosx(-p.a1, pi2()),
                    cosx(p.a2, pi4()),
                    cosx(-p.a3, pi6()),
                    cosx(p.a4, pi8()));
    }
    
    static inline double hann(uint32_t i, uint32_t n, const params& p)
    {
        return cosine_2_term(i, n, params(0.5));
    }
    
    static inline double hamming(uint32_t i, uint32_t n, const params& p)
    {
        // N.B. here we use approx alpha of 0.54 (not 25/46 or 0.543478260869565)
        // see equiripple notes on wikipedia
        
        return cosine_2_term(i, n, params(0.54));
    }
    
    static inline double blackman(uint32_t i, uint32_t n, const params& p)
    {
        return cosine_3_term(i, n, params(0.42, 0.5, 0.08));
    }
    
    static inline double exact_blackman(uint32_t i, uint32_t n, const params& p)
    {
        const params pb(div(7938, 18608), div(9240, 18608), div(1430, 18608));
        return cosine_3_term(i, n, pb);
    }
    
    static inline double blackman_harris_62dB(uint32_t i, uint32_t n, const params& p)
    {
        return cosine_3_term(i, n, params(0.44959, 0.49364, 0.05677));
    }
    
    static inline double blackman_harris_71dB(uint32_t i, uint32_t n, const params& p)
    {
        return cosine_3_term(i, n, params(0.42323, 0.49755, 0.07922));
    }
    
    static inline double blackman_harris_74dB(uint32_t i, uint32_t n, const params& p)
    {
        return cosine_4_term(i, n, params(0.402217, 0.49703, 0.09892, 0.00188));
    }
    
    static inline double blackman_harris_92dB(uint32_t i, uint32_t n, const params& p)
    {
        return cosine_4_term(i, n, params(0.35875, 0.48829, 0.14128, 0.01168));
    }
    
    static inline double nuttall_1st_64dB(uint32_t i, uint32_t n, const params& p)
    {
        return cosine_3_term(i, n, params(0.40897, 0.5, 0.09103));
    }
    
    static inline double nuttall_1st_93dB(uint32_t i, uint32_t n, const params& p)
    {
        return cosine_4_term(i, n, params(0.355768, 0.487396, 0.144232, 0.012604));
    }
    
    static inline double nuttall_3rd_47dB(uint32_t i, uint32_t n, const params& p)
    {
        return cosine_3_term(i, n, params(0.375, 0.5, 0.125));
    }
    
    static inline double nuttall_3rd_83dB(uint32_t i, uint32_t n, const params& p)
    {
        return cosine_4_term(i, n, params(0.338946, 0.481973, 0.161054, 0.018027));
    }
    
    static inline double nuttall_5th_61dB(uint32_t i, uint32_t n, const params& p)
    {
        return cosine_4_term(i, n, params(0.3125, 0.46875, 0.1875, 0.03125));
    }
    
    static inline double nuttall_minimal_71dB(uint32_t i, uint32_t n, const params& p)
    {
        return cosine_3_term(i, n, params(0.4243801, 0.4973406, 0.0782793));
    }
    
    static inline double nuttall_minimal_98dB(uint32_t i, uint32_t n, const params& p)
    {
        return cosine_4_term(i, n, params(0.3635819, 0.4891775, 0.1365995, 0.0106411));
    }
    
    static inline double ni_flat_top(uint32_t i, uint32_t n, const params& p)
    {
        return cosine_3_term(i, n, params(0.2810639, 0.5208972, 0.1980399));
    }
    
    static inline double hp_flat_top(uint32_t i, uint32_t n, const params& p)
    {
        return cosine_4_term(i, n, params(1.0, 1.912510941, 1.079173272, 0.1832630879));
    }
    
    static inline double stanford_flat_top(uint32_t i, uint32_t n, const params& p)
    {
        return cosine_5_term(i, n, params(1.0, 1.939, 1.29, 0.388, 0.028));
    }
    
    static inline double heinzel_flat_top_70dB(uint32_t i, uint32_t n, const params& p)
    {
        return cosine_4_term(i, n, params(1.0, 1.90796, 1.07349, 0.18199));
    }
    
    static inline double heinzel_flat_top_90dB(uint32_t i, uint32_t n, const params& p)
    {
        return cosine_5_term(i, n, params(1.0, 1.942604, 1.340318, 0.440811, 0.043097));
    }
    
    static inline double heinzel_flat_top_95dB(uint32_t i, uint32_t n, const params& p)
    {
        return cosine_5_term(i, n, params(1.0, 1.9383379, 1.3045202, 0.4028270, 0.0350665));
    }
    
    // Abstract generator
    
    template <window_func Func, bool Symmetric, class T>
    static inline void generate(T* window, uint32_t n, uint32_t begin, uint32_t end, const params& p)
    {
        constexpr int max_int = std::numeric_limits<int>().max();
        
        auto sq = [&](double x) { return x * x; };
        auto cb = [&](double x) { return x * x * x; };
        auto qb = [&](double x) { return sq(sq(x)); };
        auto to_type = [](double x) { return static_cast<T>(x); };
        
        const T* copy_first = nullptr;
        const T* copy_last = nullptr;
        T* out_first = nullptr;
        
        end = std::min(n + 1, end);
        begin = std::min(begin, end);
        
        if (Symmetric)
        {
            uint32_t M = (n/2) + 1;
            
            if (begin < M && end > M + 1)
            {
                uint32_t begin_n = M - begin;
                uint32_t end_n = (end - begin) - begin_n;
                
                if (begin_n > end_n)
                {
                    copy_last = window + (n+1)/2 - begin;
                    copy_first = copy_last - end_n;
                    out_first = window + begin_n;
                    end = M;
                }
                else
                {
                    copy_first = window + (n+1)/2 + 1 - begin;
                    copy_last = copy_first + (begin_n - 1);
                    out_first = window;
                    window += M - (begin + 1);
                    begin = M - 1;
                }
            }
        }
        
        if (p.exponent == 1.0)
        {
            for (uint32_t i = begin; i < end; i++)
                *window++ = to_type(Func(i, n, p));
        }
        else if (p.exponent == 0.5)
        {
            for (uint32_t i = begin; i < end; i++)
                *window++ = to_type(std::sqrt(Func(i, n, p)));
        }
        else if (p.exponent == 2.0)
        {
            for (uint32_t i = begin; i < end; i++)
                *window++ = to_type(sq(Func(i, n, p)));
        }
        else if (p.exponent == 3.0)
        {
            for (uint32_t i = begin; i < end; i++)
                *window++ = to_type(cb(Func(i, n, p)));
        }
        else if (p.exponent == 4.0)
        {
            for (uint32_t i = begin; i < end; i++)
                *window++ = to_type(qb(Func(i, n, p)));
        }
        else if (p.exponent > 0 && p.exponent <= max_int && p.exponent == std::floor(p.exponent))
        {
            int exponent = static_cast<int>(p.exponent);
            
            for (uint32_t i = begin; i < end; i++)
                *window++ = to_type(std::pow(Func(i, n, p), exponent));
        }
        else
        {
            for (uint32_t i = begin; i < end; i++)
                *window++ = to_type(std::pow(Func(i, n, p), p.exponent));
        }
        
        if (Symmetric && out_first)
            std::reverse_copy(copy_first, copy_last, out_first);
    }
};

HISSTOOLS_LIBRARY_NAMESPACE_END()

#endif /* HISSTOOLS_LIBRARY_WINDOW_HPP */
