
/**
 * @file partial_tracker.hpp
 * @brief Definition of the Partial Tracker class for managing peak and track data.
 *
 * This file contains the declaration of the `partial_tracker` class, which is designed to handle
 * the management of peaks and tracks in a data processing context. The class includes functionalities
 * for tracking changes, calculating costs, and assigning peaks to tracks, utilizing customizable
 * allocators and change tracking mechanisms.
 *
 * Key features include:
 * - Dynamic allocation of peaks and tracks.
 * - Cost calculation methods with optional scaling.
 * - Change tracking to monitor variations in peak and track data.
 * - Flexible memory management through customizable allocators.
 *
 * @tparam T The data type used for managing peak and track properties, such as frequency and amplitude.
 * @tparam Changes A boolean flag that indicates whether change tracking is enabled.
 *
 * @note This class assumes that the allocator provided is compatible with the data types being managed.
 */

#ifndef PARTIAL_TRACKER_H
#define PARTIAL_TRACKER_H

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

#include "Allocator.hpp"

/**
 * @brief A template class representing a peak object.
 *
 * This class is used to represent a peak in some form of data, where the type of the peak values
 * (e.g., amplitude, frequency, etc.) is determined by the template parameter.
 *
 * @tparam T The data type used for the peak's properties, such as a floating point or integer type.
 */

template <typename T>
class peak
{
public:
    
    /**
     * @brief Constructor for the peak object.
     *
     * Initializes a peak with a specified frequency and amplitude.
     * The pitch and decibel (dB) values are initialized to infinity by default.
     *
     * @param freq The frequency of the peak.
     * @param amp The amplitude of the peak.
     */
    
    peak(T freq, T amp)
    : m_freq(freq)
    , m_amp(amp)
    , m_pitch(std::numeric_limits<T>::infinity())
    , m_db(std::numeric_limits<T>::infinity())
    {}
    
    /**
     * @brief Default constructor for the peak object.
     *
     * Initializes the peak object by delegating to the parameterized constructor
     * with default values of 0 for both arguments.
     */
    
    peak() : peak(0, 0) {}
    
    /**
     * @brief Get the frequency of the peak.
     *
     * @return A constant reference to the frequency value of the peak.
     */
    
    const T &freq() const { return m_freq; }
    
    /**
     * @brief Get the amplitude of the peak.
     *
     * @return A constant reference to the amplitude value of the peak.
     */
    
    const T &amp() const { return m_amp; }
    
    /**
     * @brief Get the pitch of the peak.
     *
     * @return A constant reference to the pitch value of the peak.
     */
    
    const T &pitch() const
    {
        if (m_pitch == std::numeric_limits<T>::infinity())
            m_pitch = std::log2(m_freq / 440.0) * 12.0 + 69.0;
        
        return m_pitch;
    }
    
    /**
     * @brief Get the decibel (dB) value of the peak.
     *
     * @return A constant reference to the decibel (dB) value of the peak.
     */
    
    const T &db() const
    {
        if (m_db == std::numeric_limits<T>::infinity())
            m_db = std::log10(m_amp) * 20.0;
        
        return m_db;
    }
    
private:
    
    /**
     * @brief The frequency of the peak.
     *
     * This member variable stores the frequency value of the peak, represented using the type `T`.
     */
    
    T m_freq;
    
    /**
     * @brief The amplitude of the peak.
     *
     * This member variable stores the amplitude value of the peak, represented using the type `T`.
     */
    
    T m_amp;
    
    /**
     * @brief The pitch of the peak, which can be modified even in const instances.
     *
     * This mutable member variable stores the pitch value of the peak, allowing it to be modified
     * even when the containing object is const. It is represented using the type `T`.
     */
    
    mutable T m_pitch;
    
    /**
     * @brief The decibel (dB) value of the peak, which can be modified even in const instances.
     *
     * This mutable member variable stores the decibel (dB) value of the peak, allowing it to be modified
     * even when the containing object is const. It is represented using the type `T`.
     */
    
    mutable T m_db;
};

/**
 * @brief Alias for a constant peak object.
 *
 * This alias defines a type `cpeak`, which is a constant `peak` object for a given template type `T`.
 * It simplifies the usage of `const peak<T>` by providing a more concise type name.
 *
 * @tparam T The data type used for the peak's properties, such as frequency and amplitude.
 */

template <typename T>
using cpeak = const peak<T>;

/**
 * @brief A template structure representing a track of peaks.
 *
 * This structure is used to represent a series or track of peaks, where the type of the values
 * (e.g., frequency, amplitude, etc.) is determined by the template parameter.
 *
 * @tparam T The data type used for the track's properties, such as a floating point or integer type.
 */

template <typename T>
struct track
{
    
    /**
     * @brief Enum class representing the different states of a track.
     *
     * This enumeration defines the various states a track can be in during its lifecycle.
     *
     * - `Off`: The track is inactive or has ended.
     * - `Start`: The track has just started.
     * - `Continue`: The track is ongoing.
     * - `Switch`: The track has switched to a new state or context.
     */
    
    enum class State { Off, Start, Continue, Switch };
    
    /**
     * @brief Default constructor for the track object.
     *
     * Initializes a track object with default values. The peak is initialized using its default constructor,
     * and the state is set to `State::Off`, indicating that the track is inactive initially.
     */
    
    track() : m_peak(), m_state(State::Off) {}
    
