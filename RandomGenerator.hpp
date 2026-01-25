
/**
 * @file RandomGenerator.hpp
 * @brief Provides classes and functions for random number generation, including CMWC (Complementary Multiply With Carry) and Gaussian generators.
 *
 * This file contains implementations for different random number generators, including a
 * template-based random generator class that allows for flexibility in choosing the underlying
 * algorithm. The CMWC (Complementary Multiply With Carry) algorithm is used as the default
 * generator. Additionally, it includes utilities for generating Gaussian-distributed random
 * numbers, both standard and windowed, as well as functionality for seeding the generators.
 *
 * Classes:
 * - random_generators::cmwc: Implements the CMWC random number generator.
 * - random_generator: A template-based random generator that supports different algorithms.
 * - windowed_gaussian_params: Stores parameters for windowed Gaussian distributions.
 *
 * Functions:
 * - Seed and random number generation functions for both integer and floating-point types,
 *   including support for Gaussian-distributed values.
 *
 * @details This file is part of a random number generation library that offers flexibility
 * in seeding and generating numbers, with support for custom distribution windows. It is
 * particularly useful in applications where high-quality randomness and Gaussian-distributed
 * numbers are needed, such as simulations and statistical sampling.
 */

#ifndef _RANDOMGENERATOR_HPP_
#define _RANDOMGENERATOR_HPP_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>

/**
 * @namespace random_generators
 * @brief Namespace containing implementations of various random number generators.
 *
 * The random_generators namespace encapsulates all classes, functions, and constants related
 * to random number generation algorithms. It helps organize and group different random
 * generator implementations to avoid naming conflicts and improve code modularity.
 */

namespace random_generators
{
    // Basic CMWC Generator

    // A complementary modulo with carry algorithm (proposed by George Marsaglia)
    // Details can be found in:
    // Marsaglia, G. (2003). "Random number generators". Journal of Modern Applied Statistical Methods 2
    // See - http://digitalcommons.wayne.edu/cgi/viewcontent.cgi?article=1725&context=jmasm

    // The memory requirement is 34 unsigned 32 bit integers (can be altered using cmwc_lag_size)
    // The period length is currently circa 2^1054 - 1 which shold be more than adequate for most purposes

    // N.B. cmwc_lag_size must be a power of two
    // N.B. cmwc_a_value should be a suitable value according to cmwc_lag_size

    /**
     * @class cmwc
     * @brief Implements a Complementary Multiply With Carry (CMWC) random number generator.
     *
     * The cmwc class provides an implementation of the CMWC algorithm, which is a fast and
     * efficient pseudorandom number generator. It uses a lag and carry-based method to produce
     * high-quality random numbers.
     *
     * @details The CMWC algorithm is an extension of the Multiply With Carry (MWC) method, where
     * a sequence of random numbers is generated using a lag table and a carry value. This class
     * manages the state and generation process for producing random numbers.
     */

    class cmwc
    {
        
        /**
         * @brief Constant representing the size of the lag for the CMWC (Complementary Multiply With Carry) random number generator.
         *
         * This constant is used to define the size of the lag for the CMWC algorithm, which is an efficient pseudorandom number generator.
         */
        
        static constexpr uint32_t cmwc_lag_size = 32;
        
        /**
         * @brief Constant multiplier value used in the CMWC (Complementary Multiply With Carry) algorithm.
         *
         * This constant defines the multiplier 'a' in the CMWC random number generation formula.
         * The choice of this value is critical for the performance and statistical properties of
         * the random number generator.
         */
        
        static constexpr uint64_t cmwc_a_value = 987655670LL;
        
    public:
        
        /**
         * @brief Generates the next random number using the CMWC algorithm.
         *
         * This operator overload allows the cmwc object to be called like a function,
         * returning the next pseudorandom number in the sequence.
         *
         * @return A 32-bit unsigned integer representing the next random number.
         */
        
