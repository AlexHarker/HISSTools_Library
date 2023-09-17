
#ifndef HISSTOOLS_LIBRARY_PARTIAL_TRACKER_HPP
#define HISSTOOLS_LIBRARY_PARTIAL_TRACKER_HPP

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

#include "allocator.hpp"
#include "namespace.hpp"

HISSTOOLS_LIBRARY_NAMESPACE_START()

template <typename T>
class peak
{
public:
    
    peak(T freq, T amp)
    : m_freq(freq)
    , m_amp(amp)
    , m_pitch(std::numeric_limits<T>::infinity())
    , m_db(std::numeric_limits<T>::infinity())
    {}
    
    peak() : peak(0, 0) {}
    
    const T &freq() const { return m_freq; }
    const T &amp() const { return m_amp; }
    
    const T &pitch() const
    {
        if (m_pitch == std::numeric_limits<T>::infinity())
            m_pitch = std::log2(m_freq / 440.0) * 12.0 + 69.0;
        
        return m_pitch;
    }
    
    const T &db() const
    {
        if (m_db == std::numeric_limits<T>::infinity())
            m_db = std::log10(m_amp) * 20.0;
        
        return m_db;
    }
    
private:
    
    T m_freq;
    T m_amp;
    mutable T m_pitch;
    mutable T m_db;
};

template <typename T>
using cpeak = const peak<T>;

template <typename T>
struct track
{
    enum class state { off_state, start_state, continue_state, switch_state };
    
    track() : m_peak(), m_state(state::off_state) {}
    
    void set_peak(cpeak<T>& peak, bool start)
    {
        m_peak = peak;
        m_state = start ? (active() ? state::switch_state : state::start_state): state::continue_state;
    }
    
    bool active() const { return m_state != state::off_state; }
    
    peak<T> m_peak;
    state m_state;
};

template <typename T, bool Impl>
class change_tracker
{
public:
    
    change_tracker() : m_active(true)
    {
        reset();
    }
    
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
    
    void reset()
    {
        m_freq_sum = 0;
        m_freq_abs = 0;
        m_amp_sum = 0;
        m_amp_abs = 0;
        m_count = 0;
    }
    
    T freq_sum() const { return m_freq_sum; }
    T freq_abs() const { return m_freq_abs; }
    T amp_sum() const { return m_amp_sum; }
    T amp_abs() const { return m_amp_abs; }
    
    void active(bool on) { m_active = on; }
    
private:
    
    T m_freq_sum;
    T m_freq_abs;
    T m_amp_sum;
    T m_amp_abs;
    
    bool m_active;
    size_t m_count;
};

template <typename T>
class change_tracker<T, false>
{
public:
    void add_change(cpeak<T>& now, cpeak<T>& prev, bool use_pitch, bool use_db) {}
    void complete() {}
    void reset() {}
};

template <typename T, bool Changes = false, typename Allocator = malloc_allocator>
class partial_tracker
{
    using cost_type = T (*)(T, T, T);
    using get_method = const T&(peak<T>::*)() const;
    
    using cost = std::tuple<T, size_t, size_t>;
    
    template <bool B>
    using enable_if_t = typename std::enable_if<B, int>::type;
    
public:
    
    template <typename U = Allocator, enable_if_t<std::is_default_constructible<U>::value> = 0>
    partial_tracker(size_t n_tracks, size_t n_peaks)
    {
        init(n_tracks, n_peaks);
    }
    
    template <typename U = Allocator, enable_if_t<std::is_copy_constructible<U>::value> = 0>
    partial_tracker(const Allocator& allocator, size_t n_tracks, size_t n_peaks)
    : m_allocator(allocator)
    {
        init(n_tracks, n_peaks);
    }
    
    template <typename U = Allocator, enable_if_t<std::is_default_constructible<U>::value> = 0>
    partial_tracker(Allocator&& allocator, size_t n_tracks, size_t n_peaks)
    : m_allocator(allocator)
    {
        init(n_tracks, n_peaks);
    }
    
