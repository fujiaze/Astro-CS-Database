// Offline Gaia catalogue index: build_catalogue_index.
//
// The construction follows SYSTEM.md "盲解 catalogue/XP 公开能力与索引":
//   * order 6 sky sharding, the 64 brightest stars per shard;
//   * 12 nearest candidates per anchor taken from the shard and its
//     neighbouring shards;
//   * at most 16 quads per anchor built from three neighbours, ranked by the
//     smallest triangle area (descending) with the source tuple as tie break
//     and collinear configurations removed;
//   * baseline filter, de-duplication of the source quadruple;
//   * both mirror canonical descriptors are kept (one exported row per
//     chirality convention);
//   * the KD tree splits on the largest spread axis at the median with the
//     source tuple as tie break.
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <vector>

#include "acsd/contracts.hpp"
#include "catalogue/catalogue_internal.hpp"
#include "coordinates/healpix.hpp"
#include "model/array.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"

namespace acsd {
namespace {

using catalogue::describe_quad;
using catalogue::StarRow;

// One registered quadruple.  Two rows are exported per quadruple, one per
// chirality convention, so that both mirror canonical descriptors stay
// available to the solver.
struct QuadEntry {
  std::array<U64, 4> sorted_ids{};   // canonical identity of the quadruple
  std::array<U64, 4> ids_plus{};     // identifiers in the role order of the counter clockwise frame
  std::array<U64, 4> ids_minus{};    // identifiers in the role order of the clockwise frame
  std::array<double, 4> plus{};
  std::array<double, 4> minus{};
  double baseline_rad = 0.0;
  Vec3 centre{};
};

// The KD tree nodes are the quad rows themselves; the arrays are indexed by the
// row and hold the children, the split axis, the split value and the box of
// the descriptor coordinates covered by the subtree.
struct KdNode {
  U64 left = ~0ull;
  U64 right = ~0ull;
  std::uint8_t axis = 0;
  double split = 0.0;
  double lower[4]{};
  double upper[4]{};
};

constexpr U64 kNoNode = ~0ull;

Unit unit_radian() {
  Unit unit;
  unit.scale = "rad";
  unit.sr_power = 0;
  unit.pixel_power = 0;
  unit.nm_power = 0;
  unit.multiplier = 1.0;
  unit.linear_power = 0;
  return unit;
}

// Reads a whole f64 array through the public storage interface.
Result<std::vector<double>> read_f64_array(Context& context, const ArrayRef& array, U64 count,
                                            const char* what) {
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, count});
  if (!ok(view)) {
    return model::fail<std::vector<double>>(view.code, std::string("the ") + what + " array cannot be read",
                                            "build_catalogue_index", "the index cannot be built");
  }
  return model::ok_result(model::decode_f64(val(view).bytes));
}

Result<std::vector<U64>> read_u64_array(Context& context, const ArrayRef& array, U64 count,
                                         const char* what) {
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, count});
  if (!ok(view)) {
    return model::fail<std::vector<U64>>(view.code, std::string("the ") + what + " array cannot be read",
                                         "build_catalogue_index", "the index cannot be built");
  }
  return model::ok_result(model::decode_u64(val(view).bytes));
}

bool all_finite(const std::vector<double>& values) {
  for (double value : values) {
    if (!std::isfinite(value)) return false;
  }
  return true;
}

