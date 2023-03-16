
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <sstream>
#include <iostream>
#include <iomanip>

#include "HISSTools_FFT.hpp"

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
    
    Timer() : mStart(0), mStore(0) {}
    
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
        
        mStore += elapsed;
    }
        
    uint64_t finish(const std::string& msg)
    {
        tabbedOut(msg + " Elapsed ", to_string_with_precision(mStore / 1000000.0, 2), 35);
        
        uint64_t elapsed = mStore1;
        
        mStore = 0;
        
        return elapsed;
    };
    
private:
    
    uint64_t        mStart;
    uint64_t        mStore;
};

template <class SPLIT>
void fillSplit(SPLIT split, int max_log2)
{
    for (long i =0; i < (1 << max_log2); i++)
    {
        split.realp[i] = 1.0 - 2.0 * std::rand() / RAND_MAX;
        split.imagp[i] = 1.0 - 2.0 * std::rand() / RAND_MAX;
    }
}

template <class SETUP, class SPLIT, class T>
uint64_t crash_test(int min_log2, int max_log2)
{
    SETUP setup;
    SPLIT split;
    
    split.realp = (T *) malloc(sizeof(T) * 1 << max_log2);
    split.imagp = (T *) malloc(sizeof(T) * 1 << max_log2);
    
    hisstools_create_setup(&setup, max_log2);

    Timer timer;

    for (int i = min_log2; i < max_log2; i++)
    {
        fillSplit(split, i);
        timer.start();
        hisstools_fft(setup, &split, i);
        timer.stop();
    }
    
    
    for (int i = min_log2; i < max_log2; i++)
    {
        fillSplit(split, i);
        timer.start();
        hisstools_ifft(setup, &split, i);
        timer.stop();
    }
    
    for (int i = min_log2; i < max_log2; i++)
    {
        fillSplit(split, i);
        timer.start();
        hisstools_rfft(setup, &split, i);
        timer.stop();
    }
    
    for (int i = min_log2; i < max_log2; i++)
    {
        fillSplit(split, i);
        timer.start();
        hisstools_rifft(setup, &split, i);
        timer.stop();
    }
    
    uint64_t time = timer.finish("FFT Multiple Tests");

    free(split.realp);
    free(split.imagp);
    hisstools_destroy_setup(setup);
    
    return time;
}

template <class SETUP, class SPLIT, class T>
uint64_t single_test(int size, void (*Fn)(SETUP, SPLIT *, uintptr_t))
{
    SETUP setup;
    SPLIT split;
    
    split.realp = (T *) malloc(sizeof(T) * 1 << size);
    split.imagp = (T *) malloc(sizeof(T) * 1 << size);

    hisstools_create_setup(&setup, size);
    
    Timer timer;
    
    for (int i = 0; i < 10000; i++)
    {
        fillSplit(split, size);
        
        timer.start();
        Fn(setup, &split, size);
        timer.stop();
    }
    
    uint64_t time = timer.finish(std::string("FFT Single Tests ").append(std::to_string (1 << size)));

    free(split.realp);
    free(split.imagp);
    hisstools_destroy_setup(setup);
    
    return time;
}

template <class SETUP, class SPLIT, class T>
uint64_t matched_size_test(int min_log2, int max_log2)
{
    uint64_t time = 0;
    
    std::cout << "---FFT---\n";
    
    for (int i = min_log2; i < max_log2; i++)
        time += single_test<SETUP, SPLIT, T>(i, &hisstools_fft);
    
    std::cout << "---iFFT---\n";

    for (int i = min_log2; i < max_log2; i++)
        time += single_test<SETUP, SPLIT, T>(i, &hisstools_ifft);
    
    std::cout << "---Real FFT---\n";

    for (int i = min_log2; i < max_log2; i++)
        time += single_test<SETUP, SPLIT, T>(i, &hisstools_rfft);
    
    std::cout << "---Real iFFT---\n";

    for (int i = min_log2; i < max_log2; i++)
        time += single_test<SETUP, SPLIT, T>(i, &hisstools_rifft);
    
    return time;
}

