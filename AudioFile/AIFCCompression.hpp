
#ifndef AIFC_COMPRESSION_HPP
#define AIFC_COMPRESSION_HPP

#include "AudioFileFormat.hpp"

namespace HISSTools
{
    struct AIFCCompression
    {
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
            
            if (matchTag(tag, "sowt"))
                return AudioFileFormat(FileType::AIFC, PCMFormat::Int16, Endianness::Little);
            
            if (matchTag(tag, "fl32") || matchTag(tag, "FL32"))
                return AudioFileFormat(FileType::AIFC, PCMFormat::Float32, Endianness::Big);
            
            if (matchTag(tag, "fl64") || matchTag(tag, "FL64"))
                return AudioFileFormat(FileType::AIFC, PCMFormat::Float64, Endianness::Big);
                
            return AudioFileFormat();
        }
        
        static const char* getTag(const AudioFileFormat& format)
        {
            // FIX - doesn't deal with little endian... (return type)? "sowt"
            
            switch (format.getPCMFormat())
            {
                case PCMFormat::Int8:
                case PCMFormat::Int16:
                case PCMFormat::Int24:
                case PCMFormat::Int32:
                    return "NONE";
                case PCMFormat::Float32:
                    return "fl32";
                case PCMFormat::Float64:
                    return "fl64";
            }
        }
        
        static const char* getString(const AudioFileFormat& format)
        {
            // FIX - doesn't deal with little endian... (return type)? "little endian"
            
            switch (format.getPCMFormat())
            {
                case PCMFormat::Int8:
                case PCMFormat::Int16:
                case PCMFormat::Int24:
                case PCMFormat::Int32:
                    return "not compressed";
                case PCMFormat::Float32:
                    return "32-bit floating point";
                case PCMFormat::Float64:
                    return "64-bit floating point";
            }
        }
    };
}

#endif /* AIFC_COMPRESSION_HPP */
