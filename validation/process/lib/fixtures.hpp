#pragma once
// Frozen fixtures for the M5 process drivers.
//
// The drivers include only public headers.  This header builds the small input
// objects (arrays through the public runtime Storage, indexes, footprint sets,
// a TAN-SIP header, a native observation) that the typed process inputs need.
// It contains no production algorithm and no private header.
#include "../../runtime/lib/harness.hpp"

#include <acsd/contracts.hpp>
#include <acsd/modules/generated/inputs.hpp>
#include <acsd/modules/generated/status.hpp>
#include <acsd/runtime/generated_manifest.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace vpf {

using acsd::ArrayRef;
using acsd::Box;
using acsd::Code;
using acsd::DType;
using acsd::HeaderField;
using acsd::HeaderFields;
using acsd::Id;
using acsd::Index;
using acsd::NativeObservation;
using acsd::NumericBudget;
using acsd::Ref;
using acsd::Result;
using acsd::U64;
using acsd::Unit;
namespace rt = acsd::rt;

inline acsd::Identity identity_of(const std::string& object_id, const std::string& schema) {
  acsd::Identity identity;
  identity.object_id = object_id;
  identity.content_hash = "sha256:" + std::string(64, '0');
  identity.run_id = "validation.process";
  identity.revision = 0;
  identity.schema_id = schema;
  identity.schema_major = 2;
  identity.schema_minor = 0;
  return identity;
}

inline Unit unit_of(const std::string& scale, std::int32_t linear_power = 1) {
  Unit unit;
  unit.scale = scale;
  unit.multiplier = 1.0;
  unit.linear_power = linear_power;
  return unit;
}

inline Unit dimensionless() { return unit_of("1", 0); }

inline Ref<Index> make_index(U64 size, const std::string& id,
                             Index::Kind kind = Index::Kind::native) {
  auto index = std::make_shared<Index>();
  index->size = size;
  index->kind = kind;
  index->identity = identity_of(id, "acsd.Index");
  index->assessment.complete = true;
  return index;
}

inline Ref<acsd::FootprintSet> make_footprints(Ref<Index> index, U64 width, U64 height,
                                               const std::string& detector_frame) {
  auto footprints = std::make_shared<acsd::FootprintSet>();
  footprints->index = index;
  footprints->width = width;
  footprints->height = height;
  footprints->detector_from_sample = acsd::Mat2{1.0, 0.0, 0.0, 1.0};
  footprints->offset = acsd::Vec2{0.0, 0.0};
  footprints->local_cell = Box{acsd::Vec2{0.0, 0.0},
                               acsd::Vec2{static_cast<double>(width),
                                          static_cast<double>(height)}};
  footprints->detector_frame = detector_frame;
  footprints->identity = identity_of("footprints:" + detector_frame + ":" +
                                         std::to_string(width) + "x" + std::to_string(height),
                                     "acsd.FootprintSet");
  footprints->assessment.complete = true;
  return footprints;
}

struct Arrays {
  rt::ArenaRef arena;
  ArrayRef array;
};

template <class T>
Result<Arrays> make_array(const rt::InvocationContext& ctx, DType dtype,
                          const std::vector<T>& values, const std::vector<U64>& shape,
                          const Ref<Index>& index, const Unit& unit) {
  Result<Arrays> failure;
  failure.code = Code::internal;
  const U64 bytes = values.size() * sizeof(T);
  auto arena = rt::reserve(ctx, vrt::bytes(bytes + (1ull << 20)));
  if (arena.code != Code::ok || !arena.value) return failure;
  rt::BuildSpec spec;
  spec.layout.dtype = dtype;
  spec.layout.shape = shape;
  spec.layout.unit = unit;
  spec.layout.index = index;
  spec.placement = rt::Placement::memory;
  spec.buffer_bytes = bytes;
  spec.metadata_bytes = 4096;
  auto writer = rt::wait_external(rt::begin_async(ctx, *arena.value, spec));
  if (writer.code != Code::ok || !writer.value) {
    (*arena.value)->close();
    return failure;
  }
  auto buffer = rt::allocate<T>(ctx, *arena.value, acsd::BlockRequest{0, values.size()}, index,
                                unit, vrt::zero_hash());
  if (buffer.code != Code::ok || !buffer.value) {
    (*arena.value)->close();
    return failure;
  }
  if (bytes != 0) std::memcpy(buffer.value->values.data(), values.data(), bytes);
  auto block = rt::freeze(std::move(*buffer.value));
  if (block.code != Code::ok || !block.value) {
    (*arena.value)->close();
    return failure;
  }
  auto appended =
      rt::wait_external(rt::append_async<T>(ctx, *writer.value, std::move(*block.value)));
  if (appended.code != Code::ok) {
    (*arena.value)->close();
    return failure;
  }
  auto sealed = rt::wait_external(rt::seal_async(ctx, *writer.value));
  if (sealed.code != Code::ok || !sealed.value) {
    (*arena.value)->close();
    return failure;
  }
  Arrays out;
  out.arena = *arena.value;
  out.array = *sealed.value;
  (*arena.value)->close();
  Result<Arrays> result;
  result.code = Code::ok;
  result.value = std::move(out);
  return result;
}

