#include "IAudioFile.h"

#include <cmath>
#include <cassert>
#include <cstring>
#include <vector>
#include <algorithm>

#include "AudioFileUtilities.hpp"

namespace HISSTools
{
    // File Opening / Close
    
    void IAudioFile::open(const std::string& file)
    {
        close();
        
        if (!file.empty())
        {
            mFile.open(file.c_str(), std::ios_base::binary);
            if (mFile.is_open())
            {
                setErrorBit(parseHeader());
                mBuffer.resize(WORK_LOOP_SIZE * getFrameByteCount());
                seek();
            }
            else
                setErrorBit(Error::CouldNotOpen);
        }
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
    
    uint64_t IAudioFile::getU64(const char* b, Endianness e) { return getBytes<uint64_t, 8>(b, e); }
    uint32_t IAudioFile::getU32(const char* b, Endianness e) { return getBytes<uint32_t, 4>(b, e); }
    uint32_t IAudioFile::getU24(const char* b, Endianness e) { return getBytes<uint32_t, 3>(b, e); }
    uint32_t IAudioFile::getU16(const char* b, Endianness e) { return getBytes<uint32_t, 2>(b, e); }
    
    // Conversion
    
    double IAudioFile::extendedToDouble(const char* bytes) const
    {
        // Get double from big-endian IEEE 80-bit extended floating point format
        
        bool     sign       = getU16(bytes + 0, Endianness::Big) & 0x8000;
        int32_t  exponent   = getU16(bytes + 0, Endianness::Big) & 0x777F;
        uint32_t hiMantissa = getU32(bytes + 2, Endianness::Big);
        uint32_t loMantissa = getU32(bytes + 6, Endianness::Big);
  
        // Special handlers for zeros
        
        if (!exponent && !hiMantissa && !loMantissa)
            return 0.0;
        
        // Nans and Infs
        
        if (exponent == 0x777F)
            return HUGE_VAL;
        
        exponent -= 0x3FFF;
        double value = std::ldexp(hiMantissa, exponent - 31) + std::ldexp(loMantissa, exponent - (31 + 32));
                
        return sign ? -value : value;
    }
    
     //  Chunk Reading
    
    bool IAudioFile::matchTag(const char* a, const char* b) { return (strncmp(a, b, 4) == 0); }

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
                    
                    // Retrieve relevant data (AIFF or AIFC) and set AIFF defaults
                    
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
                        
                        //formatValid |= static_cast<uint32_t>(AIFFTag::Version);
                        
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
    
    template <class T, class U, class V>
    U convert(V value, T, U)
    {
        return static_cast<U>(*reinterpret_cast<T*>(&value));
    }

    template <class T>
    T convert(uint8_t value, uint8_t, T)
    {
        return (static_cast<T>(value) - T(128)) / T(128);
    }
    
    template <class T>
    T convert(uint32_t value, uint32_t, T)
    {
        // N.B. the result of the shift is a negative int32_t value, hence the negation
        
        return convert<int32_t, T>(value, uint32_t(0), T(0)) * (T(-1.0) / static_cast<T>(int32_t(1) << int32_t(31)));
    }
    
    template <class T, class U, int N, int Shift, class V>
    void IAudioFile::readLoop(V* output, uintptr_t j, uintptr_t loopSamples, uintptr_t byteStep)
    {
        Endianness endianness = getAudioEndianness();
        
        for (uintptr_t i = 0; i < loopSamples; i++, j += byteStep)
            output[i] = convert(getBytes<U, N>(mBuffer.data() + j, endianness) << Shift, T(0), V(0));
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
            uintptr_t loopFrames = std::min(numFrames, WORK_LOOP_SIZE);
            uintptr_t loopSamples = loopFrames * numChannels;
            uintptr_t j = channel * byteDepth;

            // Read raw

            readRaw(mBuffer.data(), loopFrames);

            // Copy and convert to Output

            switch (getPCMFormat())
            {
                case PCMFormat::Int8:
                    if (getFileType() == FileType::WAVE)
                        readLoop<uint8_t, uint8_t, 1, 0>(output, j, loopSamples, byteStep);
                    else
                        readLoop<uint32_t, uint32_t, 1, 24>(output, j, loopSamples, byteStep);
                    break;

                case PCMFormat::Int16:      readLoop<uint32_t, uint32_t, 2, 16>(output, j, loopSamples, byteStep);      break;
                case PCMFormat::Int24:      readLoop<uint32_t, uint32_t, 3,  8>(output, j, loopSamples, byteStep);      break;
                case PCMFormat::Int32:      readLoop<uint32_t, uint32_t, 4,  0>(output, j, loopSamples, byteStep);      break;
                case PCMFormat::Float32:    readLoop<float, uint32_t, 4,  0>(output, j, loopSamples, byteStep);         break;
                case PCMFormat::Float64:    readLoop<double, uint64_t, 8,  0>(output, j, loopSamples, byteStep);        break;

                default:
                    break;
            }
            
            numFrames -= loopFrames;
            output += loopSamples;
        }
    }
}
