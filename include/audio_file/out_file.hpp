
#ifndef HISSTOOLS_LIBRARY_AUDIO_FILE_OUT_FILE_HPP
#define HISSTOOLS_LIBRARY_AUDIO_FILE_OUT_FILE_HPP

#include <algorithm>
#include <cmath>
#include <string>

#include "../namespace.hpp"
#include "extended_double.hpp"
#include "base.hpp"
#include "utilities.hpp"

HISSTOOLS_LIBRARY_NAMESPACE_START()

class out_audio_file : public base_audio_file
{
public:
    
    // Constructor and Destructor
    
    out_audio_file() {}
    
    out_audio_file(const std::string& file, file_type type, pcm_format format, uint16_t channels, double sr)
    {
        open(file, type, format, channels, sr);
    }
    
    out_audio_file(const std::string& file, file_type type, pcm_format format, uint16_t channels, double sr, endian_type endianness)
    {
        open(file, type, format, channels, sr, endianness);
    }
    
    ~out_audio_file()
    {
        close();
    }
    
    // File Open
    
    void open(const std::string& file, file_type type, pcm_format format, uint16_t channels, double sr)
    {
        open(file, type, format, channels, sr, type == file_type::wave ? endian_type::little : endian_type::big);
    }
    
    void open(const std::string& file, file_type type, pcm_format format, uint16_t channels, double sr, endian_type endianness)
    {
        close();
        m_file.open(file.c_str(), ios_base::binary | ios_base::in | ios_base::out | ios_base::trunc);
        
        seek_internal(0);
        
        if (is_open() && position_internal() == 0)
        {
            type = type == file_type::aiff ? file_type::aifc : type;
            
            m_format = audio_file_format(type, format, endianness);
            m_sampling_rate = sr;
            m_num_channels  = channels;
            m_num_frames = 0;
            m_pcm_offset = 0;
            
            if (get_file_type() == file_type::wave)
                write_wave_header();
            else
                write_aifc_header();
            
            m_buffer.resize(work_loop_size() * frame_byte_count());
        }
        else
            set_error_bit(error_type::open_failed);
    }
    
    // File Position
    
    void seek(uintptr_t position = 0)
    {
        if (get_pcm_offset() != 0)
            seek_internal(get_pcm_offset() + frame_byte_count() * position);
    }
    
    uintptr_t position()
    {
        if (get_pcm_offset())
            return static_cast<uintptr_t>((position_internal() - get_pcm_offset()) / frame_byte_count());
        
        return 0;
    }
    
    // File Writing
    
    void write_raw(const char* input, uintptr_t num_frames)
    {
        write_pcm_data(input, num_frames);
    }
    
    void write_interleaved(const double* input, uintptr_t num_frames)
    {
        write_audio(input, num_frames);
    }
    
    void write_interleaved(const float* input, uintptr_t num_frames)
    {
        write_audio(input, num_frames);
    }
    
    void write_channel(const double* input, uintptr_t num_frames, uint16_t channel)
    {
        write_audio(input, num_frames, channel);
    }
    
    void write_channel(const float* input, uintptr_t num_frames, uint16_t channel)
    {
        write_audio(input, num_frames, channel);
    }
    
protected:
    
    uintptr_t get_header_size() const { return get_pcm_offset() - 8; }
    
private:
    
    // Internal File Handling
    
    bool write_internal(const char* buffer, uintptr_t bytes)
    {
        m_file.clear();
        m_file.write(buffer, bytes);
        return m_file.good();
    }
    
    bool seek_internal(uintptr_t position)
    {
        m_file.clear();
        m_file.seekp(position, ios_base::beg);
        return position_internal() == position;
    }
    
    bool seek_relative_internal(uintptr_t offset)
    {
        if (!offset)
            return true;
        m_file.clear();
        uintptr_t new_position = position_internal() + offset;
        m_file.seekp(offset, ios_base::cur);
        return position_internal() == new_position;
    }
    
    uintptr_t position_internal()
    {
        return m_file.tellp();
    }
    
    // Putters
    
    template <class T, int N>
    bool put_bytes(T value, endian_type endianness)
    {
        unsigned char bytes[N];
        
        set_bytes<N>(value, endianness, bytes);
        
        return write_internal(reinterpret_cast<const char*>(bytes), N);
    }
    
    bool put_u32(uint32_t value, endian_type endianness)
    {
        return put_bytes<uint32_t, 4>(value, endianness);
    }
    
    bool put_u16(uint16_t value, endian_type endianness)
    {
        return put_bytes<uint16_t, 2>(value, endianness);
    }
    
    bool put_tag(const char* tag)
    {
        return write_internal(tag, 4);
    }
    
    bool put_chunk(const char* tag, uint32_t size)
    {
        bool success = true;
        
        success &= write_internal(tag, 4);
        success &= put_u32(size, header_endianness());
        
        return success;
    }
    
    bool put_pad_byte()
    {
        char pad_byte = 0;
        return write_internal(&pad_byte, 1);
    }
    
