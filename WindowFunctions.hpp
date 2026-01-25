
/**
 * @file [YourFileName.hpp]  // Replace with the actual file name
 * @brief Provides a collection of window functions and utilities for signal processing.
 *
 * This file contains implementations of various window functions used in signal processing to reduce spectral leakage,
 * such as the Hann, Hamming, Blackman, and Nuttall windows. It also provides flat-top window variants and more advanced
 * window functions like Kaiser and cosine windows. These functions are used to generate window values for a given signal
 * over a specified range of indices.
 *
 * In addition to the window functions, the file defines a flexible and extensible framework for selecting and applying
 * different window generators dynamically using indexed generators and template programming.
 *
 * The main components of this file include:
 * - Definitions of `params` struct for parameterizing window functions.
 * - Template-based window generators for different types of windows.
 * - Utilities for managing multiple window generators, such as `indexed_generator`.
 *
 * The window functions provided in this file are widely used in signal processing, especially in spectral analysis, filter design,
 * and applications that require minimizing spectral leakage while preserving amplitude accuracy.
 *
 * @note Ensure that the appropriate template parameters are used when selecting and applying window functions.
 */

#ifndef WINDOWFUNCTIONS_HPP
#define WINDOWFUNCTIONS_HPP

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>
#include <vector>

// Coefficients (and the basis for naming) can largely be found in:
//
// Nuttall, A. (1981). Some windows with very good sidelobe behavior.
// IEEE Transactions on Acoustics, Speech, and Signal Processing, 29(1), 84-91.
//
// Similar windows / additional flat-top windows from:
//
// Heinzel, G., Rüdiger, A., & Schilling, R. (2002).
// Spectrum and spectral density estimation by the Discrete Fourier transform (DFT),
// including a comprehensive list of window functions and some new flat-top windows.


/**
 * @namespace window_functions
 *
 * @brief Contains various windowing functions and related utilities.
 *
 * This namespace encapsulates a collection of windowing functions used in signal processing.
 * Windowing functions are typically applied to signals in order to reduce spectral leakage during analysis.
 *
 * The functions and types within this namespace are designed to generate different types of window functions
 * such as Hanning, Hamming, Blackman, and others, which can be used in various signal processing algorithms.
 */

namespace window_functions
{
    // Parameter struct
    
    /**
     * @struct params
     *
     * @brief Holds parameters required for window function calculations.
     *
     * This structure encapsulates additional parameters that are needed by various window functions.
     * The specific contents of the structure may vary depending on the type of window function being used.
     * Typical parameters might include coefficients or constants that affect the shape and behavior of the window.
     *
     * The `params` struct is used as an input argument in many window function calculations
     * within the `window_functions` namespace.
     */

    struct params
    {
        
        /**
         * @brief Constructs a `params` object with the provided coefficients and exponent.
         *
         * This constructor initializes the `params` structure with up to five coefficients (a0 through a4) and an exponent.
         * These values are typically used in window function calculations to control the shape and characteristics of the window.
         *
         * @param[in] A0 Coefficient a0, defaults to 0.
         * @param[in] A1 Coefficient a1, defaults to 0.
         * @param[in] A2 Coefficient a2, defaults to 0.
         * @param[in] A3 Coefficient a3, defaults to 0.
         * @param[in] A4 Coefficient a4, defaults to 0.
         * @param[in] exp Exponent used in the window function, defaults to 1.
         */
        
        constexpr params(double A0 = 0, double A1 = 0, double A2 = 0, double A3 = 0, double A4 = 0, double exp = 1)
        : a0(A0), a1(A1), a2(A2), a3(A3), a4(A4), exponent(exp) {}
        
        /**
         * @brief Constructs a `params` object from an array of coefficients and an exponent.
         *
         * This constructor initializes the `params` structure using values from a provided array of coefficients.
         * The array may contain up to five values, which are assigned to a0 through a4, respectively.
         * If fewer than five values are provided, the remaining coefficients are initialized to 0.0.
         * The exponent is set using the provided `exp` parameter.
         *
         * @param[in] param_array A pointer to an array of coefficients. The array may contain up to five elements.
         * @param[in] N The number of coefficients in the array. Only the first five elements will be used.
         * @param[in] exp The exponent used in the window function.
         */
        
        constexpr params(const double *param_array, int N, double exp)
        : a0(N > 0 ? param_array[0] : 0.0)
        , a1(N > 1 ? param_array[1] : 0.0)
        , a2(N > 2 ? param_array[2] : 0.0)
        , a3(N > 3 ? param_array[3] : 0.0)
        , a4(N > 4 ? param_array[4] : 0.0)
        , exponent(exp) {}
     
        /**
         * @brief The first coefficient used in window function calculations.
         *
         * This variable represents the first coefficient (a0) in a window function.
         * It is typically used to control the shape and behavior of the window,
         * and its value is either provided during initialization or defaults to 0.0.
         */
        
        double a0;
        
        /**
         * @brief The second coefficient used in window function calculations.
         *
         * This variable represents the second coefficient (a1) in a window function.
         * It is typically used alongside other coefficients to influence the shape and characteristics
         * of the windowing function. The value of this coefficient can be set during initialization or defaults to 0.0.
         */
        
        double a1;
        
        /**
         * @brief The third coefficient used in window function calculations.
         *
         * This variable represents the third coefficient (a2) in a window function.
         * It works with other coefficients to define the shape of the windowing function.
         * The value of this coefficient can be set during initialization or defaults to 0.0 if not provided.
         */
        
        double a2;
        
        /**
         * @brief The fourth coefficient used in window function calculations.
         *
         * This variable represents the fourth coefficient (a3) in a window function.
         * It works with other coefficients to define the shape of the windowing function.
         * The value of this coefficient can be set during initialization or defaults to 0.0 if not provided.
         */
        
        double a3;
        
        /**
         * @brief The fifth coefficient used in window function calculations.
         *
         * This variable represents the fifth coefficient (a4) in a window function.
         * It works with other coefficients to define the shape of the windowing function.
         * The value of this coefficient can be set during initialization or defaults to 0.0 if not provided.
         */
        double a4;
        
        /**
         * @brief The exponent used in the window function calculation.
         *
         * This variable represents the exponent applied in the window function,
         * typically used to adjust the shape or tapering characteristics of the window.
         * A higher exponent may produce sharper transitions, depending on the specific window function in use.
         * The value of the exponent can be set during initialization or defaults to 1.0.
         */
        
        double exponent;
    };
    
    /**
     * @typedef window_func
     *
     * @brief Defines a function pointer type for window functions.
     *
     * This type represents a function that generates a windowing function value.
     *
     * @param[in] arg1 The first parameter of type uint32_t, typically representing the current sample index.
     * @param[in] arg2 The second parameter of type uint32_t, typically representing the total number of samples or size of the window.
     * @param[in] arg3 A constant reference to a `params` object, typically containing additional parameters required by the window function.
     *
     * @return A double representing the calculated window function value for the given sample.
     */

    using window_func = double(uint32_t, uint32_t, const params&);

    /**
     * @typedef window_generator
     *
     * @brief Defines a template type for a window generator function.
     *
     * This type represents a function that generates a windowing function over an array of elements of type `T`.
     *
     * @tparam T The data type of the elements for which the window function is applied (e.g., float, double).
     *
     * @param[out] array A pointer to an array of type `T` where the window function will be applied.
     * @param[in] size The size of the array (number of elements) to which the window function is applied.
     * @param[in] window_size The size of the window being applied, typically representing the number of samples.
     * @param[in] total_size The total number of elements or samples, usually used in window function scaling.
     * @param[in] params A constant reference to the `params` struct containing parameters required for the window function.
     */

    template <class T>
    using window_generator = void(T*, uint32_t, uint32_t, uint32_t, const params&);
    
    /**
     * @namespace impl
     *
     * @brief Contains internal implementation details for window functions.
     *
     * The `impl` namespace is used to encapsulate lower-level helper functions and implementation-specific
     * details related to window functions. The contents of this namespace are not intended to be directly
     * accessed by users of the public API but are used internally by the library to implement window generation
     * and related functionality.
     */

    namespace impl
    {
        // Constexpr functions for convenience
        
        /**
         * @brief Returns the mathematical constant π (pi).
         *
         * This function returns the value of pi (π) as defined by the macro `M_PI`.
         * It is used in various mathematical and signal processing calculations, particularly in window functions and other operations involving angles or periodic functions.
         *
         * @return A `double` representing the value of π (approximately 3.14159).
         */
        
        constexpr double pi()   { return M_PI; }
        
        /**
         * @brief Returns the value of 2π (tau).
         *
         * This function returns the value of 2π, also known as tau, which is often used in trigonometric and signal processing calculations.
         * It is commonly used in scenarios involving full rotations or periodic functions, where a complete cycle is represented by 2π radians.
         *
         * @return A `double` representing the value of 2π (approximately 6.28318).
         */
        
        constexpr double pi2()  { return M_PI * 2.0; }
        
        /**
         * @brief Returns the value of 4π.
         *
         * This function returns the value of 4π, which may be used in signal processing or mathematical calculations involving multiples of π.
         * It is particularly useful in cases where calculations involve two full rotations or specific periodic functions.
         *
         * @return A `double` representing the value of 4π (approximately 12.56637).
         */
        
        constexpr double pi4()  { return M_PI * 4.0; }
    
        /**
         * @brief Returns the value of 6π.
         *
         * This function returns the value of 6π, which can be used in mathematical or signal processing calculations that involve multiple rotations or periodic functions.
         * It is particularly relevant in scenarios requiring three full rotations (6π radians).
         *
         * @return A `double` representing the value of 6π (approximately 18.84956).
         */
        
        constexpr double pi6()  { return M_PI * 6.0; }
    
        /**
         * @brief Returns the value of 8π.
         *
         * This function returns the value of 8π, which may be used in mathematical or signal processing calculations involving multiple rotations or periodic functions.
         * It is particularly useful in situations requiring four full rotations (8π radians).
         *
         * @return A `double` representing the value of 8π (approximately 25.13274).
         */
        
        constexpr double pi8()  { return M_PI * 8.0; }
        
        /**
         * @brief Performs division of two integers and returns the result as a double.
         *
         * This function divides the integer `x` by the integer `y` and returns the result as a `double` to preserve the precision of the division.
         *
         * @param[in] x The dividend, an integer value.
         * @param[in] y The divisor, an integer value. It is expected that `y` is non-zero.
         *
         * @return A `double` representing the result of dividing `x` by `y`.
         *
         * @note If `y` is zero, the behavior is undefined. It is recommended to ensure `y` is non-zero before calling this function.
         */
        
        constexpr double div(int x, int y)
        {
            return static_cast<double>(x) / static_cast<double>(y);
        }
        
        // Operators for cosine sum windows
        
