
#include "test_utils/timer.hpp"
#include "test_utils/tabbed_out.hpp"

#include "../include/audio_file/in_file.hpp"
#include "../include/audio_file/out_file.hpp"
#include "../include/audio_file/utilities.hpp"
#include "../include/random_generator.hpp"

using namespace htl_test_utils;

bool compareNans(double x, double y)
{
    return std::isnan(x) && std::isnan(y);
}

double convertIEEEExtended(double x)
{
    char bytes[10];
    
    htl::extended_double_convertor()(reinterpret_cast<unsigned char*>(bytes), x);
    auto y = htl::extended_double_convertor()(bytes);
    
    if (x != y && !(compareNans(x, y)))
        throw;
        
    return y;
}

int main(int argc, const char * argv[])
{
    htl::random_generator<> gen;
    
    auto r = gen.rand_double();
    auto rh = gen.rand_double() * 65536.0;
    auto inf = std::numeric_limits<double>::infinity();
    auto nanS = std::numeric_limits<double>::signaling_NaN();
    auto nanN = std::numeric_limits<double>::quiet_NaN();

    tabbed_out("random", to_string_with_precision(r));
    tabbed_out("random_hi", to_string_with_precision(rh));

    tabbed_out("+0", to_string_with_precision(convertIEEEExtended(0.0)));
    tabbed_out("-0",  to_string_with_precision(convertIEEEExtended(-0.0)));

    tabbed_out("+2",  to_string_with_precision(convertIEEEExtended(2)));
    tabbed_out("+0.5",  to_string_with_precision(convertIEEEExtended(0.5)));
    tabbed_out("-0.5",  to_string_with_precision(convertIEEEExtended(-0.5)));
    tabbed_out("+1.5",  to_string_with_precision(convertIEEEExtended(1.5)));
    tabbed_out("-1.5",  to_string_with_precision(convertIEEEExtended(-1.5)));
    tabbed_out("+2",  to_string_with_precision(convertIEEEExtended(2)));
    tabbed_out("-2",  to_string_with_precision(convertIEEEExtended(-2)));
    tabbed_out("+3.5",  to_string_with_precision(convertIEEEExtended(3.5)));
    tabbed_out("-3.5",  to_string_with_precision(convertIEEEExtended(-3.5)));
    tabbed_out("+16384",  to_string_with_precision(convertIEEEExtended(16384)));
    tabbed_out("-16384",  to_string_with_precision(convertIEEEExtended(-16384)));
    tabbed_out("+random",  to_string_with_precision(convertIEEEExtended(r)));
    tabbed_out("-random",  to_string_with_precision(convertIEEEExtended(-r)));
    tabbed_out("+random",  to_string_with_precision(convertIEEEExtended(rh)));
    tabbed_out("-random",  to_string_with_precision(convertIEEEExtended(-rh)));
    tabbed_out("+inf",  to_string_with_precision(convertIEEEExtended(inf)));
    tabbed_out("-inf",  to_string_with_precision(convertIEEEExtended(-inf)));
    tabbed_out("nan (signalling)",  to_string_with_precision(convertIEEEExtended(nanS)));
    tabbed_out("nan (quiet)",  to_string_with_precision(convertIEEEExtended(nanN)));
    
    for (size_t i = 0; i < 5000000; i++)
        convertIEEEExtended(gen.rand_double(-65536.0, 65536.0));
    
    return 0;
}
