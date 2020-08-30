
#ifndef WINDOWFUNCTIONS_HPP
#define WINDOWFUNCTIONS_HPP

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>
#include <vector>

// Naming/coefficients can larged be verified in:
// Nuttall, A. (1981). Some windows with very good sidelobe behavior.
// IEEE Transactions on Acoustics, Speech, and Signal Processing, 29(1), 84-91.

namespace window_functions
{
    // Parameter struct
    
    struct params
    {
        constexpr params(double A0 = 0.0, double A1 = 0.0, double A2 = 0.0, double A3 = 0.0, double A4 = 0.0, double exp = 1.0)
        : a0(A0), a1(A1), a2(A2), a3(A3), a4(A4), exponent(exp) {}
        
        constexpr params(double *param_array, int N, double exp)
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
    
    // Constexpr functions for convenience
    
    constexpr double pi()   { return M_PI; }
    constexpr double pi2()  { return M_PI * 2.0; }
    constexpr double pi4()  { return M_PI * 4.0; }
    constexpr double pi6()  { return M_PI * 6.0; }
    constexpr double pi8()  { return M_PI * 8.0; }
    
    constexpr double div(int x, int y)
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
            return coefficient ? coefficient * cos(x * multiplier) : 0.0;
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
    inline double sum(double x, T op)
    {
        return op(x);
    }
    
    template <typename T, typename ...Ts>
    inline double sum(double x, T op, Ts... ops)
    {
        return sum(x, op) + sum(x, ops...);
    }
    
    template <typename ...Ts>
    inline double sum(uint32_t i, uint32_t N, Ts... ops)
    {
        return sum(normalise(i, N), ops...);
    }
    
    // Specific window calculations
    
    inline double rect(uint32_t i, uint32_t N, const params& p)
    {
        return 1.0;
    }
    
    inline double triangle(uint32_t i, uint32_t N, const params& p)
    {
        return 1.0 - fabs(normalise(i, N) * 2.0 - 1.0);
    }
    
    inline double trapezoid(uint32_t i, uint32_t N, const params& p)
    {
        double a = p.a0;
        double b = p.a1;
        
        if (b < a)
            std::swap(a, b);
        
        const double x = normalise(i, N);
        
        if (x <= a)
            return x / a;
        
        if (x >= b)
            return 1.0 - ((x - b) / (1.0 - b));
        
        return 1.0;
    }

    inline double welch(uint32_t i, uint32_t N, const params& p)
    {
        const double x = 2.0 * normalise(i, N) - 1.0;
        return 1.0 - x * x;
    }

    inline double sine(uint32_t i, uint32_t N, const params& p)
    {
        return sin(pi() * normalise(i, N));
    }
    
    inline double parzen(uint32_t i, uint32_t N, const params& p)
    {
        // FIX - check scaling
        const double N2 = static_cast<double>(N) * 0.5;
        
        auto w0 = [&](double x)
        {
            x = fabs(x) / N2;
            
            if (x > 0.5)
            {
                double v = (1.0 - x);
                return 2.0 * v * v * v;
            }
            
            return 1.0 - 6.0 * x * x * (1.0 - x);
        };
        
        return w0(static_cast<double>(i) - N2);
    }
    
    inline double two_term(uint32_t i, uint32_t N, const params& p)
    {
        return sum(i, N, constant(p.a0), cosx(-(1.0 - p.a0), pi2()));
    }
    
    inline double three_term(uint32_t i, uint32_t N, const params& p)
    {
        return sum(i, N, constant(p.a0), cosx(-p.a1, pi2()), cosx(p.a2, pi4()));
    }
    
    inline double four_term(uint32_t i, uint32_t N, const params& p)
    {
        return sum(i, N, constant(p.a0), cosx(-p.a1, pi2()), cosx(p.a2, pi4()), cosx(-p.a3, pi6()));
    }
    
    inline double hann(uint32_t i, uint32_t N, const params& p)
    {
        //return sum(i, N, constant(0.5), cosx(-0.5, pi2()));
        return two_term(i, N, params(0.5));
    }
    
