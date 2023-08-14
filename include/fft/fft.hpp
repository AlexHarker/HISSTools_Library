
#ifndef HISSTOOLS_FFT_HPP
#define HISSTOOLS_FFT_HPP

#include <stdint.h>

#include "../namespace.hpp"
#include "fft_types.hpp"

/** @file fft.hpp @brief The interface for the HISSTools FFT.
 
    The FFT is compatible with the FFT routines provided by Apple's vDSP library and can be configured to use this fast FFT when available.
 */

/**
    The NO_NATIVE_FFT preprocessor command instructs the HISSTools FFT to use its own code even if the Apple FFT is available. You must link against the Accelerate framework if this is not defined under Mac OS. The default is to use the Apple FFT.
 */

// Platform check for Apple FFT selection

#if defined __APPLE__ && !defined NO_NATIVE_FFT
#define USE_APPLE_FFT
#endif

HISSTOOLS_NAMESPACE_START()

/**
    split_type is a split of complex data (of a given type) for an FFT stored against two pointers (real and imaginary parts).
 */

template <class T>
struct split_type : impl::TypeBase<T>
{
    split_type() {}
    split_type(T *real, T *imag) : realp(real), imagp(imag) {}
    
    /** A pointer to the real portion of the data */
    T *realp;
    
    /** A pointer to the imaginary portion of the data */
    T *imagp;
};

/**
    setup_type is an opaque structure that holds the twiddle factors for an FFT of up to a given size.
 */

template <class T>
struct setup_type;

// N.B. Include once basic types are defined

#include "fft_core.hpp"

/**
    hisstools_create_setup() creates an FFT setup suitable for FFTs and iFFTs up to a maximum specified size.
 
	@param	setup           A pointer to an uninitialised setup_type<T>.
	@param	max_fft_log_2   The log base 2 of the FFT size of the maimum FFT size you wish to support.
	
	@remark                 On return the object pointed to by setup will be intialsed,
 */

template <class T>
void hisstools_create_setup(setup_type<T> *setup, uintptr_t max_fft_log_2)
{
    hisstools_fft_impl::create_setup(setup->m_setup, max_fft_log_2);
}

/**
    hisstools_destroy_setup() destroys an FFT setup.
 
	@param	setup		A setup_type<T> (setup of type T).
 
	@remark             After calling this routine the setup is destroyed.
 */

template <class T>
void hisstools_destroy_setup(setup_type<T> setup)
{
    hisstools_fft_impl::destroy_setup(setup.m_setup);
}

/**
    hisstools_unzip() performs unzipping prior to an in-place real FFT.
 
    @param    input     A pointer to the real input.
    @param    output    A pointer to a split_type<T> structure to unzip to.
    @param    log2n     The log base 2 of the FFT size.
 
    @remark             Prior to running a real FFT the data must be unzipped from a contiguous memory location into a complex split structure. This function performs the unzipping.
 */

template <class T>
void hisstools_unzip(const T *input, split_type<T> *output, uintptr_t log2n)
{
    hisstools_fft_impl::unzip_complex(input, output, 1U << (log2n - 1U));
}

/**
    hisstools_zip() performs zipping subsequent to an in-place real FFT.
 
    @param    input     A pointer to a split_type<T> structure to zip from.
    @param    output    A pointer to the real output.
    @param    log2n     The log base 2 of the FFT size.
 
    @remark             Subsequent to running a real FFT the data must be zipped from a complex split structue into a contiguous memory location for final output. This function performs the zipping.
 */

template <class T>
void hisstools_zip(const split_type<T> *input, T *output, uintptr_t log2n)
{
    hisstools_fft_impl::zip_complex(input, output, 1U << (log2n - 1U));
}

/**
    hisstools_unzip_zero() performs unzipping and zero-padding prior to an in-place real FFT.
 
    @param    input         A pointer to the real input.
    @param    output        A pointer to a split_type<T> structure to unzip to.
    @param    in_length     The actual length of the input
    @param    log2n         The log base 2 of the FFT size.
 
    @remark                 Prior to running a real FFT the data must be unzipped from a contiguous memory location into a complex split structure. This function performs unzipping, and zero-pads any remaining input for inputs that may not match the length of the FFT. This version allows a floating point input to be unzipped directly to a complex split structure.
 */

// Unzip incorporating zero padding

template <class T, class U>
void hisstools_unzip_zero(const U *input, split_type<T> *output, uintptr_t in_length, uintptr_t log2n)
{
    hisstools_fft_impl::unzip_zero(input, output, in_length, log2n);
}

