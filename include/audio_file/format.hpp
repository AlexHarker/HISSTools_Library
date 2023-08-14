
#ifndef HISSTOOLS_AUDIO_FILE_FORMAT_HPP
#define HISSTOOLS_AUDIO_FILE_FORMAT_HPP

#include <cstdint>
#include <vector>

#include "../namespace.hpp"

HISSTOOLS_NAMESPACE_START()

class AudioFileFormat
{
public:
    
    enum class FileType     { None, AIFF, AIFC, WAVE };
    enum class PCMFormat    { Int8, Int16, Int24, Int32, Float32, Float64 };
    enum class NumericType  { Integer, Float };
    enum class Endianness   { Little, Big };
    
    AudioFileFormat()
    : m_file_type(FileType::None)
    , m_pcm_format(PCMFormat::Int16)
    , m_endianness(Endianness::Little)
    , m_valid(false)
    {}
    
    AudioFileFormat(FileType type)
    : m_file_type(type)
    , m_pcm_format(PCMFormat::Int16)
    , m_endianness(type == FileType::WAVE ? Endianness::Little : Endianness::Big)
    , m_valid(false)
    {}
    
    AudioFileFormat(FileType type, PCMFormat format, Endianness endianness)
    : m_file_type(type)
    , m_pcm_format(format)
    , m_endianness(endianness)
    , m_valid(get_validity())
    {}
    
    AudioFileFormat(FileType type, NumericType numeric_type, uint16_t bit_depth, Endianness endianness)
    : m_file_type(type)
    , m_pcm_format(PCMFormat::Int16)
    , m_endianness(endianness)
    , m_valid(false)
    {
        struct valid_format
        {
            NumericType m_type;
            uint16_t m_depth;
            PCMFormat m_format;
        };
        
        std::vector<valid_format> valid_formats =
        {
            { NumericType::Integer,  8, PCMFormat::Int8 },
            { NumericType::Integer,  8, PCMFormat::Int8 },
            { NumericType::Integer, 16, PCMFormat::Int16 },
            { NumericType::Integer, 24, PCMFormat::Int24 },
            { NumericType::Integer, 32, PCMFormat::Int32 },
            { NumericType::Float,   32, PCMFormat::Float32 },
            { NumericType::Float,   64, PCMFormat::Float64 },
        };
        
        for (int i = 0; i < valid_formats.size(); i++)
        {
            if (numeric_type == valid_formats[i].m_type && bit_depth == valid_formats[i].m_depth)
            {
                m_pcm_format = valid_formats[i].m_format;
                m_valid = get_validity();
                break;
            }
        }
    }
    
    bool is_valid() const                   { return m_valid; }
    
    FileType get_file_type() const          { return m_file_type; }
    PCMFormat get_pcm_format() const        { return m_pcm_format; }
    NumericType get_numeric_type() const    { return find_numeric_type(get_pcm_format()); }
    
    uint16_t bit_depth() const              { return find_bit_depth(get_pcm_format()); }
    uint16_t byte_depth() const             { return bit_depth() / 8; }
    
    Endianness header_endianness() const
    {
        return m_file_type == FileType::WAVE ? m_endianness : Endianness::Big;
    }
    
    Endianness audio_endianness() const
    {
        return m_endianness;
    }
    
    static uint16_t find_bit_depth(PCMFormat format)
    {
        switch (format)
        {
            case PCMFormat::Int8:       return 8;
            case PCMFormat::Int16:      return 16;
            case PCMFormat::Int24:      return 24;
            case PCMFormat::Int32:      return 32;
            case PCMFormat::Float32:    return 32;
            case PCMFormat::Float64:    return 64;
                
            default:                    return 16;
        }
    }
    
    static NumericType find_numeric_type(PCMFormat format)
    {
        switch (format)
        {
            case PCMFormat::Int8:
            case PCMFormat::Int16:
            case PCMFormat::Int24:
            case PCMFormat::Int32:
                return NumericType::Integer;
                
            case PCMFormat::Float32:
            case PCMFormat::Float64:
                return NumericType::Float;
                
            default:
                return NumericType::Integer;
        }
    }
    
private:
    
    bool get_validity()
    {
        // If there's no file type then the format is invalid
        
        if (m_file_type == FileType::None)
            return false;
        
        // AIFF doesn't support float types or little-endianness
        
        if (m_file_type == FileType::AIFF && find_numeric_type(m_pcm_format) == NumericType::Float)
            return false;
        
        if (m_file_type == FileType::AIFF && m_endianness != Endianness::Big)
            return false;
        
        // AIFC only supports little=endianness for 16 bit integers
        
        if (m_file_type == FileType::AIFC && m_endianness != Endianness::Big && m_pcm_format != PCMFormat::Int16)
            return false;
        
        return true;
    }
    
    FileType m_file_type;
    PCMFormat m_pcm_format;
    Endianness m_endianness;
    bool m_valid;
};

HISSTOOLS_NAMESPACE_END()

#endif /* HISSTOOLS_AUDIO_FILE_FORMAT_HPP */
