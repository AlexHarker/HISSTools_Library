
/**
 * @file TableReader.hpp
 * @brief This file provides a variety of table readers and methods for retrieving values from tables with different interpolation techniques and edge handling modes.
 *
 * The `TableReader.hpp` file defines several structures and functions designed to fetch values from tables using a wide range of interpolation methods
 * (e.g., linear, cubic B-spline, cubic Hermite, cubic Lagrange) and edge handling techniques (e.g., zero-padding, extension, wrapping, folding, mirroring, extrapolation).
 *
 * The file supports the following features:
 *
 * - **Interpolation Methods**: Linear, cubic B-spline, cubic Hermite, cubic Lagrange, and others.
 * - **Edge Handling Modes**: Zero-padding, extension, wrapping, folding, mirroring, and extrapolation.
 * - **Scalable Table Fetching**: Efficient methods to read multiple samples from a table, apply scaling factors, and store results in an output buffer.
 * - **Template-Based Flexibility**: The structures and functions are designed to be highly customizable through template parameters for types, table fetchers, and readers.
 *
 * It contains:
 * - Structs for different readers, including `linear_reader`, `cubic_bspline_reader`, `cubic_hermite_reader`, and more.
 * - Functions such as `table_read`, `table_read_loop`, `table_read_optional_bound`, `table_read_edges`, which provide various ways to fetch and process table values.
 *
 * The file is well-suited for applications involving digital signal processing (DSP), scientific computing, and other areas where interpolation of sampled data is needed.
 */

#ifndef TABLEREADER_HPP
#define TABLEREADER_HPP

#include "SIMDSupport.hpp"
#include "Interpolation.hpp"
#include <algorithm>

// Enumeration of edge types

/**
 * @brief Specifies the edge handling modes for table operations.
 *
 * This enumeration defines the various modes for handling edges when performing table lookups
 * or operations that may require accessing values outside the bounds of the table.
 *
 * - `ZeroPad`: Pads with zeros for out-of-bounds values.
 * - `Extend`: Uses the nearest value for out-of-bounds indices.
 * - `Wrap`: Wraps around to the beginning of the table.
 * - `Fold`: Reflects back at the edge.
 * - `Mirror`: Mirrors the table across the boundary.
 * - `Extrapolate`: Extrapolates values based on the trend of the data.
 */

enum class EdgeMode { ZeroPad, Extend, Wrap, Fold, Mirror, Extrapolate };

// Base class for table fetchers

// Implementations
// - Must provide: T operator()(intptr_t idx) - which does the fetching of values
// - Adaptors may also provide: template <class U, V> void split(U position, intptr_t& idx, V& fract, int N)
// - which generates the idx and fractional interpolation values and may additionally constrain them
// - intptr_t limit() - which should return the highest valid position for bounds etc.
// - void prepare(InterpType interpolation)  - which prepares the table (e.g. for extrapolation) if necessary

/**
 * @brief A template structure for fetching values from a table with specified operations.
 *
 * The `table_fetcher` template structure is designed to fetch values from a table using a
 * generic type `T`. The fetching mechanism can involve operations like scaling,
 * indexing, and handling edges based on the specified mode.
 *
 * @tparam T The type of data stored in the table.
 */

template <class T>
struct table_fetcher
{
    
    /**
     * @brief Defines the type used for fetching data from the table.
     *
     * The `fetch_type` is an alias for the template parameter `T`, representing the type
     * of data that the `table_fetcher` will fetch from the table.
     */
    
    typedef T fetch_type;
    
    /**
     * @brief Constructs a table_fetcher object with a specified length and scale value.
     *
     * This constructor initializes the table_fetcher object using the given length and scale values.
     *
     * @param length The length of the table to be fetched.
     * @param scale_val The scale value to be applied to the fetched table.
     */
    
    table_fetcher(intptr_t length, double scale_val) : size(length), scale(scale_val) {}
    
    /**
     * @brief Splits a position into an integer index and a fractional part for table access.
     *
     * The `split` method decomposes a given position into two components: an integer index (`idx`)
     * and a fractional part (`fract`). This is useful for interpolation or other operations
     * where the position is not an exact integer. The method is templated to allow flexibility
     * with the types of position and fractional parts.
     *
     * @tparam U The type of the position to be split.
     * @tparam V The type for the fractional part.
     *
     * @param position The position value to be split into an integer and fractional part.
     * @param idx A reference to store the resulting integer index.
     * @param fract A reference to store the resulting fractional part.
     * @param N An unused parameter that may define the interpolation order.
     */
    
    template <class U, class V>
    void split(U position, intptr_t& idx, V& fract, int /* N */)
    {
        idx = static_cast<intptr_t>(std::floor(position));
        fract = static_cast<V>(position - static_cast<V>(idx));
    }
    
    /**
     * @brief Returns the upper limit of the table indices.
     *
     * The `limit` method provides the maximum valid index for the table, which is one less
     * than the total size of the table. This is useful for ensuring that index access stays
     * within bounds.
     *
     * @return The maximum valid index, which is `size - 1`.
     */
    
    intptr_t limit() { return size - 1; }
    
    /**
     * @brief Prepares the table_fetcher for a specific interpolation type.
     *
     * The `prepare` method is used to set up the table_fetcher based on the provided interpolation type.
     * Although the current implementation does not perform any specific action, it can be extended
     * to handle different interpolation strategies.
     *
     * @param interpolation The interpolation type used for fetching values (currently unused).
     */
    
    void prepare(InterpType /* interpolation */) {}
    
    /**
     * @brief The size of the table.
     *
     * This constant member variable stores the total number of elements in the table.
     * It is set during the construction of the `table_fetcher` object and remains unchanged
     * throughout the object's lifetime.
     */
    
    const intptr_t size;
    
    /**
     * @brief The scale factor applied during table fetching operations.
     *
     * This constant member variable stores the scale factor used for adjusting the input values
     * or positions during table fetching operations. It is set during the construction of the
     * `table_fetcher` object and remains constant throughout the object's lifetime.
     */
    
    const double scale;
};

// Adaptors to add edge functionality

/**
 * @brief A specialized table fetcher that applies zero-padding for out-of-bounds access.
 *
 * The `table_fetcher_zeropad` template structure extends the functionality of another
 * table fetcher type `T` by handling out-of-bounds indices with zero-padding. When an
 * access occurs outside the valid range of the table, it returns a value of zero.
 *
 * @tparam T The base table fetcher type that this struct extends.
 */

template <class T>
struct table_fetcher_zeropad : T
{
    
    /**
     * @brief Constructs a `table_fetcher_zeropad` object using a base table fetcher.
     *
     * This constructor initializes the `table_fetcher_zeropad` by copying the base table fetcher `T`.
     * The base table fetcher provides the core table access functionality, while
     * the `table_fetcher_zeropad` adds zero-padding behavior for out-of-bounds access.
     *
     * @param base A constant reference to the base table fetcher object to be copied.
     */
    
    table_fetcher_zeropad(const T& base) : T(base) {}
    
