// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgreconcile-posix/libpkgreconcile-posix.h>

#include <type_traits>

static_assert(std::is_base_of_v<std::runtime_error,
                                pkgreconcile::posix::store_error>);
static_assert(!std::is_copy_constructible_v<
              pkgreconcile::posix::inventory_generation_store>);

int main()
{
  return 0;
}
