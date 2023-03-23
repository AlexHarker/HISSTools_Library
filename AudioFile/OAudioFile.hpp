#ifndef _HISSTOOLS_OAUDIOFILE_
#define _HISSTOOLS_OAUDIOFILE_

#include "AudioFileUtilities.hpp"
#include "BaseAudioFile.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace HISSTools
{
    class OAudioFile : public BaseAudioFile
    {
    public:
        
        // Constructor and Destructor

        OAudioFile() {}
        
        OAudioFile(const std::string& file, FileType type, PCMFormat format, uint16_t channels, double sr)
        {
            open(file, type, format, channels, sr);
        }
        
        OAudioFile(const std::string& file, FileType type, PCMFormat format, uint16_t channels, double sr, Endianness e)
        {
            open(file, type, format, channels, sr, e);
        }
    
        ~OAudioFile()
        {
            close();
        }

        // File Open

        void open(const std::string& file, FileType type, PCMFormat format, uint16_t channels, double sr)
        {
            open(file, type, format, channels, sr, type == FileType::WAVE ? Endianness::Little : Endianness::Big);
        }
    
        void open(const std::string& file, FileType type, PCMFormat format, uint16_t channels, double sr, Endianness endianness)
        {
            close();
            m_file.open(file.c_str(), ios_base::binary | ios_base::in | ios_base::out | ios_base::trunc);
            
            seek_internal(0);
            
            if (isOpen() && position_internal() == 0)
            {
                type = type == FileType::AIFF ? FileType::AIFC : type;
                
                m_format = AudioFileFormat(type, format, endianness);
                m_sampling_rate = sr;
                m_num_channels  = channels;
                m_num_frames = 0;
                m_pcm_offset = 0;
                
                if (getFileType() == FileType::WAVE)
                    writeWaveHeader();
                else
                    writeAIFCHeader();
                
                m_buffer.resize(WORK_LOOP_SIZE * getFrameByteCount());
            }
            else
                set_error_bit(Error::CouldNotOpen);
        }
        
        // File Position
        
        void seek(uintptr_t position)
        {
            if (get_pcm_offset() != 0)
                seek_internal(get_pcm_offset() + getFrameByteCount() * position);
        }
        
        uintptr_t getPosition()
        {
            if (get_pcm_offset())
                return static_cast<uintptr_t>((position_internal() - get_pcm_offset()) / getFrameByteCount());
            
            return 0;
        }
        
        // File Writing

        void writeRaw(const char *input, uintptr_t numFrames)
        {
            writePCMData(input, numFrames);
        }
        
        void writeInterleaved(const double* input, uintptr_t numFrames)
        {
            writeAudio(input, numFrames);
        }
        
        void writeInterleaved(const float* input, uintptr_t numFrames)
        {
            writeAudio(input, numFrames);
        }
        
        void writeChannel(const double* input, uintptr_t numFrames, uint16_t channel)
        {
            writeAudio(input, numFrames, channel);
        }
        
        void writeChannel(const float* input, uintptr_t numFrames, uint16_t channel)
        {
            writeAudio(input, numFrames, channel);
        }
            
    protected:
        
        uintptr_t getHeaderSize() const { return get_pcm_offset() - 8; }

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
        bool putBytes(T value, Endianness e)
        {
            unsigned char bytes[N];
            
            setBytes<N>(value, e, bytes);
            
            return write_internal(reinterpret_cast<const char*>(bytes), N);
        }
        
        bool putU32(uint32_t value, Endianness endianness)
        {
            return putBytes<uint32_t, 4>(value, endianness);
        }
        
        bool putU16(uint16_t value, Endianness endianness)
        {
            return putBytes<uint16_t, 2>(value, endianness);
        }

        bool putTag(const char* tag)
        {
            return write_internal(tag, 4);
        }
        
        bool putChunk(const char* tag, uint32_t size)
        {
            bool success = true;
            
            success &= write_internal(tag, 4);
            success &= putU32(size, getHeaderEndianness());
            
            return success;
        }
        
        bool putPadByte()
        {
            char padByte = 0;
            return write_internal(&padByte, 1);
        }

        bool putExtended(double value)
        {
            unsigned char bytes[10];
            
            IEEEDoubleExtendedConvertor()(value, bytes);
            
            return write_internal(reinterpret_cast<const char*>(bytes), 10);
        }
        
        bool putPString(const char* string)
        {
            char length = strlen(string);
            bool success = true;
            
            success &= write_internal(&length, 1);
            success &= write_internal(string, length);
            
            // Pad byte if necessary (number of bytes written so far is odd)
            
            if ((length + 1) & 0x1)
                success &= putPadByte();
            
            return success;
        }

        // Header Manipulation
        
        void writeWaveHeader()
        {
            bool success = true;
            
            // File Header
            
            if (getHeaderEndianness() == Endianness::Little)
                success &= putChunk("RIFF", 36);
            else
                success &= putChunk("RIFX", 36);
            
            success &= putTag("WAVE");
            
            // Format Chunk
            
            success &= putChunk("fmt ", 16);
            success &= putU16(getNumericType() == NumericType::Integer ? 0x1 : 0x3, getHeaderEndianness());
            success &= putU16(getChannels(), getHeaderEndianness());
            success &= putU32(getSamplingRate(), getHeaderEndianness());
            
            // Bytes per second and block alignment
            
            success &= putU32(getSamplingRate() * getFrameByteCount(), getHeaderEndianness());
            success &= putU16(static_cast<uint16_t>(getFrameByteCount()), getHeaderEndianness());
            
            success &= putU16(getBitDepth(), getHeaderEndianness());
            
            // Data Chunk (empty)
            
            success &= putChunk("data", 0);
            m_pcm_offset = position_internal();
            
            if (!success)
                set_error_bit(Error::CouldNotWrite);
        }
        
        void writeAIFCHeader()
        {
            bool success = true;
            
            // Compression Type
            
            auto lengthAsPString = [](const char* string)
            {
                uint32_t length = static_cast<uint32_t>(strlen(string));
                return ((length + 1) & 0x1) ? length + 2 : length + 1;
            };
            
            const char *compressionString = AIFCCompression::getString(m_format);
            const char *compressionTag = AIFCCompression::getTag(m_format);
            uint32_t compressionStringLength = lengthAsPString(compressionString);
            
            // Set file type, data size offset frames and header size
            
            uint32_t headerSize = 62 + compressionStringLength;
            
            // File Header
            
            success &= putChunk("FORM", headerSize);
            success &= putTag("AIFC");
            
            // FVER Chunk
            
            success &= putChunk("FVER", 4);
            success &= putU32(AIFC_CURRENT_SPECIFICATION, getHeaderEndianness());
            
            // COMM Chunk
            
            success &= putChunk("COMM", 22 + compressionStringLength);
            success &= putU16(getChannels(), getHeaderEndianness());
            success &= putU32(getFrames(), getHeaderEndianness());
            success &= putU16(getBitDepth(), getHeaderEndianness());
            success &= putExtended(getSamplingRate());
            success &= putTag(compressionTag);
            success &= putPString(compressionString);
            
            // SSND Chunk (zero size)
            
            success &= putChunk("SSND", 8);
            
            // Set offset and block size to zero
            
            success &= putU32(0, getHeaderEndianness());
            success &= putU32(0, getHeaderEndianness());
            
            // Set offset to PCM Data
            
            m_pcm_offset = position_internal();
            
            if (!success)
                set_error_bit(Error::CouldNotWrite);
        }
        
        bool updateHeader()
        {
            const uintptr_t endFrame = getPosition();
            
            bool success = true;
            
            if (endFrame > getFrames())
            {
                // Calculate new data length and end of data
                
                m_num_frames = endFrame;
                
                const uintptr_t dataBytes = (getFrameByteCount() * getFrames());
                const uintptr_t dataEnd = position_internal();
                
                // Write padding byte if relevant
                
                if (dataBytes & 0x1)
                    success &= putPadByte();
                
                // Update chunk sizes for (file, audio and also frames for AIFF/C)
                
                if (getFileType() == FileType::WAVE)
                {
                    success &= seek_internal(4);
                    success &= putU32(static_cast<uint32_t>(getHeaderSize() + padded_length(dataBytes)), getHeaderEndianness());
                    success &= seek_internal(get_pcm_offset() - 4);
                    success &= putU32(static_cast<uint32_t>(dataBytes), getHeaderEndianness());
                }
                else
                {
                    success &= seek_internal(4);
                    success &= putU32(static_cast<uint32_t>(getHeaderSize() + padded_length(dataBytes)), getHeaderEndianness());
                    success &= seek_internal(34);
                    success &= putU32(static_cast<uint32_t>(getFrames()), getHeaderEndianness());
                    success &= seek_internal(get_pcm_offset() - 12);
                    success &= putU32(static_cast<uint32_t>(dataBytes) + 8, getHeaderEndianness());
                }
                
                // Reset the position to end of the data
                
                success &= seek_internal(dataEnd);
            }
            
            return success;
        }

        // Resize
        
        bool resize(uintptr_t numFrames)
        {
            bool success = true;
            
            if (numFrames > getFrames())
            {
                // Calculate new data length and end of data
                
                uintptr_t dataBytes = (getFrameByteCount() * getFrames());
                uintptr_t endBytes = (getFrameByteCount() * numFrames);
                uintptr_t dataEnd = position_internal();
                
                seek(getFrames());
                
                for (uintptr_t i = 0; i < endBytes - dataBytes; i++)
                    success &= putPadByte();
                
                // Return to end of data
                
                success &= seek_internal(dataEnd);
            }
            
            return success;
        }
        
        // Write PCM Data
        
        bool writePCMData(const char* input, uintptr_t numFrames)
        {
            // Write audio and update the header
            
            bool success = write_internal(input, getFrameByteCount() * numFrames);
            success &= updateHeader();
            
            return success;
        }
        
        template <int N, class T, class U, class V>
        static T convert(V value, T, U)
        {
            U typedValue = static_cast<U>(value);
            return *(reinterpret_cast<T*>(&typedValue));
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
            
            const uint32_t maxVal = 1 << ((N * 8) - 1);
            
            value = std::round(value * static_cast<T>(maxVal));
            
            return static_cast<uint32_t>(std::min(std::max(value, static_cast<T>(-maxVal)), static_cast<T>(maxVal - 1)));
        }
        
        template <class T, class U, int N, class V>
        void writeLoop(const V* input, uintptr_t j, uintptr_t loopSamples, uintptr_t byteStep)
        {
            Endianness endianness = getAudioEndianness();
            
            for (uintptr_t i = 0; i < loopSamples; i++, j += byteStep)
                setBytes<N>(convert<N>(input[i], T(0), U(0)), endianness, m_buffer.data() + j);
        }
        
        template <class T>
        void writeAudio(const T* input, uintptr_t numFrames, int32_t channel = -1)
        {
            bool success = true;
            
            const uint32_t byteDepth = getByteDepth();
            const uint16_t numChannels = (channel < 0) ? getChannels() : 1;
            const uintptr_t byteStep = byteDepth * ((channel < 0) ? 1 : numChannels);
            const uintptr_t endFrame = getPosition() + numFrames;
            const uintptr_t j = channel * byteDepth;
            
            // Check if writing to all channels
            
            const bool allChannels = !(channel >= 0 && numChannels > 1);
            
            // Write zeros to channels if necessary (multichannel files are written one channel at a time)
            
            if (!allChannels)
                success &= resize(endFrame);
            
            m_file.seekg(m_file.tellp());
            
            channel = std::max(channel, 0);
            
            while (numFrames)
            {
                uintptr_t loopFrames = std::min(numFrames, WORK_LOOP_SIZE);
                uintptr_t loopSamples = loopFrames * numChannels;
                uintptr_t pos = m_file.tellg();
                
                // Read chunk from file to write back to
                
                if (!allChannels)
                {
                    m_file.clear();
                    m_file.read(reinterpret_cast<char*>(m_buffer.data()), loopFrames * getFrameByteCount());
                    
                    if (m_file.gcount() != loopFrames * getFrameByteCount())
                    {
                        set_error_bit(Error::CouldNotWrite);
                        return;
                    }
                }
                
                // Write audio data to a local buffer
                
                switch (getPCMFormat())
                {
                    case PCMFormat::Int8:
                        if (getFileType() == FileType::WAVE)
                            writeLoop<uint8_t, uint8_t, 1>(input, j, loopSamples, byteStep);
                        else
                            writeLoop<uint32_t, uint32_t, 1>(input, j, loopSamples, byteStep);
                        break;
                        
                    case PCMFormat::Int16:      writeLoop<uint32_t, uint32_t, 2>(input, j, loopSamples, byteStep);  break;
                    case PCMFormat::Int24:      writeLoop<uint32_t, uint32_t, 3>(input, j, loopSamples, byteStep);  break;
                    case PCMFormat::Int32:      writeLoop<uint32_t, uint32_t, 4>(input, j, loopSamples, byteStep);  break;
                    case PCMFormat::Float32:    writeLoop<uint32_t, float, 4>(input, j, loopSamples, byteStep);     break;
                    case PCMFormat::Float64:    writeLoop<uint64_t, double, 8>(input, j, loopSamples, byteStep);    break;
                }
                
                // Write buffer back to file
                
                if (!allChannels)
                    m_file.seekp(pos);
                
                write_internal(reinterpret_cast<const char*>(m_buffer.data()), loopFrames * getFrameByteCount());
                
                numFrames -= loopFrames;
                input     += loopSamples;
            }
            
            // Update number of frames
            
            success &= updateHeader();
            
            // return success;
        }
    };
}

#endif /* _HISSTOOLS_OAUDIOFILE_ */
