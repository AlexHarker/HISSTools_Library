#include "OAudioFile.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "AudioFileUtilities.hpp"

namespace HISSTools
{    
    uint32_t lengthAsPString(const char* string)
    {
        uint32_t length = static_cast<uint32_t>(strlen(string));
        return ((length + 1) & 0x1) ? length + 2 : length + 1;
    }

    OAudioFile::OAudioFile(const std::string& i, FileType type, PCMFormat format, uint16_t channels, double sr)
    {
        open(i, type, format, channels, sr);
    }

    OAudioFile::OAudioFile(const std::string& i, FileType type, PCMFormat format, uint16_t channels, double sr, Endianness e)
    {
        open(i, type, format, channels, sr, e);
    }

    void OAudioFile::open(const std::string& i, FileType type, PCMFormat format, uint16_t channels, double sr)
    {
        open(i, type, format, channels, sr,type == FileType::WAVE ? Endianness::Little : Endianness::Big);
    }

    void OAudioFile::open(const std::string& i, FileType type, PCMFormat format, uint16_t channels, double sr, Endianness endianness)
    {
        close();
        using std::ios_base;
        mFile.open(i.c_str(), ios_base::binary | ios_base::in | ios_base::out | ios_base::trunc);

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

    void OAudioFile::seek(uintptr_t position)
    {
        if (getPCMOffset() != 0)
            seekInternal(getPCMOffset() + getFrameByteCount() * position);
    }

    uintptr_t OAudioFile::getPosition()
    {
        if (getPCMOffset())
            return static_cast<uintptr_t>((positionInternal() - getPCMOffset()) / getFrameByteCount());

        return 0;
    }

    uintptr_t OAudioFile::positionInternal()
    {
        return mFile.tellp();
    }

    bool OAudioFile::seekInternal(uintptr_t position)
    {
        mFile.clear();
        mFile.seekp(position, std::ios_base::beg);
        return positionInternal() == position;
    }

    bool OAudioFile::seekRelativeInternal(uintptr_t offset)
    {
        if (!offset)
            return true;
        mFile.clear();
        uintptr_t newPosition = positionInternal() + offset;
        mFile.seekp(offset, std::ios_base::cur);
        return positionInternal() == newPosition;
    }

    bool OAudioFile::writeInternal(const char* buffer, uintptr_t bytes)
    {
        mFile.clear();
        mFile.write(buffer, bytes);
        return mFile.good();
    }

    template <class T, int N>
    bool OAudioFile::putBytes(T value, Endianness e)
    {
        unsigned char bytes[N];
        
        setBytes<N>(value, e, bytes);
        
        return writeInternal(reinterpret_cast<const char*>(bytes), N);
    }
    
    bool OAudioFile::putU64(uint64_t value, Endianness e) { return putBytes<uint64_t, 8>(value, e); }
    bool OAudioFile::putU32(uint32_t value, Endianness e) { return putBytes<uint32_t, 4>(value, e); }
    bool OAudioFile::putU24(uint32_t value, Endianness e) { return putBytes<uint32_t, 3>(value, e); }
    bool OAudioFile::putU16(uint32_t value, Endianness e) { return putBytes<uint32_t, 2>(value, e); }

    bool OAudioFile::putPadByte()
    {
        char padByte = 0;
        return writeInternal(&padByte, 1);
    }

    bool OAudioFile::putChunk(const char* tag, uint32_t size)
    {
        bool success = true;
        
        success &= writeInternal(tag, 4);
        success &= putU32(size, getHeaderEndianness());
        
        return success;
    }

    bool OAudioFile::putTag(const char* tag)
    {
        return writeInternal(tag, 4);
    }

    bool OAudioFile::putExtended(double value)
    {
        unsigned char bytes[10];

        IEEEDoubleExtendedConvertor()(value, bytes);

        return writeInternal(reinterpret_cast<const char*>(bytes), 10);
    }

    bool OAudioFile::putPString(const char* string)
    {
        char length = strlen(string);
        bool success = true;

        success &= writeInternal(&length, 1);
        success &= writeInternal(string, length);

        // Pad byte if necessary (number of bytes written so far is odd)

        if ((length + 1) & 0x1) success &= putPadByte();

        return success;
    }

    void OAudioFile::writeWaveHeader()
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
        // Bytes per second
        success &= putU32(getSamplingRate() * getFrameByteCount(), getHeaderEndianness());
        // Block alignment
        success &= putU16(static_cast<uint16_t>(getFrameByteCount()), getHeaderEndianness());
        success &= putU16(getBitDepth(), getHeaderEndianness());

        // Data Chunk (empty)

        success &= putChunk("data", 0);
        mPCMOffset = positionInternal();

        if (!success)
            setErrorBit(Error::CouldNotWrite);
    }

