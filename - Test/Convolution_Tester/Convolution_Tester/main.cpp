
#include <iostream>

#include <sstream>
#include <iostream>
#include <iomanip>

#include <array>
#include <chrono>

#include "../../../HIRT_Multichannel_Convolution/ConvolveMultichannel.hpp"
#include "../../../RandomGenerator.hpp"


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

template <class T>
std::vector<T> random_vector(int length)
{
    std::vector<T> v(length);
    random_generator<> gen;
    
    for (int i = 0; i < length; i++)
        v[i] = gen.rand_double();
    
    return v;
}

template <class T, class IO>
void test_time_domain(int block_size, int num_blocks, int ir_length, const char *str)
{
    convolve_time_domain<T, IO> convolver;
    
    std::vector<IO> in = random_vector<IO>(block_size * num_blocks);
    std::vector<T> ir = random_vector<T>(ir_length);
    std::vector<IO> out(block_size * num_blocks, IO(0));
    
    convolver.set(ir.data(), ir_length);
    
    Timer t;
    
    t.start();
    
    for (int i = 0; i < num_blocks; i++)
    {
        const int offset = block_size * i;
        convolver.process(in.data() + offset, out.data() + offset, block_size, true);
    }
    
    t.stop();
    t.finish(str);
}

template <class T, class IO>
void test_what_happens(int block_size, int ir_length)
{
    convolve_time_domain<T, IO> convolver;
    
    std::vector<IO> in(block_size, IO(1));
    std::vector<T> ir(ir_length, IO(1));
    std::vector<IO> out(block_size, IO(0));
    
    convolver.set(ir.data(), ir_length);
    
    convolver.process(in.data(), out.data(), block_size, false);
    
    for (int i = 0; i < block_size; i++)
        std::cout << out[i] << " result " << i << "\n";
}

int main(int argc, const char * argv[])
{
    test_what_happens<double, double>(64, 512);
    test_what_happens<float, float>(64, 512);
    
    test_time_domain<double, double>(256, 10000, 1024, "Test double double -");
    test_time_domain<double, float>(256, 10000, 1024, "Test double float  -");
    test_time_domain<float, double>(256, 10000, 1024, "Test float  double -");
    test_time_domain<float, float>(256, 10000, 1024, "Test float  float  -");
    return 0;
}
