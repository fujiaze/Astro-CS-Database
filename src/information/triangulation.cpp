// The single 2D Delaunay implementation of the information module and the
// supported actual-scene variance reconstruction.
//
// Scientific contract (STATISTICS_SKY_COADD section 3.1 and SYSTEM M1):
//   * the control points are projected onto a gnomonic chart with a radius of
//     at most five degrees;
//   * the triangulation is built with an incremental Bowyer-Watson insertion,
//     an initial super triangle, a fixed insertion order derived from the
//     input hash, an adjacency walk for the location and a local cavity
//     update;
//   * the orientation and the in-circle predicates are filtered in binary64
//     and evaluated exactly with rational arithmetic when the filter is not
//     conclusive; a cocircular tie is broken by the control identity, never
//     by a random joggle;
//   * a triangle is kept only when it lies completely inside the observed
//     support; collinear controls give no mesh, coincident controls with the
//     same value are merged and conflicting values are an invalid model.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <numeric>
#include <utility>
#include <vector>

#include "acsd/contracts.hpp"
#include "information/internal.hpp"
#include "model/array.hpp"
#include "model/exact.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "operators/linear_action.hpp"

namespace acsd {
namespace {

using model::Rational;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kMaxChartRadiusRad = 5.0 * M_PI / 180.0;

// ---------------------------------------------------------------------------
// Array helpers
// ---------------------------------------------------------------------------

std::size_t count_elements(const std::vector<U64>& shape) {
  std::size_t total = 1;
  for (U64 extent : shape) {
    if (extent == 0) return 0;
    total *= static_cast<std::size_t>(extent);
  }
  return total;
}

Result<std::vector<std::uint8_t>> read_u8_array(Context& context, ArrayRef array) {
  if (!array) {
    return model::fail<std::vector<std::uint8_t>>(Code::invalid_request, "no array supplied",
                                                  "information.read", "the array cannot be read");
  }
  if (array->dtype != DType::u8) {
    return model::fail<std::vector<std::uint8_t>>(Code::invalid_model, "the array is not u8",
                                                  "information.read", "the array cannot be read");
  }
  const U64 count = static_cast<U64>(count_elements(array->shape));
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, count});
  if (!ok(view)) {
    return model::fail<std::vector<std::uint8_t>>(view.code, "the array cannot be read",
                                                  "information.read", "the array cannot be read");
  }
  return model::ok_result(model::decode_u8(val(view).bytes));
}

Result<std::vector<U32>> read_u32_array(Context& context, ArrayRef array) {
  if (!array) {
    return model::fail<std::vector<U32>>(Code::invalid_request, "no array supplied",
                                         "information.read", "the array cannot be read");
  }
  if (array->dtype != DType::u32) {
    return model::fail<std::vector<U32>>(Code::invalid_model, "the array is not u32",
                                         "information.read", "the array cannot be read");
  }
  const U64 count = static_cast<U64>(count_elements(array->shape));
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, count});
  if (!ok(view)) {
    return model::fail<std::vector<U32>>(view.code, "the array cannot be read", "information.read",
                                         "the array cannot be read");
  }
  const std::span<const std::byte> bytes = val(view).bytes;
  std::vector<U32> values(bytes.size() / 4);
  for (std::size_t i = 0; i < values.size(); ++i) {
    const std::size_t base = 4 * i;
    values[i] = static_cast<U32>(static_cast<std::uint8_t>(bytes[base])) |
                (static_cast<U32>(static_cast<std::uint8_t>(bytes[base + 1])) << 8) |
                (static_cast<U32>(static_cast<std::uint8_t>(bytes[base + 2])) << 16) |
                (static_cast<U32>(static_cast<std::uint8_t>(bytes[base + 3])) << 24);
  }
  return model::ok_result(std::move(values));
}

Result<std::vector<U64>> read_u64_array(Context& context, ArrayRef array) {
  if (!array) {
    return model::fail<std::vector<U64>>(Code::invalid_request, "no array supplied",
                                         "information.read", "the array cannot be read");
  }
  if (array->dtype != DType::u64) {
    return model::fail<std::vector<U64>>(Code::invalid_model, "the array is not u64",
                                         "information.read", "the array cannot be read");
  }
  const U64 count = static_cast<U64>(count_elements(array->shape));
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, count});
  if (!ok(view)) {
    return model::fail<std::vector<U64>>(view.code, "the array cannot be read", "information.read",
                                         "the array cannot be read");
  }
  return model::ok_result(model::decode_u64(val(view).bytes));
}

Result<std::vector<double>> read_f64_array(Context& context, ArrayRef array) {
  if (!array) {
    return model::fail<std::vector<double>>(Code::invalid_request, "no array supplied",
                                            "information.read", "the array cannot be read");
  }
  if (array->dtype != DType::f64) {
    return model::fail<std::vector<double>>(Code::invalid_model, "the array is not f64",
                                            "information.read", "the array cannot be read");
  }
  const U64 count = static_cast<U64>(count_elements(array->shape));
  return operators::read_f64(context, array, 0, count);
}

// ---------------------------------------------------------------------------
// Exact geometric predicates
// ---------------------------------------------------------------------------

