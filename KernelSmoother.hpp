
/**
 * @file KernelSmoother.hpp
 *
 * @brief This file defines the `kernel_smoother` class and associated methods for applying kernel smoothing
 *        and spectral processing on input data.
 *
 * The `kernel_smoother` class provides functionality for smoothing data using various kernel-based methods,
 * including FFT-based approaches for efficient convolution. The class supports different edge-handling strategies
 * and allows users to specify filter parameters such as width, gain, and symmetry. The file also includes utility
 * classes and methods for managing input data, applying filters, and handling FFT operations.
 *
 * Key Components:
 * - `kernel_smoother`: The main class that provides kernel smoothing capabilities.
 * - Edge handling strategies through `Ends` and `EdgeMode` enumerations.
 * - FFT-based filtering methods for efficient convolution.
 * - Utility methods for creating and applying filters to data.
 *
 * @note This file contains template-based functions and classes to support a wide range of data types and customizable behavior.
 */

#ifndef KERNELSMOOTHER_HPP
#define KERNELSMOOTHER_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <type_traits>

#include "Allocator.hpp"
#include "SIMDSupport.hpp"
#include "SpectralProcessor.hpp"
#include "TableReader.hpp"

/**
 * @brief A class that performs kernel smoothing on spectral data.
 *
 * This template class `kernel_smoother` is designed to smooth spectral data using a specified kernel.
 * It inherits from `spectral_processor<T, Allocator>` to leverage spectral processing capabilities,
 * and operates on data types defined by the template parameters.
 *
 * @tparam T The type of data being processed (e.g., float, double).
 * @tparam Allocator The allocator type for memory management, defaults to `aligned_allocator` for optimized memory alignment.
 * @tparam auto_resize_fft A boolean flag indicating whether the FFT size should automatically resize to accommodate different data sizes.
 *                        Defaults to `false`, meaning the FFT size remains fixed unless manually changed.
 */

template <typename T, typename Allocator = aligned_allocator, bool auto_resize_fft = false>
class kernel_smoother : private spectral_processor<T, Allocator>
{
    
    /**
     * @typedef processor
     *
     * An alias for the `spectral_processor` class template specialized with the types `T` and `Allocator`.
     *
     * This typedef simplifies access to the `spectral_processor` base class, allowing the `kernel_smoother`
     * class to refer to it more conveniently.
     *
     * @tparam T The data type being processed by the spectral processor (e.g., float, double).
     * @tparam Allocator The allocator type used for memory management in the spectral processor.
     */
    
    using processor = spectral_processor<T, Allocator>;
    
    /**
     * @typedef op_sizes
     *
     * An alias for the `op_sizes` type defined in the `processor` class (i.e., `spectral_processor<T, Allocator>`).
     *
     * This typedef provides access to the internal structure or type that handles operational sizes
     * within the `spectral_processor` class, which could involve FFT sizes or buffer lengths used
     * during spectral processing.
     *
     * @tparam T The data type being processed by the spectral processor.
     * @tparam Allocator The allocator type used in the spectral processor.
     */
    
    using op_sizes = typename processor::op_sizes;
    
    /**
     * @typedef zipped_pointer
     *
     * An alias for the `zipped_pointer` type defined in the `processor` class (i.e., `spectral_processor<T, Allocator>`).
     *
     * This typedef provides access to a specialized pointer type used in the `spectral_processor` class,
     * likely designed to point to multiple data arrays (zipped together) for efficient processing in
     * spectral operations.
     *
     * @tparam T The data type being processed by the spectral processor.
     * @tparam Allocator The allocator type used in the spectral processor.
     */
    
    using zipped_pointer = typename processor::zipped_pointer;
    
    /**
     * @typedef in_ptr
     *
     * An alias for the `in_ptr` type defined in the `processor` class (i.e., `spectral_processor<T, Allocator>`).
     *
     * This typedef represents a pointer type used to access input data in the `spectral_processor` class,
     * allowing for efficient manipulation and processing of the input data during spectral operations.
     *
     * @tparam T The data type being processed by the spectral processor.
     * @tparam Allocator The allocator type used in the spectral processor.
     */
    
    using in_ptr = typename processor::in_ptr;
 
