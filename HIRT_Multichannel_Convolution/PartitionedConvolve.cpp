
/*
 *  PartitionedConvolve
 *
 *    PartitionedConvolve performs FFT-based partitioned convolution.
 *
 *    Typically PartitionedConvolve might be used in conjuction with TimeDomainConvolve for zero-latency convolution with longer impulses.
 *
 *  Copyright 2012 Alex Harker. All rights reserved.
 *
 */

#include "ConvolveSIMD.h"

#include "PartitionedConvolve.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <functional>

#ifndef __APPLE__
#include <Windows.h>
#endif

// FIX - sort the seeding

// Pointer Utility

void offsetSplitPointer(FFT_SPLIT_COMPLEX_F &complex1, const FFT_SPLIT_COMPLEX_F &complex2, uintptr_t offset)
{
    complex1.realp = complex2.realp + offset;
    complex1.imagp = complex2.imagp + offset;
}

/*
 // Random seeding for rand
 
 static __inline unsigned int get_rand_seed ()
 {
 unsigned int seed;
 
 #ifdef __APPLE__
 seed = arc4random();
 #else
 HCRYPTPROV hProvider = 0;
 const DWORD dwLength = 4;
 BYTE *pbBuffer = (BYTE *) &seed;
 
 if (!CryptAcquireContextW(&hProvider, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
 return 0;
 
 CryptGenRandom(hProvider, dwLength, pbBuffer);
 CryptReleaseContext(hProvider, 0);
 #endif
 
 return seed;
 }
 
 
 void init_partition_convolve()
 {
 srand(get_rand_seed());
 }
 */

t_convolve_error HISSTools::PartitionedConvolve::setMaxFFTSize(uintptr_t maxFFTSize)
{
    uintptr_t maxFFTSizeLog2 = log2(maxFFTSize);
    
    t_convolve_error error = CONVOLVE_ERR_NONE;
    
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
    
    if (maxFFTSize != (1 << maxFFTSizeLog2))
        error = CONVOLVE_ERR_FFT_SIZE_MAX_NON_POWER_OF_TWO;
    
    mMaxFFTSizeLog2 = maxFFTSizeLog2;
    
    return error;
}

HISSTools::PartitionedConvolve::PartitionedConvolve(uintptr_t maxFFTSize, uintptr_t maxLength, uintptr_t offset, uintptr_t length)
: mMaxImpulseLength(maxLength), mFFTSizeLog2(0), mInputPosition(0), mPartitionsDone(0), mLastPartition(0), mNumPartitions(0), mValidPartitions(0), mResetFlag(true)
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
    
    mImpulseBuffer.realp = (float *) ALIGNED_MALLOC((mMaxImpulseLength * 4 * sizeof(float)));
    mImpulseBuffer.imagp = mImpulseBuffer.realp + mMaxImpulseLength;
    mInputBuffer.realp = mImpulseBuffer.imagp + mMaxImpulseLength;
    mInputBuffer.imagp = mInputBuffer.realp + mMaxImpulseLength;
    
    // Allocate fft and temporary buffers
    
    mFFTBuffers[0] = (float *) ALIGNED_MALLOC((maxFFTSize * 6 * sizeof(float)));
    mFFTBuffers[1] = mFFTBuffers[0] + maxFFTSize;
    mFFTBuffers[2] = mFFTBuffers[1] + maxFFTSize;
    mFFTBuffers[3] = mFFTBuffers[2] + maxFFTSize;
    
    mAccumBuffer.realp = mFFTBuffers[3] + maxFFTSize;
    mAccumBuffer.imagp = mAccumBuffer.realp + (maxFFTSize >> 1);
    mPartitionTemp.realp = mAccumBuffer.imagp + (maxFFTSize >> 1);
    mPartitionTemp.imagp = mPartitionTemp.realp + (maxFFTSize >> 1);
    
    hisstools_create_setup(&mFFTSetup, mMaxFFTSizeLog2);
}

HISSTools::PartitionedConvolve::~PartitionedConvolve()
{
    hisstools_destroy_setup(mFFTSetup);
    
    // FIX - try to do better here...
    
    ALIGNED_FREE(mImpulseBuffer.realp);
    ALIGNED_FREE(mFFTBuffers[0]);
}