    void OAudioFile::writeAIFCHeader()
    {
        bool success = true;

        const char *compressionString = AIFCCompression::getString(mFormat);
        const char *compressionTag = AIFCCompression::getTag(mFormat);

        // Set file type, data size offset frames and header size

        uint32_t headerSize = 62 + lengthAsPString(compressionString);

        // File Header

        success &= putChunk("FORM", headerSize);
        success &= putTag("AIFC");

        // FVER Chunk

        success &= putChunk("FVER", 4);
        success &= putU32(AIFC_CURRENT_SPECIFICATION, getHeaderEndianness());

        // COMM Chunk

        success &= putChunk("COMM", 22 + lengthAsPString(compressionString));
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

    bool OAudioFile::updateHeader()
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

            if (dataBytes & 0x1) success &= putPadByte();

            // Update chunk size for file and audio data and frames for aifc
            // and reset position to the end of the data

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

    bool OAudioFile::resize(uintptr_t numFrames)
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

    bool OAudioFile::writePCMData(const char* input, uintptr_t numFrames)
    {
        // Write audio

        bool success = writeInternal(input, getFrameByteCount() * numFrames);

        // Update number of frames

        success &= updateHeader();

        return success;
    }
    
    template <int N, class T, class U, class V>
    T convert(V value, T, U)
    {
        U typedValue = static_cast<U>(value);
        return *(reinterpret_cast<T*>(&typedValue));
    }
    
    template <int N, class T>
    uint8_t convert(T value, uint8_t, uint8_t)
    {
        // Scale then clip before casting
        
        value = std::round((value * T(128)) + T(128));
        value = std::min(std::max(value, T(0)), T(255));
        
        return static_cast<uint8_t>(value);
    }
    
    template <int N, class T>
    uint32_t convert(T value, uint32_t, uint32_t)
    {
        // FIX - issues of value (note 1 might be clipped by one value - what to do?)
        
        const uint32_t maxVal = 1 << ((N * 8) - 1);
        
        value = std::round(value * static_cast<T>(maxVal));
                
        return static_cast<uint32_t>(std::min(std::max(value, static_cast<T>(-maxVal)), static_cast<T>(maxVal - 1)));
    }
               
    template <class T, class U, int N, class V>
    void OAudioFile::writeLoop(const V* input, uintptr_t j, uintptr_t loopSamples, uintptr_t byteStep)
    {
        Endianness endianness = getAudioEndianness();
        
        for (uintptr_t i = 0; i < loopSamples; i++, j += byteStep)
            setBytes<N>(convert<N>(input[i], T(0), U(0)), endianness, mBuffer.data() + j);
    }
    
    template <class T>
    void OAudioFile::writeAudio(const T* input, uintptr_t numFrames, int32_t channel)
    {
        bool success = true;
  
        const uint32_t byteDepth = getByteDepth();
        const uint16_t numChannels = (channel < 0) ? getChannels() : 1;
        const uintptr_t byteStep = byteDepth * ((channel < 0) ? 1 : numChannels);
        const uintptr_t endFrame = getPosition() + numFrames;

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
            uintptr_t j = channel * byteDepth;
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
              
            bool didwrite = writeInternal(reinterpret_cast<const char*>(mBuffer.data()), loopFrames * getFrameByteCount());
            
            numFrames -= loopFrames;
            input     += loopSamples;
        }
        
        // Update number of frames

        success &= updateHeader();

        // return success;
    }
}
