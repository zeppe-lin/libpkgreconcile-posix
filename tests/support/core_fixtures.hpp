// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBPKGRECONCILE_POSIX_TEST_FIXTURES_HPP
#define LIBPKGRECONCILE_POSIX_TEST_FIXTURES_HPP

#include <libpkgreconcile/libpkgreconcile.h>

#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace test_fixtures {

inline pkgreconcile::reconciliation_target_reference
make_target(std::initializer_list<std::uint8_t> bytes = {42U},
            std::string provider = "test.target/v1")
{
  return pkgreconcile::reconciliation_target_reference::make(
      std::move(provider), std::vector<std::uint8_t>(bytes));
}

inline pkgreconcile::rejected_object_locator
make_locator(std::initializer_list<std::uint8_t> bytes = {7U},
             std::string provider = "test.object/v1")
{
  return pkgreconcile::rejected_object_locator::make(
      std::move(provider), std::vector<std::uint8_t>(bytes));
}

inline pkgreconcile::pending_reconciliation
make_pending(const char* path = "etc/tool.conf",
             pkgreconcile::rejected_object_side side =
                 pkgreconcile::rejected_object_side::incoming,
             std::initializer_list<std::uint8_t> object_bytes = {7U},
             std::initializer_list<std::uint8_t> target_bytes = {42U})
{
  return pkgreconcile::pending_reconciliation::make(
      make_target(target_bytes), path, side, make_locator(object_bytes));
}

} // namespace test_fixtures

#endif
