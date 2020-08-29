
#ifndef WINDOWFUNCTIONS_HPP
#define WINDOWFUNCTIONS_HPP

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>
#include <vector>

template <class T, class Ref>
class WindowFunctions
{
    typedef void (*Func)(T, uint32_t, uint32_t);

    struct Function
    {
        Function(Ref reference, Func func) : mReference(reference), mFunc(func) {}
        
        Ref mReference;
        Func mFunc;
    };
    
public:
    
    void add(Ref reference, Func func) { mFunctions.push_back(Function(reference, func)); }
    
    bool calculate(Ref reference, T window, uint32_t windowSize, uint32_t generateSize)
    {
        for (size_t i = 0; i < mFunctions.size(); i++)
        {
            if (reference == mFunctions[i].mReference)
            {
                mFunctions[i].mFunc(window, windowSize, generateSize);
                return true;
            }
        }
        
        return false;
    }
    
private:
    
    std::vector<Function> mFunctions;
};

static inline double normalise(uint32_t x, uint32_t N)
{
    return static_cast<double>(x) / static_cast<double>(N);
}

namespace window_functions
{
    struct params
    {
        params(double pwr = 1.0, double A0 = 0.0, double A1 = 0.0, double A2 = 0.0, double A3 = 0.0, double A4 = 0.0)
        : a0(A0), a1(A1), a2(A2), a3(A3), a4(A4), power(pwr) {}
        
        double a0;
        double a1;
        double a2;
        double a3;
        double a4;
        
        double power;
    };
    
    static constexpr double pi()    { return M_PI; }
    static constexpr double pi2()  { return M_PI * 2.0; }
    static constexpr double pi4()  { return M_PI * 4.0; }
    static constexpr double pi6()  { return M_PI * 6.0; }
    static constexpr double pi8()  { return M_PI * 8.0; }
    
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
    static inline double sum(uint32_t i, uint32_t N, Ts... ops)
    {
        return sum(normalise(i, N), ops...);
    }
    
    static inline double rect(uint32_t i, uint32_t N, const params& p)
    {
        return 1.0;
    }
    
    static inline double triangle(uint32_t i, uint32_t N, const params& p)
    {
        return 1.0 - fabs(normalise(i, N) * 2.0 - 1.0);
    }
    
    static inline double trapezoid(uint32_t i, uint32_t N, const params& p)
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

    static inline double welch(uint32_t i, uint32_t N, const params& p)
    {
        const double x = 2.0 * normalise(i, N) - 1.0;
        return 1.0 - x * x;
    }

    static inline double cosine(uint32_t i, uint32_t N, const params& p)
    {
        return sin(pi() * normalise(i, N));
    }
    
    static inline double parzen(uint32_t i, uint32_t N, const params& p)
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
    
    static inline double cosine_sum_K1(uint32_t i, uint32_t N, const params& p)
    {
        return sum(i, N, constant(p.a0), cosx(1.0 - p.a0, pi2()));
    }
    
    static inline double hann(uint32_t i, uint32_t N, const params& p)
    {
        return cosine_sum_K1(i, N, 0.5);
    }
    
    static inline double hamming(uint32_t i, uint32_t N, const params& p)
    {
        // FIX - review
        //alpha is 0.54 or 25/46 or 0.543478260869565
        // see equiripple notes on wikipedia
        return cosine_sum_K1(i, N, 0.54);
    }
    
    static inline double blackman(uint32_t i, uint32_t N, const params& p)
    {
        return sum(i, N, constant(0.42), cosx(-0.5, pi2()), cosx(0.08, pi4()));
    }
    
    static inline double cosine_sum(uint32_t i, uint32_t N, const params& p)
    {
        return sum(i, N, constant(p.a0),
                   cosx(-p.a1, pi2()),
                   cosx(p.a2, pi4()),
                   cosx(-p.a3, pi6()),
                   cosx(-p.a4, pi8()));
    }

    static inline double izero(double x2)
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
    