struct Point {
  double x = 0.0;
  double y = 0.0;
  U64 identity = 0;       // control identity used for the cocircular tie
  std::size_t control = 0;
};

// The exact conversion of a binary64 value.
Rational exact(double value) { return Rational::from_double_exact(value); }

// Sign of the orientation determinant, evaluated exactly when the binary64
// filter is not conclusive.
int orientation_sign(const Point& a, const Point& b, const Point& c) {
  const double left = (b.x - a.x) * (c.y - a.y);
  const double right = (b.y - a.y) * (c.x - a.x);
  const double determinant = left - right;
  const double scale = std::fabs(left) + std::fabs(right);
  const double error = 4.0 * std::numeric_limits<double>::epsilon() * scale;
  if (determinant > error) return 1;
  if (determinant < -error) return -1;
  const Rational bx = exact(b.x) - exact(a.x);
  const Rational by = exact(b.y) - exact(a.y);
  const Rational cx = exact(c.x) - exact(a.x);
  const Rational cy = exact(c.y) - exact(a.y);
  const Rational value = bx * cy - by * cx;
  if (value.is_zero()) return 0;
  return value.sign();
}

// Sign of the in-circle determinant, evaluated exactly when the binary64
// filter is not conclusive.  A positive value means that d lies inside the
// circumcircle of the counter clockwise triangle (a,b,c).
int incircle_sign(const Point& a, const Point& b, const Point& c, const Point& d) {
  const double ax = a.x - d.x;
  const double ay = a.y - d.y;
  const double bx = b.x - d.x;
  const double by = b.y - d.y;
  const double cx = c.x - d.x;
  const double cy = c.y - d.y;
  const double a2 = ax * ax + ay * ay;
  const double b2 = bx * bx + by * by;
  const double c2 = cx * cx + cy * cy;
  const double determinant = ax * (by * c2 - b2 * cy) - ay * (bx * c2 - b2 * cx) +
                            a2 * (bx * cy - by * cx);
  double scale = 0.0;
  for (double value : {ax, ay, bx, by, cx, cy, a2, b2, c2}) scale += std::fabs(value);
  const double error = 1e-10 * scale * scale * scale + 1e-300;
  if (determinant > error) return 1;
  if (determinant < -error) return -1;
  const Rational rax = exact(a.x) - exact(d.x);
  const Rational ray = exact(a.y) - exact(d.y);
  const Rational rbx = exact(b.x) - exact(d.x);
  const Rational rby = exact(b.y) - exact(d.y);
  const Rational rcx = exact(c.x) - exact(d.x);
  const Rational rcy = exact(c.y) - exact(d.y);
  const Rational ra2 = rax * rax + ray * ray;
  const Rational rb2 = rbx * rbx + rby * rby;
  const Rational rc2 = rcx * rcx + rcy * rcy;
  const Rational value = rax * (rby * rc2 - rb2 * rcy) - ray * (rbx * rc2 - rb2 * rcx) +
                         ra2 * (rbx * rcy - rby * rcx);
  if (value.is_zero()) return 0;
  return value.sign();
}

// ---------------------------------------------------------------------------
// Bowyer-Watson triangulation
// ---------------------------------------------------------------------------

struct Triangle {
  U64 a = 0;
  U64 b = 0;
  U64 c = 0;
  int n0 = -1;  // neighbour opposite a
  int n1 = -1;  // neighbour opposite b
  int n2 = -1;  // neighbour opposite c
  bool alive = true;
};

class BowyerWatson {
 public:
  explicit BowyerWatson(std::vector<Point> points) : points_(std::move(points)) {}

  // Builds the mesh; the super triangle vertices are removed afterwards.
  Result<U64> build(Context& context) {
    const std::size_t count = points_.size();
    if (count < 3) {
      return model::fail<U64>(Code::unavailable, "fewer than three control points",
                              "information.triangulation", "no mesh can be built");
    }
    // Super triangle that strictly contains every point.
    double extent = 0.0;
    for (const Point& point : points_) {
      extent = std::max(extent, std::max(std::fabs(point.x), std::fabs(point.y)));
    }
    extent = std::max(extent, 1e-6) * 100.0;
    const U64 super_a = static_cast<U64>(count);
    const U64 super_b = static_cast<U64>(count + 1);
    const U64 super_c = static_cast<U64>(count + 2);
    points_.push_back(Point{-extent, -extent, 0, count});
    points_.push_back(Point{extent, -extent, 0, count + 1});
    points_.push_back(Point{0.0, 2.0 * extent, 0, count + 2});

    Triangle root;
    root.a = super_a;
    root.b = super_b;
    root.c = super_c;
    if (orientation_sign(points_[root.a], points_[root.b], points_[root.c]) < 0) {
      std::swap(root.b, root.c);
    }
    triangles_.push_back(root);
    int current = 0;

    for (std::size_t index = 0; index < count; ++index) {
      // The cancellation checkpoint of the insertion: the mesh stops with a
      // named failure and without a partial mesh.
      if (context.cancellation.requested()) {
        return model::fail<U64>(Code::cancelled, "cancelled", "information.triangulation",
                                 "the mesh is cancelled");
      }
      Result<U64> consumed = context.resources.consume_work(1);
      if (!ok(consumed)) {
        return model::fail<U64>(consumed.code, "the work budget is exhausted",
                                "information.triangulation", "the mesh is incomplete");
      }
      if (!insert(index, current)) {
        return model::fail<U64>(Code::unsupported, "the incremental insertion failed",
                                "information.triangulation", "the mesh is incomplete");
      }
      current = last_created_;
    }

    // Drop every triangle that still uses a super triangle vertex.
    std::vector<U64> result;
    result.reserve(triangles_.size());
    for (const Triangle& triangle : triangles_) {
      if (!triangle.alive) continue;
      if (triangle.a >= count || triangle.b >= count || triangle.c >= count) continue;
      result.push_back(triangle.a);
      result.push_back(triangle.b);
      result.push_back(triangle.c);
    }
    triangles_ = triangles_;
    mesh_.clear();    mesh_.reserve(result.size() / 3);
    for (std::size_t i = 0; i + 2 < result.size(); i += 3) {
      mesh_.push_back({result[i], result[i + 1], result[i + 2]});
    }
    points_.resize(count);
    return model::ok_result(static_cast<U64>(mesh_.size()));
  }

