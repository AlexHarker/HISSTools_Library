
#pragma once

#include "PartitionedConvolve.h"
#include "TimeDomainConvolve.h"
#include "ConvolveErrors.h"

#include "MemorySwap.h"
#include <memory>

#include <stdint.h>

enum LatencyMode
{
    kLatencyZero,
    kLatencyShort,
    kLatencyMedium,
} ;

namespace HISSTools
{
    class MonoConvolve
    {
    	
    public:
        
        MonoConvolve(uintptr_t maxLength, LatencyMode latency);
        MonoConvolve(uintptr_t maxLength, bool zeroLatency, uint32_t A, uint32_t B = 0, uint32_t C = 0, uint32_t D = 0);
        
        MonoConvolve(MonoConvolve& obj) = delete;
        MonoConvolve& operator = (MonoConvolve& obj) = delete;
        
        MonoConvolve(MonoConvolve&& obj)
        : mAllocator(obj.mAllocator),
          mTime1(std::move(obj.mTime1)),
          mPart1(std::move(obj.mPart1)),
          mPart2(std::move(obj.mPart2)),
          mPart3(std::move(obj.mPart3)),
          mPart4(std::move(obj.mPart4)),
          mLength(obj.mLength),
          mReset(true)
        {}
        
        MonoConvolve& operator = (MonoConvolve&& obj)
        {
            mAllocator = obj.mAllocator;
            mTime1 = std::move(obj.mTime1);
            mPart1 = std::move(obj.mPart1);
            mPart2 = std::move(obj.mPart2);
            mPart3 = std::move(obj.mPart3);
            mPart4 = std::move(obj.mPart4);
            mLength = obj.mLength;
            mReset = true;
            
            return *this;
        }

        void setResetOffset(intptr_t offset = -1);

        ConvolveError resize(uintptr_t length);
        ConvolveError set(const float *input, uintptr_t length, bool requestResize);
        ConvolveError reset();
        
        void process(const float *in, float *temp, float *out, uintptr_t numSamples);
        
    private:

        void createPartitions(uintptr_t maxLength, bool zeroLatency, uint32_t A, uint32_t B = 0, uint32_t C = 0, uint32_t D = 0);
        
        MemorySwap<PartitionedConvolve>::AllocFunc mAllocator;

        std::unique_ptr<TimeDomainConvolve> mTime1;
        std::unique_ptr<PartitionedConvolve> mPart1;
        std::unique_ptr<PartitionedConvolve> mPart2;
        std::unique_ptr<PartitionedConvolve> mPart3;
        
        MemorySwap<PartitionedConvolve> mPart4;
        
        uintptr_t mLength;
        
        bool mReset;
    };
}