    /**
     * @brief Sets the current peak of the track.
     *
     * Updates the track's peak with a constant reference to a given peak and adjusts the state
     * based on whether the peak marks the start of the track.
     *
     * @param peak A constant reference to the peak that will be set for the track.
     * @param start A boolean flag indicating whether the peak marks the start of the track.
     * If true, the track's state will be set to `State::Start`; otherwise, it will be set accordingly.
     */
    
    void set_peak(cpeak<T>& peak, bool start)
    {
        m_peak = peak;
        m_state = start ? (active() ? State::Switch : State::Start): State::Continue;
    }
    
    /**
     * @brief Checks if the track is currently active.
     *
     * Determines whether the track is in an active state. A track is considered active if its state
     * is not `State::Off`.
     *
     * @return True if the track is active, false if the track is inactive (i.e., in the `State::Off`).
     */
    
    bool active() const { return m_state != State::Off; }
    
    /**
     * @brief The current peak associated with the track.
     *
     * This member variable stores the peak object for the track, represented using the template type `T`.
     * It holds information such as the frequency, amplitude, pitch, and decibel (dB) values of the peak.
     */
    
    peak<T> m_peak;
    
    /**
     * @brief The current state of the track.
     *
     * This member variable stores the state of the track, which is represented by the `State` enum class.
     * It indicates whether the track is inactive (`Off`), starting (`Start`), continuing (`Continue`), or switching (`Switch`).
     */
    
    State m_state;
};

/**
 * @brief A template class that tracks changes in values of type T.
 *
 * This class is designed to track changes in a value of type `T`. It uses the template parameter `Impl`
 * to determine whether a specific implementation or behavior should be used for change tracking.
 *
 * @tparam T The type of the value being tracked (e.g., int, float, etc.).
 * @tparam Impl A boolean template parameter that controls the behavior of the change tracking mechanism.
 * If true, a specific implementation is used; otherwise, an alternative behavior is applied.
 */

template <typename T, bool Impl>
class change_tracker
{
public:
    
    /**
     * @brief Default constructor for the change_tracker class.
     *
     * Initializes a change_tracker object with the tracking mechanism set to active by default.
     * The member variable `m_active` is initialized to `true`, indicating that change tracking is enabled.
     */
    
    change_tracker() : m_active(true)
    {
        reset();
    }
    
    /**
     * @brief Tracks changes between the current and previous peaks.
     *
     * This method compares the current peak (`now`) with the previous peak (`prev`) and records any
     * changes based on the specified parameters. The changes can include frequency, amplitude, pitch,
     * and decibel (dB) values.
     *
     * @param now A constant reference to the current peak.
     * @param prev A constant reference to the previous peak.
     * @param use_pitch A boolean flag indicating whether pitch changes should be tracked.
     * @param use_db A boolean flag indicating whether decibel (dB) changes should be tracked.
     */
    
    void add_change(cpeak<T>& now, cpeak<T>& prev, bool use_pitch, bool use_db)
    {
        T f_change;
        T a_change;
        
        if (!m_active)
            return;
        
        if (use_pitch)
            f_change = now.pitch() - prev.pitch();
        else
            f_change = now.freq() - prev.freq();
        
        if (use_db)
            a_change = now.db() - prev.db();
        else
            a_change = now.amp() - prev.amp();
        
        m_freq_sum += f_change;
        m_freq_abs += std::abs(f_change);
        m_amp_sum += a_change;
        m_amp_abs += std::abs(a_change);
        m_count++;
    }
    
    /**
     * @brief Marks the change tracking process as complete.
     *
     * This method finalizes the change tracking process, indicating that no further changes
     * will be tracked. It is typically called after all changes between peaks have been processed.
     */
    
    void complete()
    {
        if (m_count)
        {
            T recip = T(1) / static_cast<T>(m_count);
            m_freq_sum *= recip;
            m_freq_abs *= recip;
            m_amp_sum *= recip;
            m_amp_abs *= recip;
        }
    }
    
    /**
     * @brief Resets the change tracker to its initial state.
     *
     * This method clears any tracked changes and resets the change tracker, allowing it to start
     * tracking changes from scratch. It is typically used to reinitialize the tracker for a new set of comparisons.
     */
    
    void reset()
    {
        m_freq_sum = 0;
        m_freq_abs = 0;
        m_amp_sum = 0;
        m_amp_abs = 0;
        m_count = 0;
    }
    
    /**
     * @brief Retrieves the sum of frequency changes.
     *
     * This method returns the accumulated sum of frequency changes tracked by the change tracker.
     *
     * @return The sum of the frequency changes as a value of type `T`.
     */
    
    T freq_sum() const { return m_freq_sum; }
    
    /**
     * @brief Retrieves the absolute sum of frequency changes.
     *
     * This method returns the accumulated absolute sum of frequency changes tracked by the change tracker.
     * It provides the total of all frequency changes, regardless of direction.
     *
     * @return The absolute sum of frequency changes as a value of type `T`.
     */
    
    T freq_abs() const { return m_freq_abs; }
    
    /**
     * @brief Retrieves the sum of amplitude changes.
     *
     * This method returns the accumulated sum of amplitude changes tracked by the change tracker.
     *
     * @return The sum of the amplitude changes as a value of type `T`.
     */
    
    T amp_sum() const { return m_amp_sum; }
    