uintptr_t HISSTools::PartitionedConvolve::log2(uintptr_t value)
{
    uintptr_t bitShift = value;
    uintptr_t bitCount = 0;
    
    while (bitShift)
    {
        bitShift >>= 1U;
        bitCount++;
    }
    
    if (value == 1U << (bitCount - 1U))
        return bitCount - 1U;
    else
        return bitCount;
}

t_convolve_error HISSTools::PartitionedConvolve::setFFTSize(uintptr_t FFTSize)
{
    uintptr_t FFTSizeLog2 = log2(FFTSize);
    
    t_convolve_error error = CONVOLVE_ERR_NONE;
    
    if (FFTSizeLog2 < MIN_FFT_SIZE_LOG2 || FFTSizeLog2 > mMaxFFTSizeLog2)
        return CONVOLVE_ERR_FFT_SIZE_OUT_OF_RANGE;
    
    if (FFTSize != (1 << FFTSizeLog2))
        error = CONVOLVE_ERR_FFT_SIZE_NON_POWER_OF_TWO;
    
    // Set fft variables iff the fft size has actually actually changed
    
    if (FFTSizeLog2 != mFFTSizeLog2)
    {
        mNumPartitions = 0;
        mFFTSizeLog2 = FFTSizeLog2;
    }
    
    return error;
}

t_convolve_error HISSTools::PartitionedConvolve::setLength(uintptr_t length)
{
    t_convolve_error error = CONVOLVE_ERR_NONE;
    
    if (length > mMaxImpulseLength)
    {
        error = CONVOLVE_ERR_PARTITION_LENGTH_TOO_LARGE;
        length = mMaxImpulseLength;
    }
    
    mLength = length;
    
    return error;
}

void HISSTools::PartitionedConvolve::setOffset(uintptr_t offset)
{
    mOffset = offset;
}

t_convolve_error HISSTools::PartitionedConvolve::set(const float *input, uintptr_t length)
{
    t_convolve_error error = CONVOLVE_ERR_NONE;
    
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
        
        std::copy(input + bufferPosition, input + bufferPosition + numSamps, bufferTemp1);
        std::fill_n(bufferTemp1 + numSamps, FFTSize - numSamps, 0.f);
        
        // Do fft straight into position
        
        hisstools_unzip(bufferTemp1, &bufferTemp2, mFFTSizeLog2);
        hisstools_rfft(mFFTSetup, &bufferTemp2, mFFTSizeLog2);
        offsetSplitPointer(bufferTemp2, bufferTemp2, FFTSizeHalved);
    }
    
    mNumPartitions = numPartitions;
    reset();
    
    return error;
}

void HISSTools::PartitionedConvolve::reset()
{
    mResetFlag = true;
}

template<class T>
void scaleStore(float *out, float *temp, uintptr_t FFTSize, bool offset)
{
    T *outPtr = reinterpret_cast<T *>(out + (offset ? FFTSize >> 1: 0));
    T *tempPtr = reinterpret_cast<T *>(temp);
    T scaleMul((float) (1.0 / (double) (FFTSize << 2)));
    
    for (uintptr_t i = 0; i < (FFTSize / (T::size * 2)); i++)
        *(outPtr++) = *(tempPtr++) * scaleMul;
}

