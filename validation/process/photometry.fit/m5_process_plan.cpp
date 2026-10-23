// M5 typed-adapter validation: validate/bounds/plan of the nine processes.
//
// This case constructs metadata-only fixtures (no payload read happens in
// validate/bounds/plan), runs every operation's hooks and records the real
// plan R/F numbers.  Positive science runs need real arrays/models and are
// delivered separately per process; this case is explicit about what it does:
//   * every process has a negative case (missing/invalid required field),
//   * every operation has a real plan whose R/F/cohorts/work are printed,
//   * coadd.integrate's run is checked to be a named unsupported failure.
#include <acsd/modules/coadd.hpp>
#include <acsd/modules/information.hpp>
#include <acsd/modules/photometry.hpp>
#include <acsd/modules/sky.hpp>

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace acsd;
using namespace acsd::modules;

int g_failures = 0;
int g_checks = 0;

void check(const char* name, bool pass, const std::string& actual) {
  ++g_checks;
  if (!pass) ++g_failures;
  std::printf("{\"check\":\"%s\",\"pass\":%s,\"actual\":\"%s\"}\n", name,
              pass ? "true" : "false", actual.c_str());
}

std::string code_name(Code code) {
  switch (code) {
    case Code::ok: return "ok";
    case Code::invalid_request: return "invalid_request";
    case Code::invalid_model: return "invalid_model";
    case Code::unsupported: return "unsupported";
    case Code::unavailable: return "unavailable";
    case Code::identity_conflict: return "identity_conflict";
    case Code::resource_exhausted: return "resource_exhausted";
    default: return "code_" + std::to_string(static_cast<int>(code));
  }
}

Ref<Index> make_index(U64 size, const char* name) {
  auto index = std::make_shared<Index>();
  index->size = size;
  index->kind = Index::Kind::leaf;
  index->identity.object_id = std::string("fixture:") + name;
  index->identity.content_hash =
      "sha256:" + std::string(64, 'a' + (std::strlen(name) % 6));
  index->assessment.complete = true;
  return index;
}

Ref<Array> make_array(DType dtype, std::vector<U64> shape, const char* name) {
  auto array = std::make_shared<Array>();
  array->dtype = dtype;
  array->shape = std::move(shape);
  array->unit.scale = "fixture";
  array->unit.multiplier = 1.0;
  array->unit.linear_power = 1;
  array->index_id = std::string("fixture:") + name;
  array->identity.object_id = std::string("fixture:") + name;
  array->identity.content_hash =
      "sha256:" + std::string(64, 'a' + (std::strlen(name) % 6));
  array->assessment.complete = true;
  return array;
}

// A metadata-only LinearAction: plan/validate never apply it.
class MetaAction final : public LinearAction {
 public:
  MetaAction(U64 domain, U64 codomain) {
    spaces.domain = make_index(domain, "action.domain");
    spaces.codomain = make_index(codomain, "action.codomain");
    spaces.input.linear_power = 1;
    spaces.output.linear_power = 1;
    identity.object_id = "fixture:action";
    identity.content_hash = "sha256:" + std::string(64, 'd');
    assessment.complete = true;
  }
  Result<ArrayRef> apply(Context&, ArrayRef) const override {
    return model_fail();
  }
  Result<ArrayRef> transpose_apply(Context&, ArrayRef) const override {
    return model_fail();
  }
  Result<std::vector<Id>> dependency_blocks(BlockRequest, bool) const override {
    Result<std::vector<Id>> out;
    out.code = Code::ok;
    out.value = std::vector<Id>{};
    return out;
  }
  Result<ActionRecipe> serialize_recipe() const override {
    Result<ActionRecipe> out;
    out.code = Code::unsupported;
    return out;
  }
  Result<std::vector<ExactInterval>> row_enclosure(Context&, U64) const override {
    Result<std::vector<ExactInterval>> out;
    out.code = Code::unsupported;
    return out;
  }

 private:
  static Result<ArrayRef> model_fail() {
    Result<ArrayRef> out;
    out.code = Code::unsupported;
    return out;
  }
};

