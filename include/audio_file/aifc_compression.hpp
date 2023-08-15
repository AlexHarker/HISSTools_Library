
#ifndef HISSTOOLS_AUDIO_FILE_AIFC_COMPRESSION_HPP
#define HISSTOOLS_AUDIO_FILE_AIFC_COMPRESSION_HPP

#include "../namespace.hpp"
#include "format.hpp"

HISSTOOLS_NAMESPACE_START()

struct aifc_compression
{
    enum class aifc_type  { unknown, none, sowt, float32, float64 };
    
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
            return audio_file_format(file_type::aifc, numeric_type::integer, bit_depth, endianness::big);
        
        if (match_tag(tag, "twos"))
            return audio_file_format(file_type::aifc, pcm_format::int16, endianness::big);
        
        if (match_tag(tag, "sowt") || match_tag(tag, "SOWT"))
            return audio_file_format(file_type::aifc, pcm_format::int16, endianness::little);
        
        if (match_tag(tag, "in24") || match_tag(tag, "IN24"))
            return audio_file_format(file_type::aifc, pcm_format::int24, endianness::big);
        
        if (match_tag(tag, "in32") || match_tag(tag, "IN32"))
            return audio_file_format(file_type::aifc, pcm_format::int32, endianness::big);
        
        if (match_tag(tag, "fl32") || match_tag(tag, "FL32"))
            return audio_file_format(file_type::aifc, pcm_format::float32, endianness::big);
        
        if (match_tag(tag, "fl64") || match_tag(tag, "FL64"))
            return audio_file_format(file_type::aifc, pcm_format::float64, endianness::big);
        
        return audio_file_format();
    }
    
    static const char* to_tag(const audio_file_format& format)
    {
        switch (to_type(format))
        {
            case aifc_type::none:
                return "NONE";
            case aifc_type::sowt:
                return "sowt";
            case aifc_type::float32:
                return "fl32";
            case aifc_type::float64:
                return "fl64";
            default:
                return "FIXFIXFIX";
        }
    }
    
    static const char* to_string(const audio_file_format& format)
    {
        switch (to_type(format))
        {
            case aifc_type::none:
            case aifc_type::sowt:
                return "not compressed";
            case aifc_type::float32:
                return "32-bit floating point";
            case aifc_type::float64:
                return "64-bit floating point";
            default:
                return "FIXFIXFIX";
        }
    }
    
    static aifc_type to_type(const audio_file_format& format)
    {
        if (!format.is_valid() || format.get_file_type() == file_type::wave)
            return aifc_type::unknown;
        
        switch (format.get_pcm_format())
        {
            case pcm_format::int16:
                if (format.audio_endianness() == endianness::little)
                    return aifc_type::sowt;
                else
                    return aifc_type::none;
            case pcm_format::float32:
                return aifc_type::float32;
            case pcm_format::float64:
                return aifc_type::float64;
            default:
                return aifc_type::none;
        }
    }
};

HISSTOOLS_NAMESPACE_END()

#endif /* HISSTOOLS_AUDIO_FILE_AIFC_COMPRESSION_HPP */
