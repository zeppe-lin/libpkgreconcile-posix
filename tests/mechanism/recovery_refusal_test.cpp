// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgreconcile-posix/error.h>
#include <libpkgreconcile-posix/inventory_store.h>

#include "support/core_fixtures.hpp"
#include "support/store_layout.hpp"
#include "support/temp_directory.hpp"
#include "support/test.hpp"

#include <filesystem>
#include <fstream>

#include <sys/stat.h>
#include <unistd.h>

namespace {

void write_text(const std::filesystem::path& path, const char* text)
{
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << text;
}

} // namespace

int main()
{
  using pkgreconcile::posix::inventory_generation_store;
  using pkgreconcile::posix::store_error;
  test_support::runner tests;

  tests.run("resume binding publication only for selected empty inventory", [] {
    test_support::temp_directory temporary;
    const auto target = test_fixtures::make_target();
    const auto root = temporary.path() / "reconcile";
    { inventory_generation_store store(root, target); }
    TEST_CHECK(::unlink((root / "binding").c_str()) == 0);
    inventory_generation_store resumed(root, target);
    TEST_CHECK(resumed.read().size() == 0U);
    TEST_CHECK(std::filesystem::is_regular_file(root / "binding"));
  });

  tests.run("unbound empty selection cannot be rebound to another target", [] {
    test_support::temp_directory temporary;
    const auto target = test_fixtures::make_target();
    const auto root = temporary.path() / "reconcile";
    { inventory_generation_store store(root, target); }
    TEST_CHECK(::unlink((root / "binding").c_str()) == 0);
    TEST_CHECK_THROWS(
        static_cast<void>(inventory_generation_store(
            root, test_fixtures::make_target({99U}))),
        store_error);
    TEST_CHECK(!std::filesystem::exists(root / "binding"));
  });

  tests.run("refuse missing binding after nonempty publication", [] {
    test_support::temp_directory temporary;
    const auto target = test_fixtures::make_target();
    const auto root = temporary.path() / "reconcile";
    {
      inventory_generation_store store(root, target);
      TEST_CHECK(store.publish_pending({test_fixtures::make_pending()}).changed());
    }
    TEST_CHECK(::unlink((root / "binding").c_str()) == 0);
    TEST_CHECK_THROWS(
        static_cast<void>(inventory_generation_store(root, target)), store_error);
    TEST_CHECK(!std::filesystem::exists(root / "binding"));
  });

  tests.run("refuse missing selector", [] {
    test_support::temp_directory temporary;
    const auto target = test_fixtures::make_target();
    const auto root = temporary.path() / "reconcile";
    { inventory_generation_store store(root, target); }
    std::filesystem::rename(root / "current", root / "current.lost");
    TEST_CHECK_THROWS(
        static_cast<void>(inventory_generation_store(root, target)), store_error);
    TEST_CHECK(!std::filesystem::exists(root / "current"));
  });

  tests.run("refuse missing selected generation", [] {
    test_support::temp_directory temporary;
    const auto target = test_fixtures::make_target();
    const auto root = temporary.path() / "reconcile";
    std::filesystem::path generation;
    {
      inventory_generation_store store(root, target);
      generation = test_support::selected_generation(root);
    }
    std::filesystem::rename(generation, generation.string() + ".lost");
    TEST_CHECK_THROWS(
        static_cast<void>(inventory_generation_store(root, target)), store_error);
  });

  tests.run("ignore incomplete temporary entries", [] {
    test_support::temp_directory temporary;
    const auto target = test_fixtures::make_target();
    const auto root = temporary.path() / "reconcile";
    inventory_generation_store store(root, target);
    const auto selected_before = test_support::selector_text(root);
    write_text(root / "current.tmp.crash", "garbage\n");
    std::filesystem::create_directory(root / "generations" / "generation.tmp.crash");
    write_text(root / "generations" / "generation.tmp.crash" / "inventory",
               "incomplete");
    TEST_CHECK(store.read().size() == 0U);
    auto reopened = inventory_generation_store::open_existing(root, target);
    TEST_CHECK(reopened.read().size() == 0U);
    TEST_CHECK(test_support::selector_text(root) == selected_before);
    TEST_CHECK(std::filesystem::exists(root / "current.tmp.crash"));
    TEST_CHECK(std::filesystem::exists(
        root / "generations" / "generation.tmp.crash"));
  });

  return tests.finish();
}