    /**
     * @typedef Split
     *
     * An alias for the `Split` type defined within the `FFTTypes<T>` class.
     *
     * This typedef simplifies the usage of the `Split` type for the current
     * template type `T`, allowing easier access to a specialized FFT split data structure.
     *
     * @tparam T The data type for which the FFT types and associated split types are defined.
     */
    
    using Split = typename FFTTypes<T>::Split;
    
    /**
     * @typedef enable_if_t
     *
     * A template alias for `std::enable_if`, which is used for SFINAE (Substitution Failure Is Not An Error)
     * to conditionally enable or disable template specializations or functions based on a boolean constant.
     *
     * If the boolean value `B` is true, `enable_if_t` resolves to `int`. Otherwise, this alias is invalid,
     * and the function or specialization is excluded from the overload set.
     *
     * @tparam B A boolean constant used to control whether the alias is enabled or disabled.
     */
    
    template <bool B>
    using enable_if_t = typename std::enable_if<B, int>::type;
    
    /**
     * @enum Ends
     *
     * @brief An enumeration that defines different types of endpoint behaviors for kernel smoothing or spectral processing.
     *
     * This enum is used to specify how the ends or boundaries of the data should be handled during processing.
     * Different options allow for various treatments, such as zero-padding or maintaining non-zero values.
     *
     * - `Zero`: The endpoints are set to zero.
     * - `NonZero`: The endpoints maintain their non-zero values.
     * - `SymZero`: The endpoints are symmetrically zero-padded.
     * - `SymNonZero`: The endpoints are symmetrically handled with non-zero values.
     */
    
    enum class Ends { Zero, NonZero, SymZero, SymNonZero };
    
public:
    
    /**
     * @enum EdgeMode
     *
     * @brief An enumeration that defines various strategies for handling edge conditions in kernel smoothing or spectral processing.
     *
     * This enum specifies how the edges of the data should be treated when applying filters or transformations, particularly when data at the boundaries is required for computations.
     *
     * - `ZeroPad`: The edges are padded with zeros.
     * - `Extend`: The edge values are extended by repeating the boundary value.
     * - `Wrap`: The data wraps around, treating the end as if it continues from the beginning.
     * - `Fold`: The data is folded over, mirroring at the edges.
     * - `Mirror`: The data is reflected at the boundaries.
     */
    
    enum class EdgeMode { ZeroPad, Extend, Wrap, Fold, Mirror };
    
    /**
     * @brief Constructs a `kernel_smoother` object with an optional maximum FFT size.
     *
     * This constructor initializes the `kernel_smoother` object, and it is only enabled if the `Allocator`
     * type is default constructible. The constructor also initializes the base class `spectral_processor<T, Allocator>`
     * with the given maximum FFT size.
     *
     * @tparam U The allocator type, which defaults to the `Allocator` type defined for the class.
     *           This template parameter is only enabled if `U` is default constructible.
     *
     * @param max_fft_size The maximum size for the FFT, defaulting to \(2^{18}\) (262144). This size determines the largest FFT that the kernel smoother can handle.
     *
     * @note This constructor is conditionally enabled using SFINAE, meaning it is only available if the `Allocator` type
     *       is default constructible.
     */
    
    template <typename U = Allocator, enable_if_t<std::is_default_constructible<U>::value> = 0>
    kernel_smoother(uintptr_t max_fft_size = 1 << 18)
    : spectral_processor<T, Allocator>(max_fft_size)
    {}
    
    /**
     * @brief Constructs a `kernel_smoother` object with a custom allocator and an optional maximum FFT size.
     *
     * This constructor initializes the `kernel_smoother` object using a provided allocator and optionally a maximum FFT size.
     * It is only enabled if the `Allocator` type is copy constructible. The constructor also passes the allocator
     * and FFT size to the base class `spectral_processor<T, Allocator>`.
     *
     * @tparam U The allocator type, which defaults to the `Allocator` type defined for the class.
     *           This template parameter is only enabled if `U` is copy constructible.
     *
     * @param allocator A reference to the allocator used for managing memory within the `kernel_smoother`.
     * @param max_fft_size The maximum size for the FFT, defaulting to \(2^{18}\) (262144). This size determines the largest FFT that the kernel smoother can handle.
     *
     * @note This constructor is conditionally enabled using SFINAE, meaning it is only available if the `Allocator` type
     *       is copy constructible.
     */
    
