// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgreconcile-posix/error.h>
#include <libpkgreconcile-posix/inventory_store.h>

#include "support/core_fixtures.hpp"
#include "support/store_layout.hpp"
#include "support/temp_directory.hpp"
#include "support/test.hpp"

#include <filesystem>

#include <sys/stat.h>

int main()
{
  using pkgreconcile::posix::inventory_generation_store;
  using pkgreconcile::posix::store_error;
  test_support::runner tests;

  tests.run("reject corrupt selector", [] {
    test_support::temp_directory temporary;
    const auto target = test_fixtures::make_target();
    const auto root = temporary.path() / "reconcile";
    { inventory_generation_store store(root, target); }
    test_support::rewrite_immutable(root / "current", "garbage\n");
    TEST_CHECK_THROWS(
        static_cast<void>(inventory_generation_store::open_existing(root, target)),
        store_error);
  });

  tests.run("reject corrupt target binding", [] {
    test_support::temp_directory temporary;
    const auto target = test_fixtures::make_target();
    const auto root = temporary.path() / "reconcile";
    { inventory_generation_store store(root, target); }
    test_support::rewrite_immutable(root / "binding", "garbage");
    TEST_CHECK_THROWS(
        static_cast<void>(inventory_generation_store::open_existing(root, target)),
        store_error);
  });

  tests.run("reject generation bytes that do not match selector digest", [] {
    test_support::temp_directory temporary;
    const auto target = test_fixtures::make_target();
    const auto root = temporary.path() / "reconcile";
    {
      inventory_generation_store store(root, target);
      TEST_CHECK(store.publish_pending({test_fixtures::make_pending()}).changed());
    }
    const auto inventory = test_support::selected_generation(root) / "inventory";
    auto bytes = test_support::read_bytes(inventory);
    TEST_CHECK(!bytes.empty());
    bytes.back() ^= 0x01U;
    test_support::rewrite_immutable(inventory, bytes);
    TEST_CHECK_THROWS(
        static_cast<void>(inventory_generation_store::open_existing(root, target)),
        store_error);
  });

  tests.run("reject semantically invalid generation with matching digest", [] {
    test_support::temp_directory temporary;
    const auto target = test_fixtures::make_target();
    const auto root = temporary.path() / "reconcile";
    {
      inventory_generation_store store(root, target);
      TEST_CHECK(store.publish_pending({test_fixtures::make_pending()}).changed());
    }
    auto bytes = test_support::read_bytes(
        test_support::selected_generation(root) / "inventory");

    std::size_t offset = 8U;
    auto read_u32 = [&](std::size_t at) {
      return (static_cast<std::uint32_t>(bytes.at(at)) << 24U) |
          (static_cast<std::uint32_t>(bytes.at(at + 1U)) << 16U) |
          (static_cast<std::uint32_t>(bytes.at(at + 2U)) << 8U) |
          static_cast<std::uint32_t>(bytes.at(at + 3U));
    };
    const std::uint32_t target_provider_size = read_u32(offset);
    offset += 4U + target_provider_size;
    const std::uint32_t target_bytes_size = read_u32(offset);
    offset += 4U + target_bytes_size;
    offset += 8U;
    TEST_CHECK(offset < bytes.size());
    bytes[offset] = 99U;

    test_support::select_inventory_bytes(root, bytes);
    TEST_CHECK_THROWS(
        static_cast<void>(inventory_generation_store::open_existing(root, target)),
        store_error);
  });

  tests.run("reject writable canonical metadata", [] {
    test_support::temp_directory temporary;
    const auto target = test_fixtures::make_target();
    const auto root = temporary.path() / "reconcile";
    { inventory_generation_store store(root, target); }
    TEST_CHECK(::chmod((root / "current").c_str(), 0644) == 0);
    TEST_CHECK_THROWS(
        static_cast<void>(inventory_generation_store::open_existing(root, target)),
        store_error);
  });

  return tests.finish();
}
