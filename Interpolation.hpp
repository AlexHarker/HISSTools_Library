
/**
 * @file Interpolation.hpp
 * @brief Provides various interpolation methods, including linear, cubic Hermite, cubic Lagrange, and cubic B-spline interpolations.
 *
 * This file defines template structures and methods for performing different types of interpolation.
 * Each interpolation method allows for smooth estimation of values between known data points
 * using specific mathematical techniques:
 * - Linear interpolation computes values by connecting points with straight lines.
 * - Cubic Hermite interpolation provides smooth results with continuous first derivatives.
 * - Cubic Lagrange interpolation uses Lagrange polynomials to approximate values between points.
 * - Cubic B-spline interpolation uses B-spline basis functions to ensure smooth curves with continuity in function values and derivatives.
 *
 * The templates are designed to be flexible and accept various data types for interpolation, such as float or double.
 */

#ifndef INTERPOLATION_HPP
#define INTERPOLATION_HPP

// Enumeration of interpolation types

/**
 * @enum InterpType
 * @brief Defines different types of interpolation methods.
 *
 * This enumeration specifies various interpolation techniques that can be
 * used to estimate values between known data points.
 */
enum class InterpType {
    None,           /**< No interpolation applied. */
        Linear,         /**< Linear interpolation. Computes the value by connecting data points with a straight line. */
        CubicHermite,   /**< Cubic Hermite interpolation. Provides smooth results with continuity in both the function and its first derivative. */
        CubicLagrange,  /**< Cubic Lagrange interpolation. Uses the Lagrange polynomial for approximating values. */
        CubicBSpline    /**< Cubic B-Spline interpolation. Uses B-spline curves for smooth interpolation. */
};

// Linear

/**
 * @struct linear_interp
 * @brief A template structure for performing linear interpolation.
 *
 * This structure provides a mechanism for calculating the linear interpolation
 * of a given value between two points. The template parameter @p T represents
 * the data type of the values being interpolated.
 *
 * @tparam T The type of the data points to be interpolated (e.g., float, double).
 */

template <class T>
struct linear_interp
{
    
    /**
     * @brief Performs linear interpolation between two values.
     *
     * This operator overload computes the linear interpolation between two values @p y0 and @p y1
     * at a point @p x, where @p x is typically a fractional value between 0 and 1.
     *
     * @param x The interpolation factor. Typically a value between 0 and 1, representing
     *          the relative position between @p y0 and @p y1.
     * @param y0 The starting value (interpolated at @p x = 0).
     * @param y1 The ending value (interpolated at @p x = 1).
     * @return The interpolated value between @p y0 and @p y1.
     */
    
    T operator()(const T& x, const T& y0, const T& y1) { return  (y0 + x * ((y1 - y0))); }
};

// Cubic Hermite

/**
 * @struct cubic_hermite_interp
 * @brief A template structure for performing cubic Hermite interpolation.
 *
 * This structure provides a mechanism for calculating cubic Hermite interpolation,
 * which is a method of interpolation that provides smooth curves with continuity
 * in both the function values and their first derivatives.
 *
 * The template parameter @p T represents the data type of the values being interpolated.
 *
 * @tparam T The type of the data points to be interpolated (e.g., float, double).
 */

template <class T>
struct cubic_hermite_interp
{
    
    /**
     * @brief Default constructor for the cubic Hermite interpolation structure.
     *
     * Initializes constants used in the cubic Hermite interpolation process.
     * These constants represent fractional coefficients that are used in the interpolation formula:
     * - @p _5div2 is initialized to 2.5.
     * - @p _3div2 is initialized to 1.5.
     * - @p _1div2 is initialized to 0.5.
     */
    
    cubic_hermite_interp() : _5div2(2.5), _3div2(1.5), _1div2(0.5) {}
    
