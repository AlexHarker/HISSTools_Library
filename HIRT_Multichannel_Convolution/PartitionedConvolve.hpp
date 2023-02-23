
#pragma once

#include "../HISSTools_FFT/HISSTools_FFT.h"

#include "ConvolveErrors.h"
#include "../SIMDSupport.hpp"

#include <algorithm>
#include <cstdint>
#include <random>

class PartitionedConvolve
{
    // N.B. MIN_FFT_SIZE_LOG2 should never be smaller than 4, as below code assumes loop unroll of vectors (4 vals) by 4 (== 16 or 2^4)
    // MAX_FFT_SIZE_LOG2 is perhaps conservative right now, but it is easy to increase this if necessary
    
    static constexpr int MIN_FFT_SIZE_LOG2 = 5;
    static constexpr int MAX_FFT_SIZE_LOG2 = 20;
    
public:
    
    PartitionedConvolve(uintptr_t maxFFTSize, uintptr_t maxLength, uintptr_t offset, uintptr_t length)
    : mMaxImpulseLength(maxLength)
    , mFFTSizeLog2(0)
    , mInputPosition(0)
    , mPartitionsDone(0)
    , mLastPartition(0)
    , mNumPartitions(0)
    , mValidPartitions(0)
    , mResetOffset(-1)
    , mResetFlag(true)
    , mRandGenerator(std::random_device()())
    {
        // Set default initial attributes and variables
        
        setMaxFFTSize(maxFFTSize);
        setFFTSize(getMaxFFTSize());
        setOffset(offset);
        setLength(length);
        
        // Allocate impulse buffer and input buffer
        
        maxFFTSize = getMaxFFTSize();
        
        // This is designed to make sure we can load the max impulse length, whatever the fft size
        
        if (mMaxImpulseLength % (maxFFTSize >> 1))
        {
            mMaxImpulseLength /= (maxFFTSize >> 1);
            mMaxImpulseLength++;
            mMaxImpulseLength *= (maxFFTSize >> 1);
        }
        
        mImpulseBuffer.realp = allocate_aligned<float>(mMaxImpulseLength * 4);
        mImpulseBuffer.imagp = mImpulseBuffer.realp + mMaxImpulseLength;
        mInputBuffer.realp = mImpulseBuffer.imagp + mMaxImpulseLength;
        mInputBuffer.imagp = mInputBuffer.realp + mMaxImpulseLength;
        
        // Allocate fft and temporary buffers
        
        mFFTBuffers[0] = allocate_aligned<float>(maxFFTSize * 6);
        mFFTBuffers[1] = mFFTBuffers[0] + maxFFTSize;
        mFFTBuffers[2] = mFFTBuffers[1] + maxFFTSize;
        mFFTBuffers[3] = mFFTBuffers[2] + maxFFTSize;
        
        mAccumBuffer.realp = mFFTBuffers[3] + maxFFTSize;
        mAccumBuffer.imagp = mAccumBuffer.realp + (maxFFTSize >> 1);
        mPartitionTemp.realp = mAccumBuffer.imagp + (maxFFTSize >> 1);
        mPartitionTemp.imagp = mPartitionTemp.realp + (maxFFTSize >> 1);
        
        hisstools_create_setup(&mFFTSetup, mMaxFFTSizeLog2);
    }
    
    ~PartitionedConvolve()
    {
        hisstools_destroy_setup(mFFTSetup);
        
        // FIX - try to do better here...
        
        deallocate_aligned(mImpulseBuffer.realp);
        deallocate_aligned(mFFTBuffers[0]);
    }
    
    // Non-moveable and copyable
    
    PartitionedConvolve(PartitionedConvolve& obj) = delete;
    PartitionedConvolve& operator = (PartitionedConvolve& obj) = delete;
    PartitionedConvolve(PartitionedConvolve&& obj) = delete;
    PartitionedConvolve& operator = (PartitionedConvolve&& obj) = delete;
    
