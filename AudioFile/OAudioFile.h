#ifndef _HISSTOOLS_OAUDIOFILE_
#define _HISSTOOLS_OAUDIOFILE_

#include "BaseAudioFile.hpp"

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
        
        void seek(uintptr_t position = 0);
        uintptr_t getPosition();

        void writeInterleaved(const double* input, uintptr_t numFrames) { writeAudio(input, numFrames); }
        void writeInterleaved(const float* input, uintptr_t numFrames)  { writeAudio(input, numFrames); }
        
        void writeChannel(const double* input, uintptr_t numFrames, uint16_t channel)   { writeAudio(input, numFrames, channel); }
        void writeChannel(const float* input, uintptr_t numFrames, uint16_t channel)    { writeAudio(input, numFrames, channel); }
        void writeRaw(const char *input, uintptr_t numFrames)                           { writePCMData(input, numFrames); }
            
    protected:
        
        uintptr_t getHeaderSize() const { return getPCMOffset() - 8; }

    private:
        
        uintptr_t positionInternal();
        bool seekInternal(uintptr_t position);
        bool seekRelativeInternal(uintptr_t offset);
        bool writeInternal(const char* buffer, uintptr_t bytes);
        
        bool putU64(uint64_t value, Endianness endianness);
        bool putU32(uint32_t value, Endianness endianness);
        bool putU24(uint32_t value, Endianness endianness);
        bool putU16(uint32_t value, Endianness endianness);
        bool putU08(uint32_t value);

        void rawU64(uint64_t value, Endianness endianness, unsigned char* bytes);
        void rawU32(uint32_t value, Endianness endianness, unsigned char* bytes);
        void rawU24(uint32_t value, Endianness endianness, unsigned char* bytes);
        void rawU16(uint32_t value, Endianness endianness, unsigned char* bytes);
        void rawU08(uint32_t value, unsigned char* bytes);

        template <class T, int N>
        bool putBytes(T value, Endianness e);
        
        bool putPadByte();

        bool putExtended(double);
        bool putPString(const char* string);

        bool putTag(const char* tag);
        bool putChunk(const char* tag, uint32_t size);

        void writeWaveHeader();
        void writeAIFCHeader();
        
        bool resize(uintptr_t numFrames);
        bool updateHeader();
        
        template <class T, class U, int N, class V>
        void writeLoop(const V* input, uintptr_t j, uintptr_t loopSamples, uintptr_t byteStep);
        
        template <class T>
        void writeAudio(const T* input, uintptr_t numFrames, int32_t channel = -1);
        
        bool writePCMData(const char* input, uintptr_t numFrames);

        const char *getCompressionTag() const;
        const char *getCompressionString() const;
    };
}

#endif /* _HISSTOOLS_OAUDIOFILE_ */
