
#ifndef HISSTOOLS_AUDIO_FILE_UTILITIES_HPP
#define HISSTOOLS_AUDIO_FILE_UTILITIES_HPP

#include "../namespace.hpp"
#include "format.hpp"

HISSTOOLS_NAMESPACE_START()

// Byte Shift

template <int N, int M, audio_file_format::endianness E>
static constexpr int byte_shift()
{
    return E == audio_file_format::endianness::BIG ? (N - (M + 1)) * 8 : M * 8;
}

// Byte Getter

template <class T, int N, int M, audio_file_format::endianness E>
struct byte_getter
{
    T operator()(const unsigned char* bytes)
    {
        if constexpr (M == N - 1)
            return T(bytes[N - 1]) << byte_shift<N, N - 1, E>();
        else
            return (T(bytes[M]) << byte_shift<N, M, E>()) | byte_getter<T, N, M + 1, E>()(bytes);
    }
};

template <class T, int N>
T get_bytes(const unsigned char* bytes, audio_file_format::endianness endianness)
{
    if (endianness == audio_file_format::endianness::BIG)
        return byte_getter<T, N, 0, audio_file_format::endianness::BIG>()(bytes);
    else
        return byte_getter<T, N, 0, audio_file_format::endianness::LITTLE>()(bytes);
}

template <class T, int N>
T get_bytes(const char* bytes, audio_file_format::endianness endianness)
{
    return get_bytes<T, N>(reinterpret_cast<const unsigned char*>(bytes), endianness);
}

// Byte Setter

template <class T, int N, int M, audio_file_format::endianness E>
struct byte_setter
{
    void operator()(T value, unsigned char* bytes)
    {
        bytes[M] = (value >> byte_shift<N, M, E>()) & 0xFF;
        byte_setter<T, N, M + 1, E>()(value, bytes);
    }
};

template <class T, int N, audio_file_format::endianness E>
struct byte_setter<T, N, N, E>
{
    void operator()(T value, unsigned char* bytes) {}
};

template <int N, class T>
void set_bytes(T value, audio_file_format::endianness endianness, unsigned char* bytes)
{
    if (endianness == audio_file_format::endianness::BIG)
        return byte_setter<T, N, 0, audio_file_format::endianness::BIG>()(value, bytes);
    else
        return byte_setter<T, N, 0, audio_file_format::endianness::LITTLE>()(value, bytes);
}

HISSTOOLS_NAMESPACE_END()

#endif /* HISSTOOLS_AUDIO_FILE_UTILITIES_HPP */
