
#ifndef HISSTOOLS_AUDIO_FILE_EXTENDED_DOUBLE_HPP
#define HISSTOOLS_AUDIO_FILE_EXTENDED_DOUBLE_HPP

#include <cmath>
#include <limits>

#include "../namespace.hpp"
#include "utilities.hpp"
#include "base.hpp"

HISSTOOLS_NAMESPACE_START()

struct extended_double_convertor
{
    using Endianness = base_audio_file::endianness;
    
    double infinity() { return std::numeric_limits<double>::infinity(); }
    double quiet_NaN() { return std::numeric_limits<double>::quiet_NaN(); }
    
    uint32_t get_u32(const char* bytes, Endianness endianness)
    {
        return get_bytes<uint32_t, 4>(bytes, endianness);
    }
    
    uint16_t get_u16(const char* bytes, Endianness endianness)
    {
        return get_bytes<uint16_t, 2>(bytes, endianness);
    }
    
    double operator()(const char* bytes)
    {
        // Get double from big-endian IEEE 80-bit extended floating point format
        
        bool     sign        = get_u16(bytes + 0, Endianness::BIG) & 0x8000;
        int32_t  exponent    = get_u16(bytes + 0, Endianness::BIG) & 0x7FFF;
        uint32_t hi_mantissa = get_u32(bytes + 2, Endianness::BIG);
        uint32_t lo_mantissa = get_u32(bytes + 6, Endianness::BIG);
        
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
    
    void operator()(unsigned char bytes[10], double x)
    {
        auto double_to_uint32 = [](double x)
        {
            return static_cast<uint32_t>((((int32_t)(x - 2147483648.0)) + 2147483647L) + 1);
        };
        
        bool     sign        = 0;
        int32_t  exponent    = 0;
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
        
        set_bytes<2>(exponent, Endianness::BIG, bytes + 0);
        set_bytes<4>(hi_mantissa, Endianness::BIG, bytes + 2);
        set_bytes<4>(lo_mantissa, Endianness::BIG, bytes + 6);
    }
};

HISSTOOLS_NAMESPACE_END()

#endif /* HISSTOOLS_AUDIO_FILE_EXTENDED_DOUBLE_HPP */
