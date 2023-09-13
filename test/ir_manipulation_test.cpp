
#include "test_utils/timer.hpp"
#include "test_utils/tabbed_out.hpp"

#include "../include/spectral_functions.hpp"

using namespace htl_test_utils;

template <class SPLIT>
void fillSplit(SPLIT split, uintptr_t fft_log2)
{
    for (uintptr_t i = 0; i < (1 << fft_log2); i++)
    {
        split.realp[i] = 1.0 - 2.0 * std::rand() / RAND_MAX;
        split.imagp[i] = 1.0 - 2.0 * std::rand() / RAND_MAX;
    }
}

template <class T>
uint64_t timing_test(std::string test, uintptr_t fft_log2, double phase, bool zero, int testSize)
{
    htl::setup_type<T> setup;
    htl::split_type<T> split;

    uintptr_t fft_size = 1 << fft_log2;

    split.realp = (T *)malloc(sizeof(T) * fft_size);
    split.imagp = (T *)malloc(sizeof(T) * fft_size);

    create_fft_setup(&setup, fft_log2);

    steady_timer timer;

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
    destroy_fft_setup(setup);

    return time;
}

int main(int argc, const char *argv[])
{
    // insert code here...
    std::cout << "Double vector size is " << htl::simd_limits<double>::max_size << "\n";

    int fft_log2 = 14;
    int iter = 100;

    timing_test<double>("Zero Mix", fft_log2, 0.1, true, iter);
    timing_test<double>("Center Mix", fft_log2, 0.9, false, iter);
    timing_test<double>("Zero Min", fft_log2, 0.0, true, iter);
    timing_test<double>("Center Min", fft_log2, 0.0, false, iter);
    timing_test<double>("Zero Max", fft_log2, 1.0, true, iter);
    timing_test<double>("Center Max", fft_log2, 1.0, false, iter);
    timing_test<double>("Zero Lin", fft_log2, 0.5, true, iter);
    timing_test<double>("Center Lin", fft_log2, 0.5, false, iter);

    std::cout << "Float vector size is " << htl::simd_limits<float>::max_size << "\n";

    timing_test<float>("Zero Mix", fft_log2, 0.1, true, iter);
    timing_test<float>("Center Mix", fft_log2, 0.9, false, iter);
    timing_test<float>("Zero Min", fft_log2, 0.0, true, iter);
    timing_test<float>("Center Min", fft_log2, 0.0, false, iter);
    timing_test<float>("Zero Max", fft_log2, 1.0, true, iter);
    timing_test<float>("Center Max", fft_log2, 1.0, false, iter);
    timing_test<float>("Zero Lin", fft_log2, 0.5, true, iter);
    timing_test<float>("Center Lin", fft_log2, 0.5, false, iter);

    return 0;
}
