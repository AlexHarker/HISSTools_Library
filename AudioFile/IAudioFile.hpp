#ifndef _HISSTOOLS_IAUDIOFILE_
#define _HISSTOOLS_IAUDIOFILE_

#include "AudioFileUtilities.hpp"
#include "AudioFileExtendedDouble.hpp"
#include "BaseAudioFile.hpp"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

// FIX - check types, errors and returns

namespace HISSTools
{
    class IAudioFile : public BaseAudioFile
    {
        enum class AIFFTag
        {
            Unknown = 0x0,
            Version = 0x1,
            Common = 0x2,
            Audio = 0x4
        };

        struct postion_restore
        {
            postion_restore(IAudioFile &parent)
            : m_parent(parent)
            , m_position(parent.position_internal())
            {
                m_parent.m_file.seekg(12, ios_base::beg);
            }
            
            ~postion_restore()
            {
                m_parent.seek_internal(m_position);
            }
                
            IAudioFile &m_parent;
            uintptr_t m_position;
        };
        
    public:
        
        // Constructor and Destructor
        
        IAudioFile() {}
        
        IAudioFile(const std::string& file)
        {
            open(file);
        }
        
        ~IAudioFile()
        {
            close();
        }
        
        // File Open
        
        void open(const std::string& file)
        {
            close();
            
            if (!file.empty())
            {
                m_file.open(file.c_str(), ios_base::binary);
                if (m_file.is_open())
                {
                    set_error_bit(parse_header());
                    m_buffer.resize(WORK_LOOP_SIZE * getFrameByteCount());
                    seek();
                }
                else
                    set_error_bit(Error::CouldNotOpen);
            }
        }
        
        // File Position
        
        void seek(uintptr_t position = 0)
        {
            seek_internal(get_pcm_offset() + getFrameByteCount() * position);
        }
        
        uintptr_t getPosition()
        {
            if (get_pcm_offset())
                return static_cast<uintptr_t>((position_internal() - get_pcm_offset()) / getFrameByteCount());
            
            return 0;
        }

        // File Reading
        
        void readRaw(void* output, uintptr_t num_frames)
        {
            read_internal(static_cast<char *>(output), getFrameByteCount() * num_frames);
        }

        void readInterleaved(double* output, uintptr_t num_frames)
        {
            read_audio(output, num_frames);
        }
        
        void readInterleaved(float* output, uintptr_t num_frames)
        {
            read_audio(output, num_frames);
        }

        void readChannel(double* output, uintptr_t num_frames, uint16_t channel)
        {
            read_audio(output, num_frames, channel);
        }
        
        void readChannel(float* output, uintptr_t num_frames, uint16_t channel)
        {
            read_audio(output, num_frames, channel);
        }

        // Chunks
        
        std::vector<std::string> getChunkTags()
        {
            postion_restore restore(*this);
            
            char tag[5] = {0, 0, 0, 0, 0};
            uint32_t chunk_size;
            
            std::vector<std::string> tags;
            
            // Iterate through chunks
            
            while (read_chunk_header(tag, chunk_size))
            {
                tags.push_back(tag);
                read_chunk(nullptr, 0, chunk_size);
            }
            
            return tags;
        }
        
        uintptr_t getChunkSize(const char *tag)
        {
            postion_restore restore(*this);
            
            uint32_t chunk_size = 0;
            
            if (strlen(tag) <= 4)
                find_chunk(tag, chunk_size);
            
            return chunk_size;
        }
        
        void readRawChunk(void *output, const char *tag)
        {
            postion_restore restore(*this);
            
            uint32_t chunk_size = 0;
            
            if (strlen(tag) <= 4)
            {
                find_chunk(tag, chunk_size);
                read_internal(static_cast<char *>(output), chunk_size);
            }
        }
        
    private:
        
        // Internal File Handling
        
        bool read_internal(char* buffer, uintptr_t numBytes)
        {
            m_file.clear();
            m_file.read(buffer, numBytes);
            return static_cast<uintptr_t>(m_file.gcount()) == numBytes;
        }
        
