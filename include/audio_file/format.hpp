
#ifndef HISSTOOLS_AUDIO_FILE_FORMAT_HPP
#define HISSTOOLS_AUDIO_FILE_FORMAT_HPP

#include <cstdint>
#include <vector>

#include "../namespace.hpp"

HISSTOOLS_NAMESPACE_START()

class audio_file_format
{
public:
    
    enum class file_type    { NONE, AIFF, AIFC, WAVE };
    enum class pcm_format   { INT8, INT16, INT24, INT32, FLOAT32, FLOAT64 };
    enum class numeric_type { INTEGER, FLOAT };
    enum class endianness   { LITTLE, BIG };
    
    audio_file_format()
    : m_file_type(file_type::NONE)
    , m_pcm_format(pcm_format::INT16)
    , m_endianness(endianness::LITTLE)
    , m_valid(false)
    {}
    
    audio_file_format(file_type type)
    : m_file_type(type)
    , m_pcm_format(pcm_format::INT16)
    , m_endianness(type == file_type::WAVE ? endianness::LITTLE : endianness::BIG)
    , m_valid(false)
    {}
    
    audio_file_format(file_type type, pcm_format format, endianness endianity)
    : m_file_type(type)
    , m_pcm_format(format)
    , m_endianness(endianity)
    , m_valid(get_validity())
    {}
    
    audio_file_format(file_type type, numeric_type num_type, uint16_t bit_depth, endianness endianity)
    : m_file_type(type)
    , m_pcm_format(pcm_format::INT16)
    , m_endianness(endianity)
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
            { numeric_type::INTEGER,  8, pcm_format::INT8 },
            { numeric_type::INTEGER,  8, pcm_format::INT8 },
            { numeric_type::INTEGER, 16, pcm_format::INT16 },
            { numeric_type::INTEGER, 24, pcm_format::INT24 },
            { numeric_type::INTEGER, 32, pcm_format::INT32 },
            { numeric_type::FLOAT,   32, pcm_format::FLOAT32 },
            { numeric_type::FLOAT,   64, pcm_format::FLOAT64 },
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
    
    endianness header_endianness() const
    {
        return m_file_type == file_type::WAVE ? m_endianness : endianness::BIG;
    }
    
    endianness audio_endianness() const
    {
        return m_endianness;
    }
    
    static uint16_t find_bit_depth(pcm_format format)
    {
        switch (format)
        {
            case pcm_format::INT8:       return 8;
            case pcm_format::INT16:      return 16;
            case pcm_format::INT24:      return 24;
            case pcm_format::INT32:      return 32;
            case pcm_format::FLOAT32:    return 32;
            case pcm_format::FLOAT64:    return 64;
                
            default:                     return 16;
        }
    }
    
    static numeric_type find_numeric_type(pcm_format format)
    {
        switch (format)
        {
            case pcm_format::INT8:
            case pcm_format::INT16:
            case pcm_format::INT24:
            case pcm_format::INT32:
                return numeric_type::INTEGER;
                
            case pcm_format::FLOAT32:
            case pcm_format::FLOAT64:
                return numeric_type::FLOAT;
                
            default:
                return numeric_type::INTEGER;
        }
    }
    
private:
    
    bool get_validity()
    {
        // If there's no file type then the format is invalid
        
        if (m_file_type == file_type::NONE)
            return false;
        
        // AIFF doesn't support float types or little-endianness
        
        if (m_file_type == file_type::AIFF && find_numeric_type(m_pcm_format) == numeric_type::FLOAT)
            return false;
        
        if (m_file_type == file_type::AIFF && m_endianness != endianness::BIG)
            return false;
        
        // AIFC only supports little=endianness for 16 bit integers
        
        if (m_file_type == file_type::AIFC && m_endianness != endianness::BIG && m_pcm_format != pcm_format::INT16)
            return false;
        
        return true;
    }
    
    file_type m_file_type;
    pcm_format m_pcm_format;
    endianness m_endianness;
    bool m_valid;
};

HISSTOOLS_NAMESPACE_END()

#endif /* HISSTOOLS_AUDIO_FILE_FORMAT_HPP */
