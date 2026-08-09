// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file error.h
 * @brief POSIX reconciliation-store failures.
 */

#ifndef LIBPKGRECONCILE_POSIX_ERROR_H
#define LIBPKGRECONCILE_POSIX_ERROR_H

#include <stdexcept>
#include <string>

#include <libpkgreconcile-posix/export.h>

namespace pkgreconcile::posix {

/** Failure to open, validate, lock, read, or durably update a POSIX store. */
class PKGRECONCILE_POSIX_API store_error : public std::runtime_error {
public:
  /** Construct a store failure with diagnostic text. */
  explicit store_error(const std::string& message);

  /** Construct a store failure with diagnostic text. */
  explicit store_error(const char* message);

  /** Destroy the failure value. */
  ~store_error() override;
};

} // namespace pkgreconcile::posix

#endif
