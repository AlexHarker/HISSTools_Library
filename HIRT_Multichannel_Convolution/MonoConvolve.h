
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
        MonoConvolve(MonoConvolve& obj) = delete;
        MonoConvolve& operator = (MonoConvolve& obj) = delete;
        
        MonoConvolve(MonoConvolve&& obj)
        : mTime1(std::move(obj.mTime1)),
          mPart1(std::move(obj.mPart1)),
          mPart2(std::move(obj.mPart2)),
          mPart3(std::move(obj.mPart3)),
          mPart4(std::move(obj.mPart4)),
          mLength(obj.mLength),
          mLatency(obj.mLatency),
          mReset(true)
        {}
        
        MonoConvolve& operator = (MonoConvolve&& obj)
        {
            mTime1 = std::move(obj.mTime1);
            mPart1 = std::move(obj.mPart1);
            mPart2 = std::move(obj.mPart2);
            mPart3 = std::move(obj.mPart3);
            mPart4 = std::move(obj.mPart4);
            mLength = obj.mLength;
            mLatency = obj.mLatency;
            mReset = true;
            
            return *this;
        }

        void setResetOffset(intptr_t offset = -1);

        ConvolveError resize(uintptr_t length);
        ConvolveError set(const float *input, uintptr_t length, bool requestResize);
        ConvolveError reset();
        
        void process(const float *in, float *temp, float *out, uintptr_t numSamples);
        
    private:
    
        std::unique_ptr<TimeDomainConvolve> mTime1;
        std::unique_ptr<PartitionedConvolve> mPart1;
        std::unique_ptr<PartitionedConvolve> mPart2;
        std::unique_ptr<PartitionedConvolve> mPart3;
        
        MemorySwap<PartitionedConvolve> mPart4;
        
        uintptr_t mLength;
        LatencyMode mLatency;
        
        intptr_t mResetOffset;
        bool mReset;
    };
}