    ConvolveError setFFTSize(uintptr_t FFTSize)
    {
        uintptr_t FFTSizeLog2 = log2(FFTSize);
        
        ConvolveError error = CONVOLVE_ERR_NONE;
        
        if (FFTSizeLog2 < MIN_FFT_SIZE_LOG2 || FFTSizeLog2 > mMaxFFTSizeLog2)
            return CONVOLVE_ERR_FFT_SIZE_OUT_OF_RANGE;
        
        if (FFTSize != (uintptr_t(1) << FFTSizeLog2))
            error = CONVOLVE_ERR_FFT_SIZE_NON_POWER_OF_TWO;
        
        // Set fft variables iff the fft size has actually actually changed
        
        if (FFTSizeLog2 != mFFTSizeLog2)
        {
            mNumPartitions = 0;
            mFFTSizeLog2 = FFTSizeLog2;
        }
        
        mRandDistribution = std::uniform_int_distribution<uintptr_t>(0, (FFTSize >> 1) - 1);
        
        return error;
    }
    
    ConvolveError setLength(uintptr_t length)
    {
        mLength = std::min(length, mMaxImpulseLength);
        
        return (length > mMaxImpulseLength) ? CONVOLVE_ERR_PARTITION_LENGTH_TOO_LARGE : CONVOLVE_ERR_NONE;
    }
    
    void setOffset(uintptr_t offset)
    {
        mOffset = offset;
    }
    
    void setResetOffset(intptr_t offset = -1)
    {
        mResetOffset = offset;
    }
    
    ConvolveError set(const float *input, uintptr_t length)
    {
        ConvolveError error = CONVOLVE_ERR_NONE;
        
        // FFT variables / attributes
        
        uintptr_t bufferPosition;
        uintptr_t FFTSize = getFFTSize();
        uintptr_t FFTSizeHalved = FFTSize >> 1;
        
        // Partition variables
        
        float *bufferTemp1 = (float *) mPartitionTemp.realp;
        FFT_SPLIT_COMPLEX_F bufferTemp2;
        
        uintptr_t numPartitions;
        
        // Calculate how much of the buffer to load
        
        length = (!input || length <= mOffset) ? 0 : length - mOffset;
        length = (mLength && mLength < length) ? mLength : length;
        
        if (length > mMaxImpulseLength)
        {
            length = mMaxImpulseLength;
            error = CONVOLVE_ERR_MEM_ALLOC_TOO_SMALL;
        }
        
        // Partition / load the impulse
        
        for (bufferPosition = mOffset, bufferTemp2 = mImpulseBuffer, numPartitions = 0; length > 0; bufferPosition += FFTSizeHalved, numPartitions++)
        {
            // Get samples up to half the fft size
            
            uintptr_t numSamps = (length > FFTSizeHalved) ? FFTSizeHalved : length;
            length -= numSamps;
            
            // Get samples and zero pad
            
            std::copy_n(input + bufferPosition, numSamps, bufferTemp1);
            std::fill_n(bufferTemp1 + numSamps, FFTSize - numSamps, 0.f);
            
            // Do fft straight into position
            
            hisstools_rfft(mFFTSetup, bufferTemp1, &bufferTemp2, FFTSize, mFFTSizeLog2);
            offsetSplitPointer(bufferTemp2, bufferTemp2, FFTSizeHalved);
        }
        
        mNumPartitions = numPartitions;
        reset();
        
        return error;
    }

    void reset()
    {
        mResetFlag = true;
    }
    