    /**
     * @brief Fetches a value from the table at the specified index with zero-padding for out-of-bounds access.
     *
     * This operator allows the `table_fetcher_zeropad` object to be called like a function to fetch a value
     * from the table at the given index. If the index is within the bounds of the table, the value at that index
     * is returned. If the index is out of bounds, the method returns zero, implementing zero-padding behavior.
     *
     * @param idx The index of the value to fetch from the table.
     * @return The value at the specified index if within bounds, or zero if out of bounds.
     */
    
    typename T::fetch_type operator()(intptr_t idx)
    {
        return (idx < 0 || idx >= T::size) ? typename T::fetch_type(0) : T::operator()(idx);
    }
};

/**
 * @brief A specialized table fetcher that extends the nearest value for out-of-bounds access.
 *
 * The `table_fetcher_extend` template structure extends the functionality of another
 * table fetcher type `T` by handling out-of-bounds indices with value extension. When an
 * access occurs outside the valid range of the table, it returns the nearest valid value
 * from the table (either the first or the last element).
 *
 * @tparam T The base table fetcher type that this struct extends.
 */

template <class T>
struct table_fetcher_extend : T
{
    
    /**
     * @brief Constructs a `table_fetcher_extend` object using a base table fetcher.
     *
     * This constructor initializes the `table_fetcher_extend` by copying the base table fetcher `T`.
     * The base table fetcher provides the core table access functionality, while
     * the `table_fetcher_extend` adds behavior to extend the nearest valid value for out-of-bounds access.
     *
     * @param base A constant reference to the base table fetcher object to be copied.
     */
    
    table_fetcher_extend(const T& base) : T(base) {}
    
    /**
     * @brief Fetches a value from the table at the specified index with value extension for out-of-bounds access.
     *
     * This operator allows the `table_fetcher_extend` object to be called like a function to fetch a value
     * from the table at the given index. If the index is within bounds, the value at that index is returned.
     * If the index is out of bounds, the method returns the nearest valid value from the table
     * (either the first element if the index is below zero, or the last element if the index exceeds the table size).
     *
     * @param idx The index of the value to fetch from the table.
     * @return The value at the specified index if within bounds, or the nearest valid value if out of bounds.
     */
    
    typename T::fetch_type operator()(intptr_t idx)
    {
        return T::operator()(std::min(std::max(idx, static_cast<intptr_t>(0)), T::size - 1));
    }
};

/**
 * @brief A specialized table fetcher that wraps around the table for out-of-bounds access.
 *
 * The `table_fetcher_wrap` template structure extends the functionality of another table fetcher
 * type `T` by handling out-of-bounds indices with wrapping. When an index is outside the valid
 * range of the table, the method wraps the index around the table, effectively treating the table
 * as circular.
 *
 * @tparam T The base table fetcher type that this struct extends.
 */

template <class T>
struct table_fetcher_wrap : T
{
    
    /**
     * @brief Constructs a `table_fetcher_wrap` object using a base table fetcher.
     *
     * This constructor initializes the `table_fetcher_wrap` by copying the base table fetcher `T`.
     * The base table fetcher provides the core table access functionality, while
     * the `table_fetcher_wrap` adds behavior to wrap the index around the table for out-of-bounds access.
     *
     * @param base A constant reference to the base table fetcher object to be copied.
     */
    
    table_fetcher_wrap(const T& base) : T(base) {}
    
    /**
     * @brief Fetches a value from the table at the specified index with wrapping for out-of-bounds access.
     *
     * This operator allows the `table_fetcher_wrap` object to be called like a function to fetch a value
     * from the table at the given index. If the index is within bounds, the value at that index is returned.
     * If the index is out of bounds, the method wraps the index around the table, treating the table as circular.
     * For negative indices, it wraps to the end of the table, and for indices greater than the size of the table,
     * it wraps back to the beginning.
     *
     * @param idx The index of the value to fetch from the table.
     * @return The value at the specified index if within bounds, or the wrapped value if out of bounds.
     */
    
    typename T::fetch_type operator()(intptr_t idx)
    {
        idx = idx < 0 ? T::size - 1 + ((idx + 1) % T::size) : idx % T::size;
        return T::operator()(idx);
    }
    
    /**
     * @brief Returns the size limit of the table for wrapping access.
     *
     * The `limit` method provides the maximum index that can be used for wrapping operations.
     * In this case, it returns the total size of the table from the base fetcher `T`. This value
     * is used to calculate the valid range for indices before wrapping them around.
     *
     * @return The size of the table, which is the limit for wrapping indices.
     */
    
    intptr_t limit() { return T::size; }
};

/**
 * @brief A specialized table fetcher that folds indices for out-of-bounds access.
 *
 * The `table_fetcher_fold` template structure extends the functionality of another table fetcher
 * type `T` by handling out-of-bounds indices with folding. When an index is outside the valid
 * range of the table, the method reflects or "folds" the index back into the table, similar to
 * mirroring. This behavior is useful for ensuring that the indices remain within the valid range
 * without wrapping completely around like a circular table.
 *
 * @tparam T The base table fetcher type that this struct extends.
 */

template <class T>
struct table_fetcher_fold : T
{
    
    /**
     * @brief Constructs a `table_fetcher_fold` object using a base table fetcher and computes the fold size.
     *
     * This constructor initializes the `table_fetcher_fold` by copying the base table fetcher `T`.
     * It also calculates the `fold_size`, which is used for folding indices when they are out of bounds.
     * If the table size is greater than 1, the `fold_size` is set to twice the size of the table minus 1,
     * effectively defining the range of valid indices for folding. If the table size is 1 or less,
     * the fold size is set to 1.
     *
     * @param base A constant reference to the base table fetcher object to be copied.
     */
    
    table_fetcher_fold(const T& base)
    : T(base), fold_size(T::size > 1 ? (T::size - 1) * 2 : 1) {}
    
    /**
     * @brief Fetches a value from the table at the specified index with folding for out-of-bounds access.
     *
     * This operator allows the `table_fetcher_fold` object to be called like a function to fetch a value
     * from the table at the given index. If the index is within bounds, the value at that index is returned.
     * If the index is out of bounds, the method "folds" the index by reflecting it back into the table's range.
     * This folding behavior mirrors the table at its boundaries, effectively flipping the index
     * to remain within valid bounds.
     *
     * @param idx The index of the value to fetch from the table.
     * @return The value at the specified index if within bounds, or the folded value if out of bounds.
     */
    
    typename T::fetch_type operator()(intptr_t idx)
    {
        idx = std::abs(idx) % fold_size;
        idx = idx > T::size - 1 ? fold_size - idx : idx;
        return T::operator()(idx);
    }
    
    /**
     * @brief The effective size used for folding out-of-bounds indices.
     *
     * The `fold_size` determines the range of indices that can be folded when accessing
     * the table. It is typically set to twice the table size minus one, allowing for reflection
     * or mirroring of indices outside the valid table range. This variable is computed at
     * construction time and is used internally during table lookup operations to manage
     * out-of-bounds index folding.
     */
    
