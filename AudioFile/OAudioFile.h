#ifndef _HISSTOOLS_OAUDIOFILE_
#define _HISSTOOLS_OAUDIOFILE_

#include "BaseAudioFile.hpp"
#include <fstream>

namespace HISSTools
{
    class OAudioFile : public BaseAudioFile
    {
    public:
        OAudioFile();
        OAudioFile(const std::string&, FileType, PCMFormat, uint16_t channels, double sr);
        OAudioFile(const std::string&, FileType, PCMFormat, uint16_t channels, double sr, Endianness);
        
        ~OAudioFile() { close(); }

        void open(const std::string&, FileType, PCMFormat, uint16_t channels, double sr);
        void open(const std::string&, FileType, PCMFormat, uint16_t channels, double sr, Endianness);
        
        bool isOpen() const { return mFile.is_open(); }
        void close();
        void seek(uintptr_t position = 0);
        uintptr_t getPosition();

        void writeInterleaved(const double* input, uintptr_t numFrames) { writeAudio(input, numFrames); }
        void writeInterleaved(const float* input, uintptr_t numFrames)  { writeAudio(input, numFrames); }
        
        void writeChannel(const double* input, uintptr_t numFrames, uint16_t channel)   { writeAudio(input, numFrames, channel); }
        void writeChannel(const float* input, uintptr_t numFrames, uint16_t channel)    {  writeAudio(input, numFrames, channel); }
        void writeRaw(const char *input, uintptr_t numFrames)                           { writePCMData(input, numFrames); }
            
    protected:
        
        uintptr_t getHeaderSize() const { return getPCMOffset() - 8; }

    private:
        
        uintptr_t positionInternal();
        bool seekInternal(uintptr_t position);
        bool seekRelativeInternal(uintptr_t offset);
        bool writeInternal(const char* buffer, uintptr_t bytes);
        
        bool putU64(uint64_t value, Endianness fileEndianness);
        bool putU32(uint32_t value, Endianness fileEndianness);
        bool putU24(uint32_t value, Endianness fileEndianness);
        bool putU16(uint32_t value, Endianness fileEndianness);
        bool putU08(uint32_t value);
        
        bool putPadByte();

        bool putExtended(double);
        bool putPString(const char* string);

        bool putTag(const char* tag);
        bool putChunk(const char* tag, uint32_t size);

        void writeWaveHeader();
        void writeAIFCHeader();

        uint32_t inputToU32(double input, int32_t bitDepth);
        uint8_t inputToU8(double input);
        
        bool resize(uintptr_t numFrames);
        bool updateHeader();
        template <class T> void writeAudio(const T* input, uintptr_t numFrames, int32_t channel = -1);
        bool writePCMData(const char* input, uintptr_t numFrames);

        const char *getCompressionTag() const;
        const char *getCompressionString() const;
        
        //  Data

        std::ofstream mFile;
    };
}

#endif /* _HISSTOOLS_OAUDIOFILE_ */
