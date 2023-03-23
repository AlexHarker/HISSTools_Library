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
            mFile.open(file.c_str(), std::ios_base::binary | std::ios_base::in | std::ios_base::out | std::ios_base::trunc);
            
            seekInternal(0);
            
            if (isOpen() && positionInternal() == 0)
            {
                type = type == FileType::AIFF ? FileType::AIFC : type;
                
                mFormat = AudioFileFormat(type, format, endianness);
                mSamplingRate = sr;
                mNumChannels  = channels;
                mNumFrames = 0;
                mPCMOffset = 0;
                
                if (getFileType() == FileType::WAVE)
                    writeWaveHeader();
                else
                    writeAIFCHeader();
                
                mBuffer.resize(WORK_LOOP_SIZE * getFrameByteCount());
            }
            else
                setErrorBit(Error::CouldNotOpen);
        }
        
        // File Position
        
        void seek(uintptr_t position)
        {
            if (getPCMOffset() != 0)
                seekInternal(getPCMOffset() + getFrameByteCount() * position);
        }
        
        uintptr_t getPosition()
        {
            if (getPCMOffset())
                return static_cast<uintptr_t>((positionInternal() - getPCMOffset()) / getFrameByteCount());
            
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
        
        uintptr_t getHeaderSize() const { return getPCMOffset() - 8; }

    private:
        
        // Internal File Handling

        bool writeInternal(const char* buffer, uintptr_t bytes)
        {
            mFile.clear();
            mFile.write(buffer, bytes);
            return mFile.good();
        }
        
        bool seekInternal(uintptr_t position)
        {
            mFile.clear();
            mFile.seekp(position, std::ios_base::beg);
            return positionInternal() == position;
        }
        
        bool seekRelativeInternal(uintptr_t offset)
        {
            if (!offset)
                return true;
            mFile.clear();
            uintptr_t newPosition = positionInternal() + offset;
            mFile.seekp(offset, std::ios_base::cur);
            return positionInternal() == newPosition;
        }
        
        uintptr_t positionInternal()
        {
            return mFile.tellp();
        }
        
        // Putters
        
        template <class T, int N>
        bool putBytes(T value, Endianness e)
        {
            unsigned char bytes[N];
            
            setBytes<N>(value, e, bytes);
            
            return writeInternal(reinterpret_cast<const char*>(bytes), N);
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
            return writeInternal(tag, 4);
        }
        
        bool putChunk(const char* tag, uint32_t size)
        {
            bool success = true;
            
            success &= writeInternal(tag, 4);
            success &= putU32(size, getHeaderEndianness());
            
            return success;
        }
        
        bool putPadByte()
        {
            char padByte = 0;
            return writeInternal(&padByte, 1);
        }

        bool putExtended(double value)
        {
            unsigned char bytes[10];
            
            IEEEDoubleExtendedConvertor()(value, bytes);
            
            return writeInternal(reinterpret_cast<const char*>(bytes), 10);
        }
        
        bool putPString(const char* string)
        {
            char length = strlen(string);
            bool success = true;
            
            success &= writeInternal(&length, 1);
            success &= writeInternal(string, length);
            
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
            mPCMOffset = positionInternal();
            
            if (!success)
                setErrorBit(Error::CouldNotWrite);
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
            
            const char *compressionString = AIFCCompression::getString(mFormat);
            const char *compressionTag = AIFCCompression::getTag(mFormat);
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
            
            mPCMOffset = positionInternal();
            
            if (!success)
                setErrorBit(Error::CouldNotWrite);
        }
        
        bool updateHeader()
        {
            uintptr_t endFrame = getPosition();
            
            bool success = true;
            
            if (endFrame > getFrames())
            {
                // Calculate new data length and end of data
                
                mNumFrames = endFrame;
                
                uintptr_t dataBytes = (getFrameByteCount() * getFrames());
                uintptr_t dataEnd = positionInternal();
                
                // Write padding byte if relevant
                
                if (dataBytes & 0x1)
                    success &= putPadByte();
                
                // Update chunk size for file and audio data and frames for aifc and reset position to the end of the data
                
                if (getFileType() == FileType::WAVE)
                {
                    success &= seekInternal(4);
                    success &= putU32(static_cast<uint32_t>(getHeaderSize() + paddedLength(dataBytes)), getHeaderEndianness());
                    success &= seekInternal(getPCMOffset() - 4);
                    success &= putU32(static_cast<uint32_t>(dataBytes), getHeaderEndianness());
                }
                else
                {
                    success &= seekInternal(4);
                    success &= putU32(static_cast<uint32_t>(getHeaderSize() + paddedLength(dataBytes)), getHeaderEndianness());
                    success &= seekInternal(34);
                    success &= putU32(static_cast<uint32_t>(getFrames()), getHeaderEndianness());
                    success &= seekInternal(getPCMOffset() - 12);
                    success &= putU32(static_cast<uint32_t>(dataBytes) + 8, getHeaderEndianness());
                }
                
                // Return to end of data
                
                success &= seekInternal(dataEnd);
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
                uintptr_t dataEnd = positionInternal();
                
                seek(getFrames());
                
                for (uintptr_t i = 0; i < endBytes - dataBytes; i++)
                    success &= putPadByte();
                
                // Return to end of data
                
                success &= seekInternal(dataEnd);
            }
            
            return success;
        }
        
        // Write PCM Data
        
        bool writePCMData(const char* input, uintptr_t numFrames)
        {
            // Write audio and update the header
            
            bool success = writeInternal(input, getFrameByteCount() * numFrames);
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
                setBytes<N>(convert<N>(input[i], T(0), U(0)), endianness, mBuffer.data() + j);
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
            
            mFile.seekg(mFile.tellp());
            
            channel = std::max(channel, 0);
            
            while (numFrames)
            {
                uintptr_t loopFrames = std::min(numFrames, WORK_LOOP_SIZE);
                uintptr_t loopSamples = loopFrames * numChannels;
                uintptr_t pos = mFile.tellg();
                
                // Read chunk from file to write back to
                
                if (!allChannels)
                {
                    mFile.clear();
                    mFile.read(reinterpret_cast<char*>(mBuffer.data()), loopFrames * getFrameByteCount());
                    
                    if (mFile.gcount() != loopFrames * getFrameByteCount())
                    {
                        setErrorBit(Error::CouldNotWrite);
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
                    mFile.seekp(pos);
                
                writeInternal(reinterpret_cast<const char*>(mBuffer.data()), loopFrames * getFrameByteCount());
                
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