    intptr_t fold_size;
};

/**
 * @brief A specialized table fetcher that mirrors indices for out-of-bounds access.
 *
 * The `table_fetcher_mirror` template structure extends the functionality of another
 * table fetcher type `T` by handling out-of-bounds indices with mirroring. When an index
 * falls outside the valid range of the table, the method mirrors or reflects the index back
 * into the valid range, effectively treating the table as if it repeats in reverse at the boundaries.
 * This behavior is useful for applications requiring symmetrical index behavior.
 *
 * @tparam T The base table fetcher type that this struct extends.
 */

template <class T>
struct table_fetcher_mirror : T
{
    
    /**
     * @brief Constructs a `table_fetcher_mirror` object using a base table fetcher.
     *
     * This constructor initializes the `table_fetcher_mirror` by copying the base table fetcher `T`.
     * The base table fetcher provides the core table access functionality, while the
     * `table_fetcher_mirror` adds behavior to mirror indices when accessing out-of-bounds values.
     *
     * @param base A constant reference to the base table fetcher object to be copied.
     */
    
    table_fetcher_mirror(const T& base) : T(base) {}
    
    /**
     * @brief Fetches a value from the table at the specified index with mirroring for out-of-bounds access.
     *
     * This operator allows the `table_fetcher_mirror` object to be called like a function to fetch a value
     * from the table at the given index. If the index is within bounds, the value at that index is returned.
     * If the index is out of bounds, the method mirrors or reflects the index back into the table's valid range,
     * treating the table as if it repeats in reverse at its boundaries. This allows the indices to behave symmetrically.
     *
     * @param idx The index of the value to fetch from the table.
     * @return The value at the specified index if within bounds, or the mirrored value if out of bounds.
     */
    
    typename T::fetch_type operator()(intptr_t idx)
    {
        idx = (idx < 0 ? -(idx + 1) : idx) % (T::size * 2);
        idx = idx > T::size - 1 ? ((T::size * 2) - 1) - idx : idx;
        return T::operator()(idx);
    }
};

/**
 * @brief A specialized table fetcher that extrapolates values for out-of-bounds access.
 *
 * The `table_fetcher_extrapolate` template structure extends the functionality of another
 * table fetcher type `T` by handling out-of-bounds indices with extrapolation. When an index
 * falls outside the valid range of the table, the method extrapolates the value based on
 * the trend of the data, extending the behavior of the table beyond its boundaries.
 * This is useful in scenarios where an estimation of values beyond the table's limits is required.
 *
 * @tparam T The base table fetcher type that this struct extends.
 */

template <class T>
struct table_fetcher_extrapolate : T
{
    
    /**
     * @brief Constructs a `table_fetcher_extrapolate` object using a base table fetcher.
     *
     * This constructor initializes the `table_fetcher_extrapolate` by copying the base table fetcher `T`.
     * The base table fetcher provides the core table access functionality, while the
     * `table_fetcher_extrapolate` adds behavior to extrapolate values when accessing out-of-bounds indices.
     *
     * @param base A constant reference to the base table fetcher object to be copied.
     */
    
    table_fetcher_extrapolate(const T& base) : T(base) {}
    
    /**
     * @brief Fetches a value from the table at the specified index with extrapolation for out-of-bounds access.
     *
     * This operator allows the `table_fetcher_extrapolate` object to be called like a function to fetch a value
     * from the table at the given index. If the index is within bounds, the value at that index is returned.
     * If the index is out of bounds, the method extrapolates a value based on the trend of the data
     * in the table, allowing for estimation of values beyond the table's limits.
     *
     * @param idx The index of the value to fetch from the table.
     * @return The value at the specified index if within bounds, or an extrapolated value if out of bounds.
     */
    
    typename T::fetch_type operator()(intptr_t idx)
    {
        return (idx >= 0 && idx < T::size) ? T::operator()(idx) : (idx < 0 ? ends[0] : ends[1]);
    }
    
    /**
     * @brief Splits a position into an integer index and a fractional part for interpolation.
     *
     * The `split` method decomposes a given position into two components: an integer index (`idx`)
     * and a fractional part (`fract`). This is useful for interpolation or other operations where
     * the position is not an exact integer. The method is templated to allow flexibility with
     * the types of the position and the fractional part, and the parameter `N` is typically used
     * to define the interpolation order.
     *
     * @tparam U The type of the position to be split.
     * @tparam V The type of the fractional part to be calculated.
     *
     * @param position The position value to be split into an integer index and fractional part.
     * @param idx A reference to store the resulting integer index.
     * @param fract A reference to store the resulting fractional part.
     * @param N The interpolation order or other contextual parameter for the splitting operation.
     */
    
    template <class U, class V>
    void split(U position, intptr_t& idx, V& fract, int N)
    {
        U constrained = std::max(std::min(position, U(T::size - (N ? 2 : 1))), U(0));
        idx = static_cast<intptr_t>(std::floor(constrained));
        fract = static_cast<V>(position - static_cast<V>(idx));
    }
    
    /**
     * @brief Prepares the table fetcher for a specific interpolation type.
     *
     * The `prepare` method configures the table fetcher based on the provided interpolation type.
     * This setup is necessary to optimize or adjust the fetching process for the selected interpolation
     * strategy, ensuring that the correct method is used when fetching values from the table.
     *
     * @param interpolation The interpolation type that will be used in subsequent table operations.
     */
    
    void prepare(InterpType interpolation)
    {
        using fetch_type = typename T::fetch_type;
        
        auto beg = [&](intptr_t idx) { return T::operator()(idx); };
        auto end = [&](intptr_t idx) { return T::operator()(T::size - (idx + 1)); };
        
        if (T::size >= 4 && (interpolation != InterpType::None) && (interpolation != InterpType::Linear))
        {
            ends[0] = cubic_lagrange_interp<fetch_type>()(fetch_type(-2), beg(0), beg(1), beg(2), beg(3));
            ends[1] = cubic_lagrange_interp<fetch_type>()(fetch_type(-2), end(0), end(1), end(2), end(3));
        }
        else if (T::size >= 2)
        {
            ends[0] = linear_interp<fetch_type>()(fetch_type(-1), beg(0), beg(1));
            ends[1] = linear_interp<fetch_type>()(fetch_type(-1), end(0), end(1));
        }
        else
            ends[0] = ends[1] = (T::size > 0 ? T::operator()(0) : fetch_type(0));
    }
    
    /**
     * @brief Stores the boundary values of the table for interpolation or extrapolation.
     *
     * The `ends` array contains two elements of type `T::fetch_type`, which represent the values
     * at the boundaries of the table. These values can be used during interpolation or
     * extrapolation to handle cases where data outside the bounds of the table needs to be
     * accessed or estimated.
     *
     * @var ends[0] The value at the lower boundary of the table.
     * @var ends[1] The value at the upper boundary of the table.
     */
    
    typename T::fetch_type ends[2];
};

// Adaptor to take a fetcher and bound the input (can be added to the above edge behaviours)