    bool process(const float *in, float *out, uintptr_t numSamples)
    {
        FFT_SPLIT_COMPLEX_F impulseTemp;
        FFT_SPLIT_COMPLEX_F audioInTemp;
        
        // Scheduling variables
        
        intptr_t numPartitionsToDo;
        
        // FFT variables
        
        uintptr_t FFTSize = getFFTSize();
        uintptr_t FFTSizeHalved = FFTSize >> 1;
        
        uintptr_t RWCounter = mRWCounter;
        uintptr_t hopMask = FFTSizeHalved - 1;
        
        uintptr_t samplesRemaining = numSamples;
        
        if  (!mNumPartitions)
            return false;
        
        // If we need to reset everything we do that here - happens when the fft size changes, or a new buffer is loaded
        
        if (mResetFlag)
        {
            // Reset fft buffers + accum buffer
            
            std::fill_n(mFFTBuffers[0], getMaxFFTSize() * 5, 0.f);
            
            // Reset fft RWCounter (randomly or by fixed amount)
            
            if (mResetOffset < 0)
                RWCounter = mRandDistribution(mRandGenerator);
            else
                RWCounter = mResetOffset % FFTSizeHalved;
            
            // Reset scheduling variables
            
            mInputPosition = 0;
            mPartitionsDone = 0;
            mLastPartition = 0;
            mValidPartitions = 1;
            
            // Set reset flag off
            
            mResetFlag = false;
        }
        
        // Main loop
        
        while (samplesRemaining > 0)
        {
            // Calculate how many IO samples to deal with this loop (depending on whether there is an fft to do before the end of the signal block)
            
            uintptr_t tillNextFFT = (FFTSizeHalved - (RWCounter & hopMask));
            uintptr_t loopSize = samplesRemaining < tillNextFFT ? samplesRemaining : tillNextFFT;
            uintptr_t hiCounter = (RWCounter + FFTSizeHalved) & (FFTSize - 1);
            
            // Load input into buffer (twice) and output from the output buffer
            
            std::copy_n(in, loopSize, mFFTBuffers[0] + RWCounter);
            std::copy_n(in, loopSize, mFFTBuffers[1] + hiCounter);
            
            std::copy_n(mFFTBuffers[3] + RWCounter, loopSize, out);
            
            // Updates to pointers and counters
            
            samplesRemaining -= loopSize;
            RWCounter += loopSize;
            in += loopSize;
            out += loopSize;
            
            bool FFTNow = !(RWCounter & hopMask);
            
            // Work loop and scheduling - this is where most of the convolution is done
            // How many partitions to do this block? (make sure that all partitions are done before we need to do the next fft)
            
            if (FFTNow)
                numPartitionsToDo = (mValidPartitions - mPartitionsDone) - 1;
            else
                numPartitionsToDo = (((mValidPartitions - 1) * (RWCounter & hopMask)) / FFTSizeHalved) - mPartitionsDone;
            
            while (numPartitionsToDo > 0)
            {
                // Calculate buffer wraparounds (if wraparound is in the middle of this set of partitions this loop will run again)
                
                uintptr_t nextPartition = (mLastPartition < mNumPartitions) ? mLastPartition : 0;
                mLastPartition = std::min(mNumPartitions, nextPartition + numPartitionsToDo);
                numPartitionsToDo -= mLastPartition - nextPartition;
                
                // Calculate offsets and pointers
                
                offsetSplitPointer(impulseTemp, mImpulseBuffer, ((mPartitionsDone + 1) * FFTSizeHalved));
                offsetSplitPointer(audioInTemp, mInputBuffer, (nextPartition * FFTSizeHalved));
                
                // Do processing
                
                for (uintptr_t i = nextPartition; i < mLastPartition; i++)
                {
                    processPartition(audioInTemp, impulseTemp, mAccumBuffer, FFTSizeHalved);
                    offsetSplitPointer(impulseTemp, impulseTemp, FFTSizeHalved);
                    offsetSplitPointer(audioInTemp, audioInTemp, FFTSizeHalved);
                    mPartitionsDone++;
                }
            }
            
            // FFT processing
            
            if (FFTNow)
            {
                using Vec = SIMDType<float, 4>;
                
                // Do the fft into the input buffer, add first partition (needed now), do ifft, scale and store (overlap-save)
                
                offsetSplitPointer(audioInTemp, mInputBuffer, (mInputPosition * FFTSizeHalved));
                hisstools_rfft(mFFTSetup, mFFTBuffers[(RWCounter == FFTSize) ? 1 : 0], &audioInTemp, FFTSize, mFFTSizeLog2);
                processPartition(audioInTemp, mImpulseBuffer, mAccumBuffer, FFTSizeHalved);
                hisstools_rifft(mFFTSetup, &mAccumBuffer, mFFTBuffers[2], mFFTSizeLog2);
                scaleStore<Vec>(mFFTBuffers[3], mFFTBuffers[2], FFTSize, (RWCounter != FFTSize));
                
                // Clear accumulation buffer
                
                std::fill_n(mAccumBuffer.realp, FFTSizeHalved, 0.f);
                std::fill_n(mAccumBuffer.imagp, FFTSizeHalved, 0.f);
                
                // Update RWCounter
                
                RWCounter = RWCounter & (FFTSize - 1);
                
                // Set scheduling variables
                
                mValidPartitions = std::min(mNumPartitions, mValidPartitions + 1);
                mInputPosition = mInputPosition ? mInputPosition - 1 : mNumPartitions - 1;
                mLastPartition = mInputPosition + 1;
                mPartitionsDone = 0;
            }
        }
        
        // Write counter back into the object
        
        mRWCounter = RWCounter;
        
        return true;
    }

private:
    