inline std::vector<double> read_f64(const rt::InvocationContext& ctx, ArrayRef array, U64 count) {
  std::vector<double> values;
  if (!array) return values;
  auto arena = rt::reserve(ctx, vrt::bytes(count * sizeof(double) + (1ull << 20)));
  if (arena.code != Code::ok) return values;
  auto task = rt::read_async<double>(ctx, *arena.value, array, acsd::BlockRequest{0, count});
  auto result = rt::wait_external(std::move(task));
  if (result.code == Code::ok && result.value)
    values.assign(result.value->values.begin(), result.value->values.end());
  (*arena.value)->close();
  return values;
}

inline std::vector<std::uint16_t> read_u16(const rt::InvocationContext& ctx, ArrayRef array,
                                           U64 count) {
  std::vector<std::uint16_t> values;
  if (!array) return values;
  auto arena = rt::reserve(ctx, vrt::bytes(count * sizeof(std::uint16_t) + (1ull << 20)));
  if (arena.code != Code::ok) return values;
  auto task = rt::read_async<std::uint16_t>(ctx, *arena.value, array,
                                            acsd::BlockRequest{0, count});
  auto result = rt::wait_external(std::move(task));
  if (result.code == Code::ok && result.value)
    values.assign(result.value->values.begin(), result.value->values.end());
  (*arena.value)->close();
  return values;
}

inline NumericBudget budget(double absolute = 1e-9, double relative = 1e-9,
                            acsd::U32 max_iterations = 64) {
  NumericBudget value;
  value.tolerance.absolute = absolute;
  value.tolerance.relative = relative;
  value.tolerance.absolute_floor = 0.0;
  value.tolerance.unit = dimensionless();
  value.max_iterations = max_iterations;
  value.arithmetic.mode = acsd::Arithmetic::Mode::outward;
  value.arithmetic.initial_bits = 64;
  value.arithmetic.max_bits = 256;
  value.arithmetic.allow_target_fallback = false;
  return value;
}

inline HeaderFields tan_header(double cdelt_deg = 0.001) {
  HeaderFields header;
  auto text = [&header](const char* key, const std::string& value) {
    HeaderField field;
    field.key = key;
    field.value = value;
    header.push_back(std::move(field));
  };
  auto number = [&header](const char* key, double value) {
    HeaderField field;
    field.key = key;
    field.value = value;
    header.push_back(std::move(field));
  };
  text("CTYPE1", "RA---TAN");
  text("CTYPE2", "DEC--TAN");
  text("CUNIT1", "deg");
  text("CUNIT2", "deg");
  text("RADESYS", "ICRS");
  number("CRPIX1", 1.0);
  number("CRPIX2", 1.0);
  number("CRVAL1", 10.0);
  number("CRVAL2", 20.0);
  number("CD1_1", -cdelt_deg);
  number("CD1_2", 0.0);
  number("CD2_1", 0.0);
  number("CD2_2", cdelt_deg);
  return header;
}

struct NativeFixture {
  Ref<NativeObservation> observation;
  Ref<Index> index;
  Ref<acsd::FootprintSet> footprints;
  std::vector<double> values;
};