template <class SPLIT, class T, class U>
bool zip_correctness_test(int min_log2, int max_log2)
{
    SPLIT split;
    
    U *ptr = (U *) malloc(sizeof(U) * 1 << max_log2);
    split.realp = (T *) malloc(sizeof(T) * 1 << (max_log2 - 1));
    split.imagp = (T *) malloc(sizeof(T) * 1 << (max_log2 - 1));
    
    for (int i = min_log2; i < max_log2; i++)
    {
        for (int j = 0; j < (1 << i); j++)
            ptr[j] = j;
        
        hisstools_unzip(ptr, &split, i);
        
        for (int j = 0 ; j < (1 << (i - 1)); j++)
        {
            if (split.realp[j] != (j << 1))
            {
                std::cout << "zip error\n";
                return true;
            }
            if (i > 1 && split.imagp[j] != (j << 1) + 1)
            {
                std::cout << "zip error\n";
                return true;
            }
        }
        
        hisstools_zip(&split, ptr, i);
        
        for (int j = 0 ; j < (1 << (i - 1)); j++)
        {
            if (ptr[j] != j)
            {
                std::cout << "unzip error\n";
                return true;
            }
        }
    }
    
    free(ptr);
    free(split.realp);
    free(split.imagp);
    
    std::cout << "FFT Zip Tests Successful\n";
    
    return false;
}

template <class SPLIT, class T, class U>
uint64_t zip_test(int min_log2, int max_log2)
{
    SPLIT split;
    
    U *ptr = (U *) malloc(sizeof(U) * 1 << max_log2);
    split.realp = (T *) malloc(sizeof(T) * 1 << (max_log2 - 1));
    split.imagp = (T *) malloc(sizeof(T) * 1 << (max_log2 - 1));
    
    Timer timer;
    timer.start();
    
    for (int i = min_log2; i < max_log2; i++)
        hisstools_unzip(ptr, &split, i);
    
    for (int i = min_log2; i < max_log2; i++)
        hisstools_unzip_zero(ptr, &split, 1 << i, i);
    
    for (int i = min_log2; i < max_log2; i++)
        hisstools_zip(&split, ptr, i);
    
    timer.stop();
    uint64_t time = timer.finish("Zip Tests");

    free(ptr);
    free(split.realp);
    free(split.imagp);
    
    return time;
}


int main(int argc, const char * argv[])
{
    uint64_t time = 0;
    
    std::cout << "****** DOUBLE ******\n";
    
    if (zip_correctness_test<FFT_SPLIT_COMPLEX_D, double, double>(1, 24))
    {
        std::cout << "Errors - did not complete tests\n";
        return -1;
    }
    
    std::cout << "****** FLOAT ******\n";
    
    if (zip_correctness_test<FFT_SPLIT_COMPLEX_F, float, float>(1, 24))
    {
            std::cout << "Errors - did not complete tests\n";
            return -1;
    }
    
    std::cout << "****** DOUBLE ******\n";

    time += crash_test<FFT_SETUP_D, FFT_SPLIT_COMPLEX_D, double>(0, 22);

    std::cout << "****** FLOAT ******\n";

    time += crash_test<FFT_SETUP_F, FFT_SPLIT_COMPLEX_F, float>(0, 22);
    
    std::cout << "****** DOUBLE ******\n";

    time += matched_size_test<FFT_SETUP_D, FFT_SPLIT_COMPLEX_D, double>(6, 14);
    
    std::cout << "****** FLOAT ******\n";

    time += matched_size_test<FFT_SETUP_F, FFT_SPLIT_COMPLEX_F, float>(6, 14);
    
    std::cout << "****** DOUBLE ******\n";

    time += zip_test<FFT_SPLIT_COMPLEX_D, double, double>(1, 24);
    
    std::cout << "****** FLOAT ******\n";

    time += zip_test<FFT_SPLIT_COMPLEX_F, float, float>(1, 24);
    
    tabbedOut("FFT Tests Total ", to_string_with_precision(time / 1000000.0, 2), 35);
    
    std::cout << "Finished Running\n";
    return 0;
}
