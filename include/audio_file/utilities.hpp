
#ifndef HISSTOOLS_LIBRARY_AUDIO_FILE_UTILITIES_HPP
#define HISSTOOLS_LIBRARY_AUDIO_FILE_UTILITIES_HPP

#include "../namespace.hpp"
#include "format.hpp"

HISSTOOLS_LIBRARY_NAMESPACE_START()

// Byte Shift

template <int N, int M, audio_file_format::endian_type E>
static constexpr int byte_shift()
{
    if (E == audio_file_format::endian_type::big)
        return (N - (M + 1)) * 8;
    else
        return M * 8;
}

// Byte Getter

template <class T, int N, int M, audio_file_format::endian_type E>
struct byte_getter
{
    T operator()(const unsigned char* bytes)
    {
        return (T(bytes[M - 1]) << byte_shift<N, M - 1, E>()) | byte_getter<T, N, M + 1, E>()(bytes);
    }
};

template <class T, int N, audio_file_format::endian_type E>
struct byte_getter<T, N, N, E>
{
    T operator()(const unsigned char* bytes)
    {
        return T(bytes[N - 1]) << byte_shift<N, N - 1, E>();
    }
};

template <class T, int N>
T get_bytes(const unsigned char* bytes, audio_file_format::endian_type endianness)
{
    if (endianness == audio_file_format::endian_type::big)
        return byte_getter<T, N, 1, audio_file_format::endian_type::big>()(bytes);
    else
        return byte_getter<T, N, 1, audio_file_format::endian_type::little>()(bytes);
}

template <class T, int N>
T get_bytes(const char* bytes, audio_file_format::endian_type endianness)
{
    return get_bytes<T, N>(reinterpret_cast<const unsigned char*>(bytes), endianness);
}

// Byte Setter

template <class T, int N, int M, audio_file_format::endian_type E>
struct byte_setter
{
    void operator()(T value, unsigned char* bytes)
    {
        bytes[M] = (value >> byte_shift<N, M, E>()) & 0xFF;
        byte_setter<T, N, M + 1, E>()(value, bytes);
    }
};

template <class T, int N, audio_file_format::endian_type E>
struct byte_setter<T, N, N, E>
{
    void operator()(T value, unsigned char* bytes) {}
};

template <int N, class T>
void set_bytes(T value, audio_file_format::endian_type endianness, unsigned char* bytes)
{
    if (endianness == audio_file_format::endian_type::big)
        return byte_setter<T, N, 0, audio_file_format::endian_type::big>()(value, bytes);
    else
        return byte_setter<T, N, 0, audio_file_format::endian_type::little>()(value, bytes);
}

HISSTOOLS_LIBRARY_NAMESPACE_END()

#endif /* HISSTOOLS_LIBRARY_AUDIO_FILE_UTILITIES_HPP */
