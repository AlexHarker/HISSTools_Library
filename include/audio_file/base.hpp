#ifndef _HISSTOOLS_BASEAUDIOFILE_
#define _HISSTOOLS_BASEAUDIOFILE_

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "format.hpp"
#include "aifc_compression.hpp"

// FIX - frames type

namespace HISSTools
{
    class BaseAudioFile
    {
    protected:
        using ios_base = std::ios_base;
        
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
        : m_sampling_rate(0)
        , m_num_channels(0)
        , m_num_frames(0)
        , m_pcm_offset(0)
        , m_error_flags(static_cast<int>(Error::None))
        {}
        
        virtual ~BaseAudioFile() {}
        
        bool is_open() const                    { return m_file.is_open(); }
        
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
        
        FileType get_file_type() const          { return m_format.get_file_type(); }
        PCMFormat get_pcm_format() const        { return m_format.get_pcm_format(); }
        NumericType get_numeric_type() const    { return m_format.get_numeric_type(); }
        
        Endianness header_endianness() const    { return m_format.header_endianness(); }
        Endianness audio_endianness() const     { return m_format.audio_endianness(); }
        
        double sampling_rate() const            { return m_sampling_rate; }
        uint16_t channels() const               { return m_num_channels; }
        
        uintptr_t frames() const                { return m_num_frames; }
        uint16_t bit_depth() const              { return m_format.bit_depth(); }
        uint16_t byte_depth() const             { return m_format.byte_depth(); }
        uintptr_t frame_byte_count() const      { return channels() * byte_depth(); }
        
        bool is_error() const                   { return m_error_flags != static_cast<int>(Error::None); }
        int error_flags() const                 { return m_error_flags; }
        std::vector<Error> get_errors() const   { return extract_errors_from_flags(error_flags()); }
        void clear_errors()                     { m_error_flags = static_cast<int>(Error::None); }
        
        static std::vector<Error> extract_errors_from_flags(int flags)
        {
            std::vector<Error> errors;
            
            for (int i = 0; i <= static_cast<int>(Error::CouldNotWrite); i++)
            {
                if (flags & (1 << i))
                    errors.push_back(static_cast<Error>(i));
            }
            
            return errors;
        }
        
        static std::string error_string(Error error)
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
        
    protected:
        
        static constexpr uintptr_t work_loop_size()
        {
            return 1024;
        }
        
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