    /**
     * @brief Retrieves the absolute sum of amplitude changes.
     *
     * This method returns the accumulated absolute sum of amplitude changes tracked by the change tracker.
     * It provides the total of all amplitude changes, regardless of direction.
     *
     * @return The absolute sum of amplitude changes as a value of type `T`.
     */
    
    T amp_abs() const { return m_amp_abs; }
    
    /**
     * @brief Sets the active state of the change tracker.
     *
     * This method enables or disables the change tracking mechanism. When `on` is true,
     * the tracker is active and will track changes; when false, tracking is disabled.
     *
     * @param on A boolean flag to set the active state.
     * If true, the tracker is activated; if false, it is deactivated.
     */
    
    void active(bool on) { m_active = on; }
    
private:
    
    /**
     * @brief The sum of frequency changes.
     *
     * This member variable stores the cumulative sum of the frequency changes that have been tracked.
     * It is represented using the template type `T`.
     */
    
    T m_freq_sum;
    
    /**
     * @brief The absolute sum of frequency changes.
     *
     * This member variable stores the cumulative absolute sum of the frequency changes that have been tracked.
     * It is represented using the template type `T`. The absolute sum tracks the magnitude of frequency changes
     * regardless of direction (positive or negative).
     */
    
    T m_freq_abs;
    
    /**
     * @brief The sum of amplitude changes.
     *
     * This member variable stores the cumulative sum of the amplitude changes that have been tracked.
     * It is represented using the template type `T`.
     */
    
    T m_amp_sum;
    
    /**
     * @brief The absolute sum of amplitude changes.
     *
     * This member variable stores the cumulative absolute sum of the amplitude changes that have been tracked.
     * It is represented using the template type `T`. The absolute sum tracks the magnitude of amplitude changes
     * regardless of direction (positive or negative).
     */
    
    T m_amp_abs;
    
    /**
     * @brief Indicates whether the change tracker is active.
     *
     * This boolean member variable stores the state of the change tracker.
     * When `true`, the tracker is active and changes are being tracked; when `false`, tracking is disabled.
     */
    
    bool m_active;
    
    /**
     * @brief The count of changes tracked.
     *
     * This member variable stores the number of changes that have been tracked by the change tracker.
     * It is represented using `size_t` to hold a non-negative integer value.
     */
    
    size_t m_count;
};

/**
 * @brief Specialization of the change_tracker class for when the template parameter `Impl` is false.
 *
 * This specialized version of the `change_tracker` class provides an alternative implementation
 * or behavior for tracking changes when the boolean template parameter `Impl` is set to `false`.
 * It modifies or disables certain functionality depending on the design of the class.
 *
 * @tparam T The type of the value being tracked (e.g., int, float, etc.).
 */

template <typename T>
class change_tracker<T, false>
{
public:
    
    /**
     * @brief Tracks changes between the current and previous peaks.
     *
     * This is a specialized version of the `add_change` method for the case when the `Impl` parameter is false.
     * In this version, the method does not perform any actual change tracking, effectively providing a no-op implementation.
     *
     * @param now A constant reference to the current peak.
     * @param prev A constant reference to the previous peak.
     * @param use_pitch A boolean flag indicating whether pitch changes should be tracked. (Not used in this version)
     * @param use_db A boolean flag indicating whether decibel (dB) changes should be tracked. (Not used in this version)
     */
    
    void add_change(cpeak<T>& now, cpeak<T>& prev, bool use_pitch, bool use_db) {}
   
    /**
     * @brief Marks the change tracking process as complete.
     *
     * In this specialized version of the `complete` method, where the `Impl` parameter is `false`,
     * the method does not perform any actions, effectively acting as a no-op.
     * This is used when no change tracking is required.
     */
    
    void complete() {}
    
    /**
     * @brief Resets the change tracker to its initial state.
     *
     * In this specialized version of the `reset` method, where the `Impl` parameter is `false`,
     * the method does not perform any reset actions, functioning as a no-op.
     * This is used when no change tracking behavior is required.
     */
    
    void reset() {}
};

/**
 * @brief A template class for tracking partials with optional change tracking.
 *
 * The `partial_tracker` class is designed to track partials (e.g., components in a signal), where
 * the data type `T` is used for representing values. The template parameter `Changes` controls whether
 * change tracking is enabled, and `Allocator` defines the memory allocation strategy for the tracker.
 *
 * @tparam T The data type used for the partials (e.g., frequency, amplitude).
 * @tparam Changes A boolean flag that enables or disables change tracking. Default is `false` (no change tracking).
 * @tparam Allocator The allocator type used for managing memory. Default is `malloc_allocator`.
 */

template <typename T, bool Changes = false, typename Allocator = malloc_allocator>
class partial_tracker
{
    
    /**
     * @brief Typedef for a function pointer representing a cost function.
     *
     * This typedef defines `CostType` as a pointer to a function that takes three arguments of type `T`
     * and returns a value of type `T`. It is used to represent a cost function that computes a value based
     * on three input parameters.
     *
     * @typedef CostType
     * @param T The data type for the function arguments and the return value.
     */
    
    typedef T (*CostType)(T, T, T);
    
    /**
     * @brief Typedef for a pointer to a member function in the peak class.
     *
     * This typedef defines `GetMethod` as a pointer to a constant member function of the `peak` class
     * that returns a constant reference to a value of type `T`. It is used to refer to getter methods
     * in the `peak` class.
     *
     * @typedef GetMethod
     * @tparam T The return type of the member function, which is a constant reference to a value of type `T`.
     */
    
