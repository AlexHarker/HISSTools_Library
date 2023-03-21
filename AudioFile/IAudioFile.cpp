#include "IAudioFile.h"

#include <cmath>
#include <cassert>
#include <cstring>
#include <vector>
#include <algorithm>

#define WORK_LOOP_SIZE 1024

namespace HISSTools
{
    // Length Helper
    
    template <typename T>
    T paddedLength(T length)
    {
        return length + (length & 0x1);
    }

    // File Opening / Close
    
    void IAudioFile::open(const std::string& i)
    {
        close();
        
        if (!i.empty())
        {
            mFile.open(i.c_str(), std::ios_base::binary);
            if (mFile.is_open())
            {
                setErrorBit(parseHeader());
                mBuffer = new char[WORK_LOOP_SIZE * getFrameByteCount()];
                seek();
            }
            else
                setErrorBit(Error::CouldNotOpen);
        }
    }

    void IAudioFile::close()
    {
        mFile.close();
        clear();
        if (mBuffer)
        {
            delete[] mBuffer;
            mBuffer = nullptr;
        }
    }

    bool IAudioFile::isOpen()
    {
        return mFile.is_open();
    }
    
    // File Position

    void IAudioFile::seek(uintptr_t position)
    {
        seekInternal(getPCMOffset() + getFrameByteCount() * position);
    }

    uintptr_t IAudioFile::getPosition()
    {
        if (getPCMOffset())
            return static_cast<uintptr_t>((positionInternal() - getPCMOffset()) / getFrameByteCount());
        
        return 0;
    }
    
    // File Reading
    
    void IAudioFile::readRaw(void* output, uintptr_t numFrames)
    {
        readInternal(static_cast<char *>(output), getFrameByteCount() * numFrames);
    }
    
    // Chunks

    std::vector<std::string> IAudioFile::getChunkTags()
    {
        PostionRestore restore(*this);

        char tag[5] = {0, 0, 0, 0, 0};
        uint32_t chunkSize;
        
        std::vector<std::string> tags;
                        
        // Iterate through chunks

        while (readChunkHeader(tag, chunkSize))
        {
            tags.push_back(tag);
            readChunk(NULL, 0, chunkSize);
        }
                
        return tags;
    }
    
    uintptr_t IAudioFile::getChunkSize(const char *tag)
    {
        PostionRestore restore(*this);
        
        uint32_t chunkSize = 0;
        
        if (strlen(tag) <= 4)
            findChunk(tag, chunkSize);
                
        return chunkSize;
    }

    void IAudioFile::readRawChunk(void *output, const char *tag)
    {
        PostionRestore restore(*this);
        
        uint32_t chunkSize = 0;
        
        if (strlen(tag) <= 4)
        {
            findChunk(tag, chunkSize);
            readInternal(static_cast<char *>(output), chunkSize);
        }
    }

    //  Internal File Handling

    bool IAudioFile::readInternal(char* buffer, uintptr_t numBytes)
    {
        mFile.clear();
        mFile.read(buffer, numBytes);
        
       return static_cast<uintptr_t>(mFile.gcount()) == numBytes;
    }
    
    bool IAudioFile::seekInternal(uintptr_t position)
    {
        mFile.clear();
        mFile.seekg(position, std::ios_base::beg);
        return positionInternal() == position;
    }
    
    bool IAudioFile::advanceInternal(uintptr_t offset)
    {
        return seekInternal(positionInternal() + offset);
    }

    uintptr_t IAudioFile::positionInternal()
    {
        return mFile.tellg();
    }
    
    //  Extracting Single Values
    
    uint64_t IAudioFile::getU64(const char* b, Endianness fileEndianness) const
    {
        const unsigned char* bytes = reinterpret_cast<const unsigned char*>(b);
        
        if (fileEndianness == Endianness::Big)
            return ((uint64_t)bytes[0] << 56) | ((uint64_t)bytes[1] << 48)
            | ((uint64_t)bytes[2] << 40) | ((uint64_t)bytes[3] << 32)
            | ((uint64_t)bytes[4] << 24) | ((uint64_t)bytes[5] << 16)
            | ((uint64_t)bytes[6] << 8) | (uint64_t)bytes[7];
        else
            return ((uint64_t)bytes[7] << 56) | ((uint64_t)bytes[6] << 48)
            | ((uint64_t)bytes[5] << 40) | ((uint64_t)bytes[4] << 32)
            | ((uint64_t)bytes[3] << 24) | ((uint64_t)bytes[2] << 16)
            | ((uint64_t)bytes[1] << 8) | (uint64_t)bytes[0];
    }
    
