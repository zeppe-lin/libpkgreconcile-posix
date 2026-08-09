// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file export.h
 * @brief Shared-library visibility contract for libpkgreconcile-posix.
 */

#ifndef LIBPKGRECONCILE_POSIX_EXPORT_H
#define LIBPKGRECONCILE_POSIX_EXPORT_H

#if defined(_WIN32) || defined(__CYGWIN__)
#define PKGRECONCILE_POSIX_API
#else
#define PKGRECONCILE_POSIX_API __attribute__((visibility("default")))
#endif

#endif
