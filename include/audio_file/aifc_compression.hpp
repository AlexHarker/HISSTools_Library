
#ifndef HISSTOOLS_AUDIO_FILE_AIFC_COMPRESSION_HPP
#define HISSTOOLS_AUDIO_FILE_AIFC_COMPRESSION_HPP

#include "../namespace.hpp"
#include "format.hpp"

HISSTOOLS_NAMESPACE_START()

struct aifc_compression
{
    enum class AIFC_TYPE  { UNKNOWN, NONE, SOWT, FLOAT32, FLOAT64 };
    
    using file_type = audio_file_format::file_type;
    using pcm_format = audio_file_format::pcm_format;
    using numeric_type = audio_file_format::numeric_type;
    using endianness = audio_file_format::endianness;
    
    static bool match_tag(const char* a, const char* b)
    {
        return (strncmp(a, b, 4) == 0);
    }
    
    static audio_file_format to_format(const char* tag, uint16_t bit_depth)
    {
        if (match_tag(tag, "NONE"))
            return audio_file_format(file_type::AIFC, numeric_type::INTEGER, bit_depth, endianness::BIG);
        
        if (match_tag(tag, "twos"))
            return audio_file_format(file_type::AIFC, pcm_format::INT16, endianness::BIG);
        
        if (match_tag(tag, "sowt") || match_tag(tag, "SOWT"))
            return audio_file_format(file_type::AIFC, pcm_format::INT16, endianness::LITTLE);
        
        if (match_tag(tag, "in24") || match_tag(tag, "IN24"))
            return audio_file_format(file_type::AIFC, pcm_format::INT24, endianness::BIG);
        
        if (match_tag(tag, "in32") || match_tag(tag, "IN32"))
            return audio_file_format(file_type::AIFC, pcm_format::INT32, endianness::BIG);
        
        if (match_tag(tag, "fl32") || match_tag(tag, "FL32"))
            return audio_file_format(file_type::AIFC, pcm_format::FLOAT32, endianness::BIG);
        
        if (match_tag(tag, "fl64") || match_tag(tag, "FL64"))
            return audio_file_format(file_type::AIFC, pcm_format::FLOAT64, endianness::BIG);
        
        return audio_file_format();
    }
    
    static const char* to_tag(const audio_file_format& format)
    {
        switch (to_type(format))
        {
            case AIFC_TYPE::NONE:
                return "NONE";
            case AIFC_TYPE::SOWT:
                return "sowt";
            case AIFC_TYPE::FLOAT32:
                return "fl32";
            case AIFC_TYPE::FLOAT64:
                return "fl64";
            default:
                return "FIXFIXFIX";
        }
    }
    
    static const char* to_string(const audio_file_format& format)
    {
        switch (to_type(format))
        {
            case AIFC_TYPE::NONE:
            case AIFC_TYPE::SOWT:
                return "not compressed";
            case AIFC_TYPE::FLOAT32:
                return "32-bit floating point";
            case AIFC_TYPE::FLOAT64:
                return "64-bit floating point";
            default:
                return "FIXFIXFIX";
        }
    }
    
    static AIFC_TYPE to_type(const audio_file_format& format)
    {
        if (!format.is_valid() || format.get_file_type() == file_type::WAVE)
            return AIFC_TYPE::UNKNOWN;
        
        switch (format.get_pcm_format())
        {
            case pcm_format::INT16:
                if (format.audio_endianness() == endianness::LITTLE)
                    return AIFC_TYPE::SOWT;
                else
                    return AIFC_TYPE::NONE;
            case pcm_format::FLOAT32:
                return AIFC_TYPE::FLOAT32;
            case pcm_format::FLOAT64:
                return AIFC_TYPE::FLOAT64;
            default:
                return AIFC_TYPE::NONE;
        }
    }
};

HISSTOOLS_NAMESPACE_END()

#endif /* HISSTOOLS_AUDIO_FILE_AIFC_COMPRESSION_HPP */