/**
 * @brief A specialized table fetcher that applies boundary conditions for out-of-bounds access.
 *
 * The `table_fetcher_bound` template structure extends the functionality of another
 * table fetcher type `T` by adding boundary condition handling. This structure ensures
 * that when indices fall outside the valid range of the table, the boundary values are used
 * instead, providing a well-defined behavior for out-of-bounds access.
 *
 * @tparam T The base table fetcher type that this struct extends.
 */

template <class T>
struct table_fetcher_bound : T
{
    
    /**
     * @brief Constructs a `table_fetcher_bound` object using a base table fetcher.
     *
     * This constructor initializes the `table_fetcher_bound` by copying the base table fetcher `T`.
     * The base table fetcher provides the core table access functionality, while the
     * `table_fetcher_bound` adds boundary condition handling for out-of-bounds access.
     *
     * @param base A constant reference to the base table fetcher object to be copied.
     */
    
    table_fetcher_bound(const T& base) : T(base) {}

    /**
     * @brief Splits a position into an integer index and a fractional part for interpolation.
     *
     * The `split` method decomposes a given position into two components: an integer index (`idx`)
     * and a fractional part (`fract`). This is useful for interpolation or other operations where
     * the position is not an exact integer. The method is templated to allow flexibility with
     * the types of the position and the fractional part, and the parameter `N` is typically used
     * to define the interpolation order.
     *
     * @tparam U The type of the position to be split.
     * @tparam V The type of the fractional part to be calculated.
     *
     * @param position The position value to be split into an integer index and fractional part.
     * @param idx A reference to store the resulting integer index.
     * @param fract A reference to store the resulting fractional part.
     * @param N The interpolation order or other contextual parameter for the splitting operation.
     */
    
    template <class U, class V>
    void split(U position, intptr_t& idx, V& fract, int N)
    {
        position = std::max(std::min(position, U(T::limit())), U(0));
        T::split(position, idx, fract, N);
    }
};

// Generic interpolation readers

/**
 * @brief A template structure for reading and interpolating values from a table using a two-stage interpolation.
 *
 * The `interp_2_reader` structure is designed to read values from a table and apply a two-stage interpolation
 * process. It is flexible and allows customization through template parameters, which define the types used for
 * the table, the interpolation process, and other operations.
 *
 * @tparam T The type for the first stage of interpolation.
 * @tparam U The type for the second stage of interpolation.
 * @tparam V The type for the output or intermediate value.
 * @tparam Table The type of the table from which values are read.
 * @tparam Interp The type of the interpolation function or object used.
 */

template <class T, class U, class V, class Table, typename Interp>
struct interp_2_reader
{
    
    /**
     * @brief Constructs an `interp_2_reader` object with a table fetcher.
     *
     * This constructor initializes the `interp_2_reader` object using the provided table fetcher.
     * The fetcher is responsible for retrieving values from the table, which will be used in the
     * interpolation process.
     *
     * @param fetcher The table fetcher object used to retrieve values from the table.
     */
    
    interp_2_reader(Table fetcher) : fetch(fetcher) {}
    
    /**
     * @brief Performs a two-stage interpolation at the specified position in the table.
     *
     * This operator allows the `interp_2_reader` object to be called like a function,
     * performing interpolation based on the provided positions. The interpolation is applied
     * using the table fetcher and the specified positions, which are passed as a pointer.
     * The method returns the interpolated value after performing a two-stage process.
     *
     * @param positions A pointer to the position(s) in the table where interpolation is to be performed.
     * @return The interpolated value of type `T`.
     */
    
    T operator()(const V*& positions)
    {
        typename T::scalar_type fract_array[T::size];
        typename U::scalar_type array[T::size * 2];
        
        for (int i = 0; i < T::size; i++)
        {
            intptr_t idx;

            fetch.split(*positions++, idx, fract_array[i], 2);
            
            array[i]            = fetch(idx + 0);
            array[i + T::size]  = fetch(idx + 1);
        }
        
        const T y0 = U(array);
        const T y1 = U(array + T::size);
        
        return interpolate(T(fract_array), y0, y1);
    }
    
    /**
     * @brief The table fetcher used to retrieve values from the table.
     *
     * This member variable holds an instance of the `Table` type, which is responsible for
     * fetching values from the table during interpolation. It is used internally by the
     * `interp_2_reader` to access the table data as part of the interpolation process.
     */
    
    Table fetch;
    
    /**
     * @brief The interpolation function or object used to perform the interpolation.
     *
     * This member variable holds an instance of the `Interp` type, which defines the interpolation
     * method to be applied during the two-stage interpolation process. It is used by the
     * `interp_2_reader` to compute interpolated values based on the positions provided.
     */
    
    Interp interpolate;
};

/**
 * @brief A template structure for reading and interpolating values from a table using a four-stage interpolation.
 *
 * The `interp_4_reader` structure is designed to read values from a table and apply a four-stage interpolation process.
 * This structure is flexible and allows customization through template parameters, which define the types used for
 * the table, the interpolation process, and the output.
 *
 * @tparam T The type for the first stage of interpolation.
 * @tparam U The type for the second stage of interpolation.
 * @tparam V The type for the third and fourth stage, or the output type.
 * @tparam Table The type of the table from which values are read.
 * @tparam Interp The type of the interpolation function or object used in the process.
 */

template <class T, class U, class V, class Table, typename Interp>
struct interp_4_reader
{
    
    /**
     * @brief Constructs an `interp_4_reader` object with a table fetcher.
     *
     * This constructor initializes the `interp_4_reader` object using the provided table fetcher.
     * The `fetcher` is responsible for retrieving values from the table, which will be used
     * in the four-stage interpolation process.
     *
     * @param fetcher The table fetcher object used to retrieve values from the table.
     */
    
    interp_4_reader(Table fetcher) : fetch(fetcher) {}
    
    /**
     * @brief Performs a four-stage interpolation at the specified position in the table.
     *
     * This operator allows the `interp_4_reader` object to be called like a function,
     * performing interpolation based on the provided positions. The interpolation is applied
     * using the table fetcher and the specified positions, which are passed as a pointer.
     * The method returns the interpolated value after performing a four-stage process,
     * allowing for complex and precise interpolation results.
     *
     * @param positions A pointer to the position(s) in the table where interpolation is to be performed.
     * @return The interpolated value of type `T`.
     */
    
    T operator()(const V*& positions)
    {        
        typename T::scalar_type fract_array[T::size];
        typename U::scalar_type array[T::size * 4];
        
        for (int i = 0; i < T::size; i++)
        {
            intptr_t idx;
            
            fetch.split(*positions++, idx, fract_array[i], 4);
            
            array[i]                = fetch(idx - 1);
            array[i + T::size]      = fetch(idx + 0);
            array[i + T::size * 2]  = fetch(idx + 1);
            array[i + T::size * 3]  = fetch(idx + 2);
        }
        
        const T y0 = U(array);
        const T y1 = U(array + T::size);
        const T y2 = U(array + (T::size * 2));
        const T y3 = U(array + (T::size * 3));
        
        return interpolate(T(fract_array), y0, y1, y2, y3);
    }
    