    bool put_extended(double value)
    {
        unsigned char bytes[10];
        
        extended_double_convertor()(bytes, value);
        
        return write_internal(reinterpret_cast<const char*>(bytes), 10);
    }
    
    bool put_string(const char* string)
    {
        char length = strlen(string);
        bool success = true;
        
        success &= write_internal(&length, 1);
        success &= write_internal(string, length);
        
        // Pad byte if necessary (number of bytes written so far is odd)
        
        if ((length + 1) & 0x1)
            success &= put_pad_byte();
        
        return success;
    }
    
    // Header Manipulation
    
    void write_wave_header()
    {
        bool success = true;
        
        // File Header
        
        if (header_endianness() == endian_type::little)
            success &= put_chunk("RIFF", 36);
        else
            success &= put_chunk("RIFX", 36);
        
        success &= put_tag("WAVE");
        
        // Format Chunk
        
        success &= put_chunk("fmt ", 16);
        success &= put_u16(get_numeric_type() == numeric_type::integer ? 0x1 : 0x3, header_endianness());
        success &= put_u16(channels(), header_endianness());
        success &= put_u32(sampling_rate(), header_endianness());
        
        // Bytes per second and block alignment
        
        success &= put_u32(sampling_rate() * frame_byte_count(), header_endianness());
        success &= put_u16(static_cast<uint16_t>(frame_byte_count()), header_endianness());
        
        success &= put_u16(bit_depth(), header_endianness());
        
        // Data Chunk (empty)
        
        success &= put_chunk("data", 0);
        m_pcm_offset = position_internal();
        
        if (!success)
            set_error_bit(error_type::write_failed);
    }
    
    void write_aifc_header()
    {
        bool success = true;
        
        // Compression Type
        
        auto string_byte_size = [](const char* string)
        {
            uint32_t length = static_cast<uint32_t>(strlen(string));
            return ((length + 1) & 0x1) ? length + 2 : length + 1;
        };
        
        const char* compression_string = aifc_compression::to_string(m_format);
        const char* compression_tag = aifc_compression::to_tag(m_format);
        
        const uint32_t compression_string_size = string_byte_size(compression_string);
        
        // Set file type, data size offset frames and header size
        
        uint32_t header_size = 62 + compression_string_size;
        
        // File Header
        
        success &= put_chunk("FORM", header_size);
        success &= put_tag("AIFC");
        
        // FVER Chunk
        
        success &= put_chunk("FVER", 4);
        success &= put_u32(aifc_current_specification, header_endianness());
        
        // COMM Chunk
        
        success &= put_chunk("COMM", 22 + compression_string_size);
        success &= put_u16(channels(), header_endianness());
        success &= put_u32(frames(), header_endianness());
        success &= put_u16(bit_depth(), header_endianness());
        success &= put_extended(sampling_rate());
        success &= put_tag(compression_tag);
        success &= put_string(compression_string);
        
        // SSND Chunk (zero size)
        
        success &= put_chunk("SSND", 8);
        
        // Set offset and block size to zero
        
        success &= put_u32(0, header_endianness());
        success &= put_u32(0, header_endianness());
        
        // Set offset to PCM Data
        
        m_pcm_offset = position_internal();
        
        if (!success)
            set_error_bit(error_type::write_failed);
    }
    
    bool update_header()
    {
        const uintptr_t end_frame = position();
        
        bool success = true;
        
        if (end_frame > frames())
        {
            // Calculate new data length and end of data
            
            m_num_frames = end_frame;
            
            const uintptr_t data_size = (frame_byte_count() * frames());
            const uintptr_t data_end = position_internal();
            
            // Write padding byte if relevant
            
            if (data_size & 0x1)
                success &= put_pad_byte();
            
            // Update chunk sizes for (file, audio and also frames for AIFF/C)
            
            if (get_file_type() == file_type::wave)
            {
                success &= seek_internal(4);
                success &= put_u32(static_cast<uint32_t>(get_header_size() + padded_length(data_size)), 
                                                         header_endianness());
                success &= seek_internal(get_pcm_offset() - 4);
                success &= put_u32(static_cast<uint32_t>(data_size), header_endianness());
            }
            else
            {
                success &= seek_internal(4);
                success &= put_u32(static_cast<uint32_t>(get_header_size() + padded_length(data_size)), 
                                                         header_endianness());
                success &= seek_internal(34);
                success &= put_u32(static_cast<uint32_t>(frames()), header_endianness());
                success &= seek_internal(get_pcm_offset() - 12);
                success &= put_u32(static_cast<uint32_t>(data_size) + 8, header_endianness());
            }
            
            // Reset the position to end of the data
            
            success &= seek_internal(data_end);
        }
        
        return success;
    }
    
    // Resize
    