        inline uint32_t operator()()
        {
            uint32_t i = m_increment;
            uint32_t c = m_carry;
            uint32_t x;
            
            uint64_t t;
            
            i = (i + 1) & (cmwc_lag_size - 1);
            t = cmwc_a_value * m_state[i] + c;
            c = (t >> 32);
            x = static_cast<uint32_t>((t + c) & 0xFFFFFFFF);
            
            if (x < c)
            {
                x++;
                c++;
            }
            
            m_state[i] = (0xFFFFFFFE - x);
            m_increment = i;
            m_carry = c;
            
            return m_state[i];
        }
        
        // Seeding (specific / OS-specific random values)
        
        /**
         * @brief Seeds the CMWC random number generator with an initial state.
         *
         * This function initializes the internal state of the CMWC algorithm using the provided seed values.
         * The seed values are essential for determining the starting point of the random number sequence.
         *
         * @param init A pointer to an array of 32-bit unsigned integers used to seed the generator.
         *             The size of the array should match the required lag size for the CMWC algorithm.
         */
        
        void seed(uint32_t *init)
        {
            m_increment = (cmwc_lag_size - 1);
            m_carry = 123;
            
            for (uint32_t i = 0; i < cmwc_lag_size; i++)
                m_state[i] = init[i];
        }
        
        /**
         * @brief Seeds the CMWC random number generator with a random value.
         *
         * This function automatically generates a random seed to initialize the internal state
         * of the CMWC algorithm, allowing for a non-deterministic starting point of the random
         * number sequence.
         */
        
        void rand_seed()
        {
            uint32_t seeds[cmwc_lag_size];
            
            std::random_device rd;
            
            for (uint32_t i = 0; i < cmwc_lag_size; i++)
                seeds[i] = rd();
        
            seed(seeds);
        }
        
        // State
        
        /**
         * @brief The increment value used in the CMWC (Complementary Multiply With Carry) algorithm.
         *
         * This variable stores the increment (or carry) value, which is updated at each step of the
         * CMWC random number generation process. It plays a key role in maintaining the sequence
         * of generated random numbers.
         */
        
        uint32_t m_increment;
        
        /**
         * @brief The carry value used in the CMWC (Complementary Multiply With Carry) algorithm.
         *
         * This variable holds the carry value that is updated during the random number generation
         * process. The carry is essential for ensuring the randomness and quality of the generated
         * sequence in the CMWC algorithm.
         */
        
        uint32_t m_carry;
        
        /**
         * @brief The state array used in the CMWC (Complementary Multiply With Carry) algorithm.
         *
         * This array holds the internal state of the CMWC random number generator, with a size
         * defined by `cmwc_lag_size`. The elements of this array are updated during the generation
         * of random numbers and are essential for maintaining the sequence of pseudorandom values.
         */
        
        uint32_t m_state[cmwc_lag_size];
    };
}

/**
 * @tparam Generator The random number generator type to use, defaulting to `random_generators::cmwc`.
 * @class random_generator
 * @brief A template class that provides a wrapper for different random number generator algorithms.
 *
 * The random_generator class allows for flexibility in choosing the underlying random number
 * generator algorithm by specifying a template parameter. By default, it uses the CMWC (Complementary
 * Multiply With Carry) algorithm, but it can be customized to use any other random generator that
 * follows the same interface.
 *
 * @tparam Generator The random number generator class to use. It should define a method for generating
 * random numbers and managing its internal state.
 */

template <typename Generator = random_generators::cmwc>
class random_generator
{
public:

    /**
     * @class windowed_gaussian_params
     * @brief Holds parameters for generating a windowed Gaussian function.
     *
     * This class encapsulates the parameters required to define a windowed Gaussian function,
     * which is typically used in signal processing, data smoothing, or random number generation.
     * It provides an easy-to-use interface for managing the characteristics of the Gaussian window.
     *
     * @details The windowed Gaussian function is characterized by its mean, standard deviation,
     * and window size. This class stores these values and offers access to modify or retrieve
     * the parameters as needed.
     */
    