bool HISSTools::PartitionedConvolve::process(const float *in, float *out, uintptr_t numSamples)
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
        
        memset(mFFTBuffers[0], 0, getMaxFFTSize() * 5 * sizeof(float));
        
        // Reset fft RWCounter (randomly)
        
        while (FFTSizeHalved < (uintptr_t) (RWCounter = rand() / (RAND_MAX / FFTSizeHalved)));
        
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
        
        memcpy(mFFTBuffers[0] + RWCounter, in, loopSize * sizeof(float));
        
        if ((hiCounter + loopSize) > FFTSize)
        {
            uintptr_t hi_loop = FFTSize - hiCounter;
            memcpy(mFFTBuffers[1] + hiCounter, in, hi_loop * sizeof(float));
            memcpy(mFFTBuffers[1], in + hi_loop, (loopSize - hi_loop) * sizeof(float));
        }
        else
            memcpy(mFFTBuffers[1] + hiCounter, in, loopSize * sizeof(float));
        
        memcpy(out, mFFTBuffers[3] + RWCounter, loopSize * sizeof(float));
        
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
        
        // FFT processing - this is where we deal with the fft, the first partition and inverse fft
        
        if (FFTNow)
        {
            // Do the fft into the input buffer

            offsetSplitPointer(audioInTemp, mInputBuffer, (mInputPosition * FFTSizeHalved));
            hisstools_unzip(mFFTBuffers[(RWCounter == FFTSize) ? 1 : 0], &audioInTemp, mFFTSizeLog2);
            hisstools_rfft(mFFTSetup, &audioInTemp, mFFTSizeLog2);
            
            // Process first partition (needed now) and accumulate the output
            
            processPartition(audioInTemp, mImpulseBuffer, mAccumBuffer, FFTSizeHalved);
            
            // Do ifft on the accumulation buffer, scale and store (overlap-save)
            
            hisstools_rifft(mFFTSetup, &mAccumBuffer, mFFTSizeLog2);
            hisstools_zip(&mAccumBuffer, mFFTBuffers[2], mFFTSizeLog2);
            scaleStore<FloatVector>(mFFTBuffers[3], mFFTBuffers[2], FFTSize, (RWCounter != FFTSize));
            
            // Clear accumulation buffer
            
            memset(mAccumBuffer.realp, 0, FFTSizeHalved * sizeof(float));
            memset(mAccumBuffer.imagp, 0, FFTSizeHalved * sizeof(float));
            
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

void HISSTools::PartitionedConvolve::processPartition(FFT_SPLIT_COMPLEX_F in1, FFT_SPLIT_COMPLEX_F in2, FFT_SPLIT_COMPLEX_F out, uintptr_t numBins)
{
    uintptr_t numVecs = numBins >> 2;
    
    FloatVector *inReal1 = (FloatVector *) in1.realp;
    FloatVector *inImag1 = (FloatVector *) in1.imagp;
    FloatVector *inReal2 = (FloatVector *) in2.realp;
    FloatVector *inImag2 = (FloatVector *) in2.imagp;
    FloatVector *outReal = (FloatVector *) out.realp;
    FloatVector *outImag = (FloatVector *) out.imagp;
    
    float nyquist1 = in1.imagp[0];
    float nyquist2 = in2.imagp[0];
    
    // Do Nyquist Calculation and then zero these bins
    
    out.imagp[0] += nyquist1 * nyquist2;
    
    in1.imagp[0] = 0.f;
    in2.imagp[0] = 0.f;
    
    // Do other bins (loop unrolled)
    
    for (uintptr_t i = 0; i + 3 < numVecs; i += 4)
    {
        outReal[i+0] = outReal[i+0] + ((inReal1[i+0] * inReal2[i+0]) - (inImag1[i+0] * inImag2[i+0]));
        outImag[i+0] = outImag[i+0] + ((inReal1[i+0] * inImag2[i+0]) + (inImag1[i+0] * inReal2[i+0]));
        outReal[i+1] = outReal[i+1] + ((inReal1[i+1] * inReal2[i+1]) - (inImag1[i+1] * inImag2[i+1]));
        outImag[i+1] = outImag[i+1] + ((inReal1[i+1] * inImag2[i+1]) + (inImag1[i+1] * inReal2[i+1]));
        outReal[i+2] = outReal[i+2] + ((inReal1[i+2] * inReal2[i+2]) - (inImag1[i+2] * inImag2[i+2]));
        outImag[i+2] = outImag[i+2] + ((inReal1[i+2] * inImag2[i+2]) + (inImag1[i+2] * inReal2[i+2]));
        outReal[i+3] = outReal[i+3] + ((inReal1[i+3] * inReal2[i+3]) - (inImag1[i+3] * inImag2[i+3]));
        outImag[i+3] = outImag[i+3] + ((inReal1[i+3] * inImag2[i+3]) + (inImag1[i+3] * inReal2[i+3]));
    }
    
    // Replace nyquist bins
    
    in1.imagp[0] = nyquist1;
    in2.imagp[0] = nyquist2;
}
