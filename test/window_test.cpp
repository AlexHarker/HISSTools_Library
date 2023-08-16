
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>

#include "../include/window_functions.hpp"
#include "../include/audio_file/out_file.hpp"

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
        mStore1 = elapsed;
    }
    
    uint64_t finish(const std::string& msg)
    {
        tabbedOut(msg + " Elapsed ", to_string_with_precision(mStore1 / 1000000.0, 2), 35);
        
        uint64_t elapsed = mStore1;
        
        return elapsed;
    };
    
    void relative(const std::string& msg)
    {
        tabbedOut(msg + " Comparison ", to_string_with_precision(((double) mStore2 / (double) mStore1), 2), 35);
    }
    
private:
    
    uint64_t        mStart;
    uint64_t        mStore1;
    uint64_t        mStore2;
};

std::default_random_engine rand_engine;

int random_integer(int min, int max)
{
    return (rand_engine() % ((max + 1) - min)) + min;
}

bool check_symmetry()
{
    const int size = random_integer(20, 300);
    int begin = random_integer(0, size / 2 + 2);
    int end = random_integer(size / 2 - 2, size);
    
    double window1[size];
    double window2[size];

    using namespace htl;
    using namespace window_functions;

    triangle(window1, size, 0, size, params());
    triangle(window2, size, begin, end, params());

    for (int i = begin; i < end; i++)
    {
        if (window1[i] != window2[i - begin])
        {
            double relative_error = exp(fabs(log(window1[i] / window2[i - begin])));
        
            if (relative_error > 1.000000000001 || isnan(relative_error))
                return false;
        }
    }
    
    return true;
}

void check_window(const char* wind, htl::window_functions::window_generator<double> f, const htl::window_functions::params &p)
{
    constexpr int size = 32768;
    double window[size];
    
    f(window, size, 0, size, p);
    
    auto it = std::max_element(window, window + size);
    auto n = it - window;
    bool symmetry = true;
    
    for (int i = ((size >> 1) + 1); i < size; i++)
    {
        if (window[i] != window[size - i])
        {
            symmetry = false;
            break;
        }
    }
        
    std::cout << "** test " << wind << " window\n";
    std::cout << "element zero " << window[0] << "\n";
    std::cout << "middle element " << window[size >> 1] << "\n";
    std::cout << "max element " << *it << " [" << n << "]\n";
    std::cout << "symmetry " << symmetry << "\n";
}

int main(int argc, const char * argv[])
{
    constexpr int size = 32768;
    constexpr int iter = 1024;
    constexpr int sym_iter = 32768;
    double window[size];

    rand_engine.seed(std::random_device()());
    
    using namespace htl;
    using namespace window_functions;
    
    params ep;
    params tp(0.1, 0.9);
    params typ(0.1);
    params p(0.5, 0.5);
    
    check_window("parzen", &parzen<double>, ep);
    check_window("welch", &welch<double>, ep);
    check_window("sine", &sine<double>, ep);
    check_window("hann", &hann<double>, ep);
    check_window("triangle", &triangle<double>, ep);
    check_window("trapezoid", &trapezoid<double>, tp);
    check_window("tukey", &tukey<double>, typ);

    for (int i = 0; i < iter; i++)
        sine(window, size, 0, size, params());
    
    Timer timer;
    
    timer.start();
    for (int i = 0; i < iter; i++)
        cosine_2_term(window, size, 0, size, p);
    timer.stop();
    timer.finish("Branch Speed Test");
    
    timer.start();
    for (int i = 0; i < iter; i++)
        hann(window, size, 0, size, params(0.2, 0.3));
    timer.stop();
    timer.finish("Non-branch Speed Test");
    
    timer.relative("Window Speed Test");

    for (int i = 0; i < sym_iter; i++)
    {
        if (!check_symmetry())
        {
            std::cout << "Symmetry copying failed!\n";
            break;
        }
        
        if (i == sym_iter - 1)
            std::cout << "Symmetry copying succeeded!\n";
    }

    indexed_generator<double, sine_taper<double>, hann<double>> gen;
    
    gen(0, window, size, 0, size, params(4));
    
    if (argc > 1)
    {
        htl::out_audio_file file(argv[1], htl::base_audio_file::file_type::wave, 
                                          htl::base_audio_file::pcm_format::float32, 1, 44100.0);
        
        if (file.is_open())
            file.write_channel(window, size, 0);
    }
    return 0;
}