    class windowed_gaussian_params
    {
        
        /**
         * @brief Grants the random_generator class access to the private and protected members of this class.
         *
         * Declaring random_generator as a friend allows it to access and manipulate the internal
         * data and functions of this class, which may be necessary for certain operations such as
         * initializing or controlling random number generation.
         */
        
        friend random_generator;
        
    public:
        
        /**
         * @brief Constructs a windowed_gaussian_params object with the specified mean and standard deviation.
         *
         * This constructor initializes the windowed Gaussian parameters with the given mean and standard deviation,
         * allowing the Gaussian function to be customized as needed.
         *
         * @param mean The mean (or center) of the Gaussian distribution.
         * @param dev The standard deviation (spread or width) of the Gaussian distribution.
         */
        
        windowed_gaussian_params(double mean, double dev) : m_mean(mean), m_dev(dev)
        {
            constexpr double inf = HUGE_VAL;

            const double a = 1.0 / (dev * sqrt(2.0));
            const double b = -mean * a;
                   
            m_lo = erf(b);
            m_hi = erf(a + b);
            
            // N.B. inf is fine as an input, but nan is not...
            
            m_lo = std::isnan(m_lo) ? erf(-inf) : m_lo;
            m_hi = std::isnan(m_hi) ? erf( inf) : m_hi;
        };
        
        /**
         * @brief Retrieves the mean value of the Gaussian distribution.
         *
         * This function returns the mean (or center) of the windowed Gaussian distribution.
         *
         * @return The mean value as a double.
         */
        
        double mean() const { return m_mean; }
        
        /**
         * @brief Retrieves the standard deviation of the Gaussian distribution.
         *
         * This function returns the standard deviation (spread or width) of the windowed Gaussian distribution.
         *
         * @return The standard deviation value as a double.
         */
        
        double dev() const  { return m_dev; }

    private:
        
        /**
         * @brief The mean value of the Gaussian distribution.
         *
         * This member variable stores the mean (or center) of the windowed Gaussian distribution.
         * It defines the central point around which the values of the distribution are concentrated.
         */
        
        double m_mean;
        
        /**
         * @brief The standard deviation of the Gaussian distribution.
         *
         * This member variable stores the standard deviation of the windowed Gaussian distribution.
         * It defines the spread or width of the distribution, indicating how much the values deviate
         * from the mean.
         */
        
        double m_dev;
        
        /**
         * @brief The lower bound of the window for the Gaussian distribution.
         *
         * This member variable stores the lower limit (or cutoff) of the windowed Gaussian distribution.
         * It defines the starting point or minimum value of the range over which the Gaussian function is applied.
         */
        
        double m_lo;
        
        /**
         * @brief The upper bound of the window for the Gaussian distribution.
         *
         * This member variable stores the upper limit (or cutoff) of the windowed Gaussian distribution.
         * It defines the endpoint or maximum value of the range over which the Gaussian function is applied.
         */
        
        double m_hi;
    };
    
    /**
     * @brief Default constructor for the random_generator class that seeds the generator with a random value.
     *
     * This constructor initializes the random_generator by calling the `rand_seed()` method of the underlying
     * generator to automatically seed it with a random value. This ensures that the generator starts with a
     * non-deterministic seed, providing varied sequences of random numbers.
     */
    
    random_generator()                  { m_generator.rand_seed(); }
    
    /**
     * @brief Constructor for the random_generator class that seeds the generator with a user-provided seed.
     *
     * This constructor initializes the random_generator by calling the `seed()` method of the underlying
     * generator, using the seed values provided by the user. This allows for a deterministic sequence of
     * random numbers based on the given seed.
     *
     * @param init A pointer to an array of 32-bit unsigned integers used to seed the generator.
     */
    
    random_generator(uint32_t *init)    { m_generator.seed(init); }

    // Seeding (specific / random values)
    
