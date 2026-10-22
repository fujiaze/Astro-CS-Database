#pragma once
// Construction helpers for the immutable array and index objects declared in
// include/acsd/contracts.hpp.  Every helper produces a deterministic content
// hash so that the same scientific content always yields the same identity.
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"
#include "model/hash.hpp"

namespace acsd::model {

// Bytes required for one element of the given dtype.
std::size_t dtype_size(DType dtype);
bool dtype_is_float(DType dtype);

// Canonical little-endian encoding of a value array for hashing and for
// writing to disk.
std::vector<std::byte> encode_f64(std::span<const double> values);
std::vector<std::byte> encode_u64(std::span<const U64> values);
std::vector<std::byte> encode_u32(std::span<const U32> values);
std::vector<std::byte> encode_u16(std::span<const std::uint16_t> values);
std::vector<std::byte> encode_u8(std::span<const std::uint8_t> values);
std::vector<std::byte> encode_dyadic(std::span<const Dyadic> values);

std::vector<double> decode_f64(std::span<const std::byte> bytes);
std::vector<U64> decode_u64(std::span<const std::byte> bytes);
std::vector<std::uint16_t> decode_u16(std::span<const std::byte> bytes);
std::vector<std::uint8_t> decode_u8(std::span<const std::byte> bytes);

// Hash of a scientific payload: dtype, shape, unit and the raw bytes.
std::string hash_payload(DType dtype, const std::vector<U64>& shape, const Unit& unit,
                         std::span<const std::byte> bytes);

std::string hash_index(Index::Kind kind, U64 size, U32 order, std::span<const SourceIdentity> sources);

// Builds the identity + description of an immutable array.
struct ArrayDescription {
  Identity identity;
  DType dtype = DType::f64;
  std::vector<U64> shape;
  Unit unit;
  Id index_id;
};

ArrayDescription describe_array(const std::string& schema_id, DType dtype,
                                const std::vector<U64>& shape, const Unit& unit,
                                const std::string& index_id, std::span<const std::byte> bytes,
                                const std::string& run_id);

// Block descriptor for a contiguous element range.
BlockDescriptor make_block_descriptor(const Id& id, const std::string& path, DType dtype,
                                      const std::vector<U64>& shape, const Id& index_id,
                                      const Unit& unit, U64 begin, U64 count,
                                      std::span<const std::byte> bytes, bool compress);

// Index builders.
Index make_native_index(std::vector<SourceIdentity> sources, U64 size, Id namespace_id);
Index make_leaf_index(const std::vector<HealpixLeaf>& leaves);
Index make_parameter_index(const std::vector<Id>& ids);
Index make_control_index(U64 size, Id namespace_id);
Index make_latent_index(U64 size, Id namespace_id);
Index make_pixel_index(U64 size, Id namespace_id);

// The id of a source partition used by NativeObservation.
SourceIdentity source_at(const Index& index, U64 row);

}  // namespace acsd::model