        /**
         * @struct constant
         *
         * @brief Represents a collection of mathematical constants.
         *
         * This structure is designed to encapsulate various mathematical constants that may be used throughout
         * the codebase, particularly in window function calculations or other signal processing operations.
         * The `constant` struct provides a centralized location to access frequently used constants such as pi, 2π, 4π, etc.
         */
        
        struct constant
        {
            
            /**
             * @brief Constructs a `constant` object with a specified value.
             *
             * This constructor initializes the `constant` struct with a given double value.
             * The value is stored in the `value` member variable and represents a mathematical constant
             * that can be used in calculations.
             *
             * @param[in] v The value of the constant to be stored.
             */
            
            constant(double v) : value(v) {}
            
            /**
             * @brief Function call operator that returns the stored constant value.
             *
             * This operator overload allows an instance of the `constant` struct to be used as a function, returning the stored constant value.
             * The input parameter `x` is ignored, as the function always returns the same constant value.
             *
             * @param[in] x A double value (ignored by this function).
             *
             * @return The stored constant value as a `double`.
             */
            
            inline double operator()(double x) { return value; }
            
            /**
             * @brief Holds the constant value associated with this instance.
             *
             * This member variable stores the immutable double precision constant value for the `constant` struct.
             * It is set during initialization and cannot be modified afterwards.
             * The value is used in calculations or returned directly through the function call operator.
             */
            
            const double value;
        };
        
        /**
         * @struct cosx
         *
         * @brief Represents a cosine-based window function.
         *
         * The `cosx` struct is designed to generate or manipulate values based on the cosine function.
         * It can be used in signal processing or windowing applications where cosine modulation is required.
         * This structure typically provides a way to apply a cosine function to input values for tasks such as windowing or smoothing.
         */
        
        struct cosx
        {
            
            /**
             * @brief Constructs a `cosx` object with a specified coefficient and multiplier.
             *
             * This constructor initializes the `cosx` struct with a given coefficient and multiplier, which are typically used in cosine-based calculations.
             * The coefficient and multiplier modify the behavior of the cosine function, affecting the scaling and amplitude of the result.
             *
             * @param[in] c The coefficient applied to the cosine function.
             * @param[in] mul The multiplier applied to scale the result of the cosine function.
             */
            
            cosx(double c, double mul)
            : coefficient(c), multiplier(mul) {}
            
            /**
             * @brief Function call operator that applies a cosine-based calculation.
             *
             * This operator overload allows an instance of the `cosx` struct to be used as a function.
             * It takes a `double` input `x`, applies the cosine function, and scales the result based on the coefficient and multiplier.
             *
             * @param[in] x The input value to which the cosine function will be applied.
             *
             * @return A `double` representing the result of the cosine-based calculation, scaled by the coefficient and multiplier.
             */
            
            inline double operator()(double x)
            {
                return coefficient * cos(x * multiplier);
            }
            
            /**
             * @brief The coefficient applied to the cosine function.
             *
             * This member variable holds a constant `double` value that serves as the coefficient in the cosine-based calculation.
             * It adjusts the scaling of the cosine function and is set during the initialization of the `cosx` struct.
             * Once initialized, the coefficient cannot be modified.
             */
            
            const double coefficient;
            
            /**
             * @brief The multiplier applied to scale the result of the cosine function.
             *
             * This member variable holds a constant `double` value that scales the result of the cosine function.
             * It is used to adjust the amplitude of the output in the `cosx` struct's calculations.
             * The multiplier is set during initialization and remains immutable thereafter.
             */
            
            const double multiplier;
        };
        
        // Normalisation helper
        
        /**
         * @brief Normalizes a value based on the total number of elements.
         *
         * This static inline function normalizes the input value `x` by dividing it by `N`,
         * typically representing a total number of elements or samples. The result is a double
         * representing the relative position of `x` within the range `[0, N)`, where `N` is the total size.
         *
         * @param[in] x The value to be normalized, typically representing a current sample or index.
         * @param[in] N The total number of elements or samples.
         *
         * @return A `double` representing the normalized value of `x` in the range [0, 1].
         */
        
        static inline double normalise(uint32_t x, uint32_t N)
        {
            return static_cast<double>(x) / static_cast<double>(N);
        }
        
        // Summing functions for cosine sum windows
        
        /**
         * @brief Computes a sum based on an operation applied to the input.
         *
         * This template function takes a `double` input `x` and applies a user-defined operation `op` of type `T` to it.
         * The result of the operation is added to the sum, and the final result is returned.
         * The function allows flexible behavior depending on the provided operation `op`.
         *
         * @tparam T The type of the operation or functor applied to `x`.
         *
         * @param[in] x The input value of type `double` to which the operation will be applied.
         * @param[in] op The operation or functor of type `T` to be applied to `x`.
         *
         * @return A `double` representing the result of the sum after applying the operation to `x`.
         */
        
        template <typename T>
        inline double sum(double x, T op)
        {
            return op(x);
        }
        
        /**
         * @brief Recursively computes a sum based on multiple operations applied to the input.
         *
         * This template function applies a series of operations to the input value `x`.
         * The first operation `op` is applied to `x`, and then the function recursively applies the remaining operations (`ops...`)
         * until all operations have been processed. The results of these operations are added together to compute the final sum.
         *
         * @tparam T The type of the first operation or functor applied to `x`.
         * @tparam Ts The types of additional operations or functors to be applied to `x`.
         *
         * @param[in] x The input value of type `double` to which the operations will be applied.
         * @param[in] op The first operation or functor of type `T` to be applied to `x`.
         * @param[in] ops A variadic pack of additional operations or functors to be applied to `x`.
         *
         * @return A `double` representing the result of the sum after applying all the operations to `x`.
         */
        
        template <typename T, typename ...Ts>
        inline double sum(double x, T op, Ts... ops)
        {
            return sum(x, op) + sum(x, ops...);
        }
        
        /**
         * @brief Computes a sum by applying multiple operations to a normalized value.
         *
         * This template function applies a series of operations to the normalized value of `i` relative to `N`.
         * The index `i` is normalized by dividing it by `N`, and the resulting value is passed to each operation in the
         * variadic parameter pack `ops`. The results of these operations are added together to compute the final sum.
         *
         * @tparam Ts The types of the operations or functors to be applied to the normalized value.
         *
         * @param[in] i The current index or position to be normalized, typically representing the sample or iteration index.
         * @param[in] N The total number of elements or samples, used for normalization.
         * @param[in] ops A variadic pack of operations or functors to be applied to the normalized value of `i/N`.
         *
         * @return A `double` representing the sum of the results after applying all the operations to the normalized value.
         */
        
        template <typename ...Ts>
        inline double sum(uint32_t i, uint32_t N, Ts... ops)
        {
            return sum(normalise(i, N), ops...);
        }
        
        // Specific window calculations
        
        /**
         * @brief Generates a rectangular (boxcar) window function value.
         *
         * This function computes a value for a rectangular (boxcar) window function at the given index `i`.
         * The window function value is typically 1 for all values of `i` within the range [0, N) and represents
         * a simple windowing technique in signal processing.
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, although it is typically unused in a rectangular window.
         *
         * @return A `double` representing the rectangular window function value, which is typically 1 for all valid `i` values.
         */
        
        inline double rect(uint32_t i, uint32_t N, const params& p)
        {
            return 1.0;
        }
        
        /**
         * @brief Generates a triangular window function value.
         *
         * This function computes a value for a triangular window function at the given index `i`.
         * The triangular window is symmetric and tapers linearly towards zero at the edges.
         * It is often used in signal processing to reduce spectral leakage with smoother transitions.
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, although it is typically unused in a standard triangular window.
         *
         * @return A `double` representing the triangular window function value for the given index `i`.
         */
        
        inline double triangle(uint32_t i, uint32_t N, const params& p)
        {
            return 1.0 - fabs(normalise(i, N) * 2.0 - 1.0);
        }
            
        /**
         * @brief Generates a trapezoidal window function value.
         *
         * This function computes a value for a trapezoidal window function at the given index `i`.
         * The trapezoidal window is characterized by a flat section in the center and linear tapers towards zero
         * at both ends, offering a balance between the rectangular and triangular window functions in terms of smoothness.
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, which may contain parameters for adjusting the shape of the trapezoidal window.
         *
         * @return A `double` representing the trapezoidal window function value for the given index `i`.
         */
        
        inline double trapezoid(uint32_t i, uint32_t N, const params& p)
        {
            double a = p.a0;
            double b = p.a1;
            
            if (b < a)
                std::swap(a, b);
            
            const double x = normalise(i, N);
            
            if (x < a)
                return x / a;
            
            if (x > b)
                return 1.0 - ((x - b) / (1.0 - b));
            
            return 1.0;
        }
        
        /**
         * @brief Generates a Welch window function value.
         *
         * This function computes a value for the Welch window function at the given index `i`.
         * The Welch window is a parabolic window that is often used in signal processing to reduce spectral leakage.
         * The window tapers to zero at the edges and has a smooth, curved shape that minimizes the effect of sharp transitions.
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, although it is typically unused in a standard Welch window.
         *
         * @return A `double` representing the Welch window function value for the given index `i`.
         */
        
        inline double welch(uint32_t i, uint32_t N, const params& p)
        {
            const double x = 2.0 * normalise(i, N) - 1.0;
            return 1.0 - x * x;
        }
        
        /**
         * @brief Generates a Parzen window function value.
         *
         * This function computes a value for the Parzen window function at the given index `i`.
         * The Parzen window is a tapered, smooth window that reduces spectral leakage by applying
         * a non-linear taper towards the edges, similar to other window functions like the Gaussian or Hann windows.
         * It is commonly used in signal processing applications for smoothing and reducing edge effects.
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, which may contain additional parameters affecting the shape of the window.
         *
         * @return A `double` representing the Parzen window function value for the given index `i`.
         */
        
        inline double parzen(uint32_t i, uint32_t N, const params& p)
        {
            const double N2 = static_cast<double>(N) * 0.5;
            
            auto w0 = [&](double x)
            {
                x = fabs(x) / N2;
                
                if (x > 0.5)
                {
                    double v = (1.0 - x);
                    return 2.0 * v * v * v;
                }
                
                return 1.0 - 6.0 * x * x * (1.0 - x);
            };
            
            return w0(static_cast<double>(i) - N2);
        }
        
        /**
         * @brief Generates a sine window function value.
         *
         * This function computes a value for the sine window function at the given index `i`.
         * The sine window is a smooth windowing function where the values follow a sine curve from 0 to π.
         * It is often used in signal processing to taper the edges of a signal and reduce spectral leakage.
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, although it is typically unused in a standard sine window function.
         *
         * @return A `double` representing the sine window function value for the given index `i`.
         */
        
