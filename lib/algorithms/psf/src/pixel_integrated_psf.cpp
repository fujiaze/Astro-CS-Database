// ACSD PSF — 单位总通量像元积分响应实现
//
// 正本: docs/science/psf/PSF.md §4（2026-10-15 裁决）;
//       docs/engineering/contracts/NORMALIZE_DATA.md §7.1 psf kind。
// 方法: 每像元足迹上的 Gauss-Legendre 张量求积（每维 order 个节点）。
//   **不做**像元中心函数值代入（PSF.md §4 明令禁止: 欠采样时子像元相位会 bias）;
//   **不**把 ΣP 钳到 1（f_enc 由实际像元集合求和得到）。
#include "acsd/pixel_integrated_psf.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace acsd {
namespace psf {
namespace {

constexpr double kPi = 3.14159265358979323846;

// n 点 Gauss-Legendre 节点/权重（Newton 迭代求根, n ≤ 64 足够且确定性）。
bool gauss_legendre(int n, std::vector<double>* x, std::vector<double>* w) {
  if (n < 2 || n > 64) return false;
  x->assign(static_cast<std::size_t>(n), 0.0);
  w->assign(static_cast<std::size_t>(n), 0.0);
  for (int i = 0; i < (n + 1) / 2; ++i) {
    // 初值（Cos 近似）后 Newton 精化 P_n(x)=0。
    double z = std::cos(kPi * (static_cast<double>(i) + 0.75) /
                        (static_cast<double>(n) + 0.5));
    double pp = 0.0;
    for (int it = 0; it < 100; ++it) {
      double p0 = 1.0, p1 = 0.0;
      for (int j = 0; j < n; ++j) {
        const double p2 = p1;
        p1 = p0;
        p0 = ((2.0 * static_cast<double>(j) + 1.0) * z * p1 -
              static_cast<double>(j) * p2) /
             (static_cast<double>(j) + 1.0);
      }
      pp = static_cast<double>(n) * (z * p0 - p1) / (z * z - 1.0);
      const double dz = p0 / pp;
      z -= dz;
      if (std::abs(dz) < 1e-16) break;
    }
    const double xi = -z;   // 升序存放
    const double wi = 2.0 / ((1.0 - z * z) * pp * pp);
    (*x)[static_cast<std::size_t>(i)] = xi;
    (*x)[static_cast<std::size_t>(n - 1 - i)] = z;
    (*w)[static_cast<std::size_t>(i)] = wi;
    (*w)[static_cast<std::size_t>(n - 1 - i)] = wi;
  }
  return true;
}

// 未归一 Moffat4 轮廓值（含旋转二次型; PSF.md §2.2 的 p1/p2/p3）。
inline double moffat4_raw(double u, double v, double p1, double p2, double p3) {
  const double q = p1 * u * u + 2.0 * p2 * u * v + p3 * v * v;
  const double t = 1.0 + q;
  if (!(t > 0.0)) return 0.0;
  const double t2 = t * t;
  return 1.0 / (t2 * t2);
}

}  // namespace

double moffat4_total_flux_normalization(double sx, double sy) {
  if (!(sx > 0.0) || !(sy > 0.0) || !std::isfinite(sx) || !std::isfinite(sy))
    return 0.0;
  // ∫_{R²}(1 + uᵀMu)^{-4} d²u = π/(3·√det M), det M = 1/(4 sx² sy²)
  // ⇒ = 2π sx sy / 3, 归一因子 = 3/(2π sx sy)。
  return 3.0 / (2.0 * kPi * sx * sy);
}

double moffat4_closed_form_f_out_disc(double sigma, double radius) {
  if (!(sigma > 0.0) || !(radius >= 0.0)) return 1.0;
  const double t = 1.0 + (radius * radius) / (2.0 * sigma * sigma);
  return 1.0 / (t * t * t);
}

bool pixel_integrated_moffat4_profile(const PixelIntegrationRequest& req,
                                      std::vector<double>* out_p,
                                      PixelIntegrationDiagnostics* diag) {
  if (out_p == nullptr) return false;
  out_p->clear();
  if (!(req.sx > 0.0) || !(req.sy > 0.0) || !std::isfinite(req.sx) ||
      !std::isfinite(req.sy) || !std::isfinite(req.theta) ||
      !std::isfinite(req.cx) || !std::isfinite(req.cy) ||
      (req.pixel_x == nullptr && req.n_pixels > 0) ||
      (req.pixel_y == nullptr && req.n_pixels > 0) ||
      req.order < 2 || req.order > 64) {
    return false;
  }
  const std::size_t n = req.n_pixels;
  out_p->assign(n, 0.0);
  if (n == 0) {
    if (diag) *diag = PixelIntegrationDiagnostics{};
    return false;
  }
  // 二次型系数（PSF.md §2.2）
  const double c = std::cos(req.theta), s = std::sin(req.theta);
  const double c2 = c * c, s2 = s * s, s2t = std::sin(2.0 * req.theta);
  const double p1 = c2 / (2.0 * req.sx * req.sx) + s2 / (2.0 * req.sy * req.sy);
  const double p2 = s2t / (4.0 * req.sx * req.sx) - s2t / (4.0 * req.sy * req.sy);
  const double p3 = s2 / (2.0 * req.sx * req.sx) + c2 / (2.0 * req.sy * req.sy);
  const double norm = moffat4_total_flux_normalization(req.sx, req.sy);
  if (!(norm > 0.0)) return false;

  auto integrate = [&](int order, std::vector<double>* p) {
    std::vector<double> gx, gw;
    if (!gauss_legendre(order, &gx, &gw)) return false;
    p->assign(n, 0.0);
    for (std::size_t k = 0; k < n; ++k) {
      const double px = req.pixel_x[k];
      const double py = req.pixel_y[k];
      double acc = 0.0;
      for (int i = 0; i < order; ++i) {
        const double y = py + 0.5 * gx[static_cast<std::size_t>(i)];
        for (int j = 0; j < order; ++j) {
          const double x = px + 0.5 * gx[static_cast<std::size_t>(j)];
          const double u = x - req.cx, v = y - req.cy;
          // 0.25 = (0.5 per axis) 是把 [-1,1] 节点映到 [px-0.5, px+0.5] 的
          // Jacobian（Gauss 权重本身只在 [-1,1] 上有效）; 漏掉它会把 P 放大 4 倍。
          acc += 0.25 * gw[static_cast<std::size_t>(j)] *
                 gw[static_cast<std::size_t>(i)] * moffat4_raw(u, v, p1, p2, p3);
        }
      }
      (*p)[k] = norm * acc;
    }
    return true;
  };

  if (!integrate(req.order, out_p)) return false;
  if (diag) {
    PixelIntegrationDiagnostics d;
    d.method = PixelIntegrationMethod::kGaussLegendreTensor;
    d.order = req.order;
    d.n_pixels = static_cast<int>(n);
    std::vector<double> p_hi;
    if (integrate(req.order + 1, &p_hi)) {
      double worst = 0.0;
      for (std::size_t k = 0; k < n; ++k)
        worst = std::max(worst, std::abs(p_hi[k] - (*out_p)[k]));
      d.estimated_error = worst;
    } else {
      d.estimated_error = -1.0;   // 无法估计 ⇒ 显式负值（不冒充 0 误差）
    }
    double enc = 0.0;
    for (double v : *out_p) enc += v;
    d.f_enclosed = enc;
    d.f_out = 1.0 - enc;
    *diag = d;
  }
  return true;
}

}  // namespace psf
}  // namespace acsd
