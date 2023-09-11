
#include <algorithm>
#include <iostream>
#include <random>

#include "test_utils/timer.hpp"
#include "test_utils/tabbed_out.hpp"

#include "../include/window_functions.hpp"
#include "../include/audio_file/out_file.hpp"

using namespace htl_test_utils;

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
    
    double* window1 = (double*) malloc(sizeof(double) * size);
    double* window2 = (double*) malloc(sizeof(double) * size);

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
            {
                delete[] window1;
                delete[] window2;

                return false;
            }
        }
    }

    free(window1);
    free(window2);
    
    return true;
}

void check_window(const char* wind, 
                  htl::window_functions::window_generator<double> f, 
                  const htl::window_functions::params &p)
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
    
    steady_timer timer;
    
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
