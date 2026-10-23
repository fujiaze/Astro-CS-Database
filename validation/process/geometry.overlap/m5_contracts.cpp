// M5 typed-process validation: the five processes are registered through the
// generated TypeCodecs and the real runtime ProcessHost, then every operation
// is exercised through the same registry path (not a direct free-function
// call).  Each process also gets one negative request per operation; the
// positive fixtures are the smallest scientific inputs that make the existing
// implementations run.
//
// The observed JSON files are written next to each process directory.  They
// are the raw result of this program; nothing in them is hand-authored.
#include <acsd/modules/geometry.hpp>
#include <acsd/runtime/generated_manifest.hpp>
#include <acsd/modules/generated/status.hpp>
#include <acsd/modules/operators.hpp>
#include <acsd/modules/projection.hpp>
#include <acsd/modules/uncertainty.hpp>
#include <acsd/runtime/memory.hpp>
#include <acsd/runtime/process.hpp>
#include <acsd/runtime/task.hpp>

#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "coordinates/healpix.hpp"
#include "model/array.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "runtime/internal.hpp"

using namespace acsd;
using namespace acsd::modules;
using rt::Done;
using rt::InvocationContext;
using rt::ScopeLimits;
using rt::ResourceVector;
using rt::ArenaRef;
using rt::PlanOptions;

namespace {

int checks = 0;
int failures = 0;
std::string observations;

void check(bool condition, const std::string& name,
           const std::string& detail = {}) {
  ++checks;
  if (condition) return;
  ++failures;
  std::cerr << "FAIL: " << name;
  if (!detail.empty()) std::cerr << " [" << detail << "]";
  std::cerr << "\n";
}

void observe(const std::string& process, const std::string& operation,
             const std::string& field, const std::string& value) {
  observations += "{\"process\":\"" + process + "\",\"operation\":\"" +
                  operation + "\",\"field\":\"" + field + "\",\"value\":" +
                  value + "}\n";
}

const char* code_name(Code code) {
  switch (code) {
    case Code::ok: return "ok";
    case Code::invalid_request: return "invalid_request";
    case Code::invalid_model: return "invalid_model";
    case Code::domain: return "domain";
    case Code::unsupported: return "unsupported";
    case Code::unavailable: return "unavailable";
    case Code::unsolved: return "unsolved";
    case Code::not_identifiable: return "not_identifiable";
    case Code::invalid_covariance: return "invalid_covariance";
    case Code::duplicate_source: return "duplicate_source";
    case Code::identity_conflict: return "identity_conflict";
    case Code::hash_mismatch: return "hash_mismatch";
    case Code::corrupt_product: return "corrupt_product";
    case Code::missing_dependency: return "missing_dependency";
    case Code::no_support: return "no_support";
    case Code::support_unresolved: return "support_unresolved";
    case Code::geometry_solve_unresolved: return "geometry_solve_unresolved";
    case Code::selection_unresolved: return "selection_unresolved";
    case Code::nonconverged: return "nonconverged";
    case Code::precision_exhausted: return "precision_exhausted";
    case Code::resource_exhausted: return "resource_exhausted";
    case Code::io_error: return "io_error";
    case Code::cancelled: return "cancelled";
    case Code::internal: return "internal";
  }
  return "unknown";
}

template <class T>
bool good(const Result<T>& result) {
  return result.code == Code::ok && result.value.has_value();
}

template <class T>
const T& value_of(const Result<T>& result) {
  return *result.value;
}

HeaderFields make_header() {
  HeaderFields header;
  header.push_back({"CTYPE1", std::string("RA---TAN")});
  header.push_back({"CTYPE2", std::string("DEC--TAN")});
  header.push_back({"CRPIX1", 17.0});
  header.push_back({"CRPIX2", 17.0});
  header.push_back({"CRVAL1", 210.0});
  header.push_back({"CRVAL2", 34.0});
  header.push_back({"RADESYS", std::string("ICRS")});
  header.push_back({"CD1_1", 0.01});
  header.push_back({"CD1_2", 0.0});
  header.push_back({"CD2_1", 0.0});
  header.push_back({"CD2_2", 0.01});
  return header;
}

struct Harness {
  std::shared_ptr<rt::Services> services;
  InvocationContext command;
  std::shared_ptr<const rt::Registry> registry;
  std::shared_ptr<rt::ProcessHost> host;
  rt::ArenaRef arena;
  Ref<WcsModel> wcs;
  U64 array_counter = 0;