    template <typename U = Allocator, enable_if_t<std::is_copy_constructible<U>::value> = 0>
    kernel_smoother(const Allocator& allocator, uintptr_t max_fft_size = 1 << 18)
    : spectral_processor<T, Allocator>(allocator, max_fft_size)
    {}
    
    /**
     * @brief Constructs a `kernel_smoother` object using a move-constructed allocator and an optional maximum FFT size.
     *
     * This constructor initializes the `kernel_smoother` object using a move-constructed allocator and an optional
     * maximum FFT size. It is only enabled if the `Allocator` type is move constructible. The allocator and FFT size
     * are passed to the base class `spectral_processor<T, Allocator>`.
     *
     * @tparam U The allocator type, which defaults to the `Allocator` type defined for the class.
     *           This template parameter is only enabled if `U` is move constructible.
     *
     * @param allocator An rvalue reference to the allocator, which is move-constructed to manage memory within the `kernel_smoother`.
     * @param max_fft_size The maximum size for the FFT, defaulting to \(2^{18}\) (262144). This size determines the largest FFT that the kernel smoother can handle.
     *
     * @note This constructor is conditionally enabled using SFINAE, meaning it is only available if the `Allocator` type
     *       is move constructible.
     */
    
    template <typename U = Allocator, enable_if_t<std::is_move_constructible<U>::value> = 0>
    kernel_smoother(const Allocator&& allocator, uintptr_t max_fft_size = 1 << 18)
    : spectral_processor<T, Allocator>(allocator, max_fft_size)
    {}

    /**
     * @brief Sets the maximum size for the FFT operations in the kernel smoother.
     *
     * This method updates the maximum FFT size used by the `kernel_smoother` for spectral processing. It forwards
     * the size to the base class `spectral_processor` to configure the internal FFT size limit.
     *
     * @param size The maximum FFT size to set. This value defines the upper limit for the size of FFT that
     *             the smoother can handle.
     */
    
    void set_max_fft_size(uintptr_t size) { processor::set_max_fft_size(size); }
    
    /**
     * @brief Retrieves the maximum size for FFT operations in the kernel smoother.
     *
     * This method returns the current maximum FFT size used by the `kernel_smoother`,
     * as defined in the base class `spectral_processor`.
     *
     * @return The maximum FFT size that the kernel smoother can handle.
     */
    
    uintptr_t max_fft_size() { return processor::max_fft_size(); }

    /**
     * @brief Performs kernel smoothing on the input data.
     *
     * This method applies kernel smoothing to the input data `in` and stores the result in the output array `out`.
     * The smoothing is done using a specified kernel and various parameters that control the width, symmetry, and
     * edge behavior.
     *
     * @param out A pointer to the output array where the smoothed data will be stored.
     * @param in A pointer to the input data array that will be smoothed.
     * @param kernel A pointer to the kernel array used for smoothing the data.
     * @param length The length of the input data array.
     * @param kernel_length The length of the kernel array.
     * @param width_lo The lower bound width scaling factor for the kernel smoothing.
     * @param width_hi The upper bound width scaling factor for the kernel smoothing.
     * @param symmetric A boolean flag indicating whether the smoothing should be symmetric.
     *                  If `true`, the kernel is applied symmetrically around the data points.
     * @param edges Specifies how the edges of the input data should be handled, using the `EdgeMode` enumeration.
     *
     * @note The behavior at the edges is controlled by the `EdgeMode` parameter, which determines how data is treated
     *       at the boundaries during the smoothing process.
     */
    
