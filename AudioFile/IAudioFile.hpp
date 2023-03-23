#ifndef _HISSTOOLS_IAUDIOFILE_
#define _HISSTOOLS_IAUDIOFILE_

#include "AudioFileUtilities.hpp"
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

        struct PostionRestore
        {
            PostionRestore(IAudioFile &parent)
            : mParent(parent)
            , mPosition(parent.positionInternal())
            {
                mParent.mFile.seekg(12, ios_base::beg);
            }
            
            ~PostionRestore()
            {
                mParent.seekInternal(mPosition);
            }
                
            IAudioFile &mParent;
            uintptr_t mPosition;
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
                mFile.open(file.c_str(), ios_base::binary);
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
        
        void seek(uintptr_t position = 0)
        {
            seekInternal(getPCMOffset() + getFrameByteCount() * position);
        }
        
        uintptr_t getPosition()
        {
            if (getPCMOffset())
                return static_cast<uintptr_t>((positionInternal() - getPCMOffset()) / getFrameByteCount());
            
            return 0;
        }

        // File Reading
        
        void readRaw(void* output, uintptr_t numFrames)
        {
            readInternal(static_cast<char *>(output), getFrameByteCount() * numFrames);
        }

        void readInterleaved(double* output, uintptr_t numFrames)
        {
            readAudio(output, numFrames);
        }
        
        void readInterleaved(float* output, uintptr_t numFrames)
        {
            readAudio(output, numFrames);
        }

        void readChannel(double* output, uintptr_t numFrames, uint16_t channel)
        {
            readAudio(output, numFrames, channel);
        }
        
        void readChannel(float* output, uintptr_t numFrames, uint16_t channel)
        {
            readAudio(output, numFrames, channel);
        }

        // Chunks
        
        std::vector<std::string> getChunkTags()
        {
            PostionRestore restore(*this);
            
            char tag[5] = {0, 0, 0, 0, 0};
            uint32_t chunkSize;
            
            std::vector<std::string> tags;
            
            // Iterate through chunks
            
            while (readChunkHeader(tag, chunkSize))
            {
                tags.push_back(tag);
                readChunk(nullptr, 0, chunkSize);
            }
            
            return tags;
        }
        
        uintptr_t getChunkSize(const char *tag)
        {
            PostionRestore restore(*this);
            
            uint32_t chunkSize = 0;
            
            if (strlen(tag) <= 4)
                findChunk(tag, chunkSize);
            
            return chunkSize;
        }
        
        void readRawChunk(void *output, const char *tag)
        {
            PostionRestore restore(*this);
            
            uint32_t chunkSize = 0;
            
            if (strlen(tag) <= 4)
            {
                findChunk(tag, chunkSize);
                readInternal(static_cast<char *>(output), chunkSize);
            }
        }
        
    private:
        
        // Internal File Handling
        
        bool readInternal(char* buffer, uintptr_t numBytes)
        {
            mFile.clear();
            mFile.read(buffer, numBytes);
            return static_cast<uintptr_t>(mFile.gcount()) == numBytes;
        }
        
        bool seekInternal(uintptr_t position)
        {
            mFile.clear();
            mFile.seekg(position, ios_base::beg);
            return positionInternal() == position;
        }
        
        bool advanceInternal(uintptr_t offset)
        {
            return seekInternal(positionInternal() + offset);
        }
        
        uintptr_t positionInternal()
        {
            return mFile.tellg();
        }
        
        // Getters
        
        uint32_t getU32(const char* bytes, Endianness endianness)
        {
            return getBytes<uint32_t, 4>(bytes, endianness);
        }
        
        uint16_t getU16(const char* bytes, Endianness endianness)
        {
            return getBytes<uint16_t, 2>(bytes, endianness);
        }
        
        // Chunk Reading
        
        static bool matchTag(const char* a, const char* b)
        {
            return (strncmp(a, b, 4) == 0);
        }
        
        bool readChunkHeader(char* tag, uint32_t& chunkSize)
        {
            char header[8] = {};
            
            if (!readInternal(header, 8))
                return false;
            
            strncpy(tag, header, 4);
            chunkSize = getU32(header + 4, getHeaderEndianness());
            
            return true;
        }
        
        bool findChunk(const char* searchTag, uint32_t& chunkSize)
        {
            char tag[4] = {};
            
            while (readChunkHeader(tag, chunkSize))
            {
                if (matchTag(tag, searchTag))
                    return true;
                
                if (!advanceInternal(paddedLength(chunkSize)))
                    break;
            }
            
            return false;
        }
        
        bool readChunk(char* data, uint32_t readSize, uint32_t chunkSize)
        {
            if (readSize && (readSize > chunkSize || !readInternal(data, readSize)))
                return false;
            
            if (!advanceInternal(paddedLength(chunkSize) - readSize))
                return false;
            
            return true;
        }

        // AIFF Helper
        
        bool getAIFFChunkHeader(AIFFTag& enumeratedTag, uint32_t& chunkSize)
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
        
        // Parse Headers
        
        Error parseHeader()
        {
            char chunk[12] = {};
            
            const char *fileType = chunk;
            const char *fileSubtype = chunk + 8;
            
            // `Read file header
            
            if (!readInternal(chunk, 12))
                return Error::BadFormat;
            
            // AIFF or AIFC
            
            if (matchTag(fileType, "FORM") && (matchTag(fileSubtype, "AIFF") || matchTag(fileSubtype, "AIFC")))
                return parseAIFFHeader(fileSubtype);
            
            // WAVE file format
            
            if ((matchTag(fileType, "RIFF") || matchTag(fileType, "RIFX")) && matchTag(fileSubtype, "WAVE"))
                return parseWaveHeader(fileType);
            
            // No known format found
            
            return Error::UnknownFormat;
        }
        
        Error parseAIFFHeader(const char* fileSubtype)
        {
            AIFFTag tag;
            
            char chunk[22];
            
            uint32_t formatValid = static_cast<uint32_t>(AIFFTag::Common) | static_cast<uint32_t>(AIFFTag::Audio);
            uint32_t formatCheck = 0;
            uint32_t chunkSize;
            
            mFormat = AudioFileFormat(FileType::AIFF);
            
            // Iterate over chunks
            
            while (getAIFFChunkHeader(tag, chunkSize))
            {
                formatCheck |= static_cast<uint32_t>(tag);
                
                switch (tag)
                {
                    case AIFFTag::Common:
                    {
                        // Read common chunk (at least 18 bytes and up to 22)
                        
                        if (!readChunk(chunk, (chunkSize > 22) ? 22 : ((chunkSize < 18) ? 18 : chunkSize), chunkSize))
                            return Error::BadFormat;
                        
                        // Retrieve relevant data (AIFF or AIFC) and set AIFF defaults
                        
                        mNumChannels = getU16(chunk + 0, getHeaderEndianness());
                        mNumFrames = getU32(chunk + 2, getHeaderEndianness());
                        mSamplingRate = IEEEDoubleExtendedConvertor()(chunk + 8);
                        
                        uint16_t bitDepth = getU16(chunk + 6, getHeaderEndianness());
                        
                        // If there are no frames then it is not required for there to be an audio (SSND) chunk
                        
                        if (!getFrames())
                            formatCheck |= static_cast<uint32_t>(AIFFTag::Audio);
                        
                        if (matchTag(fileSubtype, "AIFC"))
                        {
                            // Require a version chunk
                            
                            //formatValid |= static_cast<uint32_t>(AIFFTag::Version);
                            
                            // Set parameters based on format
                            
                            mFormat = AIFCCompression::getFormat(chunk + 18, bitDepth);
                            
                            if (getFileType() == FileType::None)
                                return Error::UnsupportedAIFCFormat;
                        }
                        else
                            mFormat = AudioFileFormat(FileType::AIFF, NumericType::Integer, bitDepth, Endianness::Big);
                        
                        if (!mFormat.isValid())
                            return Error::UnsupportedPCMFormat;
                        
                        break;
                    }
                        
                    case AIFFTag::Version:
                    {
                        // Read format number and check for the correct version of the AIFC specification
                        
                        if (!readChunk(chunk, 4, chunkSize))
                            return Error::BadFormat;
                        
                        if (getU32(chunk, getHeaderEndianness()) != AIFC_CURRENT_SPECIFICATION)
                            return Error::WrongAIFCVersion;
                        
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
        
        Error parseWaveHeader(const char* fileType)
        {
            char chunk[40];
            
            uint32_t chunkSize;
            
            mFormat = AudioFileFormat(FileType::WAVE);
            
            // Check endianness
            
            Endianness endianness = matchTag(fileType, "RIFX") ? Endianness::Big : Endianness::Little;
            
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
            
            // Search for the data chunk and retrieve frame size and file offset to audio data
            
            if (!findChunk("data", chunkSize))
                return Error::BadFormat;
            
            // Set Format
            
            mFormat = AudioFileFormat(FileType::WAVE, type, bitDepth, endianness);
            
            if (!mFormat.isValid())
                return Error::UnsupportedPCMFormat;
            
            mNumFrames = chunkSize / getFrameByteCount();
            mPCMOffset = positionInternal();
            
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
        void readLoop(V* output, uintptr_t j, uintptr_t loopSamples, uintptr_t byteStep)
        {
            Endianness endianness = getAudioEndianness();
            
            for (uintptr_t i = 0; i < loopSamples; i++, j += byteStep)
                output[i] = convert(getBytes<U, N>(mBuffer.data() + j, endianness) << Shift, T(0), V(0));
        }
        
        template <class T>
        void readAudio(T* output, uintptr_t numFrames, int32_t channel = -1)
        {
            // Calculate sizes
            
            const uint32_t byteDepth = getByteDepth();
            const uint16_t numChannels = (channel < 0) ? getChannels() : 1;
            const uintptr_t byteStep = byteDepth * ((channel < 0) ? 1 : numChannels);
            const uintptr_t j = std::max(channel, 0) * byteDepth;
            
            while (numFrames)
            {
                const uintptr_t loopFrames = std::min(numFrames, WORK_LOOP_SIZE);
                const uintptr_t loopSamples = loopFrames * numChannels;
                
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
    };
}

#endif
