#include "math_operations.h"
#include <iostream>

int main() {
  std::cout << "Testing HBF C++ Component Integration" << std::endl;

  // Test basic operations using the hbf_math namespace
  int result_add = hbf_math::add(10, 20);
  std::cout << "hbf_math::add(10, 20) = " << result_add << std::endl;

  int result_multiply = hbf_math::multiply(4, 7);
  std::cout << "hbf_math::multiply(4, 7) = " << result_multiply << std::endl;

  int result_factorial = hbf_math::factorial(5);
  std::cout << "hbf_math::factorial(5) = " << result_factorial << std::endl;

  double result_power = hbf_math::power(2.0, 8);
  std::cout << "hbf_math::power(2.0, 8) = " << result_power << std::endl;

  // Verify results
  if (result_add == 30 && result_multiply == 28 && result_factorial == 120 &&
      result_power == 256.0) {
    std::cout << "✅ All C++ component integration tests passed!" << std::endl;
    return 0;
  } else {
    std::cout << "❌ C++ component integration tests failed!" << std::endl;
    return 1;
  }
}