    typedef const T&(peak<T>::*GetMethod)() const;
    
    /**
     * @brief Alias for a tuple representing the cost and related indices.
     *
     * This alias defines `cost` as a tuple consisting of three elements:
     * - The first element is of type `T` representing the cost value.
     * - The second element is of type `size_t`, typically representing an index (e.g., start index).
     * - The third element is of type `size_t`, typically representing another index (e.g., end index).
     *
     * This alias simplifies the usage of tuples for representing cost-related data in the class.
     */
    
    using cost = std::tuple<T, size_t, size_t>;
    
    /**
     * @brief Template alias for enabling conditional compilation based on a boolean expression.
     *
     * This alias defines `enable_if_t` as a shorthand for `std::enable_if`, which is used to enable or disable
     * function templates or class specializations based on the compile-time boolean condition `B`. If `B` is true,
     * the alias resolves to `int`, allowing the template to be instantiated; otherwise, the template is disabled.
     *
     * @tparam B A compile-time boolean condition used to control whether the template is enabled.
     */
    
    template <bool B>
    using enable_if_t = typename std::enable_if<B, int>::type;
    
public:
    
    /**
     * @brief Constructor for the partial_tracker class with a conditionally enabled allocator.
     *
     * This constructor initializes a `partial_tracker` object with the specified number of tracks and peaks.
     * The constructor is only enabled if the allocator type `U` is default-constructible, which is checked
     * using `std::enable_if` and `std::is_default_constructible`.
     *
     * @tparam U The allocator type, defaulting to `Allocator`. The constructor is only enabled if `U` is default-constructible.
     * @param n_tracks The number of tracks to be managed by the tracker.
     * @param n_peaks The number of peaks to be managed by the tracker.
     */
    
    template <typename U = Allocator, enable_if_t<std::is_default_constructible<U>::value> = 0>
    partial_tracker(size_t n_tracks, size_t n_peaks)
    {
        init(n_tracks, n_peaks);
    }
    
    /**
     * @brief Constructor for the partial_tracker class with a custom allocator.
     *
     * This constructor initializes a `partial_tracker` object with a user-provided allocator, the specified
     * number of tracks, and the specified number of peaks. The constructor is only enabled if the allocator
     * type `U` is copy-constructible, as determined by `std::enable_if` and `std::is_copy_constructible`.
     *
     * @tparam U The allocator type, defaulting to `Allocator`. The constructor is only enabled if `U` is copy-constructible.
     * @param allocator A constant reference to the allocator used for memory management.
     * @param n_tracks The number of tracks to be managed by the tracker.
     * @param n_peaks The number of peaks to be managed by the tracker.
     */
    
    template <typename U = Allocator, enable_if_t<std::is_copy_constructible<U>::value> = 0>
    partial_tracker(const Allocator& allocator, size_t n_tracks, size_t n_peaks)
    : m_allocator(allocator)
    {
        init(n_tracks, n_peaks);
    }
    
    /**
     * @brief Constructor for the partial_tracker class with a move-constructed allocator.
     *
     * This constructor initializes a `partial_tracker` object with an allocator that is moved into the class,
     * along with the specified number of tracks and peaks. The constructor is only enabled if the allocator
     * type `U` is default-constructible, checked using `std::enable_if` and `std::is_default_constructible`.
     *
     * @tparam U The allocator type, defaulting to `Allocator`. The constructor is only enabled if `U` is default-constructible.
     * @param allocator An rvalue reference to the allocator, which is moved into the `partial_tracker` object.
     * @param n_tracks The number of tracks to be managed by the tracker.
     * @param n_peaks The number of peaks to be managed by the tracker.
     */
    
    template <typename U = Allocator, enable_if_t<std::is_default_constructible<U>::value> = 0>
    partial_tracker(Allocator&& allocator, size_t n_tracks, size_t n_peaks)
    : m_allocator(allocator)
    {
        init(n_tracks, n_peaks);
    }
    
    /**
     * @brief Destructor for the partial_tracker class.
     *
     * Cleans up any resources used by the `partial_tracker` object. The destructor ensures that memory
     * allocated by the allocator and other resources related to the tracks and peaks are properly released.
     */
    
    ~partial_tracker()
    {
        m_allocator.deallocate(m_tracks);
        m_allocator.deallocate(m_costs);
        m_allocator.deallocate(m_peak_assigned);
        m_allocator.deallocate(m_track_assigned);
    }
    
    /**
     * @brief Deleted copy constructor for the partial_tracker class.
     *
     * This copy constructor is explicitly deleted to prevent copying of `partial_tracker` objects.
     * Copying is disabled to avoid potential issues with resource management, such as memory allocation
     * and state management, which could lead to unintended behavior or resource leaks.
     */
    
    partial_tracker(const partial_tracker&) = delete;
    
    /**
     * @brief Deleted copy assignment operator for the partial_tracker class.
     *
     * This copy assignment operator is explicitly deleted to prevent assignment of one `partial_tracker`
     * object to another. Disabling assignment helps avoid potential issues with resource management,
     * such as memory allocation and state consistency, ensuring the object is not inadvertently duplicated.
     */
    
    partial_tracker & operator=(const partial_tracker&) = delete;
    