    void smooth(T *out, const T *in, const T *kernel, uintptr_t length, uintptr_t kernel_length, double width_lo, double width_hi, bool symmetric, EdgeMode edges)
    {
        if (!length || !kernel_length)
            return;
        
        Allocator& allocator = processor::m_allocator;
        
        const int N = SIMDLimits<T>::max_size;
        
        width_lo = std::min(static_cast<double>(length), std::max(1.0, width_lo));
        width_hi = std::min(static_cast<double>(length), std::max(1.0, width_hi));
        
        double width_mul = (width_hi - width_lo) / (length - 1);
        
        auto half_width_calc = [&](uintptr_t a)
        {
            return static_cast<uintptr_t>(std::round((width_lo + a * width_mul) * 0.5));
        };
        
        uintptr_t filter_size = static_cast<uintptr_t>(std::ceil(std::max(width_lo, width_hi) * 0.5));
        uintptr_t filter_full = filter_size * 2 - 1;
        uintptr_t max_per_filter = static_cast<uintptr_t>(width_mul ? (2.0 / std::abs(width_mul)) + 1.0 : length);
        uintptr_t data_width = max_per_filter + (filter_full - 1);
        
        op_sizes sizes(data_width, filter_full, processor::EdgeMode::Linear);
        
        if (auto_resize_fft && processor::max_fft_size() < sizes.fft())
            set_max_fft_size(sizes.fft());
        
        uintptr_t fft_size = processor::max_fft_size() >= sizes.fft() ? sizes.fft() : 0;
        
        T *ptr = allocator.template allocate<T>(fft_size * 2 + filter_full + length + filter_size * 2);
        Split io { ptr, ptr + (fft_size >> 1) };
        Split st { io.realp + fft_size, io.imagp + fft_size };
        T *filter = ptr + (fft_size << 1);
        T *padded = filter + filter_full;
        
        Ends ends = Ends::NonZero;
        
        if (kernel_length)
        {
            const T max_value = *std::max_element(kernel, kernel + kernel_length);
            const T test_value_1 = kernel[0] / max_value;
            const T test_value_2 = kernel[kernel_length - 1] / max_value;
            const T epsilon = std::numeric_limits<T>::epsilon();
            
            if ((symmetric || test_value_1 < epsilon) && test_value_2 < epsilon)
                ends = symmetric ? Ends::SymZero : Ends::Zero;
        }
        
        // Copy data
        
        switch (edges)
        {
            case EdgeMode::ZeroPad:
                std::fill_n(padded, filter_size, 0.0);
                std::copy_n(in, length, padded + filter_size);
                std::fill_n(padded + filter_size + length, filter_size, 0.0);
                break;
                
            case EdgeMode::Extend:
                std::fill_n(padded, filter_size, in[0]);
                std::copy_n(in, length, padded + filter_size);
                std::fill_n(padded + filter_size + length, filter_size, in[length - 1]);
                break;
                
            case EdgeMode::Wrap:
                copy_edges<table_fetcher_wrap>(in, padded, length, filter_size);
            break;
                
            case EdgeMode::Fold:
                copy_edges<table_fetcher_fold>(in, padded, length, filter_size);
                break;
                
            case EdgeMode::Mirror:
                copy_edges<table_fetcher_mirror>(in, padded, length, filter_size);
                break;
        }
        
        if (symmetric)
        {
            // Offsets into the data and the filter
        
            const T *data = padded + filter_size;
            filter += filter_size - 1;
        
            // Symmetric filtering
            
            for (uintptr_t i = 0, j = 0; i < length; i = j)
            {
                const uintptr_t half_width = static_cast<uintptr_t>(half_width_calc(i));
                const uintptr_t width = half_width * 2 - 1;
                const T filter_half_sum = make_filter(filter, kernel, kernel_length, half_width, ends);
                const T filter_sum = (filter_half_sum * T(2) - filter[0]);
                const T gain = filter_sum ? T(1) / filter_sum : 1.0;

                for (j = i; (j < length) && half_width == half_width_calc(j); j++);
                
                uintptr_t n = j - i;
                uintptr_t m = use_fft_n(n, half_width, fft_size);
                uintptr_t k = 0;
                
                const double *data_fft = data - (half_width - 1);
                const double *filter_fft = filter - (half_width - 1);

                // Mirror the filter if required for the FFT processing

                if (m)
                {
                    for (intptr_t i = 1; i < static_cast<intptr_t>(half_width); i++)
                        filter[-i] = filter[i];
                }
                
                for (; k + (m - 1) < n; k += m)
                    apply_filter_fft(out + i + k, data_fft + i + k, filter_fft, io, st, width, m, gain);
                
                for (; k + (N - 1) < n; k += N)
                    apply_filter_symmetric<N>(out + i + k, data + i + k, filter, half_width, gain);
                
                for (; k < n; k++)
                    apply_filter_symmetric<1>(out + i + k, data + i + k, filter, half_width, gain);
            }
        }
        else
        {
            // Non-symmetric filtering

            for (uintptr_t i = 0, j = 0; i < length; i = j)
            {
                const uintptr_t half_width = static_cast<uintptr_t>(half_width_calc(i));
                const uintptr_t width = half_width * 2 - 1;
                const T filter_sum = make_filter(filter, kernel, kernel_length, width, ends);
                const T gain = filter_sum ? T(1) / filter_sum : 1.0;
                
                const T *data = padded + filter_size - (half_width - 1);

                for (j = i; (j < length) && half_width == half_width_calc(j); j++);
                
                uintptr_t n = j - i;
                uintptr_t m = use_fft_n(n, half_width, fft_size);
                uintptr_t k = 0;
                
                for (; k + (m - 1) < n; k += m)
                    apply_filter_fft(out + i + k, data + i + k, filter, io, st, width, m, gain);
                
                for (; k + (N - 1) < n; k += N)
                    apply_filter<N>(out + i + k, data + i + k, filter, width, gain);
                
                for (; k < n; k++)
                    apply_filter<1>(out + i + k, data + i + k, filter, width, gain);
            }
        }
        
        allocator.deallocate(ptr);
    }
    
private:
    