        bool seek_internal(uintptr_t position)
        {
            m_file.clear();
            m_file.seekg(position, ios_base::beg);
            return position_internal() == position;
        }
        
        bool advance_internal(uintptr_t offset)
        {
            return seek_internal(position_internal() + offset);
        }
        
        uintptr_t position_internal()
        {
            return m_file.tellg();
        }
        
        // Getters
        
        uint32_t getU32(const char* bytes, Endianness endianness)
        {
            return get_bytes<uint32_t, 4>(bytes, endianness);
        }
        
        uint16_t getU16(const char* bytes, Endianness endianness)
        {
            return get_bytes<uint16_t, 2>(bytes, endianness);
        }
        
        // Chunk Reading
        
        static bool match_tag(const char* a, const char* b)
        {
            return (strncmp(a, b, 4) == 0);
        }
        
        bool read_chunk_header(char* tag, uint32_t& chunk_size)
        {
            char header[8] = {};
            
            if (!read_internal(header, 8))
                return false;
            
            strncpy(tag, header, 4);
            chunk_size = getU32(header + 4, getHeaderEndianness());
            
            return true;
        }
        
        bool find_chunk(const char* search_tag, uint32_t& chunk_size)
        {
            char tag[4] = {};
            
            while (read_chunk_header(tag, chunk_size))
            {
                if (match_tag(tag, search_tag))
                    return true;
                
                if (!advance_internal(padded_length(chunk_size)))
                    break;
            }
            
            return false;
        }
        
        bool read_chunk(char* data, uint32_t read_size, uint32_t chunk_size)
        {
            if (read_size && (read_size > chunk_size || !read_internal(data, read_size)))
                return false;
            
            if (!advance_internal(padded_length(chunk_size) - read_size))
                return false;
            
            return true;
        }

        // AIFF Helper
        
        bool get_aiff_chunk_header(AIFFTag& enumerated_tag, uint32_t& chunk_size)
        {
            char tag[4];
            
            enumerated_tag = AIFFTag::Unknown;
            
            if (!read_chunk_header(tag, chunk_size))
                return false;
            
            if      (match_tag(tag, "FVER")) enumerated_tag = AIFFTag::Version;
            else if (match_tag(tag, "COMM")) enumerated_tag = AIFFTag::Common;
            else if (match_tag(tag, "SSND")) enumerated_tag = AIFFTag::Audio;
            
            return true;
        }
        
        // Parse Headers
        
        Error parse_header()
        {
            char chunk[12] = {};
            
            const char *file_type = chunk;
            const char *file_subtype = chunk + 8;
            
            // `Read file header
            
            if (!read_internal(chunk, 12))
                return Error::BadFormat;
            
            // AIFF or AIFC
            
            if (match_tag(file_type, "FORM") && (match_tag(file_subtype, "AIFF") || match_tag(file_subtype, "AIFC")))
                return parse_aiff_header(file_subtype);
            
            // WAVE file format
            
            if ((match_tag(file_type, "RIFF") || match_tag(file_type, "RIFX")) && match_tag(file_subtype, "WAVE"))
                return parse_wave_header(file_type);
            
            // No known format found
            
            return Error::UnknownFormat;
        }
        