    /**
     * @brief Configures the cost calculation method for the tracker.
     *
     * This method sets the parameters that determine how costs are calculated within the `partial_tracker`.
     * It allows the user to specify whether to square the cost values and whether to include pitch and
     * decibel (dB) values in the cost calculation.
     *
     * @param square_cost A boolean flag indicating whether the cost values should be squared.
     * @param use_pitch A boolean flag indicating whether to include pitch in the cost calculation.
     * @param use_db A boolean flag indicating whether to include decibel (dB) values in the cost calculation.
     */
    
    void set_cost_calculation(bool square_cost, bool use_pitch, bool use_db)
    {
        m_square_cost = square_cost;
        m_use_pitch = use_pitch;
        m_use_db = use_db;
    }
    
    /**
     * @brief Sets the scaling factors for cost calculation.
     *
     * This method configures the scaling factors used in cost calculations within the `partial_tracker`.
     * It allows the user to define units for frequency and amplitude, as well as the maximum cost value,
     * which can help normalize the cost metrics.
     *
     * @param freq_unit The scaling factor for frequency values used in cost calculations.
     * @param amp_unit The scaling factor for amplitude values used in cost calculations.
     * @param max_cost The maximum allowable cost value for normalization purposes.
     */
    
    void set_cost_scaling(T freq_unit, T amp_unit, T max_cost)
    {
        m_freq_scale = 1 / freq_unit;
        m_amp_scale = 1 / amp_unit;
        m_max_cost = max_cost;
    }
    
    /**
     * @brief Resets the state of the partial_tracker.
     *
     * This method clears all tracked data and resets the internal state of the `partial_tracker` object,
     * allowing it to start fresh for new calculations. It is typically used to reinitialize the tracker
     * before processing a new set of data.
     */
    
    void reset()
    {
        for (size_t i = 0; i < max_tracks(); i++)
            m_tracks[i] = track<T>{};
        
        m_changes.reset();
    }
    
    /**
     * @brief Processes an array of peaks with specified threshold.
     *
     * This method analyzes the provided array of `peak` objects, applying the specified start threshold
     * to determine which peaks to include in the processing. It performs the necessary calculations
     * or tracking operations based on the peaks and the threshold value.
     *
     * @param peaks A pointer to an array of `peak<T>` objects to be processed.
     * @param n_peaks The number of peaks in the array.
     * @param start_threshold The threshold value used to determine the significance of the peaks
     * for processing. Peaks below this threshold may be ignored.
     */
    
    void process(peak<T> *peaks, size_t n_peaks, T start_threshold)
    {
        // Setup
        
        n_peaks = std::min(n_peaks, max_peaks());
        
        reset_assignments();
        m_changes.reset();
        
        // Calculate cost functions
        
        size_t n_costs = find_costs(peaks, n_peaks);
        
        // Sort costs
        
        std::sort(m_costs, m_costs + n_costs,
                  [](const cost& a, const cost& b)
                  { return std::get<0>(a) < std::get<0>(b); });
        
        // Assign in order of lowest cost
        
        for (size_t i = 0; i < n_costs; i++)
        {
            size_t peak_idx = std::get<1>(m_costs[i]);
            size_t track_idx = std::get<2>(m_costs[i]);
            
            if (!m_peak_assigned[peak_idx] && !m_track_assigned[track_idx])
            {
                cpeak<T>& new_peak = peaks[peak_idx];
                cpeak<T>& old_peak = m_tracks[track_idx].m_peak;
                
                m_changes.add_change(new_peak, old_peak, m_use_pitch, m_use_db);

                m_tracks[track_idx].set_peak(new_peak, false);
                assign(peak_idx, track_idx);
            }
        }
        
        m_changes.complete();
        
        // Start new tracks (prioritised by input order)
        
        for (size_t i = 0, j = 0; i < n_peaks && j < max_tracks(); i++)
        {
            if (!m_peak_assigned[i] && peaks[i].amp() >= start_threshold)
            {
                for (; j < max_tracks(); j++)
                {
                    if (!m_track_assigned[j])
                    {
                        m_tracks[j].set_peak(peaks[i], true);
                        assign(i, j);
                        break;
                    }
                }
            }
        }
        
        // Make unassigned tracks inactive
        
        for (size_t i = 0; i < max_tracks(); i++)
        {
            if (!m_track_assigned[i])
                m_tracks[i] = track<T>{};
        }
    }
    
    /**
     * @brief Retrieves a constant reference to a track at a specified index.
     *
     * This method returns a constant reference to the track object stored at the given index `idx`.
     * It allows users to access the track without modifying it, ensuring that the original track data remains unchanged.
     *
     * @param idx The index of the track to retrieve. It must be within the valid range of stored tracks.
     * @return A constant reference to the track at the specified index.
     */
    
    const track<T> &get_track(size_t idx) { return m_tracks[idx]; }
    
    /**
     * @brief Retrieves the maximum number of peaks that can be stored.
     *
     * This method returns the maximum number of peaks that the tracker can manage, as defined by
     * the `m_max_peaks` member variable. It provides a way to access this value without modifying
     * the state of the `partial_tracker` object.
     *
     * @return The maximum number of peaks that can be stored, represented as a `size_t`.
     */
    
    size_t max_peaks() const { return m_max_peaks; }
    
