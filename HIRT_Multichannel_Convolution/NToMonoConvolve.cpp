
#include "NToMonoConvolve.h"

#include <string.h>

HISSTools::NToMonoConvolve::NToMonoConvolve(uint32_t inChans, uintptr_t maxLength, LatencyMode latency) :  mNumInChans(inChans)
{
    for (uint32_t i = 0; i < mNumInChans; i++)
        mConvolvers.emplace_back(maxLength, latency);
}

template<typename Method, typename... Args>
t_convolve_error HISSTools::NToMonoConvolve::doChannel(Method method, uint32_t inChan, Args...args)
{
    if (inChan < mNumInChans)
        return (mConvolvers[inChan].*method)(args...);
    else
        return CONVOLVE_ERR_IN_CHAN_OUT_OF_RANGE;
}

t_convolve_error HISSTools::NToMonoConvolve::resize(uint32_t inChan, uintptr_t impulse_length)
{
    return doChannel(&MonoConvolve::resize, inChan, impulse_length);
}

t_convolve_error HISSTools::NToMonoConvolve::set(uint32_t inChan, const float *input, uintptr_t impulse_length, bool resize)
{
    return doChannel(&MonoConvolve::set, inChan, input, impulse_length, resize);
}

t_convolve_error HISSTools::NToMonoConvolve::reset(uint32_t inChan)
{
    return doChannel(&MonoConvolve::reset, inChan);
}

void HISSTools::NToMonoConvolve::process(const float **ins, float *out, float *temp, size_t numSamples, size_t activeInChans)
{
    // Zero output then convolve
    
    memset(out, 0, sizeof(float) * numSamples);
	
	for (uint32_t i = 0; i < mNumInChans && i < activeInChans ; i++)
		mConvolvers[i].process(ins[i], temp, out, numSamples);
}
