// The public coadd entry points declared in include/acsd/contracts.hpp.
//
// Scientific contract (STATISTICS_SKY_COADD section 5 and SYSTEM M1):
//   * the eligibility comes from the data quality; the statistical rejection
//     is optional and conservative: one single maximum standardised leave one
//     out residual, computed once, with at most one rejected sample;
//   * the leave one out test needs N >= 4, a comparable common mean and an
//     externally fixed Gamma; when a prerequisite fails the position is
//     skipped with a reason and never guessed;
//   * the weights are equal, BLUE or the non negative quadratic program; the
//     automatic policy chooses BLUE only when the quantity is comparable and
//     the covariance is complete, otherwise it falls back to equal and records
//     the reason;
//   * a mixed observation only allows equal or an explicit non negative
//     program; the program solution is not called an unconstrained BLUE;
//   * coadd calls the same selection and weight procedure, it never
//     implements a second one and it never applies the sky twice.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "acsd/contracts.hpp"
#include "coadd/coadd.hpp"
#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "numerics/linalg.hpp"
#include "operators/linear_action.hpp"
#include "uncertainty/covariance.hpp"

namespace acsd {
namespace {

using numerics::Matrix;

// The reason codes of the selection.
enum Reason : std::uint16_t {
  kAccepted = 0,
  kDataQuality = 1,
  kNoSupport = 2,
  kNotObserved = 3,
  kRejectedByLeaveOneOut = 4,
  kRejectionSkipped = 5,
  kSelectionUnresolved = 6,
};

std::size_t count_elements(const std::vector<U64>& shape) {
  std::size_t total = 1;
  for (U64 extent : shape) {
    if (extent == 0) return 0;
    total *= static_cast<std::size_t>(extent);
  }
  return total;
}

Result<std::vector<double>> read_f64_array(Context& context, ArrayRef array) {
  if (!array) {
    return model::fail<std::vector<double>>(Code::invalid_request, "no array supplied",
                                            "coadd.read", "the array cannot be read");
  }
  if (array->dtype != DType::f64) {
    return model::fail<std::vector<double>>(Code::invalid_model, "the array is not f64",
                                            "coadd.read", "the array cannot be read");
  }
  const U64 count = static_cast<U64>(count_elements(array->shape));
  return operators::read_f64(context, array, 0, count);
}

Result<std::vector<U64>> read_u64_array(Context& context, ArrayRef array) {
  if (!array) {
    return model::fail<std::vector<U64>>(Code::invalid_request, "no array supplied", "coadd.read",
                                         "the array cannot be read");
  }
  if (array->dtype != DType::u64) {
    return model::fail<std::vector<U64>>(Code::invalid_model, "the array is not u64", "coadd.read",
                                         "the array cannot be read");
  }
  const U64 count = static_cast<U64>(count_elements(array->shape));
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, count});
  if (!ok(view)) {
    return model::fail<std::vector<U64>>(view.code, "the array cannot be read", "coadd.read",
                                         "the array cannot be read");
  }
  return model::ok_result(model::decode_u64(val(view).bytes));
}

Result<std::vector<std::uint8_t>> read_u8_array(Context& context, ArrayRef array) {
  if (!array) {
    return model::fail<std::vector<std::uint8_t>>(Code::invalid_request, "no array supplied",
                                                  "coadd.read", "the array cannot be read");
  }
  if (array->dtype != DType::u8) {
    return model::fail<std::vector<std::uint8_t>>(Code::invalid_model, "the array is not u8",
                                                  "coadd.read", "the array cannot be read");
  }
  const U64 count = static_cast<U64>(count_elements(array->shape));
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, count});
  if (!ok(view)) {
    return model::fail<std::vector<std::uint8_t>>(view.code, "the array cannot be read",
                                                  "coadd.read", "the array cannot be read");
  }
  return model::ok_result(model::decode_u8(val(view).bytes));
}

Result<std::vector<std::uint16_t>> read_u16_array(Context& context, ArrayRef array) {
  if (!array) {
    return model::fail<std::vector<std::uint16_t>>(Code::invalid_request, "no array supplied", "coadd.read",
                                         "the array cannot be read");
  }
  if (array->dtype != DType::u16) {
    return model::fail<std::vector<std::uint16_t>>(Code::invalid_model, "the array is not u16", "coadd.read",
                                         "the array cannot be read");
  }
  const U64 count = static_cast<U64>(count_elements(array->shape));
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, count});
  if (!ok(view)) {
    return model::fail<std::vector<std::uint16_t>>(view.code, "the array cannot be read", "coadd.read",
                                         "the array cannot be read");
  }
  return model::ok_result(model::decode_u16(val(view).bytes));
}

// A frame as it is used by the selection and the weights.
struct FrameView {
  Id frame;
  U32 order = 0;
  std::vector<HealpixLeaf> leaves;
  std::vector<double> signal;
  std::vector<std::uint16_t> dq;
  std::vector<std::uint8_t> state;
  std::map<std::pair<U64, U64>, std::size_t> row_of_leaf;
  Ref<CovarianceAction> noise;
};

