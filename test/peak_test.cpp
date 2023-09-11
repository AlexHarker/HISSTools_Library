
#include <iostream>

#include "../include/partial_tracker.hpp"

using tracker_type = htl::partial_tracker<double>;

void post_tracks(tracker_type& tracker, int N)
{
    for (int i = 0; i < N; i++)
    {
        htl::track<double> t = tracker.get_track(i);

        std::cout << "index " << i << "\n";
        std::cout << "active " << t.active() << "\n";
        std::cout << "state " << static_cast<int>(t.m_state) << "\n";
        std::cout << "freq " << t.m_peak.freq() << "\n";
        std::cout << "amp " << t.m_peak.amp() << "\n";
    }
}
        
int main(int argc, const char * argv[])
{
    tracker_type tracker(12, 12);
    htl::peak<double> peaks[10];
    
    for (int i = 0; i < 10; i++)
        peaks[i] = htl::peak<double>((i + 1) * 10.0, 1.0);
    
    tracker.process(peaks, 10, 0.0);

    post_tracks(tracker, 12);
    
    for (int i = 0; i < 10; i++)
        peaks[i] = htl::peak<double>((10 - i) * 10.0 + 2, 1.0);
    
    tracker.process(peaks, 8, 0.0);

    post_tracks(tracker, 12);
    
    return 0;
}
