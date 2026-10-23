// Independent smoke test of the generated typed bindings (M0).
//
// Only public headers and the generated headers are included: the test proves
// that the frozen registry is fully populated (22 processes / 57 operations),
// that the generated manifest and TypeId table are complete and stable, that
// the wire codecs are real (they reject malformed/unknown payloads instead of
// fabricating defaults) and that the registry refuses duplicate registration
// and missing codecs.  It does not execute scientific code.
#include <acsd/modules/generated/codecs.hpp>
#include <acsd/modules/generated/inputs.hpp>
#include <acsd/modules/generated/status.hpp>
#include <acsd/runtime/generated_manifest.hpp>
#include <acsd/runtime/process.hpp>

#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace acsd;
using namespace acsd::rt;

int checks = 0;
int failures = 0;

void check(bool condition, const std::string& name) {
  ++checks;
  if (condition) return;
  ++failures;
  std::cerr << "FAIL: " << name << "\n";
}

bool contains(std::string_view haystack, std::string_view needle) {
  return haystack.find(needle) != std::string_view::npos;
}

Code freeze_code(RegistryBuilder& builder,
                 std::shared_ptr<const Registry>* registry = nullptr) {
  Result<std::shared_ptr<const Registry>> frozen = builder.freeze();
  if (frozen.code == Code::ok && frozen.value && registry != nullptr)
    *registry = *frozen.value;
  return frozen.code;
}

std::string failure_reason(const Result<std::shared_ptr<const Registry>>& value) {
  if (value.assessment.issues.empty()) return "<no issue>";
  return value.assessment.issues.front().reason;
}

// A codec that accepted an empty/malformed payload would be a fake codec.
template <class T>
void check_codec_rejects_garbage(const std::string& name) {
  InvocationContext context{};
  WireObjects objects;
  TypeCodec<T> codec = generated::type_codec<T>();
  check(codec.decode != nullptr, name + ": decode hook is registered");
  check(codec.encode != nullptr, name + ": encode hook is registered");
  check(!codec.schema_json.empty(), name + ": schema is registered");
  check(codec.id.revision == 1, name + ": schema revision is 1");
  check(contains(codec.schema_json, "\"$id\":\"" + codec.id.schema + "\""),
        name + ": schema $id matches the TypeId");
  check(codec.schema_json.front() == '{' && codec.schema_json.back() == '}',
        name + ": schema is a JSON object");
  Result<T> truncated = codec.decode(context, std::string_view("{"), objects);
  check(truncated.code == Code::invalid_request,
        name + ": truncated JSON is rejected");
  Result<T> empty = codec.decode(context, std::string_view("{}"), objects);
  check(empty.code == Code::invalid_request,
        name + ": an empty object is rejected (no fabricated defaults)");
  Result<T> unknown =
      codec.decode(context, std::string_view("{\"not_a_field\":1}"), objects);
  check(unknown.code == Code::invalid_request,
        name + ": an unknown field is rejected");
  // A default-constructed value cannot be encoded when the input still has a
  // required object reference; if the schema mentions object aliases, encoding
  // the default value must fail instead of inventing an empty reference.
  if (contains(codec.schema_json, "\"x-acsd-object\"")) {
    T value{};
    Result<std::string> encoded = codec.encode(context, value, objects);
    check(encoded.code == Code::invalid_request,
          name + ": a null required Ref is rejected at encode");
  }
}

}  // namespace

