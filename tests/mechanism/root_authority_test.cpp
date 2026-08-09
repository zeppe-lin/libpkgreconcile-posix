// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgreconcile-posix/error.h>
#include <libpkgreconcile-posix/inventory_store.h>

#include "support/core_fixtures.hpp"
#include "support/temp_directory.hpp"
#include "support/test.hpp"

#include <filesystem>

#include <unistd.h>

int main()
{
  using pkgreconcile::posix::inventory_generation_store;
  using pkgreconcile::posix::store_error;
  test_support::runner tests;

  tests.run("store handle remains anchored after pathname replacement", [] {
    test_support::temp_directory temporary;
    const auto target = test_fixtures::make_target();
    const auto configured = temporary.path() / "reconcile";
    const auto held = temporary.path() / "reconcile.held";
    const auto first = test_fixtures::make_pending("etc/first.conf");
    const auto second = test_fixtures::make_pending(
        "etc/second.conf", pkgreconcile::rejected_object_side::incoming, {8U});

    inventory_generation_store held_store(configured, target);
    TEST_CHECK(held_store.publish_pending({first}).changed());
    std::filesystem::rename(configured, held);

    inventory_generation_store replacement(configured, target);
    TEST_CHECK(replacement.read().size() == 0U);
    TEST_CHECK(held_store.read().size() == 1U);
    TEST_CHECK(held_store.root_path() == configured);
    TEST_CHECK(held_store.publish_pending({second}).changed());
    TEST_CHECK(held_store.read().size() == 2U);
    TEST_CHECK(replacement.read().size() == 0U);

    auto reopened = inventory_generation_store::open_existing(held, target);
    TEST_CHECK(reopened.read().size() == 2U);
  });

  tests.run("store root symlink is rejected", [] {
    test_support::temp_directory temporary;
    const auto real = temporary.path() / "real";
    const auto alias = temporary.path() / "alias";
    inventory_generation_store real_store(real, test_fixtures::make_target());
    TEST_CHECK(::symlink("real", alias.c_str()) == 0);
    TEST_CHECK_THROWS(
        static_cast<void>(inventory_generation_store::open_existing(
            alias, test_fixtures::make_target())),
        store_error);
    TEST_CHECK_THROWS(
        static_cast<void>(inventory_generation_store(
            alias, test_fixtures::make_target())),
        store_error);
  });

  return tests.finish();
}