Result<FrameView> read_frame(Context& context, const FramesProvider& frames, const Id& frame) {
  Result<Ref<FrameDescription>> described = frames.describe_frame(context, frame);
  if (!ok(described)) {
    return model::fail<FrameView>(Code::unavailable, "the frame cannot be described", "coadd.read",
                                  "the frame cannot be used");
  }
  const FrameDescription description = *val(described);
  if (!description.index) {
    return model::fail<FrameView>(Code::invalid_model, "the frame has no index", "coadd.read",
                                  "the frame cannot be used");
  }
  Result<std::vector<U64>> ids = read_u64_array(context, description.index->ids);
  if (!ok(ids)) {
    return model::fail<FrameView>(ids.code, "the leaf identities cannot be read", "coadd.read",
                                  "the frame cannot be used");
  }
  FrameView view;
  view.frame = frame;
  view.order = description.index->order;
  view.noise = description.noise;
  const std::vector<U64>& flat = val(ids);
  if (flat.size() % 2 != 0) {
    return model::fail<FrameView>(Code::invalid_model, "the leaf identities are malformed",
                                  "coadd.read", "the frame cannot be used");
  }
  const std::size_t count = flat.size() / 2;
  view.leaves.reserve(count);
  for (std::size_t row = 0; row < count; ++row) {
    view.leaves.push_back(HealpixLeaf{static_cast<U32>(flat[2 * row]), flat[2 * row + 1]});
    view.row_of_leaf[{flat[2 * row], flat[2 * row + 1]}] = row;
  }
  FrameReadRequest request;
  request.frame = frame;
  request.leaves = view.leaves;
  request.fields = {"signal", "dq"};
  Result<LeafBlock> read = frames.read_leaf_block(context, request);
  if (!ok(read)) {
    return model::fail<FrameView>(read.code, "the frame leaves cannot be read", "coadd.read",
                                  "the frame cannot be used");
  }
  Result<std::vector<double>> signal = read_f64_array(context, val(read).signal);
  if (!ok(signal)) {
    return model::fail<FrameView>(signal.code, "the frame signal cannot be read", "coadd.read",
                                  "the frame cannot be used");
  }
  view.signal = val(signal);
  if (val(read).dq) {
    Result<std::vector<std::uint16_t>> dq = read_u16_array(context, val(read).dq);
    if (!ok(dq)) {
      return model::fail<FrameView>(dq.code, "the frame quality cannot be read", "coadd.read",
                                    "the frame cannot be used");
    }
    view.dq = val(dq);
  }
  Result<Ref<SupportMap>> support = frames.query_support(context, frame, description.index);
  if (ok(support) && val(support) && val(support)->states) {
    Result<std::vector<std::uint8_t>> states = read_u8_array(context, val(support)->states);
    if (!ok(states)) {
      return model::fail<FrameView>(states.code, "the support states cannot be read", "coadd.read",
                                    "the frame cannot be used");
    }
    if (val(states).size() != count) {
      return model::fail<FrameView>(Code::invalid_model, "the support states disagree with the index",
                                    "coadd.read", "the frame cannot be used");
    }
    view.state = val(states);
  } else {
    view.state.assign(count, 1);
  }
  return model::ok_result(std::move(view));
}

