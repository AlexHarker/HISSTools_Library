
#ifndef AUDIO_FILE_FORMAT_HPP
#define AUDIO_FILE_FORMAT_HPP

#include <vector>

namespace HISSTools
{
    class AudioFileFormat
    {
    public:
        
        enum class FileType     { None, AIFF, AIFC, WAVE };
        enum class PCMFormat    { Int8, Int16, Int24, Int32, Float32, Float64 };
        enum class NumericType  { Integer, Float };
        enum class Endianness   { Little, Big };
        
        AudioFileFormat()
        : mFileType(FileType::None)
        , mPCMFormat(PCMFormat::Int16)
        , mEndianness(Endianness::Little)
        , mValid(false)
        {}
        
        AudioFileFormat(FileType type)
        : mFileType(type)
        , mPCMFormat(PCMFormat::Int16)
        , mEndianness(type == FileType::WAVE ? Endianness::Little : Endianness::Big)
        , mValid(false)
        {}
        
        AudioFileFormat(FileType type, PCMFormat format, Endianness endianness)
        : mFileType(type)
        , mPCMFormat(format)
        , mEndianness(endianness)
        , mValid(getValidity())
        {}
        
        AudioFileFormat(FileType type, NumericType numericType, uint16_t bitDepth, Endianness endianness)
        : mFileType(type)
        , mEndianness(endianness)
        , mValid(false)
        {            
            struct ValidFormat
            {
                NumericType mType;
                uint16_t mDepth;
                PCMFormat mFormat;
            };
            
            std::vector<ValidFormat> validFormats =
            {
                { NumericType::Integer,  8, PCMFormat::Int8 },
                { NumericType::Integer,  8, PCMFormat::Int8 },
                { NumericType::Integer, 16, PCMFormat::Int16 },
                { NumericType::Integer, 24, PCMFormat::Int24 },
                { NumericType::Integer, 32, PCMFormat::Int32 },
                { NumericType::Float,   32, PCMFormat::Float32 },
                { NumericType::Float,   64, PCMFormat::Float64 },
            };
            
            for (int i = 0; i < validFormats.size(); i++)
            {
                if (numericType == validFormats[i].mType && bitDepth == validFormats[i].mDepth)
                {
                    mPCMFormat = validFormats[i].mFormat;
                    mValid = getValidity();
                    break;
                }
            }
        }
        
        FileType getFileType() const            { return mFileType; }
        PCMFormat getPCMFormat() const          { return mPCMFormat; }
        NumericType getNumericType() const      { return findNumericType(getPCMFormat()); }
        
        uint16_t getBitDepth() const            { return findBitDepth(getPCMFormat()); }
        uint16_t getByteDepth() const           { return getBitDepth() / 8; }
        
        Endianness getHeaderEndianness() const
        {
            return FileType() == FileType::WAVE ? mEndianness : Endianness::Big;
        }
        
        Endianness getAudioEndianness() const
        {
            return mEndianness;
        }
        
        bool isValid() const { return mValid; }
        
        static uint16_t findBitDepth(PCMFormat format)
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
        
        static NumericType findNumericType(PCMFormat format)
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
        
        bool getValidity()
        {
            if (mFileType == FileType::None)
                return false;
            
            if (mFileType == FileType::AIFF && findNumericType(mPCMFormat) == NumericType::Float )
                return false;
            
            if (mFileType == FileType::AIFF && mEndianness != Endianness::Big )
                return false;
            
            if (mFileType == FileType::AIFF && mEndianness != Endianness::Big )
                return false;
            
            return true;
        }
        
        FileType mFileType;
        PCMFormat mPCMFormat;
        Endianness mEndianness;
        bool mValid;
    };
}

#endif /* AUDIO_FILE_FORMAT_HPP */