inline NativeFixture make_native(const rt::InvocationContext& ctx,
                                 const std::vector<double>& values,
                                 const std::vector<std::uint16_t>& dq, U64 width, U64 height,
                                 const std::string& frame, double epoch,
                                 bool with_bounds = true) {
  NativeFixture fixture;
  fixture.values = values;
  fixture.index = make_index(values.size(), "native:" + frame);
  fixture.footprints = make_footprints(fixture.index, width, height, "detector0");
  auto value_array = make_array<double>(ctx, DType::f64, values,
                                        {static_cast<U64>(values.size())}, fixture.index,
                                        unit_of("ADU"));
  auto dq_array = make_array<std::uint16_t>(ctx, DType::u16, dq,
                                            {static_cast<U64>(dq.size())}, fixture.index,
                                            dimensionless());
  if (value_array.code != Code::ok || dq_array.code != Code::ok) return fixture;
  auto observation = std::make_shared<NativeObservation>();
  observation->integrated_values = value_array.value->array;
  observation->dq = dq_array.value->array;
  observation->native_index = fixture.index;
  observation->footprints = fixture.footprints;
  observation->unit = unit_of("ADU");
  observation->frame = frame;
  observation->physical_collection = true;
  observation->context.epoch_julian_year = epoch;
  observation->context.observation_jd_tdb = 2457500.5;
  observation->identity = identity_of("native:" + frame, "acsd.NativeObservation");
  observation->assessment.complete = true;
  if (with_bounds) {
    std::vector<double> lower;
    std::vector<double> upper;
    for (double value : values) {
      const double delta = std::max(1e-12, std::fabs(value) * 1e-12);
      lower.push_back(value - delta);
      upper.push_back(value + delta);
    }
    auto low = make_array<double>(ctx, DType::f64, lower,
                                  {static_cast<U64>(values.size())}, fixture.index,
                                  unit_of("ADU"));
    auto high = make_array<double>(ctx, DType::f64, upper,
                                   {static_cast<U64>(values.size())}, fixture.index,
                                   unit_of("ADU"));
    if (low.code == Code::ok) observation->numerical_lower = low.value->array;
    if (high.code == Code::ok) observation->numerical_upper = high.value->array;
  }
  fixture.observation = observation;
  return fixture;
}

inline std::vector<double> gaussian_image(U64 width, U64 height,
                                          const std::vector<std::array<double, 3>>& sources,
                                          double sigma = 1.2, double background = 1.0) {
  std::vector<double> image(width * height, 0.0);
  for (U64 y = 0; y < height; ++y) {
    for (U64 x = 0; x < width; ++x) {
      double value = background;
      for (const auto& source : sources) {
        const double dx = static_cast<double>(x) - source[0];
        const double dy = static_cast<double>(y) - source[1];
        value += source[2] * std::exp(-0.5 * (dx * dx + dy * dy) / (sigma * sigma));
      }
      image[y * width + x] = value;
    }
  }
  return image;
}

inline acsd::CatalogueStar make_star(U64 id, double ra_rad, double dec_rad, double epoch,
                                     double brightness_mag = 12.0) {
  acsd::CatalogueStar star;
  star.source_id = id;
  star.ra_rad = ra_rad;
  star.dec_rad = dec_rad;
  star.epoch_year = epoch;
  star.pmra_cosdec_rad_year = 0.0;
  star.pmdec_rad_year = 0.0;
  star.parallax_rad = 0.0;
  star.brightness_mag = brightness_mag;
  return star;
}

inline acsd::SkyICRS direction(double ra_rad, double dec_rad) {
  acsd::SkyICRS sky;
  sky.nx = std::cos(dec_rad) * std::cos(ra_rad);
  sky.ny = std::cos(dec_rad) * std::sin(ra_rad);
  sky.nz = std::sin(dec_rad);
  return sky;
}

