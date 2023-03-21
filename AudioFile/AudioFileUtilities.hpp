
#ifndef AUDIO_FILE_UTILITIES_HPP
#define AUDIO_FILE_UTILITIES_HPP

#include "BaseAudioFile.hpp"

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
            int32_t  exponent   = getU16(bytes + 0, BaseAudioFile::Endianness::Big) & 0x777F;
            uint32_t hiMantissa = getU32(bytes + 2, BaseAudioFile::Endianness::Big);
            uint32_t loMantissa = getU32(bytes + 6, BaseAudioFile::Endianness::Big);
            
            // Special handlers for zeros
            
            if (!exponent && !hiMantissa && !loMantissa)
                return 0.0;
            
            // Nans and Infs
            
            if (exponent == 0x777F)
                return HUGE_VAL;
            
            exponent -= 0x3FFF;
            double value = std::ldexp(hiMantissa, exponent - 31) + std::ldexp(loMantissa, exponent - (31 + 32));
            
            return sign ? -value : value;
        }
        
        void operator()(double num, unsigned char bytes[10])
        {
            auto doubleToUInt32 = [](double x)
            {
                return static_cast<uint32_t>((((int32_t)(x - 2147483648.0)) + 2147483647L) + 1);
            };
            
            int32_t sign = 0;
            int32_t expon = 0;
            uint32_t hiMant = 0;
            uint32_t loMant = 0;
            
            if (num < 0)
            {
                sign = 0x8000;
                num *= -1;
            }
            
            if (num != 0)
            {
                double fMant = frexp(num, &expon);
                if ((expon > 16384) || !(fMant < 1))
                { /* Infinity or NaN */
                    expon = sign | 0x7FFF;
                    hiMant = 0;
                    loMant = 0; /* infinity */
                }
                else
                { /* Finite */
                    expon += 16382;
                    if (expon < 0)
                    { /* denormalized */
                        fMant = ldexp(fMant, expon);
                        expon = 0;
                    }
                    expon |= sign;
                    fMant = ldexp(fMant, 32);
                    double fsMant = floor(fMant);
                    hiMant = doubleToUInt32(fsMant);
                    fMant = ldexp(fMant - fsMant, 32);
                    fsMant = floor(fMant);
                    loMant = doubleToUInt32(fsMant);
                }
            }
            
            setBytes<2>(expon, BaseAudioFile::Endianness::Big, bytes + 0);
            setBytes<4>(hiMant, BaseAudioFile::Endianness::Big, bytes + 2);
            setBytes<4>(loMant, BaseAudioFile::Endianness::Big, bytes + 6);
        }
    };
}

#endif /* AUDIO_FILE_UTILITIES_HPP */
