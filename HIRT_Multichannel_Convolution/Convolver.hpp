
#pragma once

#include "MemorySwap.h"
#include "NToMonoConvolve.hpp"
#include "ConvolveSIMD.h"
#include "ConvolveErrors.h"

#include <cstdint>
#include <vector>

class Convolver
{
    struct SIMDSettings
    {
        SIMDSettings()
        {
#if defined(__i386__) || defined(__x86_64__)
            mOldMXCSR = _mm_getcsr();
            _mm_setcsr(mOldMXCSR | 0x8040);
#endif
        }
        
        ~SIMDSettings()
        {
#if defined(__i386__) || defined(__x86_64__)
            _mm_setcsr(mOldMXCSR);
#endif
        }
        
        unsigned int mOldMXCSR;
    };
    
public:
    
    Convolver(uint32_t numIns, uint32_t numOuts, LatencyMode latency)
    : mTemporaryMemory(0)
    {
        numIns = numIns < 1 ? 1 : numIns;
        
        mN2M = true;
        mNumIns = numIns;
        mNumOuts = numOuts;
        
        for (uint32_t i = 0; i < numIns; i++)
            mInTemps.push_back(nullptr);
        
        for (uint32_t i = 0; i < numOuts; i++)
            mConvolvers.push_back(new NToMonoConvolve(numIns, 16384, latency));
        
        mTemp1 = nullptr;
        mTemp2 = nullptr;
    }
    
    Convolver(uint32_t numIO, LatencyMode latency);
    : mTemporaryMemory(0)
    {
        numIO = numIO < 1 ? 1 : numIO;
        
        mN2M = false;
        mNumIns = numIO;
        mNumOuts = numIO;
        
        for (uint32_t i = 0; i < numIO; i++)
        {
            mConvolvers.push_back(new NToMonoConvolve(1, 16384, latency));
            mInTemps.push_back(nullptr);
        }
        
        mTemp1 = nullptr;
        mTemp2 = nullptr;
    }
    
    virtual ~Convolver() throw()
    {
        for (uint32_t i = 0; i < mNumOuts; i++)
            delete mConvolvers[i];
    }
    
    // Clear IRs
    
    void clear(bool resize)
    {
        if (mN2M)
        {
            for (uint32_t i = 0; i < mNumOuts; i++)
                for (uint32_t j = 0; j < mNumIns; j++)
                    clear(j, i, resize);
        }
        else
        {
            for (uint32_t i = 0; i < mNumOuts; i++)
                clear(i, i, resize);
        }
    }
    
    void clear(uint32_t inChan, uint32_t outChan, bool resize)
    {
        set(inChan, outChan, nullptr, 0, resize);
    }
    
    // DSP Engine Reset
    
    void reset()
    {
        if (mN2M)
        {
            for (uint32_t i = 0; i < mNumOuts; i++)
                for (uint32_t j = 0; j < mNumIns; j++)
                    reset(j, i);
        }
        else
        {
            for (uint32_t i = 0; i < mNumOuts; i++)
                reset(i, i);
        }
    }
    
    ConvolveError reset(uint32_t inChan, uint32_t outChan)
    {
        // For Parallel operation you must pass the same in/out channel
        
        if (!mN2M) inChan -= outChan;
        
        if (outChan < mNumOuts)
            return mConvolvers[outChan]->reset(inChan);
        else
            return CONVOLVE_ERR_OUT_CHAN_OUT_OF_RANGE;
    }
    
    // Resize and set IR
    
    ConvolveError resize(uint32_t inChan, uint32_t outChan, uintptr_t impulseLength)
    {
        // For Parallel operation you must pass the same in/out channel
        
        if (!mN2M) inChan -= outChan;
        
        if (outChan < mNumOuts)
            return mConvolvers[outChan]->resize(inChan, length);
        else
            return CONVOLVE_ERR_IN_CHAN_OUT_OF_RANGE;
    }
    
    ConvolveError set(uint32_t inChan, uint32_t outChan, const float* input, uintptr_t length, bool resize)
    {
        // For Parallel operation you must pass the same in/out channel
        
        if (!mN2M) inChan -= outChan;
        
        if (outChan < mNumOuts)
            return mConvolvers[outChan]->set(inChan, input, length, resize);
        else
            return CONVOLVE_ERR_OUT_CHAN_OUT_OF_RANGE;
    }
    
    ConvolveError set(uint32_t inChan, uint32_t outChan, const double* input, uintptr_t length, bool resize)
    {
        std::vector<float> inputFloat(impulseLength);
        
        for (unsigned long i = 0; i < impulseLength; i++)
            inputFloat[i] = static_cast<float>(input[i]);
        
        return set(inChan, outChan, inputFloat.data(), impulseLength, resize);
    }
    
    // DSP
    
    void process(const double * const* ins, double** outs, size_t numIns, size_t numOuts, size_t numSamples)
    {
        auto memPointer = mTemporaryMemory.grow((mNumIns + 2) * numSamples);
        tempSetup(memPointer.get(), memPointer.getSize());
        
        if (!memPointer.get())
            numIns = numOuts = 0;
        
        SIMDSettings settings;
        
        for (size_t i = 0; i < numOuts; i++)
        {
            const float *n2nIn[1] = { ins[i] };
            
            mConvolvers[i]->process(mN2M ? ins : n2nIn, outs[i], mTemp1, numSamples, mN2M ? numIns : 1);
        }
    }
    
    void process(const float * const*  ins, float** outs, size_t numIns, size_t numOuts, size_t numSamples)
    {
        auto memPointer = mTemporaryMemory.grow((mNumIns + 2) * numSamples);
        tempSetup(memPointer.get(), memPointer.getSize());
        
        if (!memPointer.get())
            numIns = numOuts = 0;
        
        numIns = numIns > mNumIns ? mNumIns : numIns;
        numOuts = numOuts > mNumOuts ? mNumOuts : numOuts;
        
        for (uintptr_t i = 0; i < numIns; i++)
            for (uintptr_t j = 0; j < numSamples; j++)
                mInTemps[i][j] = static_cast<float>(ins[i][j]);
        
        SIMDSettings settings;
        
        for (uintptr_t i = 0; i < numOuts; i++)
        {
            const float *n2nIn[1] = { mInTemps[i] };
            const float * const *inTemps = mInTemps.data();
            
            mConvolvers[i]->process(mN2M ? inTemps : n2nIn, mTemp2, mTemp1, numSamples, mN2M ? numIns : 1);
            
            for (uintptr_t j = 0; j < numSamples; j++)
                outs[i][j] = mTemp2[j];
        }
    }

    
private:
    
    void tempSetup(float* memPointer, uintptr_t maxFrameSize)
    {
        maxFrameSize /= (mNumIns + 2);
        mInTemps[0] = memPointer;
        
        for (uint32_t i = 1; i < mNumIns; i++)
            mInTemps[i] = mInTemps[0] + (i * maxFrameSize);
        
        mTemp1 = mInTemps[mNumIns - 1] + maxFrameSize;
        mTemp2 = mTemp1 + maxFrameSize;
    }
    
    // Data
    
    uint32_t mNumIns;
    uint32_t mNumOuts;
    bool mN2M;
    
    std::vector<float*> mInTemps;
    float* mTemp1;
    float* mTemp2;
    
    MemorySwap<float> mTemporaryMemory;
    
    std::vector<NToMonoConvolve*> mConvolvers;
};

