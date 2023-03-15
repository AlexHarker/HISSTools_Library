
#pragma once

#include <algorithm>
#include <vector>

enum ConvolveError
{
	CONVOLVE_ERR_NONE = 0,
	CONVOLVE_ERR_IN_CHAN_OUT_OF_RANGE,
	CONVOLVE_ERR_OUT_CHAN_OUT_OF_RANGE,
	CONVOLVE_ERR_MEM_UNAVAILABLE,
	CONVOLVE_ERR_MEM_ALLOC_TOO_SMALL,
	CONVOLVE_ERR_TIME_IMPULSE_TOO_LONG,
	CONVOLVE_ERR_TIME_LENGTH_OUT_OF_RANGE,
	CONVOLVE_ERR_PARTITION_LENGTH_TOO_LARGE,
	CONVOLVE_ERR_FFT_SIZE_MAX_TOO_SMALL,
	CONVOLVE_ERR_FFT_SIZE_MAX_TOO_LARGE,
	CONVOLVE_ERR_FFT_SIZE_MAX_NON_POWER_OF_TWO,
	CONVOLVE_ERR_FFT_SIZE_OUT_OF_RANGE,
	CONVOLVE_ERR_FFT_SIZE_NON_POWER_OF_TWO,
};

template <class T, class U>
struct conformed_input
{
    conformed_input(const U *input, uintptr_t length)
    : m_vector(length)
    {
        for (uintptr_t i = 0; i < length; i++)
            m_vector[i] = static_cast<T>(input[i]);
    }
        
    const T *get() const { return m_vector.data(); }
    
private:
    
    std::vector<T> m_vector;
};

template <class T>
struct conformed_input<T, T>
{
    conformed_input(const T *input, uintptr_t length)
    : m_input(input)
    {}
    
    const T *get() const { return m_input; }

private:

    const T *m_input;
};