    inline double hamming(uint32_t i, uint32_t N, const params& p)
    {
        // FIX - review
        //alpha is 0.54 or 25/46 or 0.543478260869565
        // see equiripple notes on wikipedia
        //return sum(i, N, constant(0.54), cosx(-0.46, pi2()));
        return two_term(i, N, params(0.54));
    }
    
    inline double blackman(uint32_t i, uint32_t N, const params& p)
    {
        return three_term(i, N, params(0.42, -0.5, 0.08));
    }
    
    inline double exact_blackman(uint32_t i, uint32_t N, const params& p)
    {
        const params pb(div(7938, 18608), div(9240, 18608), div(1430, 18608));
        return three_term(i, N, pb);
    }
    
    inline double blackman_harris_62dB(uint32_t i, uint32_t N, const params& p)
    {
        return three_term(i, N, params(0.44959, 0.49364, 0.05677));
    }
    
    inline double blackman_harris_71dB(uint32_t i, uint32_t N, const params& p)
    {
        return three_term(i, N, params(0.42323, 0.49755, 0.07922));
    }
    
    inline double blackman_harris_74dB(uint32_t i, uint32_t N, const params& p)
    {
        return four_term(i, N, params(0.402217, 0.49703, 0.09892, 0.00188));
    }
    
    inline double blackman_harris_92dB(uint32_t i, uint32_t N, const params& p)
    {
        return four_term(i, N, params(0.35875, 0.48829, 0.14128, 0.01168));
    }
    
    inline double nuttall_1st_64dB(uint32_t i, uint32_t N, const params& p)
    {
        return three_term(i, N, params(0.40897, 0.5, 0.09103));
    }
    
    inline double nuttall_1st_93dB(uint32_t i, uint32_t N, const params& p)
    {
        return four_term(i, N, params(0.355768, 0.487396, 0.144232, 0.012604));
    }
    
    inline double nuttall_3rd_47dB(uint32_t i, uint32_t N, const params& p)
    {
        return three_term(i, N, params(0.375, 0.5, 0.125));
    }
    
    inline double nuttall_3rd_83dB(uint32_t i, uint32_t N, const params& p)
    {
        return four_term(i, N, params(0.338946, 0.481973, 0.161054, 0.018027));
    }
    
    inline double nuttall_5th_61dB(uint32_t i, uint32_t N, const params& p)
    {
        return four_term(i, N, params(0.3125, 0.46875, 0.1875, 0.03125));
    }
    
    inline double nuttall_minimal_71dB(uint32_t i, uint32_t N, const params& p)
    {
        return three_term(i, N, params(0.4243801, 0.4973406, 0.0782793));
    }
    
    inline double nuttall_minimal_98dB(uint32_t i, uint32_t N, const params& p)
    {
        return four_term(i, N, params(0.3635819, 0.4891775, 0.1365995, 0.0106411));
    }
    
    inline double cosine_sum(uint32_t i, uint32_t N, const params& p)
    {
        return sum(i, N, constant(p.a0),
                   cosx(-p.a1, pi2()),
                   cosx(p.a2, pi4()),
                   cosx(-p.a3, pi6()),
                   cosx(-p.a4, pi8()));
    }

    inline double izero(double x2)
    {
        double term = 1.0;
        double bessel = 1.0;
        
        // N.B. - loop until term is zero for maximum accuracy
        
        for (unsigned long i = 1; term > std::numeric_limits<double>::epsilon(); i++)
        {
            const double i2 = static_cast<double>(i * i);
            term = term * x2 * (1.0 / (4.0 * i2));
            bessel += term;
        }
        
        return bessel;
    }
    
    inline double kaiser(uint32_t i, uint32_t N,  const params& p)
    {
        double x = 2.0 * normalise(i, N) - 1.0;
        return izero((1.0 - x * x) * p.a0 * p.a0) * p.a1;
    }
    
    inline double tukey(uint32_t i, uint32_t N, const params& p)
    {
        // FIX - look at normalisation here...
        return 0.5 - 0.5 * cos(trapezoid(i, N, p) * pi());
    }
    
