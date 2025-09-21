#include "math_operations.h"
#include <cassert>
#include <cmath>
#include <iostream>

namespace {
// Helper function to compare floating point numbers with epsilon tolerance
bool isApproximatelyEqual(double a, double b, double epsilon = 1e-9) {
  return std::abs(a - b) < epsilon;
}
} // anonymous namespace

int main() {
  std::cout << "Testing HBF C++ Component Integration\n";
  std::cout << "=====================================\n";

  // Test basic operations using the hbf_math namespace
  // Using appropriate types to match the library signatures

  // Test addition
  const int add_result = hbf_math::add(10, 20);
  std::cout << "hbf_math::add(10, 20) = " << add_result << '\n';
  assert(add_result == 30);

  // Test multiplication
  const int multiply_result = hbf_math::multiply(4, 7);
  std::cout << "hbf_math::multiply(4, 7) = " << multiply_result << '\n';
  assert(multiply_result == 28);

  // Test factorial - use proper type to avoid conversion warnings
  const auto factorial_result = hbf_math::factorial(5);
  std::cout << "hbf_math::factorial(5) = " << factorial_result << '\n';
  assert(factorial_result == 120);

  // Test power function
  const double power_result = hbf_math::power(2.0, 8);
  std::cout << "hbf_math::power(2.0, 8) = " << power_result << '\n';
  assert(isApproximatelyEqual(power_result, 256.0));

  // All tests passed
  std::cout << "\n✅ All C++ component integration tests passed!\n";
  std::cout << "   - Addition: PASS\n";
  std::cout << "   - Multiplication: PASS\n";
  std::cout << "   - Factorial: PASS\n";
  std::cout << "   - Power: PASS\n";

  return 0;
}