  std::string id(const std::string& stem) {
    ++array_counter;
    return "sha256:" + model::sha256_hex(stem + ":" +
                                         std::to_string(array_counter));
  }
};

std::vector<std::byte> bytes_f64(const std::vector<double>& values) {
  std::vector<std::byte> bytes(values.size() * sizeof(double));
  if (!values.empty())
    std::memcpy(bytes.data(), values.data(), bytes.size());
  return bytes;
}

std::vector<std::byte> bytes_u64(const std::vector<U64>& values) {
  std::vector<std::byte> bytes(values.size() * sizeof(U64));
  if (!values.empty())
    std::memcpy(bytes.data(), values.data(), bytes.size());
  return bytes;
}

std::vector<std::byte> bytes_u8(const std::vector<std::uint8_t>& values) {
  std::vector<std::byte> bytes(values.size());
  for (std::size_t i = 0; i < values.size(); ++i)
    bytes[i] = static_cast<std::byte>(values[i]);
  return bytes;
}

Ref<Array> make_array(Harness& harness, DType dtype,
                      const std::vector<U64>& shape,
                      std::vector<std::byte> bytes, const Unit& unit,
                      const Id& index_id) {
  const Id object_id = harness.id("array");
  auto data = std::make_shared<const std::vector<std::byte>>(std::move(bytes));
  harness.services->blocks->register_array(object_id, data, dtype, shape);
  auto array = std::make_shared<Array>();
  array->identity = model::make_identity("acsd.Array", 2, 0,
                                         object_id.substr(7), "acsd.m5");
  array->identity.object_id = object_id;
  array->assessment = model::make_assessment(Code::ok);
  array->assessment.complete = true;
  array->dtype = dtype;
  array->shape = shape;
  array->unit = unit;
  array->index_id = index_id;
  return array;
}

Ref<Index> make_index(Harness& harness, U64 size, Index::Kind kind = Index::Kind::native) {
  auto index = std::make_shared<Index>();
  index->identity = model::make_identity("acsd.Index", 2, 0,
                                         harness.id("index").substr(7),
                                         "acsd.m5");
  index->assessment = model::make_assessment(Code::ok);
  index->assessment.complete = true;
  index->size = size;
  index->kind = kind;
  index->order = 0;
  index->namespace_id = "acsd.m5";
  for (U64 row = 0; row < size; ++row)
    index->sources.push_back(SourceIdentity{"m5", "exposure", "detector",
                                            "channel", row});
  return index;
}

Unit unit_adu() { return model::unit_adu("m5.adu"); }
Unit unit_sr() { return model::unit_sr(); }
Unit unit_dimensionless() { return model::unit_dimensionless(); }

// ---------------------------------------------------------------------------
// Registry and host construction
// ---------------------------------------------------------------------------

bool build_harness(Harness& harness) {
  rt::RegistryBuilder builder;
  Result<Done> step = generated::register_builtin_types(builder);
  check(good(step), "registry: register_builtin_types", code_name(step.code));
  if (!good(step)) return false;

  const std::array<Result<Done> (*)(rt::RegistryBuilder&), 5> registrations = {
      &register_geometry_overlap, &register_operators_action,
      &register_uncertainty_covariance, &register_geometry_drizzle,
      &register_projection_export};
  for (auto registration : registrations) {
    step = registration(builder);
    check(good(step), "registry: register process", code_name(step.code));
    if (!good(step)) return false;
  }
  Result<std::shared_ptr<const rt::Registry>> frozen = builder.freeze();
  check(good(frozen), "registry: freeze", code_name(frozen.code));
  if (!good(frozen)) return false;
  harness.registry = value_of(frozen);

  const std::vector<std::pair<std::string, std::vector<std::string>>> expected = {
      {"geometry.overlap",
       {"make_footprint_provider", "detector_footprint", "footprint", "measure",
        "compute_overlap", "refine_geometry", "footprint.forward_interval",
        "footprint.boundary_interval", "footprint.derivative_interval",
        "refinable.open", "refinable.refine"}},
      {"operators.action",
       {"make_action", "apply", "transpose_apply", "compose",
        "action.dependency_blocks", "action.serialize_recipe",
        "action.row_enclosure"}},
      {"uncertainty.covariance",
       {"make_covariance", "covariance_apply", "quadratic_form",
        "geometry_covariance_bound", "control_covariance"}},
      {"geometry.drizzle", {"build_drizzle"}},
      {"projection.export", {"build_export"}},
  };
  for (const auto& process : expected) {
    for (const std::string& operation : process.second) {
      check(harness.registry->has(process.first, operation),
            "registry: has " + process.first + "/" + operation);
    }
  }

  rt::PoolConfig config;
  config.cpu_threads = 2;
  config.read_slots = 1;
  config.write_slots = 1;
  config.process.ceiling.host_bytes = 512ull << 20;
  config.process.ceiling.cpu_tokens = 2;
  config.process.max_work = 4000000;
  Result<std::shared_ptr<rt::Services>> services = rt::make_services(config);
  check(good(services), "host: make_services", code_name(services.code));
  if (!good(services)) return false;
  harness.services = value_of(services);

  ScopeLimits limits;
  limits.ceiling.host_bytes = 256ull << 20;
  limits.max_work = 4000000;
  Result<InvocationContext> command =
      rt::open_command(harness.services, "m5.command", limits, 1);
  check(good(command), "host: open_command", code_name(command.code));
  if (!good(command)) return false;
  harness.command = value_of(command);

  ResourceVector fixture_capacity;
  fixture_capacity.host_bytes = 64ull << 20;
  Result<rt::ArenaRef> arena = rt::reserve(harness.command, fixture_capacity);
  check(good(arena), "host: fixture arena", code_name(arena.code));
  if (!good(arena)) return false;
  harness.arena = value_of(arena);

  Result<std::shared_ptr<rt::ProcessHost>> host =
      rt::make_host(harness.services, harness.registry);
  check(good(host), "host: make_host", code_name(host.code));
  if (!good(host)) return false;
  harness.host = value_of(host);
  return true;
}

template <class In, class Out>
Result<Out> invoke(Harness& harness, const char* process, const char* operation,
                   Ref<In> input) {
  return rt::wait_external(
      harness.host->invoke<In, Out>(harness.command, process, operation, input));
}

template <class In, class Out>
void negative(Harness& harness, const char* process, const char* operation) {
  Result<Out> result = invoke<In, Out>(harness, process, operation, Ref<In>{});
  check(result.code == Code::invalid_request,
        std::string("negative: ") + process + "/" + operation,
        code_name(result.code));
  observe(process, operation, "negative_code",
          "\"" + std::string(code_name(result.code)) + "\"");
}

double elapsed_seconds(const Result<Done>&) { return 0.0; }

// ---------------------------------------------------------------------------
// geometry.overlap
// ---------------------------------------------------------------------------

struct TestRefinable final : RefinableGeometryProvider {
  Ref<GeometrySnapshot> snapshot;
  Result<Ref<GeometrySnapshot>> open(Context&, Id) const override {
    Result<Ref<GeometrySnapshot>> out;
    out.code = Code::ok;
    out.value = snapshot;
    return out;
  }
  Result<Ref<GeometryRevision>> refine(Context&, Ref<GeometrySnapshot> base,
                                       const GeometryBudget&) const override {
    Result<Ref<GeometryRevision>> out;
    out.code = Code::ok;
    auto revision = std::make_shared<GeometryRevision>();
    revision->base = base;
    revision->next = snapshot;
    revision->identity = model::make_identity("acsd.GeometryRevision", 2, 0,
                                              "m5-revision", "acsd.m5");
    revision->assessment = model::make_assessment(Code::ok);
    out.value = revision;
    return out;
  }
};

void geometry_checks(Harness& harness) {
  const char* process = "geometry.overlap";

  // Negative: a null typed input is rejected by validate before run.
  negative<MakeFootprintProviderInput, Ref<FootprintProvider>>(
      harness, process, "make_footprint_provider");
  negative<DetectorFootprintInput, Ref<DetectorFootprint>>(
      harness, process, "detector_footprint");
  negative<FootprintInput, Ref<CurvedBoundary>>(harness, process, "footprint");
  negative<MeasureInput, SphericalAreaInterval>(harness, process, "measure");
  negative<ComputeOverlapInput, Ref<Overlaps>>(harness, process,
                                               "compute_overlap");
  negative<RefineGeometryInput, Ref<GeometryRevision>>(
      harness, process, "refine_geometry");
  negative<FootprintForwardIntervalInput, EnclosedSkyRegion>(
      harness, process, "footprint.forward_interval");
  negative<FootprintBoundaryIntervalInput, EnclosedSkyRegion>(
      harness, process, "footprint.boundary_interval");
  negative<FootprintDerivativeIntervalInput, DerivativeResult>(
      harness, process, "footprint.derivative_interval");
  negative<RefinableOpenInput, Ref<GeometrySnapshot>>(harness, process,
                                                      "refinable.open");
  negative<RefinableRefineInput, Ref<GeometryRevision>>(harness, process,
                                                        "refinable.refine");

  // Positive: a TAN WCS built once through the public science bridge.
  {
    Result<std::shared_ptr<rt::detail::ScienceContext>> bridge =
        rt::detail::make_science_context(harness.command, harness.arena);
    check(good(bridge), "geometry: science bridge");
    if (!good(bridge)) return;
    acsd::Context& science = (*bridge.value)->context();
    NumericBudget budget;
    budget.tolerance.absolute = 1e-10;
    budget.tolerance.relative = 1e-12;
    budget.max_iterations = 64;
    budget.arithmetic.max_bits = 256;
    AstrometricContext astrometric;
    const Box domain{{-0.5, -0.5}, {33.5, 33.5}};
    Result<Ref<WcsModel>> wcs = make_wcs(science, make_header(), domain,
                                         astrometric, "detector-m5", budget);
    check(good(wcs), "geometry: make_wcs", code_name(wcs.code));
    if (good(wcs)) harness.wcs = value_of(wcs);
  }
  if (!harness.wcs) return;

  auto provider_input = std::make_shared<const MakeFootprintProviderInput>(
      MakeFootprintProviderInput{harness.wcs});
  Result<Ref<FootprintProvider>> provider =
      invoke<MakeFootprintProviderInput, Ref<FootprintProvider>>(
          harness, process, "make_footprint_provider", provider_input);
  check(good(provider), "geometry positive: make_footprint_provider",
        code_name(provider.code));
  observe(process, "make_footprint_provider", "run_code",
          "\"" + std::string(code_name(provider.code)) + "\"");
  if (!good(provider)) return;

  // detector_footprint from a one-source FootprintSet.
  auto index = make_index(harness, 1);
  auto footprints = std::make_shared<FootprintSet>();
  footprints->identity = model::make_identity("acsd.FootprintSet", 2, 0,
                                              "m5-footprints", "acsd.m5");
  footprints->assessment = model::make_assessment(Code::ok);
  footprints->index = index;
  footprints->width = 1;
  footprints->height = 1;
  footprints->detector_from_sample = Mat2{1.0, 0.0, 0.0, 1.0};
  footprints->offset = Vec2{0.0, 0.0};
  footprints->local_cell = Box{{-0.5, -0.5}, {0.5, 0.5}};
  footprints->wcs = harness.wcs;
  footprints->detector_frame = "detector-m5";
  auto detector_input = std::make_shared<const DetectorFootprintInput>(
      DetectorFootprintInput{footprints, 0});
  Result<Ref<DetectorFootprint>> detector =
      invoke<DetectorFootprintInput, Ref<DetectorFootprint>>(
          harness, process, "detector_footprint", detector_input);
  check(good(detector), "geometry positive: detector_footprint",
        code_name(detector.code));
  observe(process, "detector_footprint", "run_code",
          "\"" + std::string(code_name(detector.code)) + "\"");
  if (!good(detector)) return;

  GeometryBudget budget;
  budget.max_work = 600;
  budget.max_depth = 6;
  budget.area_absolute_sr = 1e-6;
  budget.arithmetic.initial_bits = 64;
  budget.arithmetic.max_bits = 256;

  auto measure_input = std::make_shared<const MeasureInput>(
      MeasureInput{*provider.value, *detector.value, budget});
  Result<rt::InvocationPlan> measure_plan = plan_geometry_overlap_measure(
      harness.command, measure_input, rt::PlanOptions{16, 1, rt::Placement::memory});
  check(good(measure_plan), "geometry: measure plan", code_name(measure_plan.code));
  if (good(measure_plan)) {
    observe(process, "measure", "plan_r_bytes",
            std::to_string(measure_plan.value->persistent.host_bytes));
    observe(process, "measure", "plan_f_bytes",
            std::to_string(measure_plan.value->progress_floor.host_bytes));
    observe(process, "measure", "plan_work",
            std::to_string(measure_plan.value->elements));
  }
  Result<SphericalAreaInterval> measured =
      invoke<MeasureInput, SphericalAreaInterval>(harness, process, "measure",
                                                  measure_input);
  check(good(measured), "geometry positive: measure",
        code_name(measured.code));
  if (good(measured)) {
    observe(process, "measure", "lower_sr",
            std::to_string(measured.value->lower_sr));
    observe(process, "measure", "upper_sr",
            std::to_string(measured.value->upper_sr));
    check(measured.value->lower_sr <= measured.value->upper_sr &&
              measured.value->lower_sr >= 0.0,
          "geometry: measure interval ordered");
  }

  auto forward_input = std::make_shared<const FootprintForwardIntervalInput>(
      FootprintForwardIntervalInput{*provider.value,
                                    Box{{0.0, 0.0}, {8.0, 8.0}}});
  Result<EnclosedSkyRegion> forward =
      invoke<FootprintForwardIntervalInput, EnclosedSkyRegion>(
          harness, process, "footprint.forward_interval", forward_input);
  check(good(forward), "geometry positive: forward_interval",
        code_name(forward.code));
  observe(process, "footprint.forward_interval", "run_code",
          "\"" + std::string(code_name(forward.code)) + "\"");

  auto derivative_input =
      std::make_shared<const FootprintDerivativeIntervalInput>(
          FootprintDerivativeIntervalInput{*provider.value,
                                           Box{{0.0, 0.0}, {8.0, 8.0}}});
  Result<DerivativeResult> derivative =
      invoke<FootprintDerivativeIntervalInput, DerivativeResult>(
          harness, process, "footprint.derivative_interval", derivative_input);
  check(good(derivative), "geometry positive: derivative_interval",
        code_name(derivative.code));
  observe(process, "footprint.derivative_interval", "run_code",
          "\"" + std::string(code_name(derivative.code)) + "\"");

  auto footprint_input = std::make_shared<const FootprintInput>(
      FootprintInput{*provider.value, *detector.value, 1.0, budget});
  Result<Ref<CurvedBoundary>> boundary =
      invoke<FootprintInput, Ref<CurvedBoundary>>(harness, process, "footprint",
                                                  footprint_input);
  check(good(boundary), "geometry positive: footprint",
        code_name(boundary.code));
  observe(process, "footprint", "run_code",
          "\"" + std::string(code_name(boundary.code)) + "\"");
  if (good(boundary))
    observe(process, "footprint", "curved_edges",
            std::to_string((*boundary.value)->rings.empty()
                               ? 0
                               : (*boundary.value)->rings.front().size()));
  if (good(boundary) && !(*boundary.value)->rings.empty() &&
      !(*boundary.value)->rings.front().empty()) {
    auto boundary_input =
        std::make_shared<const FootprintBoundaryIntervalInput>(
            FootprintBoundaryIntervalInput{*provider.value,
                                           (*boundary.value)->rings.front().front(),
                                           Interval(0.0, 1.0)});
    Result<EnclosedSkyRegion> enclosed =
        invoke<FootprintBoundaryIntervalInput, EnclosedSkyRegion>(
            harness, process, "footprint.boundary_interval", boundary_input);
    check(good(enclosed), "geometry positive: boundary_interval",
          code_name(enclosed.code));
    observe(process, "footprint.boundary_interval", "run_code",
            "\"" + std::string(code_name(enclosed.code)) + "\"");
  }

  auto overlap_input = std::make_shared<const ComputeOverlapInput>(
      ComputeOverlapInput{*provider.value, *detector.value, 0, 1.0, budget});
  Result<Ref<Overlaps>> overlaps =
      invoke<ComputeOverlapInput, Ref<Overlaps>>(
          harness, process, "compute_overlap", overlap_input);
  check(good(overlaps), "geometry positive: compute_overlap",
        code_name(overlaps.code));
  observe(process, "compute_overlap", "run_code",
          "\"" + std::string(code_name(overlaps.code)) + "\"");
  if (good(overlaps))
    observe(process, "compute_overlap", "contributions",
            std::to_string((*overlaps.value)->contributions.size()));

  // refinable geometry with an explicit provider implementation.
  auto snapshot = std::make_shared<GeometrySnapshot>();
  snapshot->identity = model::make_identity("acsd.GeometrySnapshot", 2, 0,
                                            "m5-snapshot", "acsd.m5");
  snapshot->assessment = model::make_assessment(Code::ok);
  auto refinable = std::make_shared<TestRefinable>();
  refinable->snapshot = snapshot;
  Ref<RefinableGeometryProvider> refinable_provider = refinable;

  auto open_input = std::make_shared<const RefinableOpenInput>(
      RefinableOpenInput{refinable_provider, "snapshot:m5"});
  Result<Ref<GeometrySnapshot>> opened =
      invoke<RefinableOpenInput, Ref<GeometrySnapshot>>(
          harness, process, "refinable.open", open_input);
  check(good(opened), "geometry positive: refinable.open",
        code_name(opened.code));
  observe(process, "refinable.open", "run_code",
          "\"" + std::string(code_name(opened.code)) + "\"");

  auto refine_input = std::make_shared<const RefinableRefineInput>(
      RefinableRefineInput{refinable_provider, snapshot, budget});
  Result<Ref<GeometryRevision>> refined =
      invoke<RefinableRefineInput, Ref<GeometryRevision>>(
          harness, process, "refinable.refine", refine_input);
  check(good(refined), "geometry positive: refinable.refine",
        code_name(refined.code));
  observe(process, "refinable.refine", "run_code",
          "\"" + std::string(code_name(refined.code)) + "\"");

  auto refine_geometry_input = std::make_shared<const RefineGeometryInput>(
      RefineGeometryInput{refinable_provider, snapshot, budget});
  Result<Ref<GeometryRevision>> refined2 =
      invoke<RefineGeometryInput, Ref<GeometryRevision>>(
          harness, process, "refine_geometry", refine_geometry_input);
  check(good(refined2), "geometry positive: refine_geometry",
        code_name(refined2.code));
  observe(process, "refine_geometry", "run_code",
          "\"" + std::string(code_name(refined2.code)) + "\"");
}



Ref<Index> make_leaf_index(Harness& harness, U32 order, U64 ipix) {
  auto index = std::make_shared<Index>();
  index->identity = model::make_identity("acsd.Index", 2, 0,
                                         harness.id("leaf-index").substr(7),
                                         "acsd.m5");
  index->assessment = model::make_assessment(Code::ok);
  index->assessment.complete = true;
  index->size = 1;
  index->kind = Index::Kind::leaf;
  index->order = order;
  index->namespace_id = "acsd.m5";
  index->ids = make_array(harness, DType::u64, {2}, bytes_u64({order, ipix}),
                          unit_dimensionless(), "acsd.m5.ids");
  return index;
}

class TestProductProvider final : public ProductProvider {
 public:
  explicit TestProductProvider(Ref<ProductGraph> graph) : graph_(std::move(graph)) {}
  Ref<ProductGraph> graph() const override { return graph_; }

