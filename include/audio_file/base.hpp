#ifndef HISSTOOLS_AUDIO_FILE_BASE_HPP
#define HISSTOOLS_AUDIO_FILE_BASE_HPP

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "../namespace.hpp"
#include "format.hpp"
#include "aifc_compression.hpp"

// FIX - frames type

HISSTOOLS_NAMESPACE_START()

class base_audio_file
{
protected:
    using ios_base = std::ios_base;
    
public:
    
    using file_type = audio_file_format::file_type;
    using pcm_format = audio_file_format::pcm_format;
    using numeric_type = audio_file_format::numeric_type;
    using endianness = audio_file_format::endianness;
    
    enum class error_type
    {
        NONE                    = 0,
        FILE_ERROR              = 1 << 0,
        OPEN_FAILED             = 1 << 1,
        FMT_BAD                 = 1 << 2,
        FMT_UNKNOWN             = 1 << 3,
        PCM_FMT_UNSUPPORTED     = 1 << 4,
        AIFC_WRONG_VERSION      = 1 << 5,
        AIFC_FMT_UNSUPPORTED    = 1 << 6,
        WAV_FMT_UNSUPPORTED     = 1 << 7,
        WRITE_FAILED            = 1 << 8,
    };
    
    enum AIFC_VERSION
    {
        AIFC_CURRENT_SPECIFICATION = 0xA2805140
    };
    
    base_audio_file()
    : m_sampling_rate(0)
    , m_num_channels(0)
    , m_num_frames(0)
    , m_pcm_offset(0)
    , m_error_flags(static_cast<int>(error_type::NONE))
    {}
    
    virtual ~base_audio_file() {}
    
    bool is_open() const                    { return m_file.is_open(); }
    
    void close()
    {
        m_file.close();
        
        m_format = audio_file_format();
        
        m_sampling_rate = 0;
        m_num_channels = 0;
        m_num_frames = 0;
        m_pcm_offset = 0;
        
        m_error_flags = static_cast<int>(error_type::NONE);
    }
    
    file_type get_file_type() const         { return m_format.get_file_type(); }
    pcm_format get_pcm_format() const       { return m_format.get_pcm_format(); }
    numeric_type get_numeric_type() const   { return m_format.get_numeric_type(); }
    
    endianness header_endianness() const    { return m_format.header_endianness(); }
    endianness audio_endianness() const     { return m_format.audio_endianness(); }
    
    double sampling_rate() const            { return m_sampling_rate; }
    uint16_t channels() const               { return m_num_channels; }
    
    uintptr_t frames() const                { return m_num_frames; }
    uint16_t bit_depth() const              { return m_format.bit_depth(); }
    uint16_t byte_depth() const             { return m_format.byte_depth(); }
    uintptr_t frame_byte_count() const      { return channels() * byte_depth(); }
    
    bool is_error() const                   { return m_error_flags != static_cast<int>(error_type::NONE); }
    int error_flags() const                 { return m_error_flags; }
    std::vector<error_type> get_errors() const   { return extract_errors_from_flags(error_flags()); }
    void clear_errors()                     { m_error_flags = static_cast<int>(error_type::NONE); }
    
    static std::vector<error_type> extract_errors_from_flags(int flags)
    {
        std::vector<error_type> errors;
        
        for (int i = 0; i <= static_cast<int>(error_type::WRITE_FAILED); i++)
        {
            if (flags & (1 << i))
                errors.push_back(static_cast<error_type>(i));
        }
        
        return errors;
    }
    
    static std::string error_string(error_type error)
    {
        switch (error)
        {
            case error_type::FILE_ERROR:            return "file error";
            case error_type::OPEN_FAILED:           return "couldn't open file";
            case error_type::FMT_BAD:               return "bad format";
            case error_type::FMT_UNKNOWN:           return "unknown format";
            case error_type::PCM_FMT_UNSUPPORTED:   return "unsupported pcm format";
            case error_type::AIFC_WRONG_VERSION:    return "wrong aifc version";
            case error_type::AIFC_FMT_UNSUPPORTED:  return "unsupported aifc format";
            case error_type::WAV_FMT_UNSUPPORTED:   return "unsupported wave format";
            case error_type::WRITE_FAILED:          return "couldn't write file";
                
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
    void set_error_bit(error_type error)         { m_error_flags |= static_cast<int>(error); }
    
    template <typename T>
    static T padded_length(T length)
    {
        return length + (length & 0x1);
    }
    
    audio_file_format m_format;
    
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

HISSTOOLS_NAMESPACE_END()

#endif /* HISSTOOLS_AUDIO_FILE_BASE_HPP */