    uintptr_t getFFTSize()      { return uintptr_t(1) << mFFTSizeLog2; }
    uintptr_t getMaxFFTSize()   { return uintptr_t(1) << mMaxFFTSizeLog2; }
    
    static void processPartition(FFT_SPLIT_COMPLEX_F in1, FFT_SPLIT_COMPLEX_F in2, FFT_SPLIT_COMPLEX_F out, uintptr_t numBins)
    {
        using Vec = SIMDType<float, 4>;
        uintptr_t numVecs = numBins / Vec::size;
        
        Vec *iReal1 = reinterpret_cast<Vec *>(in1.realp);
        Vec *iImag1 = reinterpret_cast<Vec *>(in1.imagp);
        Vec *iReal2 = reinterpret_cast<Vec *>(in2.realp);
        Vec *iImag2 = reinterpret_cast<Vec *>(in2.imagp);
        Vec *oReal = reinterpret_cast<Vec *>(out.realp);
        Vec *oImag = reinterpret_cast<Vec *>(out.imagp);
        
        float nyquist1 = in1.imagp[0];
        float nyquist2 = in2.imagp[0];
        
        // Do Nyquist Calculation and then zero these bins
        
        out.imagp[0] += nyquist1 * nyquist2;
        
        in1.imagp[0] = 0.f;
        in2.imagp[0] = 0.f;
        
        // Do other bins (loop unrolled)
        
        for (uintptr_t i = 0; i + 3 < numVecs; i += 4)
        {
            *oReal++ += (iReal1[i + 0] * iReal2[i + 0]) - (iImag1[i + 0] * iImag2[i + 0]);
            *oImag++ += (iReal1[i + 0] * iImag2[i + 0]) + (iImag1[i + 0] * iReal2[i + 0]);
            *oReal++ += (iReal1[i + 1] * iReal2[i + 1]) - (iImag1[i + 1] * iImag2[i + 1]);
            *oImag++ += (iReal1[i + 1] * iImag2[i + 1]) + (iImag1[i + 1] * iReal2[i + 1]);
            *oReal++ += (iReal1[i + 2] * iReal2[i + 2]) - (iImag1[i + 2] * iImag2[i + 2]);
            *oImag++ += (iReal1[i + 2] * iImag2[i + 2]) + (iImag1[i + 2] * iReal2[i + 2]);
            *oReal++ += (iReal1[i + 3] * iReal2[i + 3]) - (iImag1[i + 3] * iImag2[i + 3]);
            *oImag++ += (iReal1[i + 3] * iImag2[i + 3]) + (iImag1[i + 3] * iReal2[i + 3]);
        }
        
        // Replace nyquist bins
        
        in1.imagp[0] = nyquist1;
        in2.imagp[0] = nyquist2;
    }