Ref<CovarianceAction> make_covariance() {
  auto covariance = std::make_shared<CovarianceAction>();
  covariance->identity.object_id = "fixture:covariance";
  covariance->identity.content_hash = "sha256:" + std::string(64, 'e');
  covariance->assessment.complete = true;
  covariance->influence =
      std::make_shared<MetaAction>(4, 4);
  covariance->input_output.domain = make_index(4, "cov.domain");
  covariance->input_output.codomain = make_index(4, "cov.codomain");
  covariance->input_output.input.linear_power = 1;
  covariance->input_output.output.linear_power = 2;
  return covariance;
}

// A FramesProvider that only exists so plan/validate can check metadata.
class DummyFrames final : public FramesProvider {
 public:
  Result<std::vector<Id>> list_frames(Context&) const override {
    Result<std::vector<Id>> out;
    out.code = Code::ok;
    out.value = std::vector<Id>{"f0", "f1"};
    return out;
  }
  Result<Ref<FrameDescription>> describe_frame(Context&, Id) const override {
    Result<Ref<FrameDescription>> out;
    out.code = Code::unavailable;
    return out;
  }
  Result<LeafBlock> read_leaf_block(Context&, const FrameReadRequest&) const override {
    Result<LeafBlock> out;
    out.code = Code::unavailable;
    return out;
  }
  Result<std::vector<ArrayRef>> read_native_dependencies(
      Context&, std::span<const SourceIdentity>) const override {
    Result<std::vector<ArrayRef>> out;
    out.code = Code::unavailable;
    return out;
  }
  Result<Ref<LinearAction>> open_action(Context&, Id) const override {
    Result<Ref<LinearAction>> out;
    out.code = Code::unsupported;
    return out;
  }
  Result<Ref<SupportMap>> query_support(Context&, Id, Ref<Index>) const override {
    Result<Ref<SupportMap>> out;
    out.code = Code::unavailable;
    return out;
  }
  Result<Ref<RefinableGeometryProvider>> open_refinable_geometry(
      Context&, Id) const override {
    Result<Ref<RefinableGeometryProvider>> out;
    out.code = Code::unsupported;
    return out;
  }
};

Ref<FramesProvider> frames_provider() {
  return std::make_shared<DummyFrames>();
}

Comparability group(const char* frame) {
  Comparability comparison;
  comparison.frames = {frame};
  comparison.response_proof = "response-proof";
  comparison.spatial_kernel_proof = "kernel-proof";
  return comparison;
}

Basis constant_basis() {
  Basis basis;
  basis.kind = Basis::Kind::constant;
  basis.degree = 0;
  basis.normalization_domain = Box{Vec2{0, 0}, Vec2{1, 1}};
  basis.center = Vec2{0.5, 0.5};
  basis.scale = Vec2{1.0, 1.0};
  return basis;
}

void record_plan(const char* process, const char* operation,
                 const Result<rt::InvocationPlan>& plan) {
  if (plan.code != Code::ok || !plan.value) {
    check((std::string(process) + "." + operation + ".plan").c_str(), false,
          code_name(plan.code));
    return;
  }
  const rt::InvocationPlan& p = *plan.value;
  std::string actual =
      "elements=" + std::to_string(p.elements) +
      " block=" + std::to_string(p.block_elements) +
      " cohorts=" + std::to_string(p.cohorts) +
      " R=" + std::to_string(p.persistent.host_bytes) +
      " F=" + std::to_string(p.progress_floor.host_bytes) +
      " regions=" + std::to_string(p.regions.size()) +
      " edges=" + std::to_string(p.region_edges.size()) +
      " input=" + p.input_hash + " request=" + p.request_hash;
  check((std::string(process) + "." + operation + ".plan").c_str(), true,
        actual);
}

// ---------------------------------------------------------------------------
// Fixtures
// ---------------------------------------------------------------------------

