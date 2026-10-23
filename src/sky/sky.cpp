// The public sky entry points declared in include/acsd/contracts.hpp.
//
// Scientific contract (STATISTICS_SKY_COADD section 4 and SYSTEM M1):
//   * the control model is c = T y = H s + A beta + e with C_c = T C_y T^T;
//   * the control operator is built only inside a group of frames whose band
//     pass and effective spatial response are comparable; different spectral
//     responses never create a difference mode edge;
//   * without an explicit T and H the standard patch builder is used: a fixed
//     32 x 32 leaf patch of a face chart, one row per frame and patch, the
//     area average over a fixed mask taken from the data quality and the
//     support, never a median or a clip;
//   * H carries one common sky column per patch, shared by the frames of the
//     group; A and E are evaluated with the same basis;
//   * every connected component gets a gauge; the public constant and the two
//     public slopes of a [1,u,v] basis are not identifiable and the gauge is
//     only a convention, never a measured absolute sky;
//   * K H = 0, so an arbitrary common gradient added to the input cannot
//     change the estimated sky parameters;
//   * the corrected covariance is (I - E K T) C_y (I - E K T)^T with both
//     negative cross terms; a direction of E that is not estimable gets no
//     correction at all and is reported as unavailable.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "acsd/contracts.hpp"
#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "numerics/linalg.hpp"
#include "operators/linear_action.hpp"
#include "sky/sky.hpp"
#include "uncertainty/covariance.hpp"

namespace acsd {
namespace {

using numerics::Matrix;

constexpr std::size_t kPatchSide = 32;  // 32 x 32 leaf patch of a face chart

// Data quality bits that disqualify a sample (SYSTEM N1).
constexpr std::uint16_t kBadQualityMask = 0x0001 | 0x0002 | 0x0004 | 0x0008 | 0x0010;

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
                                            "sky.read", "the array cannot be read");
  }
  if (array->dtype != DType::f64) {
    return model::fail<std::vector<double>>(Code::invalid_model, "the array is not f64",
                                            "sky.read", "the array cannot be read");
  }
  const U64 count = static_cast<U64>(count_elements(array->shape));
  return operators::read_f64(context, array, 0, count);
}

Result<std::vector<U64>> read_u64_array(Context& context, ArrayRef array) {
  if (!array) {
    return model::fail<std::vector<U64>>(Code::invalid_request, "no array supplied", "sky.read",
                                         "the array cannot be read");
  }
  if (array->dtype != DType::u64) {
    return model::fail<std::vector<U64>>(Code::invalid_model, "the array is not u64", "sky.read",
                                         "the array cannot be read");
  }
  const U64 count = static_cast<U64>(count_elements(array->shape));
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, count});
  if (!ok(view)) {
    return model::fail<std::vector<U64>>(view.code, "the array cannot be read", "sky.read",
                                         "the array cannot be read");
  }
  return model::ok_result(model::decode_u64(val(view).bytes));
}

Result<std::vector<std::uint8_t>> read_u8_array(Context& context, ArrayRef array) {
  if (!array) {
    return model::fail<std::vector<std::uint8_t>>(Code::invalid_request, "no array supplied",
                                                  "sky.read", "the array cannot be read");
  }
  if (array->dtype != DType::u8) {
    return model::fail<std::vector<std::uint8_t>>(Code::invalid_model, "the array is not u8",
                                                  "sky.read", "the array cannot be read");
  }
  const U64 count = static_cast<U64>(count_elements(array->shape));
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, count});
  if (!ok(view)) {
    return model::fail<std::vector<std::uint8_t>>(view.code, "the array cannot be read",
                                                  "sky.read", "the array cannot be read");
  }
  return model::ok_result(model::decode_u8(val(view).bytes));
}

Result<std::vector<std::uint16_t>> read_u16_array(Context& context, ArrayRef array) {
  if (!array) {
    return model::fail<std::vector<std::uint16_t>>(Code::invalid_request, "no array supplied", "sky.read",
                                         "the array cannot be read");
  }
  if (array->dtype != DType::u16) {
    return model::fail<std::vector<std::uint16_t>>(Code::invalid_model, "the array is not u16", "sky.read",
                                         "the array cannot be read");
  }
  const U64 count = static_cast<U64>(count_elements(array->shape));
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, count});
  if (!ok(view)) {
    return model::fail<std::vector<std::uint16_t>>(view.code, "the array cannot be read", "sky.read",
                                         "the array cannot be read");
  }
  return model::ok_result(model::decode_u16(val(view).bytes));
}

// A frame as it is used by the control builder.
struct FrameBlock {
  Id frame;
  U32 order = 0;
  std::vector<HealpixLeaf> leaves;
  std::vector<double> signal;
  std::vector<std::uint16_t> dq;
  std::vector<std::uint8_t> state;          // support state per leaf
  std::vector<std::uint32_t> patch_of_leaf;  // the patch index of every leaf
  std::size_t offset = 0;                   // offset in the concatenated leaf space
  Ref<CovarianceAction> noise;
};

