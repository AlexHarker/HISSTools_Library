
#ifndef __HISSTOOLS_IR_MANIPULATION__
#define __HISSTOOLS_IR_MANIPULATION__

#include "../HISSTools_FFT/HISSTools_FFT.h"

void ir_copy(FFT_SPLIT_COMPLEX_D *out, FFT_SPLIT_COMPLEX_D *in, uintptr_t fft_size);

void ir_spike(FFT_SPLIT_COMPLEX_D *out, uintptr_t fft_size, double spike_position);

void ir_delay(FFT_SPLIT_COMPLEX_D *out, FFT_SPLIT_COMPLEX_D *in, uintptr_t fft_size, double delay);

void ir_time_reverse(FFT_SPLIT_COMPLEX_D *out, FFT_SPLIT_COMPLEX_D *in, uintptr_t fft_size);

void ir_phase(FFT_SETUP_D setup, FFT_SPLIT_COMPLEX_D *out, FFT_SPLIT_COMPLEX_D *in, uintptr_t fft_size, double phase, bool zero_center = false);

#endif