        inline double sine(uint32_t i, uint32_t N, const params& p)
        {
            return sin(pi() * normalise(i, N));
        }
        
        /**
         * @brief Generates a sine taper window function value.
         *
         * This function computes a value for a sine taper window function at the given index `i`.
         * The sine taper window applies a tapering effect to the signal, using a sine function to smoothly transition
         * the signal's amplitude at the edges. This tapering reduces spectral leakage and is useful in signal processing
         * for smoothing transitions.
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, which may contain additional parameters to adjust the tapering effect.
         *
         * @return A `double` representing the sine taper window function value for the given index `i`.
         */
        
        inline double sine_taper(uint32_t i, uint32_t N, const params& p)
        {
            return sin(p.a0 * pi() * normalise(i, N));
        }
            
        /**
         * @brief Generates a Tukey window function value.
         *
         * This function computes a value for the Tukey window function at the given index `i`.
         * The Tukey window is a combination of a rectangular window and a tapered cosine window,
         * controlled by the parameters in `p`. It offers flexibility in adjusting the trade-off between
         * a flat passband and smooth transitions at the edges, making it useful in reducing spectral leakage
         * in signal processing applications.
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, which may contain parameters to adjust the tapering ratio of the Tukey window.
         *
         * @return A `double` representing the Tukey window function value for the given index `i`.
         */
        
        inline double tukey(uint32_t i, uint32_t N, const params& p)
        {
            return 0.5 - 0.5 * cos(trapezoid(i, N, p) * pi());
        }
        
        /**
         * @brief Computes the modified zeroth-order Bessel function of the first kind.
         *
         * This function calculates an approximation of the modified zeroth-order Bessel function of the first kind, commonly denoted as `I0(x)`.
         * The function is frequently used in signal processing and windowing techniques, such as the Kaiser window, to generate smooth tapers.
         *
         * @param[in] x2 The input value (typically the square of a variable `x`) for which the Bessel function will be evaluated.
         *
         * @return A `double` representing the computed value of the modified zeroth-order Bessel function.
         */
        
        inline double izero(double x2)
        {
            double term = 1.0;
            double bessel = 1.0;
            
            // N.B. - loop based on epsilon for maximum accuracy
            
            for (unsigned long i = 1; term > std::numeric_limits<double>::epsilon(); i++)
            {
                const double i2 = static_cast<double>(i * i);
                term = term * x2 * (1.0 / (4.0 * i2));
                bessel += term;
            }
            
            return bessel;
        }
        
        /**
         * @brief Generates a Kaiser window function value.
         *
         * This function computes a value for the Kaiser window function at the given index `i`.
         * The Kaiser window is based on the zeroth-order modified Bessel function and provides a smooth taper
         * for signal processing, effectively balancing main lobe width and side lobe level to control spectral leakage.
         * The shape of the window can be adjusted using the parameters in `p`, particularly the beta parameter.
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, which contains parameters like the beta value to control the shape of the Kaiser window.
         *
         * @return A `double` representing the Kaiser window function value for the given index `i`.
         */
        
        inline double kaiser(uint32_t i, uint32_t N,  const params& p)
        {
            double x = 2.0 * normalise(i, N) - 1.0;
            return izero((1.0 - x * x) * p.a0 * p.a0) * p.a1;
        }
        
        /**
         * @brief Generates a two-term cosine window function value.
         *
         * This function computes a value for a two-term cosine window function at the given index `i`.
         * The two-term cosine window is commonly used in signal processing for windowing applications and involves
         * the weighted sum of two cosine terms, offering a balance between main lobe width and side lobe attenuation.
         * The coefficients for the two terms are typically provided via the `params` structure.
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, which contains the coefficients for the two cosine terms.
         *
         * @return A `double` representing the two-term cosine window function value for the given index `i`.
         */
        
        inline double cosine_2_term(uint32_t i, uint32_t N, const params& p)
        {
            return sum(i, N, constant(p.a0), cosx(-(1.0 - p.a0), pi2()));
        }
        
        /**
         * @brief Generates a three-term cosine window function value.
         *
         * This function computes a value for a three-term cosine window function at the given index `i`.
         * The three-term cosine window is a weighted sum of three cosine terms, providing greater flexibility
         * in shaping the window for signal processing applications. It can offer better control over the trade-off
         * between main lobe width and side lobe suppression.
         *
         * The coefficients for the three cosine terms are supplied via the `params` structure.
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, which contains the coefficients for the three cosine terms.
         *
         * @return A `double` representing the three-term cosine window function value for the given index `i`.
         */
        
        inline double cosine_3_term(uint32_t i, uint32_t N, const params& p)
        {
            return sum(i, N, constant(p.a0), cosx(-p.a1, pi2()), cosx(p.a2, pi4()));
        }
        
        /**
         * @brief Generates a four-term cosine window function value.
         *
         * This function computes a value for a four-term cosine window function at the given index `i`.
         * The four-term cosine window is a weighted sum of four cosine terms, allowing fine control over the
         * window's shape, offering a more complex trade-off between main lobe width and side lobe suppression.
         * This window is particularly useful in signal processing applications where enhanced spectral resolution
         * and reduced leakage are required.
         *
         * The coefficients for the four cosine terms are provided through the `params` structure.
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, which contains the coefficients for the four cosine terms.
         *
         * @return A `double` representing the four-term cosine window function value for the given index `i`.
         */
        
        inline double cosine_4_term(uint32_t i, uint32_t N, const params& p)
        {
            return sum(i, N, constant(p.a0), cosx(-p.a1, pi2()), cosx(p.a2, pi4()), cosx(-p.a3, pi6()));
        }
        
        /**
         * @brief Generates a five-term cosine window function value.
         *
         * This function computes a value for a five-term cosine window function at the given index `i`.
         * The five-term cosine window is a weighted sum of five cosine terms, providing even finer control over
         * the window shape, optimizing both main lobe width and side lobe suppression. This window is particularly
         * useful in signal processing where high precision and minimal spectral leakage are essential.
         *
         * The coefficients for the five cosine terms are specified in the `params` structure.
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, which contains the coefficients for the five cosine terms.
         *
         * @return A `double` representing the five-term cosine window function value for the given index `i`.
         */
        
        inline double cosine_5_term(uint32_t i, uint32_t N, const params& p)
        {
            return sum(i, N, constant(p.a0),
                       cosx(-p.a1, pi2()),
                       cosx(p.a2, pi4()),
                       cosx(-p.a3, pi6()),
                       cosx(p.a4, pi8()));
        }
        
        /**
         * @brief Generates a Hann window function value.
         *
         * This function computes a value for the Hann window function at the given index `i`.
         * The Hann window is a commonly used window in signal processing that provides smooth tapering at the edges,
         * reducing spectral leakage. It is a specific case of a cosine window where the result is based on a single cosine term.
         *
         * The formula for the Hann window is:
         * \f[
         * w(i) = 0.5 \times \left( 1 - \cos \left( \frac{2\pi i}{N-1} \right) \right)
         * \f]
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, although it is typically unused in the standard Hann window.
         *
         * @return A `double` representing the Hann window function value for the given index `i`.
         */
        
        inline double hann(uint32_t i, uint32_t N, const params& p)
        {
            return cosine_2_term(i, N, params(0.5));
        }
        
        /**
         * @brief Generates a Hamming window function value.
         *
         * This function computes a value for the Hamming window function at the given index `i`.
         * The Hamming window is a popular window used in signal processing to smooth transitions and reduce spectral leakage.
         * It is similar to the Hann window but with a slightly different taper to improve side lobe suppression.
         *
         * The formula for the Hamming window is:
         * \f[
         * w(i) = 0.54 - 0.46 \times \cos \left( \frac{2\pi i}{N-1} \right)
         * \f]
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, although it is typically unused in the standard Hamming window.
         *
         * @return A `double` representing the Hamming window function value for the given index `i`.
         */
        
        inline double hamming(uint32_t i, uint32_t N, const params& p)
        {
            // N.B. here we use approx alpha of 0.54 (not 25/46 or 0.543478260869565)
            // see equiripple notes on wikipedia
            
            return cosine_2_term(i, N, params(0.54));
        }
        
        /**
         * @brief Generates a Blackman window function value.
         *
         * This function computes a value for the Blackman window function at the given index `i`.
         * The Blackman window is a smooth tapering function used in signal processing to reduce spectral leakage,
         * providing better side lobe suppression than the Hann or Hamming windows due to the use of multiple cosine terms.
         * It is ideal for applications where high attenuation of side lobes is required.
         *
         * The formula for the Blackman window is:
         * \f[
         * w(i) = 0.42 - 0.5 \times \cos \left( \frac{2\pi i}{N-1} \right) + 0.08 \times \cos \left( \frac{4\pi i}{N-1} \right)
         * \f]
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, although it is typically unused in the standard Blackman window.
         *
         * @return A `double` representing the Blackman window function value for the given index `i`.
         */
        
        inline double blackman(uint32_t i, uint32_t N, const params& p)
        {
            return cosine_3_term(i, N, params(0.42, 0.5, 0.08));
        }
        
        /**
         * @brief Generates an exact Blackman window function value.
         *
         * This function computes a value for the exact Blackman window function at the given index `i`.
         * The exact Blackman window is a variation of the Blackman window, using precise coefficients for
         * improved side lobe suppression compared to the standard Blackman window. It provides a better balance
         * between main lobe width and side lobe attenuation for applications requiring precise frequency response.
         *
         * The formula for the exact Blackman window is:
         * \f[
         * w(i) = \frac{7938}{18608} - \frac{9240}{18608} \times \cos \left( \frac{2\pi i}{N-1} \right) + \frac{1430}{18608} \times \cos \left( \frac{4\pi i}{N-1} \right)
         * \f]
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, although it is typically unused in the exact Blackman window.
         *
         * @return A `double` representing the exact Blackman window function value for the given index `i`.
         */
        
        inline double exact_blackman(uint32_t i, uint32_t N, const params& p)
        {
            const params pb(div(7938, 18608), div(9240, 18608), div(1430, 18608));
            return cosine_3_term(i, N, pb);
        }
        
        /**
         * @brief Generates a Blackman-Harris 62 dB window function value.
         *
         * This function computes a value for the Blackman-Harris 62 dB window function at the given index `i`.
         * The Blackman-Harris window is a generalization of the Blackman window, using more terms to achieve
         * better side lobe suppression. The 62 dB variant is optimized to provide approximately 62 dB attenuation
         * in the side lobes, making it suitable for applications where low side lobe levels are important.
         *
         * The formula for the Blackman-Harris 62 dB window is:
         * \f[
         * w(i) = 0.44959 - 0.49364 \times \cos \left( \frac{2\pi i}{N-1} \right) + 0.05677 \times \cos \left( \frac{4\pi i}{N-1} \right)
         * \f]
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, although it is typically unused in the standard Blackman-Harris window.
         *
         * @return A `double` representing the Blackman-Harris 62 dB window function value for the given index `i`.
         */
        
