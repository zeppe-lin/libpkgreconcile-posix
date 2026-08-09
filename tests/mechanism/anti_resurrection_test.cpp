// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgreconcile-posix/inventory_store.h>

#include "support/core_fixtures.hpp"
#include "support/temp_directory.hpp"
#include "support/test.hpp"

int main()
{
  using pkgreconcile::reconciliation_record_status;
  using pkgreconcile::posix::inventory_generation_store;
  using pkgreconcile::posix::resolution_outcome;
  test_support::runner tests;

  tests.run("resolved tombstone suppresses replay after reopen", [] {
    test_support::temp_directory temporary;
    const auto root = temporary.path() / "reconcile";
    const auto target = test_fixtures::make_target();
    const auto resolved_value = test_fixtures::make_pending(
        "etc/tool.conf", pkgreconcile::rejected_object_side::incoming, {1U});
    {
      inventory_generation_store store(root, target);
      TEST_CHECK(store.publish_pending({resolved_value}).changed());
      TEST_CHECK(store.resolve(resolved_value) == resolution_outcome::resolved);
    }

    auto store = inventory_generation_store::open_existing(root, target);
    const auto new_value = test_fixtures::make_pending(
        "etc/other.conf", pkgreconcile::rejected_object_side::incoming, {2U});
    const auto receipt = store.publish_pending({resolved_value, new_value});
    TEST_CHECK(receipt.published() == 1U);
    TEST_CHECK(receipt.already_pending() == 0U);
    TEST_CHECK(receipt.suppressed_resolved() == 1U);
    const auto inventory = store.read();
    TEST_CHECK(inventory.find(resolved_value)->status() ==
               reconciliation_record_status::resolved);
    TEST_CHECK(inventory.find(new_value)->status() ==
               reconciliation_record_status::pending);
  });

  tests.run("distinct evidence for same path is not suppressed", [] {
    test_support::temp_directory temporary;
    inventory_generation_store store(
        temporary.path() / "reconcile", test_fixtures::make_target());
    const auto old_value = test_fixtures::make_pending(
        "etc/tool.conf", pkgreconcile::rejected_object_side::incoming, {1U});
    const auto new_value = test_fixtures::make_pending(
        "etc/tool.conf", pkgreconcile::rejected_object_side::incoming, {2U});
    TEST_CHECK(store.publish_pending({old_value}).changed());
    TEST_CHECK(store.resolve(old_value) == resolution_outcome::resolved);
    const auto receipt = store.publish_pending({new_value});
    TEST_CHECK(receipt.published() == 1U);
    TEST_CHECK(receipt.suppressed_resolved() == 0U);
    TEST_CHECK(store.read().size() == 2U);
  });

  return tests.finish();
}
