#include "geometry/exact_polygon.hpp"

#include <cmath>

namespace acsd::geometry {
namespace {

// Sign of a*x + b*y - c evaluated exactly.
int side_of(const RationalPoint& point, const ExactFraction& a, const ExactFraction& b,
            const ExactFraction& c) {
  const ExactFraction value = a * point.x + b * point.y - c;
  return value.sign();
}

}  // namespace

RationalPolygon clip_half_space(const RationalPolygon& polygon, const ExactFraction& a,
                                const ExactFraction& b, const ExactFraction& c) {
  RationalPolygon result;
  if (polygon.empty()) return result;
  result.reserve(polygon.size() + 2);
  const std::size_t count = polygon.size();
  for (std::size_t i = 0; i < count; ++i) {
    const RationalPoint& current = polygon[i];
    const RationalPoint& next = polygon[(i + 1) % count];
    const int side_current = side_of(current, a, b, c);
    const int side_next = side_of(next, a, b, c);
    const bool inside_current = side_current <= 0;
    const bool inside_next = side_next <= 0;
    if (inside_current) result.push_back(current);
    if (inside_current != inside_next) {
      // Exact intersection of the segment with the boundary line.
      const ExactFraction f0 = a * current.x + b * current.y - c;
      const ExactFraction f1 = a * next.x + b * next.y - c;
      const ExactFraction denominator = f0 - f1;
      if (!denominator.is_zero()) {
        const ExactFraction t = f0 / denominator;
        RationalPoint crossing;
        crossing.x = current.x + t * (next.x - current.x);
        crossing.y = current.y + t * (next.y - current.y);
        result.push_back(crossing);
      }
    }
  }
  return result;
}

Rational polygon_signed_area(const RationalPolygon& polygon) {
  if (polygon.size() < 3) return Rational(0, 1);
  ExactFraction total;
  const std::size_t count = polygon.size();
  for (std::size_t i = 0; i < count; ++i) {
    const RationalPoint& a = polygon[i];
    const RationalPoint& b = polygon[(i + 1) % count];
    total = total + (a.x * b.y - b.x * a.y);
  }
  return (total / ExactFraction(2)).to_rational();
}

Rational polygon_moment_x(const RationalPolygon& polygon) {
  if (polygon.size() < 3) return Rational(0, 1);
  ExactFraction total;
  const std::size_t count = polygon.size();
  for (std::size_t i = 0; i < count; ++i) {
    const RationalPoint& a = polygon[i];
    const RationalPoint& b = polygon[(i + 1) % count];
    const ExactFraction cross = a.x * b.y - b.x * a.y;
    total = total + (a.x + b.x) * cross;
  }
  return (total / ExactFraction(6)).to_rational();
}

Rational polygon_moment_y(const RationalPolygon& polygon) {
  if (polygon.size() < 3) return Rational(0, 1);
  ExactFraction total;
  const std::size_t count = polygon.size();
  for (std::size_t i = 0; i < count; ++i) {
    const RationalPoint& a = polygon[i];
    const RationalPoint& b = polygon[(i + 1) % count];
    const ExactFraction cross = a.x * b.y - b.x * a.y;
    total = total + (a.y + b.y) * cross;
  }
  return (total / ExactFraction(6)).to_rational();
}

}  // namespace acsd::geometry
