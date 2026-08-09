// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgreconcile-posix/error.h>
#include <libpkgreconcile-posix/inventory_store.h>

#include "support/core_fixtures.hpp"
#include "support/temp_directory.hpp"
#include "support/test.hpp"

#include <cstdlib>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>

int main()
{
  using pkgreconcile::posix::inventory_generation_store;
  using pkgreconcile::posix::store_error;
  test_support::runner tests;

  tests.run("competing process lock refuses read and mutation", [] {
    test_support::temp_directory temporary;
    const auto root = temporary.path() / "reconcile";
    inventory_generation_store store(root, test_fixtures::make_target());

    int ready[2] = {-1, -1};
    int release[2] = {-1, -1};
    TEST_CHECK(::pipe(ready) == 0);
    TEST_CHECK(::pipe(release) == 0);
    const pid_t child = ::fork();
    TEST_CHECK(child != -1);
    if (child == 0)
    {
      static_cast<void>(::close(ready[0]));
      static_cast<void>(::close(release[1]));
      const int descriptor =
          ::open(root.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
      if (descriptor == -1 || ::flock(descriptor, LOCK_EX) == -1)
        std::_Exit(10);
      const char signal = 'R';
      if (::write(ready[1], &signal, 1) != 1)
        std::_Exit(11);
      char ignored = 0;
      if (::read(release[0], &ignored, 1) != 1)
        std::_Exit(12);
      static_cast<void>(::flock(descriptor, LOCK_UN));
      static_cast<void>(::close(descriptor));
      std::_Exit(0);
    }

    static_cast<void>(::close(ready[1]));
    static_cast<void>(::close(release[0]));
    char signal = 0;
    TEST_CHECK(::read(ready[0], &signal, 1) == 1);
    TEST_CHECK(signal == 'R');

    TEST_CHECK_THROWS(static_cast<void>(store.read()), store_error);
    TEST_CHECK_THROWS(
        static_cast<void>(store.publish_pending({test_fixtures::make_pending()})),
        store_error);
    TEST_CHECK_THROWS(
        static_cast<void>(store.resolve(test_fixtures::make_pending())),
        store_error);

    const char release_signal = 'X';
    TEST_CHECK(::write(release[1], &release_signal, 1) == 1);
    static_cast<void>(::close(ready[0]));
    static_cast<void>(::close(release[1]));
    int status = 0;
    TEST_CHECK(::waitpid(child, &status, 0) == child);
    TEST_CHECK(WIFEXITED(status));
    TEST_CHECK(WEXITSTATUS(status) == 0);
    TEST_CHECK(store.read().size() == 0U);
  });

  return tests.finish();
}