    ~partial_tracker()
    {
        m_allocator.deallocate(m_tracks);
        m_allocator.deallocate(m_costs);
        m_allocator.deallocate(m_peak_assigned);
        m_allocator.deallocate(m_track_assigned);
    }
    
    partial_tracker(const partial_tracker&) = delete;
    partial_tracker & operator=(const partial_tracker&) = delete;
    
    void set_cost_calculation(bool square_cost, bool use_pitch, bool use_db)
    {
        m_square_cost = square_cost;
        m_use_pitch = use_pitch;
        m_use_db = use_db;
    }
    
    void set_cost_scaling(T freq_unit, T amp_unit, T max_cost)
    {
        m_freq_scale = 1 / freq_unit;
        m_amp_scale = 1 / amp_unit;
        m_max_cost = max_cost;
    }
    
    void reset()
    {
        for (size_t i = 0; i < max_tracks(); i++)
            m_tracks[i] = track<T>{};
        
        m_changes.reset();
    }
    
    void process(peak<T>* peaks, size_t n_peaks, T start_threshold)
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
    
    const track<T> &get_track(size_t idx) { return m_tracks[idx]; }
    
    size_t max_peaks() const { return m_max_peaks; }
    size_t max_tracks() const { return m_max_tracks; }
    
    template <bool B = Changes, enable_if_t<B> = 0>
    void calc_changes(bool calc) { return m_changes.active(calc); }
    
    template <bool B = Changes, enable_if_t<B> = 0>
    T freq_change_sum() const { return m_changes.freq_sum(); }
    
    template <bool B = Changes, enable_if_t<B> = 0>
    T freq_change_abs() const { return m_changes.freq_abs(); }
    
    template <bool B = Changes, enable_if_t<B> = 0>
    T amp_change_sum() const { return m_changes.amp_sum(); }
    
    template <bool B = Changes, enable_if_t<B> = 0>
    T amp_change_abs() const { return m_changes.amp_abs(); }
    
private:
    
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
    
    void reset_assignments()
    {
        for (size_t i = 0; i < max_peaks(); i++)
            m_peak_assigned[i] = false;
        
        for (size_t i = 0; i < max_tracks(); i++)
            m_track_assigned[i] = false;
    }
    
    void assign(size_t peak_idx, size_t track_idx)
    {
        m_peak_assigned[peak_idx] = true;
        m_track_assigned[track_idx] = true;
    }
    
    static T cost_abs(T a, T b, T scale)
    {
        return std::abs(a - b) * scale;
    }
    
    static T cost_sq(T a, T b, T scale)
    {
        const T c = (a - b);
        return c * c * scale;
    }
    
    static T cost_useful(T cost)
    {
        return 1 - std::exp(-cost);
    }
    
    template <cost_type Cost, get_method Freq, get_method Amp>
    size_t find_costs(cpeak<T>* peaks, size_t n_peaks)
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
                    
                    T cost = Cost((a.*Freq)(), (b.*Freq)(), freq_scale)
                    + Cost((a.*Amp)(), (b.*Amp)(), amp_scale);
                    
                    if (cost < m_max_cost)
                        m_costs[n_costs++] = std::make_tuple(cost, i, j);
                }
            }
        }
        
        return n_costs;
    }
    
    size_t find_costs(peak<T>* peaks, size_t n_peaks)
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
    
    Allocator m_allocator;
    
    // Sizes
    
    size_t m_max_peaks;
    size_t m_max_tracks;
    
    // Cost parameters
    
    bool m_use_pitch;
    bool m_use_db;
    bool m_square_cost;
    
    T m_freq_scale;
    T m_amp_scale;
    T m_max_cost;
    
    // Tracking data
    
    track<T>* m_tracks;
    cost* m_costs;
    bool* m_peak_assigned;
    bool* m_track_assigned;
    
    change_tracker<T, Changes> m_changes;
};

HISSTOOLS_LIBRARY_NAMESPACE_END()

#endif /* HISSTOOLS_LIBRARY_PARTIAL_TRACKER_HPP */