    bool resize(uintptr_t num_frames)
    {
        bool success = true;
        
        if (num_frames > frames())
        {
            // Calculate new data length and end of data
            
            uintptr_t data_size = (frame_byte_count() * frames());
            uintptr_t new_size = (frame_byte_count() * num_frames);
            uintptr_t data_end = position_internal();
            
            seek(frames());
            
            for (uintptr_t i = 0; i < new_size - data_size; i++)
                success &= put_pad_byte();
            
            // Return to end of data
            
            success &= seek_internal(data_end);
        }
        
        return success;
    }
    
    // Write PCM Data
    
    bool write_pcm_data(const char* input, uintptr_t num_frames)
    {
        // Write audio and update the header
        
        bool success = write_internal(input, frame_byte_count() * num_frames);
        success &= update_header();
        
        return success;
    }
    
    template <int N, class T, class U, class V>
    static T convert(V value, T, U)
    {
        U typed_value = static_cast<U>(value);
        return* (reinterpret_cast<T*>(&typed_value));
    }
    
    template <int N, class T>
    static uint8_t convert(T value, uint8_t, uint8_t)
    {
        // Scale then clip before casting
        
        value = std::round((value * T(128)) + T(128));
        value = std::min(std::max(value, T(0)), T(255));
        
        return static_cast<uint8_t>(value);
    }
    
    template <int N, class T>
    static uint32_t convert(T value, uint32_t, uint32_t)
    {
        // FIX - issues of value (note 1 might be clipped by one value - what to do?)
        
        const uint32_t max_val = 1 << ((N * 8) - 1);
        
        value = std::round(value * static_cast<T>(max_val));
        
        return static_cast<uint32_t>(std::min(std::max(value, static_cast<T>(-max_val)), static_cast<T>(max_val - 1)));
    }
    
    template <class T, class U, int N, class V>
    void write_loop(const V* input, uintptr_t j, uintptr_t loop_samples, uintptr_t byte_step)
    {
        const auto endianness = audio_endianness();
        
        for (uintptr_t i = 0; i < loop_samples; i++, j += byte_step)
            set_bytes<N>(convert<N>(input[i], T(0), U(0)), endianness, m_buffer.data() + j);
    }
    
    template <class T>
    void write_audio(const T* input, uintptr_t num_frames, int32_t channel = -1)
    {
        bool success = true;
        
        const uint16_t num_channels = (channel < 0) ? channels() : 1;
        const uintptr_t byte_step = byte_depth() * ((channel < 0) ? 1 : channels());
        const uintptr_t end_frame = position() + num_frames;
        const uintptr_t j = std::max(channel, 0) * byte_depth();
        
        // Check if writing to all channels
        
        const bool all_channels = !(channel >= 0 && num_channels > 1);
        
        // Write zeros to channels if necessary (multichannel files are written one channel at a time)
        
        if (!all_channels)
            success &= resize(end_frame);
        
        m_file.seekg(m_file.tellp());
        
        channel = std::max(channel, 0);
        
        while (num_frames)
        {
            uintptr_t loop_frames = std::min(num_frames, work_loop_size());
            uintptr_t loop_samples = loop_frames * num_channels;
            uintptr_t pos = m_file.tellg();
            
            // Read chunk from file to write back to
            
            if (!all_channels)
            {
                m_file.clear();
                m_file.read(reinterpret_cast<char*>(m_buffer.data()), loop_frames * frame_byte_count());
                
                if (m_file.gcount() != loop_frames * frame_byte_count())
                {
                    set_error_bit(error_type::write_failed);
                    return;
                }
            }
            
            // Write audio data to a local buffer
            
            switch (get_pcm_format())
            {
                case pcm_format::int8:
                    if (get_file_type() == file_type::wave)
                        write_loop<uint8_t, uint8_t, 1>(input, j, loop_samples, byte_step);
                    else
                        write_loop<uint32_t, uint32_t, 1>(input, j, loop_samples, byte_step);
                    break;
                    
                case pcm_format::int16:
                    write_loop<uint32_t, uint32_t, 2>(input, j, loop_samples, byte_step);
                    break;
                    
                case pcm_format::int24:
                    write_loop<uint32_t, uint32_t, 3>(input, j, loop_samples, byte_step);
                    break;
                    
                case pcm_format::int32:
                    write_loop<uint32_t, uint32_t, 4>(input, j, loop_samples, byte_step);
                    break;
                    
                case pcm_format::float32:
                    write_loop<uint32_t, float, 4>(input, j, loop_samples, byte_step);
                    break;
                    
                case pcm_format::float64:
                    write_loop<uint64_t, double, 8>(input, j, loop_samples, byte_step);
                    break;
            }
            
            // Write buffer back to file
            
            if (!all_channels)
                m_file.seekp(pos);
            
            write_internal(reinterpret_cast<const char*>(m_buffer.data()), loop_frames * frame_byte_count());
            
            num_frames -= loop_frames;
            input += loop_samples;
        }
        
        // Update number of frames
        
        success &= update_header();
        
        // return success;
    }
};

HISSTOOLS_LIBRARY_NAMESPACE_END()

#endif /* HISSTOOLS_LIBRARY_AUDIO_FILE_OUT_FILE_HPP */