Ref<FitPhotometryInput> fit_input() {
  auto input = std::make_shared<FitPhotometryInput>();
  auto basis = std::make_shared<SpectralBasis>();
  basis->identity.content_hash = "sha256:" + std::string(64, 'b');
  basis->identity.object_id = "fixture.xp_basis";
  basis->assessment.complete = true;
  auto prediction = std::make_shared<SpectralPrediction>();
  prediction->identity.content_hash = "sha256:" + std::string(64, 'c');
  prediction->identity.object_id = "fixture.prediction";
  prediction->assessment.complete = true;
  prediction->basis = basis;
  prediction->basis_integrals = make_array(DType::f64, {4}, "prediction");
  auto response = std::make_shared<Response>();
  response->identity.content_hash = "sha256:" + std::string(64, 'f');
  response->identity.object_id = "fixture.response";
  response->assessment.complete = true;
  prediction->response = response;
  auto measurement = std::make_shared<SourceMeasurements>();
  measurement->frame = "f0";
  measurement->identity.content_hash = "sha256:" + std::string(64, '1');
  measurement->identity.object_id = "fixture.measurement";
  measurement->assessment.complete = true;
  StarFlux star;
  star.gaia_source_id = 1;
  star.integrated_flux = 1.0;
  measurement->stars.push_back(star);
  auto native = std::make_shared<NativeObservation>();
  native->frame = "f0";
  native->identity.content_hash = "sha256:" + std::string(64, '2');
  native->identity.object_id = "fixture.native";
  native->assessment.complete = true;
  measurement->observation = native;
  CatalogueStar catalogue_star;
  catalogue_star.source_id = 1;
  catalogue_star.xp_basis = basis;
  std::vector<Ref<SourceMeasurements>> measurements;
  measurements.push_back(measurement);
  std::vector<CatalogueStar> stars;
  stars.push_back(catalogue_star);
  std::vector<Ref<SpectralPrediction>> predictions;
  predictions.push_back(prediction);
  input->measurements =
      std::make_shared<const std::vector<Ref<SourceMeasurements>>>(
          std::move(measurements));
  input->stars =
      std::make_shared<const std::vector<CatalogueStar>>(std::move(stars));
  input->predictions =
      std::make_shared<const std::vector<Ref<SpectralPrediction>>>(
          std::move(predictions));
  input->request.frames = {"f0"};
  input->request.responses = {response};
  input->request.application_id = "app-1";
  input->request.basis = constant_basis();
  input->request.likelihood.kind = "joint_flux_gaussian";
  input->request.max_outer_iterations = 10;
  input->request.gradient_tolerance = 1e-6;
  input->request.step_tolerance = 1e-6;
  return input;
}

Ref<ApplyScaleInput> apply_input(bool already_applied = false) {
  auto input = std::make_shared<ApplyScaleInput>();
  auto solution = std::make_shared<PhotoSolution>();
  solution->frames = {"f0"};
  solution->application_id = "app-1";
  solution->spatial_basis = constant_basis();
  solution->theta = make_array(DType::f64, {1}, "theta");
  solution->identity.content_hash = "sha256:" + std::string(64, '3');
  solution->identity.object_id = "fixture.solution";
  solution->assessment.complete = true;
  auto observation = std::make_shared<NativeObservation>();
  observation->frame = "f0";
  observation->integrated_values = make_array(DType::f64, {16}, "values");
  observation->dq = make_array(DType::u16, {16}, "dq");
  observation->native_index = make_index(16, "native");
  auto footprints = std::make_shared<FootprintSet>();
  footprints->identity.object_id = "fixture.footprints";
  footprints->identity.content_hash = "sha256:" + std::string(64, '4');
  footprints->assessment.complete = true;
  observation->footprints = footprints;
  observation->identity.content_hash = "sha256:" + std::string(64, '5');
  observation->identity.object_id = "fixture.observation";
  observation->assessment.complete = true;
  if (already_applied) observation->scale_applications.push_back("app-1");
  input->solution = solution;
  input->observation = observation;
  input->frame = "f0";
  return input;
}