    /**
     * @brief Retrieves the maximum number of tracks that can be stored.
     *
     * This method returns the maximum number of tracks that the tracker can manage, as defined by
     * the `m_max_tracks` member variable. It allows access to this value without altering the state
     * of the `partial_tracker` object.
     *
     * @return The maximum number of tracks that can be stored, represented as a `size_t`.
     */
    
    size_t max_tracks() const { return m_max_tracks; }
    
    /**
     * @brief Activates or deactivates change tracking based on the provided flag.
     *
     * This method allows the user to enable or disable the tracking of changes within the `partial_tracker`.
     * It is only enabled if the template parameter `B` (defaulting to `Changes`) is true. When called,
     * it updates the internal state of the change tracker to reflect the desired activity status.
     *
     * @tparam B A compile-time boolean condition that determines whether change tracking is enabled.
     * Defaults to the `Changes` parameter.
     * @param calc A boolean flag indicating whether to activate (`true`) or deactivate (`false`) change tracking.
     */
    
    template<bool B = Changes, enable_if_t<B> = 0>
    void calc_changes(bool calc) { return m_changes.active(calc); }
    
    /**
     * @brief Retrieves the sum of frequency changes tracked.
     *
     * This method returns the total sum of frequency changes that have been recorded by the change tracker.
     * It is only enabled if the template parameter `B` (defaulting to `Changes`) is true, allowing access
     * to frequency change data only when change tracking is active.
     *
     * @tparam B A compile-time boolean condition that determines whether change tracking is enabled.
     * Defaults to the `Changes` parameter.
     * @return The sum of frequency changes, represented as a value of type `T`.
     */
    
    template<bool B = Changes, enable_if_t<B> = 0>
    T freq_change_sum() const { return m_changes.freq_sum(); }
    
    /**
     * @brief Retrieves the absolute sum of frequency changes tracked.
     *
     * This method returns the total absolute sum of frequency changes that have been recorded by the change tracker.
     * It is only enabled if the template parameter `B` (defaulting to `Changes`) is true, allowing access
     * to the absolute frequency change data only when change tracking is active.
     *
     * @tparam B A compile-time boolean condition that determines whether change tracking is enabled.
     * Defaults to the `Changes` parameter.
     * @return The absolute sum of frequency changes, represented as a value of type `T`.
     */
    
    template<bool B = Changes, enable_if_t<B> = 0>
    T freq_change_abs() const { return m_changes.freq_abs(); }
    
    /**
     * @brief Retrieves the sum of amplitude changes tracked.
     *
     * This method returns the total sum of amplitude changes that have been recorded by the change tracker.
     * It is only enabled if the template parameter `B` (defaulting to `Changes`) is true, allowing access
     * to amplitude change data only when change tracking is active.
     *
     * @tparam B A compile-time boolean condition that determines whether change tracking is enabled.
     * Defaults to the `Changes` parameter.
     * @return The sum of amplitude changes, represented as a value of type `T`.
     */
    
    template<bool B = Changes, enable_if_t<B> = 0>
    T amp_change_sum() const { return m_changes.amp_sum(); }
    
    /**
     * @brief Retrieves the absolute sum of amplitude changes tracked.
     *
     * This method returns the total absolute sum of amplitude changes that have been recorded by the change tracker.
     * It is only enabled if the template parameter `B` (defaulting to `Changes`) is true, allowing access
     * to the absolute amplitude change data only when change tracking is active.
     *
     * @tparam B A compile-time boolean condition that determines whether change tracking is enabled.
     * Defaults to the `Changes` parameter.
     * @return The absolute sum of amplitude changes, represented as a value of type `T`.
     */
    
    template<bool B = Changes, enable_if_t<B> = 0>
    T amp_change_abs() const { return m_changes.amp_abs(); }
    
private:
    
    /**
     * @brief Initializes the partial_tracker with the specified number of peaks and tracks.
     *
     * This method sets up the internal structures of the `partial_tracker` by allocating resources
     * for the given number of peaks and tracks. It should be called before any processing occurs to
     * ensure that the tracker is properly configured.
     *
     * @param n_peaks The number of peaks that the tracker will manage.
     * @param n_tracks The number of tracks that the tracker will manage.
     */
    
    void init(size_t n_peaks, size_t n_tracks)
    {
        m_max_peaks = n_peaks;
        m_max_tracks = n_tracks;
        
        m_tracks = m_allocator.template allocate<track<T>>(n_tracks);
        m_costs = m_allocator.template allocate<cost>(n_tracks * n_peaks);
        m_peak_assigned = m_allocator.template allocate<bool>(n_peaks);
        m_track_assigned = m_allocator.template allocate<bool>(n_tracks);
        
        reset();
        
        set_cost_calculation(true, true, true);
        set_cost_scaling(T(0.5), T(6), T(1));
    }
    
    /**
     * @brief Resets all assignments within the partial_tracker.
     *
     * This method clears any existing assignments of peaks to tracks, allowing the tracker to start
     * fresh with new assignments. It is typically used before reassigning peaks to ensure that
     * there are no leftover or conflicting assignments from previous operations.
     */
    
    void reset_assignments()
    {
        for (size_t i = 0; i < max_peaks(); i++)
            m_peak_assigned[i] = false;
        
        for (size_t i = 0; i < max_tracks(); i++)
            m_track_assigned[i] = false;
    }
    
