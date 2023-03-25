
#ifndef AUDIO_FILE_UTILITIES_HPP
#define AUDIO_FILE_UTILITIES_HPP

#include "BaseAudioFile.hpp"

namespace HISSTools
{
    // Byte Shift
    
    template <int N, int M, BaseAudioFile::Endianness E>
    static constexpr int byte_shift() { return E == BaseAudioFile::Endianness::Big ? (N - (M + 1)) * 8 : M * 8; }
    
    // Byte Getter
    
    template <class T, int N, int M, BaseAudioFile::Endianness E>
    struct byte_getter
    {
        T operator()(const unsigned char* bytes)
        {
            return (T(bytes[M]) << byte_shift<N, M, E>()) | byte_getter<T, N, M + 1, E>()(bytes);
        }
    };
    
    template <class T, int N, BaseAudioFile::Endianness E>
    struct byte_getter<T, N, N - 1, E>
    {
        T operator()(const unsigned char* bytes)
        {
            return T(bytes[N - 1]) << byte_shift<N, N - 1, E>();
        }
    };
    
    template <class T, int N>
    T get_bytes(const unsigned char* bytes, BaseAudioFile::Endianness endianness)
    {
        if (endianness == BaseAudioFile::Endianness::Big)
            return byte_getter<T, N, 0, BaseAudioFile::Endianness::Big>()(bytes);
        else
            return byte_getter<T, N, 0, BaseAudioFile::Endianness::Little>()(bytes);
    }
    
    template <class T, int N>
    T get_bytes(const char* bytes, BaseAudioFile::Endianness endianness)
    {
        return get_bytes<T, N>(reinterpret_cast<const unsigned char *>(bytes), endianness);
    }
    
    // Byte Setter
    
    template <class T, int N, int M, BaseAudioFile::Endianness E>
    struct byte_setter
    {
        void operator()(T value, unsigned char* bytes)
        {
            bytes[M] = (value >> byte_shift<N, M, E>()) & 0xFF;
            byte_setter<T, N, M + 1, E>()(value, bytes);
        }
    };
    
    template <class T, int N, BaseAudioFile::Endianness E>
    struct byte_setter<T, N, N, E>
    {
        void operator()(T value, unsigned char* bytes) {}
    };
    
    template <int N, class T>
    void set_bytes(T value, BaseAudioFile::Endianness endianness, unsigned char* bytes)
    {
        if (endianness == BaseAudioFile::Endianness::Big)
            return byte_setter<T, N, 0, BaseAudioFile::Endianness::Big>()(value, bytes);
        else
            return byte_setter<T, N, 0, BaseAudioFile::Endianness::Little>()(value, bytes);
    }
}

#endif /* AUDIO_FILE_UTILITIES_HPP */