    /**
     * @brief A struct that specializes `table_fetcher` for `double` precision values.
     *
     * The `fetcher` struct inherits from `table_fetcher<double>` and provides functionality for fetching
     * or processing table-based data with double precision. It may include additional functionality
     * or specializations specific to the `kernel_smoother`.
     *
     * @see table_fetcher
     */
    
    struct fetcher : table_fetcher<double>
    {
        
        /**
         * @brief Constructs a `fetcher` object for fetching data from a given input array.
         *
         * This constructor initializes the `fetcher` by passing the size of the input data and a scaling factor
         * to the base class `table_fetcher`. It also stores a pointer to the input data array.
         *
         * @param in A pointer to the input data array that the `fetcher` will operate on.
         * @param size The size (number of elements) of the input data array.
         *
         * @note The constructor passes a scaling factor of 1.0 to the `table_fetcher` base class.
         */
        
        fetcher(const T *in, intptr_t size)
        : table_fetcher<T>(size, 1.0), data(in) {}
        
        /**
         * @brief Retrieves the value at the specified index from the input data array.
         *
         * This function call operator allows `fetcher` objects to be used like functions to access
         * the data at a given index. It returns the value from the input data array at the specified index.
         *
         * @param idx The index of the data element to retrieve.
         * @return The value of type `T` located at the given index in the input data array.
         */
        
        T operator()(intptr_t idx) { return data[idx]; }
        
        /**
         * @brief A pointer to the input data array.
         *
         * This member stores the address of the input data that the `fetcher` operates on.
         * It is a constant pointer to ensure that the data cannot be modified through this member.
         *
         * @note The data is of type `T` and is used for fetching or processing elements by the `fetcher`.
         */
        
        const T *data;
    };

    /**
     * @brief Copies the edges of the input data to the output array with a specified filter size.
     *
     * This method is responsible for copying the edges of the input data to the output data, handling the boundaries according to a given filter size. The method template allows for a customizable edge-handling strategy, determined by the template template parameter `U`.
     *
     * @tparam U A template template parameter representing a class template that defines the edge handling strategy.
     *           It takes a single type `V` as its template argument.
     * @param in A pointer to the input data array.
     * @param out A pointer to the output data array where the edge-handled values will be copied.
     * @param length The length of the input data array.
     * @param filter_size The size of the filter to be applied to the edges.
     *
     * @note This function handles edge conditions using the specified template template parameter `U` to define how the edges are copied or processed.
     */
    
    template <template <class V> class U>
    void copy_edges(const T *in, T *out, intptr_t length, intptr_t filter_size)
    {
        intptr_t in_size = static_cast<intptr_t>(length);
        intptr_t edge_size = static_cast<intptr_t>(filter_size);
        
        U<fetcher> fetch(fetcher(in, in_size));
            
        for (intptr_t i = 0; i < edge_size; i++)
            out[i] = fetch(i - edge_size);
        
        std::copy_n(in, in_size, out + edge_size);
        
        for (intptr_t i = 0; i < edge_size; i++)
            out[i + in_size + edge_size] = fetch(i + in_size);
    }
    
