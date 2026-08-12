// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#include <libpkgreconcile-posix/inventory_store.h>

#include <libpkgreconcile-posix/error.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <linux/fs.h>
#include <openssl/evp.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace pkgreconcile::posix {
namespace {

constexpr const char* binding_file = "binding";
constexpr const char* current_file = "current";
constexpr const char* generations_directory = "generations";
constexpr const char* inventory_file = "inventory";
constexpr std::uint64_t maximum_store_file_size = 1024ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t maximum_record_count = 16ULL * 1024ULL * 1024ULL;
constexpr std::array<std::uint8_t, 8> binding_magic = {
    'Z', 'L', 'R', 'B', 'N', 'D', '0', '1'};
constexpr std::array<std::uint8_t, 8> inventory_magic = {
    'Z', 'L', 'R', 'I', 'N', 'V', '0', '1'};

class unique_fd final {
public:
  unique_fd() = default;
  explicit unique_fd(int value) noexcept
      : value_(value)
  {
  }

  unique_fd(const unique_fd&) = delete;
  unique_fd& operator=(const unique_fd&) = delete;

  unique_fd(unique_fd&& other) noexcept
      : value_(std::exchange(other.value_, -1))
  {
  }

  unique_fd& operator=(unique_fd&& other) noexcept
  {
    if (this != &other)
    {
      reset();
      value_ = std::exchange(other.value_, -1);
    }
    return *this;
  }

  ~unique_fd()
  {
    reset();
  }

  [[nodiscard]] int get() const noexcept
  {
    return value_;
  }

  [[nodiscard]] int release() noexcept
  {
    return std::exchange(value_, -1);
  }

