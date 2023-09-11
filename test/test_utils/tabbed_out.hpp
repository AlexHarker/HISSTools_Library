
#ifndef HISSTOOLS_TESTS_TABBED_OUT_HPP
#define HISSTOOLS_TESTS_TABBED_OUT_HPP

#include <iomanip>
#include <iostream>
#include <sstream>

namespace htl_test_utils {

void tabbed_out(const std::string& name, const std::string& text, int tab = 25) {
  std::cout << std::setw(tab) << std::setfill(' ');
  std::cout.setf(std::ios::left);
  std::cout.unsetf(std::ios::right);
  std::cout << name;
  std::cout.unsetf(std::ios::left);
  std::cout << text << "\n";
}

template <typename T>
std::string to_string_with_precision(const T a_value, const int n = 4, bool fixed = true) {
  std::ostringstream out;
  
  if (fixed)
    out << std::setprecision(n) << std::fixed << a_value;
  else
    out << std::setprecision(n) << a_value;

  return out.str();
}

} // namespace htl_test_utils

#endif