        inline double blackman_harris_62dB(uint32_t i, uint32_t N, const params& p)
        {
            return cosine_3_term(i, N, params(0.44959, 0.49364, 0.05677));
        }
        
        /**
         * @brief Generates a Blackman-Harris 71 dB window function value.
         *
         * This function computes a value for the Blackman-Harris 71 dB window function at the given index `i`.
         * The Blackman-Harris window is a type of window function used in signal processing to minimize side lobes,
         * and the 71 dB variant provides approximately 71 dB attenuation in the side lobes, making it well-suited
         * for applications requiring higher suppression of side lobes for better frequency resolution.
         *
         * The formula for the Blackman-Harris 71 dB window is:
         * \f[
         * w(i) = 0.42323 - 0.49755 \times \cos \left( \frac{2\pi i}{N-1} \right) + 0.07922 \times \cos \left( \frac{4\pi i}{N-1} \right)
         * \f]
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, although it is typically unused in the Blackman-Harris 71 dB window.
         *
         * @return A `double` representing the Blackman-Harris 71 dB window function value for the given index `i`.
         */
        
        inline double blackman_harris_71dB(uint32_t i, uint32_t N, const params& p)
        {
            return cosine_3_term(i, N, params(0.42323, 0.49755, 0.07922));
        }
        
        /**
         * @brief Generates a Blackman-Harris 74 dB window function value.
         *
         * This function computes a value for the Blackman-Harris 74 dB window function at the given index `i`.
         * The Blackman-Harris window is widely used in signal processing to reduce spectral leakage,
         * and the 74 dB variant is designed to provide approximately 74 dB attenuation in the side lobes.
         * This higher attenuation makes it ideal for applications where minimizing side lobe interference is critical.
         *
         * The formula for the Blackman-Harris 74 dB window is:
         * \f[
         * w(i) = 0.35875 - 0.48829 \times \cos \left( \frac{2\pi i}{N-1} \right) + 0.14128 \times \cos \left( \frac{4\pi i}{N-1} \right) - 0.01168 \times \cos \left( \frac{6\pi i}{N-1} \right)
         * \f]
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, although it is typically unused in the Blackman-Harris 74 dB window.
         *
         * @return A `double` representing the Blackman-Harris 74 dB window function value for the given index `i`.
         */
        
        inline double blackman_harris_74dB(uint32_t i, uint32_t N, const params& p)
        {
            return cosine_4_term(i, N, params(0.402217, 0.49703, 0.09892, 0.00188));
        }
        
        /**
         * @brief Generates a Blackman-Harris 92 dB window function value.
         *
         * This function computes a value for the Blackman-Harris 92 dB window function at the given index `i`.
         * The Blackman-Harris window is used to reduce spectral leakage in signal processing,
         * and the 92 dB variant offers approximately 92 dB attenuation in the side lobes, making it suitable
         * for applications that require very high side lobe suppression, such as in high-resolution spectral analysis.
         *
         * The formula for the Blackman-Harris 92 dB window is:
         * \f[
         * w(i) = 0.35875 - 0.48829 \times \cos \left( \frac{2\pi i}{N-1} \right) + 0.14128 \times \cos \left( \frac{4\pi i}{N-1} \right) - 0.01168 \times \cos \left( \frac{6\pi i}{N-1} \right)
         * \f]
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, although it is typically unused in the Blackman-Harris 92 dB window.
         *
         * @return A `double` representing the Blackman-Harris 92 dB window function value for the given index `i`.
         */
        
        inline double blackman_harris_92dB(uint32_t i, uint32_t N, const params& p)
        {
            return cosine_4_term(i, N, params(0.35875, 0.48829, 0.14128, 0.01168));
        }
        
        /**
         * @brief Generates a Nuttall 1st derivative 64 dB window function value.
         *
         * This function computes a value for the Nuttall 1st derivative 64 dB window function at the given index `i`.
         * The Nuttall window is a smooth window function used in signal processing to minimize spectral leakage,
         * and the 1st derivative 64 dB variant is optimized to provide approximately 64 dB attenuation in the side lobes.
         * It is commonly used in applications requiring a smooth taper with moderate side lobe suppression.
         *
         * The formula for the Nuttall 1st derivative 64 dB window is a weighted sum of cosine terms:
         * \f[
         * w(i) = 0.355768 - 0.487396 \times \cos \left( \frac{2\pi i}{N-1} \right) + 0.144232 \times \cos \left( \frac{4\pi i}{N-1} \right) - 0.012604 \times \cos \left( \frac{6\pi i}{N-1} \right)
         * \f]
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, although it is typically unused in the Nuttall 1st derivative 64 dB window.
         *
         * @return A `double` representing the Nuttall 1st derivative 64 dB window function value for the given index `i`.
         */
        
        inline double nuttall_1st_64dB(uint32_t i, uint32_t N, const params& p)
        {
            return cosine_3_term(i, N, params(0.40897, 0.5, 0.09103));
        }
            
        /**
         * @brief Generates a Nuttall 1st derivative 93 dB window function value.
         *
         * This function computes a value for the Nuttall 1st derivative 93 dB window function at the given index `i`.
         * The Nuttall window is designed to minimize spectral leakage, and the 1st derivative 93 dB variant offers
         * higher side lobe suppression with approximately 93 dB attenuation. It is commonly used in high-precision
         * signal processing applications where minimal side lobe interference is essential.
         *
         * The formula for the Nuttall 1st derivative 93 dB window is:
         * \f[
         * w(i) = 0.3635819 - 0.4891775 \times \cos \left( \frac{2\pi i}{N-1} \right) + 0.1365995 \times \cos \left( \frac{4\pi i}{N-1} \right) - 0.0106411 \times \cos \left( \frac{6\pi i}{N-1} \right)
         * \f]
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, although it is typically unused in the Nuttall 1st derivative 93 dB window.
         *
         * @return A `double` representing the Nuttall 1st derivative 93 dB window function value for the given index `i`.
         */
        
        inline double nuttall_1st_93dB(uint32_t i, uint32_t N, const params& p)
        {
            return cosine_4_term(i, N, params(0.355768, 0.487396, 0.144232, 0.012604));
        }
        
        /**
         * @brief Generates a Nuttall 3rd derivative 47 dB window function value.
         *
         * This function computes a value for the Nuttall 3rd derivative 47 dB window function at the given index `i`.
         * The Nuttall window is used to reduce spectral leakage, and the 3rd derivative 47 dB variant provides
         * approximately 47 dB attenuation in the side lobes. It is useful in applications where moderate side lobe
         * suppression and smooth tapering are required.
         *
         * The formula for the Nuttall 3rd derivative 47 dB window is a weighted sum of cosine terms:
         * \f[
         * w(i) = 0.4243801 - 0.4973406 \times \cos \left( \frac{2\pi i}{N-1} \right) + 0.0782793 \times \cos \left( \frac{4\pi i}{N-1} \right}
         * \f]
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, although it is typically unused in the Nuttall 3rd derivative 47 dB window.
         *
         * @return A `double` representing the Nuttall 3rd derivative 47 dB window function value for the given index `i`.
         */
        
        inline double nuttall_3rd_47dB(uint32_t i, uint32_t N, const params& p)
        {
            return cosine_3_term(i, N, params(0.375, 0.5, 0.125));
        }
        
        /**
         * @brief Generates a Nuttall 3rd derivative 83 dB window function value.
         *
         * This function computes a value for the Nuttall 3rd derivative 83 dB window function at the given index `i`.
         * The Nuttall window is designed to minimize spectral leakage, and the 3rd derivative 83 dB variant provides
         * approximately 83 dB side lobe attenuation, making it ideal for applications requiring higher suppression
         * of side lobes and smoother transitions.
         *
         * The formula for the Nuttall 3rd derivative 83 dB window is a weighted sum of cosine terms:
         * \f[
         * w(i) = 0.338946 - 0.481973 \times \cos \left( \frac{2\pi i}{N-1} \right) + 0.161054 \times \cos \left( \frac{4\pi i}{N-1} \right) - 0.018027 \times \cos \left( \frac{6\pi i}{N-1} \right)
         * \f]
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, although it is typically unused in the Nuttall 3rd derivative 83 dB window.
         *
         * @return A `double` representing the Nuttall 3rd derivative 83 dB window function value for the given index `i`.
         */
        
        inline double nuttall_3rd_83dB(uint32_t i, uint32_t N, const params& p)
        {
            return cosine_4_term(i, N, params(0.338946, 0.481973, 0.161054, 0.018027));
        }
        
        /**
         * @brief Generates a Nuttall 5th derivative 61 dB window function value.
         *
         * This function computes a value for the Nuttall 5th derivative 61 dB window function at the given index `i`.
         * The Nuttall 5th derivative window is designed to reduce spectral leakage with moderate side lobe suppression.
         * The 61 dB variant provides approximately 61 dB of side lobe attenuation, making it suitable for applications
         * requiring a balance between smooth tapering and side lobe suppression.
         *
         * The formula for the Nuttall 5th derivative 61 dB window is a weighted sum of cosine terms:
         * \f[
         * w(i) = 0.3635819 - 0.4891775 \times \cos \left( \frac{2\pi i}{N-1} \right) + 0.1365995 \times \cos \left( \frac{4\pi i}{N-1} \right) - 0.0106411 \times \cos \left( \frac{6\pi i}{N-1} \right)
         * \f]
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, although it is typically unused in the Nuttall 5th derivative 61 dB window.
         *
         * @return A `double` representing the Nuttall 5th derivative 61 dB window function value for the given index `i`.
         */
        
        inline double nuttall_5th_61dB(uint32_t i, uint32_t N, const params& p)
        {
            return cosine_4_term(i, N, params(0.3125, 0.46875, 0.1875, 0.03125));
        }
        
        /**
         * @brief Generates a Nuttall minimal 71 dB window function value.
         *
         * This function computes a value for the Nuttall minimal 71 dB window function at the given index `i`.
         * The Nuttall minimal window is a variation designed to provide good side lobe suppression with minimal impact on the main lobe width.
         * The 71 dB variant offers approximately 71 dB side lobe attenuation, making it suitable for applications where strong side lobe suppression is required,
         * but with minimal distortion to the main lobe.
         *
         * The formula for the Nuttall minimal 71 dB window is a weighted sum of cosine terms:
         * \f[
         * w(i) = 0.355768 - 0.487396 \times \cos \left( \frac{2\pi i}{N-1} \right) + 0.144232 \times \cos \left( \frac{4\pi i}{N-1} \right) - 0.012604 \times \cos \left( \frac{6\pi i}{N-1} \right)
         * \f]
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, although it is typically unused in the Nuttall minimal 71 dB window.
         *
         * @return A `double` representing the Nuttall minimal 71 dB window function value for the given index `i`.
         */
        