    /**
     * @brief Determines the optimal FFT size to use based on the input size, half-width, and a given FFT size.
     *
     * This method calculates the appropriate FFT size to be used for kernel smoothing or spectral processing.
     * It takes into account the input size `n`, the half-width of the kernel, and the available `fft_size`.
     *
     * @param n The size of the input data for which the FFT will be applied.
     * @param half_width The half-width of the kernel, which affects the size of the data segment to be processed by the FFT.
     * @param fft_size The available FFT size, which serves as the upper limit for the FFT operation.
     *
     * @return The FFT size to use, calculated based on the input parameters.
     *
     * @note This function ensures that the chosen FFT size accommodates both the input size and the kernel width,
     *       without exceeding the specified maximum FFT size.
     */
    
    uintptr_t use_fft_n(uintptr_t n, uintptr_t half_width, uintptr_t fft_size)
    {
        bool use_fft = fft_size && n > 64 && half_width > 16 && (half_width * 64 > n);
        
        return use_fft ? n : 0;
    }

    /**
     * @brief Retrieves the value of the filter kernel at a specific position.
     *
     * This method computes or retrieves the value of the kernel at a given `position`. The kernel is passed
     * as an array, and the `position` determines where in the kernel the value is fetched or interpolated.
     *
     * @param kernel A pointer to the kernel array containing the filter values.
     * @param position The position in the kernel, specified as a double, from which the value will be retrieved or computed.
     *
     * @return The value of type `T` from the kernel at the specified `position`.
     *
     * @note Depending on how the kernel is structured, the `position` may involve interpolation between values.
     */
    
    T filter_kernel(const T *kernel, double position)
    {
        uintptr_t index = static_cast<uintptr_t>(position);
        
        const T lo = kernel[index];
        const T hi = kernel[index + 1];
        
        return static_cast<T>(lo + (position - index) * (hi - lo));
    }
    
    /**
     * @brief Creates a filter based on the provided kernel and parameters.
     *
     * This method constructs a filter using the specified kernel, its length, and the desired width of the filter.
     * It also considers how the ends of the filter should be handled, based on the `Ends` enumeration.
     *
     * @param filter A pointer to the output array where the constructed filter will be stored.
     * @param kernel A pointer to the kernel array used for constructing the filter.
     * @param kernel_length The length of the kernel array.
     * @param width The width of the filter, which determines the scaling or extent of the kernel when constructing the filter.
     * @param ends Specifies how the ends of the filter should be treated, using the `Ends` enumeration. This controls edge-handling behavior for the filter.
     *
     * @return The constructed filter value of type `T`.
     *
     * @note The ends handling is based on the `Ends` enumeration, which could involve zero-padding, symmetric handling, or other boundary behaviors.
     */
    
    T make_filter(T *filter, const T *kernel, uintptr_t kernel_length, uintptr_t width, Ends ends)
    {
        if (kernel_length == 1)
        {
            std::fill_n(filter, width, kernel[0]);
            return filter[0] * width;
        }
        
        const double width_adjust = (ends == Ends::NonZero) ? -1.0 : (ends == Ends::SymZero ? 0.0 : 1.0);
        const double scale_width = std::max(1.0, width + width_adjust);
        const double width_normalise = static_cast<double>(kernel_length - 1) / scale_width;
        
        uintptr_t offset = ends == Ends::Zero ? 1 : 0;
        uintptr_t loop_size = ends == Ends::NonZero ? width - 1 : width;
        
        T filter_sum(0);
        
        for (uintptr_t j = 0; j < loop_size; j++)
        {
            filter[j] = filter_kernel(kernel, (j + offset) * width_normalise);
            filter_sum += filter[j];
        }
        
        if (ends == Ends::NonZero)
        {
            filter[width - 1] = kernel[kernel_length - 1];
            filter_sum += filter[width - 1];
        }
        
        return filter_sum;
    }
    