// The joint covariance of the frames: the innovations stay separate and the
// shared roots are merged so that the cross frame correlation survives.
Result<Ref<CovarianceAction>> build_joint_covariance(Context& context,
                                                      const std::vector<FrameView>& frames) {
  auto joint = std::make_shared<NoiseModel>();
  std::vector<Ref<NoiseRoot>> innovations;
  std::vector<Ref<LinearAction>> innovation_influences;
  std::map<std::string, std::size_t> shared_index;
  std::vector<Ref<NoiseRoot>> shared_roots;
  std::vector<std::vector<operators::CsrMatrix>> shared_blocks;
  U64 total = 0;
  for (const FrameView& frame : frames) total += frame.leaves.size();

  auto to_csr = [&](Ref<LinearAction> action, U64 rows) -> Result<operators::CsrMatrix> {
    operators::CsrMatrix matrix;
    matrix.rows = rows;
    matrix.cols = 0;
    const auto* impl = dynamic_cast<const operators::ActionImpl*>(action.get());
    if (impl && (impl->recipe().kind == ActionRecipe::Kind::csr ||
                 impl->recipe().kind == ActionRecipe::Kind::leaf_merge)) {
      const operators::CsrMatrix& payload = impl->csr();
      matrix = payload;
      return model::ok_result(matrix);
    }
    // A dense materialisation is only used when the action has no sparse
    // payload; the small matrix path of the contract is the only case here.
    const U64 columns = impl ? impl->domain_size() : 0;
    matrix.cols = columns;
    matrix.offsets.assign(static_cast<std::size_t>(rows) + 1, 0);
    for (U64 j = 0; j < columns; ++j) {
      std::vector<double> unit(static_cast<std::size_t>(columns), 0.0);
      unit[static_cast<std::size_t>(j)] = 1.0;
      const std::vector<U64> shape{static_cast<U64>(columns)};
      Result<ArrayRef> stored =
          operators::write_f64(context, unit, shape, model::unit_dimensionless(), Id());
      if (!ok(stored)) {
        return model::fail<operators::CsrMatrix>(stored.code, "the vector cannot be stored",
                                                 "coadd.covariance", "the joint noise cannot be built");
      }
      Result<ArrayRef> mapped = action->apply(context, val(stored));
      if (!ok(mapped)) {
        return model::fail<operators::CsrMatrix>(mapped.code, "the action cannot be applied",
                                                 "coadd.covariance", "the joint noise cannot be built");
      }
      Result<std::vector<double>> column =
          operators::read_f64(context, val(mapped), 0, static_cast<U64>(rows));
      if (!ok(column)) {
        return model::fail<operators::CsrMatrix>(column.code, "the output cannot be read",
                                                 "coadd.covariance", "the joint noise cannot be built");
      }
      for (U64 i = 0; i < rows; ++i) {
        if (val(column)[static_cast<std::size_t>(i)] == 0.0) continue;
        matrix.indices.push_back(j);
        matrix.values.push_back(val(column)[static_cast<std::size_t>(i)]);
      }
      matrix.offsets[1 + j] = matrix.indices.size();
    }
    return model::ok_result(matrix);
  };

  for (const FrameView& frame : frames) {
    if (!frame.noise || !frame.noise->model) continue;
    const NoiseModel& noise = *frame.noise->model;
    std::vector<Ref<NoiseRoot>> roots;
    roots.insert(roots.end(), noise.innovations.begin(), noise.innovations.end());
    roots.insert(roots.end(), noise.shared_factors.begin(), noise.shared_factors.end());
    if (roots.size() != noise.influence.size()) {
      return model::fail<Ref<CovarianceAction>>(Code::invalid_model,
                                                "the influence count does not match the roots",
                                                "coadd.covariance", "the joint noise cannot be built");
    }
    for (std::size_t k = 0; k < roots.size(); ++k) {
      Result<operators::CsrMatrix> matrix = to_csr(noise.influence[k], frame.leaves.size());
      if (!ok(matrix)) {
        return model::fail<Ref<CovarianceAction>>(matrix.code, "an influence cannot be converted",
                                                  "coadd.covariance",
                                                  "the joint noise cannot be built");
      }
      if (k < noise.innovations.size()) {
        Result<Ref<LinearAction>> action = operators::make_csr_action(
            context, val(matrix).rows, val(matrix).cols, val(matrix).offsets, val(matrix).indices,
            val(matrix).values, Id(), Id(), noise.variance_unit);
        if (!ok(action)) {
          return model::fail<Ref<CovarianceAction>>(action.code, "an influence cannot be built",
                                                    "coadd.covariance",
                                                    "the joint noise cannot be built");
        }
        innovations.push_back(roots[k]);
        innovation_influences.push_back(val(action));
        continue;
      }
      const SharedId& identity = roots[k]->shared_id;
      const std::string key = identity.external_namespace + "\x1f" + identity.entity + "\x1f" +
                              identity.model_revision + "\x1f" + identity.component;
      auto found = shared_index.find(key);
      if (found == shared_index.end()) {
        shared_index[key] = shared_roots.size();
        shared_roots.push_back(roots[k]);
        shared_blocks.emplace_back();
      }
      shared_blocks[shared_index[key]].push_back(val(matrix));
    }
  }
  joint->variance_unit = frames.empty() || !frames.front().noise || !frames.front().noise->model
                             ? model::unit_variance("unknown")
                             : frames.front().noise->model->variance_unit;
  joint->innovations = innovations;
  joint->shared_factors = shared_roots;
  joint->influence = innovation_influences;
  for (std::vector<operators::CsrMatrix>& blocks_of_root : shared_blocks) {
    operators::CsrMatrix merged;
    if (blocks_of_root.empty()) continue;
    // The column count is the largest one of the blocks: a block may leave
    // trailing latent columns without a non zero.
    merged.cols = 0;
    for (const operators::CsrMatrix& block : blocks_of_root) {
      merged.cols = std::max(merged.cols, block.cols);
      for (const U64 index : block.indices) merged.cols = std::max(merged.cols, index + 1);
    }
    merged.rows = 0;
    for (const operators::CsrMatrix& block : blocks_of_root) merged.rows += block.rows;
    merged.offsets.assign(static_cast<std::size_t>(merged.rows) + 1, 0);
    U64 row = 0;
    for (const operators::CsrMatrix& block : blocks_of_root) {
      for (std::size_t i = 0; i < block.rows; ++i) {
        for (U64 k = block.offsets[i]; k < block.offsets[i + 1]; ++k) {
          merged.indices.push_back(block.indices[k]);
          merged.values.push_back(block.values[k]);
        }
        merged.offsets[row + i + 1] = merged.indices.size();
      }
      row += block.rows;
    }
    Result<Ref<LinearAction>> action = operators::make_csr_action(
        context, merged.rows, merged.cols, merged.offsets, merged.indices, merged.values, Id(), Id(),
        joint->variance_unit);
    if (!ok(action)) {
      return model::fail<Ref<CovarianceAction>>(action.code, "a shared influence cannot be built",
                                                "coadd.covariance", "the joint noise cannot be built");
    }
    joint->influence.push_back(val(action));
  }
  joint->state = NoiseState::known;
  joint->identity = model::make_identity("acsd.NoiseModel", 2, 0,
                                          model::hash_index(Index::Kind::latent,
                                                            joint->influence.size(), 0, {}),
                                          "acsd");
  joint->assessment = model::make_assessment(Code::ok);
  joint->assessment.complete = true;
  return make_covariance(context, Ref<NoiseModel>(std::shared_ptr<const NoiseModel>(joint)));
}

// Extracts a block of a covariance action.
Result<Matrix> covariance_block(Context& context, Ref<CovarianceAction> covariance,
                                const std::vector<std::size_t>& rows,
                                const std::vector<std::size_t>& columns) {
  Matrix out(rows.size(), columns.size(), 0.0);
  const U64 size = covariance->input_output.domain ? covariance->input_output.domain->size : 0;
  for (std::size_t j = 0; j < columns.size(); ++j) {
    if (columns[j] >= size) {
      return model::fail<Matrix>(Code::invalid_model, "the covariance block is out of range",
                                 "coadd.covariance", "the block cannot be extracted");
    }
    std::vector<double> unit(static_cast<std::size_t>(size), 0.0);
    unit[columns[j]] = 1.0;
    const std::vector<U64> shape{static_cast<U64>(size)};
    Result<ArrayRef> stored =
        operators::write_f64(context, unit, shape, model::unit_dimensionless(), Id());
    if (!ok(stored)) {
      return model::fail<Matrix>(stored.code, "the vector cannot be stored", "coadd.covariance",
                                 "the block cannot be extracted");
    }
    Result<ArrayRef> mapped = covariance_apply(context, covariance, val(stored));
    if (!ok(mapped)) {
      return model::fail<Matrix>(mapped.code, "the covariance cannot be applied", "coadd.covariance",
                                 "the block cannot be extracted");
    }
    Result<std::vector<double>> column =
        operators::read_f64(context, val(mapped), 0, static_cast<U64>(size));
    if (!ok(column)) {
      return model::fail<Matrix>(column.code, "the covariance output cannot be read",
                                 "coadd.covariance", "the block cannot be extracted");
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
      out(i, j) = val(column)[rows[i]];
    }
  }
  return model::ok_result(std::move(out));
}

}  // namespace