    static inline double kaiser(uint32_t i, uint32_t N,  const params& p)
    {
        double x = 2.0 * normalise(i, N) - 1.0;
        return izero((1.0 - x * x) * p.a0 * p.a0) * p.a1;
    }
    
    static inline double tukey(uint32_t i, uint32_t N, const params& p)
    {
        // FIX - look at normalisation here...
        return 0.5 - 0.5 * cos(trapezoid(i, N, p) * pi());
    }
    
    // Abstract generator
    
    template <double Func(uint32_t, uint32_t, const params& p), class T>
    void window_generate(T window, uint32_t N, uint32_t size, const params& p)
    {
        // FIX - Do power function - specialise for sqrt / integer powers / 1?
        for (uint32_t i = 0; i < size; i++)
            window[i] = Func(i, N, p);
    }
    
    // Specific generators
    
    template <class T>
    void window_rect(T window, uint32_t N, uint32_t size, const params& p)
    {
        window_generate<window_functions::rect>(window, N, size, p);
    }
    
    template <class T>
    void window_triangle(T window, uint32_t N, uint32_t size, const params& p)
    {
        window_generate<window_functions::triangle>(window, N, size, p);
    }
    
    template <class T>
    void window_trapezoid(T window, uint32_t N, uint32_t size, const params& p)
    {
        window_generate<window_functions::trapezoid>(window, N, size, p);
    }
    
    template <class T>
    void window_welch(T window, uint32_t N, uint32_t size, const params& p)
    {
        window_generate<window_functions::welch>(window, N, size, p);
    }
    
    template <class T>
    void window_parzen(T window, uint32_t N, uint32_t size, const params& p)
    {
        window_generate<window_functions::parzen>(window, N, size, p);
    }
    
    template <class T>
    void window_cosine(T window, uint32_t N, uint32_t size, const params& p)
    {
        window_generate<window_functions::cosine>(window, N, size, p);
    }
    
    template <class T>
    void window_hann(T window, uint32_t N, uint32_t size, const params& p)
    {
        window_generate<window_functions::hann>(window, N, size, p);
    }
    
    template <class T>
    void window_hamming(T window, uint32_t N, uint32_t size, const params& p)
    {
        window_generate<window_functions::hamming>(window, N, size, p);
    }
    
    template <class T>
    void window_blackman(T window, uint32_t N, uint32_t size, const params& p)
    {
        window_generate<window_functions::blackman>(window, N, size, p);
    }
    
    template <class T>
    void window_cosine_sum(T window, uint32_t N, uint32_t size, const params& p)
    {
        window_generate<window_functions::cosine_sum>(window, N, size, p);
    }
    
    template <class T>
    void window_kaiser(T window, uint32_t N, uint32_t size, const params& p)
    {
        params p1(p.power, p.a0, 1.0 / izero(p.a0 * p.a0));
        
        window_generate<window_functions::tukey>(window, N, size, p1);
    }
    
    template <class T>
    void window_tukey(T window, uint32_t N, uint32_t size, const params& p)
    {
        params p1(p.power, p.a0 * 0.5, 1.0 - (p.a0 * 0.5));
        
        window_generate<window_functions::tukey>(window, N, size, p1);
    }
}


#define WINDOW_PI            3.14159265358979323846
#define WINDOW_TWOPI        6.28318530717958647692
#define WINDOW_THREEPI        9.42477796076937971538
#define WINDOW_FOURPI        12.56637061435817295384
#define WINDOW_FIVEPI       15.707963267947966
#define WINDOW_SIXPI        18.84955592153875943076

// HOW TO REPRESENT ALL OF THESE?

template <class T>
void window_exact_blackman(T window, uint32_t windowSize, uint32_t generateSize)
{
    for (uint32_t i = 0; i < generateSize; i++)
		window[i] = 0.42659071 - (0.49656062 * cos(WINDOW_TWOPI * normalise(i, windowSize))) + (0.07684867 * cos(WINDOW_FOURPI * normalise(i, windowSize)));
}

