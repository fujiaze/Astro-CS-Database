#pragma once
// Try-only performance spans and intermediate-data snapshots.
//
// ACSD_DEV_MODE is the single compile switch.  When it is OFF the macros
// evaluate no argument at all, including arguments that do not exist in this
// build.  Diagnostics never change a scientific code, model or hash.
#include <acsd/runtime/types.hpp>

#include <optional>
#include <string_view>
#include <variant>

namespace acsd::rt {

struct ProbeTags {
  std::optional<std::string_view> frame, component, model_hash;
  std::optional<HealpixLeaf> leaf;
  std::optional<Box> roi;
  U64 attempt = 0;
};

enum class PerfKind { span, counter, gauge };

struct PerfRecord {
  std::string_view probe_id;
  ProbeTags tags;
  PerfKind kind = PerfKind::span;
  U64 start_steady_ns = 0;
  std::optional<U64> wall_ns, cpu_ns, count, bytes, span_id, parent_span_id;
  std::optional<double> gauge;
};

struct SnapshotField {
  std::string_view name, unit;
  std::variant<std::monostate, double, U64, bool, std::string_view,
               std::reference_wrapper<const ExactInterval>>
      value;
};

struct SnapshotView {
  std::string_view name, index_hash;
  DType dtype = DType::f64;
  BlockRequest range{};
  std::span<const U64> shape;
  std::span<const std::byte> bytes;
  std::reference_wrapper<const Unit> unit;
  std::reference_wrapper<const Lease> source_lease;
};

struct SnapshotPayload {
  std::span<const SnapshotView> views;
  std::span<const SnapshotField> fields;
  std::string_view coordinate_space, index_hash;
};

bool probe_selected(const InvocationContext&, std::string_view,
                    const ProbeTags&) noexcept;
void try_record(const InvocationContext&, const PerfRecord&) noexcept;
void try_snapshot(const InvocationContext&, std::string_view,
                  const ProbeTags&, const SnapshotPayload&) noexcept;
U64 allocate_span_id(const InvocationContext&) noexcept;
U64 steady_ns() noexcept;
void probe_construction_failed() noexcept;

}  // namespace acsd::rt

#if defined(ACSD_DEV_MODE) && ACSD_DEV_MODE
#define ACSD_DEV_SNAPSHOT(ctx, id, tags, payload)                            \
  do {                                                                       \
    try {                                                                    \
      auto&& acsd_probe_ctx = (ctx);                                         \
      auto&& acsd_probe_id = (id);                                           \
      auto&& acsd_probe_tags = (tags);                                       \
      if (::acsd::rt::probe_selected(acsd_probe_ctx, acsd_probe_id,          \
                                     acsd_probe_tags))                       \
        ::acsd::rt::try_snapshot(acsd_probe_ctx, acsd_probe_id,              \
                                 acsd_probe_tags, (payload));                \
    } catch (...) {                                                          \
      ::acsd::rt::probe_construction_failed();                               \
    }                                                                        \
  } while (false)
#define ACSD_DEV_PERF(ctx, record)                                           \
  do {                                                                       \
    try {                                                                    \
      ::acsd::rt::try_record((ctx), (record));                               \
    } catch (...) {                                                          \
      ::acsd::rt::probe_construction_failed();                               \
    }                                                                        \
  } while (false)
#else
#define ACSD_DEV_SNAPSHOT(ctx, id, tags, payload) \
  do {                                            \
  } while (false)
#define ACSD_DEV_PERF(ctx, record) \
  do {                             \
  } while (false)
#endif
