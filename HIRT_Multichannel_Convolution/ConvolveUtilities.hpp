
#pragma once

#include <algorithm>
#include <vector>

enum class ConvolveError
{
	None = 0,
	InChanOutOfRange,
	OutChanOutOfRange,
	MemUnavailable,
	MemAllocTooSmall,
	TimeImpulseTooLong,
	TimeLengthOutOfRange,
	PartitionLengthTooLarge,
    FFTSizeOutOfRange,
    FFTSizeNonPowerOfTwo,
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
