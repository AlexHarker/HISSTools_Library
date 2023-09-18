
#ifndef HISSTOOLS_LIBRARY_AUDIO_FILE_FORMAT_HPP
#define HISSTOOLS_LIBRARY_AUDIO_FILE_FORMAT_HPP

#include <cstdint>
#include <vector>

#include "../namespace.hpp"

HISSTOOLS_LIBRARY_NAMESPACE_START()

class audio_file_format
{
public:
    
    enum class file_type    { none, aiff, aifc, wave };
    enum class pcm_format   { int8, int16, int24, int32, float32, float64 };
    enum class numeric_type { integer, floating };
    enum class endian_type  { little, big };
    
    audio_file_format()
    : m_file_type(file_type::none)
    , m_pcm_format(pcm_format::int16)
    , m_endianness(endian_type::little)
    , m_valid(false)
    {}
    
    audio_file_format(file_type type)
    : m_file_type(type)
    , m_pcm_format(pcm_format::int16)
    , m_endianness(type == file_type::wave ? endian_type::little : endian_type::big)
    , m_valid(false)
    {}
    
    audio_file_format(file_type type, pcm_format format, endian_type endianness)
    : m_file_type(type)
    , m_pcm_format(format)
    , m_endianness(endianness)
    , m_valid(get_validity())
    {}
    
    audio_file_format(file_type type, numeric_type num_type, uint16_t bit_depth, endian_type endianness)
    : m_file_type(type)
    , m_pcm_format(pcm_format::int16)
    , m_endianness(endianness)
    , m_valid(false)
    {
        struct valid_format
        {
            numeric_type m_type;
            uint16_t m_depth;
            pcm_format m_format;
        };
        
        std::vector<valid_format> valid_formats =
        {
            { numeric_type::integer,   8, pcm_format::int8 },
            { numeric_type::integer,   8, pcm_format::int8 },
            { numeric_type::integer,  16, pcm_format::int16 },
            { numeric_type::integer,  24, pcm_format::int24 },
            { numeric_type::integer,  32, pcm_format::int32 },
            { numeric_type::floating, 32, pcm_format::float32 },
            { numeric_type::floating, 64, pcm_format::float64 },
        };
        
        for (int i = 0; i < valid_formats.size(); i++)
        {
            if (num_type == valid_formats[i].m_type && bit_depth == valid_formats[i].m_depth)
            {
                m_pcm_format = valid_formats[i].m_format;
                m_valid = get_validity();
                break;
            }
        }
    }
    
    bool is_valid() const                   { return m_valid; }
    
    file_type get_file_type() const          { return m_file_type; }
    pcm_format get_pcm_format() const        { return m_pcm_format; }
    numeric_type get_numeric_type() const    { return find_numeric_type(get_pcm_format()); }
    
    uint16_t bit_depth() const              { return find_bit_depth(get_pcm_format()); }
    uint16_t byte_depth() const             { return bit_depth() / 8; }
    
    endian_type header_endianness() const
    {
        return m_file_type == file_type::wave ? m_endianness : endian_type::big;
    }
    
    endian_type audio_endianness() const
    {
        return m_endianness;
    }
    
    static uint16_t find_bit_depth(pcm_format format)
    {
        switch (format)
        {
            case pcm_format::int8:       return 8;
            case pcm_format::int16:      return 16;
            case pcm_format::int24:      return 24;
            case pcm_format::int32:      return 32;
            case pcm_format::float32:    return 32;
            case pcm_format::float64:    return 64;
                
            default:                     return 16;
        }
    }
    
    static numeric_type find_numeric_type(pcm_format format)
    {
        switch (format)
        {
            case pcm_format::int8:
            case pcm_format::int16:
            case pcm_format::int24:
            case pcm_format::int32:
                return numeric_type::integer;
                
            case pcm_format::float32:
            case pcm_format::float64:
                return numeric_type::floating;
                
            default:
                return numeric_type::integer;
        }
    }
    
private:
    
    bool get_validity()
    {
        // If there's no file type then the format is invalid
        
        if (m_file_type == file_type::none)
            return false;
        
        // AIFF doesn't support float types or little-endianness
        
        if (m_file_type == file_type::aiff && find_numeric_type(m_pcm_format) == numeric_type::floating)
            return false;
        
        if (m_file_type == file_type::aiff && m_endianness != endian_type::big)
            return false;
        
        // AIFC only supports little=endianness for 16 bit integers
        
        if (m_file_type == file_type::aifc && m_endianness != endian_type::big && m_pcm_format != pcm_format::int16)
            return false;
        
        return true;
    }
    
    file_type m_file_type;
    pcm_format m_pcm_format;
    endian_type m_endianness;
    bool m_valid;
};

HISSTOOLS_LIBRARY_NAMESPACE_END()

#endif /* HISSTOOLS_LIBRARY_AUDIO_FILE_FORMAT_HPP */