template <class T>
void window_blackman_harris_61(T window, uint32_t windowSize, uint32_t generateSize)
{
	for (uint32_t i = 0; i < generateSize; i++)
		window[i] = (0.44959 - 0.49364 * cos(WINDOW_TWOPI * normalise(i, windowSize)) + 0.05677 * cos(WINDOW_FOURPI * normalise(i, windowSize)));
}

template <class T>
void window_blackman_harris_67(T window, uint32_t windowSize, uint32_t generateSize)
{
	for (uint32_t i = 0; i < generateSize; i++)
		window[i] = (0.42323 - 0.49755 * cos(WINDOW_TWOPI * normalise(i, windowSize)) + 0.07922 * cos(WINDOW_FOURPI * normalise(i, windowSize)));
}

template <class T>
void window_blackman_harris_74(T window, uint32_t windowSize, uint32_t generateSize)
{
	for (uint32_t i = 0; i < generateSize; i++)
		window[i] = (0.402217f - 0.49703f * cos(WINDOW_TWOPI * normalise(i, windowSize)) + 0.09892f * cos(WINDOW_FOURPI * normalise(i, windowSize)) - 0.00188 * cos(WINDOW_SIXPI * normalise(i, windowSize)));
}

// 92dB
template <class T>
void window_blackman_harris(T window, uint32_t windowSize, uint32_t generateSize)
{
	for (uint32_t i = 0; i < generateSize; i++)
		window[i] = 0.35875 - (0.48829 * cos(WINDOW_TWOPI * normalise(i, windowSize))) + (0.14128 * cos(WINDOW_FOURPI * normalise(i, windowSize))) - (0.01168 * cos(WINDOW_SIXPI * normalise(i, windowSize)));
}

template <class T>
void window_flat_top(T window, uint32_t windowSize, uint32_t generateSize)
{
	for (uint32_t i = 0; i < generateSize; i++)
		window[i] = 0.2810639 - (0.5208972 * cos(WINDOW_TWOPI * normalise(i, windowSize))) + (0.1980399 * cos(WINDOW_FOURPI * normalise(i, windowSize)));
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
/*
template <class T>
class IndexedWindowFunctions : public WindowFunctions <T, uint32_t>
{
public:
    
    enum WindowTypes { kWindowRect, kWindowTriangle, kWindowHann, kWindowHamming, kWindowCosine, kWindowBlackman, kWindowBlackman62, kWindowBlackman70, kWindowBlackman74, kWindowBlackman92, kWindowBlackmanHarris, kWindowFlatTop, kWindowKaiser };
    
    IndexedWindowFunctions()
    {
        WindowFunctions<T, uint16_t>::add(kWindowRect, window_rect);
        WindowFunctions<T, uint16_t>::add(kWindowTriangle, window_triangle);

        WindowFunctions<T, uint16_t>::add(kWindowHann, window_hann);
        WindowFunctions<T, uint16_t>::add(kWindowHamming, window_hamming);
        WindowFunctions<T, uint16_t>::add(kWindowCosine, window_cosine);
        
        WindowFunctions<T, uint16_t>::add(kWindowBlackman, window_blackman);
        WindowFunctions<T, uint16_t>::add(kWindowBlackman62, window_blackman_62);
        WindowFunctions<T, uint16_t>::add(kWindowBlackman70, window_blackman_70);
        WindowFunctions<T, uint16_t>::add(kWindowBlackman74, window_blackman_62);
        WindowFunctions<T, uint16_t>::add(kWindowBlackman92, window_blackman_92);
        WindowFunctions<T, uint16_t>::add(kWindowBlackmanHarris, window_blackman_harris);

        WindowFunctions<T, uint16_t>::add(kWindowFlatTop, window_flat_top);
        
        WindowFunctions<T, uint16_t>::add(kWindowKaiser, window_kaiser);
    }
};
*/

#endif