Ref<EstimateFluxInformationInput> information_input() {
  auto input = std::make_shared<EstimateFluxInformationInput>();
  input->request.h = make_array(DType::f64, {4}, "h");
  input->request.covariance = make_covariance();
  return input;
}

Ref<EvaluateSceneVarianceInput> scene_evaluate_input() {
  auto input = std::make_shared<EvaluateSceneVarianceInput>();
  input->frames = frames_provider();
  input->frame = "f0";
  std::vector<HealpixLeaf> leaves{HealpixLeaf{10, 1}, HealpixLeaf{10, 2}};
  input->leaves =
      std::make_shared<const std::vector<HealpixLeaf>>(std::move(leaves));
  return input;
}

Ref<BuildSupportedTriangulationInput> scene_build_input(
    PrecisionControls::Kind kind = PrecisionControls::Kind::actual_scene,
    double radius_rad = 1.0 * 3.14159265358979323846 / 180.0) {
  auto input = std::make_shared<BuildSupportedTriangulationInput>();
  auto controls = std::make_shared<PrecisionControls>();
  controls->kind = kind;
  controls->positions = make_array(DType::f64, {8}, "positions");
  controls->values = make_array(DType::f64, {4}, "values");
  controls->identity.content_hash = "sha256:" + std::string(64, '6');
  controls->identity.object_id = "fixture.controls";
  controls->assessment.complete = true;
  auto support = std::make_shared<SupportMap>();
  support->identity.content_hash = "sha256:" + std::string(64, '7');
  support->identity.object_id = "fixture.support";
  support->assessment.complete = true;
  input->controls = controls;
  input->support = support;
  input->chart.center = SkyICRS{1.0, 0.0, 0.0};
  input->chart.maximum_radius_rad = radius_rad;
  return input;
}

Ref<ReconstructSceneVarianceInput> scene_reconstruct_input(
    PrecisionControls::Kind kind = PrecisionControls::Kind::actual_scene) {
  auto input = std::make_shared<ReconstructSceneVarianceInput>();
  auto controls = std::make_shared<PrecisionControls>();
  controls->kind = kind;
  controls->positions = make_array(DType::f64, {8}, "positions");
  controls->values = make_array(DType::f64, {4}, "values");
  controls->identity.content_hash = "sha256:" + std::string(64, '8');
  controls->identity.object_id = "fixture.controls";
  controls->assessment.complete = true;
  auto mesh = std::make_shared<SupportedTriangulation>();
  mesh->identity.content_hash = "sha256:" + std::string(64, '9');
  mesh->identity.object_id = "fixture.mesh";
  mesh->assessment.complete = true;
  input->controls = controls;
  input->triangulation = mesh;
  std::vector<SkyICRS> queries{SkyICRS{1.0, 0.0, 0.0}};
  input->queries =
      std::make_shared<const std::vector<SkyICRS>>(std::move(queries));
  return input;
}

Ref<BuildSkyControlsInput> sky_controls_input() {
  auto input = std::make_shared<BuildSkyControlsInput>();
  input->frames = frames_provider();
  input->request.groups = {group("f0")};
  input->request.basis = constant_basis();
  input->request.gauge.kind = Gauge::Kind::reference_frame;
  return input;
}

Ref<FitRelativeSkyInput> sky_fit_input(bool with_noise = true) {
  auto input = std::make_shared<FitRelativeSkyInput>();
  auto controls = std::make_shared<SkyControls>();
  controls->c = make_array(DType::f64, {4}, "controls");
  controls->component_ids = make_array(DType::u64, {4}, "components");
  controls->T = std::make_shared<MetaAction>(1, 4);
  controls->H = std::make_shared<MetaAction>(1, 4);
  if (with_noise) controls->noise = make_covariance();
  controls->identity.content_hash = "sha256:" + std::string(64, 'a');
  controls->identity.object_id = "fixture.sky_controls";
  controls->assessment.complete = true;
  input->controls = controls;
  input->request.groups = {group("f0")};
  input->request.basis = constant_basis();
  input->request.gauge.kind = Gauge::Kind::reference_frame;
  input->request.solve.max_iterations = 10;
  return input;
}

