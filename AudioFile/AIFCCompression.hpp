
#ifndef AIFC_COMPRESSION_HPP
#define AIFC_COMPRESSION_HPP

#include "AudioFileFormat.hpp"

namespace HISSTools
{
    struct AIFCCompression
    {
        enum class Type  { Unknown, None, Twos, Sowt, Float32, Float64 };

        using FileType = AudioFileFormat::FileType;
        using PCMFormat = AudioFileFormat::PCMFormat;
        using NumericType = AudioFileFormat::NumericType;
        using Endianness = AudioFileFormat::Endianness;

        // FIX
        static bool matchTag(const char* a, const char* b) { return (strncmp(a, b, 4) == 0); }

        static AudioFileFormat getFormat(const char* tag, uint16_t bitDepth)
        {
            if (matchTag(tag, "NONE"))
                return AudioFileFormat(FileType::AIFC, NumericType::Integer, bitDepth, Endianness::Big);
            
            if (matchTag(tag, "twos"))
                return AudioFileFormat(FileType::AIFC, PCMFormat::Int16, Endianness::Big);
            
            if (matchTag(tag, "sowt") || matchTag(tag, "SOWT"))
                return AudioFileFormat(FileType::AIFC, PCMFormat::Int16, Endianness::Little);
            
            if (matchTag(tag, "fl32") || matchTag(tag, "FL32"))
                return AudioFileFormat(FileType::AIFC, PCMFormat::Float32, Endianness::Big);
            
            if (matchTag(tag, "fl64") || matchTag(tag, "FL64"))
                return AudioFileFormat(FileType::AIFC, PCMFormat::Float64, Endianness::Big);
                
            return AudioFileFormat();
        }
        
        static const char* getTag(const AudioFileFormat& format)
        {
            switch (getType(format))
            {
                case Type::None:
                    return "NONE";
                case Type::Twos:
                    return "twos";
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
        
        static const char* getString(const AudioFileFormat& format)
        {
            // FIX - doesn't deal with little endian... (return type)? "little endian"
            
            switch (getType(format))
            {
                case Type::None:
                case Type::Sowt:
                case Type::Twos:
                    return "not compressed";
                case Type::Float32:
                    return "32-bit floating point";
                case Type::Float64:
                    return "64-bit floating point";
                default:
                    return "FIXFIXFIX";
            }
        }
        
        static Type getType(const AudioFileFormat& format)
        {
            if (!format.isValid() || format.getFileType() == FileType::WAVE)
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
