
#pragma once

#include "PartitionedConvolve.h"
#include "TimeDomainConvolve.h"
#include "ConvolveSIMD.h"
#include "ConvolveErrors.h"
#include "MemorySwap.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <random>
#include <stdexcept>
#include <vector>

enum LatencyMode
{
    kLatencyZero,
    kLatencyShort,
    kLatencyMedium,
} ;

class MonoConvolve
{
    using PartPtr = MemorySwap<HISSTools::PartitionedConvolve>::Ptr;
    using PartUniquePtr = std::unique_ptr<HISSTools::PartitionedConvolve>;
    
public:
    
    // Constructors
    
    MonoConvolve(uintptr_t maxLength, LatencyMode latency)
    : mAllocator(nullptr)
    , mPart4(0)
    , mLength(0)
    , mPart4ResetOffset(0)
    , mReset(false)
    , mRandGenerator(std::random_device()())
    {
        switch (latency)
        {
            case kLatencyZero:      setPartitions(maxLength, true, 256, 1024, 4096, 16384);     break;
            case kLatencyShort:     setPartitions(maxLength, false, 256, 1024, 4096, 16384);    break;
            case kLatencyMedium:    setPartitions(maxLength, false, 1024, 4096, 16384);         break;
        }
    }
    
    MonoConvolve(uintptr_t maxLength, bool zeroLatency, uint32_t A, uint32_t B = 0, uint32_t C = 0, uint32_t D = 0)
    : mAllocator(nullptr)
    , mPart4(0)
    , mLength(0)
    , mPart4ResetOffset(0)
    , mReset(false)
    , mRandGenerator(std::random_device()())
    {
        setPartitions(maxLength, zeroLatency, A, B, C, D);
    }
    
    // Moveable but not copyable
    
    MonoConvolve(MonoConvolve& obj) = delete;
    MonoConvolve& operator = (MonoConvolve& obj) = delete;
    
    MonoConvolve(MonoConvolve&& obj)
    : mAllocator(obj.mAllocator)
    , mSizes(std::move(obj.mSizes))
    , mTime1(std::move(obj.mTime1))
    , mPart1(std::move(obj.mPart1))
    , mPart2(std::move(obj.mPart2))
    , mPart3(std::move(obj.mPart3))
    , mPart4(std::move(obj.mPart4))
    , mLength(obj.mLength)
    , mPart4ResetOffset(obj.mPart4ResetOffset)
    , mReset(true)
    {}
    
    MonoConvolve& operator = (MonoConvolve&& obj)
    {
        mAllocator = obj.mAllocator;
        mSizes = std::move(obj.mSizes);
        mTime1 = std::move(obj.mTime1);
        mPart1 = std::move(obj.mPart1);
        mPart2 = std::move(obj.mPart2);
        mPart3 = std::move(obj.mPart3);
        mPart4 = std::move(obj.mPart4);
        mLength = obj.mLength;
        mPart4ResetOffset = obj.mPart4ResetOffset;
        mReset = true;
        
        return *this;
    }
    
    // Offsets / resize / set/ reset
    
    void setResetOffset(intptr_t offset = -1)
    {
        PartPtr part4 = mPart4.access();
        
        setResetOffset(part4, offset);
    }
    
    ConvolveError resize(uintptr_t length)
    {
        mLength = 0;
        PartPtr part4 = mPart4.equal(mAllocator, largeFree, length);
        
        if (part4.get())
            part4.get()->setResetOffset(mPart4ResetOffset);
        
        return part4.getSize() == length ? CONVOLVE_ERR_NONE : CONVOLVE_ERR_MEM_UNAVAILABLE;
    }
    
    ConvolveError set(const float *input, uintptr_t length, bool requestResize)
    {
        // Lock or resize first to ensure that audio finishes processing before we replace
        
        mLength = 0;
        PartPtr part4 = requestResize ? mPart4.equal(mAllocator, largeFree, length) : mPart4.access();
        
        if (part4.get())
        {
            setPart(mTime1.get(), input, length);
            setPart(mPart1.get(), input, length);
            setPart(mPart2.get(), input, length);
            setPart(mPart3.get(), input, length);
            setPart(part4.get(), input, length);
            
            part4.get()->setResetOffset(mPart4ResetOffset);
            
            mLength = length;
            reset();
        }
        
        return (length && !part4.get()) ? CONVOLVE_ERR_MEM_UNAVAILABLE : (length > part4.getSize()) ? CONVOLVE_ERR_MEM_ALLOC_TOO_SMALL : CONVOLVE_ERR_NONE;
    }
    
    ConvolveError reset()
    {
        mReset = true;
        return CONVOLVE_ERR_NONE;
    }
    
    // Process
    
    void process(const float *in, float *temp, float *out, uintptr_t numSamples, bool accumulate = false)
    {
        PartPtr part4 = mPart4.attempt();
        
        if (mLength && mLength <= part4.getSize())
        {
            if (mReset)
            {
                resetPart(mTime1.get());
                resetPart(mPart1.get());
                resetPart(mPart2.get());
                resetPart(mPart3.get());
                resetPart(part4.get());
                mReset = false;
            }
            
            processAndSum(mTime1.get(), in, temp, out, numSamples, accumulate);
            processAndSum(mPart1.get(), in, temp, out, numSamples, accumulate || mTime1);
            processAndSum(mPart2.get(), in, temp, out, numSamples, accumulate || mPart1);
            processAndSum(mPart3.get(), in, temp, out, numSamples, accumulate || mPart2);
            processAndSum(part4.get(), in, temp, out, numSamples, accumulate || mPart3);
        }
    }
    