// The eight geometric neighbours of a HEALPix cell.
//
// The neighbours follow from the cell geometry itself: the cell centre is
// displaced in the tangent plane towards the middle of each of the four edges
// and the four diagonal directions and the cell that contains the displaced
// direction is looked up.  Face boundaries are handled by the lookup, so the
// candidate pool stays local in every direction.
std::vector<HealpixLeaf> cell_neighbours(U32 order, U64 ipix) {
  std::vector<HealpixLeaf> leaves;
  const U32 face = coordinates::healpix_face(order, ipix);
  const std::array<U64, 2> xy = coordinates::healpix_face_xy(order, ipix);
  const U64 nside = coordinates::nside_for(order);
  const double s0 = static_cast<double>(xy[0]) / static_cast<double>(nside);
  const double s1 = static_cast<double>(xy[0] + 1) / static_cast<double>(nside);
  const double t0 = static_cast<double>(xy[1]) / static_cast<double>(nside);
  const double t1 = static_cast<double>(xy[1] + 1) / static_cast<double>(nside);

  const SkyICRS centre_sky = coordinates::healpix_center(order, ipix);
  const Vec3 centre{centre_sky.nx, centre_sky.ny, centre_sky.nz};
  Vec3 east{};
  Vec3 north{};
  catalogue::local_basis(centre, east, north);

  const auto project = [&](const SkyICRS& direction) {
    return catalogue::tangent_project(centre, east, north,
                                      Vec3{direction.nx, direction.ny, direction.nz});
  };
  const Vec2 p10 = project(coordinates::healpix_face_point(face, s1, t0));
  const Vec2 p01 = project(coordinates::healpix_face_point(face, s0, t1));
  const Vec2 p11 = project(coordinates::healpix_face_point(face, s1, t1));

  const Vec2 mid_x{0.5 * (p10[0] + p11[0]), 0.5 * (p10[1] + p11[1])};
  const Vec2 mid_y{0.5 * (p01[0] + p11[0]), 0.5 * (p01[1] + p11[1])};

  const std::array<std::array<double, 2>, 8> offsets{
      std::array<double, 2>{mid_x[0], mid_x[1]},
      std::array<double, 2>{-mid_x[0], -mid_x[1]},
      std::array<double, 2>{mid_y[0], mid_y[1]},
      std::array<double, 2>{-mid_y[0], -mid_y[1]},
      std::array<double, 2>{mid_x[0] + mid_y[0], mid_x[1] + mid_y[1]},
      std::array<double, 2>{-mid_x[0] + mid_y[0], -mid_x[1] + mid_y[1]},
      std::array<double, 2>{mid_x[0] - mid_y[0], mid_x[1] - mid_y[1]},
      std::array<double, 2>{-mid_x[0] - mid_y[0], -mid_x[1] - mid_y[1]}};

  std::set<U64> seen{ipix};
  for (const auto& offset : offsets) {
    // 1.8 cell steps land inside the neighbouring cell, which spans 0.5 to
    // 1.5 cell steps from the centre.
    const Vec2 target{1.8 * offset[0], 1.8 * offset[1]};
    const Vec3 direction = catalogue::tangent_unproject(centre, east, north, target);
    HealpixLeaf leaf{};
    const SkyICRS lookup_direction{direction[0], direction[1], direction[2]};
    if (!coordinates::healpix_lookup(order, lookup_direction, leaf)) continue;
    if (!seen.insert(leaf.ipix).second) continue;
    leaves.push_back(leaf);
  }
  return leaves;
}

// Builds the KD tree over the descriptor rows.
//
// The node index of the tree is the row index of the exported payload, so the
// rows keep their registration order and every node stores the descriptor of
// its own row.  The construction splits on the axis with the largest spread
// at the median; the source quadruple breaks the ties.  The left subtree of a
// node holds the rows whose descriptor is smaller than the split value on the
// split axis, the right subtree the others.
class KdBuilder {
 public:
  KdBuilder(const std::vector<std::array<double, 4>>& descriptors,
            const std::vector<std::array<U64, 4>>& source_tuples)
      : descriptors_(descriptors), tuples_(source_tuples) {}

  std::vector<KdNode> build() {
    nodes_.assign(descriptors_.size(), KdNode{});
    order_.resize(descriptors_.size());
    std::iota(order_.begin(), order_.end(), 0ull);
    if (!descriptors_.empty()) build_range(0, order_.size());
    return nodes_;
  }

  // The root of the tree, that is the node that no other node points at.
  U64 root() const {
    if (nodes_.empty()) return kNoNode;
    std::vector<bool> is_child(nodes_.size(), false);
    for (const KdNode& node : nodes_) {
      if (node.left < nodes_.size()) is_child[node.left] = true;
      if (node.right < nodes_.size()) is_child[node.right] = true;
    }
    for (std::size_t i = 0; i < is_child.size(); ++i) {
      if (!is_child[i]) return static_cast<U64>(i);
    }
    return kNoNode;
  }

 private:
  void build_range(std::size_t begin, std::size_t end) {
    if (end - begin <= 1) {
      if (begin < end) {
        const U64 node = order_[begin];
        nodes_[node].left = kNoNode;
        nodes_[node].right = kNoNode;
        nodes_[node].axis = 0;
        nodes_[node].split = descriptors_[node][0];
        for (std::size_t k = 0; k < 4; ++k) {
          nodes_[node].lower[k] = descriptors_[node][k];
          nodes_[node].upper[k] = descriptors_[node][k];
        }
      }
      return;
    }

    const std::array<double, 4> lower = min_box(begin, end);
    const std::array<double, 4> upper = max_box(begin, end);
    std::size_t axis = 0;
    double best = -1.0;
    for (std::size_t k = 0; k < 4; ++k) {
      const double spread = upper[k] - lower[k];
      if (spread > best) {
        best = spread;
        axis = k;
      }
    }

    const std::size_t mid = begin + (end - begin) / 2;
    const auto comparator = [&](U64 left, U64 right) {
      const double a = descriptors_[left][axis];
      const double b = descriptors_[right][axis];
      if (a != b) return a < b;
      return tuples_[left] < tuples_[right];
    };
    std::nth_element(order_.begin() + static_cast<std::ptrdiff_t>(begin),
                     order_.begin() + static_cast<std::ptrdiff_t>(mid),
                     order_.begin() + static_cast<std::ptrdiff_t>(end), comparator);

    const U64 node = order_[mid];
    nodes_[node].axis = static_cast<std::uint8_t>(axis);
    nodes_[node].split = descriptors_[node][axis];

    build_range(begin, mid);
    build_range(mid + 1, end);

    nodes_[node].left = mid > begin ? order_[begin + (mid - begin) / 2] : kNoNode;
    nodes_[node].right = mid + 1 < end ? order_[mid + 1 + (end - mid - 1) / 2] : kNoNode;
    for (std::size_t k = 0; k < 4; ++k) {
      nodes_[node].lower[k] = lower[k];
      nodes_[node].upper[k] = upper[k];
    }
  }

