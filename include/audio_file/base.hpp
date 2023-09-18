
#ifndef HISSTOOLS_LIBRARY_AUDIO_FILE_BASE_HPP
#define HISSTOOLS_LIBRARY_AUDIO_FILE_BASE_HPP

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "../namespace.hpp"
#include "format.hpp"
#include "aifc_compression.hpp"

// FIX - frames type

HISSTOOLS_LIBRARY_NAMESPACE_START()

class base_audio_file
{
protected:
    
    using ios_base = std::ios_base;
    
public:
    
    using file_type = audio_file_format::file_type;
    using pcm_format = audio_file_format::pcm_format;
    using numeric_type = audio_file_format::numeric_type;
    using endian_type = audio_file_format::endian_type;
    
    enum class error_type
    {
        none                    = 0,
        file_error              = 1 << 0,
        open_failed             = 1 << 1,
        fmt_bad                 = 1 << 2,
        fmt_unknown             = 1 << 3,
        pcm_fmt_unsupported     = 1 << 4,
        aifc_wrong_version      = 1 << 5,
        aifc_fmt_unsupported    = 1 << 6,
        wav_fmt_unsupported     = 1 << 7,
        write_failed            = 1 << 8,
    };
    
    enum AIFC_VERSION
    {
        aifc_current_specification = 0xA2805140
    };
    
    base_audio_file()
    : m_sampling_rate(0)
    , m_num_channels(0)
    , m_num_frames(0)
    , m_pcm_offset(0)
    , m_error_flags(static_cast<int>(error_type::none))
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
        
        m_error_flags = static_cast<int>(error_type::none);
    }
    
    file_type get_file_type() const         { return m_format.get_file_type(); }
    pcm_format get_pcm_format() const       { return m_format.get_pcm_format(); }
    numeric_type get_numeric_type() const   { return m_format.get_numeric_type(); }
    
    endian_type header_endianness() const   { return m_format.header_endianness(); }
    endian_type audio_endianness() const    { return m_format.audio_endianness(); }
    
    double sampling_rate() const            { return m_sampling_rate; }
    uint16_t channels() const               { return m_num_channels; }
    
    uintptr_t frames() const                { return m_num_frames; }
    uint16_t bit_depth() const              { return m_format.bit_depth(); }
    uint16_t byte_depth() const             { return m_format.byte_depth(); }
    uintptr_t frame_byte_count() const      { return channels() * byte_depth(); }
    
    bool is_error() const                   { return m_error_flags != static_cast<int>(error_type::none); }
    int error_flags() const                 { return m_error_flags; }
    std::vector<error_type> get_errors() const   { return extract_errors_from_flags(error_flags()); }
    void clear_errors()                     { m_error_flags = static_cast<int>(error_type::none); }
    
    static std::vector<error_type> extract_errors_from_flags(int flags)
    {
        std::vector<error_type> errors;
        
        for (int i = 0; i <= static_cast<int>(error_type::write_failed); i++)
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
            case error_type::file_error:            return "file error";
            case error_type::open_failed:           return "couldn't open file";
            case error_type::fmt_bad:               return "bad format";
            case error_type::fmt_unknown:           return "unknown format";
            case error_type::pcm_fmt_unsupported:   return "unsupported pcm format";
            case error_type::aifc_wrong_version:    return "wrong aifc version";
            case error_type::aifc_fmt_unsupported:  return "unsupported aifc format";
            case error_type::wav_fmt_unsupported:   return "unsupported wave format";
            case error_type::write_failed:          return "couldn't write file";
                
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
    
    constexpr endian_type little_endian()   { return endian_type::little; }
    constexpr endian_type big_endian()      { return endian_type::big; }

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

HISSTOOLS_LIBRARY_NAMESPACE_END()

#endif /* HISSTOOLS_LIBRARY_AUDIO_FILE_BASE_HPP */