  const std::vector<Point>& points() const { return points_; }
  const std::vector<std::array<U64, 3>>& mesh() const { return mesh_; }

 private:
  bool inside_circumcircle(const Triangle& triangle, const Point& point) const {
    const int orientation = orientation_sign(points_[triangle.a], points_[triangle.b],
                                             points_[triangle.c]);
    if (orientation == 0) return false;
    const int circle = incircle_sign(points_[triangle.a], points_[triangle.b], points_[triangle.c],
                                      point);
    if (circle == 0) {
      // Cocircular: the tie is broken by the control identity, never by a
      // random joggle.  The point joins the cavity when its identity is
      // larger than the smallest identity of the triangle.
      const U64 smallest = std::min({points_[triangle.a].identity, points_[triangle.b].identity,
                                     points_[triangle.c].identity});
      return point.identity > smallest;
    }
    return (circle > 0) == (orientation > 0);
  }

  int locate(const Point& point, int start) const {
    int current = start;
    for (int step = 0; step < 4096; ++step) {
      if (current < 0 || current >= static_cast<int>(triangles_.size())) return -1;
      const Triangle& triangle = triangles_[static_cast<std::size_t>(current)];
      if (!triangle.alive) return -1;
      const int s0 = orientation_sign(points_[triangle.a], points_[triangle.b], point);
      const int s1 = orientation_sign(points_[triangle.b], points_[triangle.c], point);
      const int s2 = orientation_sign(points_[triangle.c], points_[triangle.a], point);
      if (s0 >= 0 && s1 >= 0 && s2 >= 0) return current;
      // The neighbour that is opposite a vertex lies across the edge that
      // does not contain that vertex: s0 tests the edge (a,b), s1 the edge
      // (b,c) and s2 the edge (c,a).
      int next = -1;
      if (s0 < 0) {
        next = triangle.n2;
      } else if (s1 < 0) {
        next = triangle.n0;
      } else {
        next = triangle.n1;
      }
      if (next < 0) return -1;
      current = next;
    }
    return -1;
  }

  // The triangle that contains a point.  The walk from the last created
  // triangle is the fast path; when it does not reach the point, every alive
  // triangle is examined, so that a degenerate configuration of the mesh does
  // not stop the insertion.
  int locate_anywhere(const Point& point) const {
    for (std::size_t index = 0; index < triangles_.size(); ++index) {
      const Triangle& triangle = triangles_[index];
      if (!triangle.alive) continue;
      const int s0 = orientation_sign(points_[triangle.a], points_[triangle.b], point);
      const int s1 = orientation_sign(points_[triangle.b], points_[triangle.c], point);
      const int s2 = orientation_sign(points_[triangle.c], points_[triangle.a], point);
      if (s0 >= 0 && s1 >= 0 && s2 >= 0) return static_cast<int>(index);
    }
    return -1;
  }