  Result<std::vector<Id>> list_frames(Context&) const override { return unsupported<std::vector<Id>>(); }
  Result<Ref<FrameDescription>> describe_frame(Context&, Id) const override {
    return unsupported<Ref<FrameDescription>>();
  }
  Result<LeafBlock> read_leaf_block(Context&, const FrameReadRequest&) const override {
    return unsupported<LeafBlock>();
  }
  Result<std::vector<ArrayRef>> read_native_dependencies(
      Context&, std::span<const SourceIdentity>) const override {
    return unsupported<std::vector<ArrayRef>>();
  }
  Result<Ref<LinearAction>> open_action(Context&, Id) const override {
    return unsupported<Ref<LinearAction>>();
  }
  Result<Ref<SupportMap>> query_support(Context&, Id, Ref<Index>) const override {
    return unsupported<Ref<SupportMap>>();
  }
  Result<Ref<RefinableGeometryProvider>> open_refinable_geometry(Context&, Id) const override {
    return unsupported<Ref<RefinableGeometryProvider>>();
  }
  Result<ArrayView> read_array(Context&, ArrayRef, BlockRequest) const override {
    return unsupported<ArrayView>();
  }

 private:
  template <class T>
  static Result<T> unsupported() {
    return model::fail<T>(Code::unsupported, "the test product does not provide this capability",
                          "m5.product", "the fixture only exposes graph()");
  }
  Ref<ProductGraph> graph_;
};

std::vector<double> read_f64_values(Harness& harness, const ArrayRef& array) {
  std::vector<double> values;
  if (!array) return values;
  auto bytes = harness.services->blocks->find(array->identity.object_id);
  if (!bytes || bytes->size() % sizeof(double) != 0) return values;
  values.resize(bytes->size() / sizeof(double));
  std::memcpy(values.data(), bytes->data(), bytes->size());
  return values;
}

Ref<LinearAction> make_identity_action(Harness& harness, U64 size) {
  auto index = make_index(harness, size);
  auto recipe = std::make_shared<ActionRecipe>();
  recipe->kind = ActionRecipe::Kind::identity;
  recipe->algorithm = "m5.identity";
  recipe->version = 1;
  recipe->spaces.domain = index;
  recipe->spaces.codomain = index;
  recipe->spaces.input = unit_dimensionless();
  recipe->spaces.output = unit_dimensionless();
  auto children = std::make_shared<std::vector<Ref<LinearAction>>>();
  auto arrays = std::make_shared<std::vector<ArrayRef>>();
  auto input = std::make_shared<const MakeActionInput>(
      MakeActionInput{*recipe, children, arrays});
  Result<Ref<LinearAction>> result = invoke<MakeActionInput, Ref<LinearAction>>(
      harness, "operators.action", "make_action", input);
  if (!good(result)) return {};
  return *result.value;
}

Ref<LinearAction> make_csr_action(Harness& harness) {
  auto domain = make_index(harness, 2);
  auto codomain = make_index(harness, 2);
  auto offsets = make_array(harness, DType::u64, {3}, bytes_u64({0, 2, 3}),
                            unit_dimensionless(), domain->identity.object_id);
  auto indices = make_array(harness, DType::u64, {3}, bytes_u64({0, 1, 1}),
                            unit_dimensionless(), domain->identity.object_id);
  auto values = make_array(harness, DType::f64, {3}, bytes_f64({1.0, 2.0, 3.0}),
                           unit_dimensionless(), domain->identity.object_id);
  auto recipe = std::make_shared<ActionRecipe>();
  recipe->kind = ActionRecipe::Kind::csr;
  recipe->algorithm = "m5.csr";
  recipe->version = 1;
  recipe->spaces.domain = domain;
  recipe->spaces.codomain = codomain;
  recipe->spaces.input = unit_dimensionless();
  recipe->spaces.output = unit_dimensionless();
  recipe->arrays = {"offsets", "indices", "values"};
  auto children = std::make_shared<std::vector<Ref<LinearAction>>>();
  auto arrays = std::make_shared<std::vector<ArrayRef>>(
      std::vector<ArrayRef>{offsets, indices, values});
  auto input = std::make_shared<const MakeActionInput>(
      MakeActionInput{*recipe, children, arrays});
  Result<Ref<LinearAction>> result = invoke<MakeActionInput, Ref<LinearAction>>(
      harness, "operators.action", "make_action", input);
  if (!good(result)) return {};
  return *result.value;
}

void operators_checks(Harness& harness) {
  const char* process = "operators.action";
  negative<MakeActionInput, Ref<LinearAction>>(harness, process, "make_action");
  negative<ApplyInput, ArrayRef>(harness, process, "apply");
  negative<TransposeApplyInput, ArrayRef>(harness, process, "transpose_apply");
  negative<ComposeInput, Ref<LinearAction>>(harness, process, "compose");
  negative<ActionDependencyBlocksInput, std::vector<Id>>(
      harness, process, "action.dependency_blocks");
  negative<ActionSerializeRecipeInput, ActionRecipe>(
      harness, process, "action.serialize_recipe");
  negative<ActionRowEnclosureInput, std::vector<ExactInterval>>(
      harness, process, "action.row_enclosure");

  Ref<LinearAction> identity = make_identity_action(harness, 2);
  check(identity != nullptr, "operators positive: identity make_action");
  Ref<LinearAction> csr = make_csr_action(harness);
  check(csr != nullptr, "operators positive: csr make_action");
  if (!identity || !csr) return;

  auto values = make_array(harness, DType::f64, {2}, bytes_f64({1.0, 1.0}),
                           unit_dimensionless(), "index:m5");
  auto apply_input =
      std::make_shared<const ApplyInput>(ApplyInput{csr, values});
  Result<ArrayRef> applied =
      invoke<ApplyInput, ArrayRef>(harness, process, "apply", apply_input);
  check(good(applied), "operators positive: apply", code_name(applied.code));
  if (good(applied)) {
    const std::vector<double> out = read_f64_values(harness, *applied.value);
    check(out.size() == 2 && std::fabs(out[0] - 3.0) < 1e-15 &&
              std::fabs(out[1] - 3.0) < 1e-15,
          "operators: apply exact [3,3]");
    observe(process, "apply", "values",
            "[" + std::to_string(out.empty() ? 0.0 : out[0]) + "," +
                std::to_string(out.size() > 1 ? out[1] : 0.0) + "]");
  }

  auto transpose_input = std::make_shared<const TransposeApplyInput>(
      TransposeApplyInput{csr, values});
  Result<ArrayRef> transposed = invoke<TransposeApplyInput, ArrayRef>(
      harness, process, "transpose_apply", transpose_input);
  check(good(transposed), "operators positive: transpose_apply",
        code_name(transposed.code));
  observe(process, "transpose_apply", "run_code",
          "\"" + std::string(code_name(transposed.code)) + "\"");
  if (good(transposed)) {
    const std::vector<double> out = read_f64_values(harness, *transposed.value);
    check(out.size() == 2 && std::fabs(out[0] - 1.0) < 1e-15 &&
              std::fabs(out[1] - 5.0) < 1e-15,
          "operators: transpose exact [1,5]");
  }

  auto compose_input =
      std::make_shared<const ComposeInput>(ComposeInput{csr, csr});
  Result<Ref<LinearAction>> composed = invoke<ComposeInput, Ref<LinearAction>>(
      harness, process, "compose", compose_input);
  check(good(composed), "operators positive: compose",
        code_name(composed.code));
  observe(process, "compose", "run_code",
          "\"" + std::string(code_name(composed.code)) + "\"");

  auto blocks_input = std::make_shared<const ActionDependencyBlocksInput>(
      ActionDependencyBlocksInput{csr, BlockRequest{0, 2}, false});
  Result<std::vector<Id>> blocks = invoke<ActionDependencyBlocksInput,
                                          std::vector<Id>>(
      harness, process, "action.dependency_blocks", blocks_input);
  check(good(blocks), "operators positive: dependency_blocks",
        code_name(blocks.code));
  if (good(blocks))
    observe(process, "action.dependency_blocks", "blocks",
            std::to_string(blocks.value->size()));

  auto recipe_input = std::make_shared<const ActionSerializeRecipeInput>(
      ActionSerializeRecipeInput{csr});
  Result<ActionRecipe> recipe = invoke<ActionSerializeRecipeInput, ActionRecipe>(
      harness, process, "action.serialize_recipe", recipe_input);
  check(good(recipe), "operators positive: serialize_recipe",
        code_name(recipe.code));
  observe(process, "action.serialize_recipe", "run_code",
          "\"" + std::string(code_name(recipe.code)) + "\"");

  auto row_input = std::make_shared<const ActionRowEnclosureInput>(
      ActionRowEnclosureInput{csr, 1});
  Result<std::vector<ExactInterval>> row =
      invoke<ActionRowEnclosureInput, std::vector<ExactInterval>>(
          harness, process, "action.row_enclosure", row_input);
  check(good(row), "operators positive: row_enclosure", code_name(row.code));
  observe(process, "action.row_enclosure", "run_code",
          "\"" + std::string(code_name(row.code)) + "\"");
  if (good(row))
    check(row.value->size() == 2, "operators: row enclosure covers codomain");

  // Plan R/F evidence for one representative row operation.
  Result<rt::InvocationPlan> plan = plan_operators_action_apply(
      harness.command, apply_input, PlanOptions{2, 1, rt::Placement::memory});
  if (good(plan)) {
    observe(process, "apply", "plan_r_bytes",
            std::to_string(plan.value->persistent.host_bytes));
    observe(process, "apply", "plan_f_bytes",
            std::to_string(plan.value->progress_floor.host_bytes));
    observe(process, "apply", "plan_work",
            std::to_string(plan.value->elements));
  }
}

void uncertainty_checks(Harness& harness) {
  const char* process = "uncertainty.covariance";
  negative<MakeCovarianceInput, Ref<CovarianceAction>>(
      harness, process, "make_covariance");
  negative<CovarianceApplyInput, ArrayRef>(harness, process,
                                           "covariance_apply");
  negative<QuadraticFormInput, Variance>(harness, process, "quadratic_form");
  negative<GeometryCovarianceBoundInput, BoundOrUnavailable>(
      harness, process, "geometry_covariance_bound");
  negative<ControlCovarianceInput, Ref<CovarianceAction>>(
      harness, process, "control_covariance");

  Ref<LinearAction> identity = make_identity_action(harness, 2);
  check(identity != nullptr, "uncertainty: identity action fixture");
  if (!identity) return;
  auto root = std::make_shared<NoiseRoot>();
  root->identity = model::make_identity("acsd.NoiseRoot", 2, 0, "m5-root",
                                        "acsd.m5");
  root->assessment = model::make_assessment(Code::ok);
  root->coordinates = make_index(harness, 2);
  root->unit = unit_adu();
  root->factor = identity;
  auto model = std::make_shared<NoiseModel>();
  model->identity = model::make_identity("acsd.NoiseModel", 2, 0, "m5-noise",
                                         "acsd.m5");
  model->assessment = model::make_assessment(Code::ok);
  model->variance_unit = model::unit_variance("m5.adu");
  model->innovations = {root};
  model->influence = {identity};
  model->state = NoiseState::known;

  auto covariance_input =
      std::make_shared<const MakeCovarianceInput>(MakeCovarianceInput{model});
  Result<Ref<CovarianceAction>> covariance =
      invoke<MakeCovarianceInput, Ref<CovarianceAction>>(
          harness, process, "make_covariance", covariance_input);
  check(good(covariance), "uncertainty positive: make_covariance",
        code_name(covariance.code));
  observe(process, "make_covariance", "run_code",
          "\"" + std::string(code_name(covariance.code)) + "\"");
  if (!good(covariance) && !covariance.assessment.issues.empty())
  if (!good(covariance)) return;

  auto values = make_array(harness, DType::f64, {2}, bytes_f64({1.0, 2.0}),
                           model::unit_dimensionless(), "index:m5");
  auto apply_input = std::make_shared<const CovarianceApplyInput>(
      CovarianceApplyInput{*covariance.value, values});
  Result<ArrayRef> applied = invoke<CovarianceApplyInput, ArrayRef>(
      harness, process, "covariance_apply", apply_input);
  check(good(applied), "uncertainty positive: covariance_apply",
        code_name(applied.code));
  observe(process, "covariance_apply", "run_code",
          "\"" + std::string(code_name(applied.code)) + "\"");

  auto functional = make_array(harness, DType::f64, {2}, bytes_f64({1.0, 2.0}),
                               model::unit_dimensionless(), "index:m5");
  auto quadratic_input = std::make_shared<const QuadraticFormInput>(
      QuadraticFormInput{*covariance.value, functional});
  Result<Variance> variance = invoke<QuadraticFormInput, Variance>(
      harness, process, "quadratic_form", quadratic_input);
  check(good(variance), "uncertainty positive: quadratic_form",
        code_name(variance.code));
  if (good(variance) && variance.value->value) {
    observe(process, "quadratic_form", "variance",
            std::to_string(*variance.value->value));
    // C = I for the identity factor and t = [1,2], so t^T C t = 5 exactly.
    check(std::fabs(*variance.value->value - 5.0) < 1e-15,
          "uncertainty: identity covariance quadratic form is 5");
  }

  auto bound_input = std::make_shared<const GeometryCovarianceBoundInput>(
      GeometryCovarianceBoundInput{*covariance.value, identity, identity,
                                   functional});
  Result<BoundOrUnavailable> bound =
      invoke<GeometryCovarianceBoundInput, BoundOrUnavailable>(
          harness, process, "geometry_covariance_bound", bound_input);
  check(good(bound), "uncertainty positive: geometry_covariance_bound",
        code_name(bound.code));
  if (good(bound))
    observe(process, "geometry_covariance_bound", "available",
            bound.value->bound ? "true" : "false");
  observe(process, "geometry_covariance_bound", "run_code",
          "\"" + std::string(code_name(bound.code)) + "\"");

  Ref<LinearAction> csr = make_csr_action(harness);
  check(csr != nullptr, "uncertainty: control action fixture");
  if (csr) {
    auto control_input = std::make_shared<const ControlCovarianceInput>(
        ControlCovarianceInput{*covariance.value, csr});
    Result<Ref<CovarianceAction>> control =
        invoke<ControlCovarianceInput, Ref<CovarianceAction>>(
            harness, process, "control_covariance", control_input);
    check(good(control), "uncertainty positive: control_covariance",
          code_name(control.code));
    observe(process, "control_covariance", "run_code",
            "\"" + std::string(code_name(control.code)) + "\"");
    if (good(control)) {
      // C = I and T = [[1,2],[0,3]], so T C T^T = [[5,6],[6,9]].
      auto e0 = make_array(harness, DType::f64, {2}, bytes_f64({1.0, 0.0}),
                           model::unit_dimensionless(), "index:m5");
      auto quadratic_controlled =
          std::make_shared<const QuadraticFormInput>(
              QuadraticFormInput{*control.value, e0});
      Result<Variance> controlled_variance =
          invoke<QuadraticFormInput, Variance>(
              harness, process, "quadratic_form", quadratic_controlled);
      check(good(controlled_variance),
            "uncertainty: controlled quadratic_form runs",
            code_name(controlled_variance.code));
      if (good(controlled_variance) && controlled_variance.value->value) {
        observe(process, "control_covariance", "quadratic_e0",
                std::to_string(*controlled_variance.value->value));
        check(std::fabs(*controlled_variance.value->value - 5.0) < 1e-15,
              "uncertainty: T C T^T quadratic form on e0 is 5");
      }
      auto applied_controlled =
          std::make_shared<const CovarianceApplyInput>(
              CovarianceApplyInput{*control.value, e0});
      Result<ArrayRef> controlled_array =
          invoke<CovarianceApplyInput, ArrayRef>(
              harness, process, "covariance_apply", applied_controlled);
      check(good(controlled_array), "uncertainty: controlled apply runs",
            code_name(controlled_array.code));
      if (good(controlled_array)) {
        const std::vector<double> column =
            read_f64_values(harness, *controlled_array.value);
        check(column.size() == 2 && std::fabs(column[0] - 5.0) < 1e-15 &&
                  std::fabs(column[1] - 6.0) < 1e-15,
              "uncertainty: (T C T^T) e0 is [5,6]");
        observe(process, "control_covariance", "column_e0",
                "[" + std::to_string(column.empty() ? 0.0 : column[0]) + "," +
                    std::to_string(column.size() > 1 ? column[1] : 0.0) + "]");
      }
    }
  }

  Result<rt::InvocationPlan> plan = plan_uncertainty_covariance_quadratic_form(
      harness.command, quadratic_input, PlanOptions{2, 1, rt::Placement::memory});
  if (good(plan)) {
    observe(process, "quadratic_form", "plan_r_bytes",
            std::to_string(plan.value->persistent.host_bytes));
    observe(process, "quadratic_form", "plan_f_bytes",
            std::to_string(plan.value->progress_floor.host_bytes));
    observe(process, "quadratic_form", "plan_work",
            std::to_string(plan.value->elements));
  }
}

void drizzle_export_registration_checks(Harness& harness) {
  // ---------------------------------------------------------------- drizzle
  {
    const char* process = "geometry.drizzle";
    negative<BuildDrizzleInput, DrizzleResult>(harness, process, "build_drizzle");
    auto overlaps = std::make_shared<Overlaps>();
    overlaps->identity = model::make_identity("acsd.Overlaps", 2, 0,
                                              "m5-overlaps", "acsd.m5");
    overlaps->assessment = model::make_assessment(Code::ok);
    OverlapContribution contribution;
    contribution.source = SourceIdentity{"m5", "exposure", "detector", "channel", 0};
    contribution.leaf = HealpixLeaf{0, 0};
    contribution.native_area = SphericalAreaInterval{1.0, 1.1};
    contribution.drop_area = SphericalAreaInterval{1.0, 1.0};
    contribution.overlap_area = SphericalAreaInterval{0.5, 0.6};
    overlaps->contributions.push_back(contribution);
    auto observation = std::make_shared<NativeObservation>();
    observation->identity = model::make_identity("acsd.NativeObservation", 2, 0,
                                                 "m5-native", "acsd.m5");
    observation->assessment = model::make_assessment(Code::ok);
    observation->integrated_values =
        make_array(harness, DType::f64, {1}, bytes_f64({2.0}), unit_adu(),
                   "index:m5");
    observation->native_index = make_index(harness, 1);
    DrizzleRequest drizzle_request;
    drizzle_request.order = 0;
    drizzle_request.pixfrac = 1.0;
    drizzle_request.budget.max_work = 2000;
    drizzle_request.budget.max_depth = 8;
    drizzle_request.budget.area_absolute_sr = 1e-9;
    drizzle_request.budget.arithmetic.initial_bits = 64;
    drizzle_request.budget.arithmetic.max_bits = 256;
    auto drizzle_input = std::make_shared<const BuildDrizzleInput>(
        BuildDrizzleInput{overlaps, observation, drizzle_request});
    Result<rt::InvocationPlan> drizzle_plan = plan_geometry_drizzle_build_drizzle(
        harness.command, drizzle_input, PlanOptions{1, 1, rt::Placement::memory});
    check(good(drizzle_plan), "drizzle: plan", code_name(drizzle_plan.code));
    if (good(drizzle_plan)) {
      observe(process, "build_drizzle", "plan_r_bytes",
              std::to_string(drizzle_plan.value->persistent.host_bytes));
      observe(process, "build_drizzle", "plan_f_bytes",
              std::to_string(drizzle_plan.value->progress_floor.host_bytes));
      observe(process, "build_drizzle", "plan_work",
              std::to_string(drizzle_plan.value->elements));
    }
    Result<DrizzleResult> drizzle =
        invoke<BuildDrizzleInput, DrizzleResult>(harness, process,
                                                 "build_drizzle", drizzle_input);
    check(good(drizzle), "drizzle positive: build_drizzle",
          code_name(drizzle.code));
    if (!good(drizzle) && !drizzle.assessment.issues.empty())
      std::cerr << "  drizzle reason: " << drizzle.assessment.issues.front().reason
                << std::endl;
    observe(process, "build_drizzle", "run_code",
            "\"" + std::string(code_name(drizzle.code)) + "\"");
    if (good(drizzle) && drizzle.value->estimate) {
      auto signal_bytes = harness.services->blocks->find(
          drizzle.value->estimate->signal->identity.object_id);
      std::vector<double> values;
      if (signal_bytes && signal_bytes->size() == sizeof(double)) {
        double v = 0.0;
        std::memcpy(&v, signal_bytes->data(), sizeof(double));
        values.push_back(v);
        observe(process, "build_drizzle", "signal_sr_per_pixel",
                std::to_string(v));
        // One leaf, one source sample: w = overlap/drop = [0.5,0.6],
        // numerator = w*x with x=2, normaliser = w*native_area with
        // Omega=[1.0,1.1].  The published interval must therefore contain the
        // nominal ratio x/Omega_mid = 2/1.05.
        auto lower_bytes = harness.services->blocks->find(
            drizzle.value->estimate->numerical_lower->identity.object_id);
        auto upper_bytes = harness.services->blocks->find(
            drizzle.value->estimate->numerical_upper->identity.object_id);
        double lower = 0.0;
        double upper = 0.0;
        if (lower_bytes && lower_bytes->size() == sizeof(double) &&
            upper_bytes && upper_bytes->size() == sizeof(double)) {
          std::memcpy(&lower, lower_bytes->data(), sizeof(double));
          std::memcpy(&upper, upper_bytes->data(), sizeof(double));
          observe(process, "build_drizzle", "signal_interval",
                  "[" + std::to_string(lower) + "," + std::to_string(upper) + "]");
          const double nominal = 2.0 / 1.05;
          check(lower <= nominal && nominal <= upper,
                "drizzle: same-leaf merge interval contains x/Omega");
        }
      }
      observe(process, "build_drizzle", "leaves",
              std::to_string(drizzle.value->op->rows->spaces.domain
                                 ? drizzle.value->op->rows->spaces.domain->size
                                 : 0));
    }
  }

  // ----------------------------------------------------------------- export
  {
    const char* process = "projection.export";
    negative<BuildExportInput, ExportResult>(harness, process, "build_export");
    if (!harness.wcs) return;
    auto leaf_index = make_leaf_index(harness, 0, 2);
    auto support = std::make_shared<SupportMap>();
    support->identity = model::make_identity("acsd.SupportMap", 2, 0,
                                             "m5-support", "acsd.m5");
    support->assessment = model::make_assessment(Code::ok);
    support->kind = SupportMap::Kind::primitives;
    support->index = leaf_index;
    support->states = make_array(harness, DType::u8, {1}, bytes_u8({1}),
                                 unit_dimensionless(), "acsd.m5.leaves");
    support->area_lower_sr = make_array(harness, DType::f64, {1},
                                        bytes_f64({1.0}), unit_sr(),
                                        "acsd.m5.leaves");
    support->area_upper_sr = make_array(harness, DType::f64, {1},
                                        bytes_f64({1.1}), unit_sr(),
                                        "acsd.m5.leaves");
    auto signal = std::make_shared<SignalEstimate>();
    signal->identity = model::make_identity("acsd.SignalEstimate", 2, 0,
                                            "m5-signal", "acsd.m5");
    signal->assessment = model::make_assessment(Code::ok);
    signal->signal = make_array(harness, DType::f64, {1}, bytes_f64({2.0}),
                                model::unit_sr_per_pixel(), "acsd.m5.leaves");
    signal->unit = model::unit_sr_per_pixel();
    signal->estimator.quantity = "surface_brightness";
    signal->estimator.unit = model::unit_sr_per_pixel();
    signal->support = support;
    auto graph = std::make_shared<ProductGraph>();
    graph->identity = model::make_identity("acsd.ProductGraph", 2, 0,
                                           "m5-graph", "acsd.m5");
    graph->assessment = model::make_assessment(Code::ok);
    graph->stage = ProductGraph::Stage::export_image;
    graph->signal = signal;
    auto product = std::make_shared<TestProductProvider>(graph);

    ExportRequest request;
    request.target.model = harness.wcs;
    request.target.width = 2;
    request.target.height = 2;
    request.quantity = ExportRequest::Quantity::supported_mean;
    request.support_policy = SupportMap::Kind::primitives;
    request.output_dtype = DType::f64;
    request.budget.max_work = 20000;
    request.budget.max_depth = 16;
    request.budget.area_absolute_sr = 1e-14;
    request.budget.arithmetic.initial_bits = 64;
    request.budget.arithmetic.max_bits = 256;
    auto export_input = std::make_shared<const BuildExportInput>(
        BuildExportInput{product, request});
    Result<rt::InvocationPlan> export_plan = plan_projection_export_build_export(
        harness.command, export_input, PlanOptions{2, 1, rt::Placement::memory});
    check(good(export_plan), "export: plan", code_name(export_plan.code));
    if (good(export_plan)) {
      observe(process, "build_export", "plan_r_bytes",
              std::to_string(export_plan.value->persistent.host_bytes));
      observe(process, "build_export", "plan_f_bytes",
              std::to_string(export_plan.value->progress_floor.host_bytes));
      observe(process, "build_export", "plan_work",
              std::to_string(export_plan.value->elements));
    }
    Result<ExportResult> exported = invoke<BuildExportInput, ExportResult>(
        harness, process, "build_export", export_input);
    check(good(exported), "export positive: build_export",
          code_name(exported.code));
    if (!good(exported) && !exported.assessment.issues.empty())
      std::cerr << "  export reason: " << exported.assessment.issues.front().reason
                << std::endl;
    observe(process, "build_export", "run_code",
            "\"" + std::string(code_name(exported.code)) + "\"");
    if (good(exported) && exported.value->estimate &&
        exported.value->estimate->signal) {
      auto bytes = harness.services->blocks->find(
          exported.value->estimate->signal->identity.object_id);
      observe(process, "build_export", "output_elements",
              std::to_string(bytes ? bytes->size() / sizeof(double) : 0));
      check(bytes && bytes->size() == 4 * sizeof(double),
            "export: output covers the four target pixels");
      if (bytes) {
        std::vector<double> values(bytes->size() / sizeof(double));
        std::memcpy(values.data(), bytes->data(), bytes->size());
        for (double v : values)
          check(std::isfinite(v) || std::isnan(v),
                "export: a target pixel is finite or an explicit NaN");
      }
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  const std::filesystem::path root =
      argc > 1 ? std::filesystem::path(argv[1])
               : std::filesystem::path("validation/process");
  Harness harness;
  if (!build_harness(harness)) {
    std::cerr << "harness construction failed\n";
    return 1;
  }
  geometry_checks(harness);
  operators_checks(harness);
  uncertainty_checks(harness);
  drizzle_export_registration_checks(harness);

  std::vector<std::string> entries;
  {
    std::istringstream stream(observations);
    std::string line;
    while (std::getline(stream, line)) entries.push_back(line);
  }
  for (const char* directory :
       {"geometry.overlap", "operators.action", "uncertainty.covariance",
        "geometry.drizzle", "projection.export"}) {
    std::filesystem::create_directories(root / directory);
    std::ofstream out(root / directory / "observed_m5.jsonl");
    const std::string needle = std::string("\"process\":\"") + directory + "\"";
    for (const std::string& entry : entries)
      if (entry.find(needle) != std::string::npos) out << entry << '\n';
  }

  rt::close_command(harness.command);
  rt::wait_external(harness.host->close_async());
  if (failures != 0) {
    std::cerr << failures << " of " << checks << " M5 checks failed\n";
    return 1;
  }
  std::cout << "M5 five-process registry/lifecycle PASS: " << checks << " checks\n";
  return 0;
}
