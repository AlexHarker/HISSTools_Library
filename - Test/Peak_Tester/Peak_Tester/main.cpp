//
//  main.cpp
//  Peak_Tester
//
//  Created by Alex Harker on 19/06/2021.
//  Copyright © 2021 AHarker. All rights reserved.
//

#include <iostream>

#include "PeakTracker.hpp"

using Tracker = peak_tracker<double, 12, 10>;

void post_tracks(Tracker& tracker, int N)
{
    for (int i = 0; i < N; i++)
    {
        track<double> t = tracker.get_track(i);

        std::cout << "index " << i << "\n";
        std::cout << "active " << t.m_active << "\n";
        std::cout << "start " << t.m_start << "\n";
        std::cout << "freq " << t.m_peak.freq() << "\n";
        std::cout << "amp " << t.m_peak.amp() << "\n";
    }
}
        
int main(int argc, const char * argv[])
{
    peak_tracker<double, 12, 10> tracker;
    peak<double> peaks[10];
    
    for (int i = 0; i < 10; i++)
        peaks[i] = peak<double>((i + 1) * 10.0, 1.0);
    
    tracker.process(peaks, 10, 0.0);

    post_tracks(tracker, 12);
    
    for (int i = 0; i < 10; i++)
        peaks[i] = peak<double>((10 - i) * 10.0 + 2, 1.0);
    
    tracker.process(peaks, 8, 0.0);

    post_tracks(tracker, 12);
    
    return 0;
}