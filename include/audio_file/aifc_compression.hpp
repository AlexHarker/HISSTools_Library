
#ifndef HISSTOOLS_AUDIO_FILE_AIFC_COMPRESSION_HPP
#define HISSTOOLS_AUDIO_FILE_AIFC_COMPRESSION_HPP

#include "../namespace.hpp"
#include "format.hpp"

HISSTOOLS_NAMESPACE_START()

struct AIFCCompression
{
    enum class Type  { Unknown, None, Sowt, Float32, Float64 };
    
    using FileType = audio_file_format::file_type;
    using PCMFormat = audio_file_format::pcm_format;
    using NumericType = audio_file_format::numeric_type;
    using Endianness = audio_file_format::endianness;
    
    static bool match_tag(const char* a, const char* b)
    {
        return (strncmp(a, b, 4) == 0);
    }
    
    static audio_file_format to_format(const char* tag, uint16_t bit_depth)
    {
        if (match_tag(tag, "NONE"))
            return audio_file_format(FileType::AIFC, NumericType::INTEGER, bit_depth, Endianness::BIG);
        
        if (match_tag(tag, "twos"))
            return audio_file_format(FileType::AIFC, PCMFormat::INT16, Endianness::BIG);
        
        if (match_tag(tag, "sowt") || match_tag(tag, "SOWT"))
            return audio_file_format(FileType::AIFC, PCMFormat::INT16, Endianness::LITTLE);
        
        if (match_tag(tag, "in24") || match_tag(tag, "IN24"))
            return audio_file_format(FileType::AIFC, PCMFormat::INT24, Endianness::BIG);
        
        if (match_tag(tag, "in32") || match_tag(tag, "IN32"))
            return audio_file_format(FileType::AIFC, PCMFormat::INT32, Endianness::BIG);
        
        if (match_tag(tag, "fl32") || match_tag(tag, "FL32"))
            return audio_file_format(FileType::AIFC, PCMFormat::FLOAT32, Endianness::BIG);
        
        if (match_tag(tag, "fl64") || match_tag(tag, "FL64"))
            return audio_file_format(FileType::AIFC, PCMFormat::FLOAT64, Endianness::BIG);
        
        return audio_file_format();
    }
    
    static const char* to_tag(const audio_file_format& format)
    {
        switch (to_type(format))
        {
            case Type::None:
                return "NONE";
            case Type::Sowt:
                return "sowt";
            case Type::Float32:
                return "fl32";
            case Type::Float64:
                return "fl64";
            default:
                return "FIXFIXFIX";
        }
    }
    
    static const char* to_string(const audio_file_format& format)
    {
        switch (to_type(format))
        {
            case Type::None:
            case Type::Sowt:
                return "not compressed";
            case Type::Float32:
                return "32-bit floating point";
            case Type::Float64:
                return "64-bit floating point";
            default:
                return "FIXFIXFIX";
        }
    }
    
    static Type to_type(const audio_file_format& format)
    {
        if (!format.is_valid() || format.get_file_type() == FileType::WAVE)
            return Type::Unknown;
        
        switch (format.get_pcm_format())
        {
            case PCMFormat::INT16:
                if (format.audio_endianness() == Endianness::LITTLE)
                    return Type::Sowt;
                else
                    return Type::None;
            case PCMFormat::FLOAT32:
                return Type::Float32;
            case PCMFormat::FLOAT64:
                return Type::Float64;
            default:
                return Type::None;
        }
    }
};

HISSTOOLS_NAMESPACE_END()

#endif /* HISSTOOLS_AUDIO_FILE_AIFC_COMPRESSION_HPP */