int main() {
  // ---------------------------------------------------------------------
  // The frozen registry carries the complete whitelist.
  // ---------------------------------------------------------------------
  RegistryBuilder builder;
  Result<Done> types = generated::register_builtin_types(builder);
  check(types.code == Code::ok, "register_builtin_types succeeds");
  Result<Done> processes = generated::register_processes(builder);
  check(processes.code == Code::ok, "register_processes succeeds");
  std::shared_ptr<const Registry> registry;
  Result<std::shared_ptr<const Registry>> frozen = builder.freeze();
  check(frozen.code == Code::ok && frozen.value,
        "freeze succeeds: " + failure_reason(frozen));
  if (frozen.code != Code::ok || !frozen.value) {
    std::cerr << failures << " failure(s) before the manifest check\n";
    return 1;
  }
  registry = *frozen.value;

  const auto& expected = generated::expected_registry();
  std::size_t operation_count = 0;
  std::set<std::string> processes_seen;
  std::set<std::string> operations_seen;
  for (const auto& [process_id, operations] : expected) {
    check(processes_seen.insert(process_id).second,
          "process is listed once: " + process_id);
    for (const std::string& operation : operations) {
      ++operation_count;
      check(operations_seen.insert(operation).second,
            "operation is listed once: " + operation);
      check(registry->has(process_id, operation),
            "registry has " + process_id + "/" + operation);
    }
  }
  check(expected.size() == 22,
        "22 processes are declared (got " +
            std::to_string(expected.size()) + ")");
  check(operation_count == 57,
        "57 operations are declared (got " +
            std::to_string(operation_count) + ")");
  check(!registry->has("observation.calibrate", "not_an_operation"),
        "an unknown operation is not registered");
  check(!registry->has("not.a.process", "calibrate_observation"),
        "an unknown process is not registered");

  // ---------------------------------------------------------------------
  // The manifest is the build product of that registry: every id, document
  // and TypeId shows up.
  // ---------------------------------------------------------------------
  const std::string manifest = registry->manifest_json();
  check(contains(manifest, "\"protocol\": \"acsd.process/2\""),
        "manifest protocol is acsd.process/2");
  check(contains(manifest, "\"backend\": \"cpu\""), "manifest backend is cpu");
  for (const auto& [process_id, operations] : expected) {
    check(contains(manifest, "\"process\": \"" + process_id + "\""),
          "manifest lists process " + process_id);
    check(contains(manifest,
                   "\"document\": \"docs/engineering/rebuild/processes/" +
                       process_id + ".md\""),
          "manifest lists the document of " + process_id);
    for (const std::string& operation : operations) {
      check(contains(manifest, "\"operation\": \"" + operation + "\""),
            "manifest lists operation " + operation);
    }
  }
  for (const std::string& type_id : generated::input_type_ids()) {
    check(contains(manifest, "\"schema\":\"" + type_id + "\""),
          "manifest lists input TypeId " + type_id);
  }
  check(generated::input_type_ids().size() == 57,
        "57 Input TypeIds are generated");

  // ---------------------------------------------------------------------
  // Every registered TypeCodec is complete, unique and its schema carries the
  // matching $id.  Registration order is codecs first, Input wrappers last.
  // ---------------------------------------------------------------------
  const auto& type_ids = generated::registered_type_ids();
  std::set<std::pair<std::string, U32>> unique_types;
  for (const auto& [schema, revision] : type_ids) {
    check(unique_types.insert(std::pair<std::string, U32>{schema, revision}).second,
          "TypeId is registered once: " + schema);
    check(revision == 1, "TypeId revision is 1: " + schema);
    check(!schema.empty(), "TypeId schema is not empty");
  }
  check(type_ids.size() == unique_types.size(),
        "no TypeId is registered twice");
  check(type_ids.size() > generated::input_type_ids().size(),
        "shared model/output codecs are registered before the Input wrappers");
  for (const std::string& type_id : generated::input_type_ids()) {
    check(type_id.rfind("acsd.input.", 0) == 0,
          "Input TypeId lives under acsd.input.: " + type_id);
  }

  // ---------------------------------------------------------------------
  // Codec contract per Input type: real schema, real rejection paths.
  // ---------------------------------------------------------------------
#define ACSD_CHECK_CODEC(Type) \
  check_codec_rejects_garbage<Type>(#Type)
  ACSD_CHECK_CODEC(acsd::modules::CalibrateInput);
  ACSD_CHECK_CODEC(acsd::modules::RegroupObservationInput);
  ACSD_CHECK_CODEC(acsd::modules::MakeWcsInput);
  ACSD_CHECK_CODEC(acsd::modules::ForwardCenter0Input);
  ACSD_CHECK_CODEC(acsd::modules::ForwardEdge0Input);
  ACSD_CHECK_CODEC(acsd::modules::InverseInput);
  ACSD_CHECK_CODEC(acsd::modules::DerivativeInput);
  ACSD_CHECK_CODEC(acsd::modules::SolidAngleJacobianInput);
  ACSD_CHECK_CODEC(acsd::modules::BindWcsInput);
  ACSD_CHECK_CODEC(acsd::modules::BuildCatalogueIndexInput);
  ACSD_CHECK_CODEC(acsd::modules::MakeCatalogueProviderInput);
  ACSD_CHECK_CODEC(acsd::modules::CatalogueQueryInput);
  ACSD_CHECK_CODEC(acsd::modules::CatalogueQueryQuadsInput);
  ACSD_CHECK_CODEC(acsd::modules::CatalogueLookupInput);
  ACSD_CHECK_CODEC(acsd::modules::SolveAstrometryInput);
  ACSD_CHECK_CODEC(acsd::modules::FitPsfInput);
  ACSD_CHECK_CODEC(acsd::modules::EvaluatePsfInput);
  ACSD_CHECK_CODEC(acsd::modules::BindObservationModelInput);
  ACSD_CHECK_CODEC(acsd::modules::MeasureScienceSourcesInput);
  ACSD_CHECK_CODEC(acsd::modules::IntegrateResponseInput);
  ACSD_CHECK_CODEC(acsd::modules::FitPhotometryInput);
  ACSD_CHECK_CODEC(acsd::modules::ApplyScaleInput);
  ACSD_CHECK_CODEC(acsd::modules::MakeFootprintProviderInput);
  ACSD_CHECK_CODEC(acsd::modules::DetectorFootprintInput);
  ACSD_CHECK_CODEC(acsd::modules::FootprintInput);
  ACSD_CHECK_CODEC(acsd::modules::MeasureInput);
  ACSD_CHECK_CODEC(acsd::modules::ComputeOverlapInput);
  ACSD_CHECK_CODEC(acsd::modules::RefineGeometryInput);
  ACSD_CHECK_CODEC(acsd::modules::FootprintForwardIntervalInput);
  ACSD_CHECK_CODEC(acsd::modules::FootprintBoundaryIntervalInput);
  ACSD_CHECK_CODEC(acsd::modules::FootprintDerivativeIntervalInput);
  ACSD_CHECK_CODEC(acsd::modules::RefinableOpenInput);
  ACSD_CHECK_CODEC(acsd::modules::RefinableRefineInput);
  ACSD_CHECK_CODEC(acsd::modules::MakeActionInput);
  ACSD_CHECK_CODEC(acsd::modules::ApplyInput);
  ACSD_CHECK_CODEC(acsd::modules::TransposeApplyInput);
  ACSD_CHECK_CODEC(acsd::modules::ComposeInput);
  ACSD_CHECK_CODEC(acsd::modules::ActionDependencyBlocksInput);
  ACSD_CHECK_CODEC(acsd::modules::ActionSerializeRecipeInput);
  ACSD_CHECK_CODEC(acsd::modules::ActionRowEnclosureInput);
  ACSD_CHECK_CODEC(acsd::modules::MakeCovarianceInput);
  ACSD_CHECK_CODEC(acsd::modules::CovarianceApplyInput);
  ACSD_CHECK_CODEC(acsd::modules::QuadraticFormInput);
  ACSD_CHECK_CODEC(acsd::modules::GeometryCovarianceBoundInput);
  ACSD_CHECK_CODEC(acsd::modules::ControlCovarianceInput);
  ACSD_CHECK_CODEC(acsd::modules::BuildDrizzleInput);
  ACSD_CHECK_CODEC(acsd::modules::EstimateFluxInformationInput);
  ACSD_CHECK_CODEC(acsd::modules::EvaluateSceneVarianceInput);
  ACSD_CHECK_CODEC(acsd::modules::BuildSupportedTriangulationInput);
  ACSD_CHECK_CODEC(acsd::modules::ReconstructSceneVarianceInput);
  ACSD_CHECK_CODEC(acsd::modules::BuildSkyControlsInput);
  ACSD_CHECK_CODEC(acsd::modules::FitRelativeSkyInput);
  ACSD_CHECK_CODEC(acsd::modules::ApplySkyInput);
  ACSD_CHECK_CODEC(acsd::modules::SelectSamplesInput);
  ACSD_CHECK_CODEC(acsd::modules::SolveWeightsInput);
  ACSD_CHECK_CODEC(acsd::modules::CoaddInput);
  ACSD_CHECK_CODEC(acsd::modules::BuildExportInput);
#undef ACSD_CHECK_CODEC

  // ---------------------------------------------------------------------
  // Bounds/status report: provisional bounds are named, and the two coadd
  // fields are enforced by name (CoaddInput has five fields).
  // ---------------------------------------------------------------------
  check(!generated::bounds_provisional_operations().empty(),
        "provisional bounds operations are reported");
  const std::string status = generated::operation_status_json();
  check(contains(status, "acsd.typed-bindings/1"),
        "operation status report has its own protocol");
  check(contains(status, "\"coadd.integrate\""),
        "operation status reports coadd.integrate");
  check(contains(status, "blocked_refactor"),
        "coadd.integrate reports the explicit refactor blocker");
  check(contains(status, "not integrated before M5"),
        "run/plan status names the M5 integration boundary");

  // ---------------------------------------------------------------------
  // Negative registry cases: duplicate registration and missing codecs are
  // rejected by freeze instead of being silently overwritten.
  // ---------------------------------------------------------------------
  {
    RegistryBuilder duplicate;
    check(generated::register_builtin_types(duplicate).code == Code::ok,
          "duplicate case: first type registration succeeds");
    check(generated::register_builtin_types(duplicate).code == Code::ok,
          "duplicate case: second type registration is accepted at add time");
    check(freeze_code(duplicate) == Code::identity_conflict,
          "duplicate TypeId registration is rejected by freeze");
  }
  {
    RegistryBuilder duplicate;
    check(generated::register_builtin_types(duplicate).code == Code::ok,
          "duplicate process case: types register");
    check(generated::register_processes(duplicate).code == Code::ok,
          "duplicate process case: first process registration succeeds");
    check(generated::register_processes(duplicate).code == Code::ok,
          "duplicate process case: second process registration adds entries");
    check(freeze_code(duplicate) == Code::identity_conflict,
          "duplicate process registration is rejected by freeze");
  }
  {
    RegistryBuilder missing_codecs;
    check(generated::register_processes(missing_codecs).code == Code::ok,
          "missing codec case: process registration is accepted");
    check(freeze_code(missing_codecs) == Code::invalid_model,
          "an operation without a registered codec is rejected by freeze");
  }
  {
    RegistryBuilder types_only;
    check(generated::register_builtin_types(types_only).code == Code::ok,
          "types-only case: registration succeeds");
    std::shared_ptr<const Registry> registry_only;
    check(freeze_code(types_only, &registry_only) == Code::ok,
          "a registry with codecs but no process freezes");
    check(registry_only && !registry_only->has("observation.calibrate",
                                               "calibrate_observation"),
          "the types-only registry has no operations");
  }

  if (failures != 0) {
    std::cerr << failures << " of " << checks << " check(s) failed\n";
    return 1;
  }
  std::cout << "typed_smoke: " << checks << " checks passed; "
            << expected.size() << " processes, " << operation_count
            << " operations, " << type_ids.size() << " TypeIds, "
            << generated::bounds_provisional_operations().size()
            << " provisional bounds\n";
  return 0;
}
