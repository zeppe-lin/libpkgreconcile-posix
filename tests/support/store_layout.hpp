// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBPKGRECONCILE_POSIX_TEST_STORE_LAYOUT_HPP
#define LIBPKGRECONCILE_POSIX_TEST_STORE_LAYOUT_HPP

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <sys/stat.h>

#include <openssl/evp.h>

namespace test_support {

inline std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path)
{
  std::ifstream input(path, std::ios::binary);
  if (!input)
    throw std::runtime_error("could not read test file " + path.string());
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

inline void write_bytes(const std::filesystem::path& path,
                        const std::vector<std::uint8_t>& bytes)
{
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output)
    throw std::runtime_error("could not write test file " + path.string());
  output.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  output.close();
  if (!output)
    throw std::runtime_error("could not write test file " + path.string());
}

inline void rewrite_immutable(const std::filesystem::path& path,
                              const std::vector<std::uint8_t>& bytes)
{
  if (::chmod(path.c_str(), 0644) == -1)
    throw std::runtime_error("could not make test file writable");
  write_bytes(path, bytes);
  if (::chmod(path.c_str(), 0444) == -1)
    throw std::runtime_error("could not restore test file mode");
}

inline void rewrite_immutable(const std::filesystem::path& path,
                              std::string_view text)
{
  rewrite_immutable(path, std::vector<std::uint8_t>(text.begin(), text.end()));
}

inline std::string selector_text(const std::filesystem::path& root)
{
  const auto bytes = read_bytes(root / "current");
  return {bytes.begin(), bytes.end()};
}

inline std::filesystem::path selected_generation(const std::filesystem::path& root)
{
  std::string selector = selector_text(root);
  if (selector.empty() || selector.back() != '\n')
    throw std::runtime_error("invalid selector in test fixture");
  selector.pop_back();
  constexpr std::string_view prefix = "v1:sha256:";
  if (selector.rfind(prefix, 0) != 0)
    throw std::runtime_error("unexpected selector in test fixture");
  return root / "generations" / ("v1-sha256-" + selector.substr(prefix.size()));
}

inline std::size_t generation_count(const std::filesystem::path& root)
{
  std::size_t count = 0;
  for (const auto& entry : std::filesystem::directory_iterator(root / "generations"))
  {
    if (entry.is_directory())
      ++count;
  }
  return count;
}

inline bool has_temporary_entry(const std::filesystem::path& root)
{
  for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
  {
    if (entry.path().filename().string().find(".tmp.") != std::string::npos)
      return true;
  }
  return false;
}


inline std::string sha256_hex(const std::vector<std::uint8_t>& bytes)
{
  EVP_MD_CTX* context = EVP_MD_CTX_new();
  if (context == nullptr)
    throw std::runtime_error("could not allocate test digest context");
  std::vector<std::uint8_t> digest(32U);
  unsigned int size = 0;
  const bool ok = EVP_DigestInit_ex(context, EVP_sha256(), nullptr) == 1 &&
      (bytes.empty() || EVP_DigestUpdate(context, bytes.data(), bytes.size()) == 1) &&
      EVP_DigestFinal_ex(context, digest.data(), &size) == 1 &&
      size == digest.size();
  EVP_MD_CTX_free(context);
  if (!ok)
    throw std::runtime_error("could not compute test digest");

  static constexpr char digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(64U);
  for (const std::uint8_t byte : digest)
  {
    result.push_back(digits[(byte >> 4U) & 0x0fU]);
    result.push_back(digits[byte & 0x0fU]);
  }
  return result;
}

inline void select_inventory_bytes(const std::filesystem::path& root,
                                   const std::vector<std::uint8_t>& bytes)
{
  const std::string digest = sha256_hex(bytes);
  const std::filesystem::path generation =
      root / "generations" / ("v1-sha256-" + digest);
  std::filesystem::create_directory(generation);
  write_bytes(generation / "inventory", bytes);
  if (::chmod((generation / "inventory").c_str(), 0444) == -1 ||
      ::chmod(generation.c_str(), 0555) == -1)
  {
    throw std::runtime_error("could not freeze test generation");
  }
  rewrite_immutable(root / "current", "v1:sha256:" + digest + "\n");
}

} // namespace test_support

#endif