        inline double nuttall_minimal_71dB(uint32_t i, uint32_t N, const params& p)
        {
            return cosine_3_term(i, N, params(0.4243801, 0.4973406, 0.0782793));
        }
        
        /**
         * @brief Generates a Nuttall minimal 98 dB window function value.
         *
         * This function computes a value for the Nuttall minimal 98 dB window function at the given index `i`.
         * The Nuttall minimal window is designed to achieve excellent side lobe suppression with minimal main lobe distortion.
         * The 98 dB variant provides approximately 98 dB of side lobe attenuation, making it ideal for high-precision applications
         * requiring strong suppression of side lobes without significantly affecting the main lobe.
         *
         * The formula for the Nuttall minimal 98 dB window is a weighted sum of cosine terms:
         * \f[
         * w(i) = 0.355768 - 0.487396 \times \cos \left( \frac{2\pi i}{N-1} \right) + 0.144232 \times \cos \left( \frac{4\pi i}{N-1} \right) - 0.012604 \times \cos \left( \frac{6\pi i}{N-1} \right)
         * \f]
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, although it is typically unused in the Nuttall minimal 98 dB window.
         *
         * @return A `double` representing the Nuttall minimal 98 dB window function value for the given index `i`.
         */
        
        inline double nuttall_minimal_98dB(uint32_t i, uint32_t N, const params& p)
        {
            return cosine_4_term(i, N, params(0.3635819, 0.4891775, 0.1365995, 0.0106411));
        }
        
        /**
         * @brief Generates a National Instruments (NI) flat top window function value.
         *
         * This function computes a value for the National Instruments flat top window function at the given index `i`.
         * The flat top window is designed to have a flat passband, making it ideal for applications where amplitude
         * accuracy is critical, such as frequency measurement. The window minimizes ripple in the passband,
         * ensuring that the amplitude of the signal is accurately captured across the frequency spectrum.
         *
         * The formula for the flat top window typically involves a weighted sum of multiple cosine terms.
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, which may contain additional parameters affecting the flat top window's characteristics.
         *
         * @return A `double` representing the National Instruments flat top window function value for the given index `i`.
         */
        
        inline double ni_flat_top(uint32_t i, uint32_t N, const params& p)
        {
            return cosine_3_term(i, N, params(0.2810639, 0.5208972, 0.1980399));
        }
        
        /**
         * @brief Generates a Hewlett-Packard (HP) flat top window function value.
         *
         * This function computes a value for the Hewlett-Packard flat top window function at the given index `i`.
         * The flat top window is designed to offer a flat passband, making it ideal for applications requiring precise
         * amplitude measurements over a wide frequency range. This type of window reduces amplitude distortion and
         * is often used in frequency domain applications where accurate amplitude response is critical.
         *
         * The HP flat top window typically involves a weighted sum of cosine terms designed to flatten the passband.
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, which may contain additional parameters influencing the window shape.
         *
         * @return A `double` representing the Hewlett-Packard flat top window function value for the given index `i`.
         */
        
        inline double hp_flat_top(uint32_t i, uint32_t N, const params& p)
        {
            return cosine_4_term(i, N, params(1.0, 1.912510941, 1.079173272, 0.1832630879));
        }
        
        /**
         * @brief Generates a Stanford flat top window function value.
         *
         * This function computes a value for the Stanford flat top window function at the given index `i`.
         * The flat top window is designed to have a flat passband, ensuring that amplitude measurements are accurate
         * across a wide range of frequencies. This type of window minimizes amplitude distortion, making it suitable
         * for high-precision frequency analysis or applications requiring reliable amplitude measurement.
         *
         * The Stanford flat top window typically uses a weighted sum of cosine terms to achieve a flattened response
         * in the passband, reducing ripple effects and enhancing the amplitude accuracy of the signal.
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, which may contain additional parameters affecting the flat top window's characteristics.
         *
         * @return A `double` representing the Stanford flat top window function value for the given index `i`.
         */
        
        inline double stanford_flat_top(uint32_t i, uint32_t N, const params& p)
        {
            return cosine_5_term(i, N, params(1.0, 1.939, 1.29, 0.388, 0.028));
        }
        
        /**
         * @brief Generates a Heinzel flat top window function value with 70 dB side lobe attenuation.
         *
         * This function computes a value for the Heinzel flat top window function at the given index `i`, optimized
         * for approximately 70 dB side lobe attenuation. The Heinzel flat top window is designed to provide a flat passband,
         * making it suitable for applications requiring accurate amplitude measurements over a range of frequencies.
         * The 70 dB variant offers moderate side lobe suppression, making it a balance between amplitude accuracy and spectral leakage reduction.
         *
         * The Heinzel flat top window typically involves a weighted sum of cosine terms, designed to minimize ripple in the passband and provide flat amplitude response.
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, which may contain additional parameters influencing the window characteristics.
         *
         * @return A `double` representing the Heinzel flat top window function value for the given index `i`.
         */
        
        inline double heinzel_flat_top_70dB(uint32_t i, uint32_t N, const params& p)
        {
            return cosine_4_term(i, N, params(1.0, 1.90796, 1.07349, 0.18199));
        }
        
        /**
         * @brief Generates a Heinzel flat top window function value with 90 dB side lobe attenuation.
         *
         * This function computes a value for the Heinzel flat top window function at the given index `i`, optimized
         * for approximately 90 dB side lobe attenuation. The Heinzel flat top window is designed for precise amplitude
         * measurements over a wide frequency range, minimizing ripple in the passband. The 90 dB variant offers stronger
         * side lobe suppression, making it suitable for applications that require both high amplitude accuracy and significant
         * reduction in spectral leakage.
         *
         * The Heinzel flat top window typically involves a weighted sum of cosine terms to achieve flat amplitude response
         * and high side lobe attenuation.
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, which may contain additional parameters influencing the window's characteristics.
         *
         * @return A `double` representing the Heinzel flat top window function value for the given index `i`.
         */
        
        inline double heinzel_flat_top_90dB(uint32_t i, uint32_t N, const params& p)
        {
            return cosine_5_term(i, N, params(1.0, 1.942604, 1.340318, 0.440811, 0.043097));
        }
        
        /**
         * @brief Generates a Heinzel flat top window function value with 95 dB side lobe attenuation.
         *
         * This function computes a value for the Heinzel flat top window function at the given index `i`, optimized for approximately 95 dB side lobe attenuation.
         * The Heinzel flat top window is designed for applications that require both accurate amplitude measurements and significant suppression of side lobes.
         * The 95 dB variant offers very strong side lobe suppression, making it ideal for high-precision signal processing tasks with minimal spectral leakage.
         *
         * The Heinzel flat top window typically involves a weighted sum of cosine terms that provide a flat passband and high side lobe attenuation, ensuring amplitude accuracy across a wide range of frequencies.
         *
         * @param[in] i The current index, typically representing the sample or iteration index.
         * @param[in] N The total number of samples or elements for the window function.
         * @param[in] p A constant reference to the `params` structure, which may contain additional parameters influencing the window's characteristics.
         *
         * @return A `double` representing the Heinzel flat top window function value for the given index `i`.
         */
        
        inline double heinzel_flat_top_95dB(uint32_t i, uint32_t N, const params& p)
        {
            return cosine_5_term(i, N, params(1.0, 1.9383379, 1.3045202, 0.4028270, 0.0350665));
        }
        
        // Abstract generator
        
        /**
         * @brief Generates a window function over a specified range of elements.
         *
         * This template function generates a window function by applying the specified `Func` over the elements in the array `window`
         * from `begin` to `end` indices. The window can be either symmetric or asymmetric, based on the `symmetric` template parameter.
         * The function calculates the window values using the provided windowing function `Func` and parameters in the `params` structure.
         *
         * @tparam Func The window function to apply (e.g., Hann, Hamming, Blackman).
         * @tparam symmetric A boolean indicating whether the window should be symmetric (`true`) or asymmetric (`false`).
         * @tparam T The type of the elements in the `window` array.
         *
         * @param[out] window A pointer to an array of type `T` where the generated window values will be stored.
         * @param[in] N The total number of samples or elements in the window.
         * @param[in] begin The starting index from which to generate the window function.
         * @param[in] end The ending index up to which the window function will be generated.
         * @param[in] p A constant reference to the `params` structure, which contains any additional parameters required for the window function.
         *
         * @note The range of indices is `[begin, end)` meaning the value at `end` is not included.
         */
        
        template <window_func Func, bool symmetric, class T>
        void inline generate(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
        {
            constexpr int max_int = std::numeric_limits<int>().max();
            
            auto sq = [&](double x) { return x * x; };
            auto cb = [&](double x) { return x * x * x; };
            auto qb = [&](double x) { return sq(sq(x)); };
            auto toType = [](double x) { return static_cast<T>(x); };
            
            const T *copy_first = nullptr;
            const T *copy_last = nullptr;
            T *out_first = nullptr;
            
            end = std::min(N + 1, end);
            begin = std::min(begin, end);
            
            if (symmetric)
            {
                uint32_t M = (N/2) + 1;
                
                if (begin < M && end > M + 1)
                {
                    uint32_t begin_n = M - begin;
                    uint32_t end_n = (end - begin) - begin_n;
                    
                    if (begin_n > end_n)
                    {
                        copy_last = window + (N+1)/2 - begin;
                        copy_first = copy_last - end_n;
                        out_first = window + begin_n;
                        end = M;
                    }
                    else
                    {
                        copy_first = window + (N+1)/2 + 1 - begin;
                        copy_last = copy_first + (begin_n - 1);
                        out_first = window;
                        window += M - (begin + 1);
                        begin = M - 1;
                    }
                }
            }
            
            if (p.exponent == 1.0)
            {
                for (uint32_t i = begin; i < end; i++)
                    *window++ = toType(Func(i, N, p));
            }
            else if (p.exponent == 0.5)
            {
                for (uint32_t i = begin; i < end; i++)
                    *window++ = toType(std::sqrt(Func(i, N, p)));
            }
            else if (p.exponent == 2.0)
            {
                for (uint32_t i = begin; i < end; i++)
                    *window++ = toType(sq(Func(i, N, p)));
            }
            else if (p.exponent == 3.0)
            {
                for (uint32_t i = begin; i < end; i++)
                    *window++ = toType(cb(Func(i, N, p)));
            }
            else if (p.exponent == 4.0)
            {
                for (uint32_t i = begin; i < end; i++)
                    *window++ = toType(qb(Func(i, N, p)));
            }
            else if (p.exponent > 0 && p.exponent <= max_int && p.exponent == std::floor(p.exponent))
            {
                int exponent = static_cast<int>(p.exponent);
                
                for (uint32_t i = begin; i < end; i++)
                    *window++ = toType(std::pow(Func(i, N, p), exponent));
            }
            else
            {
                for (uint32_t i = begin; i < end; i++)
                    *window++ = toType(std::pow(Func(i, N, p), p.exponent));
            }
            
            if (symmetric && out_first)
                std::reverse_copy(copy_first, copy_last, out_first);
        }
    }
    