  bool insert(std::size_t index, int& current) {
    const Point point = points_[index];
    int located = locate(point, current);
    if (located < 0) located = locate_anywhere(point);
    if (located < 0) return false;
    // Expand the cavity over the adjacency.
    std::vector<int> cavity;
    std::vector<char> in_cavity(triangles_.size(), 0);
    std::vector<int> stack{located};
    in_cavity[static_cast<std::size_t>(located)] = 1;
    while (!stack.empty()) {
      const int triangle_index = stack.back();
      stack.pop_back();
      cavity.push_back(triangle_index);
      const Triangle& triangle = triangles_[static_cast<std::size_t>(triangle_index)];
      const int neighbours[3] = {triangle.n0, triangle.n1, triangle.n2};
      for (int neighbour : neighbours) {
        if (neighbour < 0) continue;
        if (in_cavity[static_cast<std::size_t>(neighbour)]) continue;
        if (!triangles_[static_cast<std::size_t>(neighbour)].alive) continue;
        if (inside_circumcircle(triangles_[static_cast<std::size_t>(neighbour)], point)) {
          in_cavity[static_cast<std::size_t>(neighbour)] = 1;
          stack.push_back(neighbour);
        }
      }
    }
    // Collect the boundary edges of the cavity.
    struct Boundary {
      U64 u = 0;
      U64 v = 0;
      int outside = -1;
    };
    std::vector<Boundary> boundary;
    for (int triangle_index : cavity) {
      Triangle& triangle = triangles_[static_cast<std::size_t>(triangle_index)];
      const U64 vertices[3] = {triangle.a, triangle.b, triangle.c};
      const int neighbours[3] = {triangle.n0, triangle.n1, triangle.n2};
      for (int k = 0; k < 3; ++k) {
        const int neighbour = neighbours[k];
        if (neighbour >= 0 && in_cavity[static_cast<std::size_t>(neighbour)]) continue;
        Boundary edge;
        edge.u = vertices[(k + 1) % 3];
        edge.v = vertices[(k + 2) % 3];
        edge.outside = neighbour;
        boundary.push_back(edge);
      }
      triangle.alive = false;
    }
    if (boundary.empty()) return false;
    // Create the new triangles and link them.
    std::map<std::pair<U64, U64>, std::vector<int>> edge_map;
    std::vector<int> created;
    created.reserve(boundary.size());
    for (const Boundary& edge : boundary) {
      Triangle triangle;
      triangle.a = edge.u;
      triangle.b = edge.v;
      triangle.c = static_cast<U64>(index);
      if (orientation_sign(points_[triangle.a], points_[triangle.b], points_[triangle.c]) < 0) {
        std::swap(triangle.a, triangle.b);
      }
      triangles_.push_back(triangle);
      const int new_index = static_cast<int>(triangles_.size()) - 1;
      created.push_back(new_index);
      // The neighbour opposite the inserted point is the triangle that shared
      // the boundary edge.
      Triangle& fresh = triangles_[static_cast<std::size_t>(new_index)];
      const U64 vertices[3] = {fresh.a, fresh.b, fresh.c};
      int slot = 0;
      for (int j = 0; j < 3; ++j) {
        if (vertices[j] == static_cast<U64>(index)) slot = j;
      }
      if (slot == 0) {
        fresh.n0 = edge.outside;
      } else if (slot == 1) {
        fresh.n1 = edge.outside;
      } else {
        fresh.n2 = edge.outside;
      }
      // Repair the back reference of the outside neighbour.
      if (edge.outside >= 0) {
        Triangle& outside = triangles_[static_cast<std::size_t>(edge.outside)];
        const U64 outside_vertices[3] = {outside.a, outside.b, outside.c};
        for (int j = 0; j < 3; ++j) {
          const U64 opposite = outside_vertices[(j + 1) % 3];
          const U64 opposite2 = outside_vertices[(j + 2) % 3];
          const bool shares = (opposite == edge.u && opposite2 == edge.v) ||
                              (opposite == edge.v && opposite2 == edge.u);
          if (!shares) continue;
          if (j == 0) {
            outside.n0 = new_index;
          } else if (j == 1) {
            outside.n1 = new_index;
          } else {
            outside.n2 = new_index;
          }
          break;
        }
      }
      // Both edges that contain the inserted point are shared with the
      // neighbouring triangle of the fan.
      for (int j = 0; j < 3; ++j) {
        if (vertices[j] != static_cast<U64>(index)) continue;
        for (int k = 1; k <= 2; ++k) {
          const U64 other = vertices[(j + k) % 3];
          const std::pair<U64, U64> key(std::min(other, static_cast<U64>(index)),
                                        std::max(other, static_cast<U64>(index)));
          edge_map[key].push_back(new_index);
        }
      }
    }
    for (std::size_t k = 0; k < created.size(); ++k) {
      Triangle& triangle = triangles_[static_cast<std::size_t>(created[k])];
      const U64 vertices[3] = {triangle.a, triangle.b, triangle.c};
      int slot = 0;
      for (int j = 0; j < 3; ++j) {
        if (vertices[j] == static_cast<U64>(index)) slot = j;
      }
      const U64 opposite = vertices[(slot + 1) % 3];
      const U64 opposite2 = vertices[(slot + 2) % 3];
      const std::pair<U64, U64> key1(std::min(opposite, static_cast<U64>(index)),
                                     std::max(opposite, static_cast<U64>(index)));
      const std::pair<U64, U64> key2(std::min(opposite2, static_cast<U64>(index)),
                                     std::max(opposite2, static_cast<U64>(index)));
      auto neighbour_across = [&](const std::pair<U64, U64>& key) -> int {
        auto found = edge_map.find(key);
        if (found == edge_map.end()) return -1;
        for (int candidate : found->second) {
          if (candidate != created[k]) return candidate;
        }
        return -1;
      };
      const int neighbour1 = neighbour_across(key1);
      const int neighbour2 = neighbour_across(key2);
      if (slot == 0) {
        // The edge (p, opposite) is opposite the third vertex.
        triangle.n1 = neighbour2;
        triangle.n2 = neighbour1;
      } else if (slot == 1) {
        triangle.n0 = neighbour1;
        triangle.n2 = neighbour2;
      } else {
        triangle.n0 = neighbour2;
        triangle.n1 = neighbour1;
      }
    }
    last_created_ = created.empty() ? current : created.front();
    current = last_created_;
    return true;
  }