    /**
     * @brief The table fetcher used to retrieve values from the table during interpolation.
     *
     * This member variable holds an instance of the `Table` type, which is responsible for
     * fetching values from the table that will be used in the interpolation process. It is
     * accessed internally by the `interp_4_reader` to obtain data for the four-stage interpolation.
     */
    
    Table fetch;
    
    /**
     * @brief The interpolation function or object used to perform the four-stage interpolation.
     *
     * This member variable holds an instance of the `Interp` type, which defines the interpolation
     * method used by the `interp_4_reader` to calculate the interpolated values. It is used internally
     * to apply the four-stage interpolation process on the data fetched from the table.
     */
    
    Interp interpolate;
};

// Readers with specific interpolation types

/**
 * @brief A template structure for reading values from a table without interpolation.
 *
 * The `no_interp_reader` structure is designed to fetch values from a table directly,
 * without applying any interpolation. This structure is useful in scenarios where
 * precise, unmodified data retrieval is required, or when interpolation is unnecessary.
 *
 * @tparam T The type for the values being fetched from the table.
 * @tparam U The type used for indexing or positioning.
 * @tparam V The type for the output or intermediate values.
 * @tparam Table The type of the table from which values are read.
 */

template <class T, class U, class V, class Table>
struct no_interp_reader
{
    
    /**
     * @brief Constructs a `no_interp_reader` object with a table fetcher.
     *
     * This constructor initializes the `no_interp_reader` object using the provided table fetcher.
     * The `fetcher` is responsible for retrieving values directly from the table without applying any interpolation.
     *
     * @param fetcher The table fetcher object used to retrieve values from the table.
     */
    
    no_interp_reader(Table fetcher) : fetch(fetcher) {}
    
    /**
     * @brief Retrieves a value from the table at the specified position without interpolation.
     *
     * This operator allows the `no_interp_reader` object to be called like a function to fetch
     * a value from the table at the given position. Since no interpolation is performed,
     * the value at the exact position is returned. The position is passed as a pointer, and
     * the method directly fetches the corresponding value from the table.
     *
     * @param positions A pointer to the position(s) in the table where the value is to be fetched.
     * @return The value at the specified position of type `T`.
     */
    
    T operator()(const V*& positions)
    {
        typename U::scalar_type array[T::size];
        
        for (int i = 0; i < T::size; i++)
        {
            typename T::scalar_type fract;
            intptr_t idx;

            fetch.split(*positions++, idx, fract, 0);
            array[i] = fetch(idx);
        }
        
        return U(array);
    }
    
    /**
     * @brief The table fetcher used to retrieve values from the table.
     *
     * This member variable holds an instance of the `Table` type, which is responsible for
     * directly fetching values from the table. In the context of `no_interp_reader`, this
     * fetcher retrieves the exact values at the specified positions without any interpolation.
     */
    
    Table fetch;
};

/**
 * @brief A template structure for reading values from a table using linear interpolation.
 *
 * The `linear_reader` structure extends the functionality of the `interp_2_reader` to perform
 * linear interpolation when fetching values from the table. It leverages a two-stage interpolation process
 * and uses a `linear_interp` object to calculate the interpolated values at the specified positions.
 *
 * @tparam T The type for the first stage of interpolation and output values.
 * @tparam U The type used for intermediate calculations or indexing.
 * @tparam V The type used for the position or fractional part during interpolation.
 * @tparam Table The type of the table from which values are read.
 */

template <class T, class U, class V, class Table>
struct linear_reader : public interp_2_reader<T, U, V, Table, linear_interp<T>>
{
    
    /**
     * @brief Constructs a `linear_reader` object with a table fetcher for linear interpolation.
     *
     * This constructor initializes the `linear_reader` by invoking the base class constructor
     * `interp_2_reader` with the provided table fetcher. The fetcher is responsible for retrieving
     * values from the table, and the linear interpolation process is applied to the fetched data
     * when reading from the table.
     *
     * @param fetcher The table fetcher object used to retrieve values from the table for interpolation.
     */
    
    linear_reader(Table fetcher) : interp_2_reader<T, U, V, Table, linear_interp<T>>(fetcher) {}
};

/**
 * @brief A template structure for reading values from a table using cubic B-spline interpolation.
 *
 * The `cubic_bspline_reader` structure extends the functionality of the `interp_4_reader`
 * to perform cubic B-spline interpolation when fetching values from the table. It uses
 * a four-stage interpolation process and leverages a `cubic_bspline_interp` object to calculate
 * smooth, continuous values based on the provided table data.
 *
 * @tparam T The type used for the values and output in the interpolation process.
 * @tparam U The type used for intermediate calculations or indexing.
 * @tparam V The type used for the position or fractional part during interpolation.
 * @tparam Table The type of the table from which values are read.
 */

template <class T, class U, class V,  class Table>
struct cubic_bspline_reader : public interp_4_reader<T, U, V, Table, cubic_bspline_interp<T>>
{
    
    /**
     * @brief Constructs a `cubic_bspline_reader` object with a table fetcher for cubic B-spline interpolation.
     *
     * This constructor initializes the `cubic_bspline_reader` by invoking the base class constructor
     * `interp_4_reader` with the provided table fetcher. The fetcher is responsible for retrieving
     * values from the table, and the cubic B-spline interpolation is applied to generate smooth
     * interpolated values based on the table data.
     *
     * @param fetcher The table fetcher object used to retrieve values from the table for interpolation.
     */
    
    cubic_bspline_reader(Table fetcher) : interp_4_reader<T, U, V, Table, cubic_bspline_interp<T>>(fetcher) {}
};

/**
 * @brief A template structure for reading values from a table using cubic Hermite spline interpolation.
 *
 * The `cubic_hermite_reader` structure extends the functionality of the `interp_4_reader`
 * to perform cubic Hermite spline interpolation when fetching values from the table.
 * It uses a four-stage interpolation process and a `cubic_hermite_interp` object to calculate
 * smooth interpolated values, ensuring both continuity and accurate derivative control.
 *
 * @tparam T The type used for the values and output in the interpolation process.
 * @tparam U The type used for intermediate calculations or indexing.
 * @tparam V The type used for the position or fractional part during interpolation.
 * @tparam Table The type of the table from which values are read.
 */

template <class T, class U, class V,  class Table>
struct cubic_hermite_reader : public interp_4_reader<T, U, V, Table, cubic_hermite_interp<T>>
{
    
    /**
     * @brief Constructs a `cubic_hermite_reader` object with a table fetcher for cubic Hermite spline interpolation.
     *
     * This constructor initializes the `cubic_hermite_reader` by invoking the base class constructor
     * `interp_4_reader` with the provided table fetcher. The fetcher retrieves values from the table,
     * and the cubic Hermite spline interpolation is applied to compute smooth interpolated values
     * with controlled derivative continuity.
     *
     * @param fetcher The table fetcher object used to retrieve values from the table for interpolation.
     */
    
