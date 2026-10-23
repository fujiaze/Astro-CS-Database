// The projection.export typed process adapter: one operation that calls the
// one export implementation (src/projection/export_projection.cpp).  The plan
// sizes the target pixel workspace and the shared leaf/Q closure from the real
// request/product metadata; it never materialises the full sky.
#include <acsd/modules/generated/status.hpp>
#include <acsd/modules/projection.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "model/array.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "runtime/internal.hpp"

namespace acsd::modules {

namespace {

using rt::Done;

constexpr const char* kProcess = "projection.export";

template <class T>
Result<T> failure(Code code, std::string reason, std::string impact) {
  return model::fail<T>(code, std::move(reason), kProcess, std::move(impact));
}

U64 sat_mul(U64 a, U64 b) {
  if (a == 0 || b == 0) return 0;
  if (a > std::numeric_limits<U64>::max() / b)
    return std::numeric_limits<U64>::max();
  return a * b;
}

U64 element_count(const std::vector<U64>& shape) {
  U64 total = 1;
  for (U64 extent : shape) total = sat_mul(total, extent);
  return total;
}

U64 array_bytes(const ArrayRef& array) {
  if (!array) return 0;
  return sat_mul(element_count(array->shape),
                 static_cast<U64>(model::dtype_size(array->dtype)));
}

U64 wcs_metadata_bytes(const Ref<WcsModel>& wcs) {
  if (!wcs) return 0;
  U64 bytes = 2048;
  bytes += sat_mul(static_cast<U64>(wcs->sip_a.terms.size()), sizeof(Term2));
  bytes += sat_mul(static_cast<U64>(wcs->sip_b.terms.size()), sizeof(Term2));
  bytes += sat_mul(static_cast<U64>(wcs->original_header.size()), 96);
  return bytes;
}

U64 product_bytes(const Ref<ProductGraph>& graph) {
  if (!graph) return 0;
  U64 bytes = 4096;
  bytes += sat_mul(static_cast<U64>(graph->closure.size()), 1024);
  if (graph->signal) {
    bytes += array_bytes(graph->signal->signal);
    if (graph->signal->support)
      bytes += array_bytes(graph->signal->support->states) +
               array_bytes(graph->signal->support->area_lower_sr) +
               array_bytes(graph->signal->support->area_upper_sr) + 1024;
  }
  return bytes;
}

U64 support_leaf_count(const Ref<ProductGraph>& graph) {
  if (!graph || !graph->signal || !graph->signal->support ||
      !graph->signal->support->index)
    return 0;
  return graph->signal->support->index->size;
}

Id digest(std::initializer_list<std::string> parts) {
  std::string text;
  for (const std::string& part : parts) {
    text += part;
    text.push_back('|');
  }
  return "sha256:" + model::sha256_hex(text);
}

rt::PlanBounds bounds_of(U64 max_block, bool spill) {
  rt::PlanBounds bounds;
  bounds.min_block_elements = 1;
  bounds.max_block_elements = std::max<U64>(1, max_block);
  bounds.max_window = 1;
  bounds.allow_spill = spill;
  return bounds;
}

rt::InvocationPlan plan_of(const rt::PlanOptions& options, U64 work,
                           U64 persistent, U64 progress, const Id& input_hash) {
  rt::InvocationPlan plan;
  plan.input_hash = input_hash;
  plan.request_hash = digest({"build_export", input_hash});
  plan.options = options;
  plan.elements = std::max<U64>(1, work);
  plan.block_elements =
      options.block_elements > 0 ? options.block_elements : plan.elements;
  plan.block_elements = std::min(plan.block_elements, plan.elements);
  plan.cohorts = (plan.elements + plan.block_elements - 1) / plan.block_elements;
  plan.persistent.host_bytes = persistent;
  plan.progress_floor.host_bytes = progress;
  plan.control_bytes = 1024;
  plan.cleanup_work_upper = 1;
  plan.adaptive = false;
  rt::RegionSpec region;
  region.id = 0;
  region.kind = rt::RegionKind::stream;
  region.cohorts = std::max<U64>(1, plan.cohorts);
  region.persistent = plan.persistent;
  region.progress_floor = plan.progress_floor;
  rt::StageSpec stage;
  stage.id = 0;
  stage.work_upper = plan.elements;
  stage.control_bytes = plan.control_bytes;
  region.stages.push_back(std::move(stage));
  plan.regions.push_back(std::move(region));
  return plan;
}

Result<Done> validate_build_export(rt::InvocationContext ctx,
                                   Ref<BuildExportInput> input) {
  if (!input)
    return failure<Done>(Code::invalid_request, "the typed input is null",
                         "the export cannot be built");
  if (!ctx.ids.process.empty() && ctx.ids.process != kProcess)
    return failure<Done>(Code::invalid_request,
                         "the invocation process id does not match " +
                             std::string(kProcess),
                         "the export cannot be built");
  if (!ctx.ids.operation.empty() && ctx.ids.operation != "build_export")
    return failure<Done>(Code::invalid_request,
                         "the invocation operation id does not match "
                         "build_export",
                         "the export cannot be built");
  if (!input->product)
    return failure<Done>(Code::invalid_request,
                         "field 'product' is a required object reference",
                         "the export cannot be built");
  if (!input->request.target.model)
    return failure<Done>(Code::invalid_request,
                         "the target has no WCS model",
                         "the export cannot be built");
  if (input->request.target.width == 0 || input->request.target.height == 0)
    return failure<Done>(Code::invalid_request,
                         "the target size is empty",
                         "the export cannot be built");
  if (sat_mul(input->request.target.width, input->request.target.height) ==
      std::numeric_limits<U64>::max())
    return failure<Done>(Code::invalid_request,
                         "the target pixel count overflows",
                         "the export cannot be built");
  if (input->request.output_dtype != DType::f64 &&
      input->request.output_dtype != DType::f32)
    return failure<Done>(Code::unsupported,
                         "the export supports f32 and f64 only",
                         "the export cannot be built");
  if (input->request.quantity ==
          ExportRequest::Quantity::extrapolated_full_flux &&
      !input->request.assume_uniform_uncovered)
    return failure<Done>(Code::invalid_request,
                         "the extrapolated flux needs the explicit uniform "
                         "uncovered assumption",
                         "the export cannot be built");
  if (input->request.budget.arithmetic.max_bits < 8 ||
      input->request.budget.arithmetic.max_bits > 65536)
    return failure<Done>(Code::invalid_request,
                         "arithmetic.max_bits must be within 8..65536",
                         "the export cannot be built");
  const Ref<ProductGraph> graph = input->product->graph();
  if (!graph || !graph->signal)
    return failure<Done>(Code::missing_dependency,
                         "the product has no signal estimate",
                         "the export cannot be built");
  if (!graph->signal->support)
    return failure<Done>(Code::missing_dependency,
                         "the product has no support map",
                         "the export cannot be built");
  return model::ok_result(Done{});
}

Result<rt::PlanBounds> bounds_build_export(rt::InvocationContext,
                                           Ref<BuildExportInput> input) {
  const U64 pixels = std::max<U64>(
      1, sat_mul(input->request.target.width, input->request.target.height));
  return model::ok_result(bounds_of(pixels, true));
}

Result<rt::InvocationPlan> plan_build_export(rt::InvocationContext,
                                             Ref<BuildExportInput> input,
                                             rt::PlanOptions options) {
  const U64 pixels = std::max<U64>(
      1, sat_mul(input->request.target.width, input->request.target.height));
  const Ref<ProductGraph> graph =
      input->product ? input->product->graph() : Ref<ProductGraph>{};
  const U64 leaves = std::max<U64>(1, support_leaf_count(graph));
  const U64 work = std::max<U64>(
      input->request.budget.max_work > 0 ? input->request.budget.max_work : 1,
      pixels + leaves);
  // R: the target WCS, build/closure identity and the shared leaf/Q metadata.
  const U64 r = wcs_metadata_bytes(input->request.target.model) +
                product_bytes(graph) + sat_mul(leaves, 24) + 4096;
  // F: the target pixel workspace (signal, bounds, support) and the per-leaf
  // projection spool.
  const U64 f = sat_mul(pixels, sizeof(double) * 3) +
                sat_mul(leaves, sizeof(double) * 4 + 3) + 64 * 1024;
  return model::ok_result(plan_of(
      options, work, r, f,
      digest({"build_export",
              graph ? graph->identity.object_id : Id{},
              input->request.target.model
                  ? input->request.target.model->identity.object_id
                  : Id{},
              std::to_string(pixels),
              std::to_string(static_cast<int>(input->request.quantity)),
              std::to_string(static_cast<int>(input->request.support_policy))})));
}

rt::Task<ExportResult> run_build_export(rt::InvocationContext ctx,
                                        Ref<BuildExportInput> input,
                                        Ref<rt::InvocationPlan> plan) {
  if (!plan)
    return rt::ready<ExportResult>(
        ctx, failure<ExportResult>(Code::invalid_request,
                                   "no invocation plan supplied",
                                   "the export cannot run"));
  U64 work = plan->elements;
  if (!plan->regions.empty() && !plan->regions.front().stages.empty())
    work = plan->regions.front().stages.front().work_upper;
  rt::TaskSpec spec;
  spec.key = rt::Key{plan->epoch, 0, 0, 0};
  spec.running.cpu_tokens = 1;
  spec.work_upper = std::max<U64>(1, work);
  spec.semantic = Id("build_export");
  return rt::submit_cpu<ExportResult>(
      ctx, spec,
      [input](rt::InvocationContext task) -> Result<ExportResult> {
        Result<std::shared_ptr<rt::detail::ScienceContext>> bridge =
            rt::detail::make_science_context(task, task.progress);
        if (!ok(bridge) || !bridge.value)
          return failure<ExportResult>(
              bridge.code == Code::ok ? Code::internal : bridge.code,
              "the runtime science-context bridge cannot be created",
              "the export cannot run");
        return build_export((*bridge.value)->context(), *input->product,
                            input->request);
      });
}

rt::Task<ExportResult> finalize_build_export(
    rt::InvocationContext ctx, Ref<BuildExportInput> input,
    Ref<rt::InvocationPlan>, Result<ExportResult> partial) {
  if (partial.code != Code::ok || !partial.value)
    return rt::ready<ExportResult>(ctx, std::move(partial));
  const ExportResult& result = *partial.value;
  Result<Done> checked = model::ok_result(Done{});
  const U64 pixels = sat_mul(input->request.target.width,
                             input->request.target.height);
  if (!result.estimate)
    checked = failure<Done>(Code::invalid_model,
                            "the export estimate is null", "not publishable");
  else if (!result.estimate->signal)
    checked = failure<Done>(Code::invalid_model,
                            "the export estimate carries no signal array",
                            "not publishable");
  else if (!result.target.model)
    checked = failure<Done>(Code::invalid_model,
                            "the export result has no target WCS",
                            "not publishable");
  else if (!result.Q)
    checked = failure<Done>(Code::invalid_model,
                            "the export result has no projection action",
                            "not publishable");
  else if (element_count(result.estimate->signal->shape) != pixels)
    checked = failure<Done>(Code::invalid_model,
                            "the exported signal does not cover the target "
                            "pixels",
                            "not publishable");
  else if (result.estimate->signal->unit.scale.empty())
    checked = failure<Done>(Code::invalid_model,
                            "the exported signal has no unit", "not publishable");
  else if (result.estimate->numerical_lower &&
           result.estimate->numerical_upper &&
           (element_count(result.estimate->numerical_lower->shape) != pixels ||
            element_count(result.estimate->numerical_upper->shape) != pixels))
    checked = failure<Done>(Code::invalid_model,
                            "the exported bounds do not cover the target "
                            "pixels",
                            "not publishable");
  if (!ok(checked)) {
    Result<ExportResult> failed =
        failure<ExportResult>(checked.code,
                              "the export result violates its immutable output "
                              "contract",
                              "the result is not published");
    return rt::ready<ExportResult>(ctx, std::move(failed));
  }
  return rt::ready<ExportResult>(ctx, std::move(partial));
}

}  // namespace

// Public plan introspection for the independent process tests.
Result<rt::InvocationPlan> plan_projection_export_build_export(
    rt::InvocationContext ctx, Ref<BuildExportInput> input,
    rt::PlanOptions options) {
  return plan_build_export(ctx, std::move(input), options);
}

Result<rt::Done> register_projection_export(rt::RegistryBuilder& builder) {
  Result<rt::ProcessDescriptor> descriptor =
      acsd::generated::descriptor_for(kProcess);
  if (!ok(descriptor))
    return failure<Done>(
        descriptor.code,
        "the generated descriptor for projection.export is unavailable",
        "the process cannot be registered");
  Result<std::shared_ptr<rt::ProcessRegistration>> process =
      builder.begin(*descriptor.value);
  if (!ok(process))
    return failure<Done>(process.code,
                         "the projection.export process cannot be begun",
                         "the process cannot be registered");
  return builder.add<BuildExportInput, ExportResult>(
      *process.value,
      rt::Operation<BuildExportInput, ExportResult>{
          "build_export", {"acsd.input.BuildExportInput", 1},
          {"acsd.ExportResult", 1}, &validate_build_export,
          &bounds_build_export, &plan_build_export, &run_build_export,
          &finalize_build_export});
}

}  // namespace acsd::modules
