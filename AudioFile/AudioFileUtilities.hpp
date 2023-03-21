
#ifndef AUDIO_FILE_UTILITIES_HPP
#define AUDIO_FILE_UTILITIES_HPP

#include "BaseAudioFile.hpp"

namespace HISSTools
{
    // Byte Getter
    
    template <class T, int N, int M, BaseAudioFile::Endianness E>
    struct ByteGetter
    {
        T operator()(const unsigned char* bytes)
        {
            return (T(bytes[M]) << BaseAudioFile::byteShift<N, M, E>()) | ByteGetter<T, N, M + 1, E>()(bytes);
        }
    };
    
    template <class T, int N, BaseAudioFile::Endianness E>
    struct ByteGetter<T, N, N - 1, E>
    {
        T operator()(const unsigned char* bytes)
        {
            return T(bytes[N - 1]) << BaseAudioFile::byteShift<N, N - 1, E>();
        }
    };
    
    template <class T, int N>
    T getBytes(const unsigned char* bytes, BaseAudioFile::Endianness endianness)
    {
        if (endianness == BaseAudioFile::Endianness::Big)
            return ByteGetter<T, N, 0, BaseAudioFile::Endianness::Big>()(bytes);
        else
            return ByteGetter<T, N, 0, BaseAudioFile::Endianness::Little>()(bytes);
    }
    
    template <class T, int N>
    T getBytes(const char* bytes, BaseAudioFile::Endianness endianness)
    {
        return getBytes<T, N>(reinterpret_cast<const unsigned char *>(bytes), endianness);
    }
    
    // Byte Setter
    
    template <class T, int N, int M, BaseAudioFile::Endianness E>
    struct ByteSetter
    {
        void operator()(T value, unsigned char* bytes)
        {
            bytes[M] = (value >> BaseAudioFile::byteShift<N, M, E>()) & 0xFF;
            ByteSetter<T, N, M + 1, E>()(value, bytes);
        }
    };
    
    template <class T, int N, BaseAudioFile::Endianness E>
    struct ByteSetter<T, N, N, E>
    {
        void operator()(T value, unsigned char* bytes) {}
    };
    
    template <int N, class T>
    void setBytes(T value, BaseAudioFile::Endianness endianness, unsigned char* bytes)
    {
        if (endianness == BaseAudioFile::Endianness::Big)
            return ByteSetter<T, N, 0, BaseAudioFile::Endianness::Big>()(value, bytes);
        else
            return ByteSetter<T, N, 0, BaseAudioFile::Endianness::Little>()(value, bytes);
    }
}

#endif /* AUDIO_FILE_UTILITIES_HPP */