// Builds a native observation on caller-supplied index/footprint objects (used
// when a synthetic image is generated by the pixel-integral model itself).
inline Ref<NativeObservation> make_native_with(const rt::InvocationContext& ctx,
                                               const std::vector<double>& values,
                                               const std::vector<std::uint16_t>& dq,
                                               const Ref<Index>& index,
                                               const Ref<acsd::FootprintSet>& footprints,
                                               const std::string& frame, double epoch) {
  auto value_array = make_array<double>(ctx, DType::f64, values,
                                        {static_cast<U64>(values.size())}, index,
                                        unit_of("ADU"));
  auto dq_array = make_array<std::uint16_t>(ctx, DType::u16, dq,
                                            {static_cast<U64>(dq.size())}, index,
                                            dimensionless());
  if (value_array.code != Code::ok || dq_array.code != Code::ok) return {};
  std::vector<double> lower(values.size());
  std::vector<double> upper(values.size());
  for (std::size_t i = 0; i < values.size(); ++i) {
    const double delta = std::max(1e-12, std::fabs(values[i]) * 1e-12);
    lower[i] = values[i] - delta;
    upper[i] = values[i] + delta;
  }
  auto low = make_array<double>(ctx, DType::f64, lower,
                                {static_cast<U64>(values.size())}, index, unit_of("ADU"));
  auto high = make_array<double>(ctx, DType::f64, upper,
                                 {static_cast<U64>(values.size())}, index, unit_of("ADU"));
  auto observation = std::make_shared<NativeObservation>();
  observation->integrated_values = value_array.value->array;
  observation->dq = dq_array.value->array;
  observation->native_index = index;
  observation->footprints = footprints;
  observation->unit = unit_of("ADU");
  observation->frame = frame;
  observation->physical_collection = true;
  observation->context.epoch_julian_year = epoch;
  observation->context.observation_jd_tdb = 2457500.5;
  observation->identity = identity_of("native:" + frame + ":synthetic", "acsd.NativeObservation");
  observation->assessment.complete = true;
  if (low.code == Code::ok) observation->numerical_lower = low.value->array;
  if (high.code == Code::ok) observation->numerical_upper = high.value->array;
  return observation;
}

struct Chain {
  std::shared_ptr<rt::Services> services;
  std::shared_ptr<rt::ProcessHost> host;
  rt::InvocationContext command;
};

// Starts the unique services, registers the shared types and the requested
// module registrations, freezes the registry and opens a host.  `register_codes`
// records every step so a driver can report a real refusal instead of guessing.
struct ChainResult {
  Result<Chain> chain;
  std::vector<std::pair<std::string, Code>> steps;
};

using Registrar = Result<rt::Done> (*)(rt::RegistryBuilder&);

inline ChainResult start(const std::vector<std::pair<std::string, Registrar>>& registrars,
                         acsd::U64 host_bytes = 1ull << 30, acsd::U64 work = 64000000) {
  ChainResult out;
  auto services = vrt::start_services(host_bytes, work, 2, 64, 8,
                                      vrt::scratch_root("process-m5"));
  out.steps.emplace_back("services", services.code);
  if (services.code != Code::ok || !services.value) {
    out.chain.code = services.code;
    return out;
  }
  auto command = rt::open_command(*services.value, "validation.process.command",
                                  vrt::limits(host_bytes, work, 1ull << 20, 2, 64));
  out.steps.emplace_back("open_command", command.code);
  if (command.code != Code::ok) {
    out.chain.code = command.code;
    return out;
  }
  rt::RegistryBuilder builder;
  auto types = acsd::generated::register_builtin_types(builder);
  out.steps.emplace_back("register_builtin_types", types.code);
  for (const auto& [name, registrar] : registrars) {
    auto registered = registrar(builder);
    out.steps.emplace_back(name, registered.code);
  }
  auto frozen = builder.freeze();
  out.steps.emplace_back("freeze", frozen.code);
  if (frozen.code != Code::ok || !frozen.value) {
    out.chain.code = frozen.code;
    return out;
  }
  auto host = rt::make_host(*services.value, *frozen.value);
  out.steps.emplace_back("make_host", host.code);
  if (host.code != Code::ok || !host.value) {
    out.chain.code = host.code;
    return out;
  }
  Chain chain;
  chain.services = *services.value;
  chain.host = *host.value;
  chain.command = *command.value;
  out.chain.code = Code::ok;
  out.chain.value = std::move(chain);
  return out;
}

template <class In, class Out>
Result<Out> invoke(const Chain& chain, const char* process, const char* operation,
                   Ref<In> input) {
  auto task = chain.host->invoke<In, Out>(chain.command, process, operation, input);
  return rt::wait_external(std::move(task));
}

}  // namespace vpf
