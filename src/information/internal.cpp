// Internal helpers of the information module (see internal.hpp).
#include "information/internal.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "operators/linear_action.hpp"
#include "uncertainty/covariance.hpp"

namespace acsd::information {
namespace {

using numerics::Matrix;

std::size_t element_count(const std::vector<U64>& shape) {
  std::size_t total = 1;
  for (U64 extent : shape) {
    if (extent == 0) return 0;
    total *= static_cast<std::size_t>(extent);
  }
  return total;
}

}  // namespace

Result<std::vector<double>> apply_action_full(Context& context, Ref<LinearAction> action,
                                              const std::vector<double>& values) {
  if (!action) {
    return model::fail<std::vector<double>>(Code::invalid_request, "no action supplied",
                                            "information.apply", "the action cannot be applied");
  }
  const std::vector<U64> shape{static_cast<U64>(values.size())};
  Result<ArrayRef> array =
      operators::write_f64(context, values, shape, model::unit_dimensionless(), Id());
  if (!ok(array)) {
    return model::fail<std::vector<double>>(array.code, "the vector cannot be stored",
                                            "information.apply", "the action cannot be applied");
  }
  Result<ArrayRef> mapped = action->apply(context, val(array));
  if (!ok(mapped)) {
    return model::fail<std::vector<double>>(mapped.code, "the action cannot be applied",
                                            "information.apply", "the action cannot be applied");
  }
  const std::size_t count = element_count(val(mapped)->shape);
  Result<std::vector<double>> out = operators::read_f64(context, val(mapped), 0,
                                                        static_cast<U64>(count));
  if (!ok(out)) {
    return model::fail<std::vector<double>>(out.code, "the output cannot be read",
                                            "information.apply", "the action cannot be applied");
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
    return model::fail<Matrix>(Code::invalid_request, "no action supplied", "information.dense",
                                "the action cannot be materialised");
  }
  const U64 derived_rows = action_codomain_size(action);
  const U64 derived_cols = action_domain_size(action);
  if (rows == 0) rows = derived_rows;
  if (cols == 0) cols = derived_cols;
  if (rows == 0 || cols == 0) {
    return model::fail<Matrix>(Code::invalid_model, "the action has no usable shape",
                                "information.dense", "the action cannot be materialised");
  }
  if (derived_rows != 0 && derived_rows != rows) {
    return model::fail<Matrix>(Code::invalid_model, "the action codomain does not match",
                                "information.dense", "the action cannot be materialised");
  }
  if (derived_cols != 0 && derived_cols != cols) {
    return model::fail<Matrix>(Code::invalid_model, "the action domain does not match",
                                "information.dense", "the action cannot be materialised");
  }
  Matrix out(static_cast<std::size_t>(rows), static_cast<std::size_t>(cols));
  std::vector<double> unit(static_cast<std::size_t>(cols), 0.0);
  std::vector<double> column;
  for (U64 j = 0; j < cols; ++j) {
    std::fill(unit.begin(), unit.end(), 0.0);
    unit[static_cast<std::size_t>(j)] = 1.0;
    Result<std::vector<double>> mapped = apply_action_full(context, action, unit);
    if (!ok(mapped)) {
      return model::fail<Matrix>(mapped.code, "the action cannot be applied", "information.dense",
                                  "the action cannot be materialised");
    }
    if (val(mapped).size() != rows) {
      return model::fail<Matrix>(Code::invalid_model, "the action output size is inconsistent",
                                  "information.dense", "the action cannot be materialised");
    }
    for (U64 i = 0; i < rows; ++i) {
      out(static_cast<std::size_t>(i), static_cast<std::size_t>(j)) = val(mapped)[static_cast<std::size_t>(i)];
    }
    column.clear();
  }
  return model::ok_result(std::move(out));
}

bool covariance_has_unknown_term(Ref<CovarianceAction> covariance) {
  if (!covariance || !covariance->model) return false;
  for (const UnknownTerm& term : covariance->model->unknown_terms) {
    if (term.influence) return true;
  }
  return false;
}

Result<Matrix> dense_covariance_factor(Context& context, Ref<CovarianceAction> covariance) {
  if (!covariance || !covariance->model) {
    return model::fail<Matrix>(Code::invalid_request, "no covariance model supplied",
                                "information.factor", "the covariance factor cannot be built");
  }
  const NoiseModel& noise = *covariance->model;
  std::vector<Ref<NoiseRoot>> roots;
  roots.insert(roots.end(), noise.innovations.begin(), noise.innovations.end());
  roots.insert(roots.end(), noise.shared_factors.begin(), noise.shared_factors.end());
  if (roots.size() != noise.influence.size()) {
    return model::fail<Matrix>(Code::invalid_model, "the influence count does not match the roots",
                                "information.factor", "the covariance factor cannot be built");
  }
  U64 rows = 0;
  for (const Ref<LinearAction>& influence : noise.influence) {
    if (!influence) {
      return model::fail<Matrix>(Code::invalid_model, "an influence is missing",
                                  "information.factor", "the covariance factor cannot be built");
    }
    const U64 size = action_codomain_size(influence);
    if (size == 0) {
      return model::fail<Matrix>(Code::invalid_model, "an influence has no codomain",
                                  "information.factor", "the covariance factor cannot be built");
    }
    if (rows == 0) {
      rows = size;
    } else if (rows != size) {
      return model::fail<Matrix>(Code::invalid_model, "the influences disagree on the codomain",
                                  "information.factor", "the covariance factor cannot be built");
    }
  }
  if (rows == 0) {
    return model::fail<Matrix>(Code::invalid_model, "the noise model has no root",
                                "information.factor", "the covariance factor cannot be built");
  }
  std::vector<U64> latent_counts;
  U64 total_latent = 0;
  for (const Ref<NoiseRoot>& root : roots) {
    if (!root || !root->factor) {
      return model::fail<Matrix>(Code::invalid_model, "a noise root is incomplete",
                                  "information.factor", "the covariance factor cannot be built");
    }
    const U64 latent = action_domain_size(root->factor);
    if (latent == 0) {
      return model::fail<Matrix>(Code::invalid_model, "a noise factor has no domain",
                                  "information.factor", "the covariance factor cannot be built");
    }
    latent_counts.push_back(latent);
    total_latent += latent;
  }
  Matrix factor(static_cast<std::size_t>(rows), static_cast<std::size_t>(total_latent));
  U64 offset = 0;
  for (std::size_t k = 0; k < roots.size(); ++k) {
    const U64 latent = latent_counts[k];
    std::vector<double> unit(static_cast<std::size_t>(latent), 0.0);
    std::vector<double> root_value;
    std::vector<double> output;
    for (U64 j = 0; j < latent; ++j) {
      std::fill(unit.begin(), unit.end(), 0.0);
      unit[static_cast<std::size_t>(j)] = 1.0;
      Result<std::vector<double>> mapped =
          apply_action_full(context, roots[k]->factor, unit);
      if (!ok(mapped)) {
        return model::fail<Matrix>(mapped.code, "a noise factor cannot be applied",
                                    "information.factor", "the covariance factor cannot be built");
      }
      root_value = val(mapped);
      Result<std::vector<double>> influenced =
          apply_action_full(context, noise.influence[k], root_value);
      if (!ok(influenced)) {
        return model::fail<Matrix>(influenced.code, "an influence cannot be applied",
                                    "information.factor", "the covariance factor cannot be built");
      }
      output = val(influenced);
      if (output.size() != rows) {
        return model::fail<Matrix>(Code::invalid_model, "an influence changes the output size",
                                    "information.factor", "the covariance factor cannot be built");
      }
      for (U64 i = 0; i < rows; ++i) {
        factor(static_cast<std::size_t>(i), static_cast<std::size_t>(offset + j)) =
            output[static_cast<std::size_t>(i)];
      }
    }
    offset += latent;
  }
  return model::ok_result(std::move(factor));
}

Matrix complete_orthogonal_factorisation(const Matrix& v) {
  const std::size_t n = v.rows;
  const std::size_t m = std::min(v.cols, v.rows);
  Matrix work = v;
  Matrix h(n, n, 0.0);
  for (std::size_t i = 0; i < n; ++i) h(i, i) = 1.0;
  if (m == 0) return h;
  std::vector<double> u(n);
  for (std::size_t k = 0; k < m; ++k) {
    double norm = 0.0;
    for (std::size_t i = k; i < n; ++i) norm += work(i, k) * work(i, k);
    norm = std::sqrt(norm);
    if (!(norm > 0.0)) continue;
    const double alpha = (work(k, k) > 0.0) ? -norm : norm;
    for (std::size_t i = k; i < n; ++i) u[i] = work(i, k);
    u[k] -= alpha;
    double uu = 0.0;
    for (std::size_t i = k; i < n; ++i) uu += u[i] * u[i];
    if (!(uu > 0.0)) continue;
    const double tau = 2.0 / uu;
    // Apply the reflector to the remaining columns of the work matrix.
    for (std::size_t j = k + 1; j < m; ++j) {
      double s = 0.0;
      for (std::size_t i = k; i < n; ++i) s += u[i] * work(i, j);
      s *= tau;
      for (std::size_t i = k; i < n; ++i) work(i, j) -= s * u[i];
    }
    // Left multiply the accumulated orthogonal matrix by the reflector.
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



Result<RangeSplit> split_covariance_range(const Matrix& factor, double rank_relative) {
  if (factor.rows == 0 || factor.cols == 0) {
    return model::fail<RangeSplit>(Code::invalid_model, "the covariance factor is empty",
                                   "information.split", "the null space cannot be identified");
  }
  numerics::SvdResult svd = numerics::jacobi_svd(factor, rank_relative);
  if (!svd.converged) {
    return model::fail<RangeSplit>(Code::nonconverged, "the singular value decomposition failed",
                                   "information.split", "the null space cannot be identified");
  }
  RangeSplit split;
  split.rank_relative = rank_relative;
  split.singular_values = svd.singular_values;
  const double sigma_max = svd.singular_values.empty() ? 0.0 : svd.singular_values.front();
  const double threshold = rank_relative * sigma_max;
  std::size_t rank = 0;
  for (double value : svd.singular_values) {
    if (value > threshold) ++rank;
  }
  if (rank == 0) rank = 1;
  split.rank = rank;
  // The rank is certified only when the singular values are separated around
  // the threshold; otherwise the split stays conditional.
  const double gap_low = (rank >= 1) ? svd.singular_values[rank - 1] : 0.0;
  const double gap_high = (rank < svd.singular_values.size()) ? svd.singular_values[rank] : 0.0;
  split.rank_certified = svd.sigma_error_bound + threshold < gap_low ||
                         (rank == svd.singular_values.size() && svd.sigma_error_bound == 0.0);
  if (rank < svd.singular_values.size() && gap_high <= threshold) {
    split.rank_certified = false;
  }
  Matrix basis(factor.rows, rank);
  for (std::size_t i = 0; i < factor.rows; ++i) {
    for (std::size_t j = 0; j < rank; ++j) {
      basis(i, j) = svd.u(i, j);
    }
  }
  Matrix complete = complete_orthogonal_factorisation(basis);
  Matrix null_basis(factor.rows, factor.rows - rank);
  for (std::size_t i = 0; i < factor.rows; ++i) {
    for (std::size_t j = 0; j < null_basis.cols; ++j) {
      null_basis(i, j) = complete(i, rank + j);
    }
  }
  split.range_basis = std::move(basis);
  split.null_basis = std::move(null_basis);
  return model::ok_result(std::move(split));
}

// ---------------------------------------------------------------------------
// Spherical chart
// ---------------------------------------------------------------------------

bool ChartProjection::valid() const { return maximum_radius_rad > 0.0; }

namespace {

Vec3 to_vec3(const SkyICRS& v) { return Vec3{v.nx, v.ny, v.nz}; }

Vec3 cross3(const Vec3& a, const Vec3& b) {
  return Vec3{a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

double dot3(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

}  // namespace

bool make_chart_projection(const Chart& chart, ChartProjection& out) {
  const double norm = std::sqrt(dot3(Vec3{chart.center.nx, chart.center.ny, chart.center.nz},
                                     Vec3{chart.center.nx, chart.center.ny, chart.center.nz}));
  if (!(norm > 0.0) || !std::isfinite(norm)) return false;
  if (!(chart.maximum_radius_rad > 0.0) || !std::isfinite(chart.maximum_radius_rad)) return false;
  Vec3 c{chart.center.nx / norm, chart.center.ny / norm, chart.center.nz / norm};
  const Vec3 up{0.0, 0.0, 1.0};
  Vec3 east = cross3(up, c);
  double east_norm = std::sqrt(dot3(east, east));
  if (!(east_norm > 1e-12)) {
    // At the pole use the x axis as the local east direction.
    const Vec3 reference{1.0, 0.0, 0.0};
    east = cross3(reference, c);
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
  out.maximum_radius_rad = chart.maximum_radius_rad;
  return true;
}

bool project_to_chart(const ChartProjection& projection, const SkyICRS& direction, Vec2& out) {
  const Vec3 v{direction.nx, direction.ny, direction.nz};
  const double norm = std::sqrt(dot3(v, v));
  if (!(norm > 0.0) || !std::isfinite(norm)) return false;
  const Vec3 unit{v[0] / norm, v[1] / norm, v[2] / norm};
  const double denominator = dot3(unit, to_vec3(projection.center));
  if (!(denominator > 1e-9)) return false;
  const double xi = dot3(unit, projection.east) / denominator;
  const double eta = dot3(unit, projection.north) / denominator;
  if (!std::isfinite(xi) || !std::isfinite(eta)) return false;
  // The angular distance from the centre is atan(|xi, eta|) in the tangent
  // plane; compare it with the declared radius.
  const double radius = std::atan(std::sqrt(xi * xi + eta * eta));
  if (radius > projection.maximum_radius_rad) return false;
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

double angular_distance(const SkyICRS& a, const SkyICRS& b) {
  const Vec3 u{a.nx, a.ny, a.nz};
  const Vec3 v{b.nx, b.ny, b.nz};
  const double nu = std::sqrt(dot3(u, u));
  const double nv = std::sqrt(dot3(v, v));
  if (!(nu > 0.0) || !(nv > 0.0)) return 0.0;
  Vec3 un{u[0] / nu, u[1] / nu, u[2] / nu};
  Vec3 vn{v[0] / nv, v[1] / nv, v[2] / nv};
  const Vec3 c = cross3(un, vn);
  const double sn = std::sqrt(dot3(c, c));
  const double cs = dot3(un, vn);
  return std::atan2(sn, cs);
}

// ---------------------------------------------------------------------------
// Local HEALPix NESTED face geometry
// ---------------------------------------------------------------------------

namespace {

struct LocalFaceParam {
  int j;
  int l;
};

constexpr LocalFaceParam kLocalFaces[12] = {{2, 1}, {2, 3}, {2, 5}, {2, 7},
                                             {3, 0}, {3, 2}, {3, 4}, {3, 6},
                                             {4, 1}, {4, 3}, {4, 5}, {4, 7}};

SkyICRS local_face_point_impl(const LocalFaceParam& param, double s, double t) {
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

U64 local_face_index(U32 order, U64 ix, U64 iy) {
  U64 p = 0;
  for (U32 i = 0; i < order; ++i) {
    if ((ix >> i) & 1ull) p |= (1ull << (2 * i));
    if ((iy >> i) & 1ull) p |= (1ull << (2 * i + 1));
  }
  return p;
}

}  // namespace

SkyICRS local_face_point(U32 face, double s, double t) {
  return local_face_point_impl(kLocalFaces[face % 12], s, t);
}

bool local_leaf_lookup(U32 order, const SkyICRS& direction, U64& ipix) {
  if (order > 29) return false;
  const double norm = std::sqrt(direction.nx * direction.nx + direction.ny * direction.ny +
                                direction.nz * direction.nz);
  if (!(norm > 0.0) || !std::isfinite(norm)) return false;
  const double nx = direction.nx / norm;
  const double ny = direction.ny / norm;
  const double nz = direction.nz / norm;
  const double phi = std::atan2(ny, nx);
  double tt = phi / (M_PI / 2.0);
  if (tt < 0.0) tt += 4.0;
  const double z = nz;
  const U64 nside = 1ull << order;
  U64 face = 0;
  U64 ix = 0;
  U64 iy = 0;
  if (std::fabs(z) <= 2.0 / 3.0) {
    const double jp = std::floor(static_cast<double>(nside) * (0.5 + tt - 0.75 * z));
    const double jm = std::floor(static_cast<double>(nside) * (0.5 + tt + 0.75 * z));
    const U64 ifp = static_cast<U64>(jp) / nside;
    const U64 ifm = static_cast<U64>(jm) / nside;
    U64 ifinal = 0;
    if (ifp == ifm) {
      ifinal = ifp | 4ull;
    } else if (ifp < ifm) {
      ifinal = ifp;
    } else {
      ifinal = ifm + 8ull;
    }
    face = ifinal;
    const U64 ixp = static_cast<U64>(jp) % nside;
    const U64 ixm = static_cast<U64>(jm) % nside;
    ix = ixm;
    iy = nside - 1 - ixp;
  } else {
    const bool north = z > 0.0;
    const U64 sector = static_cast<U64>(std::floor(tt));
    const double tp = tt - static_cast<double>(sector);
    const double tmp = static_cast<double>(nside) * std::sqrt(3.0 * (1.0 - std::fabs(z)));
    const U64 jp = std::min<U64>(nside - 1, static_cast<U64>(std::floor(tp * tmp)));
    const U64 jm = std::min<U64>(nside - 1, static_cast<U64>(std::floor((1.0 - tp) * tmp)));
    if (north) {
      face = sector % 4ull;
      ix = nside - jm - 1;
      iy = nside - jp - 1;
    } else {
      face = 8ull + (sector % 4ull);
      ix = jp;
      iy = jm;
    }
  }
  if (face >= 12) return false;
  ipix = face * nside * nside + local_face_index(order, ix, iy);
  return true;
}

SkyICRS local_leaf_center(U32 order, U64 ipix) {
  const U64 nside = 1ull << order;
  const U64 face = ipix / (nside * nside);
  const U64 p = ipix % (nside * nside);
  U64 ix = 0;
  U64 iy = 0;
  for (U32 i = 0; i < order; ++i) {
    if ((p >> (2 * i)) & 1ull) ix |= (1ull << i);
    if ((p >> (2 * i + 1)) & 1ull) iy |= (1ull << i);
  }
  const double s = (static_cast<double>(ix) + 0.5) / static_cast<double>(nside);
  const double t = (static_cast<double>(iy) + 0.5) / static_cast<double>(nside);
  return local_face_point(static_cast<U32>(face), s, t);
}

double local_leaf_radius_rad(U32 order) {
  const double area = (M_PI / 3.0) / static_cast<double>((1ull << order) * (1ull << order));
  return std::sqrt(area);
}

}  // namespace acsd::information