    // Abstract generator
    
    template <window_func Func, class T>
    void inline generate(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        auto sq = [&](double x) { return x * x; };
        auto cb = [&](double x) { return x * x * x; };
        auto qb = [&](double x) { return sq(sq(x)); };
        auto toType = [](double x) { return static_cast<T>(x); };
        
        if (p.exponent == 1.0)
        {
            for (uint32_t i = begin; i < end; i++)
                window[i] = toType(Func(i, N, p));
        }
        else if (p.exponent == 0.5)
        {
            for (uint32_t i = begin; i < end; i++)
                window[i] = toType(std::sqrt(Func(i, N, p)));
        }
        else if (p.exponent == 2.0)
        {
            for (uint32_t i = begin; i < end; i++)
                window[i] = toType(sq(Func(i, N, p)));
        }
        else if (p.exponent == 3.0)
        {
            for (uint32_t i = begin; i < end; i++)
                window[i] = toType(cb(Func(i, N, p)));
        }
        else if (p.exponent == 4.0)
        {
            for (uint32_t i = begin; i < end; i++)
                window[i] = toType(qb(Func(i, N, p)));
        }
        else if (p.exponent > 0 && p.exponent == std::floor(p.exponent))
        {
            // FIX range
            
            int exponent = static_cast<int>(p.exponent);
            
            for (uint32_t i = begin; i < end; i++)
                window[i] = toType(std::pow(Func(i, N, p), exponent));
        }
        else
        {
            for (uint32_t i = begin; i < end; i++)
                window[i] = toType(std::pow(Func(i, N, p), p.exponent));
        }
    }
    
    // Specific window generators
    
    template <class T>
    void window_rect(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        generate<window_functions::rect>(window, N, begin, end, p);
    }
    
    template <class T>
    void window_triangle(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        generate<window_functions::triangle>(window, N, begin, end, p);
    }
    
    template <class T>
    void window_trapezoid(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        generate<window_functions::trapezoid>(window, N, begin, end, p);
    }
    
    template <class T>
    void window_welch(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        generate<window_functions::welch>(window, N, begin, end, p);
    }
    
    template <class T>
    void window_parzen(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        generate<window_functions::parzen>(window, N, begin, end, p);
    }
    
    template <class T>
    void window_sine(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        generate<window_functions::sine>(window, N, begin, end, p);
    }
    
    template <class T>
    void window_hann(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        generate<window_functions::hann>(window, N, begin, end, p);
    }
    
    template <class T>
    void window_hamming(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        generate<window_functions::hamming>(window, N, begin, end, p);
    }
    
    template <class T>
    void window_blackman(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        generate<window_functions::blackman>(window, N, begin, end, p);
    }
    
    template <class T>
    void window_exact_blackman(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        generate<window_functions::exact_blackman>(window, N, begin, end, p);
    }
    
    template <class T>
    void window_blackman_harris_62dB(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        generate<window_functions::blackman_harris_62dB>(window, N, begin, end, p);
    }
    
    template <class T>
    void window_blackman_harris_71dB(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        generate<window_functions::blackman_harris_71dB>(window, N, begin, end, p);
    }
    
    template <class T>
    void window_blackman_harris_74dB(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        generate<window_functions::blackman_harris_74dB>(window, N, begin, end, p);
    }
    
    template <class T>
    void window_blackman_harris_92dB(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        generate<window_functions::blackman_harris_92dB>(window, N, begin, end, p);
    }
    
    template <class T>
    void window_nuttall_1st_64dB(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        generate<window_functions::nuttall_1st_64dB>(window, N, begin, end, p);
    }
    
    template <class T>
    void window_nuttall_1st_93dB(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        generate<window_functions::nuttall_1st_93dB>(window, N, begin, end, p);
    }
    
    template <class T>
    void window_nuttall_3rd_47dB(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        generate<window_functions::nuttall_3rd_47dB>(window, N, begin, end, p);
    }
    
