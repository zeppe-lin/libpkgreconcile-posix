// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgreconcile-posix/inventory_store.h>
#include <libpkgreconcile-posix/error.h>

#include "support/core_fixtures.hpp"
#include "support/store_layout.hpp"
#include "support/temp_directory.hpp"
#include "support/test.hpp"

#include <filesystem>
#include <string>

int main()
{
  using pkgreconcile::posix::inventory_generation_store;
  using pkgreconcile::posix::store_error;
  test_support::runner tests;

  tests.run("initialize target-bound empty inventory", [] {
    test_support::temp_directory temporary;
    const auto target = test_fixtures::make_target();
    const auto root = temporary.path() / "reconcile";
    inventory_generation_store store(root, target);
    TEST_CHECK(store.root_path() == root);
    TEST_CHECK(store.target_binding() == target);
    TEST_CHECK(store.read().target() == target);
    TEST_CHECK(store.read().size() == 0U);
    TEST_CHECK(std::filesystem::is_regular_file(root / "binding"));
    TEST_CHECK(std::filesystem::is_regular_file(root / "current"));
    TEST_CHECK(test_support::generation_count(root) == 1U);
    TEST_CHECK(!test_support::has_temporary_entry(root));
  });

  tests.run("reopen exact target and reject another target", [] {
    test_support::temp_directory temporary;
    const auto target = test_fixtures::make_target();
    const auto root = temporary.path() / "reconcile";
    { inventory_generation_store created(root, target); }
    auto reopened = inventory_generation_store::open_existing(root, target);
    TEST_CHECK(reopened.read().size() == 0U);
    TEST_CHECK_THROWS(
        static_cast<void>(inventory_generation_store::open_existing(
            root, test_fixtures::make_target({99U}))),
        store_error);
  });

  tests.run("open existing never initializes missing path", [] {
    test_support::temp_directory temporary;
    const auto root = temporary.path() / "missing";
    TEST_CHECK_THROWS(
        static_cast<void>(inventory_generation_store::open_existing(
            root, test_fixtures::make_target())),
        store_error);
    TEST_CHECK(!std::filesystem::exists(root));
  });

  tests.run("reject store pathname with embedded NUL", [] {
    test_support::temp_directory temporary;
    std::string bytes = (temporary.path() / "reconcile").string();
    bytes.push_back('\0');
    bytes += ".alias";
    const std::filesystem::path malformed(bytes);
    TEST_CHECK_THROWS(
        static_cast<void>(inventory_generation_store(
            malformed, test_fixtures::make_target())),
        pkgreconcile::posix::store_error);
    TEST_CHECK(!std::filesystem::exists(temporary.path() / "reconcile"));
  });

  tests.run("reject empty store pathname", [] {
    TEST_CHECK_THROWS(
        static_cast<void>(inventory_generation_store(
            {}, test_fixtures::make_target())),
        store_error);
  });

  return tests.finish();
}
