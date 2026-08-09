// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgreconcile-posix/error.h>

namespace pkgreconcile::posix {

store_error::store_error(const std::string& message)
    : std::runtime_error(message)
{
}

store_error::store_error(const char* message)
    : std::runtime_error(message)
{
}

store_error::~store_error() = default;

} // namespace pkgreconcile::posix
