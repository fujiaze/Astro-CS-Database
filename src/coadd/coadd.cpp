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
#include <cstdio>
#include <cstdlib>
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

bool same_unit(const Unit& a, const Unit& b) {
  return a.scale == b.scale && a.sr_power == b.sr_power && a.pixel_power == b.pixel_power &&
         a.nm_power == b.nm_power && a.linear_power == b.linear_power &&
         std::fabs(a.multiplier - b.multiplier) <= 1e-15 * std::max(1.0, std::fabs(a.multiplier));
}

bool unit_is_unspecified(const Unit& u) {
  return u.scale == "1" && u.sr_power == 0 && u.pixel_power == 0 && u.nm_power == 0 &&
         u.linear_power == 0 && std::fabs(u.multiplier - 1.0) <= 1e-15;
}


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
    if (val(dq).size() != count) {
      return model::fail<FrameView>(Code::invalid_model,
                                    "the frame quality array disagrees with the index", "coadd.read",
                                    "the frame cannot be used");
    }
    view.dq = val(dq);
  }
  Result<Ref<SupportMap>> support = frames.query_support(context, frame, description.index);
  if (!ok(support) || !val(support) || !val(support)->states) {
    // A missing support map is not "every leaf supported": the weights and the
    // control statistics would silently use rows the producer excluded.
    return model::fail<FrameView>(ok(support) ? Code::invalid_model : support.code,
                                  "the frame publishes no support states", "coadd.read",
                                  "the frame cannot be used");
  }
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
  }
  return model::ok_result(std::move(view));
}

