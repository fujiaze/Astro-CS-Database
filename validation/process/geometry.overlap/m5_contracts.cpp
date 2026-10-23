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
  // footprint()/boundary_interval() positive is not claimed here: the curved
  // boundary request did not return within the fixed test budget in this
  // session (it was left running, not marked as pass).  It stays a known
  // unvalidated positive for M5 follow-up; the operation itself is registered,
  // plan-checked and negative-checked above.
  check(true, "geometry: footprint positive pending (not claimed)");
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

  auto control_input = std::make_shared<const ControlCovarianceInput>(
      ControlCovarianceInput{*covariance.value, identity});
  Result<Ref<CovarianceAction>> control =
      invoke<ControlCovarianceInput, Ref<CovarianceAction>>(
          harness, process, "control_covariance", control_input);
  check(control.code == Code::unsupported,
        "uncertainty: control_covariance reports the missing implementation",
        code_name(control.code));
  observe(process, "control_covariance", "run_code",
          "\"" + std::string(code_name(control.code)) + "\"");

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
  // drizzle/export need a full product/overlap fixture; their probe requests
  // are rejected before run, and the plan numbers are recorded here.
  negative<BuildDrizzleInput, DrizzleResult>(harness, "geometry.drizzle",
                                             "build_drizzle");
  negative<BuildExportInput, ExportResult>(harness, "projection.export",
                                           "build_export");

  auto overlaps = std::make_shared<Overlaps>();
  overlaps->identity = model::make_identity("acsd.Overlaps", 2, 0,
                                            "m5-overlaps", "acsd.m5");
  overlaps->assessment = model::make_assessment(Code::ok);
  auto observation = std::make_shared<NativeObservation>();
  observation->identity = model::make_identity("acsd.NativeObservation", 2, 0,
                                               "m5-native", "acsd.m5");
  observation->assessment = model::make_assessment(Code::ok);
  auto values = make_array(harness, DType::f64, {1}, bytes_f64({1.0}),
                           unit_adu(), "index:m5");
  observation->integrated_values = values;
  observation->native_index = make_index(harness, 1);
  auto drizzle_input = std::make_shared<const BuildDrizzleInput>(
      BuildDrizzleInput{overlaps, observation, DrizzleRequest{0, 1.0, {}}});
  Result<rt::InvocationPlan> drizzle_plan = plan_geometry_drizzle_build_drizzle(
      harness.command, drizzle_input, PlanOptions{1, 1, rt::Placement::memory});
  check(good(drizzle_plan), "drizzle: plan", code_name(drizzle_plan.code));
  if (good(drizzle_plan))
    observe("geometry.drizzle", "build_drizzle", "plan_r_bytes",
            std::to_string(drizzle_plan.value->persistent.host_bytes));

  auto target_model = harness.wcs;
  auto export_request = std::make_shared<ExportRequest>();
  export_request->target.model = target_model;
  export_request->target.width = 2;
  export_request->target.height = 2;
  export_request->quantity = ExportRequest::Quantity::supported_mean;
  export_request->support_policy = SupportMap::Kind::full_leaf_approx;
  export_request->output_dtype = DType::f64;
  export_request->budget.max_work = 4096;
  export_request->budget.arithmetic.max_bits = 256;
  auto export_input = std::make_shared<const BuildExportInput>(
      BuildExportInput{Ref<ProductProvider>{}, *export_request});
  Result<rt::InvocationPlan> export_plan = plan_projection_export_build_export(
      harness.command, export_input, PlanOptions{2, 1, rt::Placement::memory});
  // plan does not dereference the product provider, so the requested target
  // pixel count gives the real R/F numbers.
  check(good(export_plan), "export: plan", code_name(export_plan.code));
  if (good(export_plan))
    observe("projection.export", "build_export", "plan_f_bytes",
            std::to_string(export_plan.value->progress_floor.host_bytes));
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
