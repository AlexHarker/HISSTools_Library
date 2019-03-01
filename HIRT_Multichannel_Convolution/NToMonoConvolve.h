
#pragma once
	
#include "MonoConvolve.h"
#include "convolve_errors.h"

#include <stdint.h>
#include <vector>

namespace HISSTools
{
    class NToMonoConvolve
    {
        
    public:
        
        NToMonoConvolve(uint32_t input_chans, uintptr_t maxLength, LatencyMode latency);
        
        t_convolve_error resize(uint32_t inChan, uintptr_t impulse_length);
        t_convolve_error set(uint32_t inChan, const float *input, uintptr_t impulse_length, bool resize);
        t_convolve_error reset(uint32_t inChan);
        
        void process(const float **ins, float *out, float *temp, size_t numSamples, size_t active_in_chans);
        
    private:
        
        template<typename Method, typename... Args>
        t_convolve_error doChannel(Method method, uint32_t inChan, Args...args);
        
        std::vector<MonoConvolve> mConvolvers;
        
        uint32_t mNumInChans;
    };
}