// The joint covariance of the frames on the concatenated leaf space.
//
// Every root influence is placed at the row offset of its own frame, so an
// innovation of frame 2 never lands on the rows of frame 1.  The shared roots
// are merged by identity, so that the cross frame correlation of the combined
// value survives.  The unknown terms of the frames are carried into the joint
// model with the same offset, and a frame without a published noise model
// becomes an explicit unknown term: a joint model with an unknown contribution
// is published as unknown, never as known.
Result<Ref<CovarianceAction>> build_joint_covariance(Context& context,
                                                      const std::vector<FrameView>& frames) {
  auto joint = std::make_shared<NoiseModel>();
  std::vector<Ref<NoiseRoot>> innovations;
  std::vector<Ref<LinearAction>> innovation_influences;
  std::map<std::string, std::size_t> shared_index;
  std::vector<Ref<NoiseRoot>> shared_roots;
  std::vector<std::vector<operators::CsrMatrix>> shared_blocks;
  std::vector<std::vector<U64>> shared_offsets;
  std::vector<UnknownTerm> unknown_terms;
  std::vector<std::size_t> frame_offsets(frames.size(), 0);
  bool any_unknown = false;
  U64 total_rows = 0;
  for (const FrameView& frame : frames) total_rows += frame.leaves.size();
  {
    U64 running = 0;
    for (std::size_t f = 0; f < frames.size(); ++f) {
      frame_offsets[f] = static_cast<std::size_t>(running);
      running += frames[f].leaves.size();
    }
    joint->variance_unit =
        frames.empty() || !frames.front().noise || !frames.front().noise->model
            ? model::unit_variance("unknown")
            : frames.front().noise->model->variance_unit;
  }

  auto to_csr = [&](Ref<LinearAction> action, U64 rows,
                    U64 expected_columns) -> Result<operators::CsrMatrix> {
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
    // The action is materialised column by column with the public interface
    // of the contract, so that an implementation outside the operators module
    // is usable as well.  The number of columns is the size of the latent
    // space of the root, which the contract carries.
    U64 columns = impl ? impl->domain_size() : 0;
    if (columns == 0) columns = expected_columns;
    if (columns == 0) {
      return model::fail<operators::CsrMatrix>(Code::invalid_model,
                                               "the action has no columns in the latent space",
                                               "coadd.covariance",
                                               "the joint noise cannot be built");
    }
    // A latent space larger than this cannot be materialised column by
    // column within a bounded time; keep the joint covariance unknown rather
    // than hanging the workflow.
    if (columns > (1ull << 12) || rows > (1ull << 12)) {
      return model::fail<operators::CsrMatrix>(
          Code::unavailable,
          "the frame influence is too large to materialise (" + std::to_string(rows) + "x" +
              std::to_string(columns) + "); the joint covariance stays unknown",
          "coadd.covariance", "the joint noise cannot be built");
    }
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

  // The exact product of two sparse matrices: the corrected covariance of a
  // frame is P C P^T, so the influence of every root is P L_k before it is
  // placed at the frame offset.
  auto multiply = [](const operators::CsrMatrix& left,
                     const operators::CsrMatrix& right) -> Result<operators::CsrMatrix> {
    if (left.cols != right.rows) {
      return model::fail<operators::CsrMatrix>(Code::invalid_model,
                                               "the shapes of the multiplied actions do not agree",
                                               "coadd.covariance",
                                               "the joint noise cannot be built");
    }
    operators::CsrMatrix out;
    out.rows = left.rows;
    out.cols = right.cols;
    out.offsets.assign(static_cast<std::size_t>(out.rows) + 1, 0);
    std::vector<double> row(static_cast<std::size_t>(right.cols), 0.0);
    std::vector<U64> touched;
    for (U64 i = 0; i < out.rows; ++i) {
      touched.clear();
      for (U64 k = left.offsets[i]; k < left.offsets[i + 1]; ++k) {
        const U64 middle = left.indices[k];
        const double scale = left.values[k];
        if (scale == 0.0 || middle >= right.rows) continue;
        for (U64 j = right.offsets[middle]; j < right.offsets[middle + 1]; ++j) {
          const U64 column = right.indices[j];
          if (row[static_cast<std::size_t>(column)] == 0.0) touched.push_back(column);
          row[static_cast<std::size_t>(column)] += scale * right.values[j];
        }
      }
      std::sort(touched.begin(), touched.end());
      for (U64 column : touched) {
        const double value = row[static_cast<std::size_t>(column)];
        if (value == 0.0) continue;
        out.indices.push_back(column);
        out.values.push_back(value);
      }
      for (U64 column : touched) row[static_cast<std::size_t>(column)] = 0.0;
      out.offsets[i + 1] = out.indices.size();
    }
    return model::ok_result(std::move(out));
  };

  auto make_rows = [&](const operators::CsrMatrix& matrix,
                       const Unit& unit) -> Result<Ref<LinearAction>> {
    return operators::make_csr_action(context, matrix.rows, matrix.cols, matrix.offsets,
                                      matrix.indices, matrix.values, Id(), Id(), unit);
  };

  // Places the rows of a frame local block at the frame offset of the
  // concatenated leaf space.  The rows before the offset are exact zero rows.
  auto place = [](const operators::CsrMatrix& matrix, U64 offset, U64 total_rows) {
    operators::CsrMatrix out;
    out.cols = matrix.cols;
    out.rows = std::max(offset + matrix.rows, total_rows);
    out.offsets.assign(static_cast<std::size_t>(out.rows) + 1, 0);
    for (U64 row = 0; row < out.rows; ++row) {
      if (row >= offset && row < offset + matrix.rows) {
        const U64 local = row - offset;
        for (U64 k = matrix.offsets[local]; k < matrix.offsets[local + 1]; ++k) {
          out.indices.push_back(matrix.indices[k]);
          out.values.push_back(matrix.values[k]);
        }
      }
      out.offsets[static_cast<std::size_t>(row) + 1] = out.indices.size();
    }
    return out;
  };

  for (std::size_t f = 0; f < frames.size(); ++f) {
    const FrameView& frame = frames[f];
    const U64 offset = frame_offsets[f];
    if (!frame.noise || !frame.noise->model) {
      // No published noise model is not a zero variance.
      any_unknown = true;
      operators::CsrMatrix identity;
      identity.rows = frame.leaves.size();
      identity.cols = frame.leaves.size();
      identity.offsets.assign(static_cast<std::size_t>(identity.rows) + 1, 0);
      for (U64 i = 0; i < identity.rows; ++i) {
        identity.indices.push_back(i);
        identity.values.push_back(1.0);
        identity.offsets[static_cast<std::size_t>(i) + 1] = identity.indices.size();
      }
      Result<Ref<LinearAction>> action =
          make_rows(place(identity, offset, total_rows), joint->variance_unit);
      if (!ok(action)) {
        return model::fail<Ref<CovarianceAction>>(action.code, "an unknown term cannot be built",
                                                  "coadd.covariance",
                                                  "the joint noise cannot be built");
      }
      UnknownTerm term;
      term.id = "frame_noise_missing_" + std::to_string(offset);
      term.reason = "the frame publishes no noise model; its variance is unknown, not zero";
      term.influence = val(action);
      unknown_terms.push_back(term);
      continue;
    }
    const NoiseModel& noise = *frame.noise->model;
    std::vector<Ref<NoiseRoot>> roots;
    roots.insert(roots.end(), noise.innovations.begin(), noise.innovations.end());
    roots.insert(roots.end(), noise.shared_factors.begin(), noise.shared_factors.end());
    if (roots.size() != noise.influence.size()) {
      return model::fail<Ref<CovarianceAction>>(Code::invalid_model,
                                                "the influence count does not match the roots",
                                                "coadd.covariance", "the joint noise cannot be built");
    }
    if (noise.state != NoiseState::known) any_unknown = true;
    // The frame covariance may carry an outer action: C_frame = I0 C_model I0^T
    // (this is how apply_sky publishes the corrected covariance when it does
    // not fold the action into the roots).  Dropping it would use the
    // uncorrected C_y for the weights and for the published covariance, so it
    // is composed with every root influence before the frame offset is applied.
    Ref<LinearAction> outer = frame.noise->influence;
    if (outer) {
      const U64 outer_rows = frame.leaves.size();
      const U64 outer_cols = frame.leaves.size();
      if (outer_rows != 0 && outer_rows * outer_cols > (1ull << 26)) {
        return model::fail<Ref<CovarianceAction>>(
            Code::unavailable,
            "the outer influence of a frame is too large to be applied inside the coaddition; the "
            "corrected covariance must be published with its influences already folded",
            "coadd.covariance", "the joint noise cannot be built");
      }
      Result<operators::CsrMatrix> outer_matrix = to_csr(outer, outer_rows, outer_cols);
      if (!ok(outer_matrix)) {
        return model::fail<Ref<CovarianceAction>>(outer_matrix.code,
                                                  "the outer influence cannot be converted",
                                                  "coadd.covariance",
                                                  "the joint noise cannot be built");
      }
      for (std::size_t k = 0; k < roots.size(); ++k) {
        const U64 latent = roots[k] && roots[k]->coordinates ? roots[k]->coordinates->size : 0;
        Result<operators::CsrMatrix> matrix =
            to_csr(noise.influence[k], frame.leaves.size(), latent);
        if (!ok(matrix)) {
          return model::fail<Ref<CovarianceAction>>(matrix.code, "an influence cannot be converted",
                                                    "coadd.covariance",
                                                    "the joint noise cannot be built");
        }
        Result<operators::CsrMatrix> corrected = multiply(val(outer_matrix), val(matrix));
        if (!ok(corrected)) {
          return model::fail<Ref<CovarianceAction>>(corrected.code,
                                                    "the corrected influence cannot be formed",
                                                    "coadd.covariance",
                                                    "the joint noise cannot be built");
        }
        if (k < noise.innovations.size()) {
          Result<Ref<LinearAction>> action =
              make_rows(place(val(corrected), offset, total_rows), noise.variance_unit);
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
          shared_offsets.emplace_back();
        }
        shared_blocks[shared_index[key]].push_back(val(corrected));
        shared_offsets[shared_index[key]].push_back(offset);
      }
      for (const UnknownTerm& term : noise.unknown_terms) {
        if (!term.influence) continue;
        const U64 columns = term.influence->spaces.domain ? term.influence->spaces.domain->size : 0;
        Result<operators::CsrMatrix> matrix =
            to_csr(term.influence, frame.leaves.size(), columns);
        if (!ok(matrix)) {
          return model::fail<Ref<CovarianceAction>>(matrix.code, "an unknown term cannot be converted",
                                                    "coadd.covariance",
                                                    "the joint noise cannot be built");
        }
        Result<operators::CsrMatrix> corrected = multiply(val(outer_matrix), val(matrix));
        if (!ok(corrected)) {
          return model::fail<Ref<CovarianceAction>>(corrected.code,
                                                    "the corrected unknown term cannot be formed",
                                                    "coadd.covariance",
                                                    "the joint noise cannot be built");
        }
        Result<Ref<LinearAction>> action =
            make_rows(place(val(corrected), offset, total_rows), joint->variance_unit);
        if (!ok(action)) {
          return model::fail<Ref<CovarianceAction>>(action.code, "an unknown term cannot be built",
                                                    "coadd.covariance",
                                                    "the joint noise cannot be built");
        }
        UnknownTerm copy = term;
        copy.influence = val(action);
        unknown_terms.push_back(copy);
        any_unknown = true;
      }
      continue;
    }
    for (std::size_t k = 0; k < roots.size(); ++k) {
      const U64 latent = roots[k] && roots[k]->coordinates
                             ? roots[k]->coordinates->size
                             : 0;
      Result<operators::CsrMatrix> matrix =
          to_csr(noise.influence[k], frame.leaves.size(), latent);
      if (!ok(matrix)) {
        return model::fail<Ref<CovarianceAction>>(matrix.code, "an influence cannot be converted",
                                                  "coadd.covariance",
                                                  "the joint noise cannot be built");
      }
      if (k < noise.innovations.size()) {
        Result<Ref<LinearAction>> action =
            make_rows(place(val(matrix), offset, total_rows), noise.variance_unit);
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
        shared_offsets.emplace_back();
      }
      shared_blocks[shared_index[key]].push_back(val(matrix));
      shared_offsets[shared_index[key]].push_back(offset);
    }
    for (const UnknownTerm& term : noise.unknown_terms) {
      if (!term.influence) continue;
      const U64 columns = term.influence->spaces.domain ? term.influence->spaces.domain->size : 0;
      Result<operators::CsrMatrix> matrix =
          to_csr(term.influence, frame.leaves.size(), columns);
      if (!ok(matrix)) {
        return model::fail<Ref<CovarianceAction>>(matrix.code, "an unknown term cannot be converted",
                                                  "coadd.covariance",
                                                  "the joint noise cannot be built");
      }
      Result<Ref<LinearAction>> action =
          make_rows(place(val(matrix), offset, total_rows), joint->variance_unit);
      if (!ok(action)) {
        return model::fail<Ref<CovarianceAction>>(action.code, "an unknown term cannot be built",
                                                  "coadd.covariance",
                                                  "the joint noise cannot be built");
      }
      UnknownTerm copy = term;
      copy.influence = val(action);
      unknown_terms.push_back(copy);
      any_unknown = true;
    }
  }

  joint->innovations = innovations;
  joint->shared_factors = shared_roots;
  joint->influence = innovation_influences;
  for (std::size_t k = 0; k < shared_blocks.size(); ++k) {
    // The blocks of one shared root keep their own columns and are placed at
    // the row offsets of their frames; a frame that does not carry the root
    // keeps exact zero rows for it.
    operators::CsrMatrix merged;
    merged.cols = 0;
    for (const operators::CsrMatrix& block : shared_blocks[k]) {
      merged.cols = std::max(merged.cols, block.cols);
      for (const U64 index : block.indices) merged.cols = std::max(merged.cols, index + 1);
    }
    U64 rows = total_rows;
    for (std::size_t b = 0; b < shared_blocks[k].size(); ++b) {
      rows = std::max(rows, shared_offsets[k][b] + shared_blocks[k][b].rows);
    }
    merged.rows = rows;
    merged.offsets.assign(static_cast<std::size_t>(rows) + 1, 0);
    std::vector<int> owner(static_cast<std::size_t>(rows), -1);
    std::vector<U64> local(static_cast<std::size_t>(rows), 0);
    for (std::size_t b = 0; b < shared_blocks[k].size(); ++b) {
      const U64 offset = shared_offsets[k][b];
      for (U64 i = 0; i < shared_blocks[k][b].rows; ++i) {
        if (offset + i < rows) {
          owner[static_cast<std::size_t>(offset + i)] = static_cast<int>(b);
          local[static_cast<std::size_t>(offset + i)] = i;
        }
      }
    }
    for (U64 row = 0; row < rows; ++row) {
      const int b = owner[static_cast<std::size_t>(row)];
      if (b >= 0) {
        const operators::CsrMatrix& block = shared_blocks[k][static_cast<std::size_t>(b)];
        const U64 i = local[static_cast<std::size_t>(row)];
        for (U64 j = block.offsets[i]; j < block.offsets[i + 1]; ++j) {
          merged.indices.push_back(block.indices[j]);
          merged.values.push_back(block.values[j]);
        }
      }
      merged.offsets[static_cast<std::size_t>(row) + 1] = merged.indices.size();
    }
    Result<Ref<LinearAction>> action = make_rows(merged, joint->variance_unit);
    if (!ok(action)) {
      return model::fail<Ref<CovarianceAction>>(action.code, "a shared influence cannot be built",
                                                "coadd.covariance", "the joint noise cannot be built");
    }
    joint->influence.push_back(val(action));
  }
  joint->unknown_terms = unknown_terms;
  joint->state = any_unknown ? NoiseState::unknown : NoiseState::known;
  joint->identity = model::make_identity("acsd.NoiseModel", 2, 0,
                                          model::hash_index(Index::Kind::latent,
                                                            joint->influence.size(), 0, {}),
                                          "acsd");
  joint->assessment = model::make_assessment(Code::ok);
  joint->assessment.complete = true;
  joint->assessment.noise = joint->state;
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
    // The cancellation checkpoint of the selection: the selection stops with a
    // named failure and without a partial selection.
    if (context.cancellation.requested()) {
      return model::fail<Ref<Selection>>(Code::cancelled, "cancelled", "select_samples",
                                         "the selection is cancelled");
    }
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
  index->identity = model::make_identity("acsd.Index", 2, 0,
                                         model::hash_index(Index::Kind::leaf, leaf_count, order, {}),
                                         "acsd");

  auto selection = std::make_shared<Selection>();
  selection->index = std::shared_ptr<const Index>(index);
  selection->eligibility = val(eligibility_array);
  selection->accepted = val(accepted_array);
  selection->reasons = val(reasons_array);
  selection->policy = request.rejection;
  selection->condition = request.target.condition;
  // The published variance is conditional on the frozen mask.  A statistical
  // rejection takes the mask from the same data, so the conditional variance is
  // not an unconditional 68/95 percent coverage statement.  The condition field
  // is the estimator identity that the later stages compare, so the marker is
  // recorded in the assessment, where it cannot be confused with the identity.
  const bool statistical_selection =
      request.rejection.kind == RejectionPolicy::Kind::single_max_loo && rejection_note.empty();
  selection->identity = model::make_identity("acsd.Selection", 2, 0,
                                             model::hash_index(Index::Kind::leaf, leaf_count, order, {}),
                                             "acsd");
  selection->assessment = model::make_assessment(Code::ok);
  selection->assessment.complete = true;
  // A frame without a quality array is not a frame whose pixels are known
  // good: it has no quality evidence.  The rows stay selectable (the DQ policy
  // has no bit to reject), but the selection is conditional and says so.
  bool missing_quality = false;
  for (const FrameView& view : views) {
    if (view.dq.empty()) missing_quality = true;
  }
  if (missing_quality) {
    model::set_omitted(selection->assessment,
                       "the_quality_mask_is_absent_for_at_least_one_frame");
    selection->assessment.science = ScienceState::conditional;
    selection->assessment.complete = false;
  }

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
  model::set_included(out.assessment, "conditional_on_frozen_selection_mask");
  if (statistical_selection) {
    model::set_omitted(out.assessment, "unconditional_coverage_after_selection");
    out.assessment.science = ScienceState::conditional;
    model::add_issue(out.assessment, Code::ok,
                     "the frozen mask conditions the variance; the conditional variance is not an "
                     "unconditional coverage statement, which needs a full forward Monte Carlo",
                     "select_samples", "the variance is conditional on the frozen mask");
  }
  return out;
}

// ---------------------------------------------------------------------------
// Sparse joint covariance: embed each frame influence with E_f (total leaves
// x frame leaves) and compose, instead of materialising every latent column.
Result<Ref<CovarianceAction>> build_joint_covariance_sparse(
    Context& context, const std::vector<FrameView>& frames) {
  auto joint = std::make_shared<NoiseModel>();
  joint->variance_unit = frames.empty() || !frames.front().noise || !frames.front().noise->model
                             ? model::unit_variance("unknown")
                             : frames.front().noise->model->variance_unit;
  U64 total_rows = 0;
  std::vector<U64> frame_offsets(frames.size(), 0);
  for (std::size_t f = 0; f < frames.size(); ++f) {
    frame_offsets[f] = total_rows;
    total_rows += frames[f].leaves.size();
  }
  bool any_unknown = false;
  struct SharedEntry {
    Ref<NoiseRoot> root;
    std::vector<Ref<LinearAction>> influences;
  };
  std::map<std::string, SharedEntry> shared;
  std::vector<std::string> shared_order;
  for (std::size_t f = 0; f < frames.size(); ++f) {
    const FrameView& frame = frames[f];
    const U64 offset = frame_offsets[f];
    operators::CsrMatrix embedding;
    embedding.rows = total_rows;
    embedding.cols = frame.leaves.size();
    embedding.offsets.assign(static_cast<std::size_t>(total_rows) + 1, 0);
    for (U64 row = 0; row < total_rows; ++row) {
      if (row >= offset && row < offset + frame.leaves.size()) {
        embedding.indices.push_back(row - offset);
        embedding.values.push_back(1.0);
      }
      embedding.offsets[static_cast<std::size_t>(row) + 1] = embedding.indices.size();
    }
    Result<Ref<LinearAction>> embed = operators::make_csr_action(
        context, embedding.rows, embedding.cols, embedding.offsets, embedding.indices,
        embedding.values, Id(), Id(), joint->variance_unit);
    if (!ok(embed)) {
      return model::fail<Ref<CovarianceAction>>(embed.code, "the frame embedding cannot be built",
                                                "coadd.covariance",
                                                "the joint noise cannot be built");
    }
    if (!frame.noise || !frame.noise->model) {
      any_unknown = true;
      UnknownTerm term;
      term.id = "frame_noise_missing_" + std::to_string(offset);
      term.reason = "the frame publishes no noise model; its variance is unknown, not zero";
      term.influence = val(embed);
      joint->unknown_terms.push_back(term);
      continue;
    }
    const NoiseModel& noise = *frame.noise->model;
    std::vector<Ref<NoiseRoot>> roots;
    roots.insert(roots.end(), noise.innovations.begin(), noise.innovations.end());
    roots.insert(roots.end(), noise.shared_factors.begin(), noise.shared_factors.end());
    if (roots.size() != noise.influence.size()) {
      return model::fail<Ref<CovarianceAction>>(Code::invalid_model,
                                                "the influence count does not match the roots",
                                                "coadd.covariance",
                                                "the joint noise cannot be built");
    }
    if (noise.state != NoiseState::known) any_unknown = true;
    for (std::size_t k = 0; k < roots.size(); ++k) {
      Ref<LinearAction> influence = noise.influence[k];
      if (frame.noise->influence) {
        Result<Ref<LinearAction>> folded =
            compose(context, frame.noise->influence, influence);
        if (!ok(folded)) {
          return model::fail<Ref<CovarianceAction>>(folded.code,
                                                    "the frame outer influence cannot be folded",
                                                    "coadd.covariance",
                                                    "the joint noise cannot be built");
        }
        influence = val(folded);
      }
      Result<Ref<LinearAction>> placed = compose(context, val(embed), influence);
      if (!ok(placed)) {
        return model::fail<Ref<CovarianceAction>>(placed.code,
                                                  "the frame influence cannot be embedded",
                                                  "coadd.covariance",
                                                  "the joint noise cannot be built");
      }
      if (k < noise.innovations.size()) {
        joint->innovations.push_back(roots[k]);
        joint->influence.push_back(val(placed));
      } else {
        const SharedId& id = roots[k]->shared_id;
        const std::string key = id.external_namespace + "\x1f" + id.entity + "\x1f" +
                                id.model_revision + "\x1f" + id.component;
        auto found = shared.find(key);
        if (found == shared.end()) {
          shared_order.push_back(key);
          SharedEntry entry;
          entry.root = roots[k];
          entry.influences.push_back(val(placed));
          shared.emplace(key, std::move(entry));
        } else {
          found->second.influences.push_back(val(placed));
        }
      }
    }
    for (const UnknownTerm& term : noise.unknown_terms) {
      UnknownTerm copy = term;
      if (term.influence) {
        Result<Ref<LinearAction>> placed = compose(context, val(embed), term.influence);
        if (!ok(placed)) {
          return model::fail<Ref<CovarianceAction>>(placed.code,
                                                    "the unknown term cannot be embedded",
                                                    "coadd.covariance",
                                                    "the joint noise cannot be built");
        }
        copy.influence = val(placed);
      }
      joint->unknown_terms.push_back(copy);
      any_unknown = true;
    }
  }
  for (const std::string& key : shared_order) {
    SharedEntry& entry = shared[key];
    Ref<LinearAction> total = entry.influences.front();
    if (entry.influences.size() > 1) {
      std::vector<double> coefficients(entry.influences.size(), 1.0);
      Result<ArrayRef> stored = operators::write_f64(
          context, coefficients,
          std::vector<U64>{static_cast<U64>(coefficients.size())},
          model::unit_dimensionless(), Id());
      if (!ok(stored)) {
        return model::fail<Ref<CovarianceAction>>(stored.code,
                                                  "the shared root coefficients cannot be stored",
                                                  "coadd.covariance",
                                                  "the joint noise cannot be built");
      }
      ActionRecipe recipe;
      recipe.kind = ActionRecipe::Kind::sum;
      recipe.algorithm = "shared_root_sum_v1";
      recipe.version = 1;
      std::vector<Id> children;
      for (const Ref<LinearAction>& part : entry.influences) {
        children.push_back(part->identity.object_id);
      }
      recipe.children = children;
      recipe.arrays = {val(stored)->identity.object_id};
      std::vector<ArrayRef> arrays{val(stored)};
      Result<Ref<LinearAction>> summed =
          make_action(context, recipe, entry.influences, arrays);
      if (!ok(summed)) {
        return model::fail<Ref<CovarianceAction>>(summed.code,
                                                  "the shared root sum cannot be built",
                                                  "coadd.covariance",
                                                  "the joint noise cannot be built");
      }
      total = val(summed);
    }
    joint->shared_factors.push_back(entry.root);
    joint->influence.push_back(total);
  }
  if (joint->innovations.empty() && joint->shared_factors.empty()) {
    return model::fail<Ref<CovarianceAction>>(
        Code::unavailable, "the joint model has no uncertainty root",
        "coadd.covariance", "the joint noise cannot be built");
  }
  joint->state = any_unknown ? NoiseState::unknown : NoiseState::known;
  joint->assessment = model::make_assessment(Code::ok);
  joint->assessment.complete = !any_unknown;
  joint->assessment.noise = joint->state;
  joint->identity = model::make_identity(
      "acsd.NoiseModel", 2, 0,
      model::sha256_hex("coadd:joint:" + std::to_string(total_rows)), "acsd");
  return make_covariance(context, Ref<NoiseModel>(joint));
}

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
  // The shape of the accepted samples is read defensively: a selection that
  // does not carry a two dimensional shape is refused by name instead of
  // reading outside the array.
  const std::vector<U64>& accepted_shape = selection->accepted->shape;
  const U64 accepted_count =
      accepted_shape.size() == 2 ? accepted_shape[0] * accepted_shape[1]
                                 : (accepted_shape.size() == 1 ? accepted_shape[0] : 0);
  Result<ArrayView> accepted_array =
      context.storage.read(context, selection->accepted, BlockRequest{0, accepted_count});
  if (!ok(accepted_array)) {
    return model::fail<Ref<WeightSolution>>(accepted_array.code, "the accepted samples cannot be read",
                                            "solve_weights", "the weights cannot be solved");
  }
  const std::vector<std::uint8_t> accepted = model::decode_u8(val(accepted_array).bytes);
  const std::size_t leaf_count =
      selection->index ? static_cast<std::size_t>(selection->index->size) : 0;
  if (accepted_count != leaf_count * frame_count) {
    return model::fail<Ref<WeightSolution>>(
        Code::invalid_model, "the accepted samples do not match the leaves and the frames",
        "solve_weights", "the weights cannot be solved");
  }

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
      if (!view.noise || !view.noise->model || !view.noise->model->unknown_terms.empty() ||
          view.noise->model->state != NoiseState::known) {
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

  // The joint covariance over the concatenated leaf space.  Every policy needs
  // it: the equal policy still has the variance t^T Gamma t (CE-G: the true
  // equal weight variance of Gamma = [[1,2],[2,5]] is 5/2, not 1/K = 1/2) and
  // the published covariance is W C W^T with the shared roots kept, so the
  // cross leaf terms of CE-H survive (Var(z1+z2) = 10, Var(z1-z2) = 0).
  Ref<CovarianceAction> joint;
  std::string covariance_failure_reason;
  bool joint_available = false;
  {
    Result<Ref<CovarianceAction>> built = build_joint_covariance_sparse(context, views);
    if (!ok(built)) {
      // A malformed model is an input error, not a science limitation.
      const bool malformed = built.code == Code::invalid_model ||
                             built.code == Code::identity_conflict ||
                             built.code == Code::invalid_request ||
                             built.code == Code::invalid_covariance;
      covariance_failure_reason = built.assessment.issues.empty()
                                      ? "the joint covariance cannot be built"
                                      : built.assessment.issues.front().reason;
      if (malformed) {
        return model::fail<Ref<WeightSolution>>(built.code, covariance_failure_reason,
                                                "solve_weights", "the weights cannot be solved");
      }
    } else {
      joint = val(built);
      joint_available = true;
    }
  }
  bool covariance_known = joint_available && joint && joint->model;
  if (covariance_known) {
    if (joint->model->state != NoiseState::known) covariance_known = false;
    for (const UnknownTerm& term : joint->model->unknown_terms) {
      if (term.influence) covariance_known = false;
    }
  }
  if (!covariance_known) {
    if (covariance_failure_reason.empty()) {
      covariance_failure_reason =
          "the joint covariance model carries an unknown contribution";
    }
    if (policy == WeightPolicy::blue || policy == WeightPolicy::nonnegative_qp) {
      // An explicit BLUE or non-negative program with an incomplete Gamma is
      // not a declared estimator: it is unavailable, never a silent equal
      // weight solution.
      return model::fail<Ref<WeightSolution>>(
          Code::unavailable,
          "the joint covariance is not complete and known, so the requested weight policy is not "
          "defined: " + covariance_failure_reason,
          "solve_weights", "the weights cannot be solved");
    }
    if (request.weights == WeightPolicy::automatic) {
      policy_reason = "the joint covariance is not complete, the equal weights are used";
    }
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
    // The Gamma block of the leaf in the joint covariance.  The joint space
    // concatenates the leaves of the frames; the leaf identity comes from the
    // selection index.
    std::vector<std::size_t> rows;
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
    }
    Matrix block;
    bool block_available = false;
    if (covariance_known) {
      Result<Matrix> read_block = covariance_block(context, joint, rows, rows);
      if (!ok(read_block)) {
        Result<Ref<WeightSolution>> out = model::fail<Ref<WeightSolution>>(
            read_block.code, "the covariance block cannot be read", "solve_weights",
            "the weights cannot be solved");
        for (const Issue& issue : read_block.assessment.issues) {
          model::add_issue(out.assessment, issue.code, issue.reason, issue.origin, issue.impact);
        }
        return out;
      }
      block = val(read_block);
      block_available = true;
    }
    if (policy == WeightPolicy::blue) {
      coadd_internal::BlueResult blue =
          coadd_internal::solve_blue(block, request.solve.rank_relative > 0.0
                                                       ? request.solve.rank_relative
                                                       : 1e-12);
      if (blue.outcome == coadd_internal::BlueOutcome::kUnresolved) {
        return model::fail<Ref<WeightSolution>>(Code::geometry_solve_unresolved, blue.reason,
                                                "solve_weights", "the weights cannot be solved");
      }
      weights = blue.weights;
    } else if (policy == WeightPolicy::nonnegative_qp) {
      std::vector<std::string> names;
      for (std::size_t f : eligible) names.push_back(views[f].frame);
      coadd_internal::QpResult qp =
          coadd_internal::solve_nonnegative_qp(context, block, names, 100);
      if (qp.outcome != coadd_internal::QpOutcome::kSolved) {
        return model::fail<Ref<WeightSolution>>(Code::geometry_solve_unresolved, qp.reason,
                                                "solve_weights", "the weights cannot be solved");
      }
      weights = qp.weights;
    }
    // The weights of one leaf must be a normalised set: the combined value is
    // B = sum_k t_k z_k with sum_k t_k = 1, and a solver output that violates
    // it is a failed self check, not a small numerical detail.
    double weight_sum = 0.0;
    for (double weight : weights) weight_sum += weight;
    if (!(std::fabs(weight_sum - 1.0) <= 1e-9)) {
      return model::fail<Ref<WeightSolution>>(
          Code::invalid_model,
          "the weights of a leaf do not sum to one; the combined value would not estimate the "
          "common quantity",
          "solve_weights", "the weights cannot be solved");
    }
    // The published variance of the leaf is the quadratic form t^T Gamma t of
    // the same block the weights were solved with; it is never 1/K.
    if (block_available) {
      double variance = 0.0;
      for (std::size_t a = 0; a < weights.size(); ++a) {
        for (std::size_t b = 0; b < weights.size(); ++b) {
          variance += weights[a] * block(a, b) * weights[b];
        }
      }
      if (!(variance >= 0.0)) {
        return model::fail<Ref<WeightSolution>>(
            Code::invalid_covariance, "the combined variance of a leaf is negative",
            "solve_weights", "the weights cannot be solved");
      }
    }
    for (std::size_t k = 0; k < eligible.size(); ++k) {
      coefficients[leaf * frame_count + eligible[k]] = weights[k];
    }
  }

  // The covariance of the combined value.  It is not a per leaf diagonal: the
  // combination is z = W y over the concatenated leaf space, so
  //   Cov(z) = W C W^T = sum_k (W L_k) F_k F_k^T (W L_k)^T,
  // which keeps the shared roots and therefore the cross leaf terms.  Replacing
  // it by the diagonal of the per leaf variances drops exactly those terms.
  std::size_t total_leaves = 0;
  for (const FrameView& view : views) total_leaves += view.leaves.size();
  std::vector<U64> w_offsets(static_cast<std::size_t>(leaf_count) + 1, 0);
  std::vector<U64> w_indices;
  std::vector<double> w_values;
  for (std::size_t leaf = 0; leaf < leaf_count; ++leaf) {
    const U64 leaf_order = leaf_ids[2 * leaf];
    const U64 leaf_ipix = leaf_ids[2 * leaf + 1];
    for (std::size_t f = 0; f < frame_count; ++f) {
      const double weight = coefficients[leaf * frame_count + f];
      if (weight == 0.0) continue;
      auto found = views[f].row_of_leaf.find({leaf_order, leaf_ipix});
      if (found == views[f].row_of_leaf.end()) continue;
      w_indices.push_back(static_cast<U64>(frame_offsets[f] + found->second));
      w_values.push_back(weight);
    }
    w_offsets[leaf + 1] = w_indices.size();
  }
  Result<Ref<LinearAction>> weight_action = operators::make_csr_action(
      context, static_cast<U64>(leaf_count), static_cast<U64>(total_leaves), w_offsets, w_indices,
      w_values, Id(), Id(), model::unit_dimensionless());
  if (!ok(weight_action)) {
    return model::fail<Ref<WeightSolution>>(weight_action.code, "the combination operator cannot be built",
                                            "solve_weights", "the weights cannot be solved");
  }
  Ref<CovarianceAction> covariance;
  // The combined covariance is C = W C_joint W^T.  The outer action of a
  // CovarianceAction is exactly a left/right factor: C(v) = W (C_joint (W^T v)),
  // so the joint model is kept unchanged and W is published as the outer
  // influence.  Every shared root, and therefore every cross leaf term, is
  // carried by the model instead of being collapsed to a per leaf diagonal.
  auto leaf_index = [&]() {
    auto index = std::make_shared<Index>();
    index->size = static_cast<U64>(leaf_count);
    index->kind = Index::Kind::leaf;
    index->namespace_id = selection->index ? selection->index->identity.object_id : Id();
    index->identity = model::make_identity("acsd.Index", 2, 0,
                                            model::hash_index(Index::Kind::leaf, leaf_count, 0, {}),
                                            "acsd");
    return std::shared_ptr<const Index>(index);
  };
  if (covariance_known) {
    auto built = std::make_shared<CovarianceAction>(*joint);
    built->influence = val(weight_action);
    built->input_output.domain = leaf_index();
    built->input_output.codomain = built->input_output.domain;
    built->input_output.input = joint->model->variance_unit;
    built->input_output.output = joint->model->variance_unit;
    built->identity = model::make_identity("acsd.CovarianceAction", 2, 0,
                                            model::hash_index(Index::Kind::leaf, leaf_count, 0, {}),
                                            "acsd");
    built->assessment = model::make_assessment(Code::ok);
    built->assessment.complete = true;
    built->assessment.noise = NoiseState::known;
    covariance = std::shared_ptr<const CovarianceAction>(built);
  } else {
    // The variance of the combined value is unknown: the model carries an
    // unknown term and the outer influence is the same combination operator,
    // so no invented diagonal is published.
    auto noise = std::make_shared<NoiseModel>();
    noise->variance_unit = joint_available && joint->model
                               ? joint->model->variance_unit
                               : model::unit_variance("unknown");
    noise->state = NoiseState::unknown;
    // Keep the actual root structure even when the covariance is unknown: an
    // empty model cannot be pushed through Q (C = Q C_model Q^T) and would
    // make the export covariance impossible to build.  The unknown term below
    // records why the total is not a known covariance.
    if (joint_available && joint->model) {
      noise->innovations = joint->model->innovations;
      noise->shared_factors = joint->model->shared_factors;
      noise->influence = joint->model->influence;
      noise->unknown_terms = joint->model->unknown_terms;
    }
    operators::CsrMatrix identity;
    identity.rows = static_cast<U64>(total_leaves);
    identity.cols = static_cast<U64>(total_leaves);
    identity.offsets.assign(static_cast<std::size_t>(identity.rows) + 1, 0);
    for (U64 i = 0; i < identity.rows; ++i) {
      identity.indices.push_back(i);
      identity.values.push_back(1.0);
      identity.offsets[static_cast<std::size_t>(i) + 1] = identity.indices.size();
    }
    Result<Ref<LinearAction>> unknown_influence = operators::make_csr_action(
        context, identity.rows, identity.cols, identity.offsets, identity.indices, identity.values,
        Id(), Id(), noise->variance_unit);
    if (!ok(unknown_influence)) {
      return model::fail<Ref<WeightSolution>>(unknown_influence.code,
                                              "the unknown covariance cannot be built",
                                              "solve_weights", "the weights cannot be solved");
    }
    UnknownTerm term;
    term.id = "coadd_combined_variance_unknown";
    term.reason = covariance_failure_reason;
    term.influence = val(unknown_influence);
    noise->unknown_terms.push_back(term);
    noise->identity = model::make_identity("acsd.NoiseModel", 2, 0,
                                            model::hash_index(Index::Kind::latent, 1, 0, {}),
                                            "acsd");
    noise->assessment = model::make_assessment(Code::ok);
    noise->assessment.complete = true;
    noise->assessment.noise = NoiseState::unknown;
    Result<Ref<CovarianceAction>> built =
        make_covariance(context, Ref<NoiseModel>(std::shared_ptr<const NoiseModel>(noise)));
    if (!ok(built)) {
      return model::fail<Ref<WeightSolution>>(built.code, "the unknown covariance cannot be built",
                                              "solve_weights", "the weights cannot be solved");
    }
    auto wrapped = std::make_shared<CovarianceAction>(*val(built));
    wrapped->influence = val(weight_action);
    wrapped->input_output.domain = leaf_index();
    wrapped->input_output.codomain = wrapped->input_output.domain;
    covariance = std::shared_ptr<const CovarianceAction>(wrapped);
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
  // The condition is the estimator identity that the later stages compare, so
  // the selection conditional marker lives in the assessment: the variance of
  // the combined value is conditional on a frozen mask that was chosen from the
  // same data and is not an unconditional coverage claim.
  solution->condition = request.target.condition;
  solution->identity = model::make_identity("acsd.WeightSolution", 2, 0,
                                             model::hash_index(Index::Kind::leaf, leaf_count, 0, {}),
                                             "acsd");
  solution->assessment = model::make_assessment(Code::ok);
  solution->assessment.complete = true;
  if (!covariance_known) {
    solution->assessment.noise = NoiseState::unknown;
  }

  Result<Ref<WeightSolution>> out = model::ok_result(std::shared_ptr<const WeightSolution>(solution));
  out.assessment.condition = solution->condition;
  model::set_included(out.assessment, "weight_policy=" + std::to_string(static_cast<int>(policy)));
  if (!policy_reason.empty()) model::set_included(out.assessment, policy_reason);
  model::set_included(out.assessment, "weights_sum_to_one_per_leaf");
  model::set_included(out.assessment, "combined_covariance_keeps_the_shared_roots");
  if (!covariance_known) {
    model::set_omitted(out.assessment, "known_combined_variance");
    out.assessment.noise = NoiseState::unknown;
    out.assessment.science = ScienceState::conditional;
    model::add_issue(out.assessment, Code::unavailable,
                     "the combined variance is unknown: " + covariance_failure_reason,
                     "solve_weights", "the combined variance is unknown");
  }
  if (empty_leaves > 0) {
    model::add_issue(out.assessment, Code::no_support,
                     "some leaves have no accepted sample and carry no weight",
                     "solve_weights", "the combined value of those leaves is not defined");
    out.assessment.science = ScienceState::conditional;
  }
  return out;
}

// ---------------------------------------------------------------------------
// The unique combination core and the frozen-input validation
// ---------------------------------------------------------------------------

namespace {

bool same_identity(const Identity& a, const Identity& b) {
  if (!a.content_hash.empty() || !b.content_hash.empty())
    return a.content_hash == b.content_hash;
  return a.object_id == b.object_id;
}

bool same_condition(const Condition& a, const Condition& b) {
  return a.estimator == b.estimator && a.kind == b.kind &&
         a.fixed_objects == b.fixed_objects && a.assumptions == b.assumptions;
}

bool same_rejection(const RejectionPolicy& a, const RejectionPolicy& b) {
  if (a.kind != b.kind || a.alpha != b.alpha ||
      a.common_mean_proof != b.common_mean_proof)
    return false;
  if (static_cast<bool>(a.external_fixed_covariance) !=
      static_cast<bool>(b.external_fixed_covariance))
    return false;
  if (a.external_fixed_covariance && b.external_fixed_covariance)
    return same_identity(a.external_fixed_covariance->identity,
                         b.external_fixed_covariance->identity);
  return true;
}

U64 array_elements(const ArrayRef& array) {
  if (!array) return 0;
  return static_cast<U64>(count_elements(array->shape));
}

Result<rt::Done> validate_inputs(const CoaddInputs& inputs) {
  if (!inputs.frames) {
    return model::fail<rt::Done>(Code::invalid_request, "the frames provider is missing", "coadd",
                                 "the combination is not available");
  }
  // A null SkyFit is the historical "no relative sky applied" case: the
  // combination never applies a sky correction, it only carries the frozen fit
  // to the result.  The compatibility entry point relies on this.
  if (!inputs.selection) {
    return model::fail<rt::Done>(Code::invalid_request, "the sealed selection is missing", "coadd",
                                 "the combination is not available");
  }
  if (!inputs.weights) {
    return model::fail<rt::Done>(Code::invalid_request,
                                 "the sealed weight solution is missing", "coadd",
                                 "the combination is not available");
  }
  const Selection& selection = *inputs.selection;
  const WeightSolution& weights = *inputs.weights;
  if (!selection.index || !selection.accepted || !selection.eligibility ||
      !selection.reasons || !selection.index->ids) {
    return model::fail<rt::Done>(Code::invalid_request,
                                 "the sealed selection is incomplete", "coadd",
                                 "the combination is not available");
  }
  if (!weights.coefficients || !weights.samples) {
    return model::fail<rt::Done>(Code::invalid_request,
                                 "the sealed weight solution is incomplete", "coadd",
                                 "the combination is not available");
  }
  if (selection.index->size == 0 || selection.index->kind != Index::Kind::leaf) {
    return model::fail<rt::Done>(Code::invalid_request,
                                 "the sealed selection has no leaf index", "coadd",
                                 "the combination is not available");
  }
  // The selection identity and the sample index of the weights must be the
  // same objects the weights were solved for.
  if (!same_identity(weights.samples->identity, selection.index->identity)) {
    return model::fail<rt::Done>(
        Code::identity_conflict,
        "the weight sample index differs from the sealed selection index", "coadd",
        "the sealed inputs cannot be combined");
  }
  if (!weights.selection) {
    return model::fail<rt::Done>(
        Code::identity_conflict,
        "the weight solution does not carry the sealed selection identity", "coadd",
        "the sealed inputs cannot be combined");
  }
  if (!same_identity(weights.selection->identity, selection.identity)) {
    return model::fail<rt::Done>(
        Code::identity_conflict,
        "the weights were solved for a different Selection object", "coadd",
        "the sealed inputs cannot be combined");
  }
  if (!same_rejection(selection.policy, inputs.request.rejection)) {
    return model::fail<rt::Done>(
        Code::identity_conflict,
        "the sealed selection rejection policy differs from the requested policy", "coadd",
        "the sealed inputs cannot be combined");
  }
  if (inputs.request.weights != WeightPolicy::automatic &&
      inputs.request.weights != weights.policy) {
    return model::fail<rt::Done>(
        Code::identity_conflict,
        "the sealed weight policy differs from the requested weight policy", "coadd",
        "the sealed inputs cannot be combined");
  }
  if (!same_condition(selection.condition, inputs.request.target.condition)) {
    return model::fail<rt::Done>(
        Code::identity_conflict,
        "the sealed selection belongs to a different estimator condition", "coadd",
        "the sealed inputs cannot be combined");
  }
  if (!same_condition(weights.condition, inputs.request.target.condition)) {
    return model::fail<rt::Done>(
        Code::identity_conflict,
        "the sealed weights belong to a different estimator condition", "coadd",
        "the sealed inputs cannot be combined");
  }
  const U64 leaves = selection.index->size;
  if (weights.samples->size != leaves) {
    return model::fail<rt::Done>(Code::invalid_request,
                                 "the weight sample index has the wrong dimension", "coadd",
                                 "the sealed inputs cannot be combined");
  }
  if (weights.coefficients->shape.size() != 2 ||
      weights.coefficients->shape[0] != leaves || weights.coefficients->shape[1] == 0) {
    return model::fail<rt::Done>(
        Code::invalid_request,
        "the weight coefficient matrix does not match leaves x frames", "coadd",
        "the sealed inputs cannot be combined");
  }
  const U64 frames = weights.coefficients->shape[1];
  if (array_elements(selection.accepted) != leaves * frames ||
      array_elements(selection.eligibility) != leaves * frames ||
      array_elements(selection.reasons) != leaves * frames) {
    return model::fail<rt::Done>(
        Code::invalid_request,
        "the sealed selection arrays do not match leaves x frames", "coadd",
        "the sealed inputs cannot be combined");
  }
  if (array_elements(selection.index->ids) != leaves * 2) {
    return model::fail<rt::Done>(Code::invalid_request,
                                 "the sealed selection leaf identities are malformed", "coadd",
                                 "the sealed inputs cannot be combined");
  }
  return model::ok_result(rt::Done{});
}

// The combination body.  It reads the frozen frames, the sealed leaf order and
// the sealed coefficients; it never selects or solves again.
Result<CoaddResult> combine_sealed(Context& context, const FramesProvider& frames,
                                   Ref<SkyFit> sky, Ref<Selection> selection,
                                   Ref<WeightSolution> weights,
                                   const CoaddRequest& request) {
  Result<std::vector<Id>> frame_ids = frames.list_frames(context);
  if (!ok(frame_ids)) {
    return model::fail<CoaddResult>(Code::unavailable, "the frames cannot be listed", "coadd",
                                    "the combination is not available");
  }
  if (val(frame_ids).empty()) {
    return model::fail<CoaddResult>(Code::invalid_request, "no frame is available", "coadd",
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
  const std::size_t leaf_count =
      selection->index ? static_cast<std::size_t>(selection->index->size) : 0;
  if (leaf_count == 0) {
    return model::fail<CoaddResult>(Code::invalid_request,
                                    "the sealed selection has no leaf", "coadd",
                                    "the combination is not available");
  }
  if (weights->coefficients->shape.size() != 2 ||
      weights->coefficients->shape[1] != frame_count) {
    return model::fail<CoaddResult>(
        Code::identity_conflict,
        "the sealed weights belong to a different frame list", "coadd",
        "the sealed inputs cannot be combined");
  }

  // The leaf order is part of the sealed selection: recompute the union of the
  // frozen frame leaves and compare it with the promised order instead of
  // renumbering silently.
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
  Result<std::vector<U64>> sealed_ids = read_u64_array(context, selection->index->ids);
  if (!ok(sealed_ids)) {
    return model::fail<CoaddResult>(sealed_ids.code, "the sealed leaf order cannot be read", "coadd",
                                    "the combination is not available");
  }
  if (val(sealed_ids).size() != leaves.size() * 2) {
    return model::fail<CoaddResult>(Code::identity_conflict,
                                    "the sealed leaf order has a different size", "coadd",
                                    "the sealed inputs cannot be combined");
  }
  for (std::size_t leaf = 0; leaf < leaves.size(); ++leaf) {
    if (val(sealed_ids)[2 * leaf] != leaves[leaf].order ||
        val(sealed_ids)[2 * leaf + 1] != leaves[leaf].ipix) {
      return model::fail<CoaddResult>(
          Code::identity_conflict,
          "the sealed leaf order differs from the frozen frame order", "coadd",
          "the sealed inputs cannot be combined");
    }
  }

  Result<std::vector<double>> coefficients =
      operators::read_f64(context, weights->coefficients, 0,
                          static_cast<U64>(leaf_count * frame_count));
  if (!ok(coefficients)) {
    return model::fail<CoaddResult>(coefficients.code, "the weights cannot be read", "coadd",
                                    "the combination is not available");
  }
  std::vector<double> combined(leaf_count, 0.0);
  std::vector<std::uint8_t> leaf_state(leaf_count, 0);
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
    // The active weights of a leaf must be a normalised set: the combined value
    // estimates the common quantity only when the active sum is one, and the
    // sealed solution is checked here instead of being trusted.
    if (!(std::fabs(total - 1.0) <= 1e-9)) {
      return model::fail<CoaddResult>(
          Code::invalid_model,
          "the active weights of a leaf do not sum to one; the sealed solution does not "
          "normalise the combined value",
          "coadd", "the sealed inputs cannot be combined");
    }
    combined[leaf] = sum;
    leaf_state[leaf] = 1;
  }
  // The output unit must agree with the covariance unit: C is Var(y), so its
  // unit is unit(y)^2.  An unspecified target unit ("1") inherits the common
  // frame unit; an explicitly different unit is refused instead of silently
  // publishing a signal whose declared unit contradicts its covariance.
  Unit output_unit = request.target.unit;
  const Unit variance_unit = weights && weights->covariance && weights->covariance->model
                                 ? weights->covariance->model->variance_unit
                                 : model::unit_variance("unknown");
  if (unit_is_unspecified(output_unit)) {
    output_unit.scale = variance_unit.scale;
    output_unit.sr_power = variance_unit.sr_power / 2;
    output_unit.pixel_power = variance_unit.pixel_power / 2;
    output_unit.nm_power = variance_unit.nm_power / 2;
    output_unit.linear_power = 1;
    output_unit.multiplier = std::sqrt(std::fabs(variance_unit.multiplier));
  } else {
    const Unit expected = model::unit_multiply(output_unit, output_unit);
    if (!same_unit(expected, variance_unit)) {
      return model::fail<CoaddResult>(
          Code::invalid_request, "the target unit contradicts the frame covariance unit",
          "coadd", "the combination is not available");
    }
  }
  Result<ArrayRef> combined_array = operators::write_f64(
      context, combined, std::vector<U64>{static_cast<U64>(leaf_count)}, output_unit,
      "coadd_signal");
  if (!ok(combined_array)) {
    return model::fail<CoaddResult>(combined_array.code, "the combined signal cannot be stored",
                                    "coadd", "the combination is not available");
  }
  auto estimate = std::make_shared<SignalEstimate>();
  estimate->signal = val(combined_array);
  estimate->unit = output_unit;
  estimate->estimator = request.target;
  estimate->numerical_lower = nullptr;
  estimate->numerical_upper = nullptr;
  // The coadded product carries the leaf support of the combined samples so
  // that the published HiPS knows which leaves exist and which are absent.
  {
    const std::vector<std::byte> state_bytes = model::encode_u8(leaf_state);
    Result<ArrayRef> state_array = context.storage.copy(
        context, DType::u8, state_bytes,
        std::vector<U64>{static_cast<U64>(leaf_state.size())}, model::unit_dimensionless(),
        selection->index ? selection->index->identity.object_id : Id());
    if (!ok(state_array)) {
      return model::fail<CoaddResult>(state_array.code, "the support states cannot be stored",
                                      "coadd", "the combination is not available");
    }
    const double leaf_area_sr =
        4.0 * 3.14159265358979323846 / (12.0 * static_cast<double>(1ull << (2 * (selection->index ? selection->index->order : 0))));
    std::vector<double> area_values(leaf_state.size(), leaf_area_sr);
    Result<ArrayRef> area_array = operators::write_f64(
        context, area_values, std::vector<U64>{static_cast<U64>(area_values.size())},
        model::unit_sr(), "coadd_support_area");
    if (!ok(area_array)) {
      return model::fail<CoaddResult>(area_array.code, "the support area cannot be stored",
                                      "coadd", "the combination is not available");
    }
    auto support = std::make_shared<SupportMap>();
    support->kind = SupportMap::Kind::full_leaf_approx;
    support->index = selection->index;
    support->states = val(state_array);
    support->area_lower_sr = val(area_array);
    support->area_upper_sr = val(area_array);
    support->identity = model::make_identity("acsd.SupportMap", 2, 0,
                                             model::sha256_hex("acsd.coadd.support"), "acsd");
    support->assessment = model::make_assessment(Code::ok);
    estimate->support = std::shared_ptr<const SupportMap>(support);
  }
  estimate->effective_response = nullptr;
  estimate->noise = weights->covariance;
  estimate->error = nullptr;
  estimate->identity = model::make_identity("acsd.SignalEstimate", 2, 0,
                                             model::hash_index(Index::Kind::leaf, leaf_count, 0, {}),
                                             "acsd");
  estimate->assessment = model::make_assessment(Code::ok);
  estimate->assessment.complete = true;

  CoaddResult result;
  result.estimate = std::shared_ptr<const SignalEstimate>(estimate);
  result.selection = selection;
  result.weights = weights;
  result.sky = sky;
  return model::ok_result(std::move(result));
}

}  // namespace

Result<rt::Done> validate_coadd_inputs(const CoaddInputs& inputs) {
  return validate_inputs(inputs);
}

Result<CoaddResult> coadd_with_inputs(Context& context, const CoaddInputs& inputs) {
  Result<rt::Done> validated = validate_inputs(inputs);
  if (!ok(validated)) {
    return model::fail<CoaddResult>(validated.code, "the sealed coadd inputs are invalid", "coadd",
                                    "the combination is not available");
  }
  return combine_sealed(context, *inputs.frames, inputs.sky, inputs.selection, inputs.weights,
                        inputs.request);
}

// The compatibility entry point keeps the historical signature.  It runs the
// selection and the weights once and then delegates to the same combination
// core; it is not a second combination implementation.
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
  CoaddInputs inputs;
  inputs.frames = &frames;
  inputs.sky = std::move(sky);
  inputs.selection = val(selection);
  inputs.weights = val(weights);
  inputs.request = request;
  return coadd_with_inputs(context, inputs);
}

}  // namespace acsd
