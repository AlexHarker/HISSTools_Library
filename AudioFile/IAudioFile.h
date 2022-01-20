#ifndef _HISSTOOLS_IAUDIOFILE_
#define _HISSTOOLS_IAUDIOFILE_

#include "BaseAudioFile.hpp"
#include <fstream>

namespace HISSTools
{
    // FIX - check types, errors and returns
    
    class IAudioFile : public BaseAudioFile
    {
        enum class AIFFTag          { Unknown = 0x0, Version = 0x1, Common = 0x2, Audio = 0x4 };
        enum class AIFCCompression  { Unknown, None, LittleEndian, Float };

    public:
        
        // Constructor and Destructor
        
        IAudioFile(const std::string& file) : mBuffer(nullptr) { open(file); }
        ~IAudioFile() { close(); }
        
        // File Open / Close
        
        void open(const std::string& file);
        void close();
        bool isOpen();
        
        // File Position
        
        void seek(uintptr_t position = 0);
        uintptr_t getPosition();

        // File Reading
        
        void readRaw(void* output, uintptr_t numFrames);

        void readInterleaved(double* output, uintptr_t numFrames) { readAudio(output, numFrames); }
        void readInterleaved(float* output, uintptr_t numFrames)  { readAudio(output, numFrames); }

        void readChannel(double* output, uintptr_t numFrames, uint16_t channel) { readAudio(output, numFrames, channel); }
        void readChannel(float* output, uintptr_t numFrames, uint16_t channel)  { readAudio(output, numFrames, channel); }

    private:
        
        //  Internal File Handling
        
        bool readInternal(char* buffer, uintptr_t numBytes);
        bool seekInternal(uintptr_t position);
        bool advanceInternal(uintptr_t offset);
        uintptr_t positionInternal();

        //  Extracting Single Values
        
        uint64_t getU64(const char* bytes, Endianness fileEndianness) const;
        uint32_t getU32(const char* bytes, Endianness fileEndianness) const;
        uint32_t getU24(const char* bytes, Endianness fileEndianness) const;
        uint32_t getU16(const char* bytes, Endianness fileEndianness) const;

        //  Conversion
        
        double extendedToDouble(const char* bytes) const;
        template <class T> void u32ToOutput(T* output, uint32_t value);
        template <class T> void u8ToOutput(T* output, uint8_t value);
        template <class T> void float32ToOutput(T* output, uint32_t value);
        template <class T> void float64ToOutput(T* output, uint64_t value);

        //  Chunk Reading
        
        static bool matchTag(const char* a, const char* b);
        bool readChunkHeader(char* tag, uint32_t& chunkSize);
        bool findChunk(const char* searchTag, uint32_t& chunkSize);
        bool readChunk(char* data, uint32_t readSize, uint32_t chunkSize);

        //  PCM Format Helper
        
        bool setPCMFormat(NumericType type, uint16_t bitDepth);

        //  AIFF Helpers
        
        bool getAIFFChunkHeader(AIFFTag& enumeratedTag, uint32_t& chunkSize);
        AIFCCompression getAIFCCompression(const char* tag) const;

        //  Parse Headers
        
        Error parseHeader();
        Error parseAIFFHeader(const char* fileSubtype);
        Error parseWaveHeader(const char* fileType);

        //  Internal Typed Audio Read
        
        template <class T>
        void readAudio(T* output, uintptr_t numFrames, int32_t channel = -1);
         
        //  Data
        
        std::ifstream mFile;
        char *mBuffer;
    };
}

#endif
