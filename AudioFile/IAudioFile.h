#ifndef _HISSTOOLS_IAUDIOFILE_
#define _HISSTOOLS_IAUDIOFILE_

#include "BaseAudioFile.hpp"

namespace HISSTools
{
    // FIX - check types, errors and returns
    
    class IAudioFile : public BaseAudioFile
    {
        enum class AIFFTag          { Unknown = 0x0, Version = 0x1, Common = 0x2, Audio = 0x4 };
        enum class AIFCCompression  { Unknown, None, LittleEndian, Float };

        struct PostionRestore
        {
            PostionRestore(IAudioFile &parent)
            : mParent(parent)
            , mPosition(parent.positionInternal())
            {
                mParent.mFile.seekg(12, std::ios_base::beg);
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
        
        IAudioFile(const std::string& file) { open(file); }
        ~IAudioFile() { close(); }
        
        // File Open
        
        void open(const std::string& file);
        
        // File Position
        
        void seek(uintptr_t position = 0);
        uintptr_t getPosition();

        // File Reading
        
        void readRaw(void* output, uintptr_t numFrames);

        void readInterleaved(double* output, uintptr_t numFrames) { readAudio(output, numFrames); }
        void readInterleaved(float* output, uintptr_t numFrames)  { readAudio(output, numFrames); }

        void readChannel(double* output, uintptr_t numFrames, uint16_t channel) { readAudio(output, numFrames, channel); }
        void readChannel(float* output, uintptr_t numFrames, uint16_t channel)  { readAudio(output, numFrames, channel); }

        // Chunks
        
        std::vector<std::string> getChunkTags();
        uintptr_t getChunkSize(const char *tag);
        void readRawChunk(void *output, const char *tag);
        
    private:
        
        //  Internal File Handling
        
        bool readInternal(char* buffer, uintptr_t numBytes);
        bool seekInternal(uintptr_t position);
        bool advanceInternal(uintptr_t offset);
        uintptr_t positionInternal();

        //  Extracting Single Values
        
        static uint64_t getU64(const char* bytes, Endianness endianness);
        static uint32_t getU32(const char* bytes, Endianness endianness);
        static uint32_t getU24(const char* bytes, Endianness endianness);
        static uint32_t getU16(const char* bytes, Endianness endianness);

        //  Conversion
        
        double extendedToDouble(const char* bytes) const;
        
        //  Chunk Reading
        
        static bool matchTag(const char* a, const char* b);
        bool readChunkHeader(char* tag, uint32_t& chunkSize);
        bool findChunk(const char* searchTag, uint32_t& chunkSize);
        bool readChunk(char* data, uint32_t readSize, uint32_t chunkSize);

        //  PCM Format Helper
        
        bool setPCMFormat(NumericType type, uint16_t bitDepth);

        //  AIFF Helpers
        
        bool getAIFFChunkHeader(AIFFTag& enumeratedTag, uint32_t& chunkSize);
        AIFCCompression getAIFCCompression(const char* tag, uint16_t& bitDepth) const;

        //  Parse Headers
        
        Error parseHeader();
        Error parseAIFFHeader(const char* fileSubtype);
        Error parseWaveHeader(const char* fileType);

        //  Internal Typed Audio Read

        template <class T, class U, int N, int Shift, class V>
        void readLoop(V* output, uintptr_t j, uintptr_t loopSamples, uintptr_t byteStep);
        
        template <class T>
        void readAudio(T* output, uintptr_t numFrames, int32_t channel = -1);
    };
}

#endif