    cubic_hermite_reader(Table fetcher) : interp_4_reader<T, U, V, Table, cubic_hermite_interp<T>>(fetcher) {}
};

/**
 * @brief A template structure for reading values from a table using cubic Lagrange interpolation.
 *
 * The `cubic_lagrange_reader` structure extends the functionality of the `interp_4_reader`
 * to perform cubic Lagrange interpolation when fetching values from the table.
 * It uses a four-stage interpolation process along with a `cubic_lagrange_interp` object
 * to compute smooth interpolated values based on the table data, using the principles
 * of Lagrange polynomial interpolation.
 *
 * @tparam T The type used for the values and output in the interpolation process.
 * @tparam U The type used for intermediate calculations or indexing.
 * @tparam V The type used for the position or fractional part during interpolation.
 * @tparam Table The type of the table from which values are read.
 */

template <class T, class U, class V, class Table>
struct cubic_lagrange_reader : public interp_4_reader<T, U, V, Table, cubic_lagrange_interp<T>>
{
    
    /**
     * @brief Constructs a `cubic_lagrange_reader` object with a table fetcher for cubic Lagrange interpolation.
     *
     * This constructor initializes the `cubic_lagrange_reader` by invoking the base class constructor
     * `interp_4_reader` with the provided table fetcher. The fetcher retrieves values from the table,
     * and the cubic Lagrange interpolation is applied to compute smooth interpolated values
     * using Lagrange polynomial interpolation.
     *
     * @param fetcher The table fetcher object used to retrieve values from the table for interpolation.
     */
    
    cubic_lagrange_reader(Table fetcher) : interp_4_reader<T, U, V, Table, cubic_lagrange_interp<T>>(fetcher) {}
};

// Reading loop

/**
 * @brief Reads values from the table in a loop using a custom reader, applies scaling, and stores the results.
 *
 * The `table_read_loop` function retrieves values from the table based on the provided `positions` array
 * using the specified `fetcher` and a custom reader template (`Reader`). The function iterates through
 * the positions, fetches the corresponding values, applies the scaling factor `mul`, and stores the results
 * in the output buffer `out`. This function is designed to handle multiple samples (`n_samps`) efficiently.
 *
 * @tparam T The type used for the output values and scaling factor.
 * @tparam U The type used for intermediate calculations or indexing (passed to the reader).
 * @tparam V The type used for the positions or fractional parts during table reading.
 * @tparam Table The type of the table fetcher used to retrieve values.
 * @tparam Reader A template class specifying the reader method, which includes the interpolation or table processing algorithm.
 *
 * @param fetcher The table fetcher used to retrieve values from the table.
 * @param out A pointer to the output buffer where the fetched and scaled values will be stored.
 * @param positions A pointer to an array of positions from which values are to be fetched.
 * @param n_samps The number of samples to process in the loop.
 * @param mul A scaling factor applied to each fetched value before storing it in the output buffer.
 */

template <class T, class U, class V, class Table, template <class W, class X, class Y, class Tb> class Reader>
void table_read_loop(Table fetcher, typename T::scalar_type *out, const V *positions, intptr_t n_samps, double mul)
{
    Reader<T, U, V, Table> reader(fetcher);
    
    T *v_out = reinterpret_cast<T *>(out);
    T scale = static_cast<typename U::scalar_type>(mul * reader.fetch.scale);
    
    for (intptr_t i = 0; i < (n_samps / T::size); i++)
        *v_out++ = scale * reader(positions);
}

// Template to determine vector/scalar types

/**
 * @brief Reads values from the table using a custom reader, applies scaling, and stores the results.
 *
 * The `table_read` function retrieves values from the table using a custom `Reader` template for
 * processing or interpolation. It fetches values from the `positions` array using the provided `fetcher`,
 * applies the scaling factor `mul`, and stores the resulting values in the output buffer `out`.
 * This function processes multiple samples (`n_samps`) in a loop for efficient data retrieval and scaling.
 *
 * @tparam Reader A template class specifying the reader method, which includes the interpolation or table processing algorithm.
 * @tparam Table The type of the table fetcher used to retrieve values.
 * @tparam W The type used for the output values.
 * @tparam X The type used for the positions or fractional parts during table reading.
 *
 * @param fetcher The table fetcher used to retrieve values from the table.
 * @param out A pointer to the output buffer where the fetched and scaled values will be stored.
 * @param positions A pointer to an array of positions from which values are to be fetched.
 * @param n_samps The number of samples to process in the loop.
 * @param mul A scaling factor applied to each fetched value before storing it in the output buffer.
 */

template <template <class T, class U, class V, class Tb> class Reader, class Table, class W, class X>
void table_read(Table fetcher, W *out, const X *positions, intptr_t n_samps, double mul)
{
    typedef typename Table::fetch_type fetch_type;
    const int vec_size = SIMDLimits<W>::max_size;
    intptr_t n_vsample = (n_samps / vec_size) * vec_size;
    
    table_read_loop<SIMDType<W, vec_size>, SIMDType<fetch_type, vec_size>, X, Table, Reader>(fetcher, out, positions, n_vsample, mul);
    table_read_loop<SIMDType<W, 1>, SIMDType<fetch_type, 1>, X, Table, Reader>(fetcher, out + n_vsample, positions + n_vsample, n_samps - n_vsample, mul);
}

// Main reading call that switches between different types of interpolation

/**
 * @brief Reads values from the table, applies scaling, and stores the results in the output buffer with interpolation.
 *
 * The `table_read` function retrieves values from the table based on the provided `positions` array, using the `fetcher`
 * to access the data. It applies the specified `interp` interpolation type and multiplies each fetched value
 * by the scaling factor `mul` before storing the result in the output buffer `out`. The function processes
 * multiple samples specified by `n_samps`, allowing for flexible and efficient table reading with interpolation.
 *
 * @tparam T The type used for the output values and scaling factor.
 * @tparam U The type used for the position or index values.
 * @tparam Table The type of the table fetcher used to retrieve values.
 *
 * @param fetcher The table fetcher used to retrieve values from the table.
 * @param out A pointer to the output buffer where the fetched and scaled values will be stored.
 * @param positions A pointer to an array of positions from which values are to be fetched.
 * @param n_samps The number of samples to process in the loop.
 * @param mul A scaling factor applied to each fetched value before storing it in the output buffer.
 * @param interp The interpolation type used to retrieve values from the table.
 */

template <class T, class U, class Table>
void table_read(Table fetcher, T *out, const U *positions, intptr_t n_samps, T mul, InterpType interp)
{
    fetcher.prepare(interp);
    
    switch(interp)
    {
        case InterpType::None:          table_read<no_interp_reader>(fetcher, out, positions, n_samps, mul);        break;
        case InterpType::Linear:        table_read<linear_reader>(fetcher, out, positions, n_samps, mul);           break;
        case InterpType::CubicHermite:  table_read<cubic_hermite_reader>(fetcher, out,positions, n_samps, mul);     break;
        case InterpType::CubicLagrange: table_read<cubic_lagrange_reader>(fetcher, out, positions, n_samps, mul);   break;
        case InterpType::CubicBSpline:  table_read<cubic_bspline_reader>(fetcher, out, positions, n_samps, mul);    break;
    }
}