    /**
     * @brief Performs cubic Hermite interpolation between four values.
     *
     * This operator overload computes the cubic Hermite interpolation between four points
     * @p y0, @p y1, @p y2, and @p y3 at a given point @p x. The values @p y1 and @p y2
     * represent the interval over which interpolation occurs, while @p y0 and @p y3 are
     * used to compute the tangent at the interval boundaries, ensuring smooth transitions.
     *
     * @param x The interpolation factor, typically between 0 and 1, representing the relative
     *          position between @p y1 and @p y2.
     * @param y0 The value before the starting point of interpolation, used to calculate the slope at @p y1.
     * @param y1 The starting value of the interpolation interval (interpolated at @p x = 0).
     * @param y2 The ending value of the interpolation interval (interpolated at @p x = 1).
     * @param y3 The value after the ending point of interpolation, used to calculate the slope at @p y2.
     * @return The interpolated value between @p y1 and @p y2 using cubic Hermite interpolation.
     */
    
    T operator()(const T& x, const T& y0, const T& y1, const T& y2, const T& y3)
    {
        const T c0 = y1;
        const T c1 = _1div2 * (y2 - y0);
        const T c2 = y0 - _5div2 * y1 + y2 + y2 - _1div2 * y3;
        const T c3 = _1div2 * (y3 - y0) + _3div2 * (y1 - y2);
        
        return (((c3 * x + c2) * x + c1) * x + c0);
    }
    
private:
    
    /**
     * @brief A constant representing the value 5/2 (or 2.5), used in cubic Hermite interpolation calculations.
     *
     * This constant is used internally in the cubic Hermite interpolation formula to scale the results
     * based on specific mathematical operations.
     */
    
    const T _5div2;
    
    /**
     * @brief A constant representing the value 3/2 (or 1.5), used in cubic Hermite interpolation calculations.
     *
     * This constant is utilized in the cubic Hermite interpolation formula to help scale and calculate
     * the interpolated value between points.
     */
    
    const T _3div2;
    
    /**
     * @brief A constant representing the value 1/2 (or 0.5), used in cubic Hermite interpolation calculations.
     *
     * This constant is used as part of the cubic Hermite interpolation formula to help adjust the scaling
     * of the interpolated value between points.
     */
    
    const T _1div2;
};

// Cubic Lagrange

/**
 * @struct cubic_lagrange_interp
 * @brief A template structure for performing cubic Lagrange interpolation.
 *
 * This structure provides a mechanism for calculating cubic Lagrange interpolation,
 * which approximates a smooth curve through four data points using Lagrange polynomials.
 * The template parameter @p T represents the data type of the values being interpolated.
 *
 * @tparam T The type of the data points to be interpolated (e.g., float, double).
 */

template <class T>
struct cubic_lagrange_interp
{
    
    /**
     * @brief Default constructor for the cubic Lagrange interpolation structure.
     *
     * Initializes constants used in the cubic Lagrange interpolation process.
     * These constants represent fractional coefficients that are used in the interpolation formula:
     * - @p _1div3 is initialized to 1/3.
     * - @p _1div6 is initialized to 1/6.
     * - @p _1div2 is initialized to 0.5.
     *
     * The values are computed based on the template parameter type @p T.
     */
    
    cubic_lagrange_interp() : _1div3(T(1)/T(3)), _1div6(T(1)/T(6)), _1div2(0.5) {}
    
    /**
     * @brief Performs cubic Lagrange interpolation between four values.
     *
     * This operator overload computes the cubic Lagrange interpolation between four points
     * @p y0, @p y1, @p y2, and @p y3 at a given point @p x. Lagrange interpolation uses
     * polynomials to estimate the value at @p x based on these four surrounding points.
     *
     * @param x The interpolation factor, typically between 0 and 1, representing the relative position
     *          within the interval between @p y1 and @p y2.
     * @param y0 The value before the starting point of interpolation, used to calculate the Lagrange polynomial.
     * @param y1 The starting value of the interpolation interval (interpolated at @p x = 0).
     * @param y2 The ending value of the interpolation interval (interpolated at @p x = 1).
     * @param y3 The value after the ending point of interpolation, used to calculate the Lagrange polynomial.
     * @return The interpolated value between @p y1 and @p y2 using cubic Lagrange interpolation.
     */
    
    T operator()(const T& x, const T& y0, const T& y1, const T& y2, const T& y3)
    {
        const T c0 = y1;
        const T c1 = y2 - _1div3 * y0 - _1div2 * y1 - _1div6 * y3;
        const T c2 = _1div2 * (y0 + y2) - y1;
        const T c3 = _1div6 * (y3 - y0) + _1div2 * (y1 - y2);
        
        return (((c3 * x + c2) * x + c1) * x + c0);
    }
    
private:

    /**
     * @brief A constant representing the value 1/3, used in cubic Lagrange interpolation calculations.
     *
     * This constant is utilized as part of the cubic Lagrange interpolation formula to scale
     * and compute the interpolated value between points.
     */
    
    const T _1div3;
    
    /**
     * @brief A constant representing the value 1/6, used in cubic Lagrange interpolation calculations.
     *
     * This constant is part of the cubic Lagrange interpolation formula, helping to scale and
     * compute the interpolated value between data points.
     */
    
    const T _1div6;
    
    /**
     * @brief A constant representing the value 1/2 (or 0.5), used in cubic Lagrange interpolation calculations.
     *
     * This constant is utilized in the cubic Lagrange interpolation formula to help adjust
     * and scale the interpolated value between data points.
     */
    
    const T _1div2;
};

// Cubic B-spline

/**
 * @struct cubic_bspline_interp
 * @brief A template structure for performing cubic B-spline interpolation.
 *
 * This structure provides a mechanism for calculating cubic B-spline interpolation,
 * which produces smooth curves through control points using B-spline basis functions.
 * The cubic B-spline method ensures continuity in both the function and its first two derivatives.
 * The template parameter @p T represents the data type of the values being interpolated.
 *
 * @tparam T The type of the data points to be interpolated (e.g., float, double).
 */

template <class T>
struct cubic_bspline_interp
{
    
    /**
     * @brief Default constructor for the cubic B-spline interpolation structure.
     *
     * Initializes constants used in the cubic B-spline interpolation process.
     * These constants represent fractional coefficients that are used in the B-spline interpolation formula:
     * - @p _2div3 is initialized to 2/3.
     * - @p _1div6 is initialized to 1/6.
     * - @p _1div2 is initialized to 0.5.
     *
     * The values are computed based on the template parameter type @p T.
     */
    
    cubic_bspline_interp() : _2div3(T(2)/T(3)), _1div6(T(1)/T(6)), _1div2(0.5) {}
    
    /**
     * @brief Performs cubic B-spline interpolation between four values.
     *
     * This operator overload computes the cubic B-spline interpolation between four control points
     * @p y0, @p y1, @p y2, and @p y3 at a given point @p x. B-spline interpolation uses basis functions
     * to create a smooth curve through the control points, ensuring continuity in both the function
     * and its first two derivatives.
     *
     * @param x The interpolation factor, typically between 0 and 1, representing the relative position
     *          within the interval between @p y1 and @p y2.
     * @param y0 The value before the starting point of interpolation, used to define the curve.
     * @param y1 The starting value of the interpolation interval (interpolated at @p x = 0).
     * @param y2 The ending value of the interpolation interval (interpolated at @p x = 1).
     * @param y3 The value after the ending point of interpolation, used to define the curve.
     * @return The interpolated value between @p y1 and @p y2 using cubic B-spline interpolation.
     */
    
    T operator()(const T& x, const T& y0, const T& y1, const T& y2, const T& y3)
    {
        const T y0py2 = y0 + y2;
        const T c0 = _1div6 * y0py2 + _2div3 * y1;
        const T c1 = _1div2 * (y2 - y0);
        const T c2 = _1div2 * y0py2 - y1;
        const T c3 = _1div2 * (y1 - y2) + _1div6 * (y3 - y0);
        
        return (((c3 * x + c2) * x + c1) * x + c0);
    }
    
private:

    /**
     * @brief A constant representing the value 2/3, used in cubic B-spline interpolation calculations.
     *
     * This constant is part of the cubic B-spline interpolation formula, used to scale and compute
     * the interpolated value between control points.
     */
    
    const T _2div3;
    
    /**
     * @brief A constant representing the value 1/6, used in cubic B-spline interpolation calculations.
     *
     * This constant is utilized as part of the cubic B-spline interpolation formula to help scale
     * and compute the interpolated value between control points.
     */
    
    const T _1div6;
    
    /**
     * @brief A constant representing the value 1/2 (or 0.5), used in cubic B-spline interpolation calculations.
     *
     * This constant is part of the cubic B-spline interpolation formula and is used to help scale
     * the interpolated value between control points.
     */
    
    const T _1div2;
};

#endif /* Interpolation_h */
