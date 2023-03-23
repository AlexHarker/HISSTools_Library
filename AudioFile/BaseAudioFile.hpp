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
   
        using ios_base = std::ios_base;
        
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
        : m_sampling_rate(0)
        , m_num_channels(0)
        , m_num_frames(0)
        , m_pcm_offset(0)
        , m_error_flags(static_cast<int>(Error::None))
        {}
        
        virtual ~BaseAudioFile() {}
        
        bool isOpen() const                     { return m_file.is_open(); }

        void close()
        {
            m_file.close();
            
            m_format = AudioFileFormat();
            
            m_sampling_rate = 0;
            m_num_channels = 0;
            m_num_frames = 0;
            m_pcm_offset = 0;
            
            m_error_flags = static_cast<int>(Error::None);
        }
        
        FileType getFileType() const            { return m_format.getFileType(); }
        PCMFormat getPCMFormat() const          { return m_format.getPCMFormat(); }
        NumericType getNumericType() const      { return m_format.getNumericType(); }
        
        Endianness getHeaderEndianness() const  { return m_format.getHeaderEndianness(); }
        Endianness getAudioEndianness() const   { return m_format.getAudioEndianness(); }
        
        double getSamplingRate() const          { return m_sampling_rate; }
        uint16_t getChannels() const            { return m_num_channels; }
        
        uintptr_t getFrames() const             { return m_num_frames; }
        uint16_t getBitDepth() const            { return m_format.getBitDepth(); }
        uint16_t getByteDepth() const           { return m_format.getByteDepth(); }
        uintptr_t getFrameByteCount() const     { return getChannels() * getByteDepth(); }
        
        bool isError() const                    { return m_error_flags != static_cast<int>(Error::None); }
        int getErrorFlags() const               { return m_error_flags; }
        void clearErrorFlags()                  { m_error_flags = static_cast<int>(Error::None); }
        
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
        
        uintptr_t get_pcm_offset() const        { return m_pcm_offset; }
        
        void set_error_flags(int flags)         { m_error_flags = flags; }
        void set_error_bit(Error error)         { m_error_flags |= static_cast<int>(error); }
        
        template <typename T>
        static T padded_length(T length)
        {
            return length + (length & 0x1);
        }
        
        AudioFileFormat m_format;
        
        double m_sampling_rate;
        uint16_t m_num_channels;
        uintptr_t m_num_frames;
        size_t m_pcm_offset;
        
        // Data
        
        std::fstream m_file;
        std::vector<unsigned char> m_buffer;
        
    private:
        
        int m_error_flags;
    };
}

#endif