    /**
     * @brief Assigns a peak to a track.
     *
     * This method assigns a specific peak, identified by `peak_idx`, to a track, identified by `track_idx`.
     * It establishes a relationship between the peak and the track within the `partial_tracker`, allowing
     * for organized management and processing of the peaks associated with the specified track.
     *
     * @param peak_idx The index of the peak to be assigned.
     * @param track_idx The index of the track to which the peak will be assigned.
     */
    
    void assign(size_t peak_idx, size_t track_idx)
    {
        m_peak_assigned[peak_idx] = true;
        m_track_assigned[track_idx] = true;
    }
    
    /**
     * @brief Calculates the absolute cost between two values.
     *
     * This static method computes the absolute cost based on the difference between two values `a` and `b`,
     * scaled by a given `scale` factor. The resulting cost can be used in various calculations where
     * relative differences are significant.
     *
     * @param a The first value to compare.
     * @param b The second value to compare.
     * @param scale The scaling factor applied to the absolute difference between `a` and `b`.
     * @return The calculated absolute cost, represented as a value of type `T`.
     */
    
    static T cost_abs(T a, T b, T scale)
    {
        return std::abs(a - b) * scale;
    }
    
    /**
     * @brief Calculates the squared cost between two values.
     *
     * This static method computes the squared cost based on the difference between two values `a` and `b`,
     * scaled by a given `scale` factor. The resulting cost is useful in contexts where squared differences
     * are significant, such as in optimization problems or when calculating variance.
     *
     * @param a The first value to compare.
     * @param b The second value to compare.
     * @param scale The scaling factor applied to the squared difference between `a` and `b`.
     * @return The calculated squared cost, represented as a value of type `T`.
     */
    
    static T cost_sq(T a, T b, T scale)
    {
        const T c = (a - b);
        return c * c * scale;
    }
    
    /**
     * @brief Determines the usefulness of a given cost value.
     *
     * This static method evaluates a cost value and determines its usefulness based on predefined criteria.
     * The method may apply transformations or thresholds to the cost to provide a meaningful output that
     * can be used in subsequent calculations or decision-making processes.
     *
     * @param cost The cost value to be evaluated for usefulness.
     * @return The transformed or evaluated cost value, represented as a value of type `T`.
     */
    
    static T cost_useful(T cost)
    {
        return 1 - std::exp(-cost);
    }
    
    /**
     * @brief Computes costs for a set of peaks using specified functions.
     *
     * This method calculates the costs associated with a given array of peaks. It utilizes the provided
     * cost function (`CostFunc`) and the specified getter methods for frequency (`Freq`) and amplitude (`Amp`)
     * to determine the costs for each peak in the input array.
     *
     * @tparam CostFunc A function pointer type that defines how to calculate the cost.
     * @tparam Freq A member function pointer type used to retrieve the frequency of each peak.
     * @tparam Amp A member function pointer type used to retrieve the amplitude of each peak.
     * @param peaks A pointer to an array of `cpeak<T>` objects representing the peaks for which costs are to be computed.
     * @param n_peaks The number of peaks in the input array.
     * @return The total number of costs computed.
     */
    
    template<CostType CostFunc, GetMethod Freq, GetMethod Amp>
    size_t find_costs(cpeak<T> *peaks, size_t n_peaks)
    {
        size_t n_costs = 0;
        
        T freq_scale = m_square_cost ? m_freq_scale * m_freq_scale : m_freq_scale;
        T amp_scale = m_square_cost ? m_amp_scale * m_amp_scale : m_amp_scale;
        
        for (size_t i = 0; i < n_peaks; i++)
        {
            for (size_t j = 0; j < max_tracks(); j++)
            {
                if (m_tracks[j].active())
                {
                    cpeak<T>& a = peaks[i];
                    cpeak<T>& b = m_tracks[j].m_peak;
                    
                    T cost = CostFunc((a.*Freq)(), (b.*Freq)(), freq_scale)
                    + CostFunc((a.*Amp)(), (b.*Amp)(), amp_scale);
                    
                    if (cost < m_max_cost)
                        m_costs[n_costs++] = std::make_tuple(cost, i, j);
                }
            }
        }
        
        return n_costs;
    }
    
    /**
     * @brief Computes costs for a set of peaks.
     *
     * This method calculates the costs associated with a given array of peaks. It analyzes each peak
     * in the provided array and computes the corresponding costs based on predefined criteria. The
     * results can be used for further processing or analysis.
     *
     * @param peaks A pointer to an array of `peak<T>` objects representing the peaks for which costs are to be computed.
     * @param n_peaks The number of peaks in the input array.
     * @return The total number of costs computed.
     */
    
    size_t find_costs(peak<T> *peaks, size_t n_peaks)
    {
        // Conveniences for code length
        
        using p = peak<T>;
        const auto np = n_peaks;
        
        size_t select = (m_square_cost ? 4 : 0) +
                        (m_use_pitch ? 2 : 0) +
                        (m_use_db ? 1 : 0);
        
        // Create inner loops
        
        switch (select)
        {
            case 0: return find_costs<cost_abs, &p::freq, &p::amp>(peaks, np);
            case 1: return find_costs<cost_abs, &p::freq, &p::db>(peaks, np);
            case 2: return find_costs<cost_abs, &p::pitch, &p::amp>(peaks, np);
            case 3: return find_costs<cost_abs, &p::pitch, &p::db>(peaks, np);
            case 4: return find_costs<cost_sq, &p::freq, &p::amp>(peaks, np);
            case 5: return find_costs<cost_sq, &p::freq, &p::db>(peaks, np);
            case 6: return find_costs<cost_sq, &p::pitch, &p::amp>(peaks, np);
            default: return find_costs<cost_sq, &p::pitch, &p::db>(peaks, np);
        }
    }
    
