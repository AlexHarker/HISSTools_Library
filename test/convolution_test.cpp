
#include <iostream>
#include <cctype>

#include "test_utils/timer.hpp"
#include "test_utils/tabbed_out.hpp"

#include "../include/convolution/multichannel.hpp"
#include "../include/random_generator.hpp"

using namespace htl_test_utils;

template <class T>
std::vector<T> random_vector(int length) {
  std::vector<T> v(length);
  htl::random_generator<> gen;

  for (int i = 0; i < length; i++) v[i] = gen.rand_double();

  return v;
}

template <template <class, class> class Convolver, class T, class IO>
void test_time_domain(int block_size, int num_blocks, int ir_length,
                      const char* str) {
  Convolver<T, IO> convolver;

  std::vector<IO> in = random_vector<IO>(block_size * num_blocks);
  std::vector<T> ir = random_vector<T>(ir_length);
  std::vector<IO> out(block_size * num_blocks, IO(0));

  convolver.set(ir.data(), ir_length);

  steady_timer timer;
  timer.start();

  for (int i = 0; i < num_blocks; i++) {
    const int offset = block_size * i;
    convolver.process(in.data() + offset, out.data() + offset, block_size,
                      true);
  }

  timer.stop();
  timer.finish(str);
}

template <template <class, class> class Convolver, class T, class IO>
void test_what_happens(int block_size, int ir_length) {
  Convolver<T, IO> convolver;

  std::vector<IO> in(block_size, IO(1));
  std::vector<T> ir(ir_length, IO(1));
  std::vector<IO> out(block_size, IO(0));

  convolver.set(ir.data(), ir_length);

  convolver.process(in.data(), out.data(), block_size, false);

  for (int i = 0; i < block_size; i++)
    std::cout << out[i] << " result " << i << "\n";
}

int main(int argc, const char* argv[]) {
  test_what_happens<htl::convolve_time_domain, double, double>(64, 512);
  test_what_happens<htl::convolve_time_domain, float, float>(64, 512);

  test_time_domain<htl::convolve_time_domain, double, double>(
      256, 10000, 1024, "Test double double -");
  test_time_domain<htl::convolve_time_domain, double, float>(
      256, 10000, 1024, "Test double float  -");
  test_time_domain<htl::convolve_time_domain, float, double>(
      256, 10000, 1024, "Test float  double -");
  test_time_domain<htl::convolve_time_domain, float, float>(
      256, 10000, 1024, "Test float  float  -");

  test_what_happens<htl::convolve_partitioned, double, double>(64, 512);
  test_what_happens<htl::convolve_partitioned, float, float>(64, 512);

  test_time_domain<htl::convolve_partitioned, double, double>(
      256, 10000, 131072, "Test double double -");
  test_time_domain<htl::convolve_partitioned, double, float>(
      256, 10000, 131072, "Test double float  -");
  test_time_domain<htl::convolve_partitioned, float, double>(
      256, 10000, 131072, "Test float  double -");
  test_time_domain<htl::convolve_partitioned, float, float>(
      256, 10000, 131072, "Test float  float  -");

  return 0;
}
