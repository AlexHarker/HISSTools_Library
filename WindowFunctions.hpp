
#ifndef WINDOWFUNCTIONS_HPP
#define WINDOWFUNCTIONS_HPP

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>
#include <vector>

#define WINDOW_PI			3.14159265358979323846
#define WINDOW_TWOPI		6.28318530717958647692
#define WINDOW_THREEPI		9.42477796076937971538
#define WINDOW_FOURPI		12.56637061435817295384
#define WINDOW_FIVEPI       15.707963267947966
#define WINDOW_SIXPI		18.84955592153875943076

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

template <double Func(uint32_t, uint32_t), class T>
void window_generate(T window, uint32_t N, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
        window[i] = Func(i, N);
}

struct window_functions
{
    struct params
    {
        double c0 = 0.0;
        double c1 = 0.0;
        double c2 = 0.0;
        double c3 = 0.0;
        double c4 = 0.0;
        double c5 = 0.0;
        double c6 = 0.0;
    };
    
    static constexpr double pi()    { return M_PI; }
    static constexpr double pi_2()  { return M_PI * 2.0; }
    static constexpr double pi_3()  { return M_PI * 3.0; }
    static constexpr double pi_4()  { return M_PI * 4.0; }
    static constexpr double pi_5()  { return M_PI * 5.0; }
    static constexpr double pi_6()  { return M_PI * 6.0; }
    
    struct constant
    {
        constant(double v) : value(v) {}
        
        inline double operator()(double x) { return value; }
        
        const double value;
    };
    
    struct cos_x
    {
        cos_x(double c, double mul)
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
    static inline double summed(uint32_t i, uint32_t N, Ts... ops)
    {
        return sum(normalise(i, N), ops...);
    }
    
    static inline double rect(uint32_t i, uint32_t N)
    {
        return 1.0;
    }
    
    static inline double triangle(uint32_t i, uint32_t N)
    {
        return 1.0 - fabs(normalise(i, N) * 2.0 - 1.0);
    }

    static inline double welch(uint32_t i, uint32_t N)
    {
        const double x = 2.0 * normalise(i, N) - 1.0;
        return 1.0 - x * x;
    }

    static inline double cosine(uint32_t i, uint32_t N)
    {
        return sin(WINDOW_PI * normalise(i, N));
    }
    
    static inline double parzen(uint32_t i, uint32_t N)
    {
        const double N2 = static_cast<double>(N) * 0.5;
        const double L2 = static_cast<double>(N + 1) * 0.5;
        
        auto w0 = [&](double x)
        {
            x = fabs(x) / L2;
            
            if (x > 0.5)
            {
                double v = (1.0 - x);
                return 2.0 * v * v * v;
            }
            
            return 1.0 - 6.0 * x * x * (1.0 - x);
        };
        
        return w0(static_cast<double>(i) - N2);
    }
    
    static inline double hann(uint32_t i, uint32_t N)
    {
        return summed(i, N, constant(0.5), cos_x(-0.5, pi_2()));
    }
    