    ConvolveError setMaxFFTSize(uintptr_t maxFFTSize)
    {
        uintptr_t maxFFTSizeLog2 = log2(maxFFTSize);
        
        ConvolveError error = CONVOLVE_ERR_NONE;
        
        if (maxFFTSizeLog2 > MAX_FFT_SIZE_LOG2)
        {
            error = CONVOLVE_ERR_FFT_SIZE_MAX_TOO_LARGE;
            maxFFTSizeLog2 = MAX_FFT_SIZE_LOG2;
        }
        
        if (maxFFTSizeLog2 && maxFFTSizeLog2 < MIN_FFT_SIZE_LOG2)
        {
            error = CONVOLVE_ERR_FFT_SIZE_MAX_TOO_SMALL;
            maxFFTSizeLog2 = MIN_FFT_SIZE_LOG2;
        }
        
        if (maxFFTSize != (uintptr_t(1) << maxFFTSizeLog2))
            error = CONVOLVE_ERR_FFT_SIZE_MAX_NON_POWER_OF_TWO;
        
        mMaxFFTSizeLog2 = maxFFTSizeLog2;
        
        return error;
    }
    
    template<class T>
    static void scaleStore(float *out, float *temp, uintptr_t FFTSize, bool offset)
    {
        T *outPtr = reinterpret_cast<T *>(out + (offset ? FFTSize >> 1: 0));
        T *tempPtr = reinterpret_cast<T *>(temp);
        T scaleMul(1.f / static_cast<float>(FFTSize << 2));
        
        for (uintptr_t i = 0; i < (FFTSize / (T::size * 2)); i++)
            *(outPtr++) = *(tempPtr++) * scaleMul;
    }
    
    static uintptr_t log2(uintptr_t value)
    {
        uintptr_t bitShift = value;
        uintptr_t bitCount = 0;
        
        while (bitShift)
        {
            bitShift >>= 1U;
            bitCount++;
        }
        
        if (value == uintptr_t(1) << (bitCount - 1U))
            return bitCount - 1U;
        else
            return bitCount;
    }
    
    static void offsetSplitPointer(FFT_SPLIT_COMPLEX_F &complex1, const FFT_SPLIT_COMPLEX_F &complex2, uintptr_t offset)
    {
        complex1.realp = complex2.realp + offset;
        complex1.imagp = complex2.imagp + offset;
    }
    
    // Parameters
    
    uintptr_t mOffset;
    uintptr_t mLength;
    uintptr_t mMaxImpulseLength;
    
    // FFT variables
    
    FFT_SETUP_F mFFTSetup;
    
    uintptr_t mMaxFFTSizeLog2;
    uintptr_t mFFTSizeLog2;
    uintptr_t mRWCounter;
    
    // Scheduling variables
    
    uintptr_t mInputPosition;
    uintptr_t mPartitionsDone;
    uintptr_t mLastPartition;
    uintptr_t mNumPartitions;
    uintptr_t mValidPartitions;
    
    // Internal buffers
    
    float *mFFTBuffers[4];
    
    FFT_SPLIT_COMPLEX_F mImpulseBuffer;
    FFT_SPLIT_COMPLEX_F    mInputBuffer;
    FFT_SPLIT_COMPLEX_F    mAccumBuffer;
    FFT_SPLIT_COMPLEX_F    mPartitionTemp;
    
    // Flags
    
    intptr_t mResetOffset;
    bool mResetFlag;
    
    // Random number generation
    
    std::default_random_engine mRandGenerator;
    std::uniform_int_distribution<uintptr_t> mRandDistribution;
};
