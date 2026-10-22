#include "model/array.hpp"

#include <cstring>
#include <stdexcept>

#include "model/hash.hpp"
#include "model/units.hpp"

namespace acsd::model {
namespace {

void append_shape(Sha256& hash, const std::vector<U64>& shape) {
  hash.update_u64(shape.size());
  for (U64 s : shape) hash.update_u64(s);
}

void append_unit(Sha256& hash, const Unit& unit) {
  hash.field("scale", unit.scale);
  hash.update_u64(static_cast<std::uint64_t>(unit.sr_power));
  hash.update_u64(static_cast<std::uint64_t>(unit.pixel_power));
  hash.update_u64(static_cast<std::uint64_t>(unit.nm_power));
  hash.field_f64("multiplier", unit.multiplier);
  hash.update_u64(static_cast<std::uint64_t>(unit.linear_power));
}

void append_source(Sha256& hash, const SourceIdentity& source) {
  hash.field("dataset", source.dataset);
  hash.field("exposure", source.exposure);
  hash.field("detector", source.detector);
  hash.field("channel", source.channel);
  hash.update_u64(source.sample);
}

}  // namespace

std::size_t dtype_size(DType dtype) {
  switch (dtype) {
    case DType::f64: return 8;
    case DType::f32: return 4;
    case DType::u64: return 8;
    case DType::u32: return 4;
    case DType::u16: return 2;
    case DType::u8: return 1;
    case DType::dyadic: return 0;  // variable length, encoded as UTF-8 objects
  }
  return 0;
}

bool dtype_is_float(DType dtype) { return dtype == DType::f64 || dtype == DType::f32; }

std::vector<std::byte> encode_f64(std::span<const double> values) {
  std::vector<std::byte> out(values.size() * 8);
  for (std::size_t i = 0; i < values.size(); ++i) append_f64_le(out, values[i]);
  return out;
}

std::vector<std::byte> encode_u64(std::span<const U64> values) {
  std::vector<std::byte> out(values.size() * 8);
  for (std::size_t i = 0; i < values.size(); ++i) append_u64_le(out, values[i]);
  return out;
}

std::vector<std::byte> encode_u32(std::span<const U32> values) {
  std::vector<std::byte> out(values.size() * 4);
  for (std::size_t i = 0; i < values.size(); ++i) append_u32_le(out, values[i]);
  return out;
}

std::vector<std::byte> encode_u16(std::span<const std::uint16_t> values) {
  std::vector<std::byte> out(values.size() * 2);
  for (std::size_t i = 0; i < values.size(); ++i) {
    out[2 * i] = static_cast<std::byte>(values[i] & 0xffu);
    out[2 * i + 1] = static_cast<std::byte>((values[i] >> 8) & 0xffu);
  }
  return out;
}

std::vector<std::byte> encode_u8(std::span<const std::uint8_t> values) {
  std::vector<std::byte> out(values.size());
  for (std::size_t i = 0; i < values.size(); ++i) out[i] = static_cast<std::byte>(values[i]);
  return out;
}

std::vector<std::byte> encode_dyadic(std::span<const Dyadic> values) {
  std::string text;
  for (const Dyadic& d : values) {
    text += d.signed_integer;
    text += ':';
    text += std::to_string(d.exponent2);
    text += '\n';
  }
  return std::vector<std::byte>(reinterpret_cast<const std::byte*>(text.data()),
                                reinterpret_cast<const std::byte*>(text.data()) + text.size());
}

std::vector<double> decode_f64(std::span<const std::byte> bytes) {
  std::vector<double> out(bytes.size() / 8);
  for (std::size_t i = 0; i < out.size(); ++i) {
    std::uint64_t bits = 0;
    for (int b = 0; b < 8; ++b)
      bits |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(bytes[8 * i + b])) << (8 * b);
    std::memcpy(&out[i], &bits, sizeof(double));
  }
  return out;
}

std::vector<U64> decode_u64(std::span<const std::byte> bytes) {
  std::vector<U64> out(bytes.size() / 8);
  for (std::size_t i = 0; i < out.size(); ++i) {
    std::uint64_t bits = 0;
    for (int b = 0; b < 8; ++b)
      bits |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(bytes[8 * i + b])) << (8 * b);
    out[i] = bits;
  }
  return out;
}

std::vector<std::uint16_t> decode_u16(std::span<const std::byte> bytes) {
  std::vector<std::uint16_t> out(bytes.size() / 2);
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i] = static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[2 * i]) |
                                        (std::to_integer<std::uint8_t>(bytes[2 * i + 1]) << 8));
  }
  return out;
}

std::vector<std::uint8_t> decode_u8(std::span<const std::byte> bytes) {
  std::vector<std::uint8_t> out(bytes.size());
  for (std::size_t i = 0; i < out.size(); ++i)
    out[i] = std::to_integer<std::uint8_t>(bytes[i]);
  return out;
}

std::string hash_payload(DType dtype, const std::vector<U64>& shape, const Unit& unit,
                         std::span<const std::byte> bytes) {
  Sha256 hash;
  hash.field("kind", "acsd.array");
  hash.update_u64(static_cast<std::uint64_t>(dtype));
  append_shape(hash, shape);
  append_unit(hash, unit);
  hash.update_u64(bytes.size());
  hash.update(bytes);
  return hash.hex();
}

