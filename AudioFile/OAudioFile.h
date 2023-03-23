#ifndef _HISSTOOLS_OAUDIOFILE_
#define _HISSTOOLS_OAUDIOFILE_

#include "AudioFileUtilities.hpp"
#include "BaseAudioFile.hpp"

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
        
        bool putU32(uint32_t value, Endianness e) { return putBytes<uint32_t, 4>(value, e); }
        bool putU16(uint16_t value, Endianness e) { return putBytes<uint16_t, 2>(value, e); }

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
        
        void writeWaveHeader();
        void writeAIFCHeader();
        
        bool updateHeader();

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
        
        template <class T, class U, int N, class V>
        void writeLoop(const V* input, uintptr_t j, uintptr_t loopSamples, uintptr_t byteStep);
        
        template <class T>
        void writeAudio(const T* input, uintptr_t numFrames, int32_t channel = -1);
    };
}

#endif /* _HISSTOOLS_OAUDIOFILE_ */