    /**
     * @brief Applies a filter to the input data and stores the result in the output array.
     *
     * This templated method applies a filter of fixed size `N` to the input data, using a specified width and gain.
     * The result is stored in the output array. The filter is convolved with the input data over the specified width.
     *
     * @tparam N The fixed size of the filter to be applied. This is a template parameter that determines the filter's length.
     *
     * @param out A pointer to the output array where the filtered data will be stored.
     * @param data A pointer to the input data array that will be processed with the filter.
     * @param filter A pointer to the filter array that will be applied to the input data.
     * @param width The width over which the filter will be applied to the data.
     * @param gain A scaling factor applied to the result of the filtering process, allowing for amplitude adjustment.
     *
     * @note This method performs a convolution or similar filtering operation using the provided filter and data,
     *       with the filter size determined by the template parameter `N`.
     */
    
    template <int N>
    void apply_filter(T *out, const T *data, const T *filter, uintptr_t width, T gain)
    {
        using VecType = SIMDType<double, N>;
        
        VecType filter_val = filter[width - 1] * VecType(data);
        
        for (uintptr_t j = 1; j < width; j++)
            filter_val += filter[width - (j + 1)] * VecType(data + j);
        
        filter_val *= gain;
        filter_val.store(out);
    }
    
    /**
     * @brief Applies a symmetric filter to the input data and stores the result in the output array.
     *
     * This templated method applies a symmetric filter of fixed size `N` to the input data. The filter is
     * applied symmetrically around the data points, using the specified half-width and gain, with the result
     * stored in the output array.
     *
     * @tparam N The fixed size of the filter to be applied. This is a template parameter that determines the filter's length.
     *
     * @param out A pointer to the output array where the symmetrically filtered data will be stored.
     * @param data A pointer to the input data array that will be processed using the symmetric filter.
     * @param filter A pointer to the filter array that will be applied symmetrically to the input data.
     * @param half_width The half-width of the filter, which determines the symmetric range of data points the filter is applied to.
     * @param gain A scaling factor applied to the result of the symmetric filtering process, allowing for amplitude adjustment.
     *
     * @note This method performs a symmetric convolution or filtering operation using the provided filter and data,
     *       with the filter size determined by the template parameter `N` and applied symmetrically around the data points.
     */
    
    template <int N>
    void apply_filter_symmetric(T *out, const T *data, const T *filter, uintptr_t half_width, T gain)
    {
        using VecType = SIMDType<double, N>;
        
        VecType filter_val = filter[0] * VecType(data);
        
        for (uintptr_t j = 1; j < half_width; j++)
            filter_val += filter[j] * (VecType(data - j) + VecType(data + j));
        
        filter_val *= gain;
        filter_val.store(out);
    }
    
    /**
     * @brief Applies a filter to the input data using FFT (Fast Fourier Transform) and stores the result in the output array.
     *
     * This method uses FFT-based convolution to apply the filter to the input data. The input and filter are processed
     * in the frequency domain, utilizing the provided FFT splits, with the result stored in the output array. The method
     * also allows for a gain factor to be applied to the final result.
     *
     * @param out A pointer to the output array where the FFT-filtered data will be stored.
     * @param data A pointer to the input data array that will be processed using FFT-based filtering.
     * @param filter A pointer to the filter array, which will be applied to the input data via FFT.
     * @param io A reference to a `Split` object, which holds the input/output data used during the FFT operations.
     * @param temp A reference to a `Split` object, used as temporary storage during FFT processing.
     * @param width The width of the data array over which the FFT-based filtering will be applied.
     * @param n The total number of data points to be processed via FFT.
     * @param gain A scaling factor applied to the final result after filtering, allowing for amplitude adjustment.
     *
     * @note This method performs filtering using FFT, which can be more efficient for large data sets than time-domain convolution.
     */
    
    void apply_filter_fft(T *out, const T *data, const T *filter, Split& io, Split& temp, uintptr_t width, uintptr_t n, T gain)
    {
        uintptr_t data_width = n + width - 1;
        op_sizes sizes(data_width, width, processor::EdgeMode::Linear);
        in_ptr data_in(data, data_width);
        in_ptr filter_in(filter, width);
        
        // Process
        
        processor::template binary_op<ir_convolve_real>(io, temp, sizes, data_in, filter_in);
        
        // Copy output with scaling
        
        zipped_pointer p(io, width - 1);
        
        for (uintptr_t i = 0; i < n; i++)
            out[i] = *p++ * gain;
    }
};

#endif