/**
    hisstools_fft() performs an in-place complex Fast Fourier Transform.
 
	@param	setup		A setup_type<T> that has been created to deal with an appropriate maximum size of FFT.
	@param	input		A pointer to a split_type<T> structure containing the complex input.
	@param	log2n		The log base 2 of the FFT size.
	
	@remark             The FFT may be performed with scalar or SIMD instructions. SIMD instuctions will be used when the pointers within the split_type<float> are sixteen byte aligned.
 */

template <class T>
void hisstools_fft(setup_type<T> setup, split_type<T> *input, uintptr_t log2n)
{
    hisstools_fft_impl::hisstools_fft(input, setup.m_setup, log2n);
}

/**
    hisstools_rfft() performs an in-place real Fast Fourier Transform.
 
	@param	setup		A setup_type<T> that has been created to deal with an appropriate maximum size of FFT.
	@param	input		A pointer to a split_type<T> structure containing a complex input.
	@param	log2n		The log base 2 of the FFT size.
	
	@remark             The FFT may be performed with either scalar or SIMD instructions. SIMD instuctions will be used when the pointers within the split_type<T> are sixteen byte aligned. Note that the input should first be unzipped into the complex input structure using hisstools_unzip() or hisstools_unzip_zero).
 */

template <class T>
void hisstools_rfft(setup_type<T> setup, split_type<T> *input, uintptr_t log2n)
{
    hisstools_fft_impl::hisstools_rfft(input, setup.m_setup, log2n);
}

/**
    hisstools_rfft() performs an out-of-place real Fast Fourier Transform.
 
	@param	setup		A setup_type<float> that has been created to deal with an appropriate maximum size of FFT.
	@param	input		A pointer to a real input
 .	@param	output		A pointer to a a split_type<T> structure which will hold the complex output.
	@param	in_length   The length of the input real array.
	@param	log2n		The log base 2 of the FFT size.
	
	@remark             The FFT may be performed with either scalar or SIMD instructions. SIMD instuctions will be used when the pointers within the split_type<T> are sixteen byte aligned.
 */

template <class T, class U>
void hisstools_rfft(setup_type<T> setup, const U *input, split_type<T> *output, uintptr_t in_length, uintptr_t log2n)
{
    hisstools_unzip_zero(input, output, in_length, log2n);
    hisstools_rfft(setup, output, log2n);
}

/**
    hisstools_ifft() performs an in-place inverse complex Fast Fourier Transform.
 
	@param	setup		A setup_type<T> that has been created to deal with an appropriate maximum size of FFT.
	@param	input		A pointer to a split_type<T> structure containing a complex input.
	@param	log2n		The log base 2 of the FFT size.
	
	@remark             The inverse FFT may be performed with either scalar or SIMD instructions. SIMD instuctions will be used when the pointers within the split_type<T> are sixteen byte aligned.
 */

template <class T>
void hisstools_ifft(setup_type<T> setup, split_type<T> *input, uintptr_t log2n)
{
    hisstools_fft_impl::hisstools_ifft(input, setup.m_setup, log2n);
}

/**
    hisstools_rifft() performs an in-place inverse real Fast Fourier Transform.
 
	@param	setup		A setup_type<T> that has been created to deal with an appropriate maximum size of FFT.
	@param	input		A pointer to a split_type<T> structure containing a complex input.
	@param	log2n		The log base 2 of the FFT size.
	
	@remark             The inverse FFT may be performed with either scalar or SIMD instructions. SIMD instuctions will be used when the pointers within the split_type<T> are sixteen byte aligned. Note that the output will need to be zipped from the complex output structure using hisstools_zip().
 */
 
template <class T>
void hisstools_rifft(setup_type<T> setup, split_type<T> *input, uintptr_t log2n)
{
    hisstools_fft_impl::hisstools_rifft(input, setup.m_setup, log2n);
}

/**
    hisstools_rifft() performs an out-out-place inverse real Fast Fourier Transform.
 
	@param	setup		A setup_type<T> that has been created to deal with an appropriate maximum size of FFT.
	@param	input		A pointer to a split_type<T> structure containing a complex input.
	@param	output		A pointer to a real array to hold the output.
	@param	log2n		The log base 2 of the FFT size.
	
	@remark             The inverse FFT may be performed with either scalar or SIMD instructions. SIMD instuctions will be used when the pointers within the split_type<T> are sixteen byte aligned.
 */

template <class T>
void hisstools_rifft(setup_type<T> setup, split_type<T> *input, T *output, uintptr_t log2n)
{
    hisstools_rifft(setup, input, log2n);
    hisstools_zip(input, output, log2n);
}

HISSTOOLS_NAMESPACE_END()

#endif /* HISSTOOLS_FFT_HPP */
