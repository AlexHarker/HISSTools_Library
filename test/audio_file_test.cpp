
#include <iostream>

#include <sstream>
#include <iostream>
#include <iomanip>

#include <array>
#include <chrono>
#include <limits>

#include "../include/audio_file/in_file.hpp"
#include "../include/audio_file/out_file.hpp"
#include "../include/audio_file/utilities.hpp"
#include "../include/random_generator.hpp"


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
    using clock = std::chrono::high_resolution_clock;
    
    double ms(clock::duration duration)
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(duration).count() / 1000.0;
    }
        
public:
    
    Timer() : mStore1(clock::duration::zero()), mStore2(clock::duration::zero()) {}

    void start()
    {
        mStart = clock::now();
    };
    
    void stop()
    {
        auto elapsed = clock::now() - mStart;
                
        mStore2 = mStore1;
        mStore1 += elapsed;
    }
    
    clock::duration finish(const std::string& msg)
    {
        tabbedOut(msg + " Elapsed ", to_string_with_precision(ms(mStore1), 2), 35);
        
        auto elapsed = mStore1;
        
        mStore2 = clock::duration::zero();
        mStore1 = clock::duration::zero();
        
        return elapsed;
    };
    
    void relative(const std::string& msg)
    {
        tabbedOut(msg + " Comparison ", to_string_with_precision(ms(mStore1) / ms(mStore2), 2), 35);
    }
    
private:
    
    clock::time_point mStart;
    clock::duration mStore1;
    clock::duration mStore2;
};

bool compareNans(double x, double y)
{
    return std::isnan(x) && std::isnan(y);
}

double convertIEEEExtended(double x)
{
    char bytes[10];
    
    htl::extended_double_convertor()(reinterpret_cast<unsigned char *>(bytes), x);
    auto y = htl::extended_double_convertor()(bytes);
    
    if (x != y && !(compareNans(x, y)))
        throw;
        
    return y;
}

int main(int argc, const char * argv[])
{
    random_generator<> gen;
    
    auto r = gen.rand_double();
    auto rh = gen.rand_double() * 65536.0;
    auto inf = std::numeric_limits<double>::infinity();
    auto nanS = std::numeric_limits<double>::signaling_NaN();
    auto nanN = std::numeric_limits<double>::quiet_NaN();

    tabbedOut("random", to_string_with_precision(r));
    tabbedOut("random_hi", to_string_with_precision(rh));

    tabbedOut("+0", to_string_with_precision(convertIEEEExtended(0.0)));
    tabbedOut("-0",  to_string_with_precision(convertIEEEExtended(-0.0)));

    tabbedOut("+2",  to_string_with_precision(convertIEEEExtended(2)));
    tabbedOut("+0.5",  to_string_with_precision(convertIEEEExtended(0.5)));
    tabbedOut("-0.5",  to_string_with_precision(convertIEEEExtended(-0.5)));
    tabbedOut("+1.5",  to_string_with_precision(convertIEEEExtended(1.5)));
    tabbedOut("-1.5",  to_string_with_precision(convertIEEEExtended(-1.5)));
    tabbedOut("+2",  to_string_with_precision(convertIEEEExtended(2)));
    tabbedOut("-2",  to_string_with_precision(convertIEEEExtended(-2)));
    tabbedOut("+3.5",  to_string_with_precision(convertIEEEExtended(3.5)));
    tabbedOut("-3.5",  to_string_with_precision(convertIEEEExtended(-3.5)));
    tabbedOut("+16384",  to_string_with_precision(convertIEEEExtended(16384)));
    tabbedOut("-16384",  to_string_with_precision(convertIEEEExtended(-16384)));
    tabbedOut("+random",  to_string_with_precision(convertIEEEExtended(r)));
    tabbedOut("-random",  to_string_with_precision(convertIEEEExtended(-r)));
    tabbedOut("+random",  to_string_with_precision(convertIEEEExtended(rh)));
    tabbedOut("-random",  to_string_with_precision(convertIEEEExtended(-rh)));
    tabbedOut("+inf",  to_string_with_precision(convertIEEEExtended(inf)));
    tabbedOut("-inf",  to_string_with_precision(convertIEEEExtended(-inf)));
    tabbedOut("nan (signalling)",  to_string_with_precision(convertIEEEExtended(nanS)));
    tabbedOut("nan (quiet)",  to_string_with_precision(convertIEEEExtended(nanN)));
    
    for (size_t i = 0; i < 5000000; i++)
        convertIEEEExtended(gen.rand_double(-65536.0, 65536.0));
    
    return 0;
}
