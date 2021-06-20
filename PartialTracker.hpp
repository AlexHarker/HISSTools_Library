
#ifndef PARTIAL_TRACKER_H
#define PARTIAL_TRACKER_H

#include <algorithm>
#include <limits>
#include <tuple>

#include "Allocator.hpp"

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
    
    const T &freq() { return m_freq; }
    const T &amp() { return m_amp; }
    
    const T &pitch()
    {
        if (m_pitch == std::numeric_limits<T>::infinity())
            m_pitch = std::log2(m_freq / 440.0) * 12.0 + 69.0;
        
        return m_pitch;
    }
    
    const T &db()
    {
        if (m_db == std::numeric_limits<T>::infinity())
            m_db = std::log10(m_amp) * 20.0;
            
        return m_db;
    }
    
private:
    
    T m_freq;
    T m_amp;
    T m_pitch;
    T m_db;
};

template <typename T>
struct track
{
    track() : m_peak(), m_active(false), m_start(false) {}
    track(peak<T> peak) : m_peak(peak), m_active(true), m_start(true) {}
    
    void set_peak(const peak<T>& peak)
    {
        m_peak = peak;
        m_start = false;
    }
    
    peak<T> m_peak;
    bool m_active;
    bool m_start;
};

template <typename T, size_t N, size_t M>//, typename Allocator = aligned_allocator>
class peak_tracker
{
    typedef T (*CostType)(T, T, T);
    typedef const T&(peak<T>::*GetMethod)();

    using cost = std::tuple<T, size_t, size_t>;
    
public:

    peak_tracker() : m_freq_scale(1), m_amp_scale(1)
    {
        reset();
        set_cost_calculation(true, true, true);
        set_cost_scaling(T(0.5), T(6), T(1));
    }
    
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
        for (size_t i = 0; i < N; i++)
            m_tracks[i] = track<T>{};
    }
    
    void process(peak<T> *peaks, size_t n_peaks, T start_threshold)
    {
        // Setup
        
        n_peaks = std::min(n_peaks, max_peaks());

        reset_assignments();
        
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
                m_tracks[track_idx].set_peak(peaks[peak_idx]);
                assign(peak_idx, track_idx);
            }
        }
        
        // Start new tracks (prioritised by input order)
        
        for (size_t i = 0, j = 0; i < n_peaks && j < max_tracks(); i++)
        {
            if (!m_peak_assigned[i] && peaks[i].amp() >= start_threshold)
            {
                for (; j < max_tracks(); j++)
                {
                    if (!m_track_assigned[j])
                    {
                        m_tracks[j] = track<T>(peaks[i]);
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
    
private:
    
    size_t max_peaks() const { return M; }
    size_t max_tracks() const { return N; }
    
    void reset_assignments()
    {
        for (size_t i = 0; i < M; i++)
            m_peak_assigned[i] = false;
        
        for (size_t i = 0; i < N; i++)
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
    
    template<CostType CostFunc, GetMethod Freq, GetMethod Amp>
    size_t find_costs(peak<T> *peaks, size_t n_peaks)
    {
        size_t n_costs = 0;
        
        T freq_scale = m_square_cost ? m_freq_scale * m_freq_scale : m_freq_scale;
        T amp_scale = m_square_cost ? m_amp_scale * m_amp_scale : m_amp_scale;

        for (size_t i = 0; i < n_peaks; i++)
        {
            for (size_t j = 0; j < N; j++)
            {
                if (m_tracks[j].m_active)
                {
                    peak<T>& a = peaks[i];
                    peak<T>& b = m_tracks[j].m_peak;
                
                    T cost = CostFunc((a.*Freq)(), (b.*Freq)(), freq_scale)
                    + CostFunc((a.*Amp)(), (b.*Amp)(), amp_scale);
                
                    if (cost < m_max_cost)
                        m_costs[n_costs++] = std::make_tuple(cost, i, j);
                }
            }
        }
        
        return n_costs;
    }
    
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
    
    bool m_use_pitch;
    bool m_use_db;
    bool m_square_cost;
    track<T> m_tracks[N];
    cost m_costs[M * N];
    bool m_peak_assigned[M];
    bool m_track_assigned[N];
    T m_freq_scale;
    T m_amp_scale;
    T m_max_cost;
};

#endif
