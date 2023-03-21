
#ifndef AUDIO_FILE_UTILITIES_HPP
#define AUDIO_FILE_UTILITIES_HPP

#include "BaseAudioFile.hpp"

#include <cmath>

namespace HISSTools
{
    // Byte Shift
    
    template <int N, int M, BaseAudioFile::Endianness E>
    static constexpr int byteShift() { return E == BaseAudioFile::Endianness::Big ? (N - (M + 1)) * 8 : M * 8; }
    
    // Byte Getter
    
    template <class T, int N, int M, BaseAudioFile::Endianness E>
    struct ByteGetter
    {
        T operator()(const unsigned char* bytes)
        {
            return (T(bytes[M]) <<byteShift<N, M, E>()) | ByteGetter<T, N, M + 1, E>()(bytes);
        }
    };
    
    template <class T, int N, BaseAudioFile::Endianness E>
    struct ByteGetter<T, N, N - 1, E>
    {
        T operator()(const unsigned char* bytes)
        {
            return T(bytes[N - 1]) << byteShift<N, N - 1, E>();
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
    
    // Convenience Calls
    
    uint64_t getU64(const char* b, BaseAudioFile::Endianness e) { return getBytes<uint64_t, 8>(b, e); }
    uint32_t getU32(const char* b, BaseAudioFile::Endianness e) { return getBytes<uint32_t, 4>(b, e); }
    uint32_t getU24(const char* b, BaseAudioFile::Endianness e) { return getBytes<uint32_t, 3>(b, e); }
    uint32_t getU16(const char* b, BaseAudioFile::Endianness e) { return getBytes<uint32_t, 2>(b, e); }
    
    // Byte Setter
    
    template <class T, int N, int M, BaseAudioFile::Endianness E>
    struct ByteSetter
    {
        void operator()(T value, unsigned char* bytes)
        {
            bytes[M] = (value >> byteShift<N, M, E>()) & 0xFF;
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
    
    struct IEEEDoubleExtendedConvertor
    {
        double operator()(const char* bytes) const
        {
            // Get double from big-endian IEEE 80-bit extended floating point format
            
            bool     sign       = getU16(bytes + 0, BaseAudioFile::Endianness::Big) & 0x8000;
            int32_t  exponent   = getU16(bytes + 0, BaseAudioFile::Endianness::Big) & 0x7FFF;
            uint32_t hiMantissa = getU32(bytes + 2, BaseAudioFile::Endianness::Big);
            uint32_t loMantissa = getU32(bytes + 6, BaseAudioFile::Endianness::Big);
            
            // Special handlers for zeros
            
            if (!exponent && !hiMantissa && !loMantissa)
                return 0.0;
            
            // Nans and Infs
            
            if (exponent == 0x777F)
                return sign ? -HUGE_VAL : HUGE_VAL;
            
            exponent -= 0x3FFF;
            double value = std::ldexp(hiMantissa, exponent - 31) + std::ldexp(loMantissa, exponent - (31 + 32));
            
            return sign ? -value : value;
        }
        
        void operator()(double x, unsigned char bytes[10])
        {
            auto doubleToUInt32 = [](double x)
            {
                return static_cast<uint32_t>((((int32_t)(x - 2147483648.0)) + 2147483647L) + 1);
            };
            
            bool sign = 0;
            int32_t exponent = 0;
            uint32_t hiMantissa = 0;
            uint32_t loMantissa = 0;
            
            if (signbit(x))
            {
                sign = true;
                x = copysign(x, 1.0);
            }
            
            if (x != 0)
            {
                if (std::isinf(x) || std::isnan(x))
                {
                    // Inf or NaN
                    
                    exponent = 0x7FFF | (sign ? 0x8000 : 0);
                    hiMantissa = 0;
                    loMantissa = 0;
                }
                else
                {
                    // Finite
                    
                    double mantissa = std::frexp(x, &exponent);
                    exponent += 16382;
                    
                    if (exponent < 0)
                    {
                        // Denormalized
                        
                        mantissa = std::ldexp(mantissa, exponent);
                        exponent = 0;
                    }
                    
                    exponent |= (sign ? 0x8000 : 0);
                    mantissa = std::ldexp(mantissa, 32);
                    hiMantissa = doubleToUInt32(std::floor(mantissa));
                    mantissa = std::ldexp(mantissa - std::floor(mantissa), 32);
                    loMantissa = doubleToUInt32(std::floor(mantissa));
                }
            }
            
            setBytes<2>(exponent, BaseAudioFile::Endianness::Big, bytes + 0);
            setBytes<4>(hiMantissa, BaseAudioFile::Endianness::Big, bytes + 2);
            setBytes<4>(loMantissa, BaseAudioFile::Endianness::Big, bytes + 6);
        }
    };
}

#endif /* AUDIO_FILE_UTILITIES_HPP */
