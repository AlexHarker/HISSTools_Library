
#include <mach/mach.h>
#include <mach/mach_time.h>
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

int main(int argc, const char * argv[])
{
    const static int size = 32768;
    const static int iter = 1024;
    double window[size];
    
    window_functions::params p;
    
    p.c0 = 0.5;
    p.c1 = 10.;
    p.c2 = -0.5;
    p.c3 = 10.;
    p.c4 = 10.;
    p.c5 = 10.;
    p.c6 = 10.;
    
    for (int i = 0; i < iter; i++)
        window_cosine(window, size, size);
    
    Timer timer;
    
    timer.start();
    for (int i = 0; i < iter; i++)
        window_hann(window, size, size);
    timer.stop();
    timer.finish("Branch Speed Test");
    
    timer.start();
    for (int i = 0; i < iter; i++)
        window_parzen(window, size, size);
    timer.stop();
    timer.finish("Non-branch Speed Test");
    
    timer.relative("Window Speed Test");

    if (argc)
    {
        std::cout << argv[1];
        HISSTools::OAudioFile file(argv[1], HISSTools::BaseAudioFile::kAudioFileWAVE, HISSTools::BaseAudioFile::kAudioFileFloat32, 1, 44100.0);
        
        if (file.isOpen())
            file.writeChannel(window, size, 0);
    }
    return 0;
}