    // Allocator
    
    /**
     * @brief The allocator used for memory management.
     *
     * This member variable stores an instance of the allocator type, which is responsible for
     * managing memory allocation and deallocation for the `partial_tracker`. It allows for custom
     * memory management strategies to be employed, providing flexibility in how resources are handled.
     */
    
    Allocator m_allocator;
    
    // Sizes
    
    /**
     * @brief The maximum number of peaks that can be managed.
     *
     * This member variable stores the maximum number of peaks that the `partial_tracker` can handle.
     * It is used to limit the number of peaks that can be added or processed, ensuring that the
     * tracker operates within its allocated resources.
     */
    
    size_t m_max_peaks;
    
    /**
     * @brief The maximum number of tracks that can be managed.
     *
     * This member variable stores the maximum number of tracks that the `partial_tracker` can handle.
     * It sets a limit on the number of tracks that can be added or processed, ensuring efficient resource
     * management and preventing overflow beyond the allocated capacity.
     */
    
    size_t m_max_tracks;
    
    // Cost parameters
    
    /**
     * @brief Indicates whether pitch is used in cost calculations.
     *
     * This member variable is a boolean flag that determines whether the pitch information
     * should be included in the cost calculations within the `partial_tracker`. If set to `true`,
     * pitch will be factored into the calculations; if `false`, it will be ignored.
     */
    
    bool m_use_pitch;
    
    /**
     * @brief Indicates whether decibel (dB) values are used in cost calculations.
     *
     * This member variable is a boolean flag that determines whether the decibel (dB) information
     * should be included in the cost calculations within the `partial_tracker`. If set to `true`,
     * dB values will be factored into the calculations; if `false`, they will be disregarded.
     */
    
    bool m_use_db;
    
    /**
     * @brief Indicates whether costs should be squared in calculations.
     *
     * This member variable is a boolean flag that determines whether the cost values should be
     * squared during calculations within the `partial_tracker`. If set to `true`, the cost will be
     * calculated as the square of the original cost value; if `false`, the original cost value will
     * be used directly.
     */
    
    bool m_square_cost;
    
    /**
     * @brief The scaling factor applied to frequency values in cost calculations.
     *
     * This member variable stores the scaling factor for frequency values used in cost calculations
     * within the `partial_tracker`. It allows for normalization or adjustment of frequency values to
     * ensure that they are properly accounted for in the overall cost metrics.
     */
    
    T m_freq_scale;
    
    /**
     * @brief The scaling factor applied to amplitude values in cost calculations.
     *
     * This member variable stores the scaling factor for amplitude values used in cost calculations
     * within the `partial_tracker`. It allows for normalization or adjustment of amplitude values to
     * ensure that they are properly incorporated into the overall cost metrics.
     */
    
    T m_amp_scale;
    
    /**
     * @brief The maximum allowable cost value in calculations.
     *
     * This member variable stores the maximum cost value that can be used in cost calculations
     * within the `partial_tracker`. It serves as a threshold to normalize or limit cost metrics,
     * ensuring that calculated costs do not exceed this predefined value.
     */
    
    T m_max_cost;
    
    // Tracking data
    
    /**
     * @brief Pointer to an array of track objects managed by the partial_tracker.
     *
     * This member variable stores a pointer to an array of `track<T>` objects, which represent
     * the tracks managed by the `partial_tracker`. It allows for dynamic management of tracks
     * and facilitates operations such as adding, removing, and processing tracks within the tracker.
     */
    
    track<T> *m_tracks;
    
    /**
     * @brief Pointer to an array of cost objects associated with the peaks.
     *
     * This member variable stores a pointer to an array of `cost` objects, which represent the
     * costs calculated for each peak managed by the `partial_tracker`. It enables efficient
     * storage and retrieval of cost data for subsequent processing and analysis.
     */
    
    cost *m_costs;
    
    /**
     * @brief Pointer to an array indicating the assignment status of each peak.
     *
     * This member variable stores a pointer to an array of boolean values, where each element indicates
     * whether a corresponding peak has been assigned to a track. It facilitates tracking of peak assignments
     * and helps manage the relationships between peaks and tracks within the `partial_tracker`.
     */
    
    bool *m_peak_assigned;
    
    /**
     * @brief Pointer to an array indicating the assignment status of each track.
     *
     * This member variable stores a pointer to an array of boolean values, where each element indicates
     * whether a corresponding track has been assigned to a peak. It facilitates tracking of track assignments
     * and helps manage the relationships between tracks and peaks within the `partial_tracker`.
     */
    
    bool *m_track_assigned;
    
    /**
     * @brief The change tracker for monitoring variations in data.
     *
     * This member variable is an instance of the `change_tracker` class, which is responsible for
     * monitoring and recording changes in the tracked values. It uses the template type `T` to define
     * the data type being tracked and the boolean parameter `Changes` to determine whether change
     * tracking functionality is enabled.
     */
    
    change_tracker<T, Changes> m_changes;
};

#endif
