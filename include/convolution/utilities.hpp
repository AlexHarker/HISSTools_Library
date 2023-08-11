
#ifndef HISSTOOLS_CONVOLUTION_UTILITIES_HPP
#define HISSTOOLS_CONVOLUTION_UTILITIES_HPP

#include <algorithm>
#include <vector>

#include "../namespace.hpp"
#include "../fft/fft.hpp"

HISSTOOLS_NAMESPACE_START()

namespace impl
{
    template <class T>
    void add_to_result(T& result, T value)  { result += value; }

    template <class T>
    void copy_to_result(T& result, T value) { result = value; }

    template <class U, void Func(U&, U), class T, class Size>
    void loop_cast_n(T *first, Size count, U *result)
    {
        for (Size i = 0; i != count; ++i, ++result, ++first)
            Func(*result, static_cast<U>(*first));
    }

    template <class T, class Size, class U>
    void add_cast_n(T *first, Size count, U *result)
    {
        loop_cast_n<U, add_to_result<U>>(first, count, result);
    }

    template <class T, class Size, class U>
    void copy_cast_n(T *first, Size count, U *result)
    {
        loop_cast_n<U, copy_to_result<U>>(first, count, result);
    }

    template <class T>
    static constexpr T ilog2(T x)
    {
        T count = 0;
    
        for (; x >> count; count++);
    
        return x == T(1) << (count - T(1)) ? count - T(1) : count;
    }
}

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

HISSTOOLS_NAMESPACE_END()

#endif /* HISSTOOLS_CONVOLUTION_UTILITIES_HPP */