// ---------------------------------------------------------------------------
// select_samples
// ---------------------------------------------------------------------------

Result<Ref<Selection>> select_samples(Context& context, const FramesProvider& frames,
                                       const CoaddRequest& request) {
  Result<std::vector<Id>> frame_ids = frames.list_frames(context);
  if (!ok(frame_ids)) {
    return model::fail<Ref<Selection>>(Code::unavailable, "the frames cannot be listed",
                                       "select_samples", "the selection is not available");
  }
  if (val(frame_ids).empty()) {
    return model::fail<Ref<Selection>>(Code::invalid_request, "no frame is available",
                                       "select_samples", "the selection is not available");
  }
  std::vector<FrameView> views;
  U32 order = 0;
  for (const Id& frame : val(frame_ids)) {
    Result<FrameView> read = read_frame(context, frames, frame);
    if (!ok(read)) {
      return model::fail<Ref<Selection>>(read.code, "a frame cannot be read", "select_samples",
                                         "the selection is not available");
    }
    if (views.empty()) {
      order = val(read).order;
    } else if (val(read).order != order) {
      return model::fail<Ref<Selection>>(Code::invalid_request, "the frames do not share a leaf order",
                                         "select_samples", "the selection is not available");
    }
    views.push_back(val(read));
  }
  const std::size_t frame_count = views.size();

  // The leaf index is the union of the leaves of the frames.
  std::vector<HealpixLeaf> leaves;
  {
    std::set<std::pair<U64, U64>> unique;
    for (const FrameView& view : views) {
      for (const HealpixLeaf& leaf : view.leaves) unique.insert({leaf.order, leaf.ipix});
    }
    for (const auto& entry : unique) {
      leaves.push_back(HealpixLeaf{static_cast<U32>(entry.first), entry.second});
    }
  }
  const std::size_t leaf_count = leaves.size();

  std::vector<std::uint8_t> eligibility(leaf_count * frame_count, 0);
  std::vector<std::uint8_t> accepted(leaf_count * frame_count, 0);
  std::vector<std::uint16_t> reasons(leaf_count * frame_count, kAccepted);
  for (std::size_t leaf = 0; leaf < leaf_count; ++leaf) {
    for (std::size_t f = 0; f < frame_count; ++f) {
      const std::size_t flat = leaf * frame_count + f;
      auto found = views[f].row_of_leaf.find({leaves[leaf].order, leaves[leaf].ipix});
      if (found == views[f].row_of_leaf.end()) {
        reasons[flat] = kNotObserved;
        continue;
      }
      const std::size_t row = found->second;
      if (views[f].state[row] != 1) {
        reasons[flat] = kNoSupport;
        continue;
      }
      if (!views[f].dq.empty() && (views[f].dq[row] & 0x0001) != 0) {
        reasons[flat] = kDataQuality;
        continue;
      }
      eligibility[flat] = 1;
      accepted[flat] = 1;
    }
  }

  // ---- the conservative leave one out rejection ----------------------------
  bool unresolved = false;
  std::size_t rejected_count = 0;
  std::string rejection_note;
  if (request.rejection.kind == RejectionPolicy::Kind::single_max_loo) {
    const bool common_mean = !request.mixed_observation;
    const bool external = request.rejection.external_fixed_covariance != nullptr;
    if (!common_mean) {
      rejection_note = "the observation is mixed, the common mean test does not apply";
    } else if (!external) {
      rejection_note = "no externally fixed covariance is available";
    }
    if (common_mean && external) {
      // The externally fixed Gamma covers the frames of one leaf.
      const U64 domain = request.rejection.external_fixed_covariance->input_output.domain
                             ? request.rejection.external_fixed_covariance->input_output.domain->size
                             : 0;
      const bool per_leaf = (domain == leaf_count * frame_count);
      const bool shared = (domain == frame_count);
      if (!per_leaf && !shared) {
        rejection_note = "the externally fixed covariance does not match the sample space";
      } else {
        const double alpha = request.rejection.alpha > 0.0 ? request.rejection.alpha : 0.001;
        for (std::size_t leaf = 0; leaf < leaf_count; ++leaf) {
          std::vector<std::size_t> eligible;
          for (std::size_t f = 0; f < frame_count; ++f) {
            if (eligibility[leaf * frame_count + f] == 1) eligible.push_back(f);
          }
          if (eligible.size() < 4) {
            for (std::size_t f : eligible) {
              reasons[leaf * frame_count + f] = kRejectionSkipped;
            }
            continue;
          }
          // The Gamma block of the leaf.
          std::vector<std::size_t> rows;
          std::vector<std::size_t> columns;
          for (std::size_t f : eligible) {
            const std::size_t index = per_leaf ? (leaf * frame_count + f) : f;
            rows.push_back(index);
            columns.push_back(index);
          }
          Result<Matrix> block = covariance_block(context, request.rejection.external_fixed_covariance,
                                                  rows, columns);
          if (!ok(block)) {
            rejection_note = "the externally fixed covariance cannot be read";
            break;
          }
          // The values of the eligible samples.
          std::vector<double> values(eligible.size(), 0.0);
          for (std::size_t k = 0; k < eligible.size(); ++k) {
            const std::size_t f = eligible[k];
            const std::size_t row = views[f].row_of_leaf[{leaves[leaf].order, leaves[leaf].ipix}];
            values[k] = views[f].signal[row];
          }
          // The leave one out residuals use the GLS weights of the sub block.
          const std::size_t count = eligible.size();
          std::vector<double> q(count, 0.0);
          bool usable = true;
          for (std::size_t k = 0; k < count && usable; ++k) {
            std::vector<std::size_t> rest;
            for (std::size_t j = 0; j < count; ++j) {
              if (j != k) rest.push_back(j);
            }
            Matrix block_rest = coadd_internal::sub_matrix(val(block), rest);
            coadd_internal::BlueResult blue = coadd_internal::solve_blue(block_rest, 1e-12);
            if (blue.outcome != coadd_internal::BlueOutcome::kSolved &&
                blue.outcome != coadd_internal::BlueOutcome::kZeroVariance) {
              usable = false;
              break;
            }
            double mean = 0.0;
            for (std::size_t j = 0; j < rest.size(); ++j) {
              mean += blue.weights[j] * values[rest[j]];
            }
            // v_res = Gamma_kk + t^T Gamma_rest t - 2 Gamma_{k,rest} t
            double quadratic = 0.0;
            double cross = 0.0;
            for (std::size_t a = 0; a < rest.size(); ++a) {
              for (std::size_t b = 0; b < rest.size(); ++b) {
                quadratic += blue.weights[a] * val(block)(rest[a], rest[b]) * blue.weights[b];
              }
              cross += val(block)(k, rest[a]) * blue.weights[a];
            }
            const double variance = val(block)(k, k) + quadratic - 2.0 * cross;
            if (!(variance > 0.0)) {
              usable = false;
              break;
            }
            q[k] = (values[k] - mean) / std::sqrt(variance);
          }
          if (!usable) {
            for (std::size_t f : eligible) {
              reasons[leaf * frame_count + f] = kRejectionSkipped;
            }
            continue;
          }
          const double threshold =
              coadd_internal::normal_quantile(1.0 - alpha / (2.0 * static_cast<double>(count)));
          std::size_t worst = 0;
          for (std::size_t k = 1; k < count; ++k) {
            if (std::fabs(q[k]) > std::fabs(q[worst])) {
              worst = k;
            } else if (std::fabs(q[k]) == std::fabs(q[worst]) &&
                       views[eligible[k]].frame < views[eligible[worst]].frame) {
              worst = k;
            }
          }
          const double magnitude = std::fabs(q[worst]);
          const double band = 1e-9 * std::max(1.0, std::fabs(threshold));
          if (std::fabs(magnitude - threshold) <= band) {
            // The numerical interval of the statistic crosses the threshold:
            // the branch is not decided.
            unresolved = true;
            for (std::size_t f : eligible) {
              reasons[leaf * frame_count + f] = kSelectionUnresolved;
            }
            continue;
          }
          if (magnitude > threshold) {
            const std::size_t f = eligible[worst];
            accepted[leaf * frame_count + f] = 0;
            reasons[leaf * frame_count + f] = kRejectedByLeaveOneOut;
            ++rejected_count;
          }
        }
      }
    } else if (!rejection_note.empty()) {
      for (std::size_t leaf = 0; leaf < leaf_count; ++leaf) {
        for (std::size_t f = 0; f < frame_count; ++f) {
          if (eligibility[leaf * frame_count + f] == 1) {
            reasons[leaf * frame_count + f] = kRejectionSkipped;
          }
        }
      }
    }
  }

  // ---- publish -------------------------------------------------------------
  const Id selection_index = "leaf_selection";
  Result<ArrayRef> eligibility_array = context.storage.copy(
      context, DType::u8, model::encode_u8(eligibility),
      std::vector<U64>{static_cast<U64>(leaf_count), static_cast<U64>(frame_count)},
      model::unit_dimensionless(), selection_index);
  if (!ok(eligibility_array)) {
    return model::fail<Ref<Selection>>(eligibility_array.code, "the eligibility cannot be stored",
                                        "select_samples", "the selection is not available");
  }
  Result<ArrayRef> accepted_array = context.storage.copy(
      context, DType::u8, model::encode_u8(accepted),
      std::vector<U64>{static_cast<U64>(leaf_count), static_cast<U64>(frame_count)},
      model::unit_dimensionless(), selection_index);
  if (!ok(accepted_array)) {
    return model::fail<Ref<Selection>>(accepted_array.code, "the accepted samples cannot be stored",
                                        "select_samples", "the selection is not available");
  }
  Result<ArrayRef> reasons_array = context.storage.copy(
      context, DType::u16, model::encode_u16(reasons),
      std::vector<U64>{static_cast<U64>(leaf_count), static_cast<U64>(frame_count)},
      model::unit_dimensionless(), selection_index);
  if (!ok(reasons_array)) {
    return model::fail<Ref<Selection>>(reasons_array.code, "the reasons cannot be stored",
                                        "select_samples", "the selection is not available");
  }
  std::vector<U64> leaf_ids;
  leaf_ids.reserve(leaves.size() * 2);
  for (const HealpixLeaf& leaf : leaves) {
    leaf_ids.push_back(leaf.order);
    leaf_ids.push_back(leaf.ipix);
  }
  Result<ArrayRef> leaf_id_array = context.storage.copy(
      context, DType::u64, model::encode_u64(leaf_ids),
      std::vector<U64>{static_cast<U64>(leaves.size()), 2}, model::unit_dimensionless(),
      selection_index);
  if (!ok(leaf_id_array)) {
    return model::fail<Ref<Selection>>(leaf_id_array.code, "the leaf identities cannot be stored",
                                        "select_samples", "the selection is not available");
  }
  auto index = std::make_shared<Index>();
  index->size = leaf_count;
  index->kind = Index::Kind::leaf;
  index->order = order;
  index->namespace_id = selection_index;
  index->ids = val(leaf_id_array);

  auto selection = std::make_shared<Selection>();
  selection->index = std::shared_ptr<const Index>(index);
  selection->eligibility = val(eligibility_array);
  selection->accepted = val(accepted_array);
  selection->reasons = val(reasons_array);
  selection->policy = request.rejection;
  selection->condition = request.target.condition;
  selection->identity = model::make_identity("acsd.Selection", 2, 0,
                                             model::hash_index(Index::Kind::leaf, leaf_count, order, {}),
                                             "acsd");
  selection->assessment = model::make_assessment(Code::ok);
  selection->assessment.complete = true;

  Result<Ref<Selection>> out = model::ok_result(std::shared_ptr<const Selection>(selection));
  out.assessment.condition = selection->condition;
  model::set_included(out.assessment, "data_quality_eligibility");
  if (request.rejection.kind == RejectionPolicy::Kind::single_max_loo) {
    model::set_included(out.assessment, "single_max_leave_one_out_rejection");
    if (!rejection_note.empty()) {
      model::add_issue(out.assessment, Code::unavailable, rejection_note, "select_samples",
                       "the statistical rejection is skipped for every position");
      out.assessment.science = ScienceState::conditional;
    }
  }
  if (unresolved) {
    model::add_issue(out.assessment, Code::selection_unresolved,
                     "a standardised leave one out residual lies on the threshold",
                     "select_samples", "the rejection branch is not decided");
    out.assessment.science = ScienceState::selection_unresolved;
  }
  if (rejected_count > 0) {
    model::set_included(out.assessment, "rejected_samples=" + std::to_string(rejected_count));
  }
  return out;
}

