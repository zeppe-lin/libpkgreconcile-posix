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
  test_support::runner tests;

  tests.run("publish pending tuples as one durable generation", [] {
    test_support::temp_directory temporary;
    inventory_generation_store store(
        temporary.path() / "reconcile", test_fixtures::make_target());
    const auto first = test_fixtures::make_pending("etc/first.conf", pkgreconcile::rejected_object_side::incoming, {1U});
    const auto second = test_fixtures::make_pending("etc/second.conf", pkgreconcile::rejected_object_side::prior_installed, {2U});
    const auto receipt = store.publish_pending({second, first});
    TEST_CHECK(receipt.published() == 2U);
    TEST_CHECK(receipt.already_pending() == 0U);
    TEST_CHECK(receipt.suppressed_resolved() == 0U);
    TEST_CHECK(receipt.changed());
    const auto inventory = store.read();
    TEST_CHECK(inventory.size() == 2U);
    TEST_CHECK(inventory.find(first)->status() == reconciliation_record_status::pending);
    TEST_CHECK(inventory.find(second)->status() == reconciliation_record_status::pending);
    TEST_CHECK(test_support::generation_count(temporary.path() / "reconcile") == 2U);
  });

  tests.run("round-trip opaque binary references and path bytes", [] {
    test_support::temp_directory temporary;
    const auto target = pkgreconcile::reconciliation_target_reference::make(
        "test.target/v1", {0U, 255U, 42U});
    inventory_generation_store store(temporary.path() / "reconcile", target);
    const auto object = pkgreconcile::rejected_object_locator::make(
        "test.object/v1", {0U, 1U, 255U});
    const auto value = pkgreconcile::pending_reconciliation::make(
        target,
        std::filesystem::path("etc/name with space\nand newline.conf"),
        pkgreconcile::rejected_object_side::incoming,
        object);
    TEST_CHECK(store.publish_pending({value}).changed());
    auto reopened = inventory_generation_store::open_existing(
        temporary.path() / "reconcile", target);
    const auto inventory = reopened.read();
    TEST_CHECK(inventory.target() == target);
    TEST_CHECK(inventory.find(value) != nullptr);
    TEST_CHECK(inventory.find(value)->value().object() == object);
  });

  tests.run("republishing pending tuple is a no-op", [] {
    test_support::temp_directory temporary;
    const auto root = temporary.path() / "reconcile";
    inventory_generation_store store(root, test_fixtures::make_target());
    const auto value = test_fixtures::make_pending();
    TEST_CHECK(store.publish_pending({value}).changed());
    const auto before = test_support::selector_text(root);
    const auto count = test_support::generation_count(root);
    const auto receipt = store.publish_pending({value});
    TEST_CHECK(receipt.published() == 0U);
    TEST_CHECK(receipt.already_pending() == 1U);
    TEST_CHECK(receipt.suppressed_resolved() == 0U);
    TEST_CHECK(!receipt.changed());
    TEST_CHECK(test_support::selector_text(root) == before);
    TEST_CHECK(test_support::generation_count(root) == count);
  });

  tests.run("reject duplicate request tuples", [] {
    test_support::temp_directory temporary;
    inventory_generation_store store(
        temporary.path() / "reconcile", test_fixtures::make_target());
    const auto value = test_fixtures::make_pending();
    TEST_CHECK_THROWS(static_cast<void>(store.publish_pending({value, value})),
                      std::invalid_argument);
    TEST_CHECK(store.read().size() == 0U);
  });

  tests.run("reject publication for another target", [] {
    test_support::temp_directory temporary;
    inventory_generation_store store(
        temporary.path() / "reconcile", test_fixtures::make_target());
    const auto foreign = test_fixtures::make_pending(
        "etc/tool.conf", pkgreconcile::rejected_object_side::incoming,
        {7U}, {99U});
    TEST_CHECK_THROWS(static_cast<void>(store.publish_pending({foreign})),
                      std::invalid_argument);
  });

  return tests.finish();
}