        Error parse_aiff_header(const char* fileSubtype)
        {
            AIFFTag tag;
            
            char chunk[22];
            
            uint32_t format_valid = static_cast<uint32_t>(AIFFTag::Common) | static_cast<uint32_t>(AIFFTag::Audio);
            uint32_t format_check = 0;
            uint32_t chunk_size;
            
            m_format = AudioFileFormat(FileType::AIFF);
            
            // Iterate over chunks
            
            while (get_aiff_chunk_header(tag, chunk_size))
            {
                format_check |= static_cast<uint32_t>(tag);
                
                switch (tag)
                {
                    case AIFFTag::Common:
                    {
                        // Read common chunk (at least 18 bytes and up to 22)
                        
                        if (!read_chunk(chunk, (chunk_size > 22) ? 22 : ((chunk_size < 18) ? 18 : chunk_size), chunk_size))
                            return Error::BadFormat;
                        
                        // Retrieve relevant data (AIFF or AIFC) and set AIFF defaults
                        
                        m_num_channels = getU16(chunk + 0, getHeaderEndianness());
                        m_num_frames = getU32(chunk + 2, getHeaderEndianness());
                        m_sampling_rate = extended_double_convertor()(chunk + 8);
                        
                        uint16_t bit_depth = getU16(chunk + 6, getHeaderEndianness());
                        
                        // If there are no frames then it is not required for there to be an audio (SSND) chunk
                        
                        if (!getFrames())
                            format_check |= static_cast<uint32_t>(AIFFTag::Audio);
                        
                        if (match_tag(fileSubtype, "AIFC"))
                        {
                            // Require a version chunk
                            
                            //formatValid |= static_cast<uint32_t>(AIFFTag::Version);
                            
                            // Set parameters based on format
                            
                            m_format = AIFCCompression::to_format(chunk + 18, bit_depth);
                            
                            if (getFileType() == FileType::None)
                                return Error::UnsupportedAIFCFormat;
                        }
                        else
                            m_format = AudioFileFormat(FileType::AIFF, NumericType::Integer, bit_depth, Endianness::Big);
                        
                        if (!m_format.is_valid())
                            return Error::UnsupportedPCMFormat;
                        
                        break;
                    }
                        
                    case AIFFTag::Version:
                    {
                        // Read format number and check for the correct version of the AIFC specification
                        
                        if (!read_chunk(chunk, 4, chunk_size))
                            return Error::BadFormat;
                        
                        if (getU32(chunk, getHeaderEndianness()) != AIFC_CURRENT_SPECIFICATION)
                            return Error::WrongAIFCVersion;
                        
                        break;
                    }
                        
                    case AIFFTag::Audio:
                    {
                        if (!read_chunk(chunk, 4, chunk_size))
                            return Error::BadFormat;
                        
                        // Audio data starts after a 32-bit block size value (ignored) plus an offset readh here
                        
                        m_pcm_offset = position_internal() + 4 + getU32(chunk, getHeaderEndianness());
                        
                        break;
                    }
                        
                    case AIFFTag::Unknown:
                    {
                        // Read no data, but update the file position
                        
                        if (!read_chunk(nullptr, 0, chunk_size))
                            return Error::BadFormat;
                        
                        break;
                    }
                        
                    default:
                        break;
                }
            }
            
            // Check that all relevant chunks were found
            
            if ((~format_check) & format_valid)
                return Error::BadFormat;
            
            return Error::None;
        }
        
        Error parse_wave_header(const char* file_type)
        {
            char chunk[40];
            
            uint32_t chunk_size;
            
            m_format = AudioFileFormat(FileType::WAVE);
            
            // Check endianness
            
            Endianness endianness = match_tag(file_type, "RIFX") ? Endianness::Big : Endianness::Little;
            
            // Search for the format chunk and read the format chunk as needed, checking for a valid size
            
            if (!(find_chunk("fmt ", chunk_size)) || (chunk_size != 16 && chunk_size != 18 && chunk_size != 40))
                return Error::BadFormat;
            
            if (!read_chunk(chunk, chunk_size, chunk_size))
                return Error::BadFormat;
            
            // Retrieve relevant data
            
            uint16_t format_byte = getU16(chunk, getHeaderEndianness());
            uint16_t bit_depth = getU16(chunk + 14, getHeaderEndianness());
            
            // WAVE_FORMAT_EXTENSIBLE
            
            if (format_byte == 0xFFFE)
            {
                format_byte = getU16(chunk + 24, getHeaderEndianness());
                
                constexpr unsigned char guid[14] = { 0x00,0x00,0x00,0x00,0x10,0x00,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71 };
                
                if (std::memcmp(chunk + 26, &guid, 14) != 0)
                    return Error::UnsupportedWaveFormat;
            }
            
            // Check for a valid format byte (currently PCM or float only)
            
            if (format_byte != 0x0001 && format_byte != 0x0003)
                return Error::UnsupportedWaveFormat;
            
            NumericType type = format_byte == 0x0003 ? NumericType::Float : NumericType::Integer;
            
            m_num_channels = getU16(chunk + 2, getHeaderEndianness());
            m_sampling_rate = getU32(chunk + 4, getHeaderEndianness());
            
            // Search for the data chunk and retrieve frame size and file offset to audio data
            
            if (!find_chunk("data", chunk_size))
                return Error::BadFormat;
            
            // Set Format
            
            m_format = AudioFileFormat(FileType::WAVE, type, bit_depth, endianness);
            
            if (!m_format.is_valid())
                return Error::UnsupportedPCMFormat;
            
            m_num_frames = chunk_size / getFrameByteCount();
            m_pcm_offset = position_internal();
            
            return Error::None;
        }
        