  std::array<double, 4> min_box(std::size_t begin, std::size_t end) const {
    std::array<double, 4> out{descriptors_[order_[begin]][0], descriptors_[order_[begin]][1],
                              descriptors_[order_[begin]][2], descriptors_[order_[begin]][3]};
    for (std::size_t i = begin + 1; i < end; ++i) {
      const auto& descriptor = descriptors_[order_[i]];
      for (std::size_t k = 0; k < 4; ++k) out[k] = std::min(out[k], descriptor[k]);
    }
    return out;
  }

  std::array<double, 4> max_box(std::size_t begin, std::size_t end) const {
    std::array<double, 4> out{descriptors_[order_[begin]][0], descriptors_[order_[begin]][1],
                              descriptors_[order_[begin]][2], descriptors_[order_[begin]][3]};
    for (std::size_t i = begin + 1; i < end; ++i) {
      const auto& descriptor = descriptors_[order_[i]];
      for (std::size_t k = 0; k < 4; ++k) out[k] = std::max(out[k], descriptor[k]);
    }
    return out;
  }

  const std::vector<std::array<double, 4>>& descriptors_;
  const std::vector<std::array<U64, 4>>& tuples_;
  std::vector<U64> order_;
  std::vector<KdNode> nodes_;
};

struct IndexPayload {
  std::vector<std::array<U64, 4>> quad_source_ids;
  std::vector<std::array<double, 4>> quad_descriptors;
  std::vector<double> quad_baselines;
  std::vector<double> quad_centers;
  std::vector<KdNode> kd;
};

}  // namespace