    static inline double cosine_sum(uint32_t i, uint32_t N, const params& p)
    {
        return summed(i, N, constant(p.c0),
                      cos_x(p.c1, pi()),
                      cos_x(p.c2, pi_2()),
                      cos_x(p.c3, pi_3()),
                      cos_x(p.c4, pi_4()),
                      cos_x(p.c5, pi_5()),
                      cos_x(p.c6, pi_6()));
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
    
    static inline double kaiser(uint32_t i, uint32_t N, double alpha, double b_recip)
    {
        double x = 2.0 * normalise(i, N) - 1.0;
        return izero((1.0 - x * x) * alpha * alpha) * b_recip;
    }
};

template <class T>
void window_rect(T window, uint32_t N, uint32_t size)
{
    window_generate<window_functions::rect>(window, N, size);
}

template <class T>
void window_triangle(T window, uint32_t N, uint32_t size)
{
    window_generate<window_functions::triangle>(window, N, size);
}

template <class T>
void window_welch(T window, uint32_t N, uint32_t size)
{
    window_generate<window_functions::welch>(window, N, size);
}

template <class T>
void window_parzen(T window, uint32_t N, uint32_t size)
{
    window_generate<window_functions::parzen>(window, N, size);
}

template <class T>
void window_cosine(T window, uint32_t N, uint32_t size)
{
    window_generate<window_functions::cosine>(window, N, size);
}

/////////////////////////


template <class T>
void window_kaiser(T window, uint32_t windowSize, uint32_t generateSize)
{
    double alpha_bessel_recip;
    double new_term;
    double x_sq;
    double b_func;
    double temp;
    long j;
    
    // First find bessel function of alpha
    
    x_sq = 46.24;
    new_term = 0.25 * x_sq;
    b_func = 1.0;
    j = 2;
    
    // Gives maximum accuracy
    
    while (new_term)
    {
        b_func += new_term;
        alpha_bessel_recip = (1.0 / (4.0 * (double) j * (double) j));
        new_term = new_term * x_sq * (1.0 / (4.0 * (double) j * (double) j));
        j++;
    }
    
    alpha_bessel_recip = 1 / b_func;
    
    // Now create kaiser window
    
    // FIX - might not work 100%
    
    for (uint32_t i = 0; i < generateSize; i++)
    {
        temp = ((2.0 * (double) i) - ((double) windowSize - 1.0));
        temp = temp / windowSize;
        temp *= temp;
        x_sq = (1 - temp) * 46.24;
        new_term = 0.25 * x_sq;
        b_func = 1;
        j = 2;
        
        // Gives maximum accuracy
        
        while (new_term)
        {
            b_func += new_term;
            new_term = new_term * x_sq * (1.0 / (4.0 * (double) j * (double) j));
            j++;
        }
        window[i] = b_func * alpha_bessel_recip;
    }
}

template <class T>
void window_kaiser(T window, uint32_t N, uint32_t size, double alpha)
{
    double b_recip = 1.0 / window_functions::izero(alpha * alpha);
    
    for (uint32_t i = 0; i < size; i++)
        window[i] = window_functions::kaiser(i, N, alpha, b_recip);
}

template <class T>
void window_cosine_sum(T window, uint32_t N, uint32_t size, const window_functions::params& p)
{
    for (uint32_t i = 0; i < size; i++)
        window[i] = window_functions::cosine_sum(i, N, p);
    //window_generate<window_functions::cosine_sum>(window, N, size, p);
}

template <class T>
void window_cosine_sum2(T window, uint32_t N, uint32_t size, const window_functions::params& p)
{
    for (uint32_t i = 0; i < size; i++)
    {
        const double x = normalise(i, N);
        window[i] = p.c0
        //+ p.c1 * cos(x * WINDOW_PI)
        + p.c2 * cos(x * WINDOW_TWOPI)
        //+ p.c3 * cos(x * WINDOW_THREEPI)
        //+ p.c4 * cos(x * WINDOW_FOURPI)
        //+ p.c5 * cos(x * WINDOW_FIVEPI)
        //+ p.c6 * cos(x * WINDOW_SIXPI)
        ;
    }
}

template <class T>
void window_hann2(T window, uint32_t N, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
    {
        const double x = normalise(i, N);
        window[i] = 0.5 - 0.5 * cos(x * WINDOW_TWOPI);
    }
}

template <class T>
void window_hann(T window, uint32_t N, uint32_t size)
{
    // c0 is 0.5
    // c2 is -0.5
    
    window_generate<window_functions::hann>(window, N, size);
}

template <class T>
void window_hamming(T window, uint32_t N, uint32_t size)
{
    // c0 is 0.54 or 25/46 or 0.543478260869565
    // c2 is -(1.0 - c0) or -0.46 or -21/46 -0.456521739130435
    // see equiripple notes on wikipedia
    
	for (uint32_t i = 0; i < size; i++)
		window[i] = 0.54347826 - (0.45652174 * cos(WINDOW_TWOPI * normalise(i, N)));
}

template <class T>
void window_blackman(T window, uint32_t windowSize, uint32_t generateSize)
{
	for (uint32_t i = 0; i < generateSize; i++)
		window[i] = 0.42659071 - (0.49656062 * cos(WINDOW_TWOPI * normalise(i, windowSize))) + (0.07684867 * cos(WINDOW_FOURPI * normalise(i, windowSize)));
}

template <class T>
void window_blackman_62(T window, uint32_t windowSize, uint32_t generateSize)
{
	for (uint32_t i = 0; i < generateSize; i++)
		window[i] = (0.44859f - 0.49364f * cos(WINDOW_TWOPI * normalise(i, windowSize)) + 0.05677f * cos(WINDOW_FOURPI * normalise(i, windowSize)));
}

template <class T>
void window_blackman_70(T window, uint32_t windowSize, uint32_t generateSize)
{
	for (uint32_t i = 0; i < generateSize; i++)
		window[i] = (0.42323f - 0.49755f * cos(WINDOW_TWOPI * normalise(i, windowSize)) + 0.07922f * cos(WINDOW_FOURPI * normalise(i, windowSize)));
}

template <class T>
void window_blackman_74(T window, uint32_t windowSize, uint32_t generateSize)
{
	for (uint32_t i = 0; i < generateSize; i++)
		window[i] = (0.402217f - 0.49703f * cos(WINDOW_TWOPI * normalise(i, windowSize)) + 0.09892f * cos(WINDOW_FOURPI * normalise(i, windowSize)) - 0.00188 * cos(WINDOW_THREEPI * normalise(i, windowSize)));
}

template <class T>
void window_blackman_92(T window, uint32_t windowSize, uint32_t generateSize)
{
	for (uint32_t i = 0; i < generateSize; i++)
		window[i] = (0.35875f - 0.48829f * cos(WINDOW_TWOPI * normalise(i, windowSize)) + 0.14128f * cos(WINDOW_FOURPI * normalise(i, windowSize)) - 0.01168 * cos(WINDOW_THREEPI * normalise(i, windowSize)));
}

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

#endif