// Reading calls to add adaptors to the basic fetcher

/**
 * @brief Reads values from the table with optional boundary handling, applies scaling, and stores the results.
 *
 * The `table_read_optional_bound` function retrieves values from the table based on the provided `positions` array
 * using the `fetcher`, with the option to handle out-of-bounds indices based on the `bound` flag.
 * If `bound` is true, boundary handling is applied to ensure indices stay within the table's limits.
 * The function also applies the specified `interp` interpolation type and multiplies each fetched value
 * by the scaling factor `mul` before storing it in the output buffer `out`. It processes multiple samples
 * specified by `n_samps` in a loop.
 *
 * @tparam T The type used for the output values and scaling factor.
 * @tparam U The type used for the position or index values.
 * @tparam Table The type of the table fetcher used to retrieve values.
 *
 * @param fetcher The table fetcher used to retrieve values from the table.
 * @param out A pointer to the output buffer where the fetched and scaled values will be stored.
 * @param positions A pointer to an array of positions from which values are to be fetched.
 * @param n_samps The number of samples to process in the loop.
 * @param mul A scaling factor applied to each fetched value before storing it in the output buffer.
 * @param interp The interpolation type used to retrieve values from the table.
 * @param bound A boolean flag indicating whether boundary handling should be applied for out-of-bounds access.
 */

template <class T, class U, class Table>
void table_read_optional_bound(Table fetcher, T *out, const U *positions, intptr_t n_samps, T mul, InterpType interp, bool bound)
{
    if (bound)
    {
        table_fetcher_bound<Table> fetch(fetcher);
        table_read(fetch, out, positions, n_samps, mul, interp);
    }
    else
        table_read(fetcher, out, positions, n_samps, mul, interp);
}

/**
 * @brief Reads values from the table with zero-padding for out-of-bounds access, applies scaling, and stores the results.
 *
 * The `table_read_zeropad` function retrieves values from the table based on the provided `positions` array using the `fetcher`.
 * For out-of-bounds indices, it returns zero (zero-padding behavior). The function also applies the specified
 * `interp` interpolation type and multiplies each fetched value by the scaling factor `mul` before storing the
 * result in the output buffer `out`. The function can optionally handle boundary conditions based on the `bound`
 * flag. Multiple samples (`n_samps`) are processed in a loop.
 *
 * @tparam T The type used for the output values and scaling factor.
 * @tparam U The type used for the position or index values.
 * @tparam Table The type of the table fetcher used to retrieve values.
 *
 * @param fetcher The table fetcher used to retrieve values from the table.
 * @param out A pointer to the output buffer where the fetched and scaled values will be stored.
 * @param positions A pointer to an array of positions from which values are to be fetched.
 * @param n_samps The number of samples to process in the loop.
 * @param mul A scaling factor applied to each fetched value before storing it in the output buffer.
 * @param interp The interpolation type used to retrieve values from the table.
 * @param bound A boolean flag indicating whether boundary handling should be applied for out-of-bounds access.
 */

template <class T, class U, class Table>
void table_read_zeropad(Table fetcher, T *out, const U *positions, intptr_t n_samps, T mul, InterpType interp, bool bound)
{
    table_fetcher_zeropad<Table> fetch(fetcher);
    table_read_optional_bound(fetch, out, positions, n_samps, mul, interp, bound);
}

/**
 * @brief Reads values from the table with boundary extension for out-of-bounds access, applies scaling, and stores the results.
 *
 * The `table_read_extend` function retrieves values from the table based on the provided `positions` array using the `fetcher`.
 * For out-of-bounds indices, it extends the nearest boundary value (boundary extension behavior). The function
 * also applies the specified `interp` interpolation type and multiplies each fetched value by the scaling factor `mul`
 * before storing the result in the output buffer `out`. The function can optionally handle boundary conditions based
 * on the `bound` flag. Multiple samples (`n_samps`) are processed in a loop.
 *
 * @tparam T The type used for the output values and scaling factor.
 * @tparam U The type used for the position or index values.
 * @tparam Table The type of the table fetcher used to retrieve values.
 *
 * @param fetcher The table fetcher used to retrieve values from the table.
 * @param out A pointer to the output buffer where the fetched and scaled values will be stored.
 * @param positions A pointer to an array of positions from which values are to be fetched.
 * @param n_samps The number of samples to process in the loop.
 * @param mul A scaling factor applied to each fetched value before storing it in the output buffer.
 * @param interp The interpolation type used to retrieve values from the table.
 * @param bound A boolean flag indicating whether boundary handling should be applied for out-of-bounds access.
 */

template <class T, class U, class Table>
void table_read_extend(Table fetcher, T *out, const U *positions, intptr_t n_samps, T mul, InterpType interp, bool bound)
{
    table_fetcher_extend<Table> fetch(fetcher);
    table_read_optional_bound(fetch, out, positions, n_samps, mul, interp, bound);
}

/**
 * @brief Reads values from the table with wrapping for out-of-bounds access, applies scaling, and stores the results.
 *
 * The `table_read_wrap` function retrieves values from the table based on the provided `positions` array using the `fetcher`.
 * For out-of-bounds indices, it wraps the index around the table (circular behavior). The function also applies the specified
 * `interp` interpolation type and multiplies each fetched value by the scaling factor `mul` before storing the result
 * in the output buffer `out`. The function can optionally handle boundary conditions based on the `bound` flag.
 * Multiple samples (`n_samps`) are processed in a loop.
 *
 * @tparam T The type used for the output values and scaling factor.
 * @tparam U The type used for the position or index values.
 * @tparam Table The type of the table fetcher used to retrieve values.
 *
 * @param fetcher The table fetcher used to retrieve values from the table.
 * @param out A pointer to the output buffer where the fetched and scaled values will be stored.
 * @param positions A pointer to an array of positions from which values are to be fetched.
 * @param n_samps The number of samples to process in the loop.
 * @param mul A scaling factor applied to each fetched value before storing it in the output buffer.
 * @param interp The interpolation type used to retrieve values from the table.
 * @param bound A boolean flag indicating whether boundary handling should be applied for out-of-bounds access.
 */

template <class T, class U, class Table>
void table_read_wrap(Table fetcher, T *out, const U *positions, intptr_t n_samps, T mul, InterpType interp, bool bound)
{
    table_fetcher_wrap<Table> fetch(fetcher);
    table_read_optional_bound(fetch, out, positions, n_samps, mul, interp, bound);
}