CoaddRequest coadd_request() {
  CoaddRequest request;
  request.target.id = "estimator-1";
  request.target.quantity = "brightness";
  request.target.unit.scale = "L";
  request.target.unit.multiplier = 1.0;
  request.target.unit.linear_power = 1;
  request.mixed_observation = false;
  request.weights = WeightPolicy::automatic;
  request.rejection.kind = RejectionPolicy::Kind::dq_only;
  request.groups = {group("f0")};
  request.solve.max_iterations = 10;
  return request;
}

Ref<Selection> selection_fixture(U64 leaves, U64 frames) {
  auto selection = std::make_shared<Selection>();
  selection->index = make_index(leaves, "selection");
  selection->eligibility = make_array(DType::u8, {leaves, frames}, "eligibility");
  selection->accepted = make_array(DType::u8, {leaves, frames}, "accepted");
  selection->reasons = make_array(DType::u16, {leaves, frames}, "reasons");
  selection->policy.kind = RejectionPolicy::Kind::dq_only;
  selection->condition.estimator = "estimator-1";
  selection->identity.object_id = "fixture.selection";
  selection->identity.content_hash = "sha256:" + std::string(64, 'b');
  selection->assessment.complete = true;
  return selection;
}

Ref<SelectSamplesInput> select_input() {
  auto input = std::make_shared<SelectSamplesInput>();
  input->frames = frames_provider();
  input->request = coadd_request();
  return input;
}

Ref<SolveWeightsInput> weights_input() {
  auto input = std::make_shared<SolveWeightsInput>();
  input->frames = frames_provider();
  input->selection = selection_fixture(2, 1);
  input->request = coadd_request();
  return input;
}

Ref<CoaddInput> integrate_input(
    const Ref<Selection>& selection_override = nullptr) {
  auto input = std::make_shared<CoaddInput>();
  input->frames = frames_provider();
  auto sky = std::make_shared<SkyFit>();
  sky->identity.object_id = "fixture.sky_fit";
  sky->identity.content_hash = "sha256:" + std::string(64, 'c');
  sky->assessment.complete = true;
  auto selection = selection_fixture(2, 1);
  auto weights = std::make_shared<WeightSolution>();
  weights->selection = selection_override ? selection_override : selection;
  weights->policy = WeightPolicy::equal;
  weights->coefficients = make_array(DType::f64, {2, 1}, "coefficients");
  weights->samples = make_index(2, "weights");
  weights->condition.estimator = "estimator-1";
  weights->identity.object_id = "fixture.weights";
  weights->identity.content_hash = "sha256:" + std::string(64, 'd');
  weights->assessment.complete = true;
  input->sky = sky;
  input->selection = selection_override ? selection_override : selection;
  input->weights = weights;
  CoaddRequest request = coadd_request();
  request.weights = WeightPolicy::equal;
  input->request = request;
  return input;
}

rt::PlanOptions default_options() {
  rt::PlanOptions options;
  options.block_elements = 64;
  options.window = 1;
  options.placement = rt::Placement::memory;
  return options;
}

}  // namespace

