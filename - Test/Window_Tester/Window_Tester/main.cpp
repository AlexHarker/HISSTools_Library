
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>

#include "../../../WindowFunctions.hpp"
#include "../../../AudioFile/OAudioFile.h"

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

template <typename FuncType>
void check_window(const char* wind, FuncType f, const window_functions::params &p)
{
    const static int size = 32768;
    double window[size];
    
    f(window, size, 0, size, p);
    
    auto it = std::max_element(window, window + size);
    auto n = it - window;
    bool symmetry = true;
    
    for (int i = ((size >> 1) + 1); i < size; i++)
    {
        if (window[i] != window[size - i])
        {
            //std::cout << "failed " << i << " " << window[i] << " " << window[size - i] << "\n";
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
    const static int size = 32768;
    const static int iter = 1024;
    double window[size];

    using namespace window_functions;
    
    params ep;
    params tp(0.1, 0.9);
    params typ(0.1);
    params p(0.5, 0.5);
    
    check_window<decltype(&window_parzen<double>)>("parzen", &window_parzen<double>, ep);
    check_window<decltype(&window_welch<double>)>("welch", &window_welch<double>, ep);
    check_window<decltype(&window_sine<double>)>("sine", &window_sine<double>, ep);
    check_window<decltype(&window_hann<double>)>("hann", &window_hann<double>, ep);
    check_window<decltype(&window_triangle<double>)>("triangle", &window_triangle<double>, ep);
    check_window<decltype(&window_trapezoid<double>)>("trapezoid", &window_trapezoid<double>, tp);
    check_window<decltype(&window_tukey<double>)>("tukey", &window_tukey<double>, typ);

    for (int i = 0; i < iter; i++)
        window_sine(window, size, 0, size, params());
    
    Timer timer;
    
    timer.start();
    for (int i = 0; i < iter; i++)
        window_cosine_sum(window, size, 0, size, p);
    timer.stop();
    timer.finish("Branch Speed Test");
    
    timer.start();
    for (int i = 0; i < iter; i++)
        window_hann(window, size, 0, size, params(0.2, 0.3));
    timer.stop();
    timer.finish("Non-branch Speed Test");
    
    timer.relative("Window Speed Test");

    indexed_generator<double, window_trapezoid<double>, window_hann<double>> gen;
    
    gen(0, window, size, 0, size, params(0.2, 0.3));
    
    if (argc)
    {
        std::cout << argv[1];
        HISSTools::OAudioFile file(argv[1], HISSTools::BaseAudioFile::kAudioFileWAVE, HISSTools::BaseAudioFile::kAudioFileFloat32, 1, 44100.0);
        
        if (file.isOpen())
            file.writeChannel(window, size, 0);
    }
    return 0;
}