std::string hash_index(Index::Kind kind, U64 size, U32 order,
                       std::span<const SourceIdentity> sources) {
  Sha256 hash;
  hash.field("kind", "acsd.index");
  hash.update_u64(static_cast<std::uint64_t>(kind));
  hash.update_u64(size);
  hash.update_u64(order);
  hash.update_u64(sources.size());
  for (const SourceIdentity& source : sources) append_source(hash, source);
  return hash.hex();
}

ArrayDescription describe_array(const std::string& schema_id, DType dtype,
                                const std::vector<U64>& shape, const Unit& unit,
                                const std::string& index_id, std::span<const std::byte> bytes,
                                const std::string& run_id) {
  ArrayDescription description;
  description.dtype = dtype;
  description.shape = shape;
  description.unit = unit;
  description.index_id = index_id;
  const std::string hex = hash_payload(dtype, shape, unit, bytes);
  description.identity = make_identity(schema_id, 2, 0, hex, run_id);
  return description;
}

BlockDescriptor make_block_descriptor(const Id& id, const std::string& path, DType dtype,
                                      const std::vector<U64>& shape, const Id& index_id,
                                      const Unit& unit, U64 begin, U64 count,
                                      std::span<const std::byte> bytes, bool compress) {
  BlockDescriptor descriptor;
  descriptor.id = id;
  descriptor.path = path;
  descriptor.dtype = dtype;
  descriptor.shape = shape;
  descriptor.index_id = index_id;
  descriptor.unit = unit;
  descriptor.begin = begin;
  descriptor.count = count;
  descriptor.codec = compress ? BlockDescriptor::Codec::zstd : BlockDescriptor::Codec::raw;
  descriptor.raw_bytes = bytes.size();
  descriptor.stored_bytes = bytes.size();
  descriptor.raw_sha256 = content_id(sha256_hex(bytes));
  return descriptor;
}

Index make_native_index(std::vector<SourceIdentity> sources, U64 size, Id namespace_id) {
  Index index;
  index.identity = make_identity("acsd.Index", 2, 0,
                                 hash_index(Index::Kind::native, size, 0, sources), "acsd");
  index.assessment = make_assessment(Code::ok);
  index.assessment.complete = true;
  index.size = size;
  index.kind = Index::Kind::native;
  index.order = 0;
  index.namespace_id = namespace_id;
  if (sources.size() <= 4096) index.sources = std::move(sources);
  return index;
}

Index make_leaf_index(const std::vector<HealpixLeaf>& leaves) {
  Index index;
  index.identity = make_identity("acsd.Index", 2, 0, hash_index(Index::Kind::leaf, leaves.size(), 0, {}),
                                 "acsd");
  index.assessment = make_assessment(Code::ok);
  index.assessment.complete = true;
  index.size = leaves.size();
  index.kind = Index::Kind::leaf;
  index.order = leaves.empty() ? 0 : leaves.front().order;
  for (const HealpixLeaf& leaf : leaves) {
    if (leaf.order != index.order) throw std::invalid_argument("leaf index mixes orders");
  }
  index.namespace_id = "acsd.leaf";
  return index;
}

Index make_parameter_index(const std::vector<Id>& ids) {
  Index index;
  Sha256 hash;
  hash.field("kind", "acsd.index");
  hash.field("subkind", "parameter");
  for (const Id& id : ids) hash.field("id", id);
  index.identity = make_identity("acsd.Index", 2, 0, hash.hex(), "acsd");
  index.assessment = make_assessment(Code::ok);
  index.assessment.complete = true;
  index.size = ids.size();
  index.kind = Index::Kind::parameter;
  index.namespace_id = "acsd.parameter";
  return index;
}

Index make_control_index(U64 size, Id namespace_id) {
  Index index;
  index.identity = make_identity("acsd.Index", 2, 0, hash_index(Index::Kind::control, size, 0, {}),
                                 "acsd");
  index.assessment = make_assessment(Code::ok);
  index.assessment.complete = true;
  index.size = size;
  index.kind = Index::Kind::control;
  index.namespace_id = namespace_id;
  return index;
}

Index make_latent_index(U64 size, Id namespace_id) {
  Index index;
  index.identity = make_identity("acsd.Index", 2, 0, hash_index(Index::Kind::latent, size, 0, {}),
                                 "acsd");
  index.assessment = make_assessment(Code::ok);
  index.assessment.complete = true;
  index.size = size;
  index.kind = Index::Kind::latent;
  index.namespace_id = namespace_id;
  return index;
}

Index make_pixel_index(U64 size, Id namespace_id) {
  Index index;
  index.identity = make_identity("acsd.Index", 2, 0, hash_index(Index::Kind::pixel, size, 0, {}),
                                 "acsd");
  index.assessment = make_assessment(Code::ok);
  index.assessment.complete = true;
  index.size = size;
  index.kind = Index::Kind::pixel;
  index.namespace_id = namespace_id;
  return index;
}

SourceIdentity source_at(const Index& index, U64 row) {
  if (!index.sources.empty()) {
    if (row < index.sources.size()) return index.sources[row];
    return SourceIdentity{"", "", "", "", row};
  }
  for (const SourcePartition& partition : index.partitions) {
    if (row >= partition.index_begin && row < partition.index_begin + partition.count) {
      const U64 local = row - partition.index_begin;
      return SourceIdentity{partition.dataset, partition.exposure, partition.detector, partition.channel,
                            partition.first_sample + local * partition.sample_stride};
    }
  }
  return SourceIdentity{"", "", "", "", row};
}

}  // namespace acsd::model
