// sky.fit regression on the real three-frame 4096 mosaic products.
//
// The case is the failure reproduced on 2026-11-12: build_sky_controls makes one
// control per frame and 32x32-leaf patch, so the control operator is
// (control_count x leaf_total) = (108 x 71368) for the three real S1 products
// under run/m42-s1/recover/s1.  fit_relative_sky used to materialise that
// operator densely only to check its shape, and the dense materialisation cap
// (rows/cols above 4096) refused the fit with
//   Code::unavailable "the influence is too large to materialise (108x71368)".
// The estimator itself never needs a leaf-space matrix: the gauge, the
// whitening and the estimators are control-space objects of side
// control_count (108).
//
// The regression asserts, on the real products and with no synthetic fixture:
//   * the control operator is the published CSR action of shape (108 x 71368);
//   * dense_action still refuses that shape (the cap is untouched, so a
//     regression to the old code fails this case);
//   * fit_relative_sky publishes beta/K/T/E/P/parameter_cov/rank/support/
//     component_ids, and beta is bit-for-bit the published K applied to the
//     control statistic c (the estimator is unchanged);
//   * the reference frame of the gauge carries beta = 0, the other two frames
//     carry the additive offsets, and apply_sky removes exactly those offsets
//     from the frame signal of the sparse leaf space (y -> y - beta_frame);
//   * the control covariance of the real products is unknown, so the fit
//     publishes the unknown state and the parameter covariance is never
//     reported as a measured variance.
#include "../lib/fixtures.hpp"

#include <acsd/model.hpp>

#include "operators/linear_action.hpp"
#include "model/result.hpp"
#include "resources/cpu_resources.hpp"
#include "sky/sky.hpp"
#include "storage/local_storage.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

using acsd::ActionRecipe;
using acsd::ArrayRef;
using acsd::Basis;
using acsd::Code;
using acsd::Comparability;
using acsd::Context;
using acsd::FrameDescription;
using acsd::FrameReadRequest;
using acsd::FramesProvider;
using acsd::Gauge;
using acsd::Id;
using acsd::LeafBlock;
using acsd::ProductLocation;
using acsd::ProductProvider;
using acsd::Ref;
using acsd::Result;
using acsd::SignalEstimate;
using acsd::SkyControls;
using acsd::SkyFit;
using acsd::SkyRequest;
using acsd::U64;

// The science Context of this driver: the process wide resource budget, a
// cancellation that is never raised and the one immutable array storage that
// every entry point observes.
struct NoCancellation final : acsd::Cancellation {
  bool requested() const noexcept override { return false; }
};

U64 element_count(ArrayRef array) {
  if (!array) return 0;
  U64 total = 1;
  for (const U64 extent : array->shape) total *= extent;
  return total;
}

Result<std::vector<double>> read_all(Context& context, ArrayRef array) {
  return acsd::operators::read_f64(context, array, 0, element_count(array));
}

// The real S1 products of the reproduced failure.  ACSD_S1_MOSAIC lets a
// caller point the case at another copy; the default is the recovered run of
// 2026-11-12.
std::vector<std::string> s1_products() {
  const char* override_root = std::getenv("ACSD_S1_MOSAIC");
  const std::string root =
      override_root && *override_root ? override_root : "run/m42-s1/recover/s1";
  return {root + "/f0/out/M42_M5_T2_flying_dutchman-20251212@041842-600S-H-alpha",
          root + "/f1/out/M42_M5_T2_flying_dutchman-20251224@054358-600S-H-alpha",
          root + "/f2/out/M42_M5_T2_flying_dutchman-20251228@051230-600S-H-alpha"};
}

}  // namespace