// ---------------------------------------------------------------------------
// solve_weights
// ---------------------------------------------------------------------------

Result<Ref<WeightSolution>> solve_weights(Context& context, const FramesProvider& frames,
                                           Ref<Selection> selection,
                                           const CoaddRequest& request) {
  if (!selection) {
    return model::fail<Ref<WeightSolution>>(Code::invalid_request, "no selection supplied",
                                            "solve_weights", "the weights cannot be solved");
  }
  Result<std::vector<Id>> frame_ids = frames.list_frames(context);
  if (!ok(frame_ids)) {
    return model::fail<Ref<WeightSolution>>(Code::unavailable, "the frames cannot be listed",
                                            "solve_weights", "the weights cannot be solved");
  }
  std::vector<FrameView> views;
  for (const Id& frame : val(frame_ids)) {
    Result<FrameView> read = read_frame(context, frames, frame);
    if (!ok(read)) {
      return model::fail<Ref<WeightSolution>>(read.code, "a frame cannot be read", "solve_weights",
                                              "the weights cannot be solved");
    }
    views.push_back(val(read));
  }
  const std::size_t frame_count = views.size();
  const U64 accepted_count = selection->accepted->shape.empty()
                                 ? 0
                                 : selection->accepted->shape[0] * selection->accepted->shape[1];
  Result<ArrayView> accepted_array =
      context.storage.read(context, selection->accepted, BlockRequest{0, accepted_count});
  if (!ok(accepted_array)) {
    return model::fail<Ref<WeightSolution>>(accepted_array.code, "the accepted samples cannot be read",
                                            "solve_weights", "the weights cannot be solved");
  }
  const std::vector<std::uint8_t> accepted = model::decode_u8(val(accepted_array).bytes);
  const std::size_t leaf_count =
      selection->index ? static_cast<std::size_t>(selection->index->size) : 0;

  // The policy.
  WeightPolicy policy = request.weights;
  std::string policy_reason;
  if (request.mixed_observation && (policy == WeightPolicy::automatic || policy == WeightPolicy::blue)) {
    if (policy == WeightPolicy::blue) {
      return model::fail<Ref<WeightSolution>>(
          Code::unavailable, "a mixed observation has no common mean, BLUE is not defined",
          "solve_weights", "the weights cannot be solved");
    }
    policy = WeightPolicy::equal;
    policy_reason = "the observation is mixed, the equal weights are used";
  }
  if (policy == WeightPolicy::automatic) {
    bool comparable = !request.mixed_observation;
    bool covariance_complete = true;
    for (const FrameView& view : views) {
      if (!view.noise || !view.noise->model) covariance_complete = false;
      if (!view.noise || !view.noise->model || !view.noise->model->unknown_terms.empty()) {
        covariance_complete = false;
      }
    }
    if (comparable && covariance_complete) {
      policy = WeightPolicy::blue;
      policy_reason = "the common quantity is comparable and the covariance is complete";
    } else {
      policy = WeightPolicy::equal;
      policy_reason = comparable ? "the covariance of the frames is not complete, the equal weights are used"
                                 : "the quantity is not comparable, the equal weights are used";
    }
  }

  Ref<CovarianceAction> joint;
  if (policy == WeightPolicy::blue || policy == WeightPolicy::nonnegative_qp) {
    Result<Ref<CovarianceAction>> built = build_joint_covariance(context, views);
    if (!ok(built)) {
      return model::fail<Ref<WeightSolution>>(built.code, "the joint covariance cannot be built",
                                              "solve_weights", "the weights cannot be solved");
    }
    joint = val(built);
  }

  // The leaf identities of the selection and the offsets of the frames in the
  // joint leaf space.
  std::vector<U64> leaf_ids;
  std::vector<std::size_t> frame_offsets(frame_count, 0);
  if (selection->index && selection->index->ids) {
    Result<std::vector<U64>> ids = read_u64_array(context, selection->index->ids);
    if (!ok(ids)) {
      return model::fail<Ref<WeightSolution>>(ids.code, "the leaf identities cannot be read",
                                              "solve_weights", "the weights cannot be solved");
    }
    leaf_ids = val(ids);
  }
  if (leaf_ids.size() != 2 * leaf_count) {
    return model::fail<Ref<WeightSolution>>(Code::invalid_model, "the leaf identities disagree",
                                            "solve_weights", "the weights cannot be solved");
  }
  {
    U64 running = 0;
    for (std::size_t f = 0; f < frame_count; ++f) {
      frame_offsets[f] = static_cast<std::size_t>(running);
      running += views[f].leaves.size();
    }
  }

  std::vector<double> coefficients(leaf_count * frame_count, 0.0);
  std::vector<double> variances(leaf_count, 0.0);
  std::size_t empty_leaves = 0;
  for (std::size_t leaf = 0; leaf < leaf_count; ++leaf) {
    std::vector<std::size_t> eligible;
    for (std::size_t f = 0; f < frame_count; ++f) {
      if (accepted[leaf * frame_count + f] == 1) eligible.push_back(f);
    }
    if (eligible.empty()) {
      ++empty_leaves;
      continue;
    }
    std::vector<double> weights(eligible.size(), 1.0 / static_cast<double>(eligible.size()));
    double variance = 0.0;
    if (policy == WeightPolicy::blue || policy == WeightPolicy::nonnegative_qp) {
      // The Gamma block of the leaf in the joint covariance.  The joint space
      // concatenates the leaves of the frames; the leaf identity comes from
      // the selection index.
      std::vector<std::size_t> rows;
      std::vector<std::size_t> columns;
      const U64 leaf_order = leaf_ids[2 * leaf];
      const U64 leaf_ipix = leaf_ids[2 * leaf + 1];
      for (std::size_t f : eligible) {
        auto found = views[f].row_of_leaf.find({leaf_order, leaf_ipix});
        if (found == views[f].row_of_leaf.end()) {
          return model::fail<Ref<WeightSolution>>(
              Code::invalid_model, "an accepted sample has no leaf in its frame",
              "solve_weights", "the weights cannot be solved");
        }
        rows.push_back(frame_offsets[f] + found->second);
        columns.push_back(frame_offsets[f] + found->second);
      }
      Result<Matrix> block = covariance_block(context, joint, rows, columns);
      if (!ok(block)) {
        return model::fail<Ref<WeightSolution>>(block.code, "the covariance block cannot be read",
                                                "solve_weights", "the weights cannot be solved");
      }
      if (policy == WeightPolicy::blue) {
        coadd_internal::BlueResult blue = coadd_internal::solve_blue(val(block), request.solve.rank_relative > 0.0
                                                                  ? request.solve.rank_relative
                                                                  : 1e-12);
        if (blue.outcome == coadd_internal::BlueOutcome::kUnresolved) {
          return model::fail<Ref<WeightSolution>>(Code::geometry_solve_unresolved, blue.reason,
                                                  "solve_weights", "the weights cannot be solved");
        }
        weights = blue.weights;
        variance = blue.variance;
      } else {
        std::vector<std::string> names;
        for (std::size_t f : eligible) names.push_back(views[f].frame);
        coadd_internal::QpResult qp = coadd_internal::solve_nonnegative_qp(val(block), names, 100);
        if (qp.outcome != coadd_internal::QpOutcome::kSolved) {
          return model::fail<Ref<WeightSolution>>(Code::geometry_solve_unresolved, qp.reason,
                                                  "solve_weights", "the weights cannot be solved");
        }
        weights = qp.weights;
        variance = qp.variance;
      }
    } else {
      variance = 1.0 / static_cast<double>(eligible.size());
    }
    for (std::size_t k = 0; k < eligible.size(); ++k) {
      coefficients[leaf * frame_count + eligible[k]] = weights[k];
    }
    variances[leaf] = variance;
  }

  // The covariance of the combined value: the diagonal of the per leaf
  // variance of the weighted sum.
  Ref<CovarianceAction> covariance;
  {
    std::vector<double> root_values(leaf_count, 0.0);
    for (std::size_t leaf = 0; leaf < leaf_count; ++leaf) {
      root_values[leaf] = variances[leaf] > 0.0 ? std::sqrt(variances[leaf]) : 0.0;
    }
    const U64 size = leaf_count;
    std::vector<U64> offsets{0};
    std::vector<U64> indices;
    std::vector<double> values;
    for (U64 i = 0; i < size; ++i) {
      indices.push_back(i);
      values.push_back(root_values[static_cast<std::size_t>(i)]);
      offsets.push_back(indices.size());
    }
    Result<Ref<LinearAction>> factor =
        operators::make_csr_action(context, size, size, offsets, indices, values, Id(), Id(),
                                    model::unit_variance("combined"));
    if (!ok(factor)) {
      return model::fail<Ref<WeightSolution>>(factor.code, "the covariance factor cannot be built",
                                              "solve_weights", "the weights cannot be solved");
    }
    auto noise = std::make_shared<NoiseModel>();
    auto root = std::make_shared<NoiseRoot>();
    root->shared_id = SharedId{"acsd", "coadd_combined_variance", "2", "leaf"};
    root->unit = model::unit_variance("combined");
    root->factor = val(factor);
    auto coordinates = std::make_shared<Index>();
    coordinates->size = leaf_count;
    coordinates->kind = Index::Kind::leaf;
    root->coordinates = std::shared_ptr<const Index>(coordinates);
    noise->innovations.push_back(std::shared_ptr<const NoiseRoot>(root));
    noise->influence.push_back(val(factor));
    noise->variance_unit = model::unit_variance("combined");
    noise->state = NoiseState::known;
    noise->identity = model::make_identity("acsd.NoiseModel", 2, 0,
                                            model::hash_index(Index::Kind::latent, 1, 0, {}), "acsd");
    noise->assessment = model::make_assessment(Code::ok);
    noise->assessment.complete = true;
    Result<Ref<CovarianceAction>> built =
        make_covariance(context, Ref<NoiseModel>(std::shared_ptr<const NoiseModel>(noise)));
    if (!ok(built)) {
      return model::fail<Ref<WeightSolution>>(built.code, "the covariance cannot be built",
                                              "solve_weights", "the weights cannot be solved");
    }
    covariance = val(built);
  }

  Result<ArrayRef> coefficients_array = operators::write_f64(
      context, coefficients,
      std::vector<U64>{static_cast<U64>(leaf_count), static_cast<U64>(frame_count)},
      model::unit_dimensionless(), "coadd_weights");
  if (!ok(coefficients_array)) {
    return model::fail<Ref<WeightSolution>>(coefficients_array.code,
                                            "the weights cannot be stored", "solve_weights",
                                            "the weights cannot be solved");
  }

  auto solution = std::make_shared<WeightSolution>();
  solution->coefficients = val(coefficients_array);
  solution->samples = selection->index;
  solution->policy = policy;
  solution->covariance = covariance;
  solution->selection = selection;
  solution->condition = request.target.condition;
  solution->identity = model::make_identity("acsd.WeightSolution", 2, 0,
                                             model::hash_index(Index::Kind::leaf, leaf_count, 0, {}),
                                             "acsd");
  solution->assessment = model::make_assessment(Code::ok);
  solution->assessment.complete = true;

  Result<Ref<WeightSolution>> out = model::ok_result(std::shared_ptr<const WeightSolution>(solution));
  out.assessment.condition = solution->condition;
  model::set_included(out.assessment, "weight_policy=" + std::to_string(static_cast<int>(policy)));
  if (!policy_reason.empty()) model::set_included(out.assessment, policy_reason);
  if (empty_leaves > 0) {
    model::add_issue(out.assessment, Code::no_support,
                     "some leaves have no accepted sample and carry no weight",
                     "solve_weights", "the combined value of those leaves is not defined");
    out.assessment.science = ScienceState::conditional;
  }
  return out;
}

