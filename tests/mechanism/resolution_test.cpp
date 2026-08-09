// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgreconcile-posix/inventory_store.h>

#include "support/core_fixtures.hpp"
#include "support/store_layout.hpp"
#include "support/temp_directory.hpp"
#include "support/test.hpp"

#include <stdexcept>

int main()
{
  using pkgreconcile::reconciliation_record_status;
  using pkgreconcile::posix::inventory_generation_store;
  using pkgreconcile::posix::resolution_outcome;
  test_support::runner tests;

  tests.run("resolve exact pending tuple durably", [] {
    test_support::temp_directory temporary;
    const auto root = temporary.path() / "reconcile";
    inventory_generation_store store(root, test_fixtures::make_target());
    const auto value = test_fixtures::make_pending();
    TEST_CHECK(store.resolve(value) == resolution_outcome::missing);
    TEST_CHECK(store.publish_pending({value}).changed());
    TEST_CHECK(store.resolve(value) == resolution_outcome::resolved);
    const auto inventory = store.read();
    const auto* record = inventory.find(value);
    TEST_CHECK(record != nullptr);
    TEST_CHECK(record->status() == reconciliation_record_status::resolved);
    TEST_CHECK(store.resolve(value) == resolution_outcome::already_resolved);
    TEST_CHECK(test_support::generation_count(root) == 3U);
  });

  tests.run("missing and idempotent resolutions do not republish", [] {
    test_support::temp_directory temporary;
    const auto root = temporary.path() / "reconcile";
    inventory_generation_store store(root, test_fixtures::make_target());
    const auto value = test_fixtures::make_pending();
    const auto before = test_support::selector_text(root);
    TEST_CHECK(store.resolve(value) == resolution_outcome::missing);
    TEST_CHECK(test_support::selector_text(root) == before);
    TEST_CHECK(store.publish_pending({value}).changed());
    TEST_CHECK(store.resolve(value) == resolution_outcome::resolved);
    const auto resolved = test_support::selector_text(root);
    TEST_CHECK(store.resolve(value) == resolution_outcome::already_resolved);
    TEST_CHECK(test_support::selector_text(root) == resolved);
  });

  tests.run("reject resolution for another target", [] {
    test_support::temp_directory temporary;
    inventory_generation_store store(
        temporary.path() / "reconcile", test_fixtures::make_target());
    const auto foreign = test_fixtures::make_pending(
        "etc/tool.conf", pkgreconcile::rejected_object_side::incoming,
        {7U}, {99U});
    TEST_CHECK_THROWS(static_cast<void>(store.resolve(foreign)),
                      std::invalid_argument);
  });

  return tests.finish();
}
