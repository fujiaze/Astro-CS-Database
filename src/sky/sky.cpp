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
  // The constant term is 1 at every position and needs no chart at all: making
  // it depend on a successful gnomonic projection would turn a chart limit into
  // a silently missing constant column.
  if (basis.kind == Basis::Kind::constant) return {1.0};
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
    // A composition publishes the spaces of its outer action only, so the
    // domain is the domain of its inner child.  Without this fallback a
    // composed influence has no usable shape and every consumer that asks for
    // it refuses the whole covariance.
    if (impl->recipe().kind == ActionRecipe::Kind::compose && impl->children().size() == 2) {
      const U64 inner = action_domain_size(impl->children()[1]);
      if (inner > 0) return inner;
    }
    // The sky action P = I - E K T acts on the codomain of E.
    if (impl->recipe().kind == ActionRecipe::Kind::sky && !impl->children().empty()) {
      const U64 rows = action_codomain_size(impl->children()[0]);
      if (rows > 0) return rows;
    }
  }
  if (action->spaces.domain) return action->spaces.domain->size;
  return 0;
}

U64 action_codomain_size(Ref<LinearAction> action) {
  if (!action) return 0;
  if (const auto* impl = dynamic_cast<const operators::ActionImpl*>(action.get())) {
    if (impl->codomain_size() > 0) return impl->codomain_size();
    if (impl->recipe().kind == ActionRecipe::Kind::compose && impl->children().size() == 2) {
      const U64 outer = action_codomain_size(impl->children()[0]);
      if (outer > 0) return outer;
    }
    if (impl->recipe().kind == ActionRecipe::Kind::sky && !impl->children().empty()) {
      const U64 rows = action_codomain_size(impl->children()[0]);
      if (rows > 0) return rows;
    }
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
  // Materialising a large latent space column by column is not a bounded
  // operation.  This is a capability limit, not a scientific result: the
  // caller keeps the covariance unknown instead of hanging.
  if (rows > (1ull << 12) || cols > (1ull << 12)) {
    return model::fail<Matrix>(Code::unavailable,
                               "the influence is too large to materialise (" +
                                   std::to_string(rows) + "x" + std::to_string(cols) +
                                   "); the covariance stays unknown",
                               "sky.dense", "the covariance is unknown");
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

Result<operators::CsrMatrix> multiply_csr(Context& context, Ref<LinearAction> outer,
                                            Ref<LinearAction> inner) {
  // The exact product of two sparse actions.  It is formed here because the
  // covariance of a corrected quantity needs the product itself: a covariance
  // whose influences are composed through the operators module is applied
  // through the transpose of the composition, and that transpose is the
  // pre-existing cross module defect reported with this fix.
  const U64 outer_rows = action_codomain_size(outer);
  const U64 outer_cols = action_domain_size(outer);
  const U64 inner_rows = action_codomain_size(inner);
  const U64 inner_cols = action_domain_size(inner);
  if (outer_cols != inner_rows || outer_rows == 0 || inner_cols == 0) {
    return model::fail<operators::CsrMatrix>(Code::invalid_model,
                                             "the shapes of the composed actions do not agree",
                                             "sky.multiply", "the product cannot be formed");
  }
  Result<operators::CsrMatrix> left = to_csr(context, outer, outer_rows, outer_cols);
  if (!ok(left)) return left;
  Result<operators::CsrMatrix> right = to_csr(context, inner, inner_rows, inner_cols);
  if (!ok(right)) return right;
  operators::CsrMatrix out;
  out.rows = outer_rows;
  out.cols = inner_cols;
  out.offsets.assign(static_cast<std::size_t>(outer_rows) + 1, 0);
  std::vector<double> row(static_cast<std::size_t>(inner_cols), 0.0);
  std::vector<U64> touched;
  for (U64 i = 0; i < outer_rows; ++i) {
    touched.clear();
    for (U64 k = val(left).offsets[i]; k < val(left).offsets[i + 1]; ++k) {
      const U64 middle = val(left).indices[k];
      const double scale = val(left).values[k];
      if (scale == 0.0 || middle >= val(right).rows) continue;
      for (U64 j = val(right).offsets[middle]; j < val(right).offsets[middle + 1]; ++j) {
        const U64 column = val(right).indices[j];
        if (row[static_cast<std::size_t>(column)] == 0.0) touched.push_back(column);
        row[static_cast<std::size_t>(column)] += scale * val(right).values[j];
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
}

Result<Ref<LinearAction>> action_from_csr(Context& context, const operators::CsrMatrix& matrix,
                                           const Unit& unit) {
  return operators::make_csr_action(context, matrix.rows, matrix.cols, matrix.offsets,
                                    matrix.indices, matrix.values, Id(), Id(), unit);
}

operators::CsrMatrix shift_rows(const operators::CsrMatrix& matrix, U64 offset, U64 total_rows) {
  // The matrix keeps its columns (the latent space of the root) and its rows
  // are placed at [offset, offset + rows) of a larger row space.  The rows
  // before the offset and after the block are exact zero rows: the root does
  // not touch them.  The row count is the total row space, not offset + rows,
  // otherwise the block of the first frame would shrink the joint space.
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
}

operators::CsrMatrix block_diagonal(const std::vector<operators::CsrMatrix>& blocks,
                                    const std::vector<U64>& row_offsets, U64 total_rows) {
  // Every block keeps its own columns and is placed at its row offset.  A block
  // that is missing for a row group leaves exact zero rows, which is the
  // truthful statement that the root does not influence those rows.
  operators::CsrMatrix out;
  if (blocks.empty()) return out;
  for (const operators::CsrMatrix& block : blocks) out.cols = std::max(out.cols, block.cols);
  U64 rows = total_rows;
  for (std::size_t b = 0; b < blocks.size(); ++b) {
    const U64 offset = row_offsets.empty() ? 0 : row_offsets[b];
    rows = std::max(rows, offset + blocks[b].rows);
  }
  out.rows = rows;
  out.offsets.assign(static_cast<std::size_t>(rows) + 1, 0);
  // The row of every block, or the block that owns it.
  std::vector<int> owner(static_cast<std::size_t>(rows), -1);
  std::vector<U64> local(static_cast<std::size_t>(rows), 0);
  U64 cursor = 0;
  for (std::size_t b = 0; b < blocks.size(); ++b) {
    const U64 offset = row_offsets.empty() ? cursor : row_offsets[b];
    for (U64 i = 0; i < blocks[b].rows; ++i) {
      if (offset + i < rows) {
        owner[static_cast<std::size_t>(offset + i)] = static_cast<int>(b);
        local[static_cast<std::size_t>(offset + i)] = i;
      }
    }
    cursor = offset + blocks[b].rows;
  }
  for (U64 row = 0; row < rows; ++row) {
    const int b = owner[static_cast<std::size_t>(row)];
    if (b >= 0) {
      const operators::CsrMatrix& block = blocks[static_cast<std::size_t>(b)];
      const U64 i = local[static_cast<std::size_t>(row)];
      for (U64 k = block.offsets[i]; k < block.offsets[i + 1]; ++k) {
        out.indices.push_back(block.indices[k]);
        out.values.push_back(block.values[k]);
      }
    }
    out.offsets[static_cast<std::size_t>(row) + 1] = out.indices.size();
  }
  return out;
}

// C_c = sum_k (L_k F_k)(L_k F_k)^T on the control space.  When every factor
// is diagonal this is built column by column of the influence, never by
// materialising a latent-sized identity.  A non-diagonal factor falls back to
// the generic factor builder.
Result<Matrix> dense_control_covariance(Context& context, Ref<CovarianceAction> covariance) {
  if (!covariance || !covariance->model) {
    return model::fail<Matrix>(Code::unavailable, "no covariance model supplied",
                                "sky.control_covariance", "the covariance is unknown");
  }
  const NoiseModel& noise = *covariance->model;
  std::vector<Ref<NoiseRoot>> roots;
  roots.insert(roots.end(), noise.innovations.begin(), noise.innovations.end());
  roots.insert(roots.end(), noise.shared_factors.begin(), noise.shared_factors.end());
  if (roots.size() != noise.influence.size() || roots.empty()) {
    return model::fail<Matrix>(Code::unavailable, "the influence count does not match the roots",
                                "sky.control_covariance", "the covariance is unknown");
  }
  U64 rows = 0;
  for (const Ref<LinearAction>& influence : noise.influence) {
    const U64 size = action_codomain_size(influence);
    if (size == 0) {
      return model::fail<Matrix>(Code::unavailable, "an influence has no codomain",
                                  "sky.control_covariance", "the covariance is unknown");
    }
    if (rows == 0) rows = size;
    if (rows != size) {
      return model::fail<Matrix>(Code::unavailable, "the influences disagree on the codomain",
                                  "sky.control_covariance", "the covariance is unknown");
    }
  }
  Matrix out(static_cast<std::size_t>(rows), static_cast<std::size_t>(rows), 0.0);
  for (std::size_t k = 0; k < roots.size(); ++k) {
    const auto* factor_impl =
        dynamic_cast<const operators::ActionImpl*>(roots[k]->factor.get());
    if (!factor_impl || factor_impl->recipe().kind != ActionRecipe::Kind::diagonal) {
      return model::fail<Matrix>(Code::unavailable, "a non-diagonal root factor",
                                  "sky.control_covariance", "the covariance is unknown");
    }
    const std::vector<double>& d = factor_impl->diagonal();
    const U64 latent = d.size();
    if (latent == 0) {
      return model::fail<Matrix>(Code::unavailable, "a noise factor has no domain",
                                  "sky.control_covariance", "the covariance is unknown");
    }
    Result<operators::CsrMatrix> influence =
        to_csr(context, noise.influence[k], rows, latent);
    if (!ok(influence)) {
      return model::fail<Matrix>(Code::unavailable, "an influence cannot be converted",
                                  "sky.control_covariance", "the covariance is unknown");
    }
    std::vector<std::vector<std::pair<U64, double>>> columns(
        static_cast<std::size_t>(latent));
    for (U64 i = 0; i < rows; ++i) {
      for (U64 a = val(influence).offsets[i]; a < val(influence).offsets[i + 1]; ++a) {
        const U64 column = val(influence).indices[a];
        const double value = val(influence).values[a];
        if (value != 0.0) columns[column].emplace_back(i, value);
      }
    }
    for (U64 column = 0; column < latent; ++column) {
      const double scale = d[column] * d[column];
      if (!(scale > 0.0) || columns[column].empty()) continue;
      for (const auto& a : columns[column]) {
        for (const auto& b : columns[column]) {
          out(static_cast<std::size_t>(a.first), static_cast<std::size_t>(b.first)) +=
              scale * a.second * b.second;
        }
      }
    }
  }
  return model::ok_result(std::move(out));
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
  if (!ok(support) || !val(support) || !val(support)->states) {
    // A missing support map is not "every leaf supported": the weights and the
    // control statistics would silently use rows the producer excluded.
    return model::fail<FrameBlock>(ok(support) ? Code::invalid_model : support.code,
                                  "the frame publishes no support states", "build_sky_controls",
                                  "the frame cannot be used");
  }
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
  }
  return model::ok_result(std::move(block));
}

// Builds the joint noise model of the frames on the concatenated leaf space.
//
// Every root influence is placed at the row offset of its own frame, so a root
// of frame 2 never lands on the rows of frame 1.  The innovations stay separate
// and the shared roots are merged by identity, so that the cross frame
// correlation of the control statistics survives:
//   C = sum_k L_k F_k F_k^T L_k^T,  L_k over the concatenated leaf space.
// The unknown terms of the frames are carried into the joint model with the
// same row offset; a frame without a published noise model becomes an explicit
// unknown term instead of a silently noiseless frame.  A joint model with any
// unknown contribution is published as unknown, never as known.
Result<Ref<NoiseModel>> build_joint_noise(Context& context, const std::vector<FrameBlock>& blocks) {
  auto joint = std::make_shared<NoiseModel>();
  std::vector<Ref<NoiseRoot>> innovations;
  std::vector<Ref<LinearAction>> innovation_influences;
  std::map<std::string, std::size_t> shared_index;
  std::vector<Ref<NoiseRoot>> shared_roots;
  std::vector<std::vector<operators::CsrMatrix>> shared_blocks;
  std::vector<std::vector<U64>> shared_offsets;
  std::vector<UnknownTerm> unknown_terms;
  U64 joint_rows = 0;
  for (const FrameBlock& block : blocks) joint_rows += block.leaves.size();
  bool any_unknown = false;
  joint->variance_unit =
      (blocks.empty() || !blocks.front().noise || !blocks.front().noise->model)
          ? model::unit_variance("unknown")
          : blocks.front().noise->model->variance_unit;

  auto make_rows = [&](const operators::CsrMatrix& matrix, U64 columns,
                       const Unit& unit) -> Result<Ref<LinearAction>> {
    return operators::make_csr_action(context, matrix.rows, columns, matrix.offsets,
                                      matrix.indices, matrix.values, Id(), Id(), unit);
  };

  for (const FrameBlock& block : blocks) {
    if (!block.noise || !block.noise->model) {
      // No published noise model is not a zero variance: the rows of this frame
      // carry an unknown contribution and the joint model says so.
      any_unknown = true;
      UnknownTerm term;
      term.id = "frame_noise_missing_" + std::to_string(block.offset);
      term.reason = "the frame publishes no noise model; its variance is unknown, not zero";
      operators::CsrMatrix identity;
      identity.rows = block.leaves.size();
      identity.cols = block.leaves.size();
      identity.offsets.assign(static_cast<std::size_t>(identity.rows) + 1, 0);
      for (U64 i = 0; i < identity.rows; ++i) {
        identity.indices.push_back(i);
        identity.values.push_back(1.0);
        identity.offsets[static_cast<std::size_t>(i) + 1] = identity.indices.size();
      }
      const operators::CsrMatrix placed = sky::shift_rows(identity, block.offset, joint_rows);
      Result<Ref<LinearAction>> action = make_rows(placed, identity.cols, joint->variance_unit);
      if (!ok(action)) {
        return model::fail<Ref<NoiseModel>>(action.code, "an unknown term cannot be built",
                                            "build_sky_controls",
                                            "the joint noise cannot be built");
      }
      term.influence = val(action);
      unknown_terms.push_back(term);
      continue;
    }
    const NoiseModel& noise = *block.noise->model;
    std::vector<Ref<NoiseRoot>> roots;
    roots.insert(roots.end(), noise.innovations.begin(), noise.innovations.end());
    roots.insert(roots.end(), noise.shared_factors.begin(), noise.shared_factors.end());
    if (roots.size() != noise.influence.size()) {
      return model::fail<Ref<NoiseModel>>(Code::invalid_model,
                                          "the influence count does not match the root count",
                                          "build_sky_controls", "the joint noise cannot be built");
    }
    if (noise.state != NoiseState::known) any_unknown = true;
    for (std::size_t k = 0; k < roots.size(); ++k) {
      const U64 columns = sky::action_domain_size(noise.influence[k]);
      Result<operators::CsrMatrix> matrix =
          sky::to_csr(context, noise.influence[k], block.leaves.size(), columns);
      if (!ok(matrix)) {
        return model::fail<Ref<NoiseModel>>(matrix.code, "an influence cannot be converted",
                                            "build_sky_controls", "the joint noise cannot be built");
      }
      if (k < noise.innovations.size()) {
        // The innovation belongs to this frame only: its rows are placed at the
        // frame offset in the concatenated leaf space.
        const operators::CsrMatrix placed = sky::shift_rows(val(matrix), block.offset, joint_rows);
        Result<Ref<LinearAction>> action = make_rows(placed, columns, noise.variance_unit);
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
        shared_offsets.emplace_back();
      }
      shared_blocks[shared_index[key]].push_back(val(matrix));
      shared_offsets[shared_index[key]].push_back(block.offset);
    }
    // The unknown terms of the frame are carried into the joint space.  An
    // unknown term that influences a row makes the complete variance of that
    // row unknown; it is never dropped and never turned into a zero.
    for (const UnknownTerm& term : noise.unknown_terms) {
      if (!term.influence) continue;
      const U64 columns = sky::action_domain_size(term.influence);
      Result<operators::CsrMatrix> matrix =
          sky::to_csr(context, term.influence, block.leaves.size(), columns);
      if (!ok(matrix)) {
        return model::fail<Ref<NoiseModel>>(matrix.code, "an unknown term cannot be converted",
                                            "build_sky_controls",
                                            "the joint noise cannot be built");
      }
      const operators::CsrMatrix placed = sky::shift_rows(val(matrix), block.offset, joint_rows);
      Result<Ref<LinearAction>> action = make_rows(placed, columns, joint->variance_unit);
      if (!ok(action)) {
        return model::fail<Ref<NoiseModel>>(action.code, "an unknown term cannot be built",
                                            "build_sky_controls",
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
    const operators::CsrMatrix merged =
        sky::block_diagonal(shared_blocks[k], shared_offsets[k], joint_rows);
    Result<Ref<LinearAction>> action = make_rows(merged, merged.cols, joint->variance_unit);
    if (!ok(action)) {
      return model::fail<Ref<NoiseModel>>(action.code, "a shared influence cannot be built",
                                          "build_sky_controls", "the joint noise cannot be built");
    }
    joint->influence.push_back(val(action));
  }
  joint->unknown_terms = unknown_terms;
  joint->state = any_unknown ? NoiseState::unknown : NoiseState::known;
  joint->identity = model::make_identity(
      "acsd.NoiseModel", 2, 0,
      model::sha256_hex("sky:joint:" + std::to_string(joint->influence.size()) + ":" +
                        std::to_string(static_cast<int>(joint->state))),
      "acsd");
  joint->assessment = model::make_assessment(Code::ok);
  joint->assessment.complete = joint->state == NoiseState::known;
  joint->assessment.noise = joint->state;
  if (joint->state != NoiseState::known) {
    joint->assessment.science = ScienceState::conditional;
    model::set_omitted(joint->assessment, "known_complete_variance");
  }
  return model::ok_result(Ref<NoiseModel>(std::shared_ptr<const NoiseModel>(joint)));
}

}  // namespace



namespace {

// The named equal-weight fallback of WORKFLOWS: when the covariance closure is
// unknown or cannot be reopened, the control statistic is computed with equal
// weights.  The model is marked unknown; no full IVAR and no known variance is
// claimed.
Result<Ref<CovarianceAction>> equal_weight_covariance(Context& context, U64 count) {
  std::vector<U64> offsets(count + 1, 0);
  std::vector<U64> indices(count, 0);
  std::vector<double> values(count, 1.0);
  for (U64 i = 0; i < count; ++i) {
    offsets[i] = i;
    indices[i] = i;
  }
  offsets[count] = count;
  Result<Ref<LinearAction>> identity =
      operators::make_csr_action(context, count, count, offsets, indices, values, Id(), Id(),
                                 model::unit_dimensionless());
  if (!ok(identity)) {
    return model::fail<Ref<CovarianceAction>>(identity.code,
                                              "the equal-weight control operator cannot be built",
                                              "sky.equal_weights",
                                              "the control covariance cannot be built");
  }
  auto root = std::make_shared<NoiseRoot>();
  root->shared_id.external_namespace = "acsd.controls";
  root->shared_id.entity = "equal_weights";
  root->shared_id.model_revision = "1";
  root->shared_id.component = "amplitude";
  root->unit = model::unit_dimensionless();
  root->factor = val(identity);
  root->assessment = model::make_assessment(Code::ok);
  root->identity = model::make_identity("acsd.NoiseRoot", 2, 0,
                                        model::sha256_hex("equal-weights-controls"), "acsd");
  auto noise = std::make_shared<NoiseModel>();
  noise->variance_unit = model::unit_dimensionless();
  noise->innovations.push_back(root);
  noise->influence.push_back(val(identity));
  noise->state = NoiseState::unknown;
  UnknownTerm term;
  term.id = "equal_weights_unknown_covariance";
  term.reason = "the joint covariance cannot be built from the published noise closure";
  term.influence = val(identity);
  noise->unknown_terms.push_back(term);
  noise->identity = model::make_identity("acsd.NoiseModel", 2, 0,
                                         model::sha256_hex("equal-weights-noise"), "acsd");
  auto covariance = std::make_shared<CovarianceAction>();
  covariance->model = std::shared_ptr<const NoiseModel>(noise);
  covariance->influence = val(identity);
  covariance->input_output.domain = nullptr;
  covariance->input_output.codomain = nullptr;
  covariance->input_output.input = model::unit_dimensionless();
  covariance->input_output.output = model::unit_dimensionless();
  covariance->identity = model::make_identity("acsd.CovarianceAction", 2, 0,
                                              model::sha256_hex("equal-weights-covariance"), "acsd");
  covariance->assessment = model::make_assessment(Code::ok, Capability::supported);
  covariance->assessment.noise = NoiseState::unknown;
  covariance->assessment.science = ScienceState::conditional;
  return model::ok_result(std::shared_ptr<const CovarianceAction>(covariance));
}

}  // namespace
// The concatenated leaf space of the frames: one index over all leaves in the
// order of the blocks (frame major), with the support state of every leaf.  It
// is the space where T acts and where the rows of E live.
Result<Ref<SupportMap>> build_training_support(Context& context,
                                                const std::vector<FrameBlock>& blocks,
                                                U64 leaf_total) {
  std::vector<U64> ids;
  std::vector<std::uint8_t> states;
  std::vector<SourcePartition> partitions;
  ids.reserve(static_cast<std::size_t>(leaf_total) * 2);
  states.reserve(static_cast<std::size_t>(leaf_total));
  U32 order = 0;
  U64 running = 0;
  for (const FrameBlock& block : blocks) {
    if (block.leaves.empty()) continue;
    order = block.order;
    // The row block of the frame inside the training leaf space.  The channel
    // of the partition is the frame identity: it is what lets apply_sky place a
    // single frame's estimate without guessing from the length.
    SourcePartition partition;
    partition.channel = block.frame;
    partition.index_begin = running;
    partition.count = static_cast<U64>(block.leaves.size());
    partitions.push_back(partition);
    running += static_cast<U64>(block.leaves.size());
    for (std::size_t row = 0; row < block.leaves.size(); ++row) {
      ids.push_back(block.leaves[row].order);
      ids.push_back(block.leaves[row].ipix);
      states.push_back(row < block.state.size() ? block.state[row] : 1);
    }
  }
  auto index = std::make_shared<Index>();
  index->size = leaf_total;
  index->kind = Index::Kind::leaf;
  index->order = order;
  index->namespace_id = "sky_training_support";
  Result<ArrayRef> id_array =
      context.storage.copy(context, DType::u64, model::encode_u64(ids),
                           std::vector<U64>{static_cast<U64>(ids.size() / 2), 2},
                           model::unit_dimensionless(), index->namespace_id);
  if (!ok(id_array)) {
    return model::fail<Ref<SupportMap>>(id_array.code, "the training leaf identities cannot be stored",
                                        "build_sky_controls",
                                        "the training support cannot be built");
  }
  index->ids = val(id_array);
  index->partitions = std::move(partitions);
  index->identity = model::make_identity("acsd.Index", 2, 0,
                                          model::hash_index(Index::Kind::leaf, leaf_total, order, {}),
                                          "acsd");
  Result<ArrayRef> state_array =
      context.storage.copy(context, DType::u8, model::encode_u8(states),
                           std::vector<U64>{static_cast<U64>(states.size())},
                           model::unit_dimensionless(), index->namespace_id);
  if (!ok(state_array)) {
    return model::fail<Ref<SupportMap>>(state_array.code, "the training support states cannot be stored",
                                        "build_sky_controls",
                                        "the training support cannot be built");
  }
  auto support = std::make_shared<SupportMap>();
  support->kind = SupportMap::Kind::full_leaf_approx;
  support->index = std::shared_ptr<const Index>(index);
  support->states = val(state_array);
  support->identity = model::make_identity("acsd.SupportMap", 2, 0,
                                            model::hash_index(Index::Kind::leaf, leaf_total, order, {}),
                                            "acsd");
  support->assessment = model::make_assessment(Code::ok);
  support->assessment.complete = true;
  // The support geometry is a deterministic statement, not a noise model.
  support->assessment.noise = NoiseState::known;
  support->assessment.science = ScienceState::bounded;
  return model::ok_result(std::shared_ptr<const SupportMap>(support));
}

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
  if (std::getenv("ACSD_FLOW_DEBUG")) {
    std::fprintf(stderr, "sky: explicit=%d T=%p H=%p groups=%zu\n", explicit_model ? 1 : 0,
                 static_cast<const void*>(request.T.get()), static_cast<const void*>(request.H.get()),
                 request.groups.size());
  }
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
  // One row per frame and patch.  The mask of a row is fixed once and produces
  // both the control value c = T y and the row of T.  Building c from one mask
  // and T from another lets the difference enter the difference model as a fake
  // sky offset (CE of the audit: the mask gives c = 2 while the T row gives 6).
  std::vector<double> control_values;
  std::vector<SkyICRS> control_positions;
  std::vector<U64> component_frame;
  std::vector<std::size_t> control_patch;
  Matrix T;
  Matrix H;
  std::size_t control_count = 0;

  if (!explicit_model) {
    struct ControlRow {
      std::size_t frame = 0;
      std::size_t patch = 0;
      std::vector<std::size_t> mask;  // leaf rows of the frame
      double value = 0.0;
    };
    std::vector<ControlRow> rows;
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
        // The mask of every patch of this frame: the common support of the
        // group and the data quality of this frame.  This is the one mask that
        // the control value and the control operator of the row share.
        std::map<std::size_t, std::vector<std::size_t>> masks;
        for (std::size_t row = 0; row < blocks[f].leaves.size(); ++row) {
          const HealpixLeaf& leaf = blocks[f].leaves[row];
          if (!common[{leaf.order, leaf.ipix}]) continue;
          if (!blocks[f].dq.empty() && (blocks[f].dq[row] & kBadQualityMask) != 0) continue;
          masks[blocks[f].patch_of_leaf[row]].push_back(row);
        }
        for (const auto& entry : masks) {
          if (entry.second.empty()) continue;
          ControlRow row;
          row.frame = f;
          row.patch = entry.first;
          row.mask = entry.second;
          double sum = 0.0;
          for (std::size_t leaf_row : row.mask) sum += blocks[f].signal[leaf_row];
          row.value = sum / static_cast<double>(row.mask.size());
          rows.push_back(std::move(row));
        }
      }
    }
    if (rows.empty()) {
      return model::fail<Ref<SkyControls>>(
          Code::no_support, "no control survives the support and the quality mask",
          "build_sky_controls", "no control can be built");
    }
    control_count = rows.size();
    T = Matrix(static_cast<std::size_t>(control_count), static_cast<std::size_t>(leaf_total), 0.0);
    H = Matrix(static_cast<std::size_t>(control_count),
               static_cast<std::size_t>(patches.size()), 0.0);
    for (std::size_t row = 0; row < rows.size(); ++row) {
      const ControlRow& control = rows[row];
      const double weight = 1.0 / static_cast<double>(control.mask.size());
      for (std::size_t leaf_row : control.mask) {
        T(row, static_cast<std::size_t>(blocks[control.frame].offset + leaf_row)) = weight;
      }
      H(row, control.patch) = 1.0;
      control_values.push_back(control.value);
      control_positions.push_back(patches[control.patch].center);
      component_frame.push_back(static_cast<U64>(control.frame));
      control_patch.push_back(control.patch);
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
      if (std::getenv("ACSD_FLOW_DEBUG")) {
        std::fprintf(stderr, "sky: dense_action rows=%zu cols=%zu code=%d reason=%s\n", rows, cols,
                     static_cast<int>(dense_t.code),
                     dense_t.assessment.issues.empty() ? "-" : dense_t.assessment.issues.front().reason.c_str());
      }
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
      // A row that spans more than one frame has no single component identity,
      // and the component of a control row is what the gauge and the connected
      // components of the fit are built from; such a row is refused by name
      // instead of being assigned to the largest entry.
      double sx = 0.0;
      double sy = 0.0;
      double sz = 0.0;
      std::size_t owner = 0;
      bool owner_known = false;
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
        if (!owner_known) {
          owner = frame_index;
          owner_known = true;
        } else if (owner != frame_index) {
          return model::fail<Ref<SkyControls>>(
              Code::unsupported,
              "a control row of the explicit operator spans more than one frame; the component "
              "identity of the row and the per frame gauge are not defined",
              "build_sky_controls", "no control can be built");
        }
        const SkyICRS position = sky::local_leaf_center(
            blocks[frame_index].order, blocks[frame_index].leaves[leaf_index].ipix);
        sx += weight * position.nx;
        sy += weight * position.ny;
        sz += weight * position.nz;
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
  const std::size_t terms = sky::basis_term_count(request.basis);
  const std::size_t parameters = blocks.size() * terms;
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
  // The gnomonic chart is only valid inside a hemisphere, so a chart radius of
  // pi is not a radius at all: project_to_chart refuses every direction with
  // cos <= 1e-9 and evaluate_basis then returns an empty row, which used to
  // leave the constant column of A and E at zero without an error.  The radius
  // is now the largest angle that the controls and the output leaves actually
  // span, and a set that does not fit into one chart is rejected by name.
  auto angle_from_center = [&](const SkyICRS& position) {
    const double dot = position.nx * chart_center.nx + position.ny * chart_center.ny +
                       position.nz * chart_center.nz;
    return std::acos(std::max(-1.0, std::min(1.0, dot)));
  };
  constexpr double kMaxChartRadius = 1.4;  // rad, about 80 degrees
  double chart_radius = 0.0;
  for (const SkyICRS& position : control_positions) {
    chart_radius = std::max(chart_radius, angle_from_center(position));
  }
  for (std::size_t f = 0; f < blocks.size(); ++f) {
    for (std::size_t leaf_row = 0; leaf_row < blocks[f].leaves.size(); ++leaf_row) {
      const SkyICRS position =
          sky::local_leaf_center(blocks[f].order, blocks[f].leaves[leaf_row].ipix);
      chart_radius = std::max(chart_radius, angle_from_center(position));
    }
  }
  // The constant basis is one everywhere and needs no chart at all.
  const bool needs_chart = terms > 1;
  if (needs_chart && chart_radius > kMaxChartRadius) {
    return model::fail<Ref<SkyControls>>(
        Code::unsupported,
        "the controls span more than one gnomonic chart; the basis cannot be evaluated with one "
        "chart centre and the control rows would silently lose their spatial columns",
        "build_sky_controls", "no control can be built");
  }
  sky::ChartProjection projection;
  if (!sky::make_chart_projection(chart_center, needs_chart ? chart_radius : 0.0, projection)) {
    return model::fail<Ref<SkyControls>>(Code::invalid_model, "the chart cannot be built",
                                         "build_sky_controls", "no control can be built");
  }
  Matrix E(static_cast<std::size_t>(leaf_total), static_cast<std::size_t>(parameters), 0.0);
  for (std::size_t f = 0; f < blocks.size(); ++f) {
    for (std::size_t leaf_row = 0; leaf_row < blocks[f].leaves.size(); ++leaf_row) {
      const SkyICRS position =
          sky::local_leaf_center(blocks[f].order, blocks[f].leaves[leaf_row].ipix);
      const std::vector<double> values = sky::evaluate_basis(request.basis, projection, position);
      if (values.size() < terms) {
        return model::fail<Ref<SkyControls>>(
            Code::invalid_model,
            "the basis cannot be evaluated at a leaf position; the output model would lose a "
            "column without an error",
            "build_sky_controls", "no control can be built");
      }
      for (std::size_t term = 0; term < terms; ++term) {
        E(static_cast<std::size_t>(blocks[f].offset + leaf_row),
          static_cast<std::size_t>(f * terms + term)) = values[term];
      }
    }
  }
  // A = T E, not a second evaluation at another position: the difference model
  // of a control row is the control operator applied to the output model of the
  // leaves.  Evaluating the basis at the patch centre instead gives A = (1,0)
  // while T E = (1,1) for a mask that only keeps x = +1, and a true a = 0, b = 1
  // is then fitted as a fake offset beta = 1.
  if (E.rows != T.cols) {
    return model::fail<Ref<SkyControls>>(Code::invalid_model,
                                         "the output model does not match the control operator",
                                         "build_sky_controls", "no control can be built");
  }
  const Matrix A = numerics::matmul(T, E);
  if (A.rows != static_cast<std::size_t>(control_count) || A.cols != parameters) {
    return model::fail<Ref<SkyControls>>(Code::invalid_model,
                                         "the difference model does not match the controls",
                                         "build_sky_controls", "no control can be built");
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
  bool equal_weights = false;
  std::string covariance_failure_reason;
  if (!control_noise) {
    Result<Ref<NoiseModel>> joint = build_joint_noise(context, blocks);
    Code failure = Code::ok;
    std::string failure_reason;
    Result<Ref<CovarianceAction>> control_covariance_result;
    if (!ok(joint)) {
      failure = joint.code;
      failure_reason = joint.assessment.issues.empty() ? "the joint noise cannot be built"
                                                       : joint.assessment.issues.front().reason;
    } else {
      Result<Ref<CovarianceAction>> joint_covariance = make_covariance(context, val(joint));
      if (!ok(joint_covariance)) {
        failure = joint_covariance.code;
        failure_reason = joint_covariance.assessment.issues.empty()
                             ? "the joint covariance cannot be built"
                             : joint_covariance.assessment.issues.front().reason;
      } else {
        Result<Ref<LinearAction>> t_action =
            sky::dense_to_action(context, T, Id(), Id(), model::unit_dimensionless());
        if (!ok(t_action)) {
          failure = t_action.code;
          failure_reason = "the control operator cannot be built";
        } else {
          control_covariance_result =
              control_covariance(context, val(joint_covariance), val(t_action));
          if (!ok(control_covariance_result)) {
            failure = control_covariance_result.code;
            failure_reason = control_covariance_result.assessment.issues.empty()
                                 ? "the control covariance cannot be built"
                                 : control_covariance_result.assessment.issues.front().reason;
          }
        }
      }
    }
    // A control covariance with no known part at all (every frame publishes an
    // unknown or an absent noise model) defines no whitening and no GLS: the
    // declared equal weights are used instead, named, with an unknown state.
    if (ok(control_covariance_result) && val(control_covariance_result) &&
        val(control_covariance_result)->model &&
        val(control_covariance_result)->model->innovations.empty() &&
        val(control_covariance_result)->model->shared_factors.empty()) {
      failure = Code::unavailable;
      failure_reason =
          "the control covariance has no known part: the published noise of the frames carries no "
          "known covariance root";
      control_covariance_result =
          model::fail<Ref<CovarianceAction>>(Code::unavailable, failure_reason,
                                             "build_sky_controls",
                                             "the control covariance is unknown");
    }
    if (ok(control_covariance_result)) {
      control_noise = val(control_covariance_result);
    } else {
      // A malformed model is an input error and is never turned into a
      // scientific fallback: only a covariance that is unavailable because the
      // closure is unknown degrades to the declared equal weights, and that
      // degradation is named, carries its reason and publishes unknown noise.
      const bool malformed = failure == Code::invalid_model ||
                             failure == Code::identity_conflict ||
                             failure == Code::invalid_request ||
                             failure == Code::invalid_covariance;
      if (malformed) {
        return model::fail<Ref<SkyControls>>(failure, failure_reason, "build_sky_controls",
                                             "the control covariance cannot be built");
      }
      Result<Ref<CovarianceAction>> equal =
          equal_weight_covariance(context, static_cast<U64>(control_count));
      if (!ok(equal)) {
        return model::fail<Ref<SkyControls>>(equal.code,
                                             "the equal-weight control covariance cannot be built",
                                             "build_sky_controls", "no control can be built");
      }
      control_noise = val(equal);
      equal_weights = true;
      covariance_failure_reason = failure_reason.empty()
                                      ? "the joint control covariance is not available"
                                      : failure_reason;
    }
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
    if (std::getenv("ACSD_FLOW_DEBUG")) {
      std::fprintf(stderr, "sky: E dims=%zux%zu code=%d reason=%s\n", E.rows, E.cols,
                   static_cast<int>(e_ref.code),
                   e_ref.assessment.issues.empty() ? "-" : e_ref.assessment.issues.front().reason.c_str());
    }
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

  // The training support of the controls is the concatenated leaf space of the
  // frames, in the same order as the columns of T and the rows of E.  The fit
  // carries it to the SkyFit, where it is what lets apply_sky place a single
  // frame's signal inside the joint leaf space by leaf identity instead of by
  // guessing from the length.
  Result<Ref<SupportMap>> training_support = build_training_support(context, blocks, leaf_total);
  if (!ok(training_support)) {
    return model::fail<Ref<SkyControls>>(training_support.code,
                                         "the training support cannot be built",
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
  controls->training_support = val(training_support);
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
  if (equal_weights) {
    controls->assessment.noise = NoiseState::unknown;
    controls->assessment.science = ScienceState::conditional;
    out.assessment.noise = NoiseState::unknown;
    out.assessment.science = ScienceState::conditional;
    model::set_omitted(out.assessment, "known_control_covariance");
    model::set_included(out.assessment, "equal_weights_unknown_covariance");
    model::add_issue(out.assessment, Code::unavailable,
                     "the joint control covariance is not available, the equal weights are used "
                     "and no known control variance is claimed: " + covariance_failure_reason,
                     "build_sky_controls", "the control covariance is unknown");
  } else {
    model::set_included(out.assessment, "control_covariance_from_the_shared_roots");
    // A declared covariance whose model carries an unknown term is not a known
    // covariance: the controls publish the state of the model they were built
    // from and never promote it to known.
    if (control_noise && control_noise->model &&
        control_noise->model->state != NoiseState::known) {
      controls->assessment.noise = NoiseState::unknown;
      controls->assessment.science = ScienceState::conditional;
      out.assessment.noise = NoiseState::unknown;
      out.assessment.science = ScienceState::conditional;
      model::set_omitted(out.assessment, "known_control_covariance");
      model::add_issue(out.assessment, Code::unavailable,
                       "the control covariance model carries an unknown contribution; the "
                       "complete control variance is unknown",
                       "build_sky_controls", "the control covariance is unknown");
    }
  }
  model::set_included(out.assessment, "basis_chart_centre_is_the_mean_control_direction");
  model::set_omitted(out.assessment, "sky_prediction_outside_the_training_support");
  return out;
}

// ---------------------------------------------------------------------------
// fit_relative_sky
// ---------------------------------------------------------------------------

namespace {

// Orthonormal completion: the first k columns of the result span the column
// space of the orthonormal columns of v, the remaining columns span the
// orthogonal complement.
//
// The previous implementation returned the transpose of the Householder product
// that triangularises v.  That is the complement only when v has a single
// column: for k >= 2 its columns are a rotation of the input and its trailing
// columns are not orthogonal to the column space, so a projector built from it
// (Q_w for the common sky, the null space of the design) was not the projector
// it claimed to be.  The completion is formed here by a twice orthogonalised
// Gram-Schmidt sweep over the standard basis.
Matrix complete_orthogonal_factorisation(const Matrix& v) {
  const std::size_t n = v.rows;
  const std::size_t k = v.cols;
  Matrix h(n, n, 0.0);
  if (k > n) return h;
  for (std::size_t j = 0; j < k; ++j) {
    for (std::size_t i = 0; i < n; ++i) h(i, j) = v(i, j);
  }
  std::size_t filled = k;
  std::vector<double> candidate(n, 0.0);
  for (std::size_t basis = 0; basis < n && filled < n; ++basis) {
    std::fill(candidate.begin(), candidate.end(), 0.0);
    candidate[basis] = 1.0;
    // Two passes of classical Gram-Schmidt: the second pass removes the
    // component that the first one lost to rounding.
    for (int pass = 0; pass < 2; ++pass) {
      for (std::size_t j = 0; j < filled; ++j) {
        double dot = 0.0;
        for (std::size_t i = 0; i < n; ++i) dot += candidate[i] * h(i, j);
        if (dot == 0.0) continue;
        for (std::size_t i = 0; i < n; ++i) candidate[i] -= dot * h(i, j);
      }
    }
    double norm = 0.0;
    for (double value : candidate) norm += value * value;
    norm = std::sqrt(norm);
    if (!(norm > 1e-12)) continue;
    for (std::size_t i = 0; i < n; ++i) h(i, filled) = candidate[i] / norm;
    ++filled;
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
  // The control operator acts on the joint training leaf space: T is
  // (control_count x leaf_total), e.g. 108 x 71368 for the three-frame 4096
  // mosaic, while every matrix the estimator itself touches is at most
  // control_count wide (the gauge, the whitening and the estimators are
  // control-space objects).  Materialising T was dead work: the dense copy was
  // never read after the shape check, and at 4096 scale the dense cap refused
  // the shape although the fit needs no leaf-space matrix at all.  The
  // operator is therefore validated through its sparse payload, which is
  // exactly what the published correction action P = I - E K T applies.
  {
    Result<operators::CsrMatrix> t_sparse =
        sky::to_csr(context, controls->T, t_rows, t_cols);
    if (!ok(t_sparse)) {
      return model::fail<Ref<SkyFit>>(t_sparse.code, "the control operator cannot be applied",
                                      "fit_relative_sky", "the sky cannot be fitted");
    }
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
  // The state of the declared control covariance is checked before a SkyFit is
  // published.  A model that carries an unknown contribution (or is marked
  // unknown, for example because the controls were built with the declared
  // equal weights) still defines the declared point estimator, but its
  // parameter covariance is not a measured variance: the fit publishes the
  // unknown state and never reports a known variance for it.
  bool control_covariance_unknown = false;
  std::string control_covariance_reason;
  if (controls->noise->model) {
    if (controls->noise->model->state != NoiseState::known) {
      control_covariance_unknown = true;
      control_covariance_reason =
          "the control covariance model is marked " +
          std::string(controls->noise->model->state == NoiseState::unknown ? "unknown"
                                                                          : "not known");
    }
    for (const UnknownTerm& term : controls->noise->model->unknown_terms) {
      if (!term.influence) continue;
      control_covariance_unknown = true;
      control_covariance_reason = term.reason.empty()
                                      ? "an unknown term of the control covariance influences the fit"
                                      : term.reason;
    }
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
  Result<Matrix> factor = sky::dense_control_covariance(context, controls->noise);
  if (ok(factor)) {
    // Factorise C_c = U diag(lambda) U^T into B = U diag(sqrt(lambda)); the
    // existing SVD whitening then sees exactly the control covariance without
    // ever forming a latent-sized identity.
    numerics::SvdResult eigen = numerics::jacobi_svd(val(factor), 0.0);
    Matrix rebuilt(static_cast<std::size_t>(control_count),
                   static_cast<std::size_t>(control_count), 0.0);
    for (std::size_t j = 0; j < control_count; ++j) {
      const double sigma = std::sqrt(std::max(0.0, eigen.singular_values[j]));
      for (std::size_t i = 0; i < control_count; ++i) {
        rebuilt(i, j) = eigen.u(i, j) * sigma;
      }
    }
    factor = model::ok_result(std::move(rebuilt));
  } else {
    factor = sky::dense_covariance_factor(context, controls->noise);
  }
  if (!ok(factor)) {
    return model::fail<Ref<SkyFit>>(factor.code, "the control covariance factor cannot be built",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }
  if (val(factor).rows != control_count) {
    return model::fail<Ref<SkyFit>>(Code::invalid_model,
                                    "the control covariance does not match the controls",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }
  const double rank_relative = request.solve.rank_relative > 0.0 ? request.solve.rank_relative : 1e-12;
  numerics::SvdResult factor_svd = numerics::jacobi_svd(val(factor), rank_relative);
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

  // The exact zero variance directions of a PSD control covariance.  They are
  // noiseless equations U0^T c = U0^T [H, A G] eta, not infinitely uncertain
  // ones: the whitening W only covers the range of C_c, so solving with W alone
  // drops them (CE-E: the point estimate is 0 instead of 1 and a known zero
  // variance becomes 1).  They are solved first, as hard equations, and the GLS
  // is then restricted to their solution set.  No epsilon floor is added to the
  // covariance and the discarded directions are not re-weighted.
  const std::size_t exact_count =
      control_count > factor_svd.rank ? control_count - factor_svd.rank : 0;
  Matrix U0(static_cast<std::size_t>(control_count), exact_count, 0.0);
  if (exact_count > 0) {
    // The range of the factor, completed to an orthonormal basis of the whole
    // control space: the columns past the rank span the exact null space.  The
    // completion is taken from the range columns only: the zero singular value
    // columns of the SVD are already orthogonal to the range, but a square
    // left factor has no spare column and completing the full left factor would
    // rotate the null directions away.
    Matrix range_basis(static_cast<std::size_t>(control_count),
                       static_cast<std::size_t>(factor_svd.rank), 0.0);
    for (std::size_t j = 0; j < factor_svd.rank; ++j) {
      for (std::size_t i = 0; i < control_count; ++i) {
        range_basis(static_cast<std::size_t>(i), j) =
            factor_svd.u(static_cast<std::size_t>(i), j);
      }
    }
    const Matrix completed = complete_orthogonal_factorisation(range_basis);
    for (std::size_t i = 0; i < control_count; ++i) {
      for (std::size_t j = 0; j < exact_count; ++j) {
        U0(static_cast<std::size_t>(i), j) =
            completed(static_cast<std::size_t>(i), factor_svd.rank + j);
      }
    }
  }

  // Q_w is the orthogonal complement of the column space of W H.
  Result<Matrix> whitened_h = multiply(W, val(H));
  if (!ok(whitened_h)) {
    return model::fail<Ref<SkyFit>>(whitened_h.code, "the whitened common sky cannot be formed",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }
  numerics::SvdResult h_svd = numerics::jacobi_svd(val(whitened_h), rank_relative);
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
  const Matrix ag = numerics::matmul(val(A), G);
  Result<Matrix> whitened_design = multiply(W, ag);
  if (!ok(whitened_design)) {
    return model::fail<Ref<SkyFit>>(whitened_design.code, "the whitened design cannot be formed",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }
  Result<Matrix> projected_design = multiply(qw_transpose, val(whitened_design));
  if (!ok(projected_design)) {
    return model::fail<Ref<SkyFit>>(projected_design.code, "the projected design cannot be formed",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }
  const Matrix design_matrix = val(projected_design);

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
  std::vector<double> projected_data(complement_size, 0.0);
  for (std::size_t i = 0; i < complement_size; ++i) {
    projected_data[i] = val(whitened_data)(static_cast<std::size_t>(i), 0);
  }

  // The numerical rank at one threshold, so that the pseudo inverses, the null
  // spaces and the rank report all use the same classification.  The default
  // threshold is relative to the largest singular value of the same matrix;
  // the exact equations use an absolute threshold on the scale of the problem
  // instead, because a matrix of rounding noise would otherwise always be
  // classified as rank one (a 1e-16 value is "larger than 1e-12 times itself").
  auto rank_at = [&](const numerics::SvdResult& svd, double threshold) {
    std::size_t rank = 0;
    for (double value : svd.singular_values) {
      if (value > threshold) ++rank;
    }
    return rank;
  };
  auto numerical_rank = [&](const numerics::SvdResult& svd) {
    const double largest = svd.singular_values.empty() ? 0.0 : svd.singular_values.front();
    return rank_at(svd, rank_relative * largest);
  };
  auto pseudo_inverse_at = [&](const numerics::SvdResult& svd, std::size_t columns,
                               double threshold) {
    Matrix out(columns, svd.u.rows, 0.0);
    for (std::size_t i = 0; i < svd.singular_values.size(); ++i) {
      const double sigma = svd.singular_values[i];
      if (!(sigma > threshold) || sigma <= 0.0) continue;
      const double reciprocal = 1.0 / sigma;
      for (std::size_t r = 0; r < svd.v.rows; ++r) {
        const double vr = svd.v(r, i);
        if (vr == 0.0) continue;
        for (std::size_t c = 0; c < svd.u.rows; ++c) out(r, c) += vr * svd.u(c, i) * reciprocal;
      }
    }
    return out;
  };

  // ---- the exact equations of the zero variance directions -----------------
  Matrix constraint_matrix(0, static_cast<std::size_t>(gauge_size), 0.0);
  Matrix constraint_data(0, 1, 0.0);
  std::size_t constraint_rank = 0;
  Matrix constraint_pinv(static_cast<std::size_t>(gauge_size), 0, 0.0);
  Matrix constraint_eta(static_cast<std::size_t>(gauge_size), 1, 0.0);
  Matrix constraint_null(static_cast<std::size_t>(gauge_size), static_cast<std::size_t>(gauge_size), 0.0);
  std::size_t constraint_null_columns = static_cast<std::size_t>(gauge_size);
  if (exact_count > 0) {
    // N_s = U0^T H: the part of the noiseless equation that the free common sky
    // can satisfy without constraining the difference model.  The rank of the
    // noiseless block is judged on the scale of the model, not on its own
    // magnitude: U0^T H is a difference of entries of H and is zero up to
    // rounding when the exact direction is orthogonal to the common sky.
    const double model_scale =
        std::max(1.0, std::max(numerics::frobenius_norm(val(H)), numerics::frobenius_norm(ag)));
    const double exact_threshold = rank_relative * model_scale;
    const Matrix u0_transposed = numerics::transpose(U0);
    const Matrix n_s = numerics::matmul(u0_transposed, val(H));
    Matrix p0 = numerics::identity_matrix(exact_count);
    if (n_s.cols > 0) {
      numerics::SvdResult ns_svd = numerics::jacobi_svd(n_s, rank_relative);
      const std::size_t ns_rank = rank_at(ns_svd, exact_threshold);
      if (ns_rank > 0) {
        const Matrix ns_pinv = pseudo_inverse_at(ns_svd, n_s.cols, exact_threshold);
        const Matrix projection = numerics::matmul(n_s, ns_pinv);
        for (std::size_t i = 0; i < exact_count; ++i) {
          for (std::size_t j = 0; j < exact_count; ++j) {
            p0(static_cast<std::size_t>(i), static_cast<std::size_t>(j)) -= projection(i, j);
          }
        }
      }
    }
    const Matrix n_eta = numerics::matmul(u0_transposed, ag);
    const Matrix b0 = numerics::matmul(u0_transposed, c_column);
    constraint_matrix = numerics::matmul(p0, n_eta);
    constraint_data = numerics::matmul(p0, b0);
    numerics::SvdResult constraint_svd = numerics::jacobi_svd(constraint_matrix, rank_relative);
    constraint_rank = rank_at(constraint_svd, exact_threshold);
    constraint_pinv = pseudo_inverse_at(constraint_svd, static_cast<std::size_t>(gauge_size),
                                        exact_threshold);
    constraint_eta = numerics::matmul(constraint_pinv, constraint_data);
    // The noiseless equations must be consistent with the model.  A residual
    // that is not numerical noise means the declared covariance says a
    // direction has exactly zero variance while the data contradict it: the
    // model is invalid, not merely imprecise.
    const Matrix residual =
        numerics::matmul(constraint_matrix, constraint_eta);
    double residual_norm = 0.0;
    double data_norm = 0.0;
    for (std::size_t i = 0; i < exact_count; ++i) {
      const double difference = constraint_data(i, 0) - residual(i, 0);
      residual_norm = std::max(residual_norm, std::fabs(difference));
      data_norm = std::max(data_norm, std::fabs(constraint_data(i, 0)));
    }
    const double consistency_tolerance = 1e-8 * std::max(1.0, data_norm);
    if (!(residual_norm <= consistency_tolerance)) {
      return model::fail<Ref<SkyFit>>(
          Code::invalid_model,
          "the zero variance directions of the control covariance are inconsistent with the "
          "declared model; the noiseless equations have no solution",
          "fit_relative_sky", "the sky cannot be fitted");
    }
    // The null space of the exact equations: the directions of the gauge that
    // remain free after the noiseless equations are satisfied.
    constraint_null_columns = gauge_size - constraint_rank;
    if (constraint_null_columns > 0) {
      numerics::SvdResult transposed =
          numerics::jacobi_svd(numerics::transpose(constraint_matrix), rank_relative);
      Matrix range(static_cast<std::size_t>(gauge_size),
                   static_cast<std::size_t>(constraint_rank), 0.0);
      for (std::size_t j = 0; j < constraint_rank; ++j) {
        for (std::size_t i = 0; i < gauge_size; ++i) {
          range(static_cast<std::size_t>(i), j) =
              transposed.u(static_cast<std::size_t>(i), j);
        }
      }
      const Matrix completed = complete_orthogonal_factorisation(range);
      for (std::size_t i = 0; i < gauge_size; ++i) {
        for (std::size_t j = 0; j < constraint_null_columns; ++j) {
          constraint_null(static_cast<std::size_t>(i), j) =
              completed(static_cast<std::size_t>(i), constraint_rank + j);
        }
      }
    }
  } else {
    // No exact direction: every gauge direction is free.
    constraint_null = numerics::identity_matrix(static_cast<std::size_t>(gauge_size));
  }

  // ---- the constrained GLS on the range of the covariance ------------------
  Matrix Z(static_cast<std::size_t>(gauge_size), constraint_null_columns, 0.0);
  for (std::size_t i = 0; i < gauge_size; ++i) {
    for (std::size_t j = 0; j < constraint_null_columns; ++j) {
      Z(static_cast<std::size_t>(i), static_cast<std::size_t>(j)) = constraint_null(i, j);
    }
  }
  const Matrix design_z = numerics::matmul(design_matrix, Z);
  numerics::SvdResult design_z_svd = numerics::jacobi_svd(design_z, rank_relative);
  const Matrix design_z_pinv =
      numerics::pseudo_inverse(design_z_svd, constraint_null_columns, rank_relative);
  Matrix residual_data(complement_size, 1, 0.0);
  {
    const Matrix fitted = numerics::matmul(design_matrix, constraint_eta);
    for (std::size_t i = 0; i < complement_size; ++i) {
      residual_data(i, 0) = projected_data[i] - fitted(i, 0);
    }
  }
  const Matrix alpha = numerics::matmul(design_z_pinv, residual_data);
  const Matrix eta = [&]() {
    const Matrix shift = numerics::matmul(Z, alpha);
    Matrix out = constraint_eta;
    for (std::size_t i = 0; i < gauge_size; ++i) out(i, 0) += shift(i, 0);
    return out;
  }();

  // ---- the estimator K: beta = K c ---------------------------------------
  // K = G [ C^+ P0 U0^T + Z (D Z)^+ ( Q_w^T W - D C^+ P0 U0^T ) ].
  Result<Matrix> projector = multiply(qw_transpose, W);
  if (!ok(projector)) {
    return model::fail<Ref<SkyFit>>(projector.code, "the whitened projector cannot be formed",
                                    "fit_relative_sky", "the sky cannot be fitted");
  }
  Matrix exact_estimator(static_cast<std::size_t>(gauge_size),
                         static_cast<std::size_t>(control_count), 0.0);
  if (exact_count > 0) {
    const Matrix u0_transposed = numerics::transpose(U0);
    exact_estimator = numerics::matmul(constraint_pinv, u0_transposed);
  }
  const Matrix design_exact = numerics::matmul(design_matrix, exact_estimator);
  Matrix range_estimator(complement_size, static_cast<std::size_t>(control_count), 0.0);
  for (std::size_t i = 0; i < complement_size; ++i) {
    for (std::size_t j = 0; j < control_count; ++j) {
      range_estimator(i, j) = val(projector)(i, j) - design_exact(i, j);
    }
  }
  const Matrix alpha_estimator = numerics::matmul(design_z_pinv, range_estimator);
  const Matrix shift_estimator = numerics::matmul(Z, alpha_estimator);
  Matrix gauge_estimator(static_cast<std::size_t>(gauge_size),
                         static_cast<std::size_t>(control_count), 0.0);
  for (std::size_t i = 0; i < gauge_size; ++i) {
    for (std::size_t j = 0; j < control_count; ++j) {
      gauge_estimator(i, j) = exact_estimator(i, j) + shift_estimator(i, j);
    }
  }
  Result<Matrix> k_matrix = multiply(G, gauge_estimator);
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

  // The directions that neither the whitened data nor the exact equations
  // determine.  They are the null space of the stacked system [D; C], mapped
  // back to the parameter space through the gauge.
  const std::size_t stacked_rows = complement_size + exact_count;
  Matrix stacked(stacked_rows, static_cast<std::size_t>(gauge_size), 0.0);
  for (std::size_t i = 0; i < complement_size; ++i) {
    for (std::size_t j = 0; j < gauge_size; ++j) {
      stacked(i, static_cast<std::size_t>(j)) = design_matrix(i, static_cast<std::size_t>(j));
    }
  }
  for (std::size_t i = 0; i < exact_count; ++i) {
    for (std::size_t j = 0; j < gauge_size; ++j) {
      stacked(complement_size + i, static_cast<std::size_t>(j)) =
          constraint_matrix(i, static_cast<std::size_t>(j));
    }
  }
  numerics::SvdResult stacked_svd = numerics::jacobi_svd(stacked, rank_relative);
  // The stacked system mixes the whitened design with the exact equations; its
  // rank is judged on the scale of both blocks so that a rounding sized
  // constraint row does not count as an equation.
  const double stacked_scale =
      std::max({1.0, numerics::frobenius_norm(design_matrix),
                exact_count > 0 ? numerics::frobenius_norm(constraint_matrix) : 0.0,
                numerics::frobenius_norm(val(H)), numerics::frobenius_norm(ag)});
  const std::size_t stacked_rank = rank_at(stacked_svd, rank_relative * stacked_scale);
  const std::size_t null_count = gauge_size > stacked_rank ? gauge_size - stacked_rank : 0;
  Matrix null_parameters(static_cast<std::size_t>(parameters), null_count, 0.0);
  if (null_count > 0) {
    numerics::SvdResult transposed =
        numerics::jacobi_svd(numerics::transpose(stacked), rank_relative);
    Matrix range(static_cast<std::size_t>(gauge_size), stacked_rank, 0.0);
    for (std::size_t j = 0; j < stacked_rank; ++j) {
      for (std::size_t i = 0; i < gauge_size; ++i) {
        range(static_cast<std::size_t>(i), j) = transposed.u(static_cast<std::size_t>(i), j);
      }
    }
    const Matrix completed = complete_orthogonal_factorisation(range);
    for (std::size_t j = 0; j < null_count; ++j) {
      for (std::size_t p = 0; p < parameters; ++p) {
        double sum = 0.0;
        for (std::size_t i = 0; i < gauge_size; ++i) {
          sum += G(static_cast<std::size_t>(p), static_cast<std::size_t>(i)) *
                 completed(static_cast<std::size_t>(i), stacked_rank + j);
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
      // The parameter covariance is K C_c K^T.  The influences of the roots are
      // K L_k, formed as an exact sparse product: the composition helper of the
      // operators module applies the transpose of a composition in the wrong
      // order, so a covariance built through it would answer quadratic_form
      // with (K L)^T instead of (K L).
      Result<operators::CsrMatrix> product = sky::multiply_csr(context, val(k_action), influence);
      if (!ok(product)) {
        return model::fail<Ref<SkyFit>>(product.code, "the parameter covariance cannot be built",
                                        "fit_relative_sky", "the sky cannot be fitted");
      }
      Result<Ref<LinearAction>> action = operators::make_csr_action(
          context, val(product).rows, val(product).cols, val(product).offsets,
          val(product).indices, val(product).values, Id(), Id(), noise_model->variance_unit);
      if (!ok(action)) {
        return model::fail<Ref<SkyFit>>(action.code, "the parameter covariance cannot be built",
                                        "fit_relative_sky", "the sky cannot be fitted");
      }
      noise_model->influence.push_back(val(action));
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
    noise_model->identity = model::make_identity(
        "acsd.NoiseModel", 2, 0,
        model::sha256_hex("sky:parameter:" + std::to_string(noise_model->influence.size()) + ":" +
                          std::to_string(static_cast<int>(noise_model->state))),
        "acsd");
    noise_model->assessment = model::make_assessment(Code::ok);
    noise_model->assessment.complete =
        noise_model->state == NoiseState::known && noise_model->unknown_terms.empty();
    if (!noise_model->assessment.complete) {
      noise_model->assessment.science = ScienceState::conditional;
      model::set_omitted(noise_model->assessment, "known_complete_variance");
    }
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
  // The rank of the stacked system [D; C]: the whitened data and the exact
  // equations together decide which gauge directions are measured.
  rank.rank = stacked_rank;
  rank.singular_error_bound = stacked_svd.sigma_error_bound;
  rank.certified = (stacked_rank == gauge_size) && (null_count == 0);
  {
    Result<ArrayRef> singular = operators::write_f64(
        context, stacked_svd.singular_values,
        std::vector<U64>{static_cast<U64>(stacked_svd.singular_values.size())},
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
    if (!flat.empty()) {
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
  if (exact_count > 0) {
    model::set_included(out.assessment, "exact_zero_variance_control_constraints");
  }
  if (control_covariance_unknown) {
    model::set_omitted(out.assessment, "known_control_covariance");
    model::set_included(out.assessment, "declared_nominal_control_covariance");
    fit->assessment.noise = NoiseState::unknown;
    fit->assessment.science = ScienceState::conditional;
    fit->assessment.complete = false;
    model::set_omitted(fit->assessment, "known_control_covariance");
    out.assessment.noise = NoiseState::unknown;
    model::add_issue(out.assessment, Code::unavailable,
                     "the control covariance is not known; the published parameter covariance is "
                     "not a measured variance: " + control_covariance_reason,
                     "fit_relative_sky", "the sky variance is unknown");
    out.assessment.science = ScienceState::conditional;
  }
  if (null_parameters.cols > 0) {
    model::set_omitted(out.assessment, "not_estimable_sky_directions");
    fit->assessment.noise = NoiseState::unknown;
    fit->assessment.science = ScienceState::conditional;
    fit->assessment.complete = false;
    model::set_omitted(fit->assessment, "not_estimable_sky_directions");
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

// ---------------------------------------------------------------------------
// apply_sky
// ---------------------------------------------------------------------------

namespace {

// The shared body of the two public entries: with a frame identity the block is
// taken from the per frame partitions of the training support, without one the
// block must be uniquely determined by the leaf identities.
Result<Ref<SignalEstimate>> apply_sky_impl(Context& context, Ref<SignalEstimate> signal,
                                            Ref<SkyFit> fit, const Id* frame) {
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
  if (!fit->E || !fit->K || !fit->T) {
    return model::fail<Ref<SignalEstimate>>(Code::invalid_model,
                                            "the correction action is incomplete", "apply_sky",
                                            "the correction cannot be applied");
  }
  const U64 joint_rows = sky::action_codomain_size(fit->E);
  const std::size_t count = val(values).size();
  if (joint_rows == 0) {
    return model::fail<Ref<SignalEstimate>>(Code::invalid_model,
                                            "the output model has no rows", "apply_sky",
                                            "the correction cannot be applied");
  }

  // -------------------------------------------------------------------------
  // Which rows of E does this signal cover?
  //
  // E and T live on the concatenated leaf space of the training frames of the
  // fit, so a single frame's estimate covers one contiguous block of rows.  The
  // block is located by leaf identity (the fit publishes the training leaf
  // space in its support, the frame estimate carries its own leaves), never by
  // guessing from the length: two frames of the same size are not the same
  // frame, and a wrong block would subtract another frame's sky.
  // -------------------------------------------------------------------------
  bool frame_block = false;
  U64 row_offset = 0;
  if (joint_rows == static_cast<U64>(count)) {
    // The signal already covers the whole training leaf space.
  } else {
    const Ref<SupportMap>& training = fit->support;
    if (!training || !training->index) {
      return model::fail<Ref<SignalEstimate>>(
          Code::invalid_model,
          "the signal covers a block of the training leaf space but the fit does not publish that "
          "space, so the block cannot be placed",
          "apply_sky", "the correction cannot be applied");
    }
    if (frame != nullptr) {
      // The frame identity names the block directly.
      bool found = false;
      for (const SourcePartition& partition : training->index->partitions) {
        if (partition.channel != *frame) continue;
        if (partition.count != static_cast<U64>(count)) {
          return model::fail<Ref<SignalEstimate>>(
              Code::invalid_model,
              "the frame names a training block whose length does not match the signal",
              "apply_sky", "the correction cannot be applied");
        }
        row_offset = partition.index_begin;
        found = true;
        break;
      }
      if (!found) {
        return model::fail<Ref<SignalEstimate>>(
            Code::invalid_model,
            "the frame is not part of the training leaf space of the fit", "apply_sky",
            "the correction cannot be applied");
      }
      frame_block = true;
    } else {
      const Ref<SupportMap>& frame_support = signal->support;
      if (!frame_support || !frame_support->index || !frame_support->index->ids ||
          !training->index->ids) {
        return model::fail<Ref<SignalEstimate>>(
            Code::invalid_model,
            "the signal covers a block of the training leaf space but the leaf identities of the "
            "fit or of the signal are missing, so the block cannot be placed",
            "apply_sky", "the correction cannot be applied");
      }
      Result<std::vector<U64>> training_ids = read_u64_array(context, training->index->ids);
      Result<std::vector<U64>> frame_ids = read_u64_array(context, frame_support->index->ids);
      if (!ok(training_ids) || !ok(frame_ids)) {
        return model::fail<Ref<SignalEstimate>>(Code::invalid_model,
                                                "the leaf identities cannot be read", "apply_sky",
                                                "the correction cannot be applied");
      }
      if (val(frame_ids).size() != count * 2) {
        return model::fail<Ref<SignalEstimate>>(
            Code::invalid_model,
            "the leaf identities of the signal do not match its length", "apply_sky",
            "the correction cannot be applied");
      }
      const std::vector<U64>& joint = val(training_ids);
      const std::vector<U64>& leaves = val(frame_ids);
      std::size_t matches = 0;
      for (U64 offset = 0; offset + static_cast<U64>(count) <= joint_rows; ++offset) {
        bool match = true;
        for (std::size_t i = 0; i < count && match; ++i) {
          if (joint[2 * (offset + i)] != leaves[2 * i] ||
              joint[2 * (offset + i) + 1] != leaves[2 * i + 1]) {
            match = false;
          }
        }
        if (!match) continue;
        ++matches;
        row_offset = offset;
      }
      if (matches == 0) {
        return model::fail<Ref<SignalEstimate>>(
            Code::invalid_model,
            "the leaf identities of the signal do not match any contiguous block of the training "
            "leaf space of the fit",
            "apply_sky", "the correction cannot be applied");
      }
      if (matches > 1) {
        // Several training frames observe exactly these leaves (the usual
        // mosaic case): the leaf identity alone cannot say which frame's sky
        // offset belongs to this signal, and choosing one would subtract
        // another frame's sky.  The caller passes the frame identity.
        return model::fail<Ref<SignalEstimate>>(
            Code::invalid_model,
            "the signal's leaves match more than one training frame; pass the frame identity (or "
            "the joint training-space signal) so the sky of the right frame is subtracted",
            "apply_sky", "the correction cannot be applied");
      }
      frame_block = true;
    }
  }

  // The selection actions of the block: S picks the block's rows of E, S^T
  // picks the block's columns of T.
  Result<Ref<LinearAction>> selection;
  Result<Ref<LinearAction>> selection_transposed;
  if (frame_block) {
    operators::CsrMatrix forward;
    forward.rows = static_cast<U64>(count);
    forward.cols = joint_rows;
    forward.offsets.assign(count + 1, 0);
    for (std::size_t i = 0; i < count; ++i) {
      forward.indices.push_back(row_offset + i);
      forward.values.push_back(1.0);
      forward.offsets[i + 1] = forward.indices.size();
    }
    operators::CsrMatrix backward;
    backward.rows = joint_rows;
    backward.cols = static_cast<U64>(count);
    backward.offsets.assign(static_cast<std::size_t>(joint_rows) + 1, 0);
    for (U64 row = 0; row < joint_rows; ++row) {
      if (row >= row_offset && row < row_offset + count) {
        backward.indices.push_back(row - row_offset);
        backward.values.push_back(1.0);
      }
      backward.offsets[static_cast<std::size_t>(row) + 1] = backward.indices.size();
    }
    selection = sky::action_from_csr(context, forward, model::unit_dimensionless());
    selection_transposed = sky::action_from_csr(context, backward, model::unit_dimensionless());
    if (!ok(selection) || !ok(selection_transposed)) {
      return model::fail<Ref<SignalEstimate>>(Code::invalid_model,
                                              "the block selection cannot be built", "apply_sky",
                                              "the correction cannot be applied");
    }
  }

  // E_f = S E (the block's rows), T_f = T S^T (the block's columns of T).
  Ref<LinearAction> e_action = fit->E;
  Ref<LinearAction> t_action = fit->T;
  if (frame_block) {
    Result<operators::CsrMatrix> block_e = sky::multiply_csr(context, val(selection), fit->E);
    if (!ok(block_e)) {
      return model::fail<Ref<SignalEstimate>>(block_e.code, "the output model block cannot be formed",
                                              "apply_sky", "the correction cannot be applied");
    }
    Result<Ref<LinearAction>> e_ref = sky::action_from_csr(context, val(block_e),
                                                            model::unit_dimensionless());
    if (!ok(e_ref)) {
      return model::fail<Ref<SignalEstimate>>(e_ref.code, "the output model block cannot be built",
                                              "apply_sky", "the correction cannot be applied");
    }
    Result<operators::CsrMatrix> block_t = sky::multiply_csr(context, fit->T,
                                                              val(selection_transposed));
    if (!ok(block_t)) {
      return model::fail<Ref<SignalEstimate>>(block_t.code, "the control operator block cannot be formed",
                                              "apply_sky", "the correction cannot be applied");
    }
    Result<Ref<LinearAction>> t_ref = sky::action_from_csr(context, val(block_t),
                                                            model::unit_dimensionless());
    if (!ok(t_ref)) {
      return model::fail<Ref<SignalEstimate>>(t_ref.code, "the control operator block cannot be built",
                                              "apply_sky", "the correction cannot be applied");
    }
    e_action = val(e_ref);
    t_action = val(t_ref);
  }

  Result<std::vector<double>> correction = sky::apply_action_full(context, e_action, val(beta));
  if (!ok(correction)) {
    return model::fail<Ref<SignalEstimate>>(correction.code, "the correction cannot be applied",
                                            "apply_sky", "the correction cannot be applied");
  }
  std::vector<double> mutable_correction = val(correction);
  if (mutable_correction.size() != count) {
    return model::fail<Ref<SignalEstimate>>(Code::invalid_model,
                                            "the correction does not match the signal",
                                            "apply_sky", "the correction cannot be applied");
  }

  // A direction of E that is not estimable gets no correction at all, and the
  // published covariance of that position must belong to the same estimator:
  // the corrected signal of such a row is y_i, so its variance is the row of
  // C_y and not the row of P C_y P^T.  The mask below is what makes the signal
  // and the covariance the same linear functional of the data.
  std::size_t unavailable_count = 0;
  std::vector<char> correction_available(count, 1);
  if (fit->rank.nullspace && fit->rank.nullspace->shape.size() == 2) {
    const U64 columns = fit->rank.nullspace->shape[1];
    const U64 rows = fit->rank.nullspace->shape[0];
    if (rows > 0 && columns > 0) {
      Result<std::vector<double>> nullspace = read_f64_array(context, fit->rank.nullspace);
      if (!ok(nullspace)) {
        return model::fail<Ref<SignalEstimate>>(nullspace.code, "the null space cannot be read",
                                                "apply_sky", "the correction cannot be applied");
      }
      const U64 e_rows = sky::action_codomain_size(e_action);
      const U64 e_cols = sky::action_domain_size(e_action);
      Result<Matrix> E = sky::dense_action(context, e_action, e_rows, e_cols);
      if (!ok(E)) {
        return model::fail<Ref<SignalEstimate>>(E.code, "the output model cannot be applied",
                                                "apply_sky", "the correction cannot be applied");
      }
      for (std::size_t i = 0; i < count && i < val(E).rows; ++i) {
        // The criterion is scale free: the row is unavailable when its
        // projection on the null directions is not negligible compared with
        // its own norm.  An absolute threshold would call a small but fully
        // undetermined row determined.
        double norm = 0.0;
        double row_norm = 0.0;
        for (std::size_t p = 0; p < val(E).cols; ++p) {
          row_norm += val(E)(static_cast<std::size_t>(i), static_cast<std::size_t>(p)) *
                      val(E)(static_cast<std::size_t>(i), static_cast<std::size_t>(p));
        }
        for (std::size_t j = 0; j < rows; ++j) {
          double sum = 0.0;
          for (std::size_t p = 0; p < columns && p < val(E).cols; ++p) {
            sum += val(nullspace)[j * columns + p] * val(E)(static_cast<std::size_t>(i),
                                                             static_cast<std::size_t>(p));
          }
          norm += sum * sum;
        }
        if (norm > 1e-20 * std::max(row_norm, 1e-300) && std::sqrt(norm) > 0.0) {
          mutable_correction[i] = 0.0;
          correction_available[i] = 0;
          ++unavailable_count;
        }
      }
    }
  }

  std::vector<double> corrected(count, 0.0);
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

  // -------------------------------------------------------------------------
  // The corrected covariance.
  //
  // The published covariance must belong to the published signal:
  //   C_z = (I - E K T) C_y (I - E K T)^T
  // with the rows that keep no correction left untouched.  Both are written as
  // one action P = I - M E K T (M the availability mask), and P is folded into
  // the influences of the roots so that the frame estimate can be merged by the
  // coaddition with its shared roots intact:
  //   C_z = sum_k (P L_k) F_k F_k^T (P L_k)^T.
  // -------------------------------------------------------------------------
  Ref<CovarianceAction> corrected_noise = signal->noise;
  bool covariance_written = false;
  std::vector<std::string> estimate_omitted;
  if (signal->noise && signal->noise->model) {
    // The outer influence of the input covariance is part of the input model:
    // C_in = I0 C_model I0^T, so the total outer action is P I0.
    Ref<LinearAction> outer = fit->P;
    if (!outer) {
      // A fit without the action is rebuilt from E, K and T of this block.
      ActionRecipe recipe;
      recipe.kind = ActionRecipe::Kind::sky;
      recipe.algorithm = "canonical_sky_v1";
      recipe.version = 1;
      recipe.children = {"E", "K", "T"};
      const std::vector<Ref<LinearAction>> children{e_action, fit->K, t_action};
      Result<Ref<LinearAction>> built =
          make_action(context, recipe, children, std::span<const ArrayRef>{});
      if (!ok(built)) {
        return model::fail<Ref<SignalEstimate>>(built.code,
                                                "the correction action cannot be built",
                                                "apply_sky", "the correction cannot be applied");
      }
      outer = val(built);
    } else if (frame_block || unavailable_count > 0) {
      // The published P of the fit is the whole-space action; the block (and
      // the mask) need their own action.
      Ref<LinearAction> masked_e = e_action;
      if (frame_block) {
        std::vector<double> mask_values(correction_available.size(), 0.0);
        for (std::size_t i = 0; i < mask_values.size(); ++i) {
          mask_values[i] = correction_available[i] ? 1.0 : 0.0;
        }
        Result<ArrayRef> mask_array = operators::write_f64(
            context, mask_values, std::vector<U64>{static_cast<U64>(mask_values.size())},
            model::unit_dimensionless(), Id());
        if (!ok(mask_array)) {
          return model::fail<Ref<SignalEstimate>>(mask_array.code,
                                                  "the correction mask cannot be stored",
                                                  "apply_sky", "the correction cannot be applied");
        }
        ActionRecipe mask_recipe;
        mask_recipe.kind = ActionRecipe::Kind::diagonal;
        mask_recipe.algorithm = "canonical_diagonal_v1";
        mask_recipe.version = 1;
        mask_recipe.arrays = {val(mask_array)->identity.object_id};
        const std::vector<ArrayRef> mask_arrays{val(mask_array)};
        Result<Ref<LinearAction>> mask = make_action(
            context, mask_recipe, std::span<const Ref<LinearAction>>{}, mask_arrays);
        if (!ok(mask)) {
          return model::fail<Ref<SignalEstimate>>(mask.code, "the correction mask cannot be built",
                                                  "apply_sky", "the correction cannot be applied");
        }
        Result<operators::CsrMatrix> product = sky::multiply_csr(context, val(mask), e_action);
        if (!ok(product)) {
          return model::fail<Ref<SignalEstimate>>(product.code,
                                                  "the masked output model cannot be formed",
                                                  "apply_sky", "the correction cannot be applied");
        }
        Result<Ref<LinearAction>> masked =
            sky::action_from_csr(context, val(product), model::unit_dimensionless());
        if (!ok(masked)) {
          return model::fail<Ref<SignalEstimate>>(masked.code,
                                                  "the masked output model cannot be built",
                                                  "apply_sky", "the correction cannot be applied");
        }
        masked_e = val(masked);
      }
      ActionRecipe recipe;
      recipe.kind = ActionRecipe::Kind::sky;
      recipe.algorithm = "canonical_sky_masked_v1";
      recipe.version = 1;
      recipe.children = {"masked_E", "K", "T"};
      const std::vector<Ref<LinearAction>> children{masked_e, fit->K, t_action};
      Result<Ref<LinearAction>> built =
          make_action(context, recipe, children, std::span<const ArrayRef>{});
      if (!ok(built)) {
        return model::fail<Ref<SignalEstimate>>(built.code,
                                                "the correction action cannot be built",
                                                "apply_sky", "the correction cannot be applied");
      }
      outer = val(built);
    }
    if (signal->noise->influence) {
      // Fold the input's outer influence into P: C_z = P I0 C I0^T P^T.
      Result<operators::CsrMatrix> product =
          sky::multiply_csr(context, outer, signal->noise->influence);
      if (!ok(product)) {
        return model::fail<Ref<SignalEstimate>>(
            product.code,
            "the input covariance already carries an outer influence and the two cannot be "
            "composed",
            "apply_sky", "the correction cannot be applied");
      }
      Result<Ref<LinearAction>> folded =
          sky::action_from_csr(context, val(product), model::unit_dimensionless());
      if (!ok(folded)) {
        return model::fail<Ref<SignalEstimate>>(folded.code,
                                                "the correction action cannot be built",
                                                "apply_sky", "the correction cannot be applied");
      }
      outer = val(folded);
    }

    auto noise_model = std::make_shared<NoiseModel>();
    const NoiseModel& source = *signal->noise->model;
    noise_model->variance_unit = source.variance_unit;
    noise_model->condition = source.condition;
    noise_model->state = source.state;
    noise_model->innovations = source.innovations;
    noise_model->shared_factors = source.shared_factors;
    // Fold the correction into each influence: outer * influence.  A CSR
    // product is preferred, but the input noise may carry a diagonal/compose
    // influence, so an exact composition is the fallback.
    const auto fold = [&](const Ref<LinearAction>& influence)
        -> Result<Ref<LinearAction>> {
      Result<operators::CsrMatrix> product = sky::multiply_csr(context, outer, influence);
      if (ok(product)) {
        return sky::action_from_csr(context, val(product), source.variance_unit);
      }
      return compose(context, outer, influence);
    };
    noise_model->influence.reserve(source.influence.size());
    for (const Ref<LinearAction>& influence : source.influence) {
      Result<Ref<LinearAction>> action = fold(influence);
      if (!ok(action)) {
        return model::fail<Ref<SignalEstimate>>(action.code,
                                                "the corrected influence cannot be formed",
                                                "apply_sky", "the correction cannot be applied");
      }
      noise_model->influence.push_back(val(action));
    }
    for (const UnknownTerm& term : source.unknown_terms) {
      UnknownTerm copy = term;
      if (term.influence) {
        Result<Ref<LinearAction>> action = fold(term.influence);
        if (!ok(action)) {
          return model::fail<Ref<SignalEstimate>>(action.code,
                                                  "the corrected unknown term cannot be formed",
                                                  "apply_sky", "the correction cannot be applied");
        }
        copy.influence = val(action);
      }
      noise_model->unknown_terms.push_back(copy);
    }

    // The per frame block is the conditional covariance of the corrected
    // frame given the fitted parameters: it carries the frame's own rows of
    // P C_y P^T, i.e. C_f - E_f K T_f C_f - C_f T_f^T K^T E_f^T
    // + E_f K T_f C_f T_f^T K^T E_f^T.  The unconditional block adds
    // E_f K (C_c - T_f C_f T_f^T) K^T E_f^T, the part of beta's variance that
    // the other frames contribute.  That part is not a sum of positive roots of
    // this frame alone (the foundation asks for the action form instead of
    // "independent parameter variances"), so it is declared as omitted instead
    // of being silently dropped: a caller that needs the unconditional C_z uses
    // the joint entry (one apply_sky call over the whole training leaf space),
    // whose action form is exact.
    if (frame_block) {
      estimate_omitted.push_back("fit_parameter_variance_in_the_per_frame_covariance");
    }

    noise_model->identity = model::make_identity(
        "acsd.NoiseModel", 2, 0,
        model::sha256_hex("sky:parameter:" + std::to_string(noise_model->influence.size()) + ":" +
                          std::to_string(static_cast<int>(noise_model->state))),
        "acsd");
    noise_model->assessment = model::make_assessment(Code::ok);
    noise_model->assessment.complete =
        noise_model->state == NoiseState::known && noise_model->unknown_terms.empty();
    if (!noise_model->assessment.complete) {
      noise_model->assessment.science = ScienceState::conditional;
      model::set_omitted(noise_model->assessment, "known_complete_variance");
    }
    noise_model->assessment.noise = noise_model->state;
    Result<Ref<CovarianceAction>> covariance = make_covariance(
        context, Ref<NoiseModel>(std::shared_ptr<const NoiseModel>(noise_model)));
    if (!ok(covariance)) {
      return model::fail<Ref<SignalEstimate>>(covariance.code,
                                              "the corrected covariance cannot be built",
                                              "apply_sky", "the correction cannot be applied");
    }
    auto wrapped = std::make_shared<CovarianceAction>(*val(covariance));
    // The corrected covariance acts on the same space as the input.
    wrapped->input_output = signal->noise->input_output;
    if (frame_block && signal->support && signal->support->index) {
      wrapped->input_output.domain = signal->support->index;
      wrapped->input_output.codomain = signal->support->index;
    }
    if (frame_block) {
      // The published signal subtracts the JOINT beta (which uses the other
      // frames), while this block models y_f - E_f K T_f y_f (the frame's own
      // control rows only).  P_f C_f P_f^T is therefore not the covariance of
      // the published signal, and the cross-frame term cannot be represented
      // in a per-frame object.  Publish unknown instead of a wrong covariance;
      // the joint entry remains exact.
      corrected_noise = nullptr;
      estimate_omitted.push_back(
          "per_frame_covariance_does_not_match_the_joint_beta_estimand");
    } else {
      corrected_noise = std::shared_ptr<const CovarianceAction>(wrapped);
      covariance_written = true;
    }
  }
  (void)covariance_written;

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
  estimate->assessment.complete = !frame_block;
  if (frame_block) {
    estimate->assessment.science = ScienceState::conditional;
    estimate->assessment.noise = NoiseState::unknown;
  }
  estimate->assessment.condition = signal->estimator.condition;
  for (const std::string& omitted : estimate_omitted) {
    model::set_omitted(estimate->assessment, omitted);
  }

  Result<Ref<SignalEstimate>> out = model::ok_result(std::shared_ptr<const SignalEstimate>(estimate));
  out.assessment.condition = estimate->assessment.condition;
  for (const std::string& omitted : estimate_omitted) {
    model::set_omitted(out.assessment, omitted);
  }
  model::set_included(out.assessment, "relative_sky_correction");
  if (!frame_block) {
    model::set_included(out.assessment, "corrected_covariance_with_both_negative_cross_terms");
  }
  if (frame_block) {
    model::set_included(out.assessment, "correction_of_one_frame_block_of_the_training_space");
    out.assessment.science = ScienceState::conditional;
  }
  if (unavailable_count > 0) {
    model::add_issue(out.assessment, Code::unavailable,
                     "the sky is not estimable in some directions, those positions keep no correction",
                     "apply_sky", "the correction is unavailable at those positions");
    out.assessment.science = ScienceState::conditional;
  }
  return out;
}

}  // namespace

Result<Ref<SignalEstimate>> apply_sky(Context& context, Ref<SignalEstimate> signal,
                                       Ref<SkyFit> fit) {
  return apply_sky_impl(context, signal, fit, nullptr);
}

Result<Ref<SignalEstimate>> apply_sky(Context& context, Ref<SignalEstimate> signal,
                                       Ref<SkyFit> fit, Id frame) {
  return apply_sky_impl(context, signal, fit, &frame);
}

}  // namespace acsd