int main() {
  const rt::InvocationContext ctx{};
  const rt::PlanOptions options = default_options();

  // ---- positive metadata plans (real R/F numbers) -----------------------
  {
    auto input = fit_input();
    check("fit.validate_ok",
          validate_photometry_fit(ctx, input).code == Code::ok, "ok");
    record_plan("photometry.fit", "fit_photometry",
                plan_photometry_fit(ctx, input, options));
    auto broken = std::make_shared<FitPhotometryInput>(*fit_input());
    broken->request.application_id.clear();
    check("fit.negative_missing_application_id",
          validate_photometry_fit(ctx, broken).code == Code::invalid_request,
          code_name(validate_photometry_fit(ctx, broken).code));
  }
  {
    auto input = apply_input();
    check("apply.validate_ok",
          validate_photometry_apply(ctx, input).code == Code::ok, "ok");
    record_plan("photometry.apply", "apply_scale",
                plan_photometry_apply(ctx, input, options));
    auto broken = apply_input(true);
    check("apply.negative_repeated_application",
          validate_photometry_apply(ctx, broken).code ==
              Code::identity_conflict,
          code_name(validate_photometry_apply(ctx, broken).code));
  }
  {
    auto input = information_input();
    check("information.validate_ok",
          validate_information_reference(ctx, input).code == Code::ok, "ok");
    record_plan("information.reference", "estimate_flux_information",
                plan_information_reference(ctx, input, options));
    auto broken = std::make_shared<EstimateFluxInformationInput>(*information_input());
    broken->request.h = nullptr;
    check("information.negative_missing_h",
          validate_information_reference(ctx, broken).code ==
              Code::invalid_request,
          code_name(validate_information_reference(ctx, broken).code));
  }
  {
    auto input = scene_evaluate_input();
    check("scene.evaluate_validate_ok",
          validate_information_scene_evaluate(ctx, input).code == Code::ok,
          "ok");
    record_plan("information.scene", "evaluate_scene_variance",
                plan_information_scene_evaluate(ctx, input, options));
    auto broken = std::make_shared<EvaluateSceneVarianceInput>(*scene_evaluate_input());
    broken->frames = nullptr;
    check("scene.evaluate_negative_missing_provider",
          validate_information_scene_evaluate(ctx, broken).code ==
              Code::invalid_request,
          code_name(validate_information_scene_evaluate(ctx, broken).code));
  }
  {
    auto input = scene_build_input();
    check("scene.build_validate_ok",
          validate_information_scene_build(ctx, input).code == Code::ok, "ok");
    record_plan("information.scene", "build_supported_triangulation",
                plan_information_scene_build(ctx, input, options));
    auto broken = scene_build_input(PrecisionControls::Kind::reference_flux);
    check("scene.build_negative_reference_controls",
          validate_information_scene_build(ctx, broken).code ==
              Code::invalid_request,
          code_name(validate_information_scene_build(ctx, broken).code));
    auto radius = scene_build_input(PrecisionControls::Kind::actual_scene,
                                    10.0 * 3.14159265358979323846 / 180.0);
    check("scene.build_negative_radius",
          validate_information_scene_build(ctx, radius).code ==
              Code::unsupported,
          code_name(validate_information_scene_build(ctx, radius).code));
  }
  {
    auto input = scene_reconstruct_input();
    check("scene.reconstruct_validate_ok",
          validate_information_scene_reconstruct(ctx, input).code == Code::ok,
          "ok");
    record_plan("information.scene", "reconstruct_scene_variance",
                plan_information_scene_reconstruct(ctx, input, options));
    auto broken =
        scene_reconstruct_input(PrecisionControls::Kind::reference_flux);
    check("scene.reconstruct_negative_reference_controls",
          validate_information_scene_reconstruct(ctx, broken).code ==
              Code::invalid_request,
          code_name(validate_information_scene_reconstruct(ctx, broken).code));
  }
  {
    auto input = sky_controls_input();
    check("sky.controls_validate_ok",
          validate_sky_controls(ctx, input).code == Code::ok, "ok");
    record_plan("sky.controls", "build_sky_controls",
                plan_sky_controls(ctx, input, options));
    auto broken = std::make_shared<BuildSkyControlsInput>(*sky_controls_input());
    broken->request.groups.clear();
    check("sky.controls_negative_no_group",
          validate_sky_controls(ctx, broken).code == Code::invalid_request,
          code_name(validate_sky_controls(ctx, broken).code));
  }
  {
    auto input = sky_fit_input();
    check("sky.fit_validate_ok", validate_sky_fit(ctx, input).code == Code::ok,
          "ok");
    record_plan("sky.fit", "fit_relative_sky",
                plan_sky_fit(ctx, input, options));
    auto broken = sky_fit_input(false);
    check("sky.fit_negative_missing_noise",
          validate_sky_fit(ctx, broken).code == Code::invalid_model,
          code_name(validate_sky_fit(ctx, broken).code));
  }
  {
    auto input = std::make_shared<ApplySkyInput>();
    auto signal = std::make_shared<SignalEstimate>();
    signal->signal = make_array(DType::f64, {8}, "signal");
    signal->identity.object_id = "fixture.signal";
    signal->identity.content_hash = "sha256:" + std::string(64, 'e');
    signal->assessment.complete = true;
    auto fit = std::make_shared<SkyFit>();
    fit->beta = make_array(DType::f64, {1}, "beta");
    fit->identity.object_id = "fixture.fit";
    fit->identity.content_hash = "sha256:" + std::string(64, 'f');
    fit->assessment.complete = true;
    input->signal = signal;
    input->fit = fit;
    check("sky.apply_validate_ok",
          validate_sky_apply(ctx, input).code == Code::ok, "ok");
    record_plan("sky.fit", "apply_sky", plan_sky_apply(ctx, input, options));
    auto broken = std::make_shared<ApplySkyInput>();
    broken->signal = signal;
    check("sky.apply_negative_missing_fit",
          validate_sky_apply(ctx, broken).code == Code::invalid_request,
          code_name(validate_sky_apply(ctx, broken).code));
  }
  {
    auto input = select_input();
    check("coadd.select_validate_ok",
          validate_coadd_select(ctx, input).code == Code::ok, "ok");
    record_plan("coadd.select", "select_samples",
                plan_coadd_select(ctx, input, options));
    auto broken = std::make_shared<SelectSamplesInput>(*select_input());
    broken->request.rejection.kind = RejectionPolicy::Kind::single_max_loo;
    check("coadd.select_negative_loo_without_proof",
          validate_coadd_select(ctx, broken).code == Code::invalid_request,
          code_name(validate_coadd_select(ctx, broken).code));
  }
  {
    auto input = weights_input();
    check("coadd.weights_validate_ok",
          validate_coadd_weights(ctx, input).code == Code::ok, "ok");
    record_plan("coadd.weights", "solve_weights",
                plan_coadd_weights(ctx, input, options));
    auto broken = std::make_shared<SolveWeightsInput>(*weights_input());
    broken->request.mixed_observation = true;
    broken->request.weights = WeightPolicy::blue;
    check("coadd.weights_negative_mixed_blue",
          validate_coadd_weights(ctx, broken).code == Code::invalid_request,
          code_name(validate_coadd_weights(ctx, broken).code));
  }
  {
    auto input = integrate_input();
    check("coadd.integrate_validate_ok",
          validate_coadd_integrate(ctx, input).code == Code::ok, "ok");
    record_plan("coadd.integrate", "coadd",
                plan_coadd_integrate(ctx, input, options));
    auto other_selection =
        std::make_shared<Selection>(*selection_fixture(2, 1));
    other_selection->identity.content_hash =
        "sha256:" + std::string(64, '0');
    auto mismatched = std::make_shared<CoaddInput>(*integrate_input());
    auto weights = std::make_shared<WeightSolution>(*mismatched->weights);
    weights->selection = other_selection;
    mismatched->weights = weights;
    check("coadd.integrate_negative_selection_identity",
          validate_coadd_integrate(ctx, mismatched).code ==
              Code::identity_conflict,
          code_name(validate_coadd_integrate(ctx, mismatched).code));
    auto integrate_plan = plan_coadd_integrate(ctx, input, options);
    Ref<rt::InvocationPlan> integrate_plan_ref;
    if (integrate_plan.value)
      integrate_plan_ref =
          std::make_shared<const rt::InvocationPlan>(*integrate_plan.value);
    auto run = rt::wait_external(
        run_coadd_integrate(ctx, input, integrate_plan_ref));
    check("coadd.integrate_run_named_unsupported",
          run.code == Code::unsupported, code_name(run.code));
  }

  std::printf("{\"case\":\"m5_process_plan\",\"checks\":%d,\"failures\":%d}\n",
              g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
