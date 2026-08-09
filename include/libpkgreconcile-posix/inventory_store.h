// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @file inventory_store.h
 * @brief Durable POSIX reconciliation inventory provider.
 */

#ifndef LIBPKGRECONCILE_POSIX_INVENTORY_STORE_H
#define LIBPKGRECONCILE_POSIX_INVENTORY_STORE_H

#include <cstddef>
#include <filesystem>
#include <vector>

#include <libpkgreconcile/pending.h>
#include <libpkgreconcile-posix/export.h>

namespace pkgreconcile::posix {

/** Result of resolving one exact reconciliation tuple. */
enum class resolution_outcome {
  resolved,         /**< Pending state changed durably to resolved. */
  already_resolved, /**< The exact tuple was already resolved. */
  missing           /**< The exact tuple has never been recorded. */
};

/** Summary of one atomic pending-evidence publication. */
class pending_publication_receipt final {
public:
  /** Return the number of newly inserted pending tuples. */
  [[nodiscard]] PKGRECONCILE_POSIX_API std::size_t published() const noexcept;

  /** Return the number of tuples already present as pending. */
  [[nodiscard]] PKGRECONCILE_POSIX_API std::size_t already_pending() const noexcept;

  /** Return the number of tuples suppressed by resolved tombstones. */
  [[nodiscard]] PKGRECONCILE_POSIX_API std::size_t suppressed_resolved() const noexcept;

  /** Return whether a new durable generation was selected. */
  [[nodiscard]] PKGRECONCILE_POSIX_API bool changed() const noexcept;

private:
  friend class inventory_generation_store;

  pending_publication_receipt(std::size_t published,
                              std::size_t already_pending,
                              std::size_t suppressed_resolved) noexcept;

  std::size_t published_ = 0;
  std::size_t already_pending_ = 0;
  std::size_t suppressed_resolved_ = 0;
};

/**
 * Durable target-bound inventory using immutable generations and atomic
 * selection.
 *
 * The store owns read/modify/write serialization for one exact target. Pending
 * publication never reactivates an exact tuple that is already resolved.
 * Successful mutating returns occur only after the selected generation has
 * been synchronized through the store directory.
 */
class inventory_generation_store final {
public:
  /**
   * Open or initialize one store.
   *
   * Initialization creates and selects an empty inventory when necessary and
   * durably binds the store to @p target. Existing non-empty or mismatched
   * state is never guessed or repaired.
   *
   * @param root Complete pathname of the store directory.
   * @param target Exact reconciliation target binding owned by this store.
   * @throws store_error for path, locking, layout, corruption, or durability
   *         failures.
   */
  PKGRECONCILE_POSIX_API
  inventory_generation_store(std::filesystem::path root,
                             reconciliation_target_reference target);

  /** Destroy the provider-owned store handle. */
  PKGRECONCILE_POSIX_API ~inventory_generation_store();

  /**
   * Open and validate an existing store without initializing it.
   *
   * @throws store_error when the store is absent, incomplete, corrupt, busy,
   *         or bound to another target.
   */
  [[nodiscard]] static PKGRECONCILE_POSIX_API inventory_generation_store
  open_existing(std::filesystem::path root,
                reconciliation_target_reference target);

  /** Store handles are not copy-constructible. */
  inventory_generation_store(const inventory_generation_store&) = delete;

  /** Store handles are not copy-assignable. */
  inventory_generation_store&
  operator=(const inventory_generation_store&) = delete;

  /** Return the configured pathname as diagnostic metadata. */
  [[nodiscard]] PKGRECONCILE_POSIX_API const std::filesystem::path&
  root_path() const noexcept;

  /** Return the exact durable target binding required by this handle. */
  [[nodiscard]] PKGRECONCILE_POSIX_API
  const reconciliation_target_reference& target_binding() const noexcept;

  /** Read and validate the currently selected complete inventory. */
  [[nodiscard]] PKGRECONCILE_POSIX_API reconciliation_inventory read() const;

  /**
   * Atomically publish newly observed pending tuples.
   *
   * Existing pending tuples remain pending. Existing resolved tuples remain
   * resolved and are reported as suppressed; publication cannot resurrect
   * them. Duplicate tuples inside @p values are rejected.
   *
   * @throws std::invalid_argument for duplicate request tuples or a tuple bound
   *         to another target.
   * @throws store_error for locking, corruption, or durability failures.
   */
  [[nodiscard]] PKGRECONCILE_POSIX_API pending_publication_receipt
  publish_pending(std::vector<pending_reconciliation> values);

  /**
   * Durably resolve one exact pending tuple.
   *
   * Resolving an already-resolved tuple is idempotent. An absent tuple is not
   * invented and returns resolution_outcome::missing.
   *
   * @throws std::invalid_argument when @p value is bound to another target.
   * @throws store_error for locking, corruption, or durability failures.
   */
  [[nodiscard]] PKGRECONCILE_POSIX_API resolution_outcome resolve(const pending_reconciliation& value);

private:
  struct existing_store_tag final {};

  inventory_generation_store(std::filesystem::path root,
                             reconciliation_target_reference target,
                             existing_store_tag);

  void initialize();
  void validate_existing();

  std::filesystem::path root_;
  reconciliation_target_reference target_;
  int root_descriptor_ = -1;
};

} // namespace pkgreconcile::posix

#endif