// The patch grid of the chart: a 32 x 32 block of leaves inside one face.
struct Patch {
  U32 face = 0;
  U64 ix = 0;
  U64 iy = 0;
  SkyICRS center;
};

}  // namespace

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace sky {
namespace {

Vec3 to_vec3(const SkyICRS& v) { return Vec3{v.nx, v.ny, v.nz}; }

Vec3 cross3(const Vec3& a, const Vec3& b) {
  return Vec3{a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

double dot3(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

struct LocalFaceParam {
  int j;
  int l;
};

constexpr LocalFaceParam kLocalFaces[12] = {{2, 1}, {2, 3}, {2, 5}, {2, 7},
                                             {3, 0}, {3, 2}, {3, 4}, {3, 6},
                                             {4, 1}, {4, 3}, {4, 5}, {4, 7}};

}  // namespace

bool make_chart_projection(const SkyICRS& center, double maximum_radius_rad,
                           ChartProjection& out) {
  const double norm = std::sqrt(dot3(to_vec3(center), to_vec3(center)));
  if (!(norm > 0.0) || !std::isfinite(norm)) return false;
  Vec3 c{center.nx / norm, center.ny / norm, center.nz / norm};
  Vec3 east = cross3(Vec3{0.0, 0.0, 1.0}, c);
  double east_norm = std::sqrt(dot3(east, east));
  if (!(east_norm > 1e-12)) {
    east = cross3(Vec3{1.0, 0.0, 0.0}, c);
    east_norm = std::sqrt(dot3(east, east));
    if (!(east_norm > 1e-12)) return false;
  }
  for (double& value : east) value /= east_norm;
  Vec3 north = cross3(c, east);
  const double north_norm = std::sqrt(dot3(north, north));
  if (!(north_norm > 0.0)) return false;
  for (double& value : north) value /= north_norm;
  out.center = SkyICRS{c[0], c[1], c[2]};
  out.east = east;
  out.north = north;
  out.maximum_radius_rad = maximum_radius_rad;
  return true;
}

bool project_to_chart(const ChartProjection& projection, const SkyICRS& direction, Vec2& out) {
  const double norm = std::sqrt(dot3(to_vec3(direction), to_vec3(direction)));
  if (!(norm > 0.0) || !std::isfinite(norm)) return false;
  const Vec3 unit{direction.nx / norm, direction.ny / norm, direction.nz / norm};
  const double denominator = dot3(unit, to_vec3(projection.center));
  if (!(denominator > 1e-9)) return false;
  const double xi = dot3(unit, projection.east) / denominator;
  const double eta = dot3(unit, projection.north) / denominator;
  if (!std::isfinite(xi) || !std::isfinite(eta)) return false;
  out = Vec2{xi, eta};
  return true;
}

bool unproject_from_chart(const ChartProjection& projection, const Vec2& point, SkyICRS& out) {
  Vec3 v = to_vec3(projection.center);
  for (int k = 0; k < 3; ++k) {
    v[k] += point[0] * projection.east[k] + point[1] * projection.north[k];
  }
  const double norm = std::sqrt(dot3(v, v));
  if (!(norm > 0.0) || !std::isfinite(norm)) return false;
  out = SkyICRS{v[0] / norm, v[1] / norm, v[2] / norm};
  return true;
}

std::size_t basis_term_count(const Basis& basis) {
  switch (basis.kind) {
    case Basis::Kind::constant:
      return 1;
    case Basis::Kind::plane:
      return 3;
    case Basis::Kind::polynomial:
      return static_cast<std::size_t>(basis.degree + 1) * (basis.degree + 2) / 2;
  }
  return 1;
}

std::vector<double> evaluate_basis(const Basis& basis, const ChartProjection& projection,
                                    const SkyICRS& position) {
  Vec2 chart_point{0.0, 0.0};
  if (!project_to_chart(projection, position, chart_point)) return {};
  const double scale_x = basis.scale[0];
  const double scale_y = basis.scale[1];
  const double u = (chart_point[0] - basis.center[0]) / scale_x;
  const double v = (chart_point[1] - basis.center[1]) / scale_y;
  std::vector<double> values;
  values.reserve(basis_term_count(basis));
  values.push_back(1.0);
  if (basis.kind == Basis::Kind::constant) return values;
  const U32 degree = basis.kind == Basis::Kind::plane ? 1u : basis.degree;
  for (U32 k = 1; k <= degree; ++k) {
    for (U32 power = k + 1; power-- > 0;) {
      double term = 1.0;
      for (U32 a = 0; a < k - power; ++a) term *= u;
      for (U32 b = 0; b < power; ++b) term *= v;
      values.push_back(term);
    }
  }
  return values;
}

void local_leaf_face_xy(U32 order, U64 ipix, U32& face, U64& ix, U64& iy) {
  const U64 nside = 1ull << order;
  face = static_cast<U32>(ipix / (nside * nside));
  const U64 p = ipix % (nside * nside);
  ix = 0;
  iy = 0;
  for (U32 i = 0; i < order; ++i) {
    if ((p >> (2 * i)) & 1ull) ix |= (1ull << i);
    if ((p >> (2 * i + 1)) & 1ull) iy |= (1ull << i);
  }
}

SkyICRS local_face_point(U32 face, double s, double t) {
  const LocalFaceParam& param = kLocalFaces[face % 12];
  const double r = static_cast<double>(param.j) - s - t;
  const double d = s - t;
  double z = 0.0;
  double phi = 0.0;
  if (r < 1.0) {
    z = 1.0 - r * r / 3.0;
    phi = (M_PI / 4.0) * (param.l + d / r);
  } else if (r <= 3.0) {
    z = (2.0 - r) * 2.0 / 3.0;
    phi = (M_PI / 4.0) * (param.l + d);
  } else {
    z = -1.0 + (4.0 - r) * (4.0 - r) / 3.0;
    phi = (M_PI / 4.0) * (param.l + d / (4.0 - r));
  }
  const double sin_theta = std::sqrt(std::max(0.0, 1.0 - z * z));
  return SkyICRS{sin_theta * std::cos(phi), sin_theta * std::sin(phi), z};
}

SkyICRS local_leaf_center(U32 order, U64 ipix) {
  U32 face = 0;
  U64 ix = 0;
  U64 iy = 0;
  local_leaf_face_xy(order, ipix, face, ix, iy);
  const U64 nside = 1ull << order;
  const double s = (static_cast<double>(ix) + 0.5) / static_cast<double>(nside);
  const double t = (static_cast<double>(iy) + 0.5) / static_cast<double>(nside);
  return local_face_point(face, s, t);
}

double local_leaf_radius_rad(U32 order) {
  const double area = (M_PI / 3.0) / static_cast<double>((1ull << order) * (1ull << order));
  return std::sqrt(area);
}

Result<std::vector<double>> apply_action_full(Context& context, Ref<LinearAction> action,
                                              const std::vector<double>& values) {
  if (!action) {
    return model::fail<std::vector<double>>(Code::invalid_request, "no action supplied",
                                            "sky.apply", "the action cannot be applied");
  }
  const std::vector<U64> shape{static_cast<U64>(values.size())};
  Result<ArrayRef> array =
      operators::write_f64(context, values, shape, model::unit_dimensionless(), Id());
  if (!ok(array)) {
    return model::fail<std::vector<double>>(array.code, "the vector cannot be stored",
                                            "sky.apply", "the action cannot be applied");
  }
  Result<ArrayRef> mapped = action->apply(context, val(array));
  if (!ok(mapped)) {
    return model::fail<std::vector<double>>(mapped.code, "the action cannot be applied",
                                            "sky.apply", "the action cannot be applied");
  }
  const U64 count = static_cast<U64>(count_elements(val(mapped)->shape));
  Result<std::vector<double>> out = operators::read_f64(context, val(mapped), 0, count);
  if (!ok(out)) {
    return model::fail<std::vector<double>>(out.code, "the output cannot be read", "sky.apply",
                                            "the action cannot be applied");
  }
  return out;
}

U64 action_domain_size(Ref<LinearAction> action) {
  if (!action) return 0;
  if (const auto* impl = dynamic_cast<const operators::ActionImpl*>(action.get())) {
    if (impl->domain_size() > 0) return impl->domain_size();
  }
  if (action->spaces.domain) return action->spaces.domain->size;
  return 0;
}

U64 action_codomain_size(Ref<LinearAction> action) {
  if (!action) return 0;
  if (const auto* impl = dynamic_cast<const operators::ActionImpl*>(action.get())) {
    if (impl->codomain_size() > 0) return impl->codomain_size();
  }
  if (action->spaces.codomain) return action->spaces.codomain->size;
  return 0;
}



Result<Matrix> dense_action(Context& context, Ref<LinearAction> action, U64 rows, U64 cols) {
  if (!action) {
    return model::fail<Matrix>(Code::invalid_request, "no action supplied", "sky.dense",
                                "the action cannot be materialised");
  }
  if (rows == 0) rows = action_codomain_size(action);
  if (cols == 0) cols = action_domain_size(action);
  if (rows == 0 || cols == 0) {
    return model::fail<Matrix>(Code::invalid_model, "the action has no usable shape", "sky.dense",
                                "the action cannot be materialised");
  }
  Matrix out(static_cast<std::size_t>(rows), static_cast<std::size_t>(cols));
  std::vector<double> unit(static_cast<std::size_t>(cols), 0.0);
  for (U64 j = 0; j < cols; ++j) {
    std::fill(unit.begin(), unit.end(), 0.0);
    unit[static_cast<std::size_t>(j)] = 1.0;
    Result<std::vector<double>> mapped = apply_action_full(context, action, unit);
    if (!ok(mapped)) {
      return model::fail<Matrix>(mapped.code, "the action cannot be applied", "sky.dense",
                                  "the action cannot be materialised");
    }
    if (val(mapped).size() != rows) {
      return model::fail<Matrix>(Code::invalid_model, "the action output size is inconsistent",
                                  "sky.dense", "the action cannot be materialised");
    }
    for (U64 i = 0; i < rows; ++i) {
      out(static_cast<std::size_t>(i), static_cast<std::size_t>(j)) =
          val(mapped)[static_cast<std::size_t>(i)];
    }
  }
  return model::ok_result(std::move(out));
}

Result<operators::CsrMatrix> to_csr(Context& context, Ref<LinearAction> action, U64 rows,
                                     U64 cols) {
  if (!action) {
    return model::fail<operators::CsrMatrix>(Code::invalid_request, "no action supplied",
                                             "sky.csr", "the action cannot be converted");
  }
  operators::CsrMatrix matrix;
  matrix.rows = rows;
  matrix.cols = cols;
  const auto* impl = dynamic_cast<const operators::ActionImpl*>(action.get());
  if (impl && (impl->recipe().kind == ActionRecipe::Kind::csr ||
               impl->recipe().kind == ActionRecipe::Kind::leaf_merge)) {
    const operators::CsrMatrix& payload = impl->csr();
    if (payload.rows != rows || payload.cols != cols) {
      return model::fail<operators::CsrMatrix>(Code::invalid_model, "the action shape disagrees",
                                               "sky.csr", "the action cannot be converted");
    }
    return model::ok_result(payload);
  }
  Result<Matrix> dense = dense_action(context, action, rows, cols);
  if (!ok(dense)) {
    return model::fail<operators::CsrMatrix>(dense.code, "the action cannot be materialised",
                                             "sky.csr", "the action cannot be converted");
  }
  matrix.offsets.assign(static_cast<std::size_t>(rows) + 1, 0);
  for (std::size_t i = 0; i < static_cast<std::size_t>(rows); ++i) {
    for (std::size_t j = 0; j < static_cast<std::size_t>(cols); ++j) {
      const double value = val(dense)(i, j);
      if (value == 0.0) continue;
      matrix.indices.push_back(static_cast<U64>(j));
      matrix.values.push_back(value);
    }
    matrix.offsets[i + 1] = matrix.indices.size();
  }
  return model::ok_result(std::move(matrix));
}

Result<Ref<LinearAction>> dense_to_action(Context& context, const Matrix& matrix,
                                           const Id& domain_index, const Id& codomain_index,
                                           const Unit& unit) {
  operators::CsrMatrix sparse;
  sparse.rows = matrix.rows;
  sparse.cols = matrix.cols;
  sparse.offsets.assign(matrix.rows + 1, 0);
  for (std::size_t i = 0; i < matrix.rows; ++i) {
    for (std::size_t j = 0; j < matrix.cols; ++j) {
      const double value = matrix(i, j);
      if (value == 0.0) continue;
      sparse.indices.push_back(static_cast<U64>(j));
      sparse.values.push_back(value);
    }
    sparse.offsets[i + 1] = sparse.indices.size();
  }
  return operators::make_csr_action(context, sparse.rows, sparse.cols, sparse.offsets,
                                    sparse.indices, sparse.values, domain_index, codomain_index,
                                    unit);
}

operators::CsrMatrix shift_rows(const operators::CsrMatrix& matrix, U64 offset) {
  operators::CsrMatrix out = matrix;
  (void)offset;
  return out;
}

operators::CsrMatrix block_diagonal(const std::vector<operators::CsrMatrix>& blocks,
                                    const std::vector<U64>& row_offsets) {
  operators::CsrMatrix out;
  if (blocks.empty()) return out;
  out.cols = blocks.front().cols;
  out.rows = 0;
  for (const operators::CsrMatrix& block : blocks) out.rows += block.rows;
  out.offsets.assign(static_cast<std::size_t>(out.rows) + 1, 0);
  U64 row = 0;
  for (std::size_t b = 0; b < blocks.size(); ++b) {
    const operators::CsrMatrix& block = blocks[b];
    for (std::size_t i = 0; i < block.rows; ++i) {
      for (U64 k = block.offsets[i]; k < block.offsets[i + 1]; ++k) {
        out.indices.push_back(block.indices[k]);
        out.values.push_back(block.values[k]);
      }
      out.offsets[row + i + 1] = out.indices.size();
    }
    row += block.rows;
  }
  (void)row_offsets;
  return out;
}

Result<Matrix> dense_covariance_factor(Context& context, Ref<CovarianceAction> covariance) {
  if (!covariance || !covariance->model) {
    return model::fail<Matrix>(Code::invalid_request, "no covariance model supplied",
                                "sky.factor", "the covariance factor cannot be built");
  }
  const NoiseModel& noise = *covariance->model;
  std::vector<Ref<NoiseRoot>> roots;
  roots.insert(roots.end(), noise.innovations.begin(), noise.innovations.end());
  roots.insert(roots.end(), noise.shared_factors.begin(), noise.shared_factors.end());
  if (roots.size() != noise.influence.size()) {
    return model::fail<Matrix>(Code::invalid_model, "the influence count does not match the roots",
                                "sky.factor", "the covariance factor cannot be built");
  }
  U64 rows = 0;
  std::vector<U64> latent_counts;
  U64 total_latent = 0;
  for (const Ref<LinearAction>& influence : noise.influence) {
    const U64 size = action_codomain_size(influence);
    if (size == 0) {
      return model::fail<Matrix>(Code::invalid_model, "an influence has no codomain", "sky.factor",
                                  "the covariance factor cannot be built");
    }
    if (rows == 0) rows = size;
    if (rows != size) {
      return model::fail<Matrix>(Code::invalid_model, "the influences disagree on the codomain",
                                  "sky.factor", "the covariance factor cannot be built");
    }
  }
  for (const Ref<NoiseRoot>& root : roots) {
    const U64 latent = action_domain_size(root->factor);
    if (latent == 0) {
      return model::fail<Matrix>(Code::invalid_model, "a noise factor has no domain", "sky.factor",
                                  "the covariance factor cannot be built");
    }
    latent_counts.push_back(latent);
    total_latent += latent;
  }
  Matrix factor(static_cast<std::size_t>(rows), static_cast<std::size_t>(total_latent));
  U64 offset = 0;
  std::vector<double> unit;
  for (std::size_t k = 0; k < roots.size(); ++k) {
    const U64 latent = latent_counts[k];
    unit.assign(static_cast<std::size_t>(latent), 0.0);
    for (U64 j = 0; j < latent; ++j) {
      std::fill(unit.begin(), unit.end(), 0.0);
      unit[static_cast<std::size_t>(j)] = 1.0;
      Result<std::vector<double>> mapped = apply_action_full(context, roots[k]->factor, unit);
      if (!ok(mapped)) {
        return model::fail<Matrix>(mapped.code, "a noise factor cannot be applied", "sky.factor",
                                    "the covariance factor cannot be built");
      }
      Result<std::vector<double>> influenced =
          apply_action_full(context, noise.influence[k], val(mapped));
      if (!ok(influenced)) {
        return model::fail<Matrix>(influenced.code, "an influence cannot be applied", "sky.factor",
                                    "the covariance factor cannot be built");
      }
      if (val(influenced).size() != rows) {
        return model::fail<Matrix>(Code::invalid_model, "an influence changes the output size",
                                    "sky.factor", "the covariance factor cannot be built");
      }
      for (U64 i = 0; i < rows; ++i) {
        factor(static_cast<std::size_t>(i), static_cast<std::size_t>(offset + j)) =
            val(influenced)[static_cast<std::size_t>(i)];
      }
    }
    offset += latent;
  }
  return model::ok_result(std::move(factor));
}

}  // namespace sky

// ---------------------------------------------------------------------------
// build_sky_controls
// ---------------------------------------------------------------------------

namespace {

bool basis_is_usable(const Basis& basis, std::string& reason) {
  if (!std::isfinite(basis.center[0]) || !std::isfinite(basis.center[1])) {
    reason = "the centre of the basis is not a finite number";
    return false;
  }
  if (!std::isfinite(basis.scale[0]) || !std::isfinite(basis.scale[1]) ||
      !(basis.scale[0] > 0.0) || !(basis.scale[1] > 0.0)) {
    reason = "the scale of the basis is not a finite positive number";
    return false;
  }
  return true;
}

Result<FrameBlock> read_frame(Context& context, const FramesProvider& frames, const Id& frame) {
  Result<Ref<FrameDescription>> described = frames.describe_frame(context, frame);
  if (!ok(described)) {
    return model::fail<FrameBlock>(Code::unavailable, "the frame cannot be described",
                                   "build_sky_controls", "the frame cannot be used");
  }
  const FrameDescription description = *val(described);
  if (!description.index) {
    return model::fail<FrameBlock>(Code::invalid_model, "the frame has no index",
                                   "build_sky_controls", "the frame cannot be used");
  }
  Result<std::vector<U64>> ids = read_u64_array(context, description.index->ids);
  if (!ok(ids)) {
    return model::fail<FrameBlock>(ids.code, "the leaf identities cannot be read",
                                   "build_sky_controls", "the frame cannot be used");
  }
  FrameBlock block;
  block.frame = frame;
  block.order = description.index->order;
  block.noise = description.noise;
  const std::vector<U64>& flat = val(ids);
  if (flat.size() % 2 != 0) {
    return model::fail<FrameBlock>(Code::invalid_model, "the leaf identities are malformed",
                                   "build_sky_controls", "the frame cannot be used");
  }
  const std::size_t count = flat.size() / 2;
  block.leaves.reserve(count);
  for (std::size_t row = 0; row < count; ++row) {
    block.leaves.push_back(HealpixLeaf{static_cast<U32>(flat[2 * row]), flat[2 * row + 1]});
  }
  FrameReadRequest request;
  request.frame = frame;
  request.leaves = block.leaves;
  request.fields = {"signal", "dq"};
  Result<LeafBlock> read = frames.read_leaf_block(context, request);
  if (!ok(read)) {
    return model::fail<FrameBlock>(read.code, "the frame leaves cannot be read",
                                   "build_sky_controls", "the frame cannot be used");
  }
  Result<std::vector<double>> signal = read_f64_array(context, val(read).signal);
  if (!ok(signal)) {
    return model::fail<FrameBlock>(signal.code, "the frame signal cannot be read",
                                   "build_sky_controls", "the frame cannot be used");
  }
  block.signal = val(signal);
  if (val(read).dq) {
    Result<std::vector<std::uint16_t>> dq = read_u16_array(context, val(read).dq);
    if (!ok(dq)) {
      return model::fail<FrameBlock>(dq.code, "the frame quality cannot be read",
                                     "build_sky_controls", "the frame cannot be used");
    }
    block.dq = val(dq);
  }
  Result<Ref<SupportMap>> support = frames.query_support(context, frame, description.index);
  if (ok(support) && val(support) && val(support)->states) {
    Result<std::vector<std::uint8_t>> states = read_u8_array(context, val(support)->states);
    if (!ok(states)) {
      return model::fail<FrameBlock>(states.code, "the support states cannot be read",
                                     "build_sky_controls", "the frame cannot be used");
    }
    if (val(states).size() != count) {
      return model::fail<FrameBlock>(Code::invalid_model,
                                     "the support states disagree with the index",
                                     "build_sky_controls", "the frame cannot be used");
    }
    block.state = val(states);
  } else {
    block.state.assign(count, 1);
  }
  return model::ok_result(std::move(block));
}

// Builds the joint noise model of the frames: the innovations stay separate
// and the shared roots are merged so that the cross frame correlation of the
// control statistics survives.
Result<Ref<NoiseModel>> build_joint_noise(Context& context, const std::vector<FrameBlock>& blocks) {
  auto joint = std::make_shared<NoiseModel>();
  std::vector<Ref<NoiseRoot>> innovations;
  std::vector<Ref<LinearAction>> innovation_influences;
  std::map<std::string, std::size_t> shared_index;
  std::vector<Ref<NoiseRoot>> shared_roots;
  std::vector<std::vector<operators::CsrMatrix>> shared_blocks;

  for (const FrameBlock& block : blocks) {
    if (!block.noise || !block.noise->model) continue;
    const NoiseModel& noise = *block.noise->model;
    std::vector<Ref<NoiseRoot>> roots;
    roots.insert(roots.end(), noise.innovations.begin(), noise.innovations.end());
    roots.insert(roots.end(), noise.shared_factors.begin(), noise.shared_factors.end());
    if (roots.size() != noise.influence.size()) {
      return model::fail<Ref<NoiseModel>>(Code::invalid_model,
                                          "the influence count does not match the root count",
                                          "build_sky_controls", "the joint noise cannot be built");
    }
    for (std::size_t k = 0; k < roots.size(); ++k) {
      const U64 columns = sky::action_domain_size(noise.influence[k]);
      Result<operators::CsrMatrix> matrix =
          sky::to_csr(context, noise.influence[k], block.leaves.size(), columns);
      if (!ok(matrix)) {
        return model::fail<Ref<NoiseModel>>(matrix.code, "an influence cannot be converted",
                                            "build_sky_controls", "the joint noise cannot be built");
      }
      if (k < noise.innovations.size()) {
        Result<Ref<LinearAction>> action = operators::make_csr_action(
            context, val(matrix).rows, val(matrix).cols, val(matrix).offsets, val(matrix).indices,
            val(matrix).values, Id(), Id(), noise.variance_unit);
        if (!ok(action)) {
          return model::fail<Ref<NoiseModel>>(action.code, "an influence cannot be built",
                                              "build_sky_controls",
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

  joint->variance_unit =
      (blocks.empty() || !blocks.front().noise || !blocks.front().noise->model)
          ? model::unit_variance("unknown")
          : blocks.front().noise->model->variance_unit;
  joint->innovations = innovations;
  joint->shared_factors = shared_roots;
  joint->influence = innovation_influences;
  for (std::vector<operators::CsrMatrix>& blocks_of_root : shared_blocks) {
    const operators::CsrMatrix merged = sky::block_diagonal(blocks_of_root, {});
    Result<Ref<LinearAction>> action = operators::make_csr_action(
        context, merged.rows, merged.cols, merged.offsets, merged.indices, merged.values, Id(), Id(),
        joint->variance_unit);
    if (!ok(action)) {
      return model::fail<Ref<NoiseModel>>(action.code, "a shared influence cannot be built",
                                          "build_sky_controls", "the joint noise cannot be built");
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
  joint->assessment.noise = NoiseState::known;
  return model::ok_result(Ref<NoiseModel>(std::shared_ptr<const NoiseModel>(joint)));
}

}  // namespace


Result<Ref<SkyControls>> build_sky_controls(Context& context, const FramesProvider& frames,
                                             const SkyRequest& request) {
  {
    std::string reason;
    if (!basis_is_usable(request.basis, reason)) {
      return model::fail<Ref<SkyControls>>(Code::invalid_model, reason, "build_sky_controls",
                                           "no control can be built");
    }
  }
  if (request.groups.empty()) {
    return model::fail<Ref<SkyControls>>(Code::invalid_request, "no comparable group supplied",
                                         "build_sky_controls", "no control can be built");
  }
  const bool explicit_model = (request.T != nullptr) || (request.H != nullptr);
  if (explicit_model && (!request.T || !request.H)) {
    return model::fail<Ref<SkyControls>>(
        Code::invalid_request, "the control operator and the common sky must be given together",
        "build_sky_controls", "no control can be built");
  }

  // ---- read the frames ----------------------------------------------------
  std::vector<FrameBlock> blocks;
  U32 order = 0;
  for (const Comparability& group : request.groups) {
    if (group.frames.empty()) {
      return model::fail<Ref<SkyControls>>(Code::invalid_request, "a comparable group is empty",
                                           "build_sky_controls", "no control can be built");
    }
    if (group.response_proof.empty()) {
      return model::fail<Ref<SkyControls>>(Code::invalid_request,
                                           "a comparable group has no response proof",
                                           "build_sky_controls",
                                           "the comparability of the group is not established");
    }
    for (const Id& frame : group.frames) {
      Result<FrameBlock> read = read_frame(context, frames, frame);
      if (!ok(read)) {
        return model::fail<Ref<SkyControls>>(read.code, "a frame cannot be read",
                                            "build_sky_controls", "no control can be built");
      }
      if (blocks.empty()) {
        order = val(read).order;
      } else if (val(read).order != order) {
        return model::fail<Ref<SkyControls>>(Code::invalid_request,
                                            "the frames do not share a leaf order",
                                            "build_sky_controls", "no control can be built");
      }
      blocks.push_back(val(read));
    }
  }
  U64 leaf_total = 0;
  for (FrameBlock& block : blocks) {
    block.offset = leaf_total;
    leaf_total += block.leaves.size();
  }

  // ---- the patch grid -----------------------------------------------------
  std::vector<Patch> patches;
  std::map<std::string, std::size_t> patch_index;
  for (std::size_t f = 0; f < blocks.size(); ++f) {
    // The cancellation checkpoint of the patch grid: the controls stop with a
    // named failure and without a partial result.
    if (context.cancellation.requested()) {
      return model::fail<Ref<SkyControls>>(Code::cancelled, "cancelled", "build_sky_controls",
                                           "the controls are cancelled");
    }
    FrameBlock& block = blocks[f];
    block.patch_of_leaf.assign(block.leaves.size(), 0);
    for (std::size_t row = 0; row < block.leaves.size(); ++row) {
      U32 face = 0;
      U64 ix = 0;
      U64 iy = 0;
      sky::local_leaf_face_xy(block.order, block.leaves[row].ipix, face, ix, iy);
      // The patch side never exceeds the grid of the face, so the centre of a
      // patch stays inside its face.
      const U64 nside = 1ull << block.order;
      U64 side = kPatchSide;
      while (side > 1 && side > nside) side >>= 1;
      const U64 bx = ix / side;
      const U64 by = iy / side;
      const std::string key =
          std::to_string(face) + ":" + std::to_string(bx) + ":" + std::to_string(by);
      auto found = patch_index.find(key);
      if (found == patch_index.end()) {
        Patch patch;
        patch.face = face;
        patch.ix = bx;
        patch.iy = by;
        const double s =
            (static_cast<double>(bx * side) + static_cast<double>(side) / 2.0) /
            static_cast<double>(nside);
        const double t =
            (static_cast<double>(by * side) + static_cast<double>(side) / 2.0) /
            static_cast<double>(nside);
        patch.center = sky::local_face_point(face, s, t);
        patch_index[key] = patches.size();
        patches.push_back(patch);
      }
      block.patch_of_leaf[row] = static_cast<std::uint32_t>(patch_index[key]);
    }
  }

  // ---- the control rows ---------------------------------------------------
  std::vector<double> control_values;
  std::vector<SkyICRS> control_positions;
  std::vector<U64> component_frame;
  std::vector<std::size_t> control_patch;
  Matrix T;
  Matrix H;
  std::size_t control_count = 0;

  if (!explicit_model) {
    // One row per frame and patch; the mask is the set of leaves of the patch
    // that every frame of the group supports and that pass the quality test.
    for (std::size_t g = 0; g < request.groups.size(); ++g) {
      const Comparability& group = request.groups[g];
      for (std::size_t f = 0; f < blocks.size(); ++f) {
        bool in_group = false;
        for (const Id& id : group.frames) {
          if (id == blocks[f].frame) in_group = true;
        }
        if (!in_group) continue;
        // The leaves of this frame that every frame of the group supports.
        std::map<std::pair<U64, U64>, bool> common;
        for (std::size_t row = 0; row < blocks[f].leaves.size(); ++row) {
          const HealpixLeaf& leaf = blocks[f].leaves[row];
          bool supported = blocks[f].state[row] == 1;
          for (const Id& id : group.frames) {
            if (id == blocks[f].frame) continue;
            bool found = false;
            for (std::size_t other = 0; other < blocks.size(); ++other) {
              if (blocks[other].frame != id) continue;
              for (std::size_t other_row = 0; other_row < blocks[other].leaves.size(); ++other_row) {
                if (blocks[other].leaves[other_row].order != leaf.order) continue;
                if (blocks[other].leaves[other_row].ipix != leaf.ipix) continue;
                if (blocks[other].state[other_row] != 1) supported = false;
                found = true;
                break;
              }
              break;
            }
            if (!found) supported = false;
          }
          common[{leaf.order, leaf.ipix}] = supported;
        }
        // The mask of every patch of this frame.
        std::map<std::size_t, std::vector<std::size_t>> masks;
        for (std::size_t row = 0; row < blocks[f].leaves.size(); ++row) {
          const HealpixLeaf& leaf = blocks[f].leaves[row];
          if (!common[{leaf.order, leaf.ipix}]) continue;
          if (!blocks[f].dq.empty() && (blocks[f].dq[row] & kBadQualityMask) != 0) continue;
          masks[blocks[f].patch_of_leaf[row]].push_back(row);
        }
        for (const auto& entry : masks) {
          const std::size_t patch = entry.first;
          const std::vector<std::size_t>& mask = entry.second;
          if (mask.empty()) continue;
          double sum = 0.0;
          for (std::size_t row : mask) sum += blocks[f].signal[row];
          control_values.push_back(sum / static_cast<double>(mask.size()));
          control_positions.push_back(patches[patch].center);
          component_frame.push_back(static_cast<U64>(f));
          control_patch.push_back(patch);
          ++control_count;
        }
      }
    }
    if (control_count == 0) {
      return model::fail<Ref<SkyControls>>(
          Code::no_support, "no control survives the support and the quality mask",
          "build_sky_controls", "no control can be built");
    }
    T = Matrix(static_cast<std::size_t>(control_count), static_cast<std::size_t>(leaf_total), 0.0);
    H = Matrix(static_cast<std::size_t>(control_count), static_cast<std::size_t>(patches.size()), 0.0);
    // The masks are rebuilt to fill the rows of T.
    for (std::size_t row = 0; row < control_count; ++row) {
      const std::size_t f = static_cast<std::size_t>(component_frame[row]);
      const std::size_t patch = control_patch[row];
      std::vector<std::size_t> mask;
      for (std::size_t leaf_row = 0; leaf_row < blocks[f].leaves.size(); ++leaf_row) {
        if (blocks[f].patch_of_leaf[leaf_row] != patch) continue;
        if (blocks[f].state[leaf_row] != 1) continue;
        if (!blocks[f].dq.empty() && (blocks[f].dq[leaf_row] & kBadQualityMask) != 0) continue;
        mask.push_back(leaf_row);
      }
      if (mask.empty()) {
        return model::fail<Ref<SkyControls>>(Code::no_support, "a control mask became empty",
                                             "build_sky_controls", "no control can be built");
      }
      const double weight = 1.0 / static_cast<double>(mask.size());
      for (std::size_t leaf_row : mask) {
        T(static_cast<std::size_t>(row),
          static_cast<std::size_t>(blocks[f].offset + leaf_row)) = weight;
      }
      H(static_cast<std::size_t>(row), static_cast<std::size_t>(patch)) = 1.0;
    }
  } else {
    const U64 rows = sky::action_codomain_size(request.T);
    const U64 cols = sky::action_domain_size(request.T);
    if (rows == 0 || cols != leaf_total) {
      return model::fail<Ref<SkyControls>>(Code::invalid_model,
                                           "the control operator does not match the frames",
                                           "build_sky_controls", "no control can be built");
    }
    Result<Matrix> dense_t = sky::dense_action(context, request.T, rows, cols);
    if (!ok(dense_t)) {
      return model::fail<Ref<SkyControls>>(dense_t.code, "the control operator cannot be applied",
                                           "build_sky_controls", "no control can be built");
    }
    T = val(dense_t);
    std::vector<double> signal(static_cast<std::size_t>(leaf_total), 0.0);
    for (std::size_t f = 0; f < blocks.size(); ++f) {
      for (std::size_t row = 0; row < blocks[f].leaves.size(); ++row) {
        signal[static_cast<std::size_t>(blocks[f].offset + row)] = blocks[f].signal[row];
      }
    }
    for (U64 i = 0; i < rows; ++i) {
      double value = 0.0;
      for (U64 j = 0; j < cols; ++j) {
        value += T(static_cast<std::size_t>(i), static_cast<std::size_t>(j)) *
                 signal[static_cast<std::size_t>(j)];
      }
      control_values.push_back(value);
      // The control position is the weighted centre of the leaves of the row.
      double sx = 0.0;
      double sy = 0.0;
      double sz = 0.0;
      std::size_t owner = 0;
      double best = -1.0;
      for (U64 j = 0; j < cols; ++j) {
        const double weight = std::fabs(T(static_cast<std::size_t>(i), static_cast<std::size_t>(j)));
        if (weight == 0.0) continue;
        std::size_t leaf_index = static_cast<std::size_t>(j);
        std::size_t frame_index = 0;
        for (std::size_t f = 0; f < blocks.size(); ++f) {
          if (leaf_index < blocks[f].offset + blocks[f].leaves.size()) {
            frame_index = f;
            leaf_index -= blocks[f].offset;
            break;
          }
        }
        const SkyICRS position = sky::local_leaf_center(
            blocks[frame_index].order, blocks[frame_index].leaves[leaf_index].ipix);
        sx += weight * position.nx;
        sy += weight * position.ny;
        sz += weight * position.nz;
        if (weight > best) {
          best = weight;
          owner = frame_index;
        }
      }
      const double norm = std::sqrt(sx * sx + sy * sy + sz * sz);
      control_positions.push_back(norm > 0.0 ? SkyICRS{sx / norm, sy / norm, sz / norm}
                                              : SkyICRS{0.0, 0.0, 1.0});
      component_frame.push_back(static_cast<U64>(owner));
      control_patch.push_back(0);
    }
    control_count = static_cast<std::size_t>(rows);
  }

  // ---- the chart, the basis and the matrices ------------------------------
  Vec3 mean{0.0, 0.0, 0.0};
  for (const SkyICRS& position : control_positions) {
    mean[0] += position.nx;
    mean[1] += position.ny;
    mean[2] += position.nz;
  }
  const double mean_norm = std::sqrt(mean[0] * mean[0] + mean[1] * mean[1] + mean[2] * mean[2]);
  const SkyICRS chart_center =
      mean_norm > 0.0 ? SkyICRS{mean[0] / mean_norm, mean[1] / mean_norm, mean[2] / mean_norm}
                      : SkyICRS{0.0, 0.0, 1.0};
  sky::ChartProjection projection;
  if (!sky::make_chart_projection(chart_center, M_PI, projection)) {
    return model::fail<Ref<SkyControls>>(Code::invalid_model, "the chart cannot be built",
                                         "build_sky_controls", "no control can be built");
  }
  const std::size_t terms = sky::basis_term_count(request.basis);
  const std::size_t parameters = blocks.size() * terms;
  Matrix A(static_cast<std::size_t>(control_count), static_cast<std::size_t>(parameters), 0.0);
  Matrix E(static_cast<std::size_t>(leaf_total), static_cast<std::size_t>(parameters), 0.0);
  for (std::size_t f = 0; f < blocks.size(); ++f) {
    for (std::size_t leaf_row = 0; leaf_row < blocks[f].leaves.size(); ++leaf_row) {
      const SkyICRS position =
          sky::local_leaf_center(blocks[f].order, blocks[f].leaves[leaf_row].ipix);
      const std::vector<double> values = sky::evaluate_basis(request.basis, projection, position);
      for (std::size_t term = 0; term < terms && term < values.size(); ++term) {
        E(static_cast<std::size_t>(blocks[f].offset + leaf_row),
          static_cast<std::size_t>(f * terms + term)) = values[term];
      }
    }
  }
  for (std::size_t row = 0; row < control_count; ++row) {
    const std::size_t f = static_cast<std::size_t>(component_frame[row]);
    const std::vector<double> values =
        sky::evaluate_basis(request.basis, projection, control_positions[row]);
    for (std::size_t term = 0; term < terms && term < values.size(); ++term) {
      A(static_cast<std::size_t>(row), static_cast<std::size_t>(f * terms + term)) = values[term];
    }
  }
  if (explicit_model) {
    const U64 rows = static_cast<U64>(control_count);
    const U64 columns = sky::action_domain_size(request.H);
    Result<Matrix> dense_h = sky::dense_action(context, request.H, rows, columns);
    if (!ok(dense_h)) {
      return model::fail<Ref<SkyControls>>(dense_h.code, "the common sky cannot be applied",
                                           "build_sky_controls", "no control can be built");
    }
    H = val(dense_h);
  }

  // ---- the control covariance ---------------------------------------------
  Ref<CovarianceAction> control_noise = request.control_noise;
  if (!control_noise) {
    Result<Ref<NoiseModel>> joint = build_joint_noise(context, blocks);
    if (!ok(joint)) {
      return model::fail<Ref<SkyControls>>(joint.code, "the joint noise cannot be built",
                                           "build_sky_controls", "no control can be built");
    }
    Result<Ref<CovarianceAction>> joint_covariance = make_covariance(context, val(joint));
    if (!ok(joint_covariance)) {
      return model::fail<Ref<SkyControls>>(joint_covariance.code,
                                           "the joint covariance cannot be built",
                                           "build_sky_controls", "no control can be built");
    }
    Result<Ref<LinearAction>> t_action =
        sky::dense_to_action(context, T, Id(), Id(), model::unit_dimensionless());
    if (!ok(t_action)) {
      return model::fail<Ref<SkyControls>>(t_action.code, "the control operator cannot be built",
                                           "build_sky_controls", "no control can be built");
    }
    Result<Ref<CovarianceAction>> control_covariance_result =
        control_covariance(context, val(joint_covariance), val(t_action));
    if (!ok(control_covariance_result)) {
      return model::fail<Ref<SkyControls>>(control_covariance_result.code,
                                           "the control covariance cannot be built",
                                           "build_sky_controls", "no control can be built");
    }
    control_noise = val(control_covariance_result);
  }

  // ---- publish ------------------------------------------------------------
  const Id control_index = "sky_control";
  Result<ArrayRef> c_array = operators::write_f64(
      context, control_values, std::vector<U64>{static_cast<U64>(control_values.size())},
      model::unit_dimensionless(), control_index);
  if (!ok(c_array)) {
    return model::fail<Ref<SkyControls>>(c_array.code, "the control values cannot be stored",
                                         "build_sky_controls", "no control can be built");
  }
  Result<Ref<LinearAction>> t_ref =
      sky::dense_to_action(context, T, Id(), Id(), model::unit_dimensionless());
  if (!ok(t_ref)) {
    return model::fail<Ref<SkyControls>>(t_ref.code, "the control operator cannot be built",
                                         "build_sky_controls", "no control can be built");
  }
  Result<Ref<LinearAction>> h_ref =
      sky::dense_to_action(context, H, Id(), Id(), model::unit_dimensionless());
  if (!ok(h_ref)) {
    return model::fail<Ref<SkyControls>>(h_ref.code, "the common sky cannot be built",
                                         "build_sky_controls", "no control can be built");
  }
  Result<Ref<LinearAction>> a_ref =
      sky::dense_to_action(context, A, Id(), Id(), model::unit_dimensionless());
  if (!ok(a_ref)) {
    return model::fail<Ref<SkyControls>>(a_ref.code, "the difference model cannot be built",
                                         "build_sky_controls", "no control can be built");
  }
  Result<Ref<LinearAction>> e_ref =
      sky::dense_to_action(context, E, Id(), Id(), model::unit_dimensionless());
  if (!ok(e_ref)) {
    return model::fail<Ref<SkyControls>>(e_ref.code, "the output model cannot be built",
                                         "build_sky_controls", "no control can be built");
  }
  Result<ArrayRef> component_array = context.storage.copy(
      context, DType::u64, model::encode_u64(component_frame),
      std::vector<U64>{static_cast<U64>(component_frame.size())}, model::unit_dimensionless(),
      control_index);
  if (!ok(component_array)) {
    return model::fail<Ref<SkyControls>>(component_array.code,
                                         "the component identities cannot be stored",
                                         "build_sky_controls", "no control can be built");
  }
  std::vector<double> position_values;
  position_values.reserve(control_positions.size() * 3);
  for (const SkyICRS& position : control_positions) {
    position_values.push_back(position.nx);
    position_values.push_back(position.ny);
    position_values.push_back(position.nz);
  }
  Result<ArrayRef> position_array = operators::write_f64(
      context, position_values,
      std::vector<U64>{static_cast<U64>(control_positions.size()), 3}, model::unit_dimensionless(),
      control_index);
  if (!ok(position_array)) {
    return model::fail<Ref<SkyControls>>(position_array.code,
                                         "the control positions cannot be stored",
                                         "build_sky_controls", "no control can be built");
  }

  auto controls = std::make_shared<SkyControls>();
  controls->c = val(c_array);
  controls->T = val(t_ref);
  controls->H = val(h_ref);
  controls->A = val(a_ref);
  controls->E = val(e_ref);
  controls->noise = control_noise;
  controls->component_ids = val(component_array);
  controls->positions = val(position_array);
  controls->training_support = nullptr;
  controls->identity = model::make_identity("acsd.SkyControls", 2, 0,
                                            model::hash_index(Index::Kind::control, control_count, 0, {}),
                                            "acsd");
  controls->assessment = model::make_assessment(Code::ok);
  controls->assessment.complete = true;
  controls->assessment.condition = model::make_condition("relative_sky_control", "control_statistic");
  Result<Ref<SkyControls>> out = model::ok_result(std::shared_ptr<const SkyControls>(controls));
  out.assessment.condition = controls->assessment.condition;
  model::set_included(out.assessment, explicit_model
                                            ? "explicit_control_operator"
                                            : "standard_32x32_patch_control_operator");
  model::set_included(out.assessment, "control_covariance_from_the_shared_roots");
  model::set_included(out.assessment, "basis_chart_centre_is_the_mean_control_direction");
  model::set_omitted(out.assessment, "sky_prediction_outside_the_training_support");
  return out;
}

// ---------------------------------------------------------------------------
// fit_relative_sky
// ---------------------------------------------------------------------------

namespace {

// Complete orthogonal factorisation: the first m columns of the result span
// the column space of v, the remaining columns span the complement.
Matrix complete_orthogonal_factorisation(const Matrix& v) {
  const std::size_t n = v.rows;
  const std::size_t m = std::min(v.cols, v.rows);
  Matrix work = v;
  Matrix h(n, n, 0.0);
  for (std::size_t i = 0; i < n; ++i) h(i, i) = 1.0;
  std::vector<double> u(n, 0.0);
  for (std::size_t k = 0; k < m; ++k) {
    double norm = 0.0;
    for (std::size_t i = k; i < n; ++i) norm += work(i, k) * work(i, k);
    norm = std::sqrt(norm);
    if (!(norm > 0.0)) continue;
    const double alpha = work(k, k) > 0.0 ? -norm : norm;
    for (std::size_t i = k; i < n; ++i) u[i] = work(i, k);
    u[k] -= alpha;
    double uu = 0.0;
    for (std::size_t i = k; i < n; ++i) uu += u[i] * u[i];
    if (!(uu > 0.0)) continue;
    const double tau = 2.0 / uu;
    for (std::size_t j = k + 1; j < m; ++j) {
      double s = 0.0;
      for (std::size_t i = k; i < n; ++i) s += u[i] * work(i, j);
      s *= tau;
      for (std::size_t i = k; i < n; ++i) work(i, j) -= s * u[i];
    }
    std::vector<double> uh(n, 0.0);
    for (std::size_t j = 0; j < n; ++j) {
      double s = 0.0;
      for (std::size_t i = k; i < n; ++i) s += u[i] * h(i, j);
      uh[j] = s;
    }
    for (std::size_t j = 0; j < n; ++j) {
      const double s = tau * uh[j];
      if (s == 0.0) continue;
      for (std::size_t i = k; i < n; ++i) h(i, j) -= s * u[i];
    }
  }
  return h;
}

// The product of two dense matrices.  The shapes are checked, because a
// product of incompatible matrices is silently wrong.
Result<Matrix> multiply(const Matrix& a, const Matrix& b) {
  if (a.cols != b.rows) {
    return model::fail<Matrix>(Code::invalid_model,
                                "the shapes of the multiplied matrices do not agree",
                                "sky.multiply", "the product cannot be formed");
  }
  Matrix out(a.rows, b.cols, 0.0);
  for (std::size_t i = 0; i < a.rows; ++i) {
    for (std::size_t k = 0; k < a.cols; ++k) {
      const double left = a(i, k);
      if (left == 0.0) continue;
      for (std::size_t j = 0; j < b.cols; ++j) out(i, j) += left * b(k, j);
    }
  }
  return model::ok_result(std::move(out));
}

Matrix transpose(const Matrix& a) {
  Matrix out(a.cols, a.rows, 0.0);
  for (std::size_t i = 0; i < a.rows; ++i) {
    for (std::size_t j = 0; j < a.cols; ++j) out(j, i) = a(i, j);
  }
  return out;
}

}  // namespace

Result<Ref<SkyFit>> fit_relative_sky(Context& context, Ref<SkyControls> controls,
                                      const SkyRequest& request) {
  {
    std::string reason;
    if (!basis_is_usable(request.basis, reason)) {
      return model::fail<Ref<SkyFit>>(Code::invalid_model, reason, "fit_relative_sky",
                                      "the sky cannot be fitted");
    }
  }
  if (context.cancellation.requested()) {
    return model::fail<Ref<SkyFit>>(Code::cancelled, "cancelled", "fit_relative_sky",
                                    "the sky cannot be fitted");
  }
  if (!controls) {
    return model::fail<Ref<SkyFit>>(Code::invalid_request, "no controls supplied",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }
  Result<std::vector<double>> c = read_f64_array(context, controls->c);
  if (!ok(c)) {
    return model::fail<Ref<SkyFit>>(c.code, "the control values cannot be read", "fit_relative_sky",
                                    "the sky cannot be fitted");
  }
  Result<std::vector<U64>> components = read_u64_array(context, controls->component_ids);
  if (!ok(components)) {
    return model::fail<Ref<SkyFit>>(components.code, "the component identities cannot be read",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }
  const std::size_t control_count = val(c).size();
  if (val(components).size() != control_count) {
    return model::fail<Ref<SkyFit>>(Code::invalid_model, "the component identities disagree",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }
  std::vector<Id> frame_order;
  for (const Comparability& group : request.groups) {
    for (const Id& frame : group.frames) frame_order.push_back(frame);
  }
  const std::size_t frame_count = frame_order.size();
  if (frame_count == 0) {
    return model::fail<Ref<SkyFit>>(Code::invalid_request, "no frame is declared",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }
  const std::size_t terms = sky::basis_term_count(request.basis);
  const std::size_t parameters = frame_count * terms;

  const U64 t_rows = sky::action_codomain_size(controls->T);
  const U64 t_cols = sky::action_domain_size(controls->T);
  if (t_rows != control_count || t_cols == 0) {
    return model::fail<Ref<SkyFit>>(Code::invalid_model,
                                    "the control operator has an inconsistent shape",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }
  Result<Matrix> T = sky::dense_action(context, controls->T, t_rows, t_cols);
  if (!ok(T)) {
    return model::fail<Ref<SkyFit>>(T.code, "the control operator cannot be applied",
                                   "fit_relative_sky", "the sky cannot be fitted");
  }
  const U64 h_rows = sky::action_codomain_size(controls->H);
  const U64 h_cols = sky::action_domain_size(controls->H);
  Result<Matrix> H = sky::dense_action(context, controls->H, h_rows, h_cols);
  if (!ok(H)) {
    return model::fail<Ref<SkyFit>>(H.code, "the common sky cannot be applied", "fit_relative_sky",
                                   "the sky cannot be fitted");
  }
  const U64 a_rows = sky::action_codomain_size(controls->A);
  const U64 a_cols = sky::action_domain_size(controls->A);
  Result<Matrix> A = sky::dense_action(context, controls->A, a_rows, a_cols);
  if (!ok(A)) {
    return model::fail<Ref<SkyFit>>(A.code, "the difference model cannot be applied",
                                   "fit_relative_sky", "the sky cannot be fitted");
  }
  if (a_rows != control_count || a_cols != parameters) {
    return model::fail<Ref<SkyFit>>(Code::invalid_model,
                                    "the difference model has an inconsistent shape",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }
  if (!controls->noise) {
    return model::fail<Ref<SkyFit>>(Code::invalid_request, "the control covariance is missing",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }

  // ---- the connected components -------------------------------------------
  // Two frames are connected when they share a common sky column.
  std::vector<std::set<std::size_t>> neighbours(frame_count);
  for (std::size_t col = 0; col < val(H).cols; ++col) {
    std::vector<std::size_t> frames;
    for (std::size_t row = 0; row < control_count; ++row) {
      if (val(H)(row, col) == 0.0) continue;
      const std::size_t frame = static_cast<std::size_t>(val(components)[row]);
      if (frame >= frame_count) continue;
      if (std::find(frames.begin(), frames.end(), frame) == frames.end()) frames.push_back(frame);
    }
    for (std::size_t a = 0; a < frames.size(); ++a) {
      for (std::size_t b = 0; b < frames.size(); ++b) {
        if (a == b) continue;
        neighbours[frames[a]].insert(frames[b]);
      }
    }
  }
  std::vector<int> component(frame_count, -1);
  std::size_t component_count = 0;
  for (std::size_t f = 0; f < frame_count; ++f) {
    if (component[f] >= 0) continue;
    std::vector<std::size_t> stack{f};
    component[f] = static_cast<int>(component_count);
    while (!stack.empty()) {
      const std::size_t current = stack.back();
      stack.pop_back();
      for (std::size_t other : neighbours[current]) {
        if (component[other] < 0) {
          component[other] = static_cast<int>(component_count);
          stack.push_back(other);
        }
      }
    }
    ++component_count;
  }

  // ---- the gauge ----------------------------------------------------------
  std::vector<double> weights(frame_count, 1.0);
  if (request.gauge.kind == Gauge::Kind::weighted_sum) {
    if (!request.gauge.weights) {
      return model::fail<Ref<SkyFit>>(Code::invalid_request,
                                      "the weighted sum gauge has no weights",
                                      "fit_relative_sky", "the sky cannot be fitted");
    }
    Result<std::vector<double>> w = read_f64_array(context, request.gauge.weights);
    if (!ok(w)) {
      return model::fail<Ref<SkyFit>>(w.code, "the gauge weights cannot be read", "fit_relative_sky",
                                     "the sky cannot be fitted");
    }
    if (val(w).size() != frame_count) {
      return model::fail<Ref<SkyFit>>(Code::invalid_request,
                                      "the gauge weights do not match the frames",
                                      "fit_relative_sky", "the sky cannot be fitted");
    }
    for (std::size_t f = 0; f < frame_count; ++f) {
      if (!(val(w)[f] > 0.0)) {
        return model::fail<Ref<SkyFit>>(Code::invalid_request,
                                        "the gauge weights must be positive", "fit_relative_sky",
                                        "the sky cannot be fitted");
      }
      weights[f] = val(w)[f];
    }
  }

  Gauge resolved_gauge = request.gauge;
  std::vector<std::size_t> component_reference(component_count, 0);
  std::vector<std::vector<double>> component_free_basis(component_count);
  std::vector<std::size_t> component_free_columns(component_count, 0);
  for (std::size_t comp = 0; comp < component_count; ++comp) {
    std::vector<std::size_t> members;
    for (std::size_t f = 0; f < frame_count; ++f) {
      if (static_cast<std::size_t>(component[f]) == comp) members.push_back(f);
    }
    if (members.empty()) continue;
    if (request.gauge.kind == Gauge::Kind::reference_frame) {
      std::size_t reference = members.front();
      if (!request.gauge.reference.empty()) {
        bool found = false;
        for (std::size_t f : members) {
          if (frame_order[f] == request.gauge.reference) {
            reference = f;
            found = true;
          }
        }
        if (!found) {
          return model::fail<Ref<SkyFit>>(Code::invalid_request,
                                          "the reference frame is not part of the component",
                                          "fit_relative_sky", "the sky cannot be fitted");
        }
      } else {
        // The reference is the frame with the smallest UTF-8 identity.
        for (std::size_t f : members) {
          if (frame_order[f] < frame_order[reference]) reference = f;
        }
      }
      component_reference[comp] = reference;
      resolved_gauge.reference = frame_order[reference];
    }
    // The constraint rows of the component and the free directions that are
    // orthogonal to them.  The reference frame fixes its own component, the
    // weighted sum fixes the sum of the components of the component.
    const std::size_t local = members.size() * terms;
    std::vector<std::vector<double>> rows;
    if (request.gauge.kind == Gauge::Kind::reference_frame) {
      const std::size_t reference = component_reference[comp];
      for (std::size_t term = 0; term < terms; ++term) {
        std::vector<double> row(local, 0.0);
        row[reference * terms + term] = 1.0;
        rows.push_back(row);
      }
    } else {
      std::vector<double> row(local, 0.0);
      for (std::size_t m = 0; m < members.size(); ++m) {
        for (std::size_t term = 0; term < terms; ++term) {
          row[m * terms + term] = weights[members[m]];
        }
      }
      rows.push_back(row);
    }
    Matrix constraints(rows.size(), local, 0.0);
    for (std::size_t r = 0; r < rows.size(); ++r) {
      for (std::size_t i = 0; i < local; ++i) constraints(r, i) = rows[r][i];
    }
    numerics::SvdResult svd = numerics::jacobi_svd(numerics::transpose(constraints), 1e-12);
    Matrix constrained(static_cast<std::size_t>(local), static_cast<std::size_t>(svd.rank), 0.0);
    for (std::size_t j = 0; j < svd.rank; ++j) {
      for (std::size_t i = 0; i < local; ++i) {
        constrained(static_cast<std::size_t>(i), j) = svd.u(static_cast<std::size_t>(i), j);
      }
    }
    const Matrix complete = complete_orthogonal_factorisation(constrained);
    std::vector<double> basis;
    for (std::size_t j = svd.rank; j < local; ++j) {
      // The sign of a free direction is fixed, so that the fit is
      // reproducible.
      double first = 0.0;
      for (std::size_t i = 0; i < local; ++i) {
        if (std::fabs(complete(i, j)) > 1e-12) {
          first = complete(i, j);
          break;
        }
      }
      const double sign = first < 0.0 ? -1.0 : 1.0;
      for (std::size_t i = 0; i < local; ++i) basis.push_back(sign * complete(i, j));
    }
    component_free_basis[comp] = basis;
    component_free_columns[comp] = local - svd.rank;
  }

  // Build the gauge matrix G (P x q).
  std::size_t gauge_size = 0;
  for (std::size_t comp = 0; comp < component_count; ++comp) {
    std::vector<std::size_t> members;
    for (std::size_t f = 0; f < frame_count; ++f) {
      if (static_cast<std::size_t>(component[f]) == comp) members.push_back(f);
    }
    if (members.empty()) continue;
    gauge_size += component_free_columns[comp];
  }
  Matrix G(static_cast<std::size_t>(parameters), static_cast<std::size_t>(gauge_size), 0.0);
  {
    std::size_t column = 0;
    for (std::size_t comp = 0; comp < component_count; ++comp) {
      std::vector<std::size_t> members;
      for (std::size_t f = 0; f < frame_count; ++f) {
        if (static_cast<std::size_t>(component[f]) == comp) members.push_back(f);
      }
      if (members.empty()) continue;
      const std::vector<double>& basis = component_free_basis[comp];
      const std::size_t columns = component_free_columns[comp];
      const std::size_t local = members.size() * terms;
      for (std::size_t j = 0; j < columns; ++j) {
        for (std::size_t m = 0; m < members.size(); ++m) {
          for (std::size_t term = 0; term < terms; ++term) {
            G(static_cast<std::size_t>(members[m] * terms + term),
              static_cast<std::size_t>(column)) = basis[j * local + m * terms + term];
          }
        }
        ++column;
      }
    }
  }

  // ---- whitening and the profile -----------------------------------------
  Result<Matrix> factor = sky::dense_covariance_factor(context, controls->noise);
  if (!ok(factor)) {
    return model::fail<Ref<SkyFit>>(factor.code, "the control covariance factor cannot be built",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }
  if (val(factor).rows != control_count) {
    return model::fail<Ref<SkyFit>>(Code::invalid_model,
                                    "the control covariance does not match the controls",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }
  numerics::SvdResult factor_svd = numerics::jacobi_svd(val(factor), 1e-12);
  Matrix W(static_cast<std::size_t>(control_count), static_cast<std::size_t>(control_count), 0.0);
  for (std::size_t j = 0; j < factor_svd.rank; ++j) {
    const double sigma = factor_svd.singular_values[j];
    if (!(sigma > 0.0)) continue;
    for (std::size_t i = 0; i < control_count; ++i) {
      for (std::size_t k = 0; k < control_count; ++k) {
        W(static_cast<std::size_t>(i), static_cast<std::size_t>(k)) +=
            factor_svd.u(static_cast<std::size_t>(i), j) * factor_svd.u(static_cast<std::size_t>(k), j) / sigma;
      }
    }
  }
  // Q_w is the orthogonal complement of the column space of W H.
  Result<Matrix> whitened_h = multiply(W, val(H));
  if (!ok(whitened_h)) {
    return model::fail<Ref<SkyFit>>(whitened_h.code, "the whitened common sky cannot be formed",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }
  numerics::SvdResult h_svd = numerics::jacobi_svd(val(whitened_h), 1e-12);
  Matrix basis_h(static_cast<std::size_t>(control_count), h_svd.rank, 0.0);
  for (std::size_t j = 0; j < h_svd.rank; ++j) {
    for (std::size_t i = 0; i < control_count; ++i) {
      basis_h(static_cast<std::size_t>(i), static_cast<std::size_t>(j)) = h_svd.u(static_cast<std::size_t>(i), j);
    }
  }
  const Matrix complete = complete_orthogonal_factorisation(basis_h);
  const std::size_t complement_size = control_count - h_svd.rank;
  Matrix Qw(static_cast<std::size_t>(control_count), complement_size, 0.0);
  for (std::size_t i = 0; i < control_count; ++i) {
    for (std::size_t j = 0; j < complement_size; ++j) {
      Qw(static_cast<std::size_t>(i), static_cast<std::size_t>(j)) =
          complete(static_cast<std::size_t>(i), h_svd.rank + j);
    }
  }

  // The effective design D = Q_w^T W A G and the projected data Q_w^T W c.
  const Matrix qw_transpose = numerics::transpose(Qw);
  Result<Matrix> whitened_design = multiply(W, val(A));
  if (!ok(whitened_design)) {
    return model::fail<Ref<SkyFit>>(whitened_design.code, "the whitened design cannot be formed",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }
  Result<Matrix> projected_design = multiply(qw_transpose, val(whitened_design));
  if (!ok(projected_design)) {
    return model::fail<Ref<SkyFit>>(projected_design.code, "the projected design cannot be formed",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }
  Result<Matrix> design = multiply(val(projected_design), G);
  if (!ok(design)) {
    return model::fail<Ref<SkyFit>>(design.code, "the effective design cannot be formed",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }

  std::vector<double> data(static_cast<std::size_t>(control_count), 0.0);
  for (std::size_t i = 0; i < control_count; ++i) data[i] = val(c)[i];
  Matrix c_column(static_cast<std::size_t>(control_count), 1, 0.0);
  for (std::size_t i = 0; i < control_count; ++i) c_column(static_cast<std::size_t>(i), 0) = data[i];
  Result<Matrix> whitened_column = multiply(W, c_column);
  if (!ok(whitened_column)) {
    return model::fail<Ref<SkyFit>>(whitened_column.code, "the whitened data cannot be formed",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }
  Result<Matrix> whitened_data = multiply(qw_transpose, val(whitened_column));
  if (!ok(whitened_data)) {
    return model::fail<Ref<SkyFit>>(whitened_data.code, "the projected data cannot be formed",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }

  // The minimum norm solution of the projected least squares problem.
  numerics::SvdResult design_svd = numerics::jacobi_svd(val(design), 1e-12);
  std::vector<double> projected_data(complement_size, 0.0);
  for (std::size_t i = 0; i < complement_size; ++i) {
    projected_data[i] = val(whitened_data)(static_cast<std::size_t>(i), 0);
  }
  // The pseudo inverse of the projected design comes from the base, so that
  // the threshold of the singular values is the same everywhere.
  const Matrix pseudoinverse =
      numerics::pseudo_inverse(design_svd, static_cast<std::size_t>(gauge_size), 1e-12);
  // K = G (Q_w W A G)^+ Q_w W.
  Result<Matrix> projector = multiply(qw_transpose, W);
  if (!ok(projector)) {
    return model::fail<Ref<SkyFit>>(projector.code, "the whitened projector cannot be formed",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }
  Result<Matrix> inverse = multiply(pseudoinverse, val(projector));
  if (!ok(inverse)) {
    return model::fail<Ref<SkyFit>>(inverse.code, "the estimator cannot be formed",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }
  Result<Matrix> k_matrix = multiply(G, val(inverse));
  if (!ok(k_matrix)) {
    return model::fail<Ref<SkyFit>>(k_matrix.code, "the estimator cannot be formed",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }
  const Matrix K = val(k_matrix);
  // beta = K c.
  Result<Matrix> beta_result = multiply(K, c_column);
  if (!ok(beta_result)) {
    return model::fail<Ref<SkyFit>>(beta_result.code, "the components cannot be formed",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }
  const Matrix beta_column = val(beta_result);
  std::vector<double> beta(static_cast<std::size_t>(parameters), 0.0);
  for (std::size_t p = 0; p < parameters; ++p) beta[p] = beta_column(static_cast<std::size_t>(p), 0);

  // The null space of the design, mapped back to the parameter space.  The
  // null space of the design is the orthogonal complement of its row space in
  // the space of the gauge, so the range of the transposed design is
  // completed to an orthonormal basis of the space of the gauge.
  const std::size_t null_count = (gauge_size > design_svd.rank) ? gauge_size - design_svd.rank : 0;
  Matrix null_parameters(static_cast<std::size_t>(parameters), null_count, 0.0);
  if (null_count > 0) {
    numerics::SvdResult transposed = numerics::jacobi_svd(numerics::transpose(val(design)), 1e-12);
    Matrix range(static_cast<std::size_t>(gauge_size), static_cast<std::size_t>(transposed.rank), 0.0);
    for (std::size_t j = 0; j < transposed.rank; ++j) {
      for (std::size_t i = 0; i < gauge_size; ++i) {
        range(static_cast<std::size_t>(i), j) = transposed.u(static_cast<std::size_t>(i), j);
      }
    }
    const Matrix complete = complete_orthogonal_factorisation(range);
    for (std::size_t j = 0; j < null_count; ++j) {
      for (std::size_t p = 0; p < parameters; ++p) {
        double sum = 0.0;
        for (std::size_t i = 0; i < gauge_size; ++i) {
          sum += G(static_cast<std::size_t>(p), static_cast<std::size_t>(i)) *
                 complete(static_cast<std::size_t>(i), transposed.rank + j);
        }
        null_parameters(static_cast<std::size_t>(p), j) = sum;
      }
    }
  }

  // ---- publish ------------------------------------------------------------
  Result<ArrayRef> beta_array = operators::write_f64(
      context, beta, std::vector<U64>{static_cast<U64>(beta.size())}, model::unit_dimensionless(),
      "sky_parameter");
  if (!ok(beta_array)) {
    return model::fail<Ref<SkyFit>>(beta_array.code, "the parameters cannot be stored",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }
  Result<Ref<LinearAction>> k_action =
      sky::dense_to_action(context, K, Id(), Id(), model::unit_dimensionless());
  if (!ok(k_action)) {
    return model::fail<Ref<SkyFit>>(k_action.code, "the estimator cannot be built",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }
  if (!controls->E) {
    return model::fail<Ref<SkyFit>>(Code::invalid_model, "the output model is missing",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }
  // The correction action P = I - E K T.
  ActionRecipe sky_recipe;
  sky_recipe.kind = ActionRecipe::Kind::sky;
  sky_recipe.algorithm = "canonical_sky_v1";
  sky_recipe.version = 1;
  sky_recipe.children = {"E", "K", "T"};
  const std::vector<Ref<LinearAction>> children{controls->E, val(k_action), controls->T};
  Result<Ref<LinearAction>> p_action =
      make_action(context, sky_recipe, children, std::span<const ArrayRef>{});
  if (!ok(p_action)) {
    return model::fail<Ref<SkyFit>>(p_action.code, "the correction action cannot be built",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }

  // The parameter covariance: K C_c K^T, with an unknown term for the
  // directions that are not estimable.
  Ref<CovarianceAction> parameter_covariance;
  {
    auto noise_model = std::make_shared<NoiseModel>();
    const NoiseModel& source = *controls->noise->model;
    noise_model->variance_unit = source.variance_unit;
    noise_model->condition = source.condition;
    noise_model->state = source.state;
    noise_model->unknown_terms = source.unknown_terms;
    noise_model->innovations = source.innovations;
    noise_model->shared_factors = source.shared_factors;
    noise_model->influence.reserve(source.influence.size());
    for (const Ref<LinearAction>& influence : source.influence) {
      Result<Ref<LinearAction>> composed = compose(context, val(k_action), influence);
      if (!ok(composed)) {
        return model::fail<Ref<SkyFit>>(composed.code, "the parameter covariance cannot be built",
                                        "fit_relative_sky", "the sky cannot be fitted");
      }
      noise_model->influence.push_back(val(composed));
    }
    if (null_parameters.cols > 0) {
      UnknownTerm term;
      term.id = "not_estimable_sky_directions";
      term.reason = "the difference model has directions that the common sky cannot separate";
      Result<Ref<LinearAction>> influence = sky::dense_to_action(
          context, null_parameters, Id(), Id(), model::unit_dimensionless());
      if (!ok(influence)) {
        return model::fail<Ref<SkyFit>>(influence.code, "the unknown term cannot be built",
                                        "fit_relative_sky", "the sky cannot be fitted");
      }
      term.influence = val(influence);
      noise_model->unknown_terms.push_back(term);
    }
    noise_model->identity = model::make_identity("acsd.NoiseModel", 2, 0,
                                                 model::hash_index(Index::Kind::latent,
                                                                   noise_model->influence.size(),
                                                                   0, {}),
                                                 "acsd");
    noise_model->assessment = model::make_assessment(Code::ok);
    noise_model->assessment.complete = true;
    Result<Ref<CovarianceAction>> covariance = make_covariance(
        context, Ref<NoiseModel>(std::shared_ptr<const NoiseModel>(noise_model)));
    if (!ok(covariance)) {
      return model::fail<Ref<SkyFit>>(covariance.code, "the parameter covariance cannot be built",
                                      "fit_relative_sky", "the sky cannot be fitted");
    }
    parameter_covariance = val(covariance);
  }

  RankReport rank;
  rank.columns = gauge_size;
  rank.rank = design_svd.rank;
  rank.singular_error_bound = design_svd.sigma_error_bound;
  rank.certified = (design_svd.rank == gauge_size) && (null_count == 0);
  {
    Result<ArrayRef> singular = operators::write_f64(
        context, design_svd.singular_values,
        std::vector<U64>{static_cast<U64>(design_svd.singular_values.size())},
        model::unit_dimensionless(), "sky_parameter");
    if (!ok(singular)) {
      return model::fail<Ref<SkyFit>>(singular.code, "the singular values cannot be stored",
                                      "fit_relative_sky", "the sky cannot be fitted");
    }
    rank.singular_values = val(singular);
    std::vector<double> flat;
    for (std::size_t j = 0; j < null_parameters.cols; ++j) {
      for (std::size_t p = 0; p < parameters; ++p) {
        flat.push_back(null_parameters(static_cast<std::size_t>(p), j));
      }
    }
    Result<ArrayRef> nullspace = operators::write_f64(
        context, flat, std::vector<U64>{static_cast<U64>(null_parameters.cols),
                                        static_cast<U64>(parameters)},
        model::unit_dimensionless(), "sky_parameter");
    if (!ok(nullspace)) {
      return model::fail<Ref<SkyFit>>(nullspace.code, "the null space cannot be stored",
                                      "fit_relative_sky", "the sky cannot be fitted");
    }
    rank.nullspace = val(nullspace);
  }

  auto fit = std::make_shared<SkyFit>();
  fit->basis = request.basis;
  fit->gauge = resolved_gauge;
  fit->beta = val(beta_array);
  fit->K = val(k_action);
  fit->T = controls->T;
  fit->E = controls->E;
  fit->P = val(p_action);
  fit->parameter_cov = parameter_covariance;
  fit->rank = rank;
  fit->support = controls->training_support;
  fit->component_ids = controls->component_ids;
  fit->identity = model::make_identity("acsd.SkyFit", 2, 0,
                                       model::hash_index(Index::Kind::parameter, parameters, 0, {}),
                                       "acsd");
  fit->assessment = model::make_assessment(Code::ok);
  fit->assessment.complete = true;
  fit->assessment.condition = model::make_condition("relative_sky", "control_statistic");

  Result<Ref<SkyFit>> out = model::ok_result(std::shared_ptr<const SkyFit>(fit));
  out.assessment.condition = fit->assessment.condition;
  model::set_included(out.assessment, "gauge_subspace");
  model::set_included(out.assessment, "whitened_profile_of_the_common_sky");
  model::set_included(out.assessment, "corrected_covariance_action");
  if (null_parameters.cols > 0) {
    model::set_omitted(out.assessment, "not_estimable_sky_directions");
    out.assessment.noise = NoiseState::unknown;
    model::add_issue(out.assessment, Code::not_identifiable,
                     "the difference model has directions that the common sky cannot separate",
                     "fit_relative_sky", "the sky is not measured in those directions");
    out.assessment.science = ScienceState::conditional;
  }
  return out;
}

// ---------------------------------------------------------------------------
// apply_sky
// ---------------------------------------------------------------------------

Result<Ref<SignalEstimate>> apply_sky(Context& context, Ref<SignalEstimate> signal,
                                       Ref<SkyFit> fit) {
  if (!signal || !fit) {
    return model::fail<Ref<SignalEstimate>>(Code::invalid_request, "the inputs are incomplete",
                                            "apply_sky", "the correction cannot be applied");
  }
  if (!signal->signal) {
    return model::fail<Ref<SignalEstimate>>(Code::invalid_request, "the signal is missing",
                                            "apply_sky", "the correction cannot be applied");
  }
  Result<std::vector<double>> values = read_f64_array(context, signal->signal);
  if (!ok(values)) {
    return model::fail<Ref<SignalEstimate>>(values.code, "the signal cannot be read", "apply_sky",
                                            "the correction cannot be applied");
  }
  Result<std::vector<double>> beta = read_f64_array(context, fit->beta);
  if (!ok(beta)) {
    return model::fail<Ref<SignalEstimate>>(beta.code, "the sky parameters cannot be read",
                                            "apply_sky", "the correction cannot be applied");
  }
  Result<std::vector<double>> correction = sky::apply_action_full(context, fit->E, val(beta));
  if (!ok(correction)) {
    return model::fail<Ref<SignalEstimate>>(correction.code, "the correction cannot be applied",
                                            "apply_sky", "the correction cannot be applied");
  }
  std::vector<double> mutable_correction = val(correction);
  if (mutable_correction.size() != val(values).size()) {
    return model::fail<Ref<SignalEstimate>>(Code::invalid_model,
                                            "the correction does not match the signal",
                                            "apply_sky", "the correction cannot be applied");
  }

  // A direction of E that is not estimable gets no correction at all.
  std::size_t unavailable_count = 0;
  if (fit->rank.nullspace && fit->rank.nullspace->shape.size() == 2) {
    const U64 columns = fit->rank.nullspace->shape[1];
    const U64 rows = fit->rank.nullspace->shape[0];
    if (rows > 0 && columns > 0) {
      Result<std::vector<double>> nullspace = read_f64_array(context, fit->rank.nullspace);
      if (!ok(nullspace)) {
        return model::fail<Ref<SignalEstimate>>(nullspace.code, "the null space cannot be read",
                                                "apply_sky", "the correction cannot be applied");
      }
      const U64 e_rows = sky::action_codomain_size(fit->E);
      const U64 e_cols = sky::action_domain_size(fit->E);
      Result<Matrix> E = sky::dense_action(context, fit->E, e_rows, e_cols);
      if (!ok(E)) {
        return model::fail<Ref<SignalEstimate>>(E.code, "the output model cannot be applied",
                                                "apply_sky", "the correction cannot be applied");
      }
      for (std::size_t i = 0; i < val(values).size() && i < val(E).rows; ++i) {
        double norm = 0.0;
        for (std::size_t j = 0; j < rows; ++j) {
          double sum = 0.0;
          for (std::size_t p = 0; p < columns && p < val(E).cols; ++p) {
            sum += val(nullspace)[j * columns + p] * val(E)(static_cast<std::size_t>(i),
                                                             static_cast<std::size_t>(p));
          }
          norm += sum * sum;
        }
        if (std::sqrt(norm) > 1e-12) {
          mutable_correction[i] = 0.0;
          ++unavailable_count;
        }
      }
    }
  }

  std::vector<double> corrected(val(values).size(), 0.0);
  for (std::size_t i = 0; i < corrected.size(); ++i) {
    corrected[i] = val(values)[i] - mutable_correction[i];
  }
  Result<ArrayRef> corrected_array =
      operators::write_f64(context, corrected, signal->signal->shape, signal->unit,
                           signal->signal->index_id);
  if (!ok(corrected_array)) {
    return model::fail<Ref<SignalEstimate>>(corrected_array.code,
                                            "the corrected signal cannot be stored",
                                            "apply_sky", "the correction cannot be applied");
  }

  // The corrected covariance is (I - E K T) C_y (I - E K T)^T.
  Ref<CovarianceAction> corrected_noise = signal->noise;
  if (signal->noise && signal->noise->model && fit->P) {
    auto noise_model = std::make_shared<NoiseModel>();
    const NoiseModel& source = *signal->noise->model;
    noise_model->variance_unit = source.variance_unit;
    noise_model->condition = source.condition;
    noise_model->state = source.state;
    noise_model->unknown_terms = source.unknown_terms;
    noise_model->innovations = source.innovations;
    noise_model->shared_factors = source.shared_factors;
    noise_model->influence.reserve(source.influence.size());
    for (const Ref<LinearAction>& influence : source.influence) {
      Result<Ref<LinearAction>> composed = compose(context, fit->P, influence);
      if (!ok(composed)) {
        return model::fail<Ref<SignalEstimate>>(composed.code,
                                                "the corrected covariance cannot be built",
                                                "apply_sky", "the correction cannot be applied");
      }
      noise_model->influence.push_back(val(composed));
    }
    noise_model->identity = model::make_identity("acsd.NoiseModel", 2, 0,
                                                 model::hash_index(Index::Kind::latent,
                                                                   noise_model->influence.size(),
                                                                   0, {}),
                                                 "acsd");
    noise_model->assessment = model::make_assessment(Code::ok);
    noise_model->assessment.complete = true;
    Result<Ref<CovarianceAction>> covariance = make_covariance(
        context, Ref<NoiseModel>(std::shared_ptr<const NoiseModel>(noise_model)));
    if (!ok(covariance)) {
      return model::fail<Ref<SignalEstimate>>(covariance.code,
                                              "the corrected covariance cannot be built",
                                              "apply_sky", "the correction cannot be applied");
    }
    corrected_noise = val(covariance);
  }

  auto estimate = std::make_shared<SignalEstimate>();
  estimate->signal = val(corrected_array);
  estimate->unit = signal->unit;
  estimate->estimator = signal->estimator;
  estimate->numerical_lower = signal->numerical_lower;
  estimate->numerical_upper = signal->numerical_upper;
  estimate->support = signal->support;
  estimate->effective_response = signal->effective_response;
  estimate->noise = corrected_noise;
  estimate->error = signal->error;
  estimate->identity = model::make_identity("acsd.SignalEstimate", 2, 0, signal->identity.content_hash,
                                             "acsd");
  estimate->assessment = model::make_assessment(Code::ok);
  estimate->assessment.complete = true;
  estimate->assessment.condition = signal->estimator.condition;

  Result<Ref<SignalEstimate>> out = model::ok_result(std::shared_ptr<const SignalEstimate>(estimate));
  out.assessment.condition = estimate->assessment.condition;
  model::set_included(out.assessment, "relative_sky_correction");
  model::set_included(out.assessment, "corrected_covariance_with_both_negative_cross_terms");
  if (unavailable_count > 0) {
    model::add_issue(out.assessment, Code::unavailable,
                     "the sky is not estimable in some directions, those positions keep no correction",
                     "apply_sky", "the correction is unavailable at those positions");
    out.assessment.science = ScienceState::conditional;
  }
  return out;
}


}  // namespace acsd
