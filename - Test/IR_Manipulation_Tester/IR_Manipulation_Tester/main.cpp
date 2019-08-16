
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <sstream>
#include <iostream>
#include <iomanip>

#include "../../../SpectralFunctions.hpp"

// Output

void tabbedOut(const std::string& name, const std::string& text, int tab = 25)
{
    std::cout << std::setw(tab) << std::setfill(' ');
    std::cout.setf(std::ios::left);
    std::cout.unsetf(std::ios::right);
    std::cout << name;
    std::cout.unsetf(std::ios::left);
    std::cout << text << "\n";
}

template <typename T>
std::string to_string_with_precision(const T a_value, const int n = 4, bool fixed = true)
{
    std::ostringstream out;
    if (fixed)
        out << std::setprecision(n) << std::fixed << a_value;
    else
        out << std::setprecision(n) << a_value;
    
    return out.str();
}

// Timing

class Timer
{
    
public:
    
    Timer() : mStart(0), mStore1(0), mStore2(0) {}
    
    void start()
    {
        mStart = mach_absolute_time();
    };
    
    void stop()
    {
        uint64_t end = mach_absolute_time();
        
        mach_timebase_info_data_t info;
        mach_timebase_info(&info);
        
        uint64_t elapsed = ((end - mStart) * info.numer) / info.denom;
        
        mStore2 = mStore1;
        mStore1 += elapsed;
    }
    
    uint64_t finish(const std::string& msg)
    {
        tabbedOut(msg + " Elapsed ", to_string_with_precision(mStore1 / 1000000.0, 2), 35);
        
        uint64_t elapsed = mStore1;
        
        mStore2 = 0;
        mStore1 = 0;
        
        return elapsed;
    };
    
    void relative(const std::string& msg)
    {
        tabbedOut(msg + " Comparison ", to_string_with_precision(((double) mStore1 / (double) mStore2), 2), 35);
    }
    
private:
    
    uint64_t        mStart;
    uint64_t        mStore1;
    uint64_t        mStore2;
};


template <class SPLIT>
void fillSplit(SPLIT split, uintptr_t fft_log2)
{
    for (uintptr_t i =0; i < (1 << fft_log2); i++)
    {
        split.realp[i] = 1.0 - 2.0 * std::rand() / RAND_MAX;
        split.imagp[i] = 1.0 - 2.0 * std::rand() / RAND_MAX;
    }
}

template<class SETUP, class SPLIT, class T>
uint64_t timing_test(std::string test, uintptr_t fft_log2, double phase, bool zero, int testSize)
{
    SETUP setup;
    SPLIT split;
    
    uintptr_t fft_size = 1 << fft_log2;
    
    split.realp = (T *) malloc(sizeof(T) * fft_size);
    split.imagp = (T *) malloc(sizeof(T) * fft_size);
    
    hisstools_create_setup(&setup, fft_log2);
    
    Timer timer;
    
    for (int i = 0; i < testSize; i++)
    {
        fillSplit(split, fft_log2);
        timer.start();
        ir_phase(setup, &split, &split, fft_size, phase, zero);
        timer.stop();
    }
    
    uint64_t time = timer.finish(test);
    
    free(split.realp);
    free(split.imagp);
    hisstools_destroy_setup(setup);
    
    return time;
}

int main(int argc, const char * argv[])
{
    // insert code here...
    std::cout << "Hello, World!\n";
    
    int fft_log2 = 14;
    int iter = 100;
    
    timing_test<FFT_SETUP_D, FFT_SPLIT_COMPLEX_D, double>("Zero Mix", fft_log2, 0.1, true, iter);
    timing_test<FFT_SETUP_D, FFT_SPLIT_COMPLEX_D, double>("Center Mix", fft_log2, 0.9, false, iter);
    timing_test<FFT_SETUP_D, FFT_SPLIT_COMPLEX_D, double>("Zero Min", fft_log2, 0.0, true, iter);
    timing_test<FFT_SETUP_D, FFT_SPLIT_COMPLEX_D, double>("Center Min", fft_log2, 0.0, false, iter);
    timing_test<FFT_SETUP_D, FFT_SPLIT_COMPLEX_D, double>("Zero Max", fft_log2, 1.0, true, iter);
    timing_test<FFT_SETUP_D, FFT_SPLIT_COMPLEX_D, double>("Center Max", fft_log2, 1.0, false, iter);
    timing_test<FFT_SETUP_D, FFT_SPLIT_COMPLEX_D, double>("Zero Lin", fft_log2, 0.5, true, iter);
    timing_test<FFT_SETUP_D, FFT_SPLIT_COMPLEX_D, double>("Center Lin", fft_log2, 0.5, false, iter);
    
    timing_test<FFT_SETUP_F, FFT_SPLIT_COMPLEX_F, float>("Zero Mix", fft_log2, 0.1, true, iter);
    timing_test<FFT_SETUP_F, FFT_SPLIT_COMPLEX_F, float>("Center Mix", fft_log2, 0.9, false, iter);
    timing_test<FFT_SETUP_F, FFT_SPLIT_COMPLEX_F, float>("Zero Min", fft_log2, 0.0, true, iter);
    timing_test<FFT_SETUP_F, FFT_SPLIT_COMPLEX_F, float>("Center Min", fft_log2, 0.0, false, iter);
    timing_test<FFT_SETUP_F, FFT_SPLIT_COMPLEX_F, float>("Zero Max", fft_log2, 1.0, true, iter);
    timing_test<FFT_SETUP_F, FFT_SPLIT_COMPLEX_F, float>("Center Max", fft_log2, 1.0, false, iter);
    timing_test<FFT_SETUP_F, FFT_SPLIT_COMPLEX_F, float>("Zero Lin", fft_log2, 0.5, true, iter);
    timing_test<FFT_SETUP_F, FFT_SPLIT_COMPLEX_F, float>("Center Lin", fft_log2, 0.5, false, iter);

    return 0;
}