    /**
     * @brief Seeds the underlying random number generator with a user-provided seed.
     *
     * This function initializes the internal state of the random generator by calling the `seed()` method
     * of the underlying generator and passing the provided seed values. This allows the random generator
     * to produce a deterministic sequence of random numbers based on the given seed.
     *
     * @param init A pointer to an array of 32-bit unsigned integers used to seed the generator.
     */
    
    void seed(uint32_t *init)   { m_generator.seed(init); }
    
    /**
     * @brief Seeds the underlying random number generator with a random value.
     *
     * This function calls the `rand_seed()` method of the underlying generator,
     * which automatically generates and uses a random seed to initialize the generator.
     * This results in a non-deterministic sequence of random numbers.
     */
    
    void rand_seed()            { m_generator.rand_seed(); }
    
    // Generate a Single Pseudo-random Unsigned Integer (full range /  in the range [0, n] / in the range [lo, hi])
    
    /**
     * @brief Generates and returns the next random 32-bit unsigned integer.
     *
     * This function produces the next pseudorandom number in the sequence by
     * invoking the underlying random number generator and returns it as a
     * 32-bit unsigned integer.
     *
     * @return A 32-bit unsigned integer representing the next random number.
     */
    
    uint32_t rand_int()
    {
        return m_generator();
    }
    
    /**
     * @brief Generates and returns a random 32-bit unsigned integer in the range [0, n).
     *
     * This function generates a random number using the underlying random number generator
     * and returns it as a 32-bit unsigned integer, constrained to the range from 0 to n-1.
     *
     * @param n The upper bound (exclusive) for the random number. The generated number
     *          will be in the range [0, n).
     * @return A 32-bit unsigned integer in the range [0, n).
     */
    
    uint32_t rand_int(uint32_t n)
    {
        uint32_t used = n;
        uint32_t i;
        
        used |= used >> 1;
        used |= used >> 2;
        used |= used >> 4;
        used |= used >> 8;
        used |= used >> 16;
        
        do
            i = rand_int() & used;   // toss unused bits shortens search
        while (i > n);
        
        return i;
    }
    
    /**
     * @brief Generates and returns a random 32-bit signed integer in the range [lo, hi].
     *
     * This function generates a random number using the underlying random number generator
     * and returns it as a 32-bit signed integer, constrained to the range between the specified
     * lower and upper bounds, inclusive.
     *
     * @param lo The lower bound (inclusive) of the random number range.
     * @param hi The upper bound (inclusive) of the random number range.
     * @return A 32-bit signed integer in the range [lo, hi].
     */
    
    int32_t rand_int(int32_t lo, int32_t hi)
    {
        return lo + rand_int(hi - lo);
    }
    
    // Generate a 32 bit Random Double (in the range [0,1] / in the range [0, n] / in the range [lo, hi])
    
    /**
     * @brief Generates and returns a random double in the range [0.0, 1.0).
     *
     * This function generates a random 32-bit unsigned integer using the underlying
     * random number generator, scales it to a floating-point value by multiplying
     * with 2.32830643653869628906e-10 (which is 1/2^32), and returns the result as a
     * double in the range [0.0, 1.0).
     *
     * @return A double-precision floating-point number in the range [0.0, 1.0).
     */
    
    double rand_double()                        { return rand_int() * 2.32830643653869628906e-10; }
    
    /**
     * @brief Generates and returns a random double in the range [0.0, n).
     *
     * This function generates a random double in the range [0.0, 1.0) by calling
     * `rand_double()`, then scales it by multiplying with the provided upper bound `n`,
     * resulting in a random double in the range [0.0, n).
     *
     * @param n The upper bound for the random double. The returned value will be in the range [0.0, n).
     * @return A double-precision floating-point number in the range [0.0, n).
     */
    
    double rand_double(double n)                { return rand_double() * n; }
    
