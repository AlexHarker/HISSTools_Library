#include "IAudioFile.h"

#include <cmath>
#include <cstring>

namespace HISSTools
{
    // Parse Headers

    BaseAudioFile::Error IAudioFile::parseAIFFHeader(const char* fileSubtype)
    {
        AIFFTag tag;
        
        uint32_t formatValid = static_cast<uint32_t>(AIFFTag::Common) | static_cast<uint32_t>(AIFFTag::Audio);
        uint32_t formatCheck = 0;
        char chunk[22];
        uint32_t chunkSize;
        
        mFormat = AudioFileFormat(FileType::AIFF);
        
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
                    mSamplingRate = IEEEDoubleExtendedConvertor()(chunk + 8);
                                        
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
}
