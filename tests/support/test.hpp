// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBPKGRECONCILE_POSIX_TEST_SUPPORT_HPP
#define LIBPKGRECONCILE_POSIX_TEST_SUPPORT_HPP

#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace test_support {

class failure : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

template <typename T> std::string describe(const T& value)
{
  std::ostringstream output;
  output << value;
  return output.str();
}

[[noreturn]] inline void fail(const char* expression, const char* file,
                              int line, const std::string& detail = {})
{
  std::ostringstream output;
  output << file << ':' << line << ": check failed: " << expression;
  if (!detail.empty())
  {
    output << " (" << detail << ')';
  }
  throw failure(output.str());
}

class runner {
public:
  template <typename Function>
  void run(const std::string& name, Function&& function)
  {
    ++total_;
    try
    {
      std::forward<Function>(function)();
      std::cout << "ok " << total_ << " - " << name << '\n';
    }
    catch (const std::exception& error)
    {
      ++failed_;
      std::cerr << "not ok " << total_ << " - " << name << "\n  "
                << error.what() << '\n';
    }
    catch (...)
    {
      ++failed_;
      std::cerr << "not ok " << total_ << " - " << name
                << "\n  unknown exception\n";
    }
  }

  int finish() const
  {
    std::cout << "1.." << total_ << '\n';
    return failed_ == 0U ? 0 : 1;
  }

private:
  std::size_t total_{0};
  std::size_t failed_{0};
};

} // namespace test_support

#define TEST_CHECK(expression)                                                 \
  do                                                                           \
  {                                                                            \
    if (!(expression))                                                         \
    {                                                                          \
      ::test_support::fail(#expression, __FILE__, __LINE__);                  \
    }                                                                          \
  } while (false)

#define TEST_CHECK_EQ(actual, expected)                                        \
  do                                                                           \
  {                                                                            \
    const auto test_actual_value = (actual);                                   \
    const auto test_expected_value = (expected);                               \
    if (!(test_actual_value == test_expected_value))                           \
    {                                                                          \
      ::test_support::fail(                                                    \
          #actual " == " #expected, __FILE__, __LINE__,                       \
          "actual=" + ::test_support::describe(test_actual_value) +           \
              ", expected=" + ::test_support::describe(test_expected_value)); \
    }                                                                          \
  } while (false)

#define TEST_CHECK_THROWS(statement, exception_type)                           \
  do                                                                           \
  {                                                                            \
    bool test_thrown = false;                                                  \
    try                                                                        \
    {                                                                          \
      statement;                                                               \
    }                                                                          \
    catch (const exception_type&)                                              \
    {                                                                          \
      test_thrown = true;                                                      \
    }                                                                          \
    if (!test_thrown)                                                          \
    {                                                                          \
      ::test_support::fail(#statement " throws " #exception_type, __FILE__,   \
                           __LINE__);                                           \
    }                                                                          \
  } while (false)

#endif