/**
 * @brief Reads values from the table with folding for out-of-bounds access, applies scaling, and stores the results.
 *
 * The `table_read_fold` function retrieves values from the table based on the provided `positions` array using the `fetcher`.
 * For out-of-bounds indices, it "folds" the index back into the table by reflecting it, similar to mirroring.
 * The function also applies the specified `interp` interpolation type and multiplies each fetched value by
 * the scaling factor `mul` before storing the result in the output buffer `out`. The function can optionally
 * handle boundary conditions based on the `bound` flag. Multiple samples (`n_samps`) are processed in a loop.
 *
 * @tparam T The type used for the output values and scaling factor.
 * @tparam U The type used for the position or index values.
 * @tparam Table The type of the table fetcher used to retrieve values.
 *
 * @param fetcher The table fetcher used to retrieve values from the table.
 * @param out A pointer to the output buffer where the fetched and scaled values will be stored.
 * @param positions A pointer to an array of positions from which values are to be fetched.
 * @param n_samps The number of samples to process in the loop.
 * @param mul A scaling factor applied to each fetched value before storing it in the output buffer.
 * @param interp The interpolation type used to retrieve values from the table.
 * @param bound A boolean flag indicating whether boundary handling should be applied for out-of-bounds access.
 */

template <class T, class U, class Table>
void table_read_fold(Table fetcher, T *out, const U *positions, intptr_t n_samps, T mul, InterpType interp, bool bound)
{
    table_fetcher_fold<Table> fetch(fetcher);
    table_read_optional_bound(fetch, out, positions, n_samps, mul, interp, bound);
}

/**
 * @brief Reads values from the table with mirroring for out-of-bounds access, applies scaling, and stores the results.
 *
 * The `table_read_mirror` function retrieves values from the table based on the provided `positions` array using the `fetcher`.
 * For out-of-bounds indices, it mirrors the index back into the table, effectively reflecting it across the table's boundaries.
 * The function also applies the specified `interp` interpolation type and multiplies each fetched value by the scaling factor `mul`
 * before storing the result in the output buffer `out`. The function can optionally handle boundary conditions based on the `bound` flag.
 * Multiple samples (`n_samps`) are processed in a loop.
 *
 * @tparam T The type used for the output values and scaling factor.
 * @tparam U The type used for the position or index values.
 * @tparam Table The type of the table fetcher used to retrieve values.
 *
 * @param fetcher The table fetcher used to retrieve values from the table.
 * @param out A pointer to the output buffer where the fetched and scaled values will be stored.
 * @param positions A pointer to an array of positions from which values are to be fetched.
 * @param n_samps The number of samples to process in the loop.
 * @param mul A scaling factor applied to each fetched value before storing it in the output buffer.
 * @param interp The interpolation type used to retrieve values from the table.
 * @param bound A boolean flag indicating whether boundary handling should be applied for out-of-bounds access.
 */

template <class T, class U, class Table>
void table_read_mirror(Table fetcher, T *out, const U *positions, intptr_t n_samps, T mul, InterpType interp, bool bound)
{
    table_fetcher_mirror<Table> fetch(fetcher);
    table_read_optional_bound(fetch, out, positions, n_samps, mul, interp, bound);
}

/**
 * @brief Reads values from the table with extrapolation for out-of-bounds access, applies scaling, and stores the results.
 *
 * The `table_read_extrapolate` function retrieves values from the table based on the provided `positions` array using the `fetcher`.
 * For out-of-bounds indices, it extrapolates values based on the trend of the data in the table, extending beyond its boundaries.
 * The function applies the specified `interp` interpolation type and multiplies each fetched value by the scaling factor `mul`
 * before storing the result in the output buffer `out`. The function can optionally handle boundary conditions based on the `bound` flag.
 * Multiple samples (`n_samps`) are processed in a loop.
 *
 * @tparam T The type used for the output values and scaling factor.
 * @tparam U The type used for the position or index values.
 * @tparam Table The type of the table fetcher used to retrieve values.
 *
 * @param fetcher The table fetcher used to retrieve values from the table.
 * @param out A pointer to the output buffer where the fetched and scaled values will be stored.
 * @param positions A pointer to an array of positions from which values are to be fetched.
 * @param n_samps The number of samples to process in the loop.
 * @param mul A scaling factor applied to each fetched value before storing it in the output buffer.
 * @param interp The interpolation type used to retrieve values from the table.
 * @param bound A boolean flag indicating whether boundary handling should be applied for out-of-bounds access.
 */

template <class T, class U, class Table>
void table_read_extrapolate(Table fetcher, T *out, const U *positions, intptr_t n_samps, T mul, InterpType interp, bool bound)
{
    table_fetcher_extrapolate<Table> fetch(fetcher);
    table_read_optional_bound(fetch, out, positions, n_samps, mul, interp, bound);
}

// Main read call for variable edge behaviour

/**
 * @brief Reads values from the table with edge handling for out-of-bounds access, applies scaling, and stores the results.
 *
 * The `table_read_edges` function retrieves values from the table based on the provided `positions` array using the `fetcher`.
 * For out-of-bounds indices, it applies edge handling based on the specified `edges` mode, which can include zero-padding,
 * extension, wrapping, folding, mirroring, or extrapolation. The function applies the specified `interp` interpolation type
 * and multiplies each fetched value by the scaling factor `mul` before storing the result in the output buffer `out`.
 * The function can optionally handle boundary conditions based on the `bound` flag. Multiple samples (`n_samps`) are processed in a loop.
 *
 * @tparam T The type used for the output values and scaling factor.
 * @tparam U The type used for the position or index values.
 * @tparam Table The type of the table fetcher used to retrieve values.
 *
 * @param fetcher The table fetcher used to retrieve values from the table.
 * @param out A pointer to the output buffer where the fetched and scaled values will be stored.
 * @param positions A pointer to an array of positions from which values are to be fetched.
 * @param n_samps The number of samples to process in the loop.
 * @param mul A scaling factor applied to each fetched value before storing it in the output buffer.
 * @param interp The interpolation type used to retrieve values from the table.
 * @param edges The edge handling mode used for out-of-bounds access (e.g., ZeroPad, Extend, Wrap, Fold, Mirror, Extrapolate).
 * @param bound A boolean flag indicating whether boundary handling should be applied for out-of-bounds access.
 */

template <class T, class U, class Table>
void table_read_edges(Table fetcher, T *out, const U *positions, intptr_t n_samps, T mul, InterpType interp, EdgeMode edges, bool bound)
{
    switch (edges)
    {
        case EdgeMode::ZeroPad:     table_read_zeropad(fetcher, out, positions, n_samps, mul, interp, bound);       break;
        case EdgeMode::Extend:      table_read_extend(fetcher, out, positions, n_samps, mul, interp, bound);        break;
        case EdgeMode::Wrap:        table_read_wrap(fetcher, out, positions, n_samps, mul, interp, bound);          break;
        case EdgeMode::Fold:        table_read_fold(fetcher, out, positions, n_samps, mul, interp, bound);          break;
        case EdgeMode::Mirror:      table_read_mirror(fetcher, out, positions, n_samps, mul, interp, bound);        break;
        case EdgeMode::Extrapolate: table_read_extrapolate(fetcher, out, positions, n_samps, mul, interp, bound);   break;
    }
}

#endif /* TableReader_h */