    /**
     * @brief Generates and returns a random double in the range [lo, hi).
     *
     * This function generates a random double in the range [lo, hi) by first generating a random
     * double in the range [0.0, 1.0) using `rand_double()`, then scaling and shifting it to fit
     * within the specified range [lo, hi).
     *
     * @param lo The lower bound (inclusive) for the random double.
     * @param hi The upper bound (exclusive) for the random double.
     * @return A double-precision floating-point number in the range [lo, hi).
     */
    
    double rand_double(double lo, double hi)    { return lo + rand_double() * (hi - lo); }

    // Generate a 32 bit Random Double of Gaussian Distribution with given Mean / Deviation
    
    /**
     * @brief Generates and returns a random double following a Gaussian (normal) distribution.
     *
     * This function generates a random double based on a Gaussian distribution with the specified
     * mean and standard deviation. The result is a random value centered around `mean` with a spread
     * determined by `dev` (standard deviation).
     *
     * @param mean The mean (center) of the Gaussian distribution.
     * @param dev The standard deviation of the Gaussian distribution, which controls the spread of the values.
     * @return A double-precision floating-point number following a Gaussian distribution with the specified mean and deviation.
     */
    
    double rand_gaussian(double mean, double dev)
    {
        double x, y, R;
        
        rand_gaussians(x, y, R);
        
        return (R * x) * dev + mean;
    }
    
    // Generate two independent gaussians (Mean 0 and Deviation 1)
    
    /**
     * @brief Generates two independent random numbers following a Gaussian (normal) distribution.
     *
     * This function generates two random values `x` and `y` that are independent and follow a
     * Gaussian distribution with mean 0 and standard deviation 1. This is typically achieved
     * using the Box-Muller transform or a similar method.
     *
     * @param x Reference to the first generated random value following a standard Gaussian distribution.
     * @param y Reference to the second generated random value following a standard Gaussian distribution.
     */
    
    void rand_gaussians(double& x, double& y)
    {
        double R;
        
        rand_gaussians(x, y, R);
        
        x *= R;
        y *= R;
    }
    
    /**
     * @brief Generates and returns a random number following a windowed Gaussian distribution.
     *
     * This function generates a random number that follows a Gaussian distribution specified by the
     * given `params`, which include the mean, standard deviation, and window bounds. The random
     * number is constrained to lie within the window defined by the lower and upper bounds in
     * `params`.
     *
     * @param params A reference to a `windowed_gaussian_params` object that contains the mean,
     * standard deviation, and window bounds for the Gaussian distribution.
     * @return A double-precision floating-point number following the specified windowed Gaussian distribution.
     */
    
    double rand_windowed_gaussian(const windowed_gaussian_params& params)
    {
        const double r = ltqnorm(0.5 + 0.5 * rand_double(params.m_lo, params.m_hi)) * params.dev() + params.mean();
        return std::max(0.0, std::min(1.0, r));
    }
    
    /**
     * @brief Generates and returns a random number following a windowed Gaussian distribution with default bounds.
     *
     * This function generates a random number that follows a Gaussian distribution with the specified
     * mean and standard deviation. The result is constrained within a default window, typically defined
     * by some reasonable limits (e.g., several standard deviations from the mean).
     *
     * @param mean The mean (center) of the Gaussian distribution.
     * @param dev The standard deviation (spread) of the Gaussian distribution.
     * @return A double-precision floating-point number following the specified windowed Gaussian distribution.
     */
    
    double rand_windowed_gaussian(double mean, double dev)
    {
        const windowed_gaussian_params params(mean, dev);
        return rand_windowed_gaussian(params);
    }
    
private:

    // Gaussian Helper
    
    /**
     * @brief Generates two independent random numbers following a Gaussian (normal) distribution and their radius.
     *
     * This function generates two random values `x` and `y` that follow a standard Gaussian distribution
     * with mean 0 and standard deviation 1. Additionally, it calculates and returns the radius `R`,
     * which represents the distance from the origin in a 2D Gaussian-distributed space (often used in
     * Box-Muller or polar coordinate methods).
     *
     * @param x Reference to the first generated random value following a standard Gaussian distribution.
     * @param y Reference to the second generated random value following a standard Gaussian distribution.
     * @param R Reference to the radius, which is the distance from the origin in 2D Gaussian space.
     */
    