    // Specific window generators
    
    /**
     * @brief Fills an array with values from a rectangular (boxcar) window function.
     *
     * This template function generates a rectangular (boxcar) window over a specified range of elements in the `window` array.
     * The rectangular window assigns a constant value (typically 1) to all elements within the specified range `[begin, end)`.
     * It is commonly used in signal processing where no tapering is required at the edges.
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the rectangular window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the rectangular window function.
     * @param[in] end The ending index up to which the rectangular window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, although it is typically unused in the rectangular window.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void rect(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::rect, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a triangular window function.
     *
     * This template function generates a triangular window over a specified range of elements in the `window` array.
     * The triangular window linearly tapers from zero at the edges to a peak in the middle of the window, creating a symmetric triangular shape.
     * It is commonly used in signal processing to reduce spectral leakage with smoother transitions compared to a rectangular window.
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the triangular window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the triangular window function.
     * @param[in] end The ending index up to which the triangular window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, although it is typically unused in the standard triangular window.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void triangle(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::triangle, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a trapezoidal window function.
     *
     * This template function generates a trapezoidal window over a specified range of elements in the `window` array.
     * The trapezoidal window features a flat section in the center with linear tapers at the edges, creating a shape that
     * combines aspects of both rectangular and triangular windows. This window is useful in signal processing for reducing
     * spectral leakage while retaining a flat section for more consistent amplitude within a given range.
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the trapezoidal window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the trapezoidal window function.
     * @param[in] end The ending index up to which the trapezoidal window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, which may contain additional parameters for adjusting the shape of the trapezoidal window.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void trapezoid(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::trapezoid, false>(window, N, begin, end, p);
    }
        
    /**
     * @brief Fills an array with values from a Welch window function.
     *
     * This template function generates a Welch window over a specified range of elements in the `window` array.
     * The Welch window is a parabolic window that tapers smoothly from the center of the window to the edges,
     * creating a shape that reduces spectral leakage while maintaining smooth transitions.
     * It is commonly used in signal processing applications where gradual edge tapering is preferred.
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the Welch window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the Welch window function.
     * @param[in] end The ending index up to which the Welch window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, although it is typically unused in the standard Welch window.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void welch(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::welch, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a Parzen window function.
     *
     * This template function generates a Parzen window over a specified range of elements in the `window` array.
     * The Parzen window, also known as the de la Vallée Poussin window, is a smooth, bell-shaped window used in
     * signal processing to reduce spectral leakage. It is characterized by a gradual tapering of the window function,
     * which helps to minimize discontinuities at the boundaries.
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the Parzen window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the Parzen window function.
     * @param[in] end The ending index up to which the Parzen window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, although it is typically unused in the Parzen window.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void parzen(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::parzen, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a sine window function.
     *
     * This template function generates a sine window over a specified range of elements in the `window` array.
     * The sine window follows a half-cycle of a sine wave, starting from zero and increasing to its maximum value in the middle,
     * then tapering back to zero at the end. It is often used in signal processing to reduce spectral leakage while preserving
     * smooth transitions at the boundaries.
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the sine window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the sine window function.
     * @param[in] end The ending index up to which the sine window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, although it is typically unused in the sine window.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void sine(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::sine, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a sine taper window function.
     *
     * This template function generates a sine taper window over a specified range of elements in the `window` array.
     * The sine taper window applies a gradual tapering effect to the edges of the signal, using a sine function to smooth the transitions
     * at the start and end of the window. This helps reduce spectral leakage while maintaining the amplitude in the middle portion of the window.
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the sine taper window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the sine taper window function.
     * @param[in] end The ending index up to which the sine taper window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, which may contain additional parameters influencing the tapering effect.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void sine_taper(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        params p1(std::round(p.a0));
        p1.exponent = p.exponent;
        
        impl::generate<impl::sine_taper, false>(window, N, begin, end, p1);
    }
    
    /**
     * @brief Fills an array with values from a Tukey window function.
     *
     * This template function generates a Tukey window over a specified range of elements in the `window` array.
     * The Tukey window, also known as the tapered cosine window, is a combination of a rectangular window and a cosine taper.
     * It is characterized by flat sections in the middle with tapered, cosine-shaped edges. The proportion of the taper
     * is controlled via parameters provided in the `params` structure.
     *
     * The Tukey window is useful in applications where both smooth transitions and flat regions are desired, offering
     * a flexible trade-off between side lobe suppression and spectral leakage.
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the Tukey window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the Tukey window function.
     * @param[in] end The ending index up to which the Tukey window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, which may contain additional parameters like the tapering ratio.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void tukey(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        params p1(p.a0 * 0.5, 1.0 - (p.a0 * 0.5));
        p1.exponent = p.exponent;
        
        impl::generate<impl::tukey, true>(window, N, begin, end, p1);
    }
    
    /**
     * @brief Fills an array with values from a Kaiser window function.
     *
     * This template function generates a Kaiser window over a specified range of elements in the `window` array.
     * The Kaiser window is a flexible window function that uses a parameter (beta) to control the trade-off between
     * main lobe width and side lobe attenuation. The window is based on the modified zeroth-order Bessel function,
     * providing smooth tapering at the edges with adjustable characteristics via the `params` structure.
     *
     * The Kaiser window is commonly used in signal processing where precise control over spectral leakage is required.
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the Kaiser window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the Kaiser window function.
     * @param[in] end The ending index up to which the Kaiser window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, which should contain the `beta` value to adjust the window shape.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void kaiser(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        params p1(p.a0, 1.0 / impl::izero(p.a0 * p.a0));
        p1.exponent = p.exponent;
        
        impl::generate<impl::kaiser, true>(window, N, begin, end, p1);
    }
    
    /**
     * @brief Fills an array with values from a two-term cosine window function.
     *
     * This template function generates a two-term cosine window over a specified range of elements in the `window` array.
     * The two-term cosine window is a weighted sum of two cosine terms, providing a balance between main lobe width and side lobe suppression.
     * It is commonly used in signal processing to reduce spectral leakage while offering smoother transitions at the edges.
     *
     * The weights for the two cosine terms are provided via the `params` structure, which allows customization of the window shape.
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the two-term cosine window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the two-term cosine window function.
     * @param[in] end The ending index up to which the two-term cosine window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, which should contain the weights for the two cosine terms.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void cosine_2_term(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::cosine_2_term, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a three-term cosine window function.
     *
     * This template function generates a three-term cosine window over a specified range of elements in the `window` array.
     * The three-term cosine window is a weighted sum of three cosine terms, providing enhanced control over the shape of the window.
     * It offers better side lobe suppression compared to a two-term cosine window, making it suitable for applications where
     * higher precision in spectral analysis is needed.
     *
     * The coefficients for the three cosine terms are specified via the `params` structure, allowing customization of the window's behavior.
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the three-term cosine window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the three-term cosine window function.
     * @param[in] end The ending index up to which the three-term cosine window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, which should contain the coefficients for the three cosine terms.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void cosine_3_term(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::cosine_3_term, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a four-term cosine window function.
     *
     * This template function generates a four-term cosine window over a specified range of elements in the `window` array.
     * The four-term cosine window is a weighted sum of four cosine terms, offering even greater control over the window's shape
     * compared to the two-term and three-term windows. This window function provides a very fine balance between main lobe width
     * and side lobe attenuation, making it ideal for applications requiring high precision in frequency resolution and minimal spectral leakage.
     *
     * The coefficients for the four cosine terms are provided via the `params` structure, allowing customization of the window's behavior.
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the four-term cosine window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the four-term cosine window function.
     * @param[in] end The ending index up to which the four-term cosine window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, which should contain the coefficients for the four cosine terms.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void cosine_4_term(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::cosine_4_term, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a five-term cosine window function.
     *
     * This template function generates a five-term cosine window over a specified range of elements in the `window` array.
     * The five-term cosine window is a weighted sum of five cosine terms, offering the highest degree of control over
     * the window's shape and behavior compared to lower-order cosine windows. This function provides fine-tuned control
     * over side lobe suppression and main lobe width, making it ideal for applications requiring extremely low spectral leakage
     * and precise frequency resolution.
     *
     * The coefficients for the five cosine terms are supplied via the `params` structure, allowing customization of the window's characteristics.
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the five-term cosine window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the five-term cosine window function.
     * @param[in] end The ending index up to which the five-term cosine window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, which should contain the coefficients for the five cosine terms.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void cosine_5_term(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::cosine_5_term, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a Hann window function.
     *
     * This template function generates a Hann window over a specified range of elements in the `window` array.
     * The Hann window is a smooth, tapering window that reduces spectral leakage by gradually tapering the signal
     * to zero at the edges. It is one of the most commonly used window functions in signal processing due to its
     * balance between side lobe attenuation and main lobe width.
     *
     * The formula for the Hann window is:
     * \f[
     * w(i) = 0.5 \times \left( 1 - \cos \left( \frac{2\pi i}{N-1} \right) \right)
     * \f]
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the Hann window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the Hann window function.
     * @param[in] end The ending index up to which the Hann window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, although it is typically unused in the Hann window.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void hann(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::hann, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a Hamming window function.
     *
     * This template function generates a Hamming window over a specified range of elements in the `window` array.
     * The Hamming window is similar to the Hann window but is optimized to minimize side lobe levels, which reduces
     * spectral leakage even further. It is commonly used in signal processing applications, especially for tasks requiring
     * better side lobe suppression while maintaining a relatively narrow main lobe.
     *
     * The formula for the Hamming window is:
     * \f[
     * w(i) = 0.54 - 0.46 \times \cos \left( \frac{2\pi i}{N-1} \right)
     * \f]
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the Hamming window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the Hamming window function.
     * @param[in] end The ending index up to which the Hamming window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, although it is typically unused in the standard Hamming window.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void hamming(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::hamming, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a Blackman window function.
     *
     * This template function generates a Blackman window over a specified range of elements in the `window` array.
     * The Blackman window is designed to provide better side lobe attenuation compared to the Hann and Hamming windows,
     * making it ideal for applications where strong suppression of side lobes is important. It achieves this by incorporating
     * additional cosine terms to more effectively reduce spectral leakage.
     *
     * The formula for the Blackman window is:
     * \f[
     * w(i) = 0.42 - 0.5 \times \cos \left( \frac{2\pi i}{N-1} \right) + 0.08 \times \cos \left( \frac{4\pi i}{N-1} \right)
     * \f]
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the Blackman window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the Blackman window function.
     * @param[in] end The ending index up to which the Blackman window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, although it is typically unused in the standard Blackman window.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void blackman(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::blackman, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from an exact Blackman window function.
     *
     * This template function generates an exact Blackman window over a specified range of elements in the `window` array.
     * The exact Blackman window provides a more precise version of the standard Blackman window by using accurate coefficients
     * for better side lobe suppression and reduced spectral leakage. This variant is useful in applications where a higher degree
     * of precision is required in the frequency domain, particularly for improving side lobe attenuation while maintaining
     * the characteristics of the Blackman window.
     *
     * The formula for the exact Blackman window is:
     * \f[
     * w(i) = \frac{7938}{18608} - \frac{9240}{18608} \times \cos \left( \frac{2\pi i}{N-1} \right) + \frac{1430}{18608} \times \cos \left( \frac{4\pi i}{N-1} \right)
     * \f]
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the exact Blackman window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the exact Blackman window function.
     * @param[in] end The ending index up to which the exact Blackman window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, although it is typically unused in the exact Blackman window.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */
        