  std::vector<Point> points_;
  std::vector<Triangle> triangles_;
  std::vector<std::array<U64, 3>> mesh_;
  int last_created_ = 0;
};

// ---------------------------------------------------------------------------
// Support containment
// ---------------------------------------------------------------------------

struct SupportModel {
  bool usable = false;
  std::string reason;
  bool has_states = false;
  U32 order = 0;
  std::vector<std::pair<U64, U64>> leaves;   // (order, ipix)
  std::vector<std::uint8_t> states;          // 0 absent, 1 positive, 2 undecided
  bool has_cells = false;
  std::vector<std::uint8_t> cell_face;
  std::vector<U32> cell_depth;
  std::vector<U64> cell_x;
  std::vector<U64> cell_y;
  std::vector<std::uint8_t> cell_state;
};

bool boxes_overlap(double ax0, double ay0, double ax1, double ay1, double bx0, double by0,
                   double bx1, double by1) {
  return !(ax1 < bx0 || bx1 < ax0 || ay1 < by0 || by1 < ay0);
}

}  // namespace

Result<Ref<SupportedTriangulation>> build_supported_triangulation(
    Context& context, Ref<PrecisionControls> controls, Chart chart, Ref<SupportMap> support) {
  if (!controls) {
    return model::fail<Ref<SupportedTriangulation>>(Code::invalid_request,
                                                    "no precision controls supplied",
                                                    "build_supported_triangulation",
                                                    "no mesh can be built");
  }
  if (!support) {
    return model::fail<Ref<SupportedTriangulation>>(Code::invalid_request,
                                                    "no support map supplied",
                                                    "build_supported_triangulation",
                                                    "no mesh can be built");
  }
  if (controls->kind != PrecisionControls::Kind::actual_scene) {
    return model::fail<Ref<SupportedTriangulation>>(
        Code::invalid_request, "the controls are not actual scene controls",
        "build_supported_triangulation",
        "a reference source control cannot be renamed into an actual scene variance");
  }
  if (!(chart.maximum_radius_rad > 0.0) || chart.maximum_radius_rad > kMaxChartRadiusRad) {
    return model::fail<Ref<SupportedTriangulation>>(
        Code::unsupported, "the chart radius is not supported by the first implementation",
        "build_supported_triangulation", "no mesh can be built");
  }
  information::ChartProjection projection;
  if (!information::make_chart_projection(chart, projection)) {
    return model::fail<Ref<SupportedTriangulation>>(Code::invalid_model, "the chart is not usable",
                                                    "build_supported_triangulation",
                                                    "no mesh can be built");
  }
  Result<std::vector<double>> positions = read_f64_array(context, controls->positions);
  if (!ok(positions)) {
    return model::fail<Ref<SupportedTriangulation>>(positions.code,
                                                    "the control positions cannot be read",
                                                    "build_supported_triangulation",
                                                    "no mesh can be built");
  }
  Result<std::vector<double>> values = read_f64_array(context, controls->values);
  if (!ok(values)) {
    return model::fail<Ref<SupportedTriangulation>>(values.code,
                                                    "the control values cannot be read",
                                                    "build_supported_triangulation",
                                                    "no mesh can be built");
  }
  Result<std::vector<double>> log_errors = read_f64_array(context, controls->log_errors);
  if (!ok(log_errors)) {
    return model::fail<Ref<SupportedTriangulation>>(log_errors.code,
                                                    "the control errors cannot be read",
                                                    "build_supported_triangulation",
                                                    "no mesh can be built");
  }
  const std::size_t count = val(values).size();
  if (count != val(log_errors).size() || count == 0) {
    return model::fail<Ref<SupportedTriangulation>>(Code::invalid_model,
                                                    "the control arrays disagree",
                                                    "build_supported_triangulation",
                                                    "no mesh can be built");
  }
  const std::size_t width = val(positions).size() / count;
  if (width != 2 && width != 3) {
    return model::fail<Ref<SupportedTriangulation>>(Code::invalid_model,
                                                    "the control positions have an unexpected shape",
                                                    "build_supported_triangulation",
                                                    "no mesh can be built");
  }
  for (double value : val(values)) {
    if (!(value > 0.0) || !std::isfinite(value)) {
      return model::fail<Ref<SupportedTriangulation>>(
          Code::invalid_model, "a control variance is not a positive finite number",
          "build_supported_triangulation", "the log interpolation is not defined");
    }
  }
  for (double value : val(log_errors)) {
    if (!(value >= 0.0) || !std::isfinite(value)) {
      return model::fail<Ref<SupportedTriangulation>>(
          Code::invalid_model, "a control error bound is not a non negative finite number",
          "build_supported_triangulation", "the error bound is not usable");
    }
  }

  // ---- project the controls onto the chart --------------------------------
  std::vector<Point> points;
  points.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    Vec2 chart_point{0.0, 0.0};
    if (width == 2) {
      chart_point = Vec2{val(positions)[2 * i], val(positions)[2 * i + 1]};
    } else {
      const SkyICRS direction{val(positions)[3 * i], val(positions)[3 * i + 1],
                              val(positions)[3 * i + 2]};
      if (!information::project_to_chart(projection, direction, chart_point)) {
        return model::fail<Ref<SupportedTriangulation>>(
            Code::domain, "a control point lies outside the chart",
            "build_supported_triangulation", "the control cannot be placed on the chart");
      }
    }
    Point point;
    point.x = chart_point[0];
    point.y = chart_point[1];
    point.identity = static_cast<U64>(i);
    point.control = i;
    points.push_back(point);
  }

  // ---- merge coincident controls ------------------------------------------
  std::vector<Point> merged;
  std::vector<std::size_t> merged_index(count, 0);
  for (std::size_t i = 0; i < points.size(); ++i) {
    bool found = false;
    for (std::size_t j = 0; j < merged.size(); ++j) {
      if (std::fabs(merged[j].x - points[i].x) <= 1e-15 &&
          std::fabs(merged[j].y - points[i].y) <= 1e-15) {
        const double a = val(values)[merged[j].control];
        const double b = val(values)[points[i].control];
        if (a != b) {
          return model::fail<Ref<SupportedTriangulation>>(
              Code::invalid_model, "coincident controls carry conflicting values",
              "build_supported_triangulation", "the control set is inconsistent");
        }
        merged_index[i] = j;
        found = true;
        break;
      }
    }
    if (!found) {
      merged_index[i] = merged.size();
      merged.push_back(points[i]);
    }
  }
  if (merged.size() < 3) {
    return model::fail<Ref<SupportedTriangulation>>(Code::unavailable,
                                                    "fewer than three distinct control points",
                                                    "build_supported_triangulation",
                                                    "no mesh can be built");
  }

  // ---- fixed insertion order from the input hash ---------------------------
  std::vector<std::pair<std::string, std::size_t>> order;
  order.reserve(merged.size());
  for (std::size_t i = 0; i < merged.size(); ++i) {
    model::Sha256 hash;
    hash.update_f64(merged[i].x);
    hash.update_f64(merged[i].y);
    hash.update_f64(val(values)[merged[i].control]);
    order.emplace_back(hash.hex(), i);
  }
  std::sort(order.begin(), order.end());

  // ---- build the mesh ------------------------------------------------------
  std::vector<Point> ordered;
  ordered.reserve(merged.size());
  for (const auto& entry : order) {
    Point point = merged[entry.second];
    point.identity = static_cast<U64>(entry.second);
    ordered.push_back(point);
  }
  BowyerWatson builder(ordered);
  Result<U64> built = builder.build(context);
  if (!ok(built)) {
    return model::fail<Ref<SupportedTriangulation>>(built.code,
                                                    "the triangulation could not be built",
                                                    "build_supported_triangulation",
                                                    "no mesh is available");
  }
  const std::vector<std::array<U64, 3>>& mesh = builder.mesh();
  if (mesh.empty()) {
    // Collinear or degenerate controls give no mesh.
    return model::fail<Ref<SupportedTriangulation>>(Code::unavailable,
                                                    "the control points are collinear or degenerate",
                                                    "build_supported_triangulation",
                                                    "no mesh is available");
  }

  // ---- support containment -------------------------------------------------
  SupportModel support_model;
  if (support->states && support->index) {
    Result<std::vector<std::uint8_t>> states = read_u8_array(context, support->states);
    if (!ok(states)) {
      return model::fail<Ref<SupportedTriangulation>>(states.code, "the support states cannot be read",
                                                      "build_supported_triangulation",
                                                      "the mesh is not available");
    }
    Result<std::vector<U64>> ids = read_u64_array(context, support->index->ids);
    if (!ok(ids)) {
      return model::fail<Ref<SupportedTriangulation>>(ids.code,
                                                      "the support leaf identities cannot be read",
                                                      "build_supported_triangulation",
                                                      "the mesh is not available");
    }
    const std::vector<U64>& flat = val(ids);
    if (flat.size() != 2 * val(states).size()) {
      return model::fail<Ref<SupportedTriangulation>>(Code::invalid_model,
                                                      "the support index and states disagree",
                                                      "build_supported_triangulation",
                                                      "the mesh is not available");
    }
    support_model.has_states = true;
    support_model.states = val(states);
    support_model.order = support->index->order;
    for (std::size_t row = 0; row < val(states).size(); ++row) {
      support_model.leaves.emplace_back(flat[2 * row], flat[2 * row + 1]);
    }
  }
  if (support->cell_face && support->cell_state) {
    Result<std::vector<std::uint8_t>> faces = read_u8_array(context, support->cell_face);
    Result<std::vector<std::uint8_t>> states = read_u8_array(context, support->cell_state);
    Result<std::vector<U32>> depths = read_u32_array(context, support->cell_depth);
    Result<std::vector<U64>> xs = read_u64_array(context, support->cell_x);
    Result<std::vector<U64>> ys = read_u64_array(context, support->cell_y);
    if (!ok(faces) || !ok(states) || !ok(depths) || !ok(xs) || !ok(ys)) {
      return model::fail<Ref<SupportedTriangulation>>(Code::invalid_model,
                                                      "the support cells cannot be read",
                                                      "build_supported_triangulation",
                                                      "the mesh is not available");
    }
    const std::size_t cells = val(states).size();
    if (val(faces).size() != cells || val(depths).size() != cells || val(xs).size() != cells ||
        val(ys).size() != cells) {
      return model::fail<Ref<SupportedTriangulation>>(Code::invalid_model,
                                                      "the support cell arrays disagree",
                                                      "build_supported_triangulation",
                                                      "the mesh is not available");
    }
    support_model.has_cells = true;
    support_model.cell_face = val(faces);
    support_model.cell_depth = val(depths);
    support_model.cell_x = val(xs);
    support_model.cell_y = val(ys);
    support_model.cell_state = val(states);
  }
  if (!support_model.has_states && !support_model.has_cells) {
    return model::fail<Ref<SupportedTriangulation>>(
        Code::unavailable,
        "the support map exposes neither leaf states nor dyadic cells, so a triangle cannot be "
        "certified to lie inside the observed support",
        "build_supported_triangulation", "the mesh is not available");
  }

  const std::size_t vertex_count = builder.points().size();
  const double leaf_radius = support_model.has_states
                                  ? information::local_leaf_radius_rad(support_model.order)
                                  : 0.0;
  information::ChartProjection wide = projection;
  wide.maximum_radius_rad = projection.maximum_radius_rad + leaf_radius + 1e-9;

  auto triangle_inside_support = [&](const std::array<U64, 3>& triangle) -> bool {
    double x0 = 0.0;
    double y0 = 0.0;
    double x1 = 0.0;
    double y1 = 0.0;
    for (std::size_t k = 0; k < 3; ++k) {
      const double x = builder.points()[triangle[k]].x;
      const double y = builder.points()[triangle[k]].y;
      if (k == 0) {
        x0 = x1 = x;
        y0 = y1 = y;
      } else {
        x0 = std::min(x0, x);
        x1 = std::max(x1, x);
        y0 = std::min(y0, y);
        y1 = std::max(y1, y);
      }
    }
    const double margin = leaf_radius + 1e-12;
    const double ax0 = x0 - margin;
    const double ay0 = y0 - margin;
    const double ax1 = x1 + margin;
    const double ay1 = y1 + margin;
    if (support_model.has_states) {
      // The leaves that carry the vertices of the triangle must be supported
      // as well; a leaf that the support does not list at all is not supported.
      for (std::size_t k = 0; k < 3; ++k) {
        Vec2 chart_point{builder.points()[triangle[k]].x, builder.points()[triangle[k]].y};
        SkyICRS direction;
        if (!information::unproject_from_chart(projection, chart_point, direction)) continue;
        U64 ipix = 0;
        if (!information::local_leaf_lookup(support_model.order, direction, ipix)) continue;
        bool found = false;
        for (std::size_t row = 0; row < support_model.leaves.size(); ++row) {
          if (support_model.leaves[row].first != support_model.order) continue;
          if (support_model.leaves[row].second != ipix) continue;
          found = true;
          if (support_model.states[row] != 1) return false;
          break;
        }
        if (!found) return false;
      }
      for (std::size_t row = 0; row < support_model.leaves.size(); ++row) {
        const U64 order = support_model.leaves[row].first;
        const U64 ipix = support_model.leaves[row].second;
        if (order != support_model.order) continue;
        const SkyICRS center = information::local_leaf_center(static_cast<U32>(order), ipix);
        Vec2 chart_point{0.0, 0.0};
        if (!information::project_to_chart(wide, center, chart_point)) continue;
        if (chart_point[0] < ax0 || chart_point[0] > ax1 || chart_point[1] < ay0 ||
            chart_point[1] > ay1) {
          continue;
        }
        // The leaf overlaps the triangle, so it must be inside the support.
        if (support_model.states[row] != 1) return false;
      }
    }
    if (support_model.has_cells) {
      for (std::size_t cell = 0; cell < support_model.cell_state.size(); ++cell) {
        if (support_model.cell_state[cell] == 1) continue;
        const U32 face = support_model.cell_face[cell];
        const U32 depth = support_model.cell_depth[cell];
        if (depth > 30) return false;
        const double scale = 1.0 / static_cast<double>(1ull << depth);
        const double s0 = static_cast<double>(support_model.cell_x[cell]) * scale;
        const double s1 = s0 + scale;
        const double t0 = static_cast<double>(support_model.cell_y[cell]) * scale;
        const double t1 = t0 + scale;
        double cx0 = 0.0;
        double cy0 = 0.0;
        double cx1 = 0.0;
        double cy1 = 0.0;
        const double corners[4][2] = {{s0, t0}, {s1, t0}, {s1, t1}, {s0, t1}};
        bool projected = true;
        for (const auto& corner : corners) {
          const SkyICRS direction = information::local_face_point(face, corner[0], corner[1]);
          Vec2 chart_point{0.0, 0.0};
          if (!information::project_to_chart(wide, direction, chart_point)) {
            projected = false;
            break;
          }
          if (corner == corners[0]) {
            cx0 = cx1 = chart_point[0];
            cy0 = cy1 = chart_point[1];
          } else {
            cx0 = std::min(cx0, chart_point[0]);
            cx1 = std::max(cx1, chart_point[0]);
            cy0 = std::min(cy0, chart_point[1]);
            cy1 = std::max(cy1, chart_point[1]);
          }
        }
        if (!projected) return false;
        if (boxes_overlap(ax0, ay0, ax1, ay1, cx0, cy0, cx1, cy1)) return false;
      }
    }
    return true;
  };

  std::vector<std::array<U64, 3>> kept;
  kept.reserve(mesh.size());
  for (const std::array<U64, 3>& triangle : mesh) {
    if (triangle[0] >= vertex_count || triangle[1] >= vertex_count || triangle[2] >= vertex_count) {
      continue;
    }
    if (!triangle_inside_support(triangle)) continue;
    kept.push_back(triangle);
  }
  if (kept.empty()) {
    return model::fail<Ref<SupportedTriangulation>>(
        Code::unavailable, "no triangle lies completely inside the observed support",
        "build_supported_triangulation", "no mesh is available");
  }

  // ---- publish the mesh ----------------------------------------------------
  std::vector<double> vertex_array;
  vertex_array.reserve(vertex_count * 2);
  for (const Point& point : builder.points()) {
    vertex_array.push_back(point.x);
    vertex_array.push_back(point.y);
  }
  std::vector<U64> triangle_array;
  triangle_array.reserve(kept.size() * 3);
  for (const std::array<U64, 3>& triangle : kept) {
    triangle_array.push_back(triangle[0]);
    triangle_array.push_back(triangle[1]);
    triangle_array.push_back(triangle[2]);
  }
  const Id index_id = "chart:" + controls->estimator.id;
  Result<ArrayRef> vertices = operators::write_f64(
      context, vertex_array, std::vector<U64>{static_cast<U64>(vertex_count), 2},
      model::unit_dimensionless(), index_id);
  if (!ok(vertices)) {
    return model::fail<Ref<SupportedTriangulation>>(vertices.code,
                                                    "the chart vertices cannot be stored",
                                                    "build_supported_triangulation",
                                                    "the mesh is not available");
  }
  const std::vector<std::byte> triangle_bytes = model::encode_u64(triangle_array);
  Result<ArrayRef> triangle_ref = context.storage.copy(
      context, DType::u64, triangle_bytes,
      std::vector<U64>{static_cast<U64>(kept.size()), 3}, model::unit_dimensionless(), index_id);
  if (!ok(triangle_ref)) {
    return model::fail<Ref<SupportedTriangulation>>(triangle_ref.code,
                                                    "the triangles cannot be stored",
                                                    "build_supported_triangulation",
                                                    "the mesh is not available");
  }

  // The holes of the mesh are the leaves of the support map without observed
  // support.  Their boundary is the outline of the leaf in the chart, so a
  // query inside such a leaf is unavailable instead of being filled with the
  // nearest control.
  std::vector<Ring> holes;
  if (support_model.has_states) {
    for (std::size_t row = 0; row < support_model.states.size(); ++row) {
      if (support_model.states[row] == 1) continue;
      const U64 order = support_model.leaves[row].first;
      const U64 ipix = support_model.leaves[row].second;
      std::vector<SkyICRS> corners;
      information::local_leaf_corners(static_cast<U32>(order), ipix, corners);
      Ring ring;
      ring.hole = true;
      bool usable = true;
      for (const SkyICRS& corner : corners) {
        Vec2 point{0.0, 0.0};
        if (!information::project_to_chart_unbounded(projection, corner, point)) {
          usable = false;
          break;
        }
        ring.vertices.push_back(PixelEdge0{point[0], point[1]});
      }
      if (usable && ring.vertices.size() >= 3) holes.push_back(std::move(ring));
    }
  }

  auto mesh_model = std::make_shared<SupportedTriangulation>();
  mesh_model->chart = chart;
  mesh_model->vertices = val(vertices);
  mesh_model->triangles = val(triangle_ref);
  mesh_model->holes = std::move(holes);
  mesh_model->identity = model::make_identity("acsd.SupportedTriangulation", 2, 0,
                                              model::hash_payload(DType::f64,
                                                                  {static_cast<U64>(vertex_count), 2},
                                                                  model::unit_dimensionless(),
                                                                  model::encode_f64(vertex_array)),
                                              "acsd");
  mesh_model->assessment = model::make_assessment(Code::ok);
  mesh_model->assessment.complete = true;
  mesh_model->assessment.condition = controls->condition;

  Result<Ref<SupportedTriangulation>> out =
      model::ok_result(std::shared_ptr<const SupportedTriangulation>(mesh_model));
  out.assessment.condition = controls->condition;
  model::set_included(out.assessment, "gnomonic_chart_projection");
  model::set_included(out.assessment, "exact_geometric_predicates");
  model::set_included(out.assessment, "support_containment_test");
  model::set_included(out.assessment, "holes_from_the_leaves_without_observed_support");
  model::set_included(out.assessment, "hole_count_" + std::to_string(mesh_model->holes.size()));
  if (!mesh_model->holes.empty()) {
    // The boundary of every hole is the outline of the leaf without observed
    // support in the chart of the mesh.
    model::set_included(out.assessment, "hole_boundary_is_the_outline_of_the_unsupported_leaf");
  }
  return out;
}

}  // namespace acsd