Result<Ref<CatalogueIndex>> build_catalogue_index(Context& context,
                                                    const CatalogueBuildRequest& request) {
  if (!request.columns) {
    return model::fail<Ref<CatalogueIndex>>(Code::invalid_request, "no catalogue columns supplied",
                                            "build_catalogue_index", "the index cannot be built");
  }
  const CatalogueColumns& columns = *request.columns;
  if (!columns.source_ids || !columns.astrometry || !columns.brightness_mag) {
    return model::fail<Ref<CatalogueIndex>>(Code::invalid_request,
                                            "the catalogue columns lack a mandatory array",
                                            "build_catalogue_index", "the index cannot be built");
  }
  if (columns.source_ids->dtype != DType::u64) {
    return model::fail<Ref<CatalogueIndex>>(Code::invalid_request,
                                            "the source identifier array must be u64",
                                            "build_catalogue_index", "the index cannot be built");
  }
  if (columns.astrometry->dtype != DType::f64 || columns.brightness_mag->dtype != DType::f64) {
    return model::fail<Ref<CatalogueIndex>>(Code::invalid_model,
                                            "the astrometry and brightness arrays must be f64",
                                            "build_catalogue_index", "the index cannot be built");
  }
  if (columns.source_ids->shape.size() != 1) {
    return model::fail<Ref<CatalogueIndex>>(Code::invalid_request,
                                            "the source identifier array must be one dimensional",
                                            "build_catalogue_index", "the index cannot be built");
  }
  const U64 count = columns.source_ids->shape[0];
  if (count == 0) {
    return model::fail<Ref<CatalogueIndex>>(Code::invalid_request, "the catalogue is empty",
                                            "build_catalogue_index", "the index cannot be built");
  }
  if (columns.astrometry->shape.size() != 2 || columns.astrometry->shape[0] != count ||
      columns.astrometry->shape[1] != 8) {
    return model::fail<Ref<CatalogueIndex>>(Code::invalid_model,
                                            "the astrometry array must have the shape [N,8]",
                                            "build_catalogue_index", "the index cannot be built");
  }
  if (columns.brightness_mag->shape.size() != 1 || columns.brightness_mag->shape[0] != count) {
    return model::fail<Ref<CatalogueIndex>>(Code::invalid_model,
                                            "the brightness array does not match the source count",
                                            "build_catalogue_index", "the index cannot be built");
  }
  if (request.sky_order > coordinates::kMaxOrder) {
    return model::fail<Ref<CatalogueIndex>>(Code::invalid_request, "the sky order is out of range",
                                            "build_catalogue_index", "the index cannot be built");
  }
  if (request.max_stars_per_cell == 0 || request.neighbors_per_anchor < 3 ||
      request.quads_per_anchor == 0) {
    return model::fail<Ref<CatalogueIndex>>(Code::invalid_request,
                                            "the shard and anchor budgets must be positive",
                                            "build_catalogue_index", "the index cannot be built");
  }
  const Interval baseline = request.quad_baseline_rad;
  if (!std::isfinite(baseline.lower) || !std::isfinite(baseline.upper) || baseline.lower < 0.0 ||
      baseline.upper < baseline.lower) {
    return model::fail<Ref<CatalogueIndex>>(Code::invalid_request, "the baseline range is invalid",
                                            "build_catalogue_index", "the index cannot be built");
  }

  Result<std::vector<U64>> source_ids =
      read_u64_array(context, columns.source_ids, count, "source identifier");
  if (!ok(source_ids)) {
    return model::fail<Ref<CatalogueIndex>>(source_ids.code, "the source identifiers cannot be read",
                                            "build_catalogue_index", "the index cannot be built");
  }
  Result<std::vector<double>> astrometry =
      read_f64_array(context, columns.astrometry, count * 8, "astrometry");
  if (!ok(astrometry)) {
    return model::fail<Ref<CatalogueIndex>>(astrometry.code, "the astrometry cannot be read",
                                            "build_catalogue_index", "the index cannot be built");
  }
  Result<std::vector<double>> brightness =
      read_f64_array(context, columns.brightness_mag, count, "brightness");
  if (!ok(brightness)) {
    return model::fail<Ref<CatalogueIndex>>(brightness.code, "the brightness cannot be read",
                                            "build_catalogue_index", "the index cannot be built");
  }
  if (!all_finite(val(astrometry)) || !all_finite(val(brightness))) {
    return model::fail<Ref<CatalogueIndex>>(Code::invalid_model,
                                            "the catalogue contains a non finite value",
                                            "build_catalogue_index", "the index cannot be built");
  }
  {
    const std::set<U64> unique(val(source_ids).begin(), val(source_ids).end());
    if (unique.size() != count) {
      return model::fail<Ref<CatalogueIndex>>(Code::duplicate_source,
                                              "the catalogue contains duplicate source identifiers",
                                              "build_catalogue_index", "the index cannot be built");
    }
  }

  // Work accounting: the shard pass is charged per source.
  Result<U64> work = context.resources.consume_work(count);
  if (!ok(work)) {
    return model::fail<Ref<CatalogueIndex>>(Code::resource_exhausted,
                                            "the work budget is exhausted before the index is built",
                                            "build_catalogue_index", "the index cannot be built");
  }

  std::vector<StarRow> stars;
  stars.reserve(count);
  U64 sources_without_rv = 0;
  double parallax_max = 0.0;
  for (U64 i = 0; i < count; ++i) {
    StarRow row;
    row.source_id = val(source_ids)[i];
    const double* values = &val(astrometry)[static_cast<std::size_t>(i) * 8];
    row.ra_rad = values[0];
    row.dec_rad = values[1];
    row.epoch_year = values[2];
    row.pmra_cosdec_rad_year = values[3];
    row.pmdec_rad_year = values[4];
    row.parallax_rad = values[5];
    row.rv_m_s = values[6];
    row.rv_present = values[7] != 0.0;
    row.brightness_mag = val(brightness)[i];
    if (row.rv_present) {
      if (!std::isfinite(row.rv_m_s)) {
        return model::fail<Ref<CatalogueIndex>>(Code::invalid_model,
                                                "the radial velocity flag is set but the value is not finite",
                                                "build_catalogue_index", "the index cannot be built");
      }
    } else {
      row.rv_m_s = 0.0;
      ++sources_without_rv;
    }
    if (row.dec_rad < -1.5707963267948966 || row.dec_rad > 1.5707963267948966) {
      return model::fail<Ref<CatalogueIndex>>(Code::invalid_model, "the declination is out of range",
                                              "build_catalogue_index", "the index cannot be built");
    }
    parallax_max = std::max(parallax_max, row.parallax_rad);
    row.unit = catalogue::unit_from_ra_dec(row.ra_rad, row.dec_rad);
    stars.push_back(row);
  }

  // Sharding.
  std::map<U64, std::vector<U64>> shards;
  for (U64 i = 0; i < count; ++i) {
    HealpixLeaf leaf{};
    if (!coordinates::healpix_lookup(request.sky_order,
                                     SkyICRS{stars[i].unit[0], stars[i].unit[1], stars[i].unit[2]},
                                     leaf)) {
      return model::fail<Ref<CatalogueIndex>>(Code::invalid_model,
                                              "a catalogue position is not a unit vector",
                                              "build_catalogue_index", "the index cannot be built");
    }
    shards[leaf.ipix].push_back(i);
  }

  // The brightest sources of every shard; the source identifier breaks the tie.
  std::map<U64, std::vector<U64>> selected;
  for (auto& shard : shards) {
    std::vector<U64>& rows = shard.second;
    std::sort(rows.begin(), rows.end(), [&](U64 left, U64 right) {
      if (stars[left].brightness_mag != stars[right].brightness_mag) {
        return stars[left].brightness_mag < stars[right].brightness_mag;
      }
      return stars[left].source_id < stars[right].source_id;
    });
    if (rows.size() > request.max_stars_per_cell) rows.resize(request.max_stars_per_cell);
    selected[shard.first] = rows;
  }

  std::map<std::array<U64, 4>, QuadEntry> registered;
  U64 anchors = 0;
  for (const auto& shard : selected) {
    const std::vector<HealpixLeaf> neighbours = cell_neighbours(request.sky_order, shard.first);
    std::vector<U64> pool;
    pool.insert(pool.end(), shard.second.begin(), shard.second.end());
    for (const HealpixLeaf& leaf : neighbours) {
      const auto found = selected.find(leaf.ipix);
      if (found == selected.end()) continue;
      pool.insert(pool.end(), found->second.begin(), found->second.end());
    }

    for (U64 anchor : shard.second) {
      ++anchors;
      std::vector<std::pair<double, U64>> ranked;
      ranked.reserve(pool.size());
      for (U64 candidate : pool) {
        if (candidate == anchor) continue;
        const double separation =
            catalogue::angular_separation(stars[anchor].unit, stars[candidate].unit);
        ranked.emplace_back(separation, candidate);
      }
      std::sort(ranked.begin(), ranked.end(), [&](const auto& left, const auto& right) {
        if (left.first != right.first) return left.first < right.first;
        return stars[left.second].source_id < stars[right.second].source_id;
      });
      const std::size_t neighbour_count =
          std::min<std::size_t>(ranked.size(), request.neighbors_per_anchor);
      if (neighbour_count < 3) continue;

      struct Candidate {
        double area = 0.0;
        std::array<U64, 4> ids{};
        std::array<U64, 4> rows{};
      };
      std::vector<Candidate> candidates;
      for (std::size_t i = 0; i < neighbour_count; ++i) {
        for (std::size_t j = i + 1; j < neighbour_count; ++j) {
          for (std::size_t k = j + 1; k < neighbour_count; ++k) {
            Candidate candidate;
            candidate.rows = {anchor, ranked[i].second, ranked[j].second, ranked[k].second};
            for (std::size_t p = 0; p < 4; ++p) {
              candidate.ids[p] = stars[candidate.rows[p]].source_id;
            }
            std::sort(candidate.ids.begin(), candidate.ids.end());

            Vec3 centre{0.0, 0.0, 0.0};
            for (U64 row : candidate.rows) {
              centre[0] += stars[row].unit[0];
              centre[1] += stars[row].unit[1];
              centre[2] += stars[row].unit[2];
            }
            const double norm =
                std::sqrt(centre[0] * centre[0] + centre[1] * centre[1] + centre[2] * centre[2]);
            if (!(norm > 0.0)) continue;
            centre = Vec3{centre[0] / norm, centre[1] / norm, centre[2] / norm};
            Vec3 east{};
            Vec3 north{};
            catalogue::local_basis(centre, east, north);
            std::array<Vec2, 4> points{};
            for (std::size_t p = 0; p < 4; ++p) {
              points[p] =
                  catalogue::tangent_project(centre, east, north, stars[candidate.rows[p]].unit);
            }
            double minimum_area = std::numeric_limits<double>::infinity();
            for (std::size_t p = 0; p < 4; ++p) {
              for (std::size_t q = p + 1; q < 4; ++q) {
                for (std::size_t r = q + 1; r < 4; ++r) {
                  const double ax = points[q][0] - points[p][0];
                  const double ay = points[q][1] - points[p][1];
                  const double bx = points[r][0] - points[p][0];
                  const double by = points[r][1] - points[p][1];
                  minimum_area = std::min(minimum_area, 0.5 * std::abs(ax * by - ay * bx));
                }
              }
            }
            candidate.area = minimum_area;
            candidates.push_back(candidate);
          }
        }
      }
      std::sort(candidates.begin(), candidates.end(),
                [](const Candidate& left, const Candidate& right) {
                  if (left.area != right.area) return left.area > right.area;
                  return left.ids < right.ids;
                });
      if (candidates.size() > request.quads_per_anchor) candidates.resize(request.quads_per_anchor);

      for (const Candidate& candidate : candidates) {
        double longest = 0.0;
        for (std::size_t p = 0; p < 4; ++p) {
          for (std::size_t q = p + 1; q < 4; ++q) {
            longest = std::max(longest, catalogue::angular_separation(stars[candidate.rows[p]].unit,
                                                                      stars[candidate.rows[q]].unit));
          }
        }
        // Collinear configurations are discarded.
        if (!(candidate.area > 1e-12 * longest * longest)) continue;
        if (longest < baseline.lower || longest > baseline.upper) continue;

        Vec3 centre{0.0, 0.0, 0.0};
        for (U64 row : candidate.rows) {
          centre[0] += stars[row].unit[0];
          centre[1] += stars[row].unit[1];
          centre[2] += stars[row].unit[2];
        }
        const double norm =
            std::sqrt(centre[0] * centre[0] + centre[1] * centre[1] + centre[2] * centre[2]);
        if (!(norm > 0.0)) continue;
        centre = Vec3{centre[0] / norm, centre[1] / norm, centre[2] / norm};
        Vec3 east{};
        Vec3 north{};
        catalogue::local_basis(centre, east, north);

        std::array<Vec2, 4> points{};
        std::array<U64, 4> ids{};
        for (std::size_t p = 0; p < 4; ++p) {
          points[p] =
              catalogue::tangent_project(centre, east, north, stars[candidate.rows[p]].unit);
          ids[p] = stars[candidate.rows[p]].source_id;
        }
        const catalogue::QuadDescriptor descriptor = describe_quad(points, ids);

        if (registered.count(candidate.ids) != 0) continue;

        QuadEntry entry;
        entry.sorted_ids = candidate.ids;
        entry.baseline_rad = longest;
        entry.centre = centre;
        entry.plus = descriptor.chirality_plus;
        entry.minus = descriptor.chirality_minus;
        for (std::size_t p = 0; p < 4; ++p) {
          entry.ids_plus[p] = ids[descriptor.roles_plus[p]];
          entry.ids_minus[p] = ids[descriptor.roles_minus[p]];
        }
        registered.emplace(candidate.ids, entry);
      }
    }
  }

  // Work accounting for the quad enumeration.
  Result<U64> quad_work = context.resources.consume_work(anchors * request.quads_per_anchor);
  const bool budget_exhausted = !ok(quad_work);

  // Two descriptor rows per quadruple: one per chirality convention.
  IndexPayload payload;
  std::vector<std::array<U64, 4>> tuples;
  for (const auto& item : registered) {
    const QuadEntry& entry = item.second;
    payload.quad_source_ids.push_back(entry.ids_plus);
    payload.quad_descriptors.push_back(entry.plus);
    payload.quad_baselines.push_back(entry.baseline_rad);
    payload.quad_centers.push_back(entry.centre[0]);
    payload.quad_centers.push_back(entry.centre[1]);
    payload.quad_centers.push_back(entry.centre[2]);
    tuples.push_back(entry.sorted_ids);

    payload.quad_source_ids.push_back(entry.ids_minus);
    payload.quad_descriptors.push_back(entry.minus);
    payload.quad_baselines.push_back(entry.baseline_rad);
    payload.quad_centers.push_back(entry.centre[0]);
    payload.quad_centers.push_back(entry.centre[1]);
    payload.quad_centers.push_back(entry.centre[2]);
    tuples.push_back(entry.sorted_ids);
  }

  // The KD tree over the descriptor rows.  The node index is the row index, so
  // the rows keep their registration order.
  {
    KdBuilder builder(payload.quad_descriptors, tuples);
    payload.kd = builder.build();
  }

  // Export the arrays.
  std::vector<Id> row_ids;
  row_ids.reserve(payload.quad_source_ids.size());
  for (std::size_t i = 0; i < payload.quad_source_ids.size(); ++i) {
    row_ids.push_back("quad:" + std::to_string(i));
  }
  const Index row_index = model::make_parameter_index(row_ids);
  const Id index_id = row_index.identity.object_id;

  const auto store = [&](DType dtype, const std::vector<std::byte>& bytes,
                         const std::vector<U64>& shape, const Unit& unit) -> Result<ArrayRef> {
    return context.storage.copy(context, dtype, bytes, shape, unit, index_id);
  };

  std::vector<std::byte> source_bytes;
  source_bytes.reserve(payload.quad_source_ids.size() * 4 * sizeof(U64));
  for (const auto& ids : payload.quad_source_ids) {
    for (U64 id : ids) model::append_u64_le(source_bytes, id);
  }
  Result<ArrayRef> quad_source_ids =
      store(DType::u64, source_bytes, {payload.quad_source_ids.size(), 4}, model::unit_dimensionless());
  if (!ok(quad_source_ids)) {
    return model::fail<Ref<CatalogueIndex>>(quad_source_ids.code,
                                            "the quad identifier array cannot be stored",
                                            "build_catalogue_index", "the index cannot be built");
  }

  std::vector<std::byte> descriptor_bytes;
  descriptor_bytes.reserve(payload.quad_descriptors.size() * 4 * sizeof(double));
  for (const auto& descriptor : payload.quad_descriptors) {
    for (double value : descriptor) model::append_f64_le(descriptor_bytes, value);
  }
  Result<ArrayRef> quad_descriptors = store(DType::f64, descriptor_bytes,
                                            {payload.quad_descriptors.size(), 4},
                                            model::unit_dimensionless());
  if (!ok(quad_descriptors)) {
    return model::fail<Ref<CatalogueIndex>>(quad_descriptors.code,
                                            "the quad descriptor array cannot be stored",
                                            "build_catalogue_index", "the index cannot be built");
  }

  std::vector<std::byte> baseline_bytes;
  baseline_bytes.reserve(payload.quad_baselines.size() * sizeof(double));
  for (double value : payload.quad_baselines) model::append_f64_le(baseline_bytes, value);
  Result<ArrayRef> quad_baselines =
      store(DType::f64, baseline_bytes, {payload.quad_baselines.size()}, unit_radian());
  if (!ok(quad_baselines)) {
    return model::fail<Ref<CatalogueIndex>>(quad_baselines.code,
                                            "the quad baseline array cannot be stored",
                                            "build_catalogue_index", "the index cannot be built");
  }

  std::vector<std::byte> center_bytes;
  center_bytes.reserve(payload.quad_centers.size() * sizeof(double));
  for (double value : payload.quad_centers) model::append_f64_le(center_bytes, value);
  Result<ArrayRef> quad_centers = store(DType::f64, center_bytes,
                                        {payload.quad_centers.size() / 3, 3}, model::unit_dimensionless());
  if (!ok(quad_centers)) {
    return model::fail<Ref<CatalogueIndex>>(quad_centers.code,
                                            "the quad centre array cannot be stored",
                                            "build_catalogue_index", "the index cannot be built");
  }

  std::vector<std::byte> left_bytes;
  std::vector<std::byte> right_bytes;
  std::vector<std::byte> axis_bytes;
  std::vector<std::byte> split_bytes;
  std::vector<std::byte> bounds_bytes;
  for (const KdNode& node : payload.kd) {
    model::append_u64_le(left_bytes, node.left);
    model::append_u64_le(right_bytes, node.right);
    axis_bytes.push_back(static_cast<std::byte>(node.axis));
    model::append_f64_le(split_bytes, node.split);
    for (std::size_t k = 0; k < 4; ++k) {
      model::append_f64_le(bounds_bytes, node.lower[k]);
      model::append_f64_le(bounds_bytes, node.upper[k]);
    }
  }
  const U64 node_count = static_cast<U64>(payload.kd.size());
  Result<ArrayRef> kd_left = store(DType::u64, left_bytes, {node_count}, model::unit_dimensionless());
  if (!ok(kd_left)) {
    return model::fail<Ref<CatalogueIndex>>(kd_left.code, "the KD tree cannot be stored",
                                            "build_catalogue_index", "the index cannot be built");
  }
  Result<ArrayRef> kd_right = store(DType::u64, right_bytes, {node_count}, model::unit_dimensionless());
  if (!ok(kd_right)) {
    return model::fail<Ref<CatalogueIndex>>(kd_right.code, "the KD tree cannot be stored",
                                            "build_catalogue_index", "the index cannot be built");
  }
  Result<ArrayRef> kd_axis = store(DType::u8, axis_bytes, {node_count}, model::unit_dimensionless());
  if (!ok(kd_axis)) {
    return model::fail<Ref<CatalogueIndex>>(kd_axis.code, "the KD tree cannot be stored",
                                            "build_catalogue_index", "the index cannot be built");
  }
  Result<ArrayRef> kd_split = store(DType::f64, split_bytes, {node_count}, model::unit_dimensionless());
  if (!ok(kd_split)) {
    return model::fail<Ref<CatalogueIndex>>(kd_split.code, "the KD tree cannot be stored",
                                            "build_catalogue_index", "the index cannot be built");
  }
  Result<ArrayRef> kd_bounds =
      store(DType::f64, bounds_bytes, {node_count, 4, 2}, model::unit_dimensionless());
  if (!ok(kd_bounds)) {
    return model::fail<Ref<CatalogueIndex>>(kd_bounds.code, "the KD tree cannot be stored",
                                            "build_catalogue_index", "the index cannot be built");
  }

  auto index = std::make_shared<CatalogueIndex>();
  index->metadata.release = request.release;
  index->metadata.calibration = request.calibration;
  index->metadata.xp_basis = columns.xp_basis;
  index->metadata.quad_baseline_rad = baseline;
  index->metadata.sky_order = request.sky_order;
  index->metadata.max_stars_per_cell = request.max_stars_per_cell;
  index->metadata.neighbors_per_anchor = request.neighbors_per_anchor;
  index->metadata.quads_per_anchor = request.quads_per_anchor;
  index->metadata.index_algorithm = catalogue::kCatalogueIndexAlgorithm;
  index->metadata.index_complete = true;
  index->metadata.star_count = count;
  index->metadata.quad_count = registered.size();
  index->columns = request.columns;
  index->quad_source_ids = val(quad_source_ids);
  index->quad_descriptors = val(quad_descriptors);
  index->quad_baselines = val(quad_baselines);
  index->quad_centers = val(quad_centers);
  index->kd_left = val(kd_left);
  index->kd_right = val(kd_right);
  index->kd_axis = val(kd_axis);
  index->kd_split = val(kd_split);
  index->kd_bounds = val(kd_bounds);

  // The identity binds the release, the calibration and the exported payload.
  model::Sha256 hash;
  hash.field("algorithm", catalogue::kCatalogueIndexAlgorithm);
  hash.field("release", request.release);
  hash.field("calibration", request.calibration);
  hash.field_u64("stars", count);
  hash.field_u64("quads", registered.size());
  hash.field_u64("rows", payload.quad_source_ids.size());
  hash.field_u64("sky_order", request.sky_order);
  hash.field_u64("max_stars_per_cell", request.max_stars_per_cell);
  hash.field_u64("neighbors_per_anchor", request.neighbors_per_anchor);
  hash.field_u64("quads_per_anchor", request.quads_per_anchor);
  hash.field_f64("baseline_lower", baseline.lower);
  hash.field_f64("baseline_upper", baseline.upper);
  for (const auto& ids : payload.quad_source_ids) {
    for (U64 id : ids) hash.field_u64("source", id);
  }
  for (const auto& descriptor : payload.quad_descriptors) {
    for (double value : descriptor) hash.field_f64("descriptor", value);
  }
  index->identity = model::make_identity("acsd.CatalogueIndex", 2, 0, hash.hex(), "acsd");

  index->assessment = model::make_assessment(Code::ok);
  index->assessment.complete = true;
  index->assessment.validity = Validity::certified;
  index->assessment.science = ScienceState::bounded;
  index->assessment.noise = NoiseState::unknown;
  index->assessment.condition = model::make_condition("catalogue_quad_index", "offline_index");
  index->assessment.condition.assumptions.push_back(
      "the descriptor is mirror invariant; the canonical value is the smaller of the two chirality rows");
  index->assessment.condition.assumptions.push_back(
      "quad_baseline_rad filters the angular length of the longest edge of a quadruple");
  model::set_included(index->assessment, "proper_motion_propagation");
  model::set_included(index->assessment, "parallax_propagation");
  if (sources_without_rv == 0) {
    model::set_included(index->assessment, "radial_velocity_perspective_acceleration");
  } else {
    model::set_omitted(index->assessment,
                       "radial_velocity_missing_for_" + std::to_string(sources_without_rv) +
                           "_sources:the_perspective_acceleration_is_not_applied");
  }
  model::set_omitted(index->assessment,
                     "observer_position_absent:the_catalogue_positions_are_barycentric_and_the_position"
                     "_budget_is_bounded_by_the_parallax_of_each_source");
  index->assessment.included.push_back(
      "position_budget_observer_omission_rad<=" + std::to_string(parallax_max));

  if (budget_exhausted) {
    // The work budget was exhausted: the index is explicitly incomplete and
    // must not be used as a complete search space.
    index->metadata.index_complete = false;
    model::set_omitted(index->assessment, "index_incomplete:the_work_budget_was_exhausted");
    Result<Ref<CatalogueIndex>> out =
        model::fail<Ref<CatalogueIndex>>(Code::resource_exhausted,
                                         "the work budget was exhausted while the index was built",
                                         "build_catalogue_index",
                                         "the index is not a complete search space");
    out.value = std::shared_ptr<const CatalogueIndex>(index);
    out.assessment = index->assessment;
    out.usage = context.resources.usage();
    return out;
  }

  Result<Ref<CatalogueIndex>> out = model::ok_result(std::shared_ptr<const CatalogueIndex>(index));
  out.usage = context.resources.usage();
  return out;
}

}  // namespace acsd