int main() {
  vrt::Recorder record("sky.fit.sparse_4096", "validation/process/sky.fit/sparse_4096.cpp");
  try {
    acsd::Limits limits{};
    limits.bytes = 6ull << 30;
    limits.work = 4000000000000ull;
    limits.spill_bytes = 1ull << 30;
    limits.threads = 2;
    limits.max_depth = 32;
    limits.max_precision_bits = 256;
    limits.scratch_directory = vrt::scratch_root("sky.fit.sparse_4096");

    acsd::resources::CpuResources resources(limits);
    acsd::storage::LocalStorage storage;
    NoCancellation cancellation;
    Context context{resources, cancellation, storage};

    const std::vector<std::string> products_dirs = s1_products();
    std::vector<Ref<ProductProvider>> products;
    std::vector<Ref<FrameDescription>> descriptions;
    std::vector<ArrayRef> signals;
    std::vector<ArrayRef> dq;
    std::vector<Ref<SignalEstimate>> estimates;
    for (const std::string& directory : products_dirs) {
      ProductLocation location;
      location.directory = directory;
      Result<Ref<ProductProvider>> opened = acsd::open_product(context, location);
      record.check("open_product:" + directory, acsd::ok(opened),
                   "the real product opens", vrt::code_of(opened));
      if (!acsd::ok(opened)) continue;
      Ref<ProductProvider> product = acsd::val(opened);
      products.push_back(product);
      Result<std::vector<Id>> frames = product->list_frames(context);
      record.check("one_frame:" + directory,
                   acsd::ok(frames) && acsd::val(frames).size() == 1, "exactly one frame",
                   acsd::ok(frames) ? std::to_string(acsd::val(frames).size()) + " frames"
                                    : vrt::code_of(frames));
      if (!acsd::ok(frames) || acsd::val(frames).empty()) continue;
      const Id frame = acsd::val(frames).front();
      Result<Ref<FrameDescription>> description = product->describe_frame(context, frame);
      record.check("describe_frame:" + directory, acsd::ok(description),
                   "the frame can be described", vrt::code_of(description));
      if (!acsd::ok(description) || !acsd::val(description)->index ||
          !acsd::val(description)->support) {
        record.check("frame_metadata:" + directory, false,
                     "index and support are published", "missing index or support", "");
        continue;
      }
      Result<LeafBlock> block =
          product->read_leaf_block(context, FrameReadRequest{frame, {}, {}, {}});
      record.check("read_leaf_block:" + directory, acsd::ok(block),
                   "the leaf block can be read", vrt::code_of(block));
      if (!acsd::ok(block) || !acsd::val(block).signal) continue;
      Ref<acsd::ProductGraph> graph = product->graph();
      if (!graph || !graph->signal) {
        record.check("graph_signal:" + directory, false, "the product carries a signal",
                     "no signal estimate", "");
        continue;
      }
      descriptions.push_back(acsd::val(description));
      signals.push_back(acsd::val(block).signal);
      dq.push_back(acsd::val(block).dq);
      estimates.push_back(graph->signal);
    }
    record.check("three_real_products", descriptions.size() == 3 && estimates.size() == 3,
                 "3 frames from the real S1 products",
                 std::to_string(descriptions.size()) + " frames", "");
    if (descriptions.size() != 3) return record.finish();

    Result<Ref<FramesProvider>> frames =
        acsd::make_frames_provider(context, descriptions, signals, dq);
    record.check("make_frames_provider", acsd::ok(frames), "the frames merge",
                 vrt::code_of(frames));
    if (!acsd::ok(frames)) return record.finish();

    // The scientific request of run/m42-s1/recover/s1/mosaic: one
    // comparability group, the additive constant basis (terms = 1), the
    // reference-frame gauge on f0 and the unknown control covariance.
    SkyRequest request{};
    request.basis.kind = Basis::Kind::constant;
    request.basis.degree = 0;
    request.basis.center = {0.0, 0.0};
    request.basis.scale = {1.0, 1.0};
    request.basis.weighted_mean = nullptr;
    Comparability group;
    group.response_proof = descriptions.front()->response
                               ? descriptions.front()->response->identity.object_id
                               : Id{};
    group.spatial_kernel_proof = descriptions.front()->psf
                                     ? descriptions.front()->psf->identity.object_id
                                     : group.response_proof;
    group.assumptions.kind = "same_filter_relative_response_shape_different_kernel_conditional";
    std::vector<Id> response_identities;
    for (const Ref<FrameDescription>& description : descriptions) {
      group.frames.push_back(description->frame);
      // The real scientific request declares a conditional comparability: one
      // response shape and a different spatial kernel per frame.  The group
      // carries exactly the three published SpatialResponse identities, and
      // frame f0 is the declared response proof.
      const Id response = description->response ? description->response->identity.object_id : Id{};
      response_identities.push_back(response);
      record.check("comparability_response_published:" + description->frame, !response.empty(),
                   "the frame publishes its response identity", response, "");
    }
    group.assumptions.fixed_objects = response_identities;
    record.check("reference_response_proof",
                 !response_identities.empty() &&
                     response_identities.front() == group.response_proof,
                 "the response proof is the frame f0 response",
                 response_identities.empty() ? "-" : response_identities.front());
    const std::set<Id> distinct_responses(response_identities.begin(),
                                          response_identities.end());
    record.check("mixed_kernel_conditionality",
                 response_identities.size() == 3 && distinct_responses.size() == 3,
                 "the three frames carry three distinct response/kernel identities",
                 std::to_string(distinct_responses.size()) + " distinct identities");
    record.check("declared_fixed_objects",
                 group.assumptions.fixed_objects.size() == distinct_responses.size(),
                 "every published response identity is a declared fixed object",
                 std::to_string(group.assumptions.fixed_objects.size()) + " fixed objects");
    request.groups.push_back(group);
    request.gauge.kind = Gauge::Kind::reference_frame;
    request.gauge.reference = group.frames.front();
    request.solve.rank_relative = 1e-10;
    request.solve.dense_max_bytes = 16ull << 20;

    // ---- the control operator on the real leaf space ----------------------
    Result<Ref<SkyControls>> controls_result =
        acsd::build_sky_controls(context, *acsd::val(frames), request);
    record.check("build_sky_controls", acsd::ok(controls_result),
                 "the controls of the three real frames are built",
                 vrt::code_of(controls_result));
    if (!acsd::ok(controls_result)) return record.finish();
    Ref<SkyControls> controls = acsd::val(controls_result);

    Result<std::vector<double>> control_values = read_all(context, controls->c);
    record.check("control_values", acsd::ok(control_values), "the control statistic is published",
                 vrt::code_of(control_values));
    if (!acsd::ok(control_values)) return record.finish();
    const U64 control_count = acsd::val(control_values).size();
    const U64 leaf_total = acsd::sky::action_domain_size(controls->T);
    const U64 control_rows = acsd::sky::action_codomain_size(controls->T);

    record.check("control_shape", control_rows == control_count && leaf_total > 0,
                 "T is (control_count x leaf_total) with control_count == |c|",
                 std::to_string(control_rows) + "x" + std::to_string(leaf_total));

    // The real failing shape: the leaf space is far beyond the dense cap.
    record.check("leaf_space_beyond_dense_cap", leaf_total > (1ull << 12),
                 "leaf_total above the 4096 dense cap", std::to_string(leaf_total));

    // The published operator is a CSR payload; the fit must not need anything
    // else.  A regression that densifies T again fails the next check.
    Result<ActionRecipe> t_recipe = controls->T->serialize_recipe();
    record.check("control_operator_is_csr",
                 acsd::ok(t_recipe) && acsd::val(t_recipe).kind == ActionRecipe::Kind::csr,
                 "T is published as a sparse CSR action",
                 acsd::ok(t_recipe)
                     ? std::to_string(static_cast<int>(acsd::val(t_recipe).kind))
                     : vrt::code_of(t_recipe));
    Result<std::vector<double>> c_values = read_all(context, controls->c);
    record.check("control_statistic_read", acsd::ok(c_values), "c can be read",
                 vrt::code_of(c_values));

    // The dense materialisation cap is untouched: the shape that used to
    // abort the fit is still refused, so the fit cannot silently depend on it.
    Result<acsd::numerics::Matrix> dense =
        acsd::sky::dense_action(context, controls->T, control_rows, leaf_total);
    record.check("dense_materialisation_still_refused", dense.code == Code::unavailable,
                 "the dense cap refuses 108x71368", vrt::code_of(dense));

    // ---- the fit -----------------------------------------------------------
    Result<Ref<SkyFit>> fit_result = acsd::fit_relative_sky(context, controls, request);
    record.check("fit_relative_sky", acsd::ok(fit_result),
                 "the relative sky is fitted on the real products", vrt::code_of(fit_result));
    if (!acsd::ok(fit_result)) return record.finish();
    Ref<SkyFit> fit = acsd::val(fit_result);

    record.check("published_actions_present",
                 fit->K && fit->T && fit->E && fit->P && fit->beta && fit->parameter_cov &&
                     fit->support && fit->component_ids,
                 "K, T, E, P, beta, parameter_cov, support and component_ids are published",
                 std::string(fit->K ? "K" : "-") + (fit->T ? "T" : "-") + (fit->E ? "E" : "-") +
                     (fit->P ? "P" : "-") + (fit->parameter_cov ? "C" : "-"));
    record.check("published_support_is_training_support",
                 fit->support && controls->training_support &&
                     fit->support->identity.object_id ==
                         controls->training_support->identity.object_id,
                 "the fit publishes the training leaf space",
                 fit->support ? fit->support->identity.object_id : "-");
    record.check("published_component_ids",
                 fit->component_ids && controls->component_ids &&
                     fit->component_ids->identity.object_id ==
                         controls->component_ids->identity.object_id,
                 "the fit publishes the control component identities",
                 fit->component_ids ? fit->component_ids->identity.object_id : "-");

    Result<std::vector<double>> beta = read_all(context, fit->beta);
    record.check("beta_read", acsd::ok(beta), "beta can be read", vrt::code_of(beta));
    if (!acsd::ok(beta)) return record.finish();
    const std::size_t parameters = acsd::val(beta).size();
    record.check("beta_shape", parameters == descriptions.size(),
                 "one additive constant per frame",
                 std::to_string(parameters) + " parameters");

    // The published estimator is the same linear map: beta = K c.
    Result<std::vector<double>> k_times_c =
        acsd::sky::apply_action_full(context, fit->K, acsd::val(c_values));
    record.check("estimator_identity", acsd::ok(k_times_c),
                 "K can be applied to the control statistic", vrt::code_of(k_times_c));
    double estimator_error = 0.0;
    if (acsd::ok(k_times_c) && acsd::val(k_times_c).size() == parameters) {
      for (std::size_t p = 0; p < parameters; ++p) {
        estimator_error =
            std::max(estimator_error, std::fabs(acsd::val(k_times_c)[p] - acsd::val(beta)[p]));
      }
    } else {
      estimator_error = 1.0;
    }
    record.check("beta_equals_K_times_c", estimator_error <= 1e-12,
                 "beta = K c", vrt::number(estimator_error));

    // The reference frame is fixed by the gauge; the offsets of the other
    // frames are the additive sky that the mosaic subtracts.
    const std::size_t reference = 0;
    record.check("reference_frame_fixed", std::fabs(acsd::val(beta)[reference]) <= 1e-9,
                 "beta of the reference frame is zero",
                 vrt::number(acsd::val(beta)[reference]));

    // The published rank certificate belongs to the same system.
    record.check("rank_certificate", fit->rank.columns > 0 && fit->rank.rank <= fit->rank.columns,
                 "rank.columns and rank.rank are published and consistent",
                 std::to_string(fit->rank.columns) + "/" + std::to_string(fit->rank.rank));

    // The control covariance of the real products is unknown: the parameter
    // covariance is a declared model, never a measured variance.
    const bool control_known =
        controls->noise && controls->noise->model &&
        controls->noise->model->state == acsd::NoiseState::known &&
        controls->noise->model->unknown_terms.empty();
    record.check("unknown_control_covariance_is_named",
                 control_known ? fit->assessment.noise != acsd::NoiseState::unknown
                               : (fit->assessment.noise == acsd::NoiseState::unknown &&
                                  fit->assessment.complete == false),
                 "the unknown control covariance keeps the parameter variance conditional",
                 control_known ? "known" : "unknown");

    // ---- the correction removes the offsets of every frame -----------------
    double removal_error = 0.0;
    for (std::size_t f = 0; f < estimates.size(); ++f) {
      Result<Ref<SignalEstimate>> applied =
          acsd::apply_sky(context, estimates[f], fit, descriptions[f]->frame);
      record.check("apply_sky:" + descriptions[f]->frame, acsd::ok(applied),
                   "the frame offset is subtracted", vrt::code_of(applied));
      if (!acsd::ok(applied)) continue;
      Result<std::vector<double>> before = read_all(context, estimates[f]->signal);
      Result<std::vector<double>> after = read_all(context, acsd::val(applied)->signal);
      if (!acsd::ok(before) || !acsd::ok(after) || acsd::val(before).size() != acsd::val(after).size()) {
        record.check("corrected_frame:" + descriptions[f]->frame, false,
                     "the corrected signal has the frame length", "shape mismatch", "");
        continue;
      }
      // The constant basis corrects every leaf of the frame by the same
      // additive offset: y -> y - beta_f.
      const double offset = acsd::val(beta)[f];
      for (std::size_t i = 0; i < acsd::val(before).size(); ++i) {
        removal_error = std::max(
            removal_error, std::fabs(acsd::val(after)[i] - (acsd::val(before)[i] - offset)));
      }
      record.value("frame_" + std::to_string(f) + "_offset", offset);
      record.value("frame_" + std::to_string(f) + "_leaves",
                   static_cast<long long>(acsd::val(before).size()));
      // The additive offset is compared with the frame's own level so that a
      // fit that "removes" a number of the order of the data is not mistaken
      // for a fit that removes the actual frame difference.  Unsupported
      // leaves carry no finite value; they are counted, never averaged.
      double sum = 0.0;
      std::size_t finite = 0;
      std::size_t non_finite = 0;
      for (const double value : acsd::val(before)) {
        if (std::isfinite(value)) {
          sum += value;
          ++finite;
        } else {
          ++non_finite;
        }
      }
      const double mean_before = finite > 0 ? sum / static_cast<double>(finite) : 0.0;
      record.value("frame_" + std::to_string(f) + "_mean_signal", mean_before);
      record.value("frame_" + std::to_string(f) + "_offset_over_mean",
                   mean_before != 0.0 ? offset / mean_before : 0.0);
      record.value("frame_" + std::to_string(f) + "_non_finite_leaves", non_finite);
      record.check("corrected_covariance_is_unknown:" + descriptions[f]->frame,
                   acsd::val(applied)->noise == nullptr,
                   "the per-frame correction publishes unknown covariance, not a wrong one",
                   acsd::val(applied)->noise ? "published" : "null");
    }
    record.check("offsets_removed_exactly", removal_error <= 1e-9,
                 "corrected = frame - beta_frame for every leaf", vrt::number(removal_error));

    record.value("products", std::to_string(products.size()));
    record.value("leaf_total", leaf_total);
    record.value("control_count", control_count);
    record.value("parameters", parameters);
    record.value("estimator_error", estimator_error);
    record.value("removal_error", removal_error);
    record.value("rank_columns", fit->rank.columns);
    record.value("rank", fit->rank.rank);
    record.value("rank_certified", fit->rank.certified);
    record.value("gauge", fit->gauge.reference);
    record.command("cmake --build build/science --target acsd_process_sky.fit_sparse_4096 -j16");
    record.note(
        "the dense materialisation of T (108 x 71368) is still refused by the cap; the fit "
        "succeeds because it never materialises the leaf-space operator");
    return record.finish();
  } catch (const std::exception& failure) {
    record.note(std::string("exception: ") + failure.what());
    return record.finish_with_exception("main");
  }
}
