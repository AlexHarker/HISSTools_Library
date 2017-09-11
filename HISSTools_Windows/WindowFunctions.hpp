
#include <stdint>
#include <vector>

#define WINDOW_PI			3.14159265358979323846
#define WINDOW_TWOPI		6.28318530717958647692
#define WINDOW_THREEPI		9.42477796076937971538
#define WINDOW_FOURPI		12.56637061435817295384
#define WINDOW_SIXPI		18.84955592153875943076

<template class T, class Ref>
class WindowFunctions
{
    class Function
    {
        typedef void(T, uint32_t) FuncPointer;
        
        void calculate(T window, uint32_t size) { return mFunc(window, size); }
        
        Ref mReference;
        FuncPointer mFunc;
    };
    
    void add(Function function) { mFunctions.push_back(function); }
    
    bool calculate(Ref reference, T window, uint32_t size)
    {
        for (long i = 0; i < windows_count; i++)
        {
            if (reference == mFunctions[i].mReference)
            {
                mFunctions[i].calculate(window, size);
                return true;
            }
        }
        
        return false;
    }
    
    std::vector<Function> mFunctions:
};


<template class T> void window_rect(T window, uint32_t size)
{
	for (long i = 0; i < size; i++)
		window[i] = 1;
}

void window_triangle(T window, uint32_t size)
{
    long i;
    
    for (i = 0; i < (size >> 1); i++)
        window[i] = (double) i / (double) (size >> 1);
    for (; i < size; i++)
        window[i] = (double) (((double) size - 1) - (double) i) / (double) (size >> 1);
}

void window_hann(T window, uint32_t size)
{
	for (long i = 0; i < size; i++)
		window[i] = 0.5 - (0.5 * cos(WINDOW_TWOPI * ((double) i / (double) size)));
}

void window_hamming(T window, uint32_t size)
{
	for (long i = 0; i < size; i++)
		window[i] = 0.54347826 - (0.45652174 * cos(WINDOW_TWOPI * ((double) i / (double) size)));
}


void window_blackman(T window, uint32_t size)
{
	for (long i = 0; i < size; i++)
		window[i] = 0.42659071 - (0.49656062 * cos(WINDOW_TWOPI * ((double) i / (double) size))) + (0.07684867 * cos(WINDOW_FOURPI * ((double) i / (double) size)));
}

void window_blackman_62(T window, uint32_t size)
{
	for (long i = 0; i < size; i++)
		window[i] = (0.44859f - 0.49364f * cos(WINDOW_TWOPI * ((double) i / (double) size)) + 0.05677f * cos(WINDOW_FOURPI * ((double) i / (double) size)));
}

void window_blackman_70(T window, uint32_t size)
{
	for (long i = 0; i < size; i++)
		window[i] = (0.42323f - 0.49755f * cos(WINDOW_TWOPI * ((double) i / (double) size)) + 0.07922f * cos(WINDOW_FOURPI * ((double) i / (double) size)));
}


void window_blackman_74(T window, uint32_t size)
{
	for (long i = 0; i < size; i++)
		window[i] = (0.402217f - 0.49703f * cos(WINDOW_TWOPI * ((double) i / (double) size)) + 0.09892f * cos(WINDOW_FOURPI * ((double) i / (double) size)) - 0.00188 * cos(WINDOW_THREEPI * ((double) i / (double) size)));
}

void window_blackman_92(T window, uint32_t size)
{
	for (long i = 0; i < size; i++)
		window[i] = (0.35875f - 0.48829f * cos(WINDOW_TWOPI * ((double) i / (double) size)) + 0.14128f * cos(WINDOW_FOURPI * ((double) i / (double) size)) - 0.01168 * cos(WINDOW_THREEPI * ((double) i / (double) size)));
}

void window_blackman_harris(T window, uint32_t size)
{
	for (long i = 0; i < size; i++)
		window[i] = 0.35875 - (0.48829 * cos(WINDOW_TWOPI * ((double) i / (double) size))) + (0.14128 * cos(WINDOW_FOURPI * ((double) i / (double) size))) - (0.01168 * cos(WINDOW_SIXPI * ((double) i / (double) size)));
}

void window_flat_top(T window, uint32_t size)
{
	for (long i = 0; i < size; i++)
		window[i] = 0.2810639 - (0.5208972 * cos(WINDOW_TWOPI * ((double) i / (double) size))) + (0.1980399 * cos(WINDOW_FOURPI * ((double) i / (double) size)));
}

void window_kaiser(T window, uint32_t size)
{
    double alpha_bessel_recip;
    double new_term;
    double x_sq;
    double b_func;
    double temp;
    long i, j;
    
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
    
    for (i = 0; i < size; i++)
    {
        temp = ((2.0 * (double) i) - ((double) size - 1.0));
        temp = temp / size;
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

// The below is incorrect!!!!

void window_multisine_tapers(T window, uint32_t size, uint32_t num_tapers)
{
	long i, j;
	
	for (j = 0; j < size; j++)
		window[j] = 0.;
	
	for (i = 0; i < num_tapers; i++)
	{
		for (j = 0; j < size; j++)
			window[j] += sin (WINDOW_PI * (double) (i + 1) * (double) (j + 1) / (double) (size + 1));
	}
}

void window_msinetaper1(T window, uint32_t size)
{
	window_multisine_tapers(window, size, 1);
}

void window_msinetaper2(T window, uint32_t size)
{
	window_multisine_tapers(window, size, 2);
}

void window_msinetaper3(T window, uint32_t size)
{
	window_multisine_tapers(window, size, 3);
//}

void window_msinetaper4(T window, uint32_t size)
{
	window_multisine_tapers(window, size, 4);
}

void window_msinetaper5(T window, uint32_t size)
{
	window_multisine_tapers(window, size, 5);
}

void window_msinetaper6(T window, uint32_t size)
{
	window_multisine_tapers(window, size, 6);
}

<template class T>
class IndexedWindowFunctions :: WindowFunctions <T, int>
{
    enum WindowTypes {kWindowRect, kWindowTriangle, kWindowHann, kWindowHamming, kWindowBlackman, kWindowBlackman62, kWindowBlackman70, kWindowBlackman74, kWindowBlackman92, kWindowBlackmanHarris, kWindowFlatTop, kWindowKaiser };
    
    IndexedWindowsFunctions()
    {
        add(kWindowRect, window_rect);
        add(kWindowTriangle, window_triangle);

        add(kWindowHann, window_hann);
        add(kWindowHamming, window_hamming);
        
        add(kWindowBlackman, window_blackman);
        add(kWindowBlackman62, window_blackman_62);
        add(kWindowBlackman70, window_blackman_70);
        add(kWindowBlackman74, window_blackman_62);
        add(kWindowBlackman92, window_blackman_92);
        add(kWindowBlackmanHarris, window_blackman_harris);

        add(kWindowFlatTop, window_kaiser);
        
        add(kWindowKaiser, window_flat_top);
    }
};
