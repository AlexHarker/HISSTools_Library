
#include <iostream>

#include "../include/simd_support.hpp"

int main(int argc, const char * argv[])
{
    float values_f[8] = { 2, 1, 3, 4, 5, 6, 7, 8};
    double values_d[4] = { 9, 8, 10, 11};
    
    htl::simd_type<float, 1> vf0(values_f);
    htl::simd_type<double, 1> vd0(values_d);
    htl::simd_type<float, 4> vf1(values_f);
    htl::simd_type<double, 2> vd1(values_d);
    // htl::simd_type<float, 8> vf2(values_f);
    // htl::simd_type<double, 4> vd2(values_d);

    std::cout << sum(vf0) <<" float 1 sum\n";
    std::cout << sum(vd0) <<" double 1 sum\n";
    std::cout << sum(vf1) <<" float 4 sum\n";
    std::cout << sum(vd1) <<" double 2 sum\n";
    // std::cout << sum(vf2) <<" float 8 sum\n";
    // std::cout << sum(vd2) <<" double 4 sum\n";
    return 0;
}