// ---------------------------------------------------------------------------
// coadd
// ---------------------------------------------------------------------------

Result<CoaddResult> coadd(Context& context, const FramesProvider& frames, Ref<SkyFit> sky,
                           const CoaddRequest& request) {
  Result<Ref<Selection>> selection = select_samples(context, frames, request);
  if (!ok(selection)) {
    return model::fail<CoaddResult>(selection.code, "the samples cannot be selected", "coadd",
                                    "the combination is not available");
  }
  Result<Ref<WeightSolution>> weights =
      solve_weights(context, frames, val(selection), request);
  if (!ok(weights)) {
    return model::fail<CoaddResult>(weights.code, "the weights cannot be solved", "coadd",
                                    "the combination is not available");
  }
  Result<std::vector<Id>> frame_ids = frames.list_frames(context);
  if (!ok(frame_ids)) {
    return model::fail<CoaddResult>(Code::unavailable, "the frames cannot be listed", "coadd",
                                    "the combination is not available");
  }
  std::vector<FrameView> views;
  for (const Id& frame : val(frame_ids)) {
    Result<FrameView> read = read_frame(context, frames, frame);
    if (!ok(read)) {
      return model::fail<CoaddResult>(read.code, "a frame cannot be read", "coadd",
                                      "the combination is not available");
    }
    views.push_back(val(read));
  }
  const std::size_t frame_count = views.size();
  const std::size_t leaf_count = val(selection)->index ? static_cast<std::size_t>(val(selection)->index->size) : 0;

  // The leaf identity of the selection.
  std::vector<HealpixLeaf> leaves;
  if (leaf_count > 0) {
    std::set<std::pair<U64, U64>> unique;
    for (const FrameView& view : views) {
      for (const HealpixLeaf& leaf : view.leaves) unique.insert({leaf.order, leaf.ipix});
    }
    for (const auto& entry : unique) {
      leaves.push_back(HealpixLeaf{static_cast<U32>(entry.first), entry.second});
    }
  }
  Result<std::vector<double>> coefficients =
      operators::read_f64(context, val(weights)->coefficients, 0,
                          static_cast<U64>(leaf_count * frame_count));
  if (!ok(coefficients)) {
    return model::fail<CoaddResult>(coefficients.code, "the weights cannot be read", "coadd",
                                    "the combination is not available");
  }
  std::vector<double> combined(leaf_count, 0.0);
  std::vector<std::uint8_t> usable(leaf_count, 0);
  for (std::size_t leaf = 0; leaf < leaf_count && leaf < leaves.size(); ++leaf) {
    double sum = 0.0;
    double total = 0.0;
    for (std::size_t f = 0; f < frame_count; ++f) {
      const double weight = val(coefficients)[leaf * frame_count + f];
      if (weight == 0.0) continue;
      auto found = views[f].row_of_leaf.find({leaves[leaf].order, leaves[leaf].ipix});
      if (found == views[f].row_of_leaf.end()) {
        continue;
      }
      sum += weight * views[f].signal[found->second];
      total += weight;
    }
    if (total == 0.0) continue;
    combined[leaf] = sum;
    usable[leaf] = 1;
  }
  Result<ArrayRef> combined_array = operators::write_f64(
      context, combined, std::vector<U64>{static_cast<U64>(leaf_count)}, request.target.unit,
      "coadd_signal");
  if (!ok(combined_array)) {
    return model::fail<CoaddResult>(combined_array.code, "the combined signal cannot be stored",
                                    "coadd", "the combination is not available");
  }
  auto estimate = std::make_shared<SignalEstimate>();
  estimate->signal = val(combined_array);
  estimate->unit = request.target.unit;
  estimate->estimator = request.target;
  estimate->numerical_lower = nullptr;
  estimate->numerical_upper = nullptr;
  estimate->support = nullptr;
  estimate->effective_response = nullptr;
  estimate->noise = val(weights)->covariance;
  estimate->error = nullptr;
  estimate->identity = model::make_identity("acsd.SignalEstimate", 2, 0,
                                             model::hash_index(Index::Kind::leaf, leaf_count, 0, {}),
                                             "acsd");
  estimate->assessment = model::make_assessment(Code::ok);
  estimate->assessment.complete = true;

  CoaddResult result;
  result.estimate = std::shared_ptr<const SignalEstimate>(estimate);
  result.selection = val(selection);
  result.weights = val(weights);
  result.sky = sky;
  return model::ok_result(std::move(result));
}

}  // namespace acsd
