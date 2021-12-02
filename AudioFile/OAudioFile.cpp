#include "OAudioFile.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace HISSTools
{
    template <typename T> T paddedLength(T length)
    {
        return length + (length & 0x1);
    }

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
        mFile.open(i.c_str(), std::ios_base::binary);

        seekInternal(0);

        if (isOpen() && positionInternal() == 0)
        {
            mFileType = type == FileType::AIFF ? FileType::AIFC : type;
            mPCMFormat = format;
            mHeaderEndianness = getFileType() == FileType::WAVE ? endianness : Endianness::Big;
            mAudioEndianness = endianness;
            mSamplingRate = sr;
            mNumChannels  = channels;
            mNumFrames = 0;
            mPCMOffset = 0;

            if (getFileType() == FileType::WAVE)
                writeWaveHeader();
            else
                writeAIFCHeader();
        }
        else
            setErrorBit(Error::CouldNotOpen);
    }

    void OAudioFile::close()
    {
        mFile.close();
        clear();
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

    bool OAudioFile::putU64(uint64_t value, Endianness fileEndianness)
    {
        unsigned char bytes[8];

        if (fileEndianness == Endianness::Big)
        {
            bytes[0] = (value >> 56) & 0xFF;
            bytes[1] = (value >> 48) & 0xFF;
            bytes[2] = (value >> 40) & 0xFF;
            bytes[3] = (value >> 32) & 0xFF;
            bytes[4] = (value >> 24) & 0xFF;
            bytes[5] = (value >> 16) & 0xFF;
            bytes[6] = (value >> 8) & 0xFF;
            bytes[7] = (value >> 0) & 0xFF;
        }
        else
        {
            bytes[0] = (value >> 0) & 0xFF;
            bytes[1] = (value >> 8) & 0xFF;
            bytes[2] = (value >> 16) & 0xFF;
            bytes[3] = (value >> 24) & 0xFF;
            bytes[4] = (value >> 32) & 0xFF;
            bytes[5] = (value >> 40) & 0xFF;
            bytes[6] = (value >> 48) & 0xFF;
            bytes[7] = (value >> 56) & 0xFF;
        }

        return writeInternal(reinterpret_cast<const char*>(bytes), 8);
    }

    bool OAudioFile::putU32(uint32_t value, Endianness fileEndianness)
    {
        unsigned char bytes[4];

        if (fileEndianness == Endianness::Big)
        {
            bytes[0] = (value >> 24) & 0xFF;
            bytes[1] = (value >> 16) & 0xFF;
            bytes[2] = (value >> 8) & 0xFF;
            bytes[3] = (value >> 0) & 0xFF;
        }
        else
        {
            bytes[0] = (value >> 0) & 0xFF;
            bytes[1] = (value >> 8) & 0xFF;
            bytes[2] = (value >> 16) & 0xFF;
            bytes[3] = (value >> 24) & 0xFF;
        }

        return writeInternal(reinterpret_cast<const char*>(bytes), 4);
    }

    bool OAudioFile::putU24(uint32_t value, Endianness fileEndianness)
    {
        unsigned char bytes[3];

        if (fileEndianness == Endianness::Big)
        {
            bytes[0] = (value >> 16) & 0xFF;
            bytes[1] = (value >> 8) & 0xFF;
            bytes[2] = (value >> 0) & 0xFF;
        }
        else
        {
            bytes[0] = (value >> 0) & 0xFF;
            bytes[1] = (value >> 8) & 0xFF;
            bytes[2] = (value >> 16) & 0xFF;
        }

        return writeInternal(reinterpret_cast<const char*>(bytes), 3);
    }

    bool OAudioFile::putU16(uint32_t value, Endianness fileEndianness)
    {
        unsigned char bytes[2];

        if (fileEndianness == Endianness::Big)
        {
            bytes[0] = (value >> 8) & 0xFF;
            bytes[1] = (value >> 0) & 0xFF;
        }
        else
        {
            bytes[0] = (value >> 0) & 0xFF;
            bytes[1] = (value >> 8) & 0xFF;
        }

        return writeInternal(reinterpret_cast<const char*>(bytes), 2);
    }

    bool OAudioFile::putU08(uint32_t value)
    {
        unsigned char byte;

        byte = value & 0xFF;

        return writeInternal(reinterpret_cast<const char*>(&byte), 1);
    }

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

    uint32_t doubleToUInt32(double x)
    {
        return ((uint32_t)(((int32_t)(x - 2147483648.0)) + 2147483647L) + 1);
    }

    void doubleToExtended(double num, unsigned char* bytes)
    {
        int sign;
        int expon;
        double fMant, fsMant;
        uint32_t hiMant, loMant;

        if (num < 0)
        {
            sign = 0x8000;
            num *= -1;
        }
        else
        {
            sign = 0;
        }

        if (num == 0)
        {
            expon = 0;
            hiMant = 0;
            loMant = 0;
        }
        else
        {
            fMant = frexp(num, &expon);
            if ((expon > 16384) || !(fMant < 1))
            { /* Infinity or NaN */
                expon = sign | 0x7FFF;
                hiMant = 0;
                loMant = 0; /* infinity */
            }
            else
            { /* Finite */
                expon += 16382;
                if (expon < 0)
                { /* denormalized */
                    fMant = ldexp(fMant, expon);
                    expon = 0;
                }
                expon |= sign;
                fMant = ldexp(fMant, 32);
                fsMant = floor(fMant);
                hiMant = doubleToUInt32(fsMant);
                fMant = ldexp(fMant - fsMant, 32);
                fsMant = floor(fMant);
                loMant = doubleToUInt32(fsMant);
            }
        }

        bytes[0] = expon >> 8;
        bytes[1] = expon;
        bytes[2] = hiMant >> 24;
        bytes[3] = hiMant >> 16;
        bytes[4] = hiMant >> 8;
        bytes[5] = hiMant;
        bytes[6] = loMant >> 24;
        bytes[7] = loMant >> 16;
        bytes[8] = loMant >> 8;
        bytes[9] = loMant;
    }

    bool OAudioFile::putExtended(double value)
    {
        unsigned char bytes[10];

        doubleToExtended(value, bytes);

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

        if (!success) setErrorBit(Error::CouldNotWrite);
    }

    const char* OAudioFile::getCompressionTag() const
    {
        // FIX - doesn't deal with little endian... (return type)? "sowt"
        
        switch (getPCMFormat())
        {
            case PCMFormat::Int8:
            case PCMFormat::Int16:
            case PCMFormat::Int24:
            case PCMFormat::Int32:
                return "NONE";
            case PCMFormat::Float32:
                return "fl32";
            case PCMFormat::Float64:
                return "fl64";
        }
    }

    const char* OAudioFile::getCompressionString() const
    {
        // FIX - doesn't deal with little endian... (return type)? "little endian"
        
        switch (getPCMFormat())
        {
            case PCMFormat::Int8:
            case PCMFormat::Int16:
            case PCMFormat::Int24:
            case PCMFormat::Int32:
                return "not compressed";
            case PCMFormat::Float32:
                return "32-bit floating point";
            case PCMFormat::Float64:
                return "64-bit floating point";
        }
    }

    void OAudioFile::writeAIFCHeader()
    {
        bool success = true;

        // Set file type, data size offset frames and header size

        uint32_t headerSize = 62 + lengthAsPString(getCompressionString());

        // File Header

        success &= putChunk("FORM", headerSize);
        success &= putTag("AIFC");

        // FVER Chunk

        success &= putChunk("FVER", 4);
        success &= putU32(AIFC_CURRENT_SPECIFICATION, getHeaderEndianness());

        // COMM Chunk

        success &= putChunk("COMM", 22 + lengthAsPString(getCompressionString()));
        success &= putU16(getChannels(), getHeaderEndianness());
        success &= putU32(getFrames(), getHeaderEndianness());
        success &= putU16(getBitDepth(), getHeaderEndianness());
        success &= putExtended(getSamplingRate());
        success &= putTag(getCompressionTag());
        success &= putPString(getCompressionString());

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

    uint32_t OAudioFile::inputToU32(double input, int32_t bitDepth)
    {
        // FIX - issues of value (note 1 might be clipped by one value - what to do?)
        
        const uint32_t maxVal = 1 << (bitDepth - 1);
        
        input = round(input * static_cast<double>(maxVal));
        
        std::min(std::max(input, static_cast<double>(-maxVal)), static_cast<double>(maxVal - 1));

        return static_cast<uint32_t>(input);
    }
    
    uint8_t OAudioFile::inputToU8(double input)
    {
        // Convert then clip before casting
        
        input = round((input * 128.0) + 128.0);
        input = std::min(std::max(input, 0.0), 255.0);
        
        return static_cast<uint8_t>(input);
    }

    template <class T>
    void OAudioFile::writeAudio(const T* input, uintptr_t numFrames, int32_t channel)
    {
        bool success = true;

        uint32_t byteDepth = getByteDepth();
        uintptr_t inputSamples = (channel < 0) ? getChannels() * numFrames : numFrames;
        uintptr_t offset = (channel < 0) ? 0 : channel * byteDepth;
        uintptr_t byteStep = (channel < 0) ? 0 : getFrameByteCount() - (byteDepth + offset);
        uintptr_t endFrame = getPosition() + numFrames;
        
        // FIX - the slowest thing is seeking in the file, so that seems like a bad plan - it might be better to read in chunks overwrite locally and then write back the chunk
        
        // Write zeros to channels if necessary (multichannel files written one channel at a time)

        if (channel >= 0 && getChannels() > 1)
            success &= resize(endFrame);

        // Write audio data

        switch (getPCMFormat())
        {
            case PCMFormat::Int8:
                if (getFileType() == FileType::WAVE)
                {
                    for (uintptr_t i = 0; i < inputSamples; i++)
                    {
                        seekRelativeInternal(offset);
                        putU08(inputToU8(input[i]));
                        seekRelativeInternal(byteStep);
                    }
                }
                else
                {
                    for (uintptr_t i = 0; i < inputSamples; i++)
                    {
                        seekRelativeInternal(offset);
                        putU08(inputToU32(input[i], 8));
                        seekRelativeInternal(byteStep);
                    }
                }
                break;

            case PCMFormat::Int16:
                for (uintptr_t i = 0; i < inputSamples; i++)
                {
                    seekRelativeInternal(offset);
                    putU16(inputToU32(input[i], 16), getAudioEndianness());
                    seekRelativeInternal(byteStep);
                }
                break;

            case PCMFormat::Int24:
                for (uintptr_t i = 0; i < inputSamples; i++)
                {
                    seekRelativeInternal(offset);
                    putU24(inputToU32(input[i], 24), getAudioEndianness());
                    seekRelativeInternal(byteStep);
                }
                break;

            case PCMFormat::Int32:
                for (uintptr_t i = 0; i < inputSamples; i++)
                {
                    seekRelativeInternal(offset);
                    putU32(inputToU32(input[i], 32), getAudioEndianness());
                    seekRelativeInternal(byteStep);
                }
                break;

            case PCMFormat::Float32:
                for (uintptr_t i = 0; i < inputSamples; i++)
                {
                    seekRelativeInternal(offset);
                    float value = input[i];
                    putU32(*reinterpret_cast<uint32_t*>(&value), getAudioEndianness());
                    seekRelativeInternal(byteStep);
                }
                break;

            case PCMFormat::Float64:
                for (uintptr_t i = 0; i < inputSamples; i++)
                {
                    seekRelativeInternal(offset);
                    double value = input[i];
                    putU64(*reinterpret_cast<uint64_t*>(&value), getAudioEndianness());
                    seekRelativeInternal(byteStep);
                }
                break;
        }

        // Update number of frames

        success &= updateHeader();

        // return success;
    }
}