    template <class T>
    void window_nuttall_3rd_83dB(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        generate<window_functions::nuttall_3rd_83dB>(window, N, begin, end, p);
    }
    
    template <class T>
    void window_nuttall_5th_61dB(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        generate<window_functions::nuttall_5th_61dB>(window, N, begin, end, p);
    }
    
    template <class T>
    void window_nuttall_minimal_71dB(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        generate<window_functions::nuttall_minimal_71dB>(window, N, begin, end, p);
    }
    
    template <class T>
    void window_nuttall_minimal_98dB(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        generate<window_functions::nuttall_minimal_98dB>(window, N, begin, end, p);
    }

    template <class T>
    void window_cosine_sum(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        generate<window_functions::cosine_sum>(window, N, begin, end, p);
    }
    
    template <class T>
    void window_kaiser(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        params p1(p.a0, 1.0 / izero(p.a0 * p.a0));
        p1.exponent = p.exponent;
        
        generate<window_functions::kaiser>(window, N, begin, end, p1);
    }
    
    template <class T>
    void window_tukey(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        params p1(p.a0 * 0.5, 1.0 - (p.a0 * 0.5));
        p1.exponent = p.exponent;

        generate<window_functions::tukey>(window, N, begin, end, p1);
    }
    
    template <class T, window_generator<T> ...gens>
    struct indexed_generator
    {
        void operator()(size_t type, T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
        {
            return generators[type](window, N, begin, end, p);
        }
        
        window_generator<T> *generators[sizeof...(gens)] = { gens... };
    };

}

// FIX - below
/*
#define WINDOW_PI            3.14159265358979323846
#define WINDOW_TWOPI        6.28318530717958647692
#define WINDOW_THREEPI        9.42477796076937971538
#define WINDOW_FOURPI        12.56637061435817295384
#define WINDOW_FIVEPI       15.707963267947966
#define WINDOW_SIXPI        18.84955592153875943076

// HOW TO REPRESENT ALL OF THESE?

template <class T>
void window_flat_top(T window, uint32_t windowSize, uint32_t generateSize)
{
	for (uint32_t i = 0; i < generateSize; i++)
        window[i] = 0.2810639 - (0.5208972 * cos(WINDOW_TWOPI * window_functions::normalise(i, windowSize))) + (0.1980399 * cos(WINDOW_FOURPI * window_functions::normalise(i, windowSize)));
}


// The below is incorrect!!!!

template <class T>
void window_multisine_tapers(T window, uint32_t windowSize, uint32_t generateSize, uint32_t num_tapers)
{
	for (uint32_t j = 0; j < generateSize; j++)
		window[j] = 0.0;
	
	for (uint32_t i = 0; i < num_tapers; i++)
	{
		for (uint32_t j = 0; j < generateSize; j++)
			window[j] += sin(WINDOW_PI * (double) (i + 1) * (double) (j + 1) / (double) (windowSize + 1));
	}
}

template <class T>
void window_msinetaper1(T window, uint32_t windowSize, uint32_t generateSize)
{
	window_multisine_tapers(window, windowSize, generateSize, 1);
}

template <class T>
void window_msinetaper2(T window, uint32_t windowSize, uint32_t generateSize)
{
	window_multisine_tapers(window, windowSize, generateSize, 2);
}

template <class T>
void window_msinetaper3(T window, uint32_t windowSize, uint32_t generateSize)
{
	window_multisine_tapers(window, windowSize, generateSize, 3);
}

template <class T>
void window_msinetaper4(T window, uint32_t windowSize, uint32_t generateSize)
{
	window_multisine_tapers(window, windowSize, generateSize, 4);
}

template <class T>
void window_msinetaper5(T window, uint32_t windowSize, uint32_t generateSize)
{
	window_multisine_tapers(window, windowSize, generateSize, 5);
}

template <class T>
void window_msinetaper6(T window, uint32_t windowSize, uint32_t generateSize)
{
	window_multisine_tapers(window, windowSize, generateSize, 6);
}
*/

#endif