    // Set partiioning
    
    void setPartitions(uintptr_t maxLength, bool zeroLatency, uint32_t A, uint32_t B = 0, uint32_t C = 0, uint32_t D = 0)
    {
        // Utilities
        
        auto checkAndStoreFFTSize = [this](int size, int prev)
        {
            if ((size >= (1 << 5)) && (size <= (1 << 20)) && size > prev)
                mSizes.push_back(size);
            else if (size)
                throw std::runtime_error("invalid FFT size or order");
        };
        
        auto createPart = [](PartUniquePtr& obj, uint32_t& offset, uint32_t size, uint32_t next)
        {
            obj.reset(new HISSTools::PartitionedConvolve(size, (next - size) >> 1, offset, (next - size) >> 1));
            offset += (next - size) >> 1;
        };
        
        // Sanity checks
        
        checkAndStoreFFTSize(A, 0);
        checkAndStoreFFTSize(B, A);
        checkAndStoreFFTSize(C, B);
        checkAndStoreFFTSize(D, C);
        
        if (!numSizes())
            throw std::runtime_error("no valid FFT sizes given");
        
        // Lock to ensure we have exclusive access
        
        PartPtr part4 = mPart4.access();
        
        uint32_t offset = zeroLatency ? mSizes[0] >> 1 : 0;
        uint32_t largestSize = mSizes[numSizes() - 1];
        
        // Allocate paritions in unique pointers
        
        if (zeroLatency) mTime1.reset(new HISSTools::TimeDomainConvolve(0, mSizes[0] >> 1));
        if (numSizes() == 4) createPart(mPart1, offset, mSizes[0], mSizes[1]);
        if (numSizes() > 2) createPart(mPart2, offset, mSizes[numSizes() - 3], mSizes[numSizes() - 2]);
        if (numSizes() > 1) createPart(mPart3, offset, mSizes[numSizes() - 2], mSizes[numSizes() - 1]);
        
        // Allocate the final resizeable partition
        
        mAllocator = [offset, largestSize](uintptr_t size)
        {
            return new HISSTools::PartitionedConvolve(largestSize, std::max(size, uintptr_t(largestSize)) - offset, offset, 0);
        };
        
        part4.equal(mAllocator, largeFree, maxLength);
        
        // Set offsets
        
        mRandDistribution = std::uniform_int_distribution<uintptr_t>(0, (mSizes.back() >> 1) - 1);
        setResetOffset(part4);
    }
    
private:
    
    size_t numSizes() { return mSizes.size(); }

    void setResetOffset(PartPtr &part4, intptr_t offset = -1)
    {
        if (offset < 0)
            offset = mRandDistribution(mRandGenerator);
        
        if (mPart1) mPart1.get()->setResetOffset(offset + (mSizes[numSizes() - 3] >> 3));
        if (mPart2) mPart2.get()->setResetOffset(offset + (mSizes[numSizes() - 2] >> 3));
        if (mPart3) mPart3.get()->setResetOffset(offset + (mSizes[numSizes() - 1] >> 3));
        
        if (part4.get()) part4.get()->setResetOffset(offset);
        
        mPart4ResetOffset = offset;
    }
    
    // Utilities
    
    template<class T>
    static void sum(T *temp, T *out, uintptr_t numItems)
    {
        for (uintptr_t i = 0; i < numItems; i++, out++, temp++)
            *out = *out + *temp;
    }
    
    template<class T>
    static void processAndSum(T *obj, const float *in, float *temp, float *out, uintptr_t numSamples, bool accumulate)
    {
        if (obj && obj->process(in, accumulate ? temp : out, numSamples) && accumulate)
        {
            if ((numSamples % 4) || isUnaligned(out) || isUnaligned(temp))
                sum(temp, out, numSamples);
            else
                sum(reinterpret_cast<FloatVector *>(temp), reinterpret_cast<FloatVector *>(out), numSamples / FloatVector::size);
        }
    }
    
    static void largeFree(HISSTools::PartitionedConvolve *largePartition)
    {
        delete largePartition;
    }
    
    template <class T>
    static void setPart(T *obj, const float *input, uintptr_t length)
    {
        if (obj) obj->set(input, length);
    }
    
    template <class T>
    static void resetPart(T *obj)
    {
        if (obj) obj->reset();
    }
    
    template<class T>
    static bool isUnaligned(const T* ptr)
    {
        return reinterpret_cast<uintptr_t>(ptr) % 16;
    }
    
    MemorySwap<HISSTools::PartitionedConvolve>::AllocFunc mAllocator;
    
    std::vector<uint32_t> mSizes;
    
    std::unique_ptr<HISSTools::TimeDomainConvolve> mTime1;
    std::unique_ptr<HISSTools::PartitionedConvolve> mPart1;
    std::unique_ptr<HISSTools::PartitionedConvolve> mPart2;
    std::unique_ptr<HISSTools::PartitionedConvolve> mPart3;
    
    MemorySwap<HISSTools::PartitionedConvolve> mPart4;
    
    uintptr_t mLength;
    
    intptr_t mPart4ResetOffset;
    bool mReset;
    
    // Random Number Generation
    
    std::default_random_engine mRandGenerator;
    std::uniform_int_distribution<uintptr_t> mRandDistribution;
};
