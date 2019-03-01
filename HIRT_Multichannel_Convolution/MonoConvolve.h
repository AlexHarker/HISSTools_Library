
#pragma once

#include "PartitionedConvolve.h"
#include "TimeDomainConvolve.h"
#include "convolve_errors.h"

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
    	typedef MemorySwap<PartitionedConvolve>::Ptr PartPtr;
    	
    public:
        
        MonoConvolve(uintptr_t maxLength, LatencyMode latency);

        void resize(uintptr_t length);
        t_convolve_error set(const float *input, uintptr_t length, bool requestResize);
        void reset();
        
        void process(const float *in, float *temp, float *out, uintptr_t numSamples);
        
    private:
    
        std::unique_ptr<TimeDomainConvolve> mTime1;
        std::unique_ptr<PartitionedConvolve> mPart1;
        std::unique_ptr<PartitionedConvolve> mPart2;
        std::unique_ptr<PartitionedConvolve> mPart3;
        
        MemorySwap<PartitionedConvolve> mPart4;
        
        uintptr_t mLength;
        LatencyMode mLatency;
    };
}
