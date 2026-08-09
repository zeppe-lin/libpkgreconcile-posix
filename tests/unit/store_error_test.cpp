// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgreconcile-posix/error.h>

#include "support/test.hpp"

#include <stdexcept>
#include <string>
#include <type_traits>

int main()
{
  test_support::runner tests;
  tests.run("store error is a runtime error", [] {
    static_assert(std::is_base_of_v<std::runtime_error,
                                    pkgreconcile::posix::store_error>);
    const pkgreconcile::posix::store_error error("fixture failure");
    TEST_CHECK(std::string(error.what()) == "fixture failure");
  });
  return tests.finish();
}