    template <class T>
    void exact_blackman(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::exact_blackman, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a Blackman-Harris 62 dB window function.
     *
     * This template function generates a Blackman-Harris 62 dB window over a specified range of elements in the `window` array.
     * The Blackman-Harris window is a generalization of the Blackman window, designed to provide better side lobe attenuation.
     * The 62 dB variant offers approximately 62 dB of side lobe suppression, making it ideal for applications that require reduced spectral leakage
     * with moderately strong side lobe attenuation. It is commonly used in signal processing for applications such as frequency analysis and filter design.
     *
     * The formula for the Blackman-Harris 62 dB window is:
     * \f[
     * w(i) = 0.44959 - 0.49364 \times \cos \left( \frac{2\pi i}{N-1} \right) + 0.05677 \times \cos \left( \frac{4\pi i}{N-1} \right)
     * \f]
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the Blackman-Harris 62 dB window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the Blackman-Harris 62 dB window function.
     * @param[in] end The ending index up to which the Blackman-Harris 62 dB window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, although it is typically unused in the Blackman-Harris 62 dB window.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void blackman_harris_62dB(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::blackman_harris_62dB, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a Blackman-Harris 71 dB window function.
     *
     * This template function generates a Blackman-Harris 71 dB window over a specified range of elements in the `window` array.
     * The Blackman-Harris window is a generalized version of the Blackman window, designed to provide even better side lobe attenuation.
     * The 71 dB variant offers approximately 71 dB side lobe suppression, making it ideal for applications that require high precision
     * in frequency analysis and a stronger reduction of spectral leakage.
     *
     * The formula for the Blackman-Harris 71 dB window is:
     * \f[
     * w(i) = 0.42323 - 0.49755 \times \cos \left( \frac{2\pi i}{N-1} \right) + 0.07922 \times \cos \left( \frac{4\pi i}{N-1} \right)
     * \f]
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the Blackman-Harris 71 dB window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the Blackman-Harris 71 dB window function.
     * @param[in] end The ending index up to which the Blackman-Harris 71 dB window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, although it is typically unused in the Blackman-Harris 71 dB window.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void blackman_harris_71dB(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::blackman_harris_71dB, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a Blackman-Harris 74 dB window function.
     *
     * This template function generates a Blackman-Harris 74 dB window over a specified range of elements in the `window` array.
     * The Blackman-Harris window is a generalized version of the Blackman window, designed to provide high side lobe attenuation.
     * The 74 dB variant offers approximately 74 dB side lobe suppression, making it suitable for applications requiring high precision
     * in frequency analysis and minimal spectral leakage. This window is often used in applications like filter design, spectral analysis, and smoothing.
     *
     * The formula for the Blackman-Harris 74 dB window is:
     * \f[
     * w(i) = 0.35875 - 0.48829 \times \cos \left( \frac{2\pi i}{N-1} \right) + 0.14128 \times \cos \left( \frac{4\pi i}{N-1} \right) - 0.01168 \times \cos \left( \frac{6\pi i}{N-1} \right)
     * \f]
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the Blackman-Harris 74 dB window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the Blackman-Harris 74 dB window function.
     * @param[in] end The ending index up to which the Blackman-Harris 74 dB window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, although it is typically unused in the Blackman-Harris 74 dB window.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void blackman_harris_74dB(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::blackman_harris_74dB, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a Blackman-Harris 92 dB window function.
     *
     * This template function generates a Blackman-Harris 92 dB window over a specified range of elements in the `window` array.
     * The Blackman-Harris window is designed to provide excellent side lobe attenuation, and the 92 dB variant offers very strong
     * suppression of side lobes, approximately 92 dB, making it ideal for high-precision applications where minimizing spectral
     * leakage is critical. This window is commonly used in signal processing tasks such as filter design, frequency analysis, and
     * smoothing where extreme side lobe suppression is required.
     *
     * The formula for the Blackman-Harris 92 dB window is:
     * \f[
     * w(i) = 0.35875 - 0.48829 \times \cos \left( \frac{2\pi i}{N-1} \right) + 0.14128 \times \cos \left( \frac{4\pi i}{N-1} \right) - 0.01168 \times \cos \left( \frac{6\pi i}{N-1} \right)
     * \f]
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the Blackman-Harris 92 dB window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the Blackman-Harris 92 dB window function.
     * @param[in] end The ending index up to which the Blackman-Harris 92 dB window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, although it is typically unused in the Blackman-Harris 92 dB window.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void blackman_harris_92dB(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::blackman_harris_92dB, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a Nuttall 1st derivative 64 dB window function.
     *
     * This template function generates a Nuttall 1st derivative window with approximately 64 dB side lobe attenuation
     * over a specified range of elements in the `window` array. The Nuttall window is designed to minimize spectral leakage,
     * and the 1st derivative variant offers a smooth taper that provides moderate side lobe suppression. This window is useful
     * in signal processing applications that require a balance between main lobe width and side lobe attenuation.
     *
     * The formula for the Nuttall 1st 64 dB window is a weighted sum of cosine terms:
     * \f[
     * w(i) = 0.355768 - 0.487396 \times \cos \left( \frac{2\pi i}{N-1} \right) + 0.144232 \times \cos \left( \frac{4\pi i}{N-1} \right) - 0.012604 \times \cos \left( \frac{6\pi i}{N-1} \right)
     * \f]
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the Nuttall 1st 64 dB window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the Nuttall window function.
     * @param[in] end The ending index up to which the Nuttall window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, which may contain additional parameters influencing the window's shape.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void nuttall_1st_64dB(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::nuttall_1st_64dB, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a Nuttall 1st derivative 93 dB window function.
     *
     * This template function generates a Nuttall 1st derivative window with approximately 93 dB side lobe attenuation
     * over a specified range of elements in the `window` array. The Nuttall window is known for its ability to minimize
     * spectral leakage, and the 1st derivative variant with 93 dB suppression provides strong side lobe attenuation while
     * maintaining a smooth taper. This window is ideal for high-precision applications where reducing side lobes is critical.
     *
     * The formula for the Nuttall 1st 93 dB window is:
     * \f[
     * w(i) = 0.3635819 - 0.4891775 \times \cos \left( \frac{2\pi i}{N-1} \right) + 0.1365995 \times \cos \left( \frac{4\pi i}{N-1} \right) - 0.0106411 \times \cos \left( \frac{6\pi i}{N-1} \right)
     * \f]
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the Nuttall 1st 93 dB window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the Nuttall window function.
     * @param[in] end The ending index up to which the Nuttall window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, which may contain additional parameters influencing the window's characteristics.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void nuttall_1st_93dB(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::nuttall_1st_93dB, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a Nuttall 3rd derivative 47 dB window function.
     *
     * This template function generates a Nuttall 3rd derivative window with approximately 47 dB side lobe attenuation
     * over a specified range of elements in the `window` array. The Nuttall 3rd derivative window provides smooth tapering
     * and moderate side lobe suppression, making it suitable for applications where a balance between main lobe width and
     * side lobe attenuation is needed. This variant is ideal for tasks that require moderate spectral leakage reduction.
     *
     * The formula for the Nuttall 3rd 47 dB window is a weighted sum of cosine terms:
     * \f[
     * w(i) = 0.4243801 - 0.4973406 \times \cos \left( \frac{2\pi i}{N-1} \right) + 0.0782793 \times \cos \left( \frac{4\pi i}{N-1} \right)
     * \f]
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the Nuttall 3rd 47 dB window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the Nuttall window function.
     * @param[in] end The ending index up to which the Nuttall window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, which may contain additional parameters influencing the window's characteristics.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void nuttall_3rd_47dB(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::nuttall_3rd_47dB, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a Nuttall 3rd derivative 83 dB window function.
     *
     * This template function generates a Nuttall 3rd derivative window with approximately 83 dB side lobe attenuation
     * over a specified range of elements in the `window` array. The Nuttall 3rd derivative window is designed to provide
     * stronger side lobe suppression compared to lower dB variants, making it suitable for high-precision applications
     * requiring reduced spectral leakage. This window provides smooth tapering and strong side lobe suppression, making
     * it ideal for tasks such as filter design, frequency analysis, and smoothing.
     *
     * The formula for the Nuttall 3rd 83 dB window is:
     * \f[
     * w(i) = 0.338946 - 0.481973 \times \cos \left( \frac{2\pi i}{N-1} \right) + 0.161054 \times \cos \left( \frac{4\pi i}{N-1} \right) - 0.018027 \times \cos \left( \frac{6\pi i}{N-1} \right)
     * \f]
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the Nuttall 3rd 83 dB window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the Nuttall window function.
     * @param[in] end The ending index up to which the Nuttall window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, which may contain additional parameters influencing the window's characteristics.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void nuttall_3rd_83dB(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::nuttall_3rd_83dB, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a Nuttall 5th derivative 61 dB window function.
     *
     * This template function generates a Nuttall 5th derivative window with approximately 61 dB side lobe attenuation
     * over a specified range of elements in the `window` array. The Nuttall 5th derivative window provides smoother tapering
     * and moderate side lobe suppression, making it suitable for applications requiring a balance between main lobe width
     * and side lobe attenuation. This variant is ideal for signal processing tasks where minimizing spectral leakage
     * with moderate side lobe suppression is important.
     *
     * The formula for the Nuttall 5th 61 dB window is a weighted sum of cosine terms, similar to other Nuttall windows,
     * but with more precise control for smoothing and reducing leakage.
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the Nuttall 5th 61 dB window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the Nuttall window function.
     * @param[in] end The ending index up to which the Nuttall window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, which may contain additional parameters influencing the window's characteristics.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void nuttall_5th_61dB(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::nuttall_5th_61dB, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a Nuttall minimal 71 dB window function.
     *
     * This template function generates a Nuttall minimal window with approximately 71 dB side lobe attenuation
     * over a specified range of elements in the `window` array. The Nuttall minimal window is optimized to provide
     * strong side lobe suppression while maintaining a relatively narrow main lobe, minimizing spectral leakage.
     * The 71 dB variant is suitable for applications that require a moderate balance between side lobe suppression
     * and main lobe width.
     *
     * The formula for the Nuttall minimal 71 dB window is a weighted sum of cosine terms:
     * \f[
     * w(i) = 0.355768 - 0.487396 \times \cos \left( \frac{2\pi i}{N-1} \right) + 0.144232 \times \cos \left( \frac{4\pi i}{N-1} \right) - 0.012604 \times \cos \left( \frac{6\pi i}{N-1} \right)
     * \f]
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the Nuttall minimal 71 dB window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the Nuttall minimal window function.
     * @param[in] end The ending index up to which the Nuttall minimal window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, which may contain additional parameters influencing the window's characteristics.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void nuttall_minimal_71dB(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::nuttall_minimal_71dB, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a Nuttall minimal 98 dB window function.
     *
     * This template function generates a Nuttall minimal window with approximately 98 dB side lobe attenuation
     * over a specified range of elements in the `window` array. The Nuttall minimal window is designed to provide
     * very strong side lobe suppression while maintaining a narrow main lobe, making it ideal for high-precision
     * applications that require minimal spectral leakage. The 98 dB variant is especially useful in applications
     * like frequency analysis, filter design, and signal processing where high side lobe attenuation is necessary.
     *
     * The formula for the Nuttall minimal 98 dB window is a weighted sum of cosine terms:
     * \f[
     * w(i) = 0.355768 - 0.487396 \times \cos \left( \frac{2\pi i}{N-1} \right) + 0.144232 \times \cos \left( \frac{4\pi i}{N-1} \right) - 0.012604 \times \cos \left( \frac{6\pi i}{N-1} \right)
     * \f]
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the Nuttall minimal 98 dB window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the Nuttall minimal window function.
     * @param[in] end The ending index up to which the Nuttall minimal window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, which may contain additional parameters influencing the window's characteristics.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void nuttall_minimal_98dB(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::nuttall_minimal_98dB, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a National Instruments (NI) flat top window function.
     *
     * This template function generates a National Instruments (NI) flat top window over a specified range of elements in the `window` array.
     * The NI flat top window is designed to provide accurate amplitude measurements across a wide range of frequencies,
     * offering a flat passband with minimal amplitude distortion. This window is commonly used in applications such as signal analysis
     * and frequency domain measurements where amplitude accuracy is important.
     *
     * The flat top window reduces ripple in the passband and is useful in scenarios where it is important to capture the exact amplitude
     * of frequency components in a signal.
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the NI flat top window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the NI flat top window function.
     * @param[in] end The ending index up to which the NI flat top window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, which may contain additional parameters influencing the window's characteristics.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void ni_flat_top(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::ni_flat_top, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a Hewlett-Packard (HP) flat top window function.
     *
     * This template function generates a Hewlett-Packard (HP) flat top window over a specified range of elements in the `window` array.
     * The HP flat top window is designed to minimize amplitude distortion and provide a flat passband, making it useful in applications
     * that require precise amplitude measurements across a range of frequencies. This window reduces passband ripple, ensuring more
     * accurate amplitude values in frequency-domain analysis.
     *
     * The HP flat top window is commonly used in applications such as signal analysis, spectral measurements, and frequency response
     * analysis where amplitude accuracy is critical.
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the HP flat top window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the HP flat top window function.
     * @param[in] end The ending index up to which the HP flat top window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, which may contain additional parameters influencing the window's characteristics.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void hp_flat_top(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::hp_flat_top, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a Stanford flat top window function.
     *
     * This template function generates a Stanford flat top window over a specified range of elements in the `window` array.
     * The Stanford flat top window is designed to provide a flat passband with minimal amplitude distortion, making it ideal
     * for precise amplitude measurements across a range of frequencies. By minimizing the ripple in the passband, this window
     * ensures that the amplitude of the frequency components in a signal is accurately represented.
     *
     * This window function is typically used in signal analysis, frequency domain measurements, and applications requiring
     * accurate representation of signal amplitude.
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the Stanford flat top window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the Stanford flat top window function.
     * @param[in] end The ending index up to which the Stanford flat top window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, which may contain additional parameters influencing the window's characteristics.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void stanford_flat_top(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::stanford_flat_top, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a Heinzel flat top window function with 70 dB side lobe attenuation.
     *
     * This template function generates a Heinzel flat top window with approximately 70 dB side lobe attenuation
     * over a specified range of elements in the `window` array. The Heinzel flat top window is designed to provide
     * a flat passband, ensuring accurate amplitude measurements across a wide range of frequencies while reducing
     * side lobes to 70 dB. It is typically used in high-precision signal processing applications that require
     * reduced spectral leakage and high amplitude accuracy.
     *
     * The Heinzel flat top window is particularly useful in spectral analysis and frequency domain applications where
     * minimizing passband ripple and ensuring amplitude accuracy is essential.
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the Heinzel flat top 70 dB window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the Heinzel flat top 70 dB window function.
     * @param[in] end The ending index up to which the Heinzel flat top window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, which may contain additional parameters influencing the window's characteristics.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void heinzel_flat_top_70dB(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::heinzel_flat_top_70dB, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a Heinzel flat top window function with 90 dB side lobe attenuation.
     *
     * This template function generates a Heinzel flat top window with approximately 90 dB side lobe attenuation
     * over a specified range of elements in the `window` array. The Heinzel flat top window is designed to provide
     * a very flat passband, ensuring precise amplitude measurements across a wide frequency range while minimizing
     * side lobes. The 90 dB variant is ideal for applications that demand higher side lobe suppression, ensuring
     * minimal spectral leakage and high amplitude accuracy.
     *
     * This window is often used in high-precision frequency domain measurements, spectral analysis, and applications
     * where accurate amplitude representation is critical.
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the Heinzel flat top 90 dB window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the Heinzel flat top 90 dB window function.
     * @param[in] end The ending index up to which the Heinzel flat top window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, which may contain additional parameters influencing the window's characteristics.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void heinzel_flat_top_90dB(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::heinzel_flat_top_90dB, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief Fills an array with values from a Heinzel flat top window function with 95 dB side lobe attenuation.
     *
     * This template function generates a Heinzel flat top window with approximately 95 dB side lobe attenuation
     * over a specified range of elements in the `window` array. The Heinzel flat top window is designed for applications
     * requiring extremely precise amplitude measurements over a wide frequency range. The 95 dB variant provides
     * very strong side lobe suppression, ensuring minimal spectral leakage while maintaining a flat passband for amplitude accuracy.
     *
     * This window is commonly used in high-precision signal processing tasks, such as spectral analysis and frequency domain
     * measurements, where both amplitude accuracy and side lobe suppression are critical.
     *
     * @tparam T The type of the elements in the `window` array.
     *
     * @param[out] window A pointer to an array of type `T` where the Heinzel flat top 95 dB window values will be stored.
     * @param[in] N The total number of samples or elements in the window.
     * @param[in] begin The starting index from which to generate the Heinzel flat top 95 dB window function.
     * @param[in] end The ending index up to which the Heinzel flat top window function will be generated (exclusive).
     * @param[in] p A constant reference to the `params` structure, which may contain additional parameters influencing the window's characteristics.
     *
     * @note The range of indices is `[begin, end)`, meaning the value at `end` is not included.
     */

    template <class T>
    void heinzel_flat_top_95dB(T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
    {
        impl::generate<impl::heinzel_flat_top_95dB, true>(window, N, begin, end, p);
    }
    
    /**
     * @brief A struct to manage and generate windows based on indexed window generators.
     *
     * This template struct `indexed_generator` manages a collection of window generators, each identified by an index.
     * The struct allows for the selection and application of a specific window generator from a list of generators
     * based on a provided index. This design is useful when multiple window functions need to be applied to different parts of a signal or dataset,
     * allowing for flexible and efficient window generation.
     *
     * @tparam T The data type for the window values, typically `float` or `double`.
     * @tparam gens A parameter pack of `window_generator<T>` types, representing the window functions that can be used.
     *
     * This struct can be used to select and generate windows based on their index, allowing different window functions
     * to be applied dynamically.
     */

    template <class T, window_generator<T> ...gens>
    struct indexed_generator
    {
        
        /**
         * @brief Applies a window function based on the specified type.
         *
         * This function call operator selects and applies a window function from a collection of window generators
         * based on the provided `type` index. It fills the `window` array with values generated by the window function
         * corresponding to the `type`. This allows dynamic selection of different window functions at runtime.
         *
         * @param[in] type The index of the window function to apply. This index corresponds to one of the available window generators.
         * @param[out] window A pointer to an array of type `T` where the generated window values will be stored.
         * @param[in] N The total number of samples or elements in the window.
         * @param[in] begin The starting index from which to generate the window function.
         * @param[in] end The ending index up to which the window function will be generated (exclusive).
         * @param[in] p A constant reference to the `params` structure, containing any parameters required by the window function.
         *
         * @note The range of indices for generating the window is `[begin, end)`, meaning the value at `end` is not included.
         *
         * This operator allows dynamic selection and application of different window functions based on the provided `type` index.
         */
        
        void operator()(size_t type, T *window, uint32_t N, uint32_t begin, uint32_t end, const params& p)
        {
            return generators[type](window, N, begin, end, p);
        }
        
        /**
         * @brief Retrieves the window generator function based on the specified type.
         *
         * This function returns a pointer to a window generator function from the list of available generators, based on the provided `type` index.
         *
         * @param[in] type The index of the window generator to retrieve. This index corresponds to one of the available window generators.
         *
         * @return A pointer to the `window_generator<T>` function corresponding to the specified `type` index.
         *
         * @note The `type` index should be within the bounds of the available window generators to avoid undefined behavior.
         */
        
        window_generator<T> *get(size_t type) { return generators[type]; }
        
        /**
         * @brief Array of window generator function pointers initialized with the provided generator functions.
         *
         * This array holds pointers to window generator functions, initialized with the list of generators passed as template parameters (`gens...`).
         * The size of the array is determined by the number of template arguments, `sizeof...(gens)`, which corresponds to the number of window generator functions provided.
         *
         * @tparam gens A parameter pack representing the window generator functions of type `window_generator<T>`.
         *
         * This array allows dynamic access to the different window generator functions for applying various windowing techniques.
         */
        
        window_generator<T> *generators[sizeof...(gens)] = { gens... };
    };
}

#endif
