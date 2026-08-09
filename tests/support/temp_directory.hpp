// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBPKGRECONCILE_POSIX_TEST_TEMP_DIRECTORY_HPP
#define LIBPKGRECONCILE_POSIX_TEST_TEMP_DIRECTORY_HPP

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <unistd.h>

namespace test_support {

class temp_directory final {
public:
  temp_directory()
  {
    std::string pattern = "/tmp/libpkgreconcile-posix-test.XXXXXX";
    std::vector<char> mutable_pattern(pattern.begin(), pattern.end());
    mutable_pattern.push_back('\0');
    char* created = ::mkdtemp(mutable_pattern.data());
    if (created == nullptr)
      std::abort();
    path_ = created;
  }

  temp_directory(const temp_directory&) = delete;
  temp_directory& operator=(const temp_directory&) = delete;

  ~temp_directory()
  {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }

  [[nodiscard]] const std::filesystem::path& path() const noexcept
  {
    return path_;
  }

private:
  std::filesystem::path path_;
};

} // namespace test_support

#endif
