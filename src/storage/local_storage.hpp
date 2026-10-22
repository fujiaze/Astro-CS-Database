#pragma once
// Immutable array storage: in-memory arrays, disk backed blocks and the
// product directory layout.  One implementation serves the CLI, the contract
// driver and the in-process API, so that every entry point observes the same
// block hashes, units and index identities.
#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "acsd/contracts.hpp"

namespace acsd::storage {

// A stored immutable array: raw little endian bytes plus its description.
struct StoredArray {
  Identity identity;
  DType dtype = DType::f64;
  std::vector<U64> shape;
  Unit unit;
  Id index_id;
  std::vector<BlockDescriptor> blocks;
  std::shared_ptr<std::vector<std::byte>> bytes;
};

class LocalStorage final : public Storage {
 public:
  LocalStorage();
  ~LocalStorage() override;

  Result<ArrayRef> copy(Context& context, DType dtype, std::span<const std::byte> bytes,
                        std::span<const U64> shape, const Unit& unit, Id index) override;
  Result<ArrayView> read(Context& context, ArrayRef array, BlockRequest range) const override;
  Result<ArrayRef> open(Context& context, std::string root, const Array& description,
                        std::span<const BlockDescriptor> blocks) override;

  // Registers an already built buffer (used by the product reader and by the
  // workflow layer when it assembles a closure).
  Result<ArrayRef> adopt(Context& context, DType dtype, std::span<const std::byte> bytes,
                         std::span<const U64> shape, const Unit& unit, Id index,
                         std::span<const BlockDescriptor> blocks);

  // Looks up a stored array by identity id.
  std::shared_ptr<const StoredArray> find(const Id& id) const;

  void account_read(U64 bytes) const;
  void account_written(U64 bytes) const;

  // Filesystem helpers used by the product layer.
  static bool write_file(const std::filesystem::path& path, std::span<const std::byte> bytes,
                         std::string& error);
  static bool read_file(const std::filesystem::path& path, std::vector<std::byte>& bytes,
                        std::string& error);
  static std::vector<std::byte> compress(std::span<const std::byte> bytes);
  static std::vector<std::byte> decompress(std::span<const std::byte> bytes, U64 raw_bytes,
                                           std::string& error);

 private:
  mutable std::shared_mutex mutex_;
  std::unordered_map<Id, std::shared_ptr<StoredArray>> arrays_;
  mutable std::atomic<U64> read_bytes_{0};
  mutable std::atomic<U64> written_bytes_{0};
};

}  // namespace acsd::storage