    void rand_gaussians(double& x, double& y, double& R)
    {
        x = 0.0;
        y = 0.0;
        R = 0.0;
        
        while (R >= 1.0 || R == 0.0)
        {
            x = rand_double(-1.0, 1.0);
            y = rand_double(-1.0, 1.0);
            R = (x * x) + (y * y);
        }
        
        R = sqrt((-2.0 * std::log(R)) / R);
    }
    
    // This is adapted from http://home.online.no/~pjacklam/notes/invnorm/impl/sprouse/ltqnorm.c

    /*
     * Lower tail quantile for standard normal distribution function.
     *
     * This function returns an approximation of the inverse cumulative
     * standard normal distribution function.  I.e., given P, it returns
     * an approximation to the X satisfying P = Pr{Z <= X} where Z is a
     * random variable from the standard normal distribution.
     *
     * The algorithm uses a minimax approximation by rational functions
     * and the result has a relative error whose absolute value is less
     * than 1.15e-9.
     *
     * Author:      Peter J. Acklam
     * Time-stamp:  2002-06-09 18:45:44 +0200
     * E-mail:      jacklam@math.uio.no
     * WWW URL:     http://www.math.uio.no/~jacklam
     *
     */

    double ltqnorm(double p)
    {
        /* Coefficients in rational approximations. */

        constexpr double a[] =
        {
            -3.969683028665376e+01,
            2.209460984245205e+02,
            -2.759285104469687e+02,
            1.383577518672690e+02,
            -3.066479806614716e+01,
            2.506628277459239e+00
        };

        constexpr double b[] =
        {
            -5.447609879822406e+01,
            1.615858368580409e+02,
            -1.556989798598866e+02,
            6.680131188771972e+01,
            -1.328068155288572e+01
        };

        constexpr double c[] =
        {
            -7.784894002430293e-03,
            -3.223964580411365e-01,
            -2.400758277161838e+00,
            -2.549732539343734e+00,
            4.374664141464968e+00,
            2.938163982698783e+00
        };

        constexpr double d[] =
        {
            7.784695709041462e-03,
            3.224671290700398e-01,
            2.445134137142996e+00,
            3.754408661907416e+00
        };

        constexpr double low = 0.02425;
        constexpr double high = 0.97575;
        
        double q, r;
        
        errno = 0;
        
        if (p < 0 || p > 1)
        {
            errno = EDOM;
            return 0.0;
        }
        else if (p == 0)
        {
            errno = ERANGE;
            return -HUGE_VAL    /* minus "infinity" */;
        }
        else if (p == 1)
        {
            errno = ERANGE;
            return HUGE_VAL        /* "infinity" */;
        }
        else if (p < low)
        {
            /* Rational approximation for lower region */
            
            q = sqrt(-2.0*std::log(p));
            return (((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5]) /
            ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1);
        }
        else if (p > high)
        {
            /* Rational approximation for upper region */
            
            q  = sqrt(-2.0*std::log(1-p));
            return -(((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5]) /
            ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1);
        }
        else
        {
            /* Rational approximation for central region */
            
            q = p - 0.5;
            r = q*q;
            return (((((a[0]*r+a[1])*r+a[2])*r+a[3])*r+a[4])*r+a[5])*q /
            (((((b[0]*r+b[1])*r+b[2])*r+b[3])*r+b[4])*r+1);
        }
    }
    // State
    
    /**
     * @brief The instance of the random number generator used by the random_generator class.
     *
     * This member variable holds the underlying random number generator, which is specified by
     * the `Generator` template parameter. It is responsible for generating the random numbers
     * used by the random_generator class.
     */
    
    Generator m_generator;
};

#endif
