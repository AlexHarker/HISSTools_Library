
#include "MonoConvolve.h"

#include "ConvolveSIMD.h"
#include <functional>
#include <random>

typedef MemorySwap<HISSTools::PartitionedConvolve>::Ptr PartPtr;

// Allocation Utilities

template <uintptr_t x>
HISSTools::PartitionedConvolve *alloc(uintptr_t size)
{
    return new HISSTools::PartitionedConvolve(16384, std::max(size, uintptr_t(16384)) - x, x, 0);
}

MemorySwap<HISSTools::PartitionedConvolve>::AllocFunc getAllocator(LatencyMode latency)
{
    switch (latency)
    {
        case kLatencyZero:      return alloc<8192>;
        case kLatencyShort:     return alloc<8064>;
        case kLatencyMedium:    return alloc<7680>;
    }
}

void largeFree(HISSTools::PartitionedConvolve *largePartition)
{
    delete largePartition;
}

// Constructor and Deconstructor

HISSTools::MonoConvolve::MonoConvolve(uintptr_t maxLength, LatencyMode latency)
: mPart4(getAllocator(latency), largeFree, maxLength), mLength(0), mLatency(latency), mReset(false)
{
    switch (latency)
    {
        case kLatencyZero:
            mTime1.reset(new TimeDomainConvolve(0, 128));
            mPart1.reset(new PartitionedConvolve(256, 384, 128, 384));
            mPart2.reset(new PartitionedConvolve(1024, 1536, 512, 1536));
            mPart3.reset(new PartitionedConvolve(4096, 6144, 2048, 6144));
            break;
            
        case kLatencyShort:
            mPart1.reset(new PartitionedConvolve(256, 384, 0, 384));
            mPart2.reset(new PartitionedConvolve(1024, 1536, 384, 1536));
            mPart3.reset(new PartitionedConvolve(4096, 6144, 1920, 6144));
            break;
            
        case kLatencyMedium:
            mPart2.reset(new PartitionedConvolve(1024, 1536, 0, 1536));
            mPart3.reset(new PartitionedConvolve(4096, 6144, 1536, 6144));
            break;
    }

    setResetOffset();
}

void HISSTools::MonoConvolve::setResetOffset(intptr_t offset)
{
    PartPtr part4 = mPart4.access();
    
    if (offset < 0)
        offset = std::rand() % 8192;
    
    if (mPart1) mPart1.get()->setResetOffset(offset + 16);
    if (mPart2) mPart2.get()->setResetOffset(offset + 128);
    if (mPart3) mPart3.get()->setResetOffset(offset + 1024);
    if (part4.get()) part4.get()->setResetOffset(offset + 0);
}

ConvolveError HISSTools::MonoConvolve::resize(uintptr_t length)
{
    mLength = 0;
    PartPtr part4 = mPart4.equal(getAllocator(mLatency), largeFree, length);
    
    return part4.getSize() == length ? CONVOLVE_ERR_NONE : CONVOLVE_ERR_MEM_UNAVAILABLE;
}

template <class T>
void setPart(T *obj, const float *input, uintptr_t length)
{
    if (obj) obj->set(input, length);
}

ConvolveError HISSTools::MonoConvolve::set(const float *input, uintptr_t length, bool requestResize)
{
    // Lock or resize first to ensure that audio finishes processing before we replace
    
    mLength = 0;
    PartPtr part4 = requestResize ? mPart4.equal(getAllocator(mLatency), largeFree, length) : mPart4.access();
    
    if (part4.get())
    {
        setPart(mTime1.get(), input, length);
        setPart(mPart1.get(), input, length);
        setPart(mPart2.get(), input, length);
        setPart(mPart3.get(), input, length);
        setPart(part4.get(), input, length);
        
        mLength = length;
        reset();
    }
    
    return (length && !part4.get()) ? CONVOLVE_ERR_MEM_UNAVAILABLE : (length > part4.getSize()) ? CONVOLVE_ERR_MEM_ALLOC_TOO_SMALL : CONVOLVE_ERR_NONE;
}

template <class T>
void resetPart(T *obj)
{
    if (obj) obj->reset();
}

ConvolveError HISSTools::MonoConvolve::reset()
{
    mReset = true;
    return CONVOLVE_ERR_NONE;
}

template<class T>
void sum(T *temp, T *out, uintptr_t numItems)
{
    for (uintptr_t i = 0; i < numItems; i++, out++, temp++)
        *out = *out + *temp;
}

template<class T>
void processAndSum(T *obj, const float *in, float *temp, float *out, uintptr_t numSamples)
{
    if (obj && obj->process(in, temp, numSamples))
    {
        if ((numSamples % 4) || (((uintptr_t) out) % 16) || (((uintptr_t) temp) % 16))
            sum(temp, out, numSamples);
        else
            sum(reinterpret_cast<FloatVector *>(temp), reinterpret_cast<FloatVector *>(out), numSamples / FloatVector::size);
    }
}

void HISSTools::MonoConvolve::process(const float *in, float *temp, float *out, uintptr_t numSamples)
{
    PartPtr part4 = mPart4.attempt();
    
    // N.B. This function DOES NOT zero the output buffer as this is done elsewhere
    
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
        
        processAndSum(mTime1.get(), in, temp, out, numSamples);
        processAndSum(mPart1.get(), in, temp, out, numSamples);
        processAndSum(mPart2.get(), in, temp, out, numSamples);
        processAndSum(mPart3.get(), in, temp, out, numSamples);
        processAndSum(part4.get(), in, temp, out, numSamples);
    }
}
