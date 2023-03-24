
#ifndef AUDIO_FILE_UTILITIES_HPP
#define AUDIO_FILE_UTILITIES_HPP

#include "BaseAudioFile.hpp"

#include <cmath>
#include <limits>

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
    
    struct IEEEDoubleExtendedConvertor
    {
        using Endianness = BaseAudioFile::Endianness;
        
        double infinity() { return std::numeric_limits<double>::infinity(); }
        double quiet_NaN() { return std::numeric_limits<double>::quiet_NaN(); }
        
        uint32_t getU32(const char* bytes, Endianness endianness)
        {
            return get_bytes<uint32_t, 4>(bytes, endianness);
        }
        
        uint16_t getU16(const char* bytes, Endianness endianness)
        {
            return get_bytes<uint16_t, 2>(bytes, endianness);
        }
        
        double operator()(const char* bytes)
        {
            // Get double from big-endian IEEE 80-bit extended floating point format
            
            bool     sign        = getU16(bytes + 0, Endianness::Big) & 0x8000;
            int32_t  exponent    = getU16(bytes + 0, Endianness::Big) & 0x7FFF;
            uint32_t hi_mantissa = getU32(bytes + 2, Endianness::Big);
            uint32_t lo_mantissa = getU32(bytes + 6, Endianness::Big);
            
            // Special handlers for zeros
            
            if (!exponent && !hi_mantissa && !lo_mantissa)
                return 0.0;
            
            // Nans and Infs
            
            if (exponent == 0x7FFF)
            {
                if (hi_mantissa || lo_mantissa)
                    return quiet_NaN();
                
                return sign ? -infinity() : infinity();
            }
            
            exponent -= 0x3FFF;
            double value = std::ldexp(hi_mantissa, exponent - 0x1F) + std::ldexp(lo_mantissa, exponent - 0x3F);
            
            return sign ? -value : value;
        }
        
        void operator()(double x, unsigned char bytes[10])
        {
            auto double_to_uint32 = [](double x)
            {
                return static_cast<uint32_t>((((int32_t)(x - 2147483648.0)) + 2147483647L) + 1);
            };
            
            bool sign = 0;
            int32_t exponent = 0;
            uint32_t hi_mantissa = 0;
            uint32_t lo_mantissa = 0;
            
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
                    hi_mantissa = lo_mantissa = std::isnan(x) ? 0xFFFFFFFF : 0;
                }
                else
                {
                    // Finite
                    
                    double mantissa = std::frexp(x, &exponent);
                    exponent += 0x3FFE;
                    
                    if (exponent < 0)
                    {
                        // Denormalized
                        
                        mantissa = std::ldexp(mantissa, exponent);
                        exponent = 0;
                    }
                    
                    exponent |= (sign ? 0x8000 : 0);
                    mantissa = std::ldexp(mantissa, 0x20);
                    hi_mantissa = double_to_uint32(std::floor(mantissa));
                    mantissa = std::ldexp(mantissa - std::floor(mantissa), 0x20);
                    lo_mantissa = double_to_uint32(std::floor(mantissa));
                }
            }
            
            set_bytes<2>(exponent, Endianness::Big, bytes + 0);
            set_bytes<4>(hi_mantissa, Endianness::Big, bytes + 2);
            set_bytes<4>(lo_mantissa, Endianness::Big, bytes + 6);
        }
    };
}

#endif /* AUDIO_FILE_UTILITIES_HPP */
