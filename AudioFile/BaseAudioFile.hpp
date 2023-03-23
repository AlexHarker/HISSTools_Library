#ifndef _HISSTOOLS_BASEAUDIOFILE_
#define _HISSTOOLS_BASEAUDIOFILE_

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "AudioFileFormat.hpp"
#include "AIFCCompression.hpp"

// FIX - frames type

namespace HISSTools
{
    class BaseAudioFile
    {
    public:
        
        using FileType = AudioFileFormat::FileType;
        using PCMFormat = AudioFileFormat::PCMFormat;
        using NumericType = AudioFileFormat::NumericType;
        using Endianness = AudioFileFormat::Endianness;
   
        enum class Error
        {
            None                    = 0,
            FileError               = 1 << 0,
            CouldNotOpen            = 1 << 1,
            BadFormat               = 1 << 2,
            UnknownFormat           = 1 << 3,
            UnsupportedPCMFormat    = 1 << 4,
            WrongAIFCVersion        = 1 << 5,
            UnsupportedAIFCFormat   = 1 << 6,
            UnsupportedWaveFormat   = 1 << 7,
            CouldNotWrite           = 1 << 8,
        };
        
        enum AiffVersion
        {
            AIFC_CURRENT_SPECIFICATION = 0xA2805140
        };
        
        BaseAudioFile()
        : mSamplingRate(0)
        , mNumChannels(0)
        , mNumFrames(0)
        , mPCMOffset(0)
        , mErrorFlags(static_cast<int>(Error::None))
        {}
        
        virtual ~BaseAudioFile() {}
        
        bool isOpen() const                     { return mFile.is_open(); }

        void close()
        {
            mFile.close();
            
            mFormat = AudioFileFormat();
            
            mSamplingRate = 0;
            mNumChannels = 0;
            mNumFrames = 0;
            mPCMOffset = 0;
            
            mErrorFlags = static_cast<int>(Error::None);
        }
        
        FileType getFileType() const            { return mFormat.getFileType(); }
        PCMFormat getPCMFormat() const          { return mFormat.getPCMFormat(); }
        NumericType getNumericType() const      { return mFormat.getNumericType(); }
        
        Endianness getHeaderEndianness() const  { return mFormat.getHeaderEndianness(); }
        Endianness getAudioEndianness() const   { return mFormat.getAudioEndianness(); }
        
        double getSamplingRate() const          { return mSamplingRate; }
        uint16_t getChannels() const            { return mNumChannels; }
        
        uintptr_t getFrames() const             { return mNumFrames; }
        uint16_t getBitDepth() const            { return mFormat.getBitDepth(); }
        uint16_t getByteDepth() const           { return mFormat.getByteDepth(); }
        uintptr_t getFrameByteCount() const     { return getChannels() * getByteDepth(); }
        
        bool isError() const                    { return mErrorFlags != static_cast<int>(Error::None); }
        int getErrorFlags() const               { return mErrorFlags; }
        void clearErrorFlags()                  { mErrorFlags = static_cast<int>(Error::None); }
        
        static std::string getErrorString(Error error)
        {
            switch (error)
            {
                case Error::FileError:                  return "file error";
                case Error::CouldNotOpen:               return "couldn't open file";
                case Error::BadFormat:                  return "bad format";
                case Error::UnknownFormat:              return "unknown format";
                case Error::UnsupportedPCMFormat:       return "unsupported pcm format";
                case Error::WrongAIFCVersion:           return "wrong aifc version";
                case Error::UnsupportedAIFCFormat:      return "unsupported aifc format";
                case Error::UnsupportedWaveFormat:      return "unsupported wave format";
                case Error::CouldNotWrite:              return "couldn't write file";
                    
                default:                                return "no error";
            }
        }
        
        static std::vector<Error> extractErrorsFromFlags(int flags)
        {
            std::vector<Error> errors;
            
            for (int i = 0; i <= static_cast<int>(Error::CouldNotWrite); i++)
            {
                if (flags & (1 << i))
                    errors.push_back(static_cast<Error>(i));
            }
            
            return errors;
        }
        
        std::vector<Error> getErrors() const    { return extractErrorsFromFlags(getErrorFlags()); }
        
    protected:
        
        static constexpr uintptr_t WORK_LOOP_SIZE = 1024;
        
        uintptr_t getPCMOffset() const          { return mPCMOffset; }
        
        void setErrorFlags(int flags)           { mErrorFlags = flags; }
        void setErrorBit(Error error)           { mErrorFlags |= static_cast<int>(error); }
        
        template <typename T>
        static T paddedLength(T length)
        {
            return length + (length & 0x1);
        }
        
        AudioFileFormat mFormat;
        
        double mSamplingRate;
        uint16_t mNumChannels;
        uintptr_t mNumFrames;
        size_t mPCMOffset;
        
        // Data
        
        std::fstream mFile;
        std::vector<unsigned char> mBuffer;
        
    private:
        
        int mErrorFlags;
    };
}

#endif
