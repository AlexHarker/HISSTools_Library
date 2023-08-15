#include <iostream>

#include "../include/allocator.hpp"
#include "../include/interpolation.hpp"
#include "../include/kernel_smoother.hpp"
#include "../include/interpolation.hpp"
#include "../include/memory_swap.hpp"
#include "../include/namespace.hpp"
#include "../include/partial_tracker.hpp"
#include "../include/random_generator.hpp"
#include "../include/resampler.hpp"
#include "../include/simd_support.hpp"
#include "../include/spectral_functions.hpp"
#include "../include/spectral_processor.hpp"
#include "../include/statistics.hpp"
#include "../include/table_reader.hpp"
#include "../include/thread_locks.hpp"
#include "../include/window_functions.hpp"

#include "../include/audio_file/aifc_compression.hpp"
#include "../include/audio_file/base.hpp"
#include "../include/audio_file/extended_double.hpp"
#include "../include/audio_file/format.hpp"
#include "../include/audio_file/in_file.hpp"
#include "../include/audio_file/out_file.hpp"
#include "../include/audio_file/utilities.hpp"

#include "../include/fft/fft.hpp"
#include "../include/fft/fft_core.hpp"
#include "../include/fft/fft_types.hpp"

#include "../include/convolution/mono.hpp"
#include "../include/convolution/multichannel.hpp"
#include "../include/convolution/n_to_mono.hpp"
#include "../include/convolution/partitioned.hpp"
#include "../include/convolution/time_domain.hpp"
#include "../include/convolution/utilities.hpp"


int main(int argc, const char * argv[])
{
    std::cout << "testing ctest" << std::endl;

    return 0;
}