    uint32_t IAudioFile::getU32(const char* b, Endianness fileEndianness) const
    {
        const unsigned char* bytes = reinterpret_cast<const unsigned char*>(b);
        
        if (fileEndianness == Endianness::Big)
            return (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
        else
            return (bytes[3] << 24) | (bytes[2] << 16) | (bytes[1] << 8) | bytes[0];
    }
    
    uint32_t IAudioFile::getU24(const char* b, Endianness fileEndianness) const
    {
        const unsigned char* bytes = reinterpret_cast<const unsigned char*>(b);
        
        if (fileEndianness == Endianness::Big)
            return (bytes[0] << 16) | (bytes[1] << 8) | bytes[2];
        else
            return (bytes[2] << 16) | (bytes[1] << 8) | bytes[0];
    }
    
    uint32_t IAudioFile::getU16(const char* b, Endianness fileEndianness) const
    {
        const unsigned char* bytes = reinterpret_cast<const unsigned char*>(b);
        
        if (fileEndianness == Endianness::Big)
            return (bytes[0] << 8) | bytes[1];
        else
            return (bytes[1] << 8) | bytes[0];
    }
    
    // Conversion
    
    double IAudioFile::extendedToDouble(const char* bytes) const
    {
        // Get double from big-endian ieee 80 bit extended floating point format
        
        bool sign = getU16(bytes, Endianness::Big) & 0x8000;
        int32_t exponent = getU16(bytes, Endianness::Big) & 0x777F;
        uint32_t hiSignificand = getU32(bytes + 2, Endianness::Big);
        uint32_t loSignificand = getU32(bytes + 6, Endianness::Big);
        
        double value;
        
        // Special handlers for zeros and infs / NaNs (in either case the
        // file is probably useless as the sampling rate *should* have a
        // sensible value)
        
        if (!exponent && !hiSignificand && !loSignificand) return 0.0;
        
        if (exponent == 0x777F) return HUGE_VAL;
        
        exponent -= 16383;
        value = ldexp(static_cast<double>(hiSignificand), exponent - 31);
        value += ldexp(static_cast<double>(loSignificand), exponent - (31 + 32));
        
        if (sign) value = -value;
        
        return value;
    }

    template <class T>
    void IAudioFile::u32ToOutput(T* output, uint32_t value)
    {
        // N.B. the result of the shift is a negative int32_T value, hence the negation
        
        *output = *reinterpret_cast<int32_t*>(&value) * (T(-1.0) / static_cast<T>(int32_t(1) << int32_t(31)));
    }
    
    template <class T>
    void IAudioFile::u8ToOutput(T* output, uint8_t value)
    {
        *output = (T(value) - T(128)) / T(128);
    }
    
    template <class T>
    void IAudioFile::float32ToOutput(T* output, uint32_t value)
    {
        *output = *reinterpret_cast<float*>(&value);
    }
    
    template <class T>
    void IAudioFile::float64ToOutput(T* output, uint64_t value)
    {
        *output = static_cast<T>(*reinterpret_cast<double*>(&value));
    }
    
     //  Chunk Reading
    
    bool IAudioFile::matchTag(const char* a, const char* b)
    {
        return (strncmp(a, b, 4) == 0);
    }

    bool IAudioFile::readChunkHeader(char* tag, uint32_t& chunkSize)
    {
        char header[8] = {};
        
        if (!readInternal(header, 8)) return false;
        
        strncpy(tag, header, 4);
        chunkSize = getU32(header + 4, getHeaderEndianness());
        
        return true;
    }

    bool IAudioFile::findChunk(const char* searchTag, uint32_t& chunkSize)
    {
        char tag[4] = {};
        
        while (readChunkHeader(tag, chunkSize))
        {
            if (matchTag(tag, searchTag)) return true;
            
            if (!advanceInternal(paddedLength(chunkSize))) return false;
        }
        
        return false;
    }
    
    bool IAudioFile::readChunk(char* data, uint32_t readSize, uint32_t chunkSize)
    {
        if (readSize)
            if (readSize > chunkSize || !readInternal(data, readSize))
                return false;
        
        if (!advanceInternal(paddedLength(chunkSize) - readSize))
            return false;
        
        return true;
    }
    
    // PCM Format Helpers

    bool IAudioFile::setPCMFormat(NumericType type, uint16_t bitDepth)
    {
        auto matchFormat = [&](NumericType t, uint16_t d, PCMFormat format)
        {
            if (!(type == t && bitDepth == d))
                return false;
                
            mPCMFormat = format;
            return true;
        };
        
        if (matchFormat(NumericType::Integer,  8, PCMFormat::Int8))    return true;
        if (matchFormat(NumericType::Integer, 16, PCMFormat::Int16))   return true;
        if (matchFormat(NumericType::Integer, 24, PCMFormat::Int24))   return true;
        if (matchFormat(NumericType::Integer, 32, PCMFormat::Int32))   return true;
        if (matchFormat(NumericType::Float,   32, PCMFormat::Float32)) return true;
        if (matchFormat(NumericType::Float,   64, PCMFormat::Float64)) return true;

        return false;
    }

    // AIFF Helpers
    
    bool IAudioFile::getAIFFChunkHeader(AIFFTag& enumeratedTag, uint32_t& chunkSize)
    {
        char tag[4];
        
        enumeratedTag = AIFFTag::Unknown;
        
        if (!readChunkHeader(tag, chunkSize))
            return false;
        
        if      (matchTag(tag, "FVER")) enumeratedTag = AIFFTag::Version;
        else if (matchTag(tag, "COMM")) enumeratedTag = AIFFTag::Common;
        else if (matchTag(tag, "SSND")) enumeratedTag = AIFFTag::Audio;
        
        return true;
    }
    
    IAudioFile::AIFCCompression IAudioFile::getAIFCCompression(const char* tag, uint16_t& bitDepth) const
    {
        if (matchTag(tag, "NONE"))
            return AIFCCompression::None;
        
        if (matchTag(tag, "twos"))
        {
            bitDepth = 16;
            return AIFCCompression::None;
        }
        
        if (matchTag(tag, "sowt"))
        {
            bitDepth = 16;
            return AIFCCompression::LittleEndian;
        }
        
        if (matchTag(tag, "fl32") || matchTag(tag, "FL32"))
        {
            bitDepth = 32;
            return AIFCCompression::Float;
        }
        
        if (matchTag(tag, "fl64") || matchTag(tag, "FL64"))
        {
            bitDepth = 32;
            return AIFCCompression::Float;
        }
         
        return AIFCCompression::Unknown;
    }
    
    //  Parse Headers

    BaseAudioFile::Error IAudioFile::parseHeader()
    {
        char chunk[12] = {}, fileType[4] = {}, fileSubtype[4] = {};

        // `Read file header

        if (!readInternal(chunk, 12))
            return Error::BadFormat;
            
        // Get file type and subtype

        strncpy(fileType, chunk, 4);
        strncpy(fileSubtype, chunk + 8, 4);

        // AIFF or AIFC

        if (matchTag(fileType, "FORM") && (matchTag(fileSubtype, "AIFF") || matchTag(fileSubtype, "AIFC")))
            return parseAIFFHeader(fileSubtype);

        // WAVE file format

        if ((matchTag(fileType, "RIFF") || matchTag(fileType, "RIFX")) && matchTag(fileSubtype, "WAVE"))
            return parseWaveHeader(fileType);

        // No known format found

        return Error::UnknownFormat;
    }

    BaseAudioFile::Error IAudioFile::parseAIFFHeader(const char* fileSubtype)
    {
        AIFFTag tag;
        
        uint32_t formatValid = static_cast<uint32_t>(AIFFTag::Common) | static_cast<uint32_t>(AIFFTag::Audio);
        uint32_t formatCheck = 0;
        char chunk[22];
        uint32_t chunkSize;
        
        mHeaderEndianness = Endianness::Big;
    
        // Iterate over chunks
        
        while (getAIFFChunkHeader(tag, chunkSize))
        {
            formatCheck |= static_cast<uint32_t>(tag);
            
            switch (tag)
            {
                case AIFFTag::Version:
                {
                    // Read format number and check for the correct version of the AIFC specification
                    
                    if (!readChunk(chunk, 4, chunkSize))
                        return Error::BadFormat;

                    if (getU32(chunk, getHeaderEndianness()) != AIFC_CURRENT_SPECIFICATION)
                        return Error::WrongAIFCVersion;
                    
                    break;
                }
                    
                case AIFFTag::Common:
                {
                    // Read common chunk (at least 18 bytes and up to 22)
                    
                    if (!readChunk(chunk, (chunkSize > 22) ? 22 : ((chunkSize < 18) ? 18 : chunkSize), chunkSize))
                        return Error::BadFormat;
                    
                    // Retrieve relevant data (AIFF or AIFC) and set AIFF
                    // defaults
                    
                    mNumChannels = getU16(chunk + 0, getHeaderEndianness());
                    mNumFrames = getU32(chunk + 2, getHeaderEndianness());
                    uint16_t bitDepth = getU16(chunk + 6, getHeaderEndianness());
                    mSamplingRate = extendedToDouble(chunk + 8);
                    
                    NumericType type = NumericType::Integer;
                    mAudioEndianness = Endianness::Big;
                    
                    // If there are no frames then it is not required for there
                    // to be an audio (SSND) chunk
                    
                    if (!getFrames())
                        formatCheck |= static_cast<uint32_t>(AIFFTag::Audio);
                    
                    if (matchTag(fileSubtype, "AIFC"))
                    {
                        mFileType = FileType::AIFC;
                        
                        // Require a version chunk
                        
                        formatValid |= static_cast<uint32_t>(AIFFTag::Version);
                        
                        // Set parameters based on format
                        
                        switch (getAIFCCompression(chunk + 18, bitDepth))
                        {
                            case AIFCCompression::None:
                                break;
                            case AIFCCompression::LittleEndian:
                                mAudioEndianness = Endianness::Little;
                                break;
                            case AIFCCompression::Float:
                                type = NumericType::Float;
                                break;
                            default:
                                return Error::UnsupportedAIFCFormat;
                        }
                    }
                    else
                        mFileType = FileType::AIFF;
                    
                    if (!setPCMFormat(type, bitDepth))
                        return Error::UnsupportedPCMFormat;
                    
                    break;
                }
                    
                case AIFFTag::Audio:
                {
                    if (!readChunk(chunk, 4, chunkSize))
                        return Error::BadFormat;
                    
                    // Audio data starts after a 32-bit block size value (ignored) plus an offset readh here

                    mPCMOffset = positionInternal() + 4 + getU32(chunk, getHeaderEndianness());
                    
                    break;
                }
                    
                case AIFFTag::Unknown:
                {
                    // Read no data, but update the file position
                    
                    if (!readChunk(nullptr, 0, chunkSize))
                        return Error::BadFormat;
                    
                    break;
                }

                default:
                    break;
            }
        }
        
        // Check that all relevant chunks were found
        
        if ((~formatCheck) & formatValid)
            return Error::BadFormat;
     
        return Error::None;
    }
    
    BaseAudioFile::Error IAudioFile::parseWaveHeader(const char* fileType)
    {
        char chunk[40];
        uint32_t chunkSize;
        
        // Check endianness
        
        mHeaderEndianness = matchTag(fileType, "RIFX") ? Endianness::Big : Endianness::Little;
        mAudioEndianness = getHeaderEndianness();
        
        // Search for the format chunk and read the format chunk as needed, checking for a valid size
        
        if (!(findChunk("fmt ", chunkSize)) || (chunkSize != 16 && chunkSize != 18 && chunkSize != 40))
            return Error::BadFormat;

        if (!readChunk(chunk, chunkSize, chunkSize))
            return Error::BadFormat;
                
        // Retrieve relevant data

        uint16_t formatByte = getU16(chunk, getHeaderEndianness());
        uint16_t bitDepth = getU16(chunk + 14, getHeaderEndianness());
        
        // WAVE_FORMAT_EXTENSIBLE
        
        if (formatByte == 0xFFFE)
        {
            formatByte = getU16(chunk + 24, getHeaderEndianness());
                
            constexpr unsigned char guid[14] = { 0x00,0x00,0x00,0x00,0x10,0x00,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71 };
       
            if (std::memcmp(chunk + 26, &guid, 14) != 0)
                return Error::UnsupportedWaveFormat;
        }

        // Check for a valid format byte (currently PCM or float only)

        if (formatByte != 0x0001 && formatByte != 0x0003)
            return Error::UnsupportedWaveFormat;

        NumericType type = formatByte == 0x0003 ? NumericType::Float : NumericType::Integer;
        mNumChannels = getU16(chunk + 2, getHeaderEndianness());
        mSamplingRate = getU32(chunk + 4, getHeaderEndianness());
        
        // Set PCM Format
        
        if (!setPCMFormat(type, bitDepth))
            return Error::UnsupportedPCMFormat;
        
        // Search for the data chunk and retrieve frame size and file offset to audio data
        
        if (!findChunk("data", chunkSize))
            return Error::BadFormat;
                
        mNumFrames = chunkSize / getFrameByteCount();
        mPCMOffset = positionInternal();
        mFileType = FileType::WAVE;

        return Error::None;
    }

    //  Internal Typed Audio Read

    template <class T>
    void IAudioFile::readAudio(T* output, uintptr_t numFrames, int32_t channel)
    {
        // Calculate sizes
        
        uint16_t numChannels = (channel < 0) ? getChannels() : 1;
        uint16_t channelStep = (channel < 0) ? 1 : getChannels();
        uint32_t byteDepth = getByteDepth();
        uintptr_t byteStep = byteDepth * channelStep;
        
        channel = std::max(channel, 0);
        
        while (numFrames)
        {
            uintptr_t loopFrames = numFrames > WORK_LOOP_SIZE ? WORK_LOOP_SIZE : numFrames;
            uintptr_t loopSamples = loopFrames * numChannels;
            uintptr_t j = channel * byteDepth;

            // Read raw

            readRaw(mBuffer, loopFrames);

            // Move to Output

            switch (getPCMFormat())
            {
                case PCMFormat::Int8:
                    if (getFileType() == FileType::WAVE)
                    {
                        for (uintptr_t i = 0; i < loopSamples; i++, j += byteStep)
                            u8ToOutput(output + i, *(reinterpret_cast<uint8_t *>(mBuffer + j)));
                    }
                    else
                    {
                        for (uintptr_t i = 0; i < loopSamples; i++, j += byteStep)
                            u32ToOutput(output + i, mBuffer[j] << 24);
                    }
                    break;

                case PCMFormat::Int16:
                    for (uintptr_t i = 0; i < loopSamples; i++, j += byteStep)
                        u32ToOutput(output + i, getU16(mBuffer + j, getAudioEndianness()) << 16);
                    break;

                case PCMFormat::Int24:
                    for (uintptr_t i = 0; i < loopSamples; i++, j += byteStep)
                        u32ToOutput(output + i, getU24(mBuffer + j, getAudioEndianness()) << 8);
                    break;

                case PCMFormat::Int32:
                    for (size_t i = 0; i < loopSamples; i++, j += byteStep)
                        u32ToOutput(output + i, getU32(mBuffer + j, getAudioEndianness()));
                    break;

                case PCMFormat::Float32:
                    for (size_t i = 0; i < loopSamples; i++, j += byteStep)
                        float32ToOutput(output + i, getU32(mBuffer + j, getAudioEndianness()));
                    break;

                case PCMFormat::Float64:
                    for (size_t i = 0; i < loopSamples; i++, j += byteStep)
                        float64ToOutput(output + i, getU64(mBuffer + j, getAudioEndianness()));
                    break;
                    
                default:
                    break;
            }
            
            numFrames -= loopFrames;
            output += loopSamples;
        }
    }
}