        // Conversions
        
        template <class T, class U, class V>
        static U convert(V value, T, U)
        {
            return static_cast<U>(*reinterpret_cast<T*>(&value));
        }
        
        template <class T>
        static T convert(uint8_t value, uint8_t, T)
        {
            return (static_cast<T>(value) - T(128)) / T(128);
        }
        
        template <class T>
        static T convert(uint32_t value, uint32_t, T)
        {
            // N.B. the result of the shift is a negative int32_t value, hence the negation
            
            return convert<int32_t, T>(value, uint32_t(0), T(0)) * (T(-1.0) / static_cast<T>(int32_t(1) << int32_t(31)));
        }
        
        // Internal Typed Audio Read

        template <class T, class U, int N, int Shift, class V>
        void read_loop(V* output, uintptr_t j, uintptr_t loop_samples, uintptr_t byte_step)
        {
            Endianness endianness = getAudioEndianness();
            
            for (uintptr_t i = 0; i < loop_samples; i++, j += byte_step)
                output[i] = convert(get_bytes<U, N>(m_buffer.data() + j, endianness) << Shift, T(0), V(0));
        }
        
        template <class T>
        void read_audio(T* output, uintptr_t num_frames, int32_t channel = -1)
        {
            // Calculate sizes
            
            const uint32_t byte_depth = getByteDepth();
            const uint16_t num_channels = (channel < 0) ? getChannels() : 1;
            const uintptr_t byte_step = byte_depth * ((channel < 0) ? 1 : num_channels);
            const uintptr_t j = std::max(channel, 0) * byte_depth;
            
            while (num_frames)
            {
                const uintptr_t loop_frames = std::min(num_frames, WORK_LOOP_SIZE);
                const uintptr_t loop_samples = loop_frames * num_channels;
                
                // Read raw
                
                readRaw(m_buffer.data(), loop_frames);
                
                // Copy and convert to Output
                
                switch (getPCMFormat())
                {
                    case PCMFormat::Int8:
                        if (getFileType() == FileType::WAVE)
                            read_loop<uint8_t, uint8_t, 1, 0>(output, j, loop_samples, byte_step);
                        else
                            read_loop<uint32_t, uint32_t, 1, 24>(output, j, loop_samples, byte_step);
                        break;
                        
                    case PCMFormat::Int16:
                        read_loop<uint32_t, uint32_t, 2, 16>(output, j, loop_samples, byte_step);
                        break;
                        
                    case PCMFormat::Int24:
                        read_loop<uint32_t, uint32_t, 3,  8>(output, j, loop_samples, byte_step);
                        break;
                        
                    case PCMFormat::Int32:
                        read_loop<uint32_t, uint32_t, 4,  0>(output, j, loop_samples, byte_step);
                        break;
                        
                    case PCMFormat::Float32:
                        read_loop<float, uint32_t, 4,  0>(output, j, loop_samples, byte_step);
                        break;
                        
                    case PCMFormat::Float64:
                        read_loop<double, uint64_t, 8,  0>(output, j, loop_samples, byte_step);
                        break;
                        
                    default:
                        break;
                }
                
                num_frames -= loop_frames;
                output += loop_samples;
            }
        }
    };
}

#endif
