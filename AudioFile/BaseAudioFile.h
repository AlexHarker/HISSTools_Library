#ifndef _HISSTOOLS_BASEAUDIOFILE_
#define _HISSTOOLS_BASEAUDIOFILE_

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace HISSTools
{
    class BaseAudioFile
    {
        
    public:

        typedef uint32_t FrameCount;
        typedef uintptr_t ByteCount;
        
        enum FileType
        {
            kAudioFileNone,
            kAudioFileAIFF,
            kAudioFileAIFC,
            kAudioFileWAVE
        };
        
        enum PCMFormat
        {
            kAudioFileInt8,
            kAudioFileInt16,
            kAudioFileInt24,
            kAudioFileInt32,
            kAudioFileFloat32,
            kAudioFileFloat64
        };
        
        enum Endianness
        {
            kAudioFileLittleEndian,
            kAudioFileBigEndian
        };
        
        enum NumberFormat
        {
            kAudioFileInt,
            kAudioFileFloat
        };

        enum Error
        {
            ERR_NONE = 0,

            ERR_MEM_COULD_NOT_ALLOCATE = 1 << 0,

            ERR_FILE_ERROR = 1 << 1,
            ERR_FILE_COULDNT_OPEN = 1 << 2,
            ERR_FILE_BAD_FORMAT = 1 << 3,
            ERR_FILE_UNKNOWN_FORMAT = 1 << 4,
            ERR_FILE_UNSUPPORTED_PCM_FORMAT = 1 << 5,

            ERR_AIFC_WRONG_VERSION = 1 << 6,
            ERR_AIFC_UNSUPPORTED_FORMAT = 1 << 7,

            ERR_WAVE_UNSUPPORTED_FORMAT = 1 << 8,

            ERR_FILE_COULDNT_WRITE = 1 << 9,
        };

        enum AiffVersion
        {
            AIFC_CURRENT_SPECIFICATION = 0xA2805140
        };
        
        BaseAudioFile()                             { close(); }
        virtual ~BaseAudioFile()                    {}
    
        FileType getFileType() const                { return mFileType; }
        PCMFormat getPCMFormat() const              { return mPCMFormat; }
        Endianness getHeaderEndianness() const      { return mHeaderEndianness; }
        Endianness getAudioEndianness() const       { return mAudioEndianness; }
        double getSamplingRate() const              { return mSamplingRate; }
        uint16_t getChannels() const                { return mNumChannels; }
        uint32_t getFrames() const                  { return mNumFrames; }
        uint16_t getBitDepth() const                { return findBitDepth(getPCMFormat()); }
        uint16_t getByteDepth() const               { return getBitDepth() / 8U; }
        uintptr_t getFrameByteCount() const         { return getChannels() * getByteDepth(); }
        NumberFormat getNumberFormat() const        { return findNumberFormat(getPCMFormat()); }
        
        int getErrorFlags() const                   { return mErrorFlags; }
        bool getIsError() const                     { return mErrorFlags != ERR_NONE; }
        void clearErrorFlags()                      { setErrorFlags(ERR_NONE); }

        static std::string getErrorString(Error error)
        {
            switch (error)
            {
                case ERR_MEM_COULD_NOT_ALLOCATE:        return "mem could not allocate";
                case ERR_FILE_ERROR:                    return "file error";
                case ERR_FILE_COULDNT_OPEN:             return "file couldn't open";
                case ERR_FILE_BAD_FORMAT:               return "file bad format";
                case ERR_FILE_UNKNOWN_FORMAT:           return "file unknown format";
                case ERR_FILE_UNSUPPORTED_PCM_FORMAT:   return "file unsupported pcm format";
                case ERR_AIFC_WRONG_VERSION:            return "aifc wrong version";
                case ERR_AIFC_UNSUPPORTED_FORMAT:       return "aifc unsupported format";
                case ERR_WAVE_UNSUPPORTED_FORMAT:       return "wave unsupported format";
                case ERR_FILE_COULDNT_WRITE:            return "file couldn't write";
                default:                                return "no error";
            }
        }
        
        static std::vector<Error> extractErrorsFromFlags(int flags)
        {
            std::vector<Error> result;
            
            for (int i = 0; i != sizeof(int) * 4; i++)
            {
                if (flags & (1 << i))
                    result.push_back(static_cast<Error>(i));
            }
            
            return result;
        }
        
        std::vector<Error> getErrors() const
        {
            return extractErrorsFromFlags(getErrorFlags());
        }
        
        static uint16_t findBitDepth(PCMFormat format)
        {
            switch (format)
            {
                case kAudioFileInt8:        return  8U;
                case kAudioFileInt16:       return 16U;
                case kAudioFileInt24:       return 24U;
                case kAudioFileInt32:       return 32U;
                case kAudioFileFloat32:     return 32U;
                case kAudioFileFloat64:     return 64U;
                default:                    return 16U;
            }
        }
        
        static NumberFormat findNumberFormat(PCMFormat format)
        {
            switch (format)
            {
                case kAudioFileFloat32:
                case kAudioFileFloat64:
                    return kAudioFileFloat;
                    
                case kAudioFileInt8:
                case kAudioFileInt16:
                case kAudioFileInt24:
                case kAudioFileInt32:
                default:
                    return kAudioFileInt;
            }
        }

        virtual void close()
        {
            setFileType(kAudioFileNone);
            setPCMFormat(kAudioFileInt8);
            setHeaderEndianness(kAudioFileLittleEndian);
            setAudioEndianness(kAudioFileLittleEndian);
            setSamplingRate(0);
            setChannels(0);
            setFrames(0);
            setPCMOffset(0);
            clearErrorFlags();
        }
        
        virtual bool isOpen() = 0;
        virtual void seek(uint32_t position) = 0;
        virtual uint32_t getPosition() = 0;

    protected:
        
        uintptr_t getPCMOffset() const                      { return mPCMOffset; }
        
        void setFileType(FileType type)                     { mFileType = type; }
        void setPCMFormat(PCMFormat format)                 { mPCMFormat = format; }
        void setHeaderEndianness(Endianness endianness)     { mHeaderEndianness = endianness; }
        void setAudioEndianness(Endianness endianness)      { mAudioEndianness = endianness; }
        void setSamplingRate(double samplingRate)           { mSamplingRate = samplingRate; }
        void setChannels(uint16_t chans)                    { mNumChannels = chans; }
        void setFrames(uint32_t frames)                     { mNumFrames = frames; }
        void setPCMOffset(uintptr_t offset)                 { mPCMOffset = offset; }

        void setErrorFlags(int flags)                       { mErrorFlags = flags; }
        void setErrorBit(Error error)                       { mErrorFlags |= error; }

    private:
        
        FileType mFileType;
        PCMFormat mPCMFormat;
        Endianness mHeaderEndianness;
        Endianness mAudioEndianness;

        double mSamplingRate;
        uint16_t mNumChannels;
        uint32_t mNumFrames;
        size_t mPCMOffset;

        int mErrorFlags;
    };
}

#endif 