  void reset(int value = -1) noexcept
  {
    if (value_ != -1)
      static_cast<void>(::close(value_));
    value_ = value;
  }

private:
  int value_ = -1;
};

[[nodiscard]] std::string
system_failure(std::string_view operation, int error_number)
{
  return std::string(operation) + ": " + std::strerror(error_number);
}

[[noreturn]] void
throw_store_failure(std::string_view operation)
{
  const int error_number = errno;
  throw store_error(system_failure(operation, error_number));
}

void
validate_store_path(const std::filesystem::path& path)
{
  if (path.empty())
    throw store_error("reconciliation generation store path is empty");
  if (path.native().find('\0') != std::string::npos)
    throw store_error("reconciliation generation store path contains a NUL byte");
}

[[nodiscard]] unique_fd
open_directory(const std::filesystem::path& path)
{
  const int descriptor = ::open(
      path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  if (descriptor == -1)
    throw_store_failure("open reconciliation store directory");
  return unique_fd(descriptor);
}

[[nodiscard]] unique_fd
open_directory_at(int parent, const char* name, std::string_view label)
{
  const int descriptor = ::openat(
      parent, name, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  if (descriptor == -1)
    throw_store_failure(label);
  return unique_fd(descriptor);
}

[[nodiscard]] unique_fd
reopen_directory_authority(int directory, std::string_view label)
{
  if (directory == -1)
    throw store_error(std::string(label) + ": store authority is closed");
  return open_directory_at(directory, ".", label);
}

void
lock_directory(int descriptor, int operation, std::string_view label)
{
  for (;;)
  {
    if (::flock(descriptor, operation | LOCK_NB) == 0)
      return;
    if (errno != EINTR)
      throw_store_failure(label);
  }
}

[[nodiscard]] bool
synchronize(int descriptor) noexcept
{
  for (;;)
  {
    if (::fsync(descriptor) == 0)
      return true;
    if (errno != EINTR)
      return false;
  }
}

void
synchronize_directory(int descriptor, std::string_view label)
{
  if (!synchronize(descriptor))
    throw_store_failure(label);
}

void
ensure_directory_at(int parent, const char* name)
{
  if (::mkdirat(parent, name, 0755) == -1 && errno != EEXIST)
    throw_store_failure("create reconciliation generations directory");

  unique_fd directory = open_directory_at(
      parent, name, "open reconciliation generations directory");
  struct stat status {};
  if (::fstat(directory.get(), &status) == -1)
    throw_store_failure("inspect reconciliation generations directory");
  if (!S_ISDIR(status.st_mode))
    throw store_error("reconciliation generations path is not a directory");
}

[[nodiscard]] std::optional<std::vector<std::uint8_t>>
read_file_at(int directory,
             const char* name,
             std::uint64_t maximum_size,
             bool optional,
             std::string_view label)
{
  const int opened = ::openat(
      directory, name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
  if (opened == -1)
  {
    if (optional && errno == ENOENT)
      return std::nullopt;
    throw_store_failure(label);
  }
  unique_fd file(opened);

  struct stat status {};
  if (::fstat(file.get(), &status) == -1)
    throw_store_failure(label);
  if (!S_ISREG(status.st_mode))
    throw store_error(std::string(label) + " is not a regular file");
  if (status.st_nlink != 1)
    throw store_error(std::string(label) + " has multiple hard links");
  if ((status.st_mode & 0222) != 0)
    throw store_error(std::string(label) + " is not immutable");
  if (status.st_size < 0 ||
      static_cast<std::uint64_t>(status.st_size) > maximum_size)
  {
    throw store_error(std::string(label) + " exceeds the supported size");
  }
  if (static_cast<std::uint64_t>(status.st_size) >
      static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
  {
    throw store_error(std::string(label) + " size is not representable");
  }

  std::vector<std::uint8_t> result(static_cast<std::size_t>(status.st_size));
  std::size_t offset = 0;
  while (offset < result.size())
  {
    const ssize_t count =
        ::read(file.get(), result.data() + offset, result.size() - offset);
    if (count == -1)
    {
      if (errno == EINTR)
        continue;
      throw_store_failure(label);
    }
    if (count == 0)
      throw store_error(std::string(label) + " was truncated while reading");
    offset += static_cast<std::size_t>(count);
  }

  std::uint8_t extra = 0;
  for (;;)
  {
    const ssize_t count = ::read(file.get(), &extra, 1);
    if (count == -1 && errno == EINTR)
      continue;
    if (count == -1)
      throw_store_failure(label);
    if (count != 0)
      throw store_error(std::string(label) + " changed while reading");
    break;
  }
  return result;
}

void
write_all(int descriptor, const std::vector<std::uint8_t>& bytes,
          std::string_view label)
{
  std::size_t offset = 0;
  while (offset < bytes.size())
  {
    const ssize_t count =
        ::write(descriptor, bytes.data() + offset, bytes.size() - offset);
    if (count == -1)
    {
      if (errno == EINTR)
        continue;
      throw_store_failure(label);
    }
    if (count == 0)
      throw store_error(std::string(label) + ": zero-length write");
    offset += static_cast<std::size_t>(count);
  }
}

[[nodiscard]] std::string
unique_temporary_name(std::string_view prefix)
{
  static std::atomic<std::uint64_t> sequence{0};
  return std::string(prefix) + ".tmp." + std::to_string(::getpid()) + "." +
      std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
}

void
write_atomic_metadata(int directory,
                      const char* final_name,
                      const std::vector<std::uint8_t>& bytes)
{
  const std::string temporary = unique_temporary_name(final_name);
  const int opened = ::openat(directory,
                              temporary.c_str(),
                              O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC |
                                  O_NOFOLLOW,
                              0600);
  if (opened == -1)
    throw_store_failure("create reconciliation metadata temporary");
  unique_fd file(opened);

  try
  {
    write_all(file.get(), bytes, "write reconciliation metadata");
    if (::fchmod(file.get(), 0444) == -1)
      throw_store_failure("set reconciliation metadata mode");
    if (!synchronize(file.get()))
      throw_store_failure("synchronize reconciliation metadata");
    file.reset();
    if (::renameat(directory,
                   temporary.c_str(),
                   directory,
                   final_name) == -1)
    {
      throw_store_failure("publish reconciliation metadata");
    }
    synchronize_directory(directory, "synchronize reconciliation store");
  }
  catch (...)
  {
    file.reset();
    static_cast<void>(::unlinkat(directory, temporary.c_str(), 0));
    throw;
  }
}

class byte_writer final {
public:
  void put_u8(std::uint8_t value)
  {
    bytes_.push_back(value);
  }

  void put_u32(std::uint32_t value)
  {
    for (int shift = 24; shift >= 0; shift -= 8)
      bytes_.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffU));
  }

  void put_u64(std::uint64_t value)
  {
    for (int shift = 56; shift >= 0; shift -= 8)
      bytes_.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffU));
  }

  void put_raw(const std::uint8_t* begin, std::size_t size)
  {
    bytes_.insert(bytes_.end(), begin, begin + size);
  }

  template <std::size_t Size>
  void put_magic(const std::array<std::uint8_t, Size>& magic)
  {
    put_raw(magic.data(), magic.size());
  }

  void put_string(const std::string& value)
  {
    require_u32_size(value.size(), "encoded string");
    put_u32(static_cast<std::uint32_t>(value.size()));
    put_raw(reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
  }

  void put_bytes(const std::vector<std::uint8_t>& value)
  {
    require_u32_size(value.size(), "encoded byte string");
    put_u32(static_cast<std::uint32_t>(value.size()));
    put_raw(value.data(), value.size());
  }

  [[nodiscard]] std::vector<std::uint8_t> finish() &&
  {
    return std::move(bytes_);
  }

private:
  static void require_u32_size(std::size_t size, const char* what)
  {
    if (size > std::numeric_limits<std::uint32_t>::max())
      throw store_error(std::string(what) + " exceeds format limit");
  }

  std::vector<std::uint8_t> bytes_;
};

class byte_reader final {
public:
  explicit byte_reader(const std::vector<std::uint8_t>& bytes)
      : bytes_(bytes)
  {
  }

  template <std::size_t Size>
  void require_magic(const std::array<std::uint8_t, Size>& expected,
                     const char* what)
  {
    require(Size, what);
    if (!std::equal(expected.begin(), expected.end(), bytes_.begin() + offset_))
      throw store_error(std::string(what) + " magic is invalid");
    offset_ += Size;
  }

  [[nodiscard]] std::uint8_t get_u8(const char* what)
  {
    require(1, what);
    return bytes_[offset_++];
  }

  [[nodiscard]] std::uint32_t get_u32(const char* what)
  {
    require(4, what);
    std::uint32_t result = 0;
    for (int index = 0; index < 4; ++index)
      result = (result << 8U) | bytes_[offset_++];
    return result;
  }

  [[nodiscard]] std::uint64_t get_u64(const char* what)
  {
    require(8, what);
    std::uint64_t result = 0;
    for (int index = 0; index < 8; ++index)
      result = (result << 8U) | bytes_[offset_++];
    return result;
  }

  [[nodiscard]] std::string get_string(const char* what)
  {
    const std::uint32_t size = get_u32(what);
    require(size, what);
    std::string result(reinterpret_cast<const char*>(bytes_.data() + offset_),
                       size);
    offset_ += size;
    return result;
  }

  [[nodiscard]] std::vector<std::uint8_t> get_bytes(const char* what)
  {
    const std::uint32_t size = get_u32(what);
    require(size, what);
    std::vector<std::uint8_t> result(bytes_.begin() + offset_,
                                     bytes_.begin() + offset_ + size);
    offset_ += size;
    return result;
  }

  [[nodiscard]] std::size_t remaining() const noexcept
  {
    return bytes_.size() - offset_;
  }

  void require_end(const char* what) const
  {
    if (offset_ != bytes_.size())
      throw store_error(std::string(what) + " contains trailing bytes");
  }

private:
  void require(std::size_t size, const char* what) const
  {
    if (size > bytes_.size() - offset_)
      throw store_error(std::string(what) + " is truncated");
  }

  const std::vector<std::uint8_t>& bytes_;
  std::size_t offset_ = 0;
};

void
encode_target(byte_writer& writer, const reconciliation_target_reference& target)
{
  writer.put_string(target.provider());
  writer.put_bytes(target.bytes());
}

[[nodiscard]] reconciliation_target_reference
decode_target(byte_reader& reader, const char* what)
{
  const std::string provider = reader.get_string(what);
  const std::vector<std::uint8_t> bytes = reader.get_bytes(what);
  try
  {
    return reconciliation_target_reference::make(provider, bytes);
  }
  catch (const std::invalid_argument& failure)
  {
    throw store_error(std::string(what) + ": " + failure.what());
  }
}

[[nodiscard]] std::vector<std::uint8_t>
encode_binding(const reconciliation_target_reference& target)
{
  byte_writer writer;
  writer.put_magic(binding_magic);
  encode_target(writer, target);
  return std::move(writer).finish();
}

[[nodiscard]] reconciliation_target_reference
decode_binding(const std::vector<std::uint8_t>& bytes)
{
  byte_reader reader(bytes);
  reader.require_magic(binding_magic, "reconciliation binding");
  reconciliation_target_reference target =
      decode_target(reader, "reconciliation binding");
  reader.require_end("reconciliation binding");
  return target;
}

[[nodiscard]] std::vector<std::uint8_t>
encode_inventory(const reconciliation_inventory& inventory)
{
  byte_writer writer;
  writer.put_magic(inventory_magic);
  encode_target(writer, inventory.target());
  writer.put_u64(static_cast<std::uint64_t>(inventory.records().size()));
  if (inventory.records().size() > maximum_record_count)
    throw store_error("reconciliation record count exceeds format limit");
  for (const reconciliation_record& record : inventory.records())
  {
    writer.put_u8(static_cast<std::uint8_t>(record.status()));
    writer.put_u8(static_cast<std::uint8_t>(record.value().side()));
    writer.put_string(record.value().path().native());
    writer.put_string(record.value().object().provider());
    writer.put_bytes(record.value().object().bytes());
  }
  std::vector<std::uint8_t> encoded = std::move(writer).finish();
  if (static_cast<std::uint64_t>(encoded.size()) > maximum_store_file_size)
    throw store_error("reconciliation inventory exceeds supported size");
  return encoded;
}

[[nodiscard]] reconciliation_inventory
decode_inventory(const std::vector<std::uint8_t>& bytes)
{
  byte_reader reader(bytes);
  reader.require_magic(inventory_magic, "reconciliation inventory");
  reconciliation_target_reference target =
      decode_target(reader, "reconciliation inventory target");
  const std::uint64_t count = reader.get_u64("reconciliation record count");
  constexpr std::uint64_t minimum_encoded_record_size = 14U;
  if (count > maximum_record_count ||
      count > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) ||
      count > static_cast<std::uint64_t>(reader.remaining()) /
          minimum_encoded_record_size)
  {
    throw store_error("reconciliation record count exceeds encoded bounds");
  }

  std::vector<reconciliation_record> records;
  records.reserve(static_cast<std::size_t>(count));
  for (std::uint64_t index = 0; index < count; ++index)
  {
    const auto status = static_cast<reconciliation_record_status>(
        reader.get_u8("reconciliation record status"));
    const auto side = static_cast<rejected_object_side>(
        reader.get_u8("reconciliation rejected-object side"));
    const std::string path = reader.get_string("reconciliation path");
    const std::string provider =
        reader.get_string("reconciliation object provider");
    const std::vector<std::uint8_t> locator =
        reader.get_bytes("reconciliation object locator");

    try
    {
      rejected_object_locator object =
          rejected_object_locator::make(provider, locator);
      pending_reconciliation value = pending_reconciliation::make(
          target, std::filesystem::path(path), side, std::move(object));
      switch (status)
      {
        case reconciliation_record_status::pending:
          records.push_back(reconciliation_record::pending(std::move(value)));
          break;
        case reconciliation_record_status::resolved:
          records.push_back(reconciliation_record::resolved(std::move(value)));
          break;
        default:
          throw store_error("reconciliation record status is invalid");
      }
    }
    catch (const std::invalid_argument& failure)
    {
      throw store_error(std::string("reconciliation inventory value: ") +
                        failure.what());
    }
  }
  reader.require_end("reconciliation inventory");

  try
  {
    reconciliation_inventory result =
        reconciliation_inventory::make(target, std::move(records));
    if (encode_inventory(result) != bytes)
      throw store_error("reconciliation inventory encoding is noncanonical");
    return result;
  }
  catch (const std::invalid_argument& failure)
  {
    throw store_error(std::string("reconciliation inventory: ") +
                      failure.what());
  }
}

[[nodiscard]] std::array<std::uint8_t, 32>
sha256_bytes(const std::vector<std::uint8_t>& bytes)
{
  using context_ptr =
      std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
  context_ptr context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
  if (!context)
    throw store_error("allocate reconciliation digest context");
  if (EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1 ||
      (!bytes.empty() &&
       EVP_DigestUpdate(context.get(), bytes.data(), bytes.size()) != 1))
  {
    throw store_error("compute reconciliation generation digest");
  }
  std::array<std::uint8_t, 32> result {};
  unsigned int size = 0;
  if (EVP_DigestFinal_ex(context.get(), result.data(), &size) != 1 ||
      size != result.size())
  {
    throw store_error("finalize reconciliation generation digest");
  }
  return result;
}

[[nodiscard]] std::string
hex_digest(const std::array<std::uint8_t, 32>& digest)
{
  static constexpr char digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(digest.size() * 2U);
  for (const std::uint8_t byte : digest)
  {
    result.push_back(digits[(byte >> 4U) & 0x0fU]);
    result.push_back(digits[byte & 0x0fU]);
  }
  return result;
}

[[nodiscard]] std::string
generation_identity(const std::vector<std::uint8_t>& bytes)
{
  return "v1:sha256:" + hex_digest(sha256_bytes(bytes));
}

[[nodiscard]] bool
valid_hex_digest(std::string_view value)
{
  if (value.size() != 64)
    return false;
  return std::all_of(value.begin(), value.end(), [](char character) {
    return (character >= '0' && character <= '9') ||
        (character >= 'a' && character <= 'f');
  });
}

[[nodiscard]] std::string
generation_name(std::string_view identity)
{
  constexpr std::string_view prefix = "v1:sha256:";
  if (identity.size() != prefix.size() + 64 ||
      identity.substr(0, prefix.size()) != prefix ||
      !valid_hex_digest(identity.substr(prefix.size())))
  {
    throw store_error("reconciliation generation identity is invalid");
  }
  return "v1-sha256-" + std::string(identity.substr(prefix.size()));
}

[[nodiscard]] std::string
parse_selector(const std::vector<std::uint8_t>& bytes)
{
  if (bytes.empty() || bytes.back() != '\n')
    throw store_error("reconciliation current selector is not newline terminated");
  if (std::count(bytes.begin(), bytes.end(), static_cast<std::uint8_t>('\n')) !=
      1)
  {
    throw store_error("reconciliation current selector contains extra lines");
  }
  const std::string identity(reinterpret_cast<const char*>(bytes.data()),
                             bytes.size() - 1);
  static_cast<void>(generation_name(identity));
  return identity;
}

[[nodiscard]] std::optional<reconciliation_target_reference>
read_binding_locked(int root)
{
  const auto bytes = read_file_at(root,
                                  binding_file,
                                  maximum_store_file_size,
                                  true,
                                  "reconciliation store binding");
  if (!bytes)
    return std::nullopt;
  return decode_binding(*bytes);
}

void
require_binding_locked(int root,
                       const reconciliation_target_reference& expected)
{
  const auto binding = read_binding_locked(root);
  if (!binding)
    throw store_error("reconciliation store binding is missing");
  if (*binding != expected)
    throw store_error("reconciliation store target binding does not match caller");
}

[[nodiscard]] reconciliation_inventory
read_generation_at(int generations,
                   std::string_view identity,
                   const reconciliation_target_reference& expected_target)
{
  const std::string name = generation_name(identity);
  unique_fd generation = open_directory_at(
      generations, name.c_str(), "open selected reconciliation generation");
  struct stat generation_status {};
  if (::fstat(generation.get(), &generation_status) == -1)
    throw_store_failure("inspect selected reconciliation generation");
  if ((generation_status.st_mode & 0222) != 0)
    throw store_error("selected reconciliation generation is not immutable");

  const auto bytes = read_file_at(generation.get(),
                                  inventory_file,
                                  maximum_store_file_size,
                                  false,
                                  "selected reconciliation inventory");
  if (generation_identity(*bytes) != identity)
    throw store_error("selected reconciliation generation digest does not match selector");
  reconciliation_inventory result = decode_inventory(*bytes);
  if (result.target() != expected_target)
    throw store_error("selected reconciliation generation target is inconsistent");
  return result;
}

[[nodiscard]] reconciliation_inventory
read_selected_inventory_locked(int root,
                               const reconciliation_target_reference& target)
{
  unique_fd generations = open_directory_at(
      root, generations_directory, "open reconciliation generations directory");
  const auto selector = read_file_at(root,
                                     current_file,
                                     256,
                                     false,
                                     "reconciliation current selector");
  return read_generation_at(generations.get(), parse_selector(*selector), target);
}

[[nodiscard]] reconciliation_inventory
read_inventory_locked(int root,
                      const reconciliation_target_reference& target)
{
  require_binding_locked(root, target);
  return read_selected_inventory_locked(root, target);
}

void
write_generation_file(int directory,
                      const char* name,
                      const std::vector<std::uint8_t>& bytes)
{
  const int opened = ::openat(directory,
                              name,
                              O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC |
                                  O_NOFOLLOW,
                              0600);
  if (opened == -1)
    throw_store_failure("create reconciliation generation file");
  unique_fd file(opened);
  try
  {
    write_all(file.get(), bytes, "write reconciliation generation file");
    if (::fchmod(file.get(), 0444) == -1)
      throw_store_failure("set reconciliation generation file mode");
    if (!synchronize(file.get()))
      throw_store_failure("synchronize reconciliation generation file");
  }
  catch (...)
  {
    file.reset();
    static_cast<void>(::unlinkat(directory, name, 0));
    throw;
  }
}

[[nodiscard]] int
rename_noreplace(int old_directory,
                 const char* old_name,
                 int new_directory,
                 const char* new_name)
{
  return static_cast<int>(::syscall(SYS_renameat2,
                                    old_directory,
                                    old_name,
                                    new_directory,
                                    new_name,
                                    RENAME_NOREPLACE));
}

void
remove_temporary_generation(int generations, const std::string& name) noexcept
{
  const int opened = ::openat(generations,
                              name.c_str(),
                              O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  if (opened != -1)
  {
    unique_fd directory(opened);
    static_cast<void>(::fchmod(directory.get(), 0700));
    static_cast<void>(::unlinkat(directory.get(), inventory_file, 0));
  }
  static_cast<void>(::unlinkat(generations, name.c_str(), AT_REMOVEDIR));
}

void
validate_existing_generation(int generations,
                             const std::string& final_name,
                             std::string_view identity,
                             const std::vector<std::uint8_t>& encoded,
                             const reconciliation_target_reference& target)
{
  unique_fd generation = open_directory_at(
      generations, final_name.c_str(), "open existing reconciliation generation");
  struct stat status {};
  if (::fstat(generation.get(), &status) == -1)
    throw_store_failure("inspect existing reconciliation generation");
  if ((status.st_mode & 0222) != 0)
    throw store_error("existing reconciliation generation is not immutable");
  const auto existing = read_file_at(generation.get(),
                                     inventory_file,
                                     maximum_store_file_size,
                                     false,
                                     "existing reconciliation inventory");
  if (*existing != encoded)
    throw store_error("reconciliation generation identity collision or corruption");
  if (generation_identity(*existing) != identity)
    throw store_error("existing reconciliation generation digest is corrupt");
  if (decode_inventory(*existing).target() != target)
    throw store_error("existing reconciliation generation target is inconsistent");
}

void
ensure_generation(int generations,
                  const reconciliation_inventory& inventory,
                  const std::vector<std::uint8_t>& encoded,
                  std::string_view identity)
{
  const std::string final_name = generation_name(identity);
  const int existing = ::openat(generations,
                                final_name.c_str(),
                                O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  if (existing != -1)
  {
    unique_fd ignored(existing);
    validate_existing_generation(
        generations, final_name, identity, encoded, inventory.target());
    synchronize_directory(generations, "synchronize existing reconciliation generation");
    return;
  }
  if (errno != ENOENT)
    throw_store_failure("inspect reconciliation generation destination");

  const std::string temporary = unique_temporary_name("generation");
  if (::mkdirat(generations, temporary.c_str(), 0700) == -1)
    throw_store_failure("create reconciliation generation temporary");

  try
  {
    unique_fd temporary_directory = open_directory_at(
        generations, temporary.c_str(), "open reconciliation generation temporary");
    write_generation_file(temporary_directory.get(), inventory_file, encoded);
    synchronize_directory(temporary_directory.get(),
                          "synchronize reconciliation generation directory");
    if (::fchmod(temporary_directory.get(), 0555) == -1)
      throw_store_failure("set reconciliation generation directory mode");
    synchronize_directory(temporary_directory.get(),
                          "synchronize reconciliation generation mode");
    temporary_directory.reset();

    if (rename_noreplace(generations,
                         temporary.c_str(),
                         generations,
                         final_name.c_str()) == -1)
    {
      if (errno != EEXIST)
        throw_store_failure("install immutable reconciliation generation");
      remove_temporary_generation(generations, temporary);
      validate_existing_generation(
          generations, final_name, identity, encoded, inventory.target());
      synchronize_directory(generations,
                            "synchronize existing reconciliation generation");
      return;
    }
    synchronize_directory(generations, "synchronize reconciliation generations domain");
  }
  catch (...)
  {
    remove_temporary_generation(generations, temporary);
    throw;
  }
}

void
write_current_temporary(int root,
                        const std::string& temporary,
                        std::string_view identity)
{
  const std::string text = std::string(identity) + "\n";
  const std::vector<std::uint8_t> bytes(text.begin(), text.end());
  const int opened = ::openat(root,
                              temporary.c_str(),
                              O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC |
                                  O_NOFOLLOW,
                              0600);
  if (opened == -1)
    throw_store_failure("create reconciliation current selector temporary");
  unique_fd file(opened);
  try
  {
    write_all(file.get(), bytes, "write reconciliation current selector");
    if (::fchmod(file.get(), 0444) == -1)
      throw_store_failure("set reconciliation current selector mode");
    if (!synchronize(file.get()))
      throw_store_failure("synchronize reconciliation current selector");
  }
  catch (...)
  {
    file.reset();
    static_cast<void>(::unlinkat(root, temporary.c_str(), 0));
    throw;
  }
}

void
publish_inventory_locked(int root, const reconciliation_inventory& inventory)
{
  const std::vector<std::uint8_t> encoded = encode_inventory(inventory);
  const std::string identity = generation_identity(encoded);
  unique_fd generations = open_directory_at(
      root, generations_directory, "open reconciliation generations for publication");
  ensure_generation(generations.get(), inventory, encoded, identity);

  const std::string temporary = unique_temporary_name(current_file);
  write_current_temporary(root, temporary, identity);
  if (::renameat(root, temporary.c_str(), root, current_file) == -1)
  {
    static_cast<void>(::unlinkat(root, temporary.c_str(), 0));
    throw_store_failure("select reconciliation generation");
  }

  const bool durable = synchronize(root);
  const reconciliation_inventory observed =
      read_selected_inventory_locked(root, inventory.target());
  if (encode_inventory(observed) != encoded)
    throw store_error("selected reconciliation generation does not match publication");
  if (!durable)
    throw store_error("reconciliation generation selected but durability is unconfirmed");
}

[[nodiscard]] bool
same_inventory(const reconciliation_inventory& lhs,
               const reconciliation_inventory& rhs)
{
  return lhs.target() == rhs.target() && lhs.records() == rhs.records();
}

void
validate_request_target(const reconciliation_target_reference& store_target,
                        const pending_reconciliation& value)
{
  if (value.target() != store_target)
    throw std::invalid_argument("reconciliation value is bound to another target");
}

} // namespace

pending_publication_receipt::pending_publication_receipt(
    std::size_t published,
    std::size_t already_pending,
    std::size_t suppressed_resolved) noexcept
    : published_(published), already_pending_(already_pending),
      suppressed_resolved_(suppressed_resolved)
{
}

std::size_t pending_publication_receipt::published() const noexcept
{
  return published_;
}

std::size_t pending_publication_receipt::already_pending() const noexcept
{
  return already_pending_;
}

std::size_t pending_publication_receipt::suppressed_resolved() const noexcept
{
  return suppressed_resolved_;
}

bool pending_publication_receipt::changed() const noexcept
{
  return published_ != 0;
}

inventory_generation_store::inventory_generation_store(
    std::filesystem::path root, reconciliation_target_reference target)
    : root_(std::move(root)), target_(std::move(target))
{
  initialize();
}

inventory_generation_store::~inventory_generation_store()
{
  if (root_descriptor_ != -1)
    static_cast<void>(::close(root_descriptor_));
}

inventory_generation_store inventory_generation_store::open_existing(
    std::filesystem::path root, reconciliation_target_reference target)
{
  return inventory_generation_store(
      std::move(root), std::move(target), existing_store_tag{});
}

inventory_generation_store::inventory_generation_store(
    std::filesystem::path root,
    reconciliation_target_reference target,
    existing_store_tag)
    : root_(std::move(root)), target_(std::move(target))
{
  validate_existing();
}

const std::filesystem::path&
inventory_generation_store::root_path() const noexcept
{
  return root_;
}

const reconciliation_target_reference&
inventory_generation_store::target_binding() const noexcept
{
  return target_;
}

reconciliation_inventory inventory_generation_store::read() const
{
  unique_fd root = reopen_directory_authority(
      root_descriptor_, "reopen reconciliation read authority");
  lock_directory(root.get(), LOCK_SH, "acquire reconciliation read lock");
  return read_inventory_locked(root.get(), target_);
}

pending_publication_receipt inventory_generation_store::publish_pending(
    std::vector<pending_reconciliation> values)
{
  for (const pending_reconciliation& value : values)
    validate_request_target(target_, value);

  std::sort(values.begin(), values.end());
  if (std::adjacent_find(values.begin(), values.end()) != values.end())
    throw std::invalid_argument("pending publication contains duplicate evidence");

  unique_fd root = reopen_directory_authority(
      root_descriptor_, "reopen reconciliation publication authority");
  lock_directory(root.get(), LOCK_EX, "acquire reconciliation publication lock");
  const reconciliation_inventory current = read_inventory_locked(root.get(), target_);
  std::vector<reconciliation_record> records = current.records();

  std::size_t inserted = 0;
  std::size_t already_pending = 0;
  std::size_t suppressed_resolved = 0;
  for (const pending_reconciliation& value : values)
  {
    const reconciliation_record* existing = current.find(value);
    if (existing == nullptr)
    {
      records.push_back(reconciliation_record::pending(value));
      ++inserted;
      continue;
    }
    if (existing->status() == reconciliation_record_status::pending)
      ++already_pending;
    else
      ++suppressed_resolved;
  }

  if (inserted != 0)
  {
    reconciliation_inventory resulting =
        reconciliation_inventory::make(target_, std::move(records));
    if (same_inventory(current, resulting))
      throw store_error("reconciliation publication changed count without state change");
    publish_inventory_locked(root.get(), resulting);
  }

  return pending_publication_receipt(
      inserted, already_pending, suppressed_resolved);
}

resolution_outcome
inventory_generation_store::resolve(const pending_reconciliation& value)
{
  validate_request_target(target_, value);
  unique_fd root = reopen_directory_authority(
      root_descriptor_, "reopen reconciliation resolution authority");
  lock_directory(root.get(), LOCK_EX, "acquire reconciliation resolution lock");
  const reconciliation_inventory current = read_inventory_locked(root.get(), target_);
  const reconciliation_record* existing = current.find(value);
  if (existing == nullptr)
    return resolution_outcome::missing;
  if (existing->status() == reconciliation_record_status::resolved)
    return resolution_outcome::already_resolved;

  std::vector<reconciliation_record> records = current.records();
  const auto position = std::find_if(
      records.begin(), records.end(), [&](const reconciliation_record& record) {
        return record.value() == value;
      });
  if (position == records.end())
    throw store_error("reconciliation inventory lookup became inconsistent");
  *position = reconciliation_record::resolved(value);
  reconciliation_inventory resulting =
      reconciliation_inventory::make(target_, std::move(records));
  publish_inventory_locked(root.get(), resulting);
  return resolution_outcome::resolved;
}

void inventory_generation_store::validate_existing()
{
  validate_store_path(root_);
  unique_fd root = open_directory(root_);
  lock_directory(root.get(), LOCK_SH, "acquire reconciliation validation lock");
  static_cast<void>(read_inventory_locked(root.get(), target_));
  unique_fd retained = reopen_directory_authority(
      root.get(), "retain reconciliation store authority");
  root_descriptor_ = retained.release();
}

void inventory_generation_store::initialize()
{
  validate_store_path(root_);

  std::error_code failure;
  std::filesystem::create_directories(root_, failure);
  if (failure)
    throw store_error("create reconciliation store directory: " + failure.message());

  unique_fd root = open_directory(root_);
  lock_directory(root.get(), LOCK_EX, "acquire reconciliation initialization lock");
  ensure_directory_at(root.get(), generations_directory);

  const auto stored_binding = read_binding_locked(root.get());
  if (stored_binding)
  {
    if (*stored_binding != target_)
      throw store_error("reconciliation store target binding does not match caller");
    static_cast<void>(read_selected_inventory_locked(root.get(), target_));
    synchronize_directory(root.get(), "synchronize reconciliation store layout");
    unique_fd retained = reopen_directory_authority(
        root.get(), "retain reconciliation store authority");
    root_descriptor_ = retained.release();
    return;
  }

  const reconciliation_inventory empty = reconciliation_inventory::make(target_);
  const auto selector = read_file_at(root.get(),
                                     current_file,
                                     256,
                                     true,
                                     "reconciliation current selector");
  if (!selector)
  {
    const std::vector<std::uint8_t> encoded = encode_inventory(empty);
    const std::string identity = generation_identity(encoded);
    unique_fd generations = open_directory_at(
        root.get(), generations_directory, "open reconciliation generations for initialization");
    ensure_generation(generations.get(), empty, encoded, identity);
    const std::string temporary = unique_temporary_name(current_file);
    write_current_temporary(root.get(), temporary, identity);
    if (::renameat(root.get(), temporary.c_str(), root.get(), current_file) == -1)
    {
      static_cast<void>(::unlinkat(root.get(), temporary.c_str(), 0));
      throw_store_failure("select initial reconciliation generation");
    }
    if (!synchronize(root.get()))
      throw store_error("initial reconciliation generation selected but durability is unconfirmed");
  }
  else
  {
    const reconciliation_inventory selected =
        read_selected_inventory_locked(root.get(), target_);
    if (selected.size() != 0)
      throw store_error("unbound reconciliation store does not select an empty inventory");
  }

  write_atomic_metadata(root.get(), binding_file, encode_binding(target_));
  static_cast<void>(read_inventory_locked(root.get(), target_));
  synchronize_directory(root.get(), "synchronize reconciliation store layout");
  unique_fd retained = reopen_directory_authority(
      root.get(), "retain reconciliation store authority");
  root_descriptor_ = retained.release();
}

} // namespace pkgreconcile::posix
