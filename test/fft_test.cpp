
#include "test_utils/timer.hpp"
#include "test_utils/tabbed_out.hpp"

#include "../include/fft/fft.hpp"

using namespace htl_test_utils;

template <class T>
void fill_split(htl::split_type<T> split, int max_log2)
{
  for (long i = 0; i < (1 << max_log2); i++)
  {
    split.realp[i] = 1.0 - 2.0 * std::rand() / RAND_MAX;
    split.imagp[i] = 1.0 - 2.0 * std::rand() / RAND_MAX;
  }
}

template <class T>
uint64_t crash_test(int min_log2, int max_log2)
{
  htl::setup_type<T> setup;
  htl::split_type<T> split;

  split.realp = (T *)malloc(sizeof(T) * 1 << max_log2);
  split.imagp = (T *)malloc(sizeof(T) * 1 << max_log2);

  htl::create_fft_setup(&setup, max_log2);

  steady_timer timer;

  for (int i = min_log2; i < max_log2; i++)
  {
    fill_split(split, i);
    timer.start();
    htl::fft(setup, &split, i);
    timer.stop();
  }

  for (int i = min_log2; i < max_log2; i++)
  {
    fill_split(split, i);
    timer.start();
    htl::ifft(setup, &split, i);
    timer.stop();
  }

  for (int i = min_log2; i < max_log2; i++)
  {
    fill_split(split, i);
    timer.start();
    htl::rfft(setup, &split, i);
    timer.stop();
  }

  for (int i = min_log2; i < max_log2; i++)
  {
    fill_split(split, i);
    timer.start();
    htl::rifft(setup, &split, i);
    timer.stop();
  }

  uint64_t time = timer.finish("FFT Multiple Tests");

  free(split.realp);
  free(split.imagp);

  htl::destroy_fft_setup(setup);

  return time;
}

template <class T>
uint64_t single_test(int size, void (*fn)(htl::setup_type<T>,
                                          htl::split_type<T> *, uintptr_t))
{
  htl::setup_type<T> setup;
  htl::split_type<T> split;

  split.realp = (T *)malloc(sizeof(T) * 1 << size);
  split.imagp = (T *)malloc(sizeof(T) * 1 << size);

  htl::create_fft_setup(&setup, size);

  steady_timer timer;

  for (int i = 0; i < 10000; i++)
  {
    fill_split(split, size);

    timer.start();
    fn(setup, &split, size);
    timer.stop();
  }

  uint64_t time = timer.finish(std::string("FFT Single Tests// ").append(std::to_string(1 << size)));

  free(split.realp);
  free(split.imagp);
  htl::destroy_fft_setup(setup);

  return time;
}

template <class T>
uint64_t matched_size_test(int min_log2, int max_log2)
{
  uint64_t time = 0;

  std::cout << "---FFT---\n";

  for (int i = min_log2; i < max_log2; i++)
    time += single_test<T>(i, &htl::fft);

  std::cout << "---iFFT---\n";

  for (int i = min_log2; i < max_log2; i++)
    time += single_test<T>(i, &htl::ifft);

  std::cout << "---Real FFT---\n";

  for (int i = min_log2; i < max_log2; i++)
    time += single_test<T>(i, &htl::rfft);

  std::cout << "---Real iFFT---\n";

  for (int i = min_log2; i < max_log2; i++)
    time += single_test<T>(i, &htl::rifft);

  return time;
}

template <class T, class U>
bool zip_correctness_test(int min_log2, int max_log2)
{
  htl::split_type<T> split;

  U *ptr = (U *)malloc(sizeof(U) * 1 << max_log2);
  split.realp = (T *)malloc(sizeof(T) * 1 << (max_log2 - 1));
  split.imagp = (T *)malloc(sizeof(T) * 1 << (max_log2 - 1));

  for (int i = min_log2; i < max_log2; i++)
  {
    for (int j = 0; j < (1 << i); j++)
      ptr[j] = j;

    htl::unzip(ptr, &split, i);

    for (int j = 0; j < (1 << (i - 1)); j++)
    {
      if (split.realp[j] != (j << 1))
      {
        std::cout << "zip error\n";
        return true;
      }
      if (i > 1 && split.imagp[j] != (j << 1) + 1)
      {
        std::cout << "zip error\n";
        return true;
      }
    }

    htl::zip(&split, ptr, i);

    for (int j = 0; j < (1 << (i - 1)); j++)
    {
      if (ptr[j] != j)
      {
        std::cout << "unzip error\n";
        return true;
      }
    }
  }

  free(ptr);
  free(split.realp);
  free(split.imagp);

  std::cout << "FFT Zip Tests Successful\n";

  return false;
}

template <class T, class U>
uint64_t zip_test(int min_log2, int max_log2)
{
  htl::split_type<T> split;

  U *ptr = (U *)malloc(sizeof(U) * 1 << max_log2);
  split.realp = (T *)malloc(sizeof(T) * 1 << (max_log2 - 1));
  split.imagp = (T *)malloc(sizeof(T) * 1 << (max_log2 - 1));

  steady_timer timer;
  timer.start();

  for (int i = min_log2; i < max_log2; i++)
    htl::unzip(ptr, &split, i);

  for (int i = min_log2; i < max_log2; i++)
    htl::unzip_zero(ptr, &split, 1 << i, i);

  for (int i = min_log2; i < max_log2; i++)
    htl::zip(&split, ptr, i);

  timer.stop();
  uint64_t time = timer.finish("Zip Tests");

  free(ptr);
  free(split.realp);
  free(split.imagp);

  return time;
}

int main(int argc, const char *argv[])
{
  uint64_t time = 0;

  std::cout << "****** DOUBLE ******\n";

  if (zip_correctness_test<double, double>(1, 24))
  {
    std::cout << "Errors - did not complete tests\n";
    return -1;
  }

  std::cout << "****** FLOAT ******\n";

  if (zip_correctness_test<float, float>(1, 24))
  {
    std::cout << "Errors - did not complete tests\n";
    return -1;
  }

  std::cout << "****** DOUBLE ******\n";

  time += crash_test<double>(0, 22);

  std::cout << "****** FLOAT ******\n";

  time += crash_test<float>(0, 22);

  std::cout << "****** DOUBLE ******\n";

  time += matched_size_test<double>(6, 14);

  std::cout << "****** FLOAT ******\n";

  time += matched_size_test<float>(6, 14);

  std::cout << "****** DOUBLE ******\n";

  time += zip_test<double, double>(1, 24);

  std::cout << "****** FLOAT ******\n";

  time += zip_test<float, float>(1, 24);

  tabbed_out("FFT Tests Total ", to_string_with_precision(time / 1000000.0, 2),
             35);

  std::cout << "Finished Running\n";
  return 0;
}
