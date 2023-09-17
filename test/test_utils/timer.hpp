
#ifndef HISSTOOLS_LIBRARY_TESTS_TIMER_HPP
#define HISSTOOLS_LIBRARY_TESTS_TIMER_HPP

#include <array>
#include <chrono>
#include <limits>

#include "tabbed_out.hpp"

namespace htl_test_utils
{
    class steady_timer
    {
        using clock = std::chrono::steady_clock;

        double ms(clock::duration duration)
        {
            using namespace std::chrono;
            return duration_cast<microseconds>(duration).count() / 1000.0;
        }

    public:
        steady_timer() : m_start{clock::duration::zero()},
                         m_store1{clock::duration::zero()},
                         m_store2{clock::duration::zero()} {};

        void start() { m_start = clock::now(); };

        void stop()
        {
            auto elapsed = clock::now() - m_start;

            m_store2 = m_store1;
            m_store1 += elapsed;
        }

        uint64_t finish(const std::string &msg)
        {
            tabbed_out(msg + " Elapsed ", to_string_with_precision(ms(m_store1), 2), 35);

            auto elapsed = m_store1;

            m_store2 = clock::duration::zero();
            m_store1 = clock::duration::zero();

            return ms(elapsed);
        };

        void relative(const std::string &msg)
        {
            tabbed_out(msg + " Comparison ",
                       to_string_with_precision(ms(m_store1) / ms(m_store2), 2), 35);
        }

    private:
        clock::time_point m_start;
        clock::duration m_store1, m_store2;
    };
} // namespace htl_test_utils

#endif /* HISSTOOLS_LIBRARY_TESTS_TIMER_HPP */
