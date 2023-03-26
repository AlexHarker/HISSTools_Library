
#ifndef AIFC_COMPRESSION_HPP
#define AIFC_COMPRESSION_HPP

#include "AudioFileFormat.hpp"

namespace HISSTools
{
    struct AIFCCompression
    {
        enum class Type  { Unknown, None, Sowt, Float32, Float64 };

        using FileType = AudioFileFormat::FileType;
        using PCMFormat = AudioFileFormat::PCMFormat;
        using NumericType = AudioFileFormat::NumericType;
        using Endianness = AudioFileFormat::Endianness;

        static bool match_tag(const char* a, const char* b)
        {
            return (strncmp(a, b, 4) == 0);
        }

        static AudioFileFormat to_format(const char* tag, uint16_t bit_depth)
        {
            if (match_tag(tag, "NONE"))
                return AudioFileFormat(FileType::AIFC, NumericType::Integer, bit_depth, Endianness::Big);
            
            if (match_tag(tag, "twos"))
                return AudioFileFormat(FileType::AIFC, PCMFormat::Int16, Endianness::Big);
            
            if (match_tag(tag, "sowt") || match_tag(tag, "SOWT"))
                return AudioFileFormat(FileType::AIFC, PCMFormat::Int16, Endianness::Little);
            
            if (match_tag(tag, "in24") || match_tag(tag, "IN24"))
                return AudioFileFormat(FileType::AIFC, PCMFormat::Int24, Endianness::Big);
            
            if (match_tag(tag, "in32") || match_tag(tag, "IN32"))
                return AudioFileFormat(FileType::AIFC, PCMFormat::Int32, Endianness::Big);
            
            if (match_tag(tag, "fl32") || match_tag(tag, "FL32"))
                return AudioFileFormat(FileType::AIFC, PCMFormat::Float32, Endianness::Big);
            
            if (match_tag(tag, "fl64") || match_tag(tag, "FL64"))
                return AudioFileFormat(FileType::AIFC, PCMFormat::Float64, Endianness::Big);
                
            return AudioFileFormat();
        }
        
        static const char* to_tag(const AudioFileFormat& format)
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
        
        static const char* to_string(const AudioFileFormat& format)
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
        
        static Type to_type(const AudioFileFormat& format)
        {
            if (!format.is_valid() || format.getFileType() == FileType::WAVE)
                return Type::Unknown;
            
            switch (format.getPCMFormat())
            {
                case PCMFormat::Int16:
                    if (format.getAudioEndianness() == Endianness::Little)
                        return Type::Sowt;
                    else
                        return Type::None;
                case PCMFormat::Float32:
                    return Type::Float32;
                case PCMFormat::Float64:
                    return Type::Float64;
                default:
                    return Type::None;
            }
        }
    };
}

#endif /* AIFC_COMPRESSION_HPP */
