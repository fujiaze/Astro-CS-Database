// ACSD Normalize — 第五轮 SNR 科学复核（ROUND2 独立审查执行结论第 1 项）
//
// 前台裁决（实验/normalize-precision-p5/ROUND2_INDEPENDENT_REVIEW_633FFC31.md）:
// 像元积分算法正确 ≠ 真实设备 PSF/参考通量/增益/噪声条件已校准。在把 stage-1
// 稀疏参考精度作为跨设备产品前, 先做有边界的独立科学复核:
//   (1) 不同 σx、σy、θ、亚像元相位、边界/欠采样条件下的**独立方法**像元足迹求积;
//   (2) 指定真实 F_ref、gain/读噪/背景 的 photon+RN 前向模拟, 已知点源真值,
//       对拍 GLS 条件 V_F（散度/偏差/区间覆盖）。
//
// 依据:
//   docs/science/noise_snr/NOISE_SNR.md §3（冻结条件噪声下的 GLS 量）、§11.3
//   docs/science/psf/PSF.md §4（连续单位总通量、像元求积）
//   run/norm-precision-test.json 的真实生产参数: gain=1.0 e-/ADU, RN=9.0 e,
//   F_ref=1000 ADU (explicit_linear_flux, native_calibrated), σ=2.211089 px
//
// 独立性: 求积对照用**复合辛普森**（与生产 Gauss-Legendre 张量不同算法族）;
// 前向模拟的估计量散度用 nl 空间显式 M⁻¹ 第三条路径对拍生产 QR 行空间路径。
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstdio>

#include "acsd/pixel_integrated_psf.h"
#include "acsd/sparse_reference.h"

using namespace acsd::normalize;
using acsd::psf::PixelIntegrationRequest;
using acsd::psf::PixelIntegrationDiagnostics;
using acsd::psf::pixel_integrated_moffat4_profile;
using acsd::psf::moffat4_total_flux_normalization;

namespace {
int g_failures = 0;
const char* g_case = "";
void check(bool ok, const char* what) {
  std::printf("  [%s] %s (%s)\n", ok ? "PASS" : "FAIL", what, g_case);
  if (!ok) ++g_failures;
}
double rel(double a, double b) {
  const double d = std::abs(a - b);
  const double s = std::max(1e-300, std::max(std::abs(a), std::abs(b)));
  return d / s;
}

// ── 生产真实参数（run/norm-precision-test.json 实读, 非编造）─────────────────
constexpr double kGainEPerADU = 1.0;    // config: snr.gain_e_per_adu
constexpr double kReadNoiseE = 9.0;     // config: snr.read_noise_e
constexpr double kFRefADU = 1000.0;     // config: snr.reference.flux (explicit)
constexpr double kSigmaPx = 2.211089;   // M42 一帧 p1_sources sx/sy 中位几何平均
constexpr double kSkyADUPerPx = 58.0;   // 帧中位背景面亮度 ADU/px（E2E 证据:
                                        // "58 ADU/px ÷ Ω≈2.08e-11 sr ≈ 2.8e12"）

ReferenceDefinition make_ref(double flux) {
  ReferenceDefinition r;
  r.kind = ReferenceKind::kExplicitLinearFlux;
  r.reference_id = "verify5:unit-source";
  r.flux_unit = "adu";
  r.normalization = "photo_applied";
  r.flux = flux;
  return r;
}

// ── 独立求积: 复合辛普森, 每像元 nsub×nsub 子格 ────────────────────────────
// 与生产 Gauss-Legendre 张量（order 10）是**不同算法族**; Moffat4 在像元内解析,
// 辛普森 4 阶收敛 ⇒ nsub=128 时单像元离散化误差远低于 1e-11。
struct Moffat4Quad {
  double sx, sy, theta, cx, cy;
  double p1, p2, p3, norm;
  void init(double sx_, double sy_, double th, double cx_, double cy_) {
    sx = sx_; sy = sy_; theta = th; cx = cx_; cy = cy_;
    const double c = std::cos(theta), s = std::sin(theta);
    p1 = c * c / (2 * sx * sx) + s * s / (2 * sy * sy);
    p2 = std::sin(2 * theta) / (4 * sx * sx) - std::sin(2 * theta) / (4 * sy * sy);
    p3 = s * s / (2 * sx * sx) + c * c / (2 * sy * sy);
    norm = 3.0 / (2.0 * M_PI * sx * sy);   // PSF.md §4: 连续未归一积分 2π sx sy/3
  }
  double raw(double x, double y) const {
    const double dx = x - cx, dy = y - cy;
    const double q = p1 * dx * dx + 2 * p2 * dx * dy + p3 * dy * dy;
    return 1.0 / std::pow(1.0 + q, 4.0);
  }
  // 单像元 [px-0.5,px+0.5]×[py-0.5,py+0.5] 的单位总通量响应（辛普森）
  double pixel(double px, double py, int nsub) const {
    const double x0 = px - 0.5, y0 = py - 0.5, h = 1.0 / nsub;
    double acc = 0.0;
    for (int i = 0; i <= nsub; ++i) {
      const double wx = (i == 0 || i == nsub) ? 1.0 : (i % 2 ? 4.0 : 2.0);
      const double x = x0 + i * h;
      for (int j = 0; j <= nsub; ++j) {
        const double wy = (j == 0 || j == nsub) ? 1.0 : (j % 2 ? 4.0 : 2.0);
        acc += wx * wy * raw(x, y0 + j * h);
      }
    }
    return norm * acc * h * h / 9.0;
  }
};

// ── 窗口构造: m 个源像元 + nl 个叶的稀疏 R ────────────────────────────────
// mode: "identity" ⇒ R = I（每像元一叶, c=1, N_l=1）;
//       "sparse"  ⇒ 每 2 个相邻像元共享一叶（c 由等权 drop 归一给出）。
struct WindowSpec {
  int m = 0, nl = 0;
  std::vector<int> op_pix, op_leaf;
  std::vector<double> op_c;
};

WindowSpec make_window(int m, const char* mode) {
  WindowSpec s;
  s.m = m;
  if (std::string(mode) == "identity") {
    s.nl = m;
    for (int j = 0; j < m; ++j) {
      s.op_pix.push_back(j);
      s.op_leaf.push_back(j);
      s.op_c.push_back(1.0);
    }
  } else {
    // 每叶看 2 个像元: c_lj = (a_lj/A_drop,j)/N_l; 取 a=A_drop=1, Ω=1 ⇒
    // N_l = Σ_j w_lj Ω_j = 2, c = (1/1)/2 = 0.5。R 满行秩（nl=ceil(m/2) < m ⇒
    // Q=RD^{1/2} 秩亏, 走 §11.4 秩亏路径）。
    s.nl = (m + 1) / 2;
    for (int j = 0; j < m; ++j) {
      const int l = j / 2;
      s.op_pix.push_back(j);
      s.op_leaf.push_back(l);
      s.op_c.push_back(0.5);
    }
  }
  return s;
}

PlanarPatchWindow build_patch(const std::vector<double>& P,
                              const std::vector<double>& v,
                              const WindowSpec& s,
                              const std::vector<std::int64_t>& pix_index,
                              double phase_x, double phase_y,
                              BackgroundNuisance bg) {
  PlanarPatchWindow w;
  w.control_id = "verify5";
  w.cx_px = 100.0 + phase_x;
  w.cy_px = 200.0 + phase_y;
  w.ra_deg = 84.23;
  w.dec_deg = -6.37;
  w.leaf_order = 11;
  w.omega_leaf = 4.0 * M_PI / (12.0 * 2048.0 * 2048.0);
  w.pix_index = pix_index;
  w.profile = P;
  w.variance = v;
  const int m = s.m;
  for (int j = 0; j < m; ++j) {
    const int x = static_cast<int>(pix_index[static_cast<std::size_t>(j)] % 1000);
    const int y = static_cast<int>(pix_index[static_cast<std::size_t>(j)] / 1000);
    w.src_x.push_back(x);
    w.src_y.push_back(y);
    w.pix_omega.push_back(1.0);
    w.pix_a_drop.push_back(1.0);
  }
  w.op_pix = s.op_pix;
  w.op_leaf = s.op_leaf;
  w.op_c = s.op_c;
  for (int l = 0; l < s.nl; ++l) {
    w.leaf_ipix.push_back(500000ULL + l);
    w.leaf_dp.push_back(1.0);
    w.leaf_n_full.push_back(2.0);
    w.leaf_n_covered.push_back(2.0);
  }
  w.has_target_leaf = true;
  w.target_leaf = 500000ULL;
  return w;
}

// nl 空间显式 GLS（第三条独立路径）: M = A_yᵀ C_y⁻¹ A_y, C_y = R diag(v) Rᵀ
// 返回 [M⁻¹]_00 与估计行向量 W（每 rep O(m+nl)）。要求 R 满行秩（C_y 可逆）。
struct ExplicitGls {
  bool ok = false;
  std::string error;
  std::vector<double> w_row;   // F̂ = w_row · y
  double var_f = 0.0;          // [M⁻¹]_00
};

ExplicitGls explicit_nl_gls(const PlanarPatchWindow& win,
                            const std::vector<double>& v, int n_bg) {
  ExplicitGls g;
  const int m = static_cast<int>(win.pix_index.size());
  const int nl = static_cast<int>(win.leaf_ipix.size());
  const int ncol = 1 + n_bg;
  if (m <= 0 || nl <= 0) { g.error = "empty window"; return g; }
  // C_y = R diag(v) Rᵀ（nl×nl）
  std::vector<double> cy(static_cast<std::size_t>(nl) * nl, 0.0);
  for (std::size_t k = 0; k < win.op_c.size(); ++k) {
    const int j = win.op_pix[k], l = win.op_leaf[k];
    if (j < 0 || j >= m || l < 0 || l >= nl) continue;
    cy[static_cast<std::size_t>(l) * nl + l] +=
        win.op_c[k] * v[static_cast<std::size_t>(j)] * win.op_c[k];
  }
  std::vector<std::vector<std::pair<int, double>>> per_pix(m);
  for (std::size_t k = 0; k < win.op_c.size(); ++k) {
    const int j = win.op_pix[k], l = win.op_leaf[k];
    if (j < 0 || j >= m || l < 0 || l >= nl) continue;
    per_pix[static_cast<std::size_t>(j)].emplace_back(l, win.op_c[k]);
  }
  for (int j = 0; j < m; ++j)
    for (std::size_t a = 0; a < per_pix[static_cast<std::size_t>(j)].size(); ++a)
      for (std::size_t b = a + 1; b < per_pix[static_cast<std::size_t>(j)].size(); ++b) {
        const double t = per_pix[static_cast<std::size_t>(j)][a].second *
                         v[static_cast<std::size_t>(j)] *
                         per_pix[static_cast<std::size_t>(j)][b].second;
        const int la = per_pix[static_cast<std::size_t>(j)][a].first;
        const int lb = per_pix[static_cast<std::size_t>(j)][b].first;
        cy[static_cast<std::size_t>(la) * nl + lb] += t;
        cy[static_cast<std::size_t>(lb) * nl + la] += t;
      }
  // A_y = R A_src（nl×ncol）
  std::vector<double> ay(static_cast<std::size_t>(nl) * ncol, 0.0);
  for (std::size_t k = 0; k < win.op_c.size(); ++k) {
    const int j = win.op_pix[k], l = win.op_leaf[k];
    if (j < 0 || j >= m || l < 0 || l >= nl) continue;
    const double c = win.op_c[k];
    ay[static_cast<std::size_t>(l) * ncol + 0] +=
        c * win.profile[static_cast<std::size_t>(j)];
    for (int b = 1; b < ncol; ++b) {
      const double planar = (b == 1) ? 1.0
                          : (b == 2) ? win.src_x[static_cast<std::size_t>(j)]
                                     : win.src_y[static_cast<std::size_t>(j)];
      ay[static_cast<std::size_t>(l) * ncol + b] += c * planar;
    }
  }
  // C_y 求逆（Gauss-Jordan, nl 小; 含行主元; 右块即精确 C_y⁻¹）
  std::vector<double> aug(static_cast<std::size_t>(nl) * 2 * nl, 0.0);
  for (int l = 0; l < nl; ++l) {
    for (int l2 = 0; l2 < nl; ++l2)
      aug[static_cast<std::size_t>(l) * 2 * nl + l2] =
          cy[static_cast<std::size_t>(l) * nl + l2];
    aug[static_cast<std::size_t>(l) * 2 * nl + nl + l] = 1.0;
  }
  for (int col = 0; col < nl; ++col) {
    int piv = col;
    double best = std::abs(aug[static_cast<std::size_t>(col) * 2 * nl + col]);
    for (int r = col + 1; r < nl; ++r) {
      const double x = std::abs(aug[static_cast<std::size_t>(r) * 2 * nl + col]);
      if (x > best) { best = x; piv = r; }
    }
    if (!(best > 0.0)) { g.error = "C_y singular (R not full row rank)"; return g; }
    if (piv != col)
      for (int k = 0; k < 2 * nl; ++k)
        std::swap(aug[static_cast<std::size_t>(col) * 2 * nl + k],
                  aug[static_cast<std::size_t>(piv) * 2 * nl + k]);
    const double d = aug[static_cast<std::size_t>(col) * 2 * nl + col];
    for (int k = 0; k < 2 * nl; ++k)
      aug[static_cast<std::size_t>(col) * 2 * nl + k] /= d;
    for (int r = 0; r < nl; ++r) {
      if (r == col) continue;
      const double f = aug[static_cast<std::size_t>(r) * 2 * nl + col];
      if (f == 0.0) continue;
      for (int k = 0; k < 2 * nl; ++k)
        aug[static_cast<std::size_t>(r) * 2 * nl + k] -=
            f * aug[static_cast<std::size_t>(col) * 2 * nl + k];
    }
  }
  // 反排列不需要: Gauss-Jordan（含行主元交换）的右块就是精确逆 M⁻¹;
  // 但提取必须在消元循环**结束之后**（循环内读到的是后续仍会改动的中间值）。
  std::vector<double> cinv(static_cast<std::size_t>(nl) * nl, 0.0);
  for (int i = 0; i < nl; ++i)
    for (int j = 0; j < nl; ++j)
      cinv[static_cast<std::size_t>(i) * nl + j] =
          aug[static_cast<std::size_t>(i) * 2 * nl + nl + j];
  // M = A_yᵀ C_y⁻¹ A_y（ncol×ncol; 全部用原始坐标）
  std::vector<double> mm(static_cast<std::size_t>(ncol) * ncol, 0.0);
  for (int a = 0; a < ncol; ++a) {
    std::vector<double> ca(nl, 0.0);
    for (int l = 0; l < nl; ++l) {
      double s = 0.0;
      for (int l2 = 0; l2 < nl; ++l2)
        s += ay[static_cast<std::size_t>(l2) * ncol + a] *
             cinv[static_cast<std::size_t>(l) * nl + l2];
      ca[static_cast<std::size_t>(l)] = s;
    }
    for (int b = 0; b < ncol; ++b) {
      double s = 0.0;
      for (int l = 0; l < nl; ++l)
        s += ca[static_cast<std::size_t>(l)] *
             ay[static_cast<std::size_t>(l) * ncol + b];
      mm[static_cast<std::size_t>(a) * ncol + b] = s;
    }
  }
  // M 求逆取 [0][0]; W = e₀ᵀ M⁻¹ A_yᵀ C_y⁻¹（1×nl 行向量）。
  // 注意: 提取必须在消元循环**结束后**——Gauss-Jordan（含行主元）右块即
  // 精确 M⁻¹; 循环内读到的 a2 右块是后续仍会被消元改动的中间值（第一版
  // bug: mi[0][0] 读到了 0）。
  std::vector<double> mi(static_cast<std::size_t>(ncol) * ncol, 0.0);
  {
    std::vector<double> a2(static_cast<std::size_t>(ncol) * 2 * ncol, 0.0);
    for (int a = 0; a < ncol; ++a) {
      for (int b = 0; b < ncol; ++b)
        a2[static_cast<std::size_t>(a) * 2 * ncol + b] =
            mm[static_cast<std::size_t>(a) * ncol + b];
      a2[static_cast<std::size_t>(a) * 2 * ncol + ncol + a] = 1.0;
    }
    for (int col = 0; col < ncol; ++col) {
      int piv = col;
      double best = std::abs(a2[static_cast<std::size_t>(col) * 2 * ncol + col]);
      for (int r = col + 1; r < ncol; ++r) {
        const double x = std::abs(a2[static_cast<std::size_t>(r) * 2 * ncol + col]);
        if (x > best) { best = x; piv = r; }
      }
      if (!(best > 0.0)) { g.error = "M singular"; return g; }
      if (piv != col)
        for (int k = 0; k < 2 * ncol; ++k)
          std::swap(a2[static_cast<std::size_t>(col) * 2 * ncol + k],
                    a2[static_cast<std::size_t>(piv) * 2 * ncol + k]);
      const double d = a2[static_cast<std::size_t>(col) * 2 * ncol + col];
      for (int k = 0; k < 2 * ncol; ++k)
        a2[static_cast<std::size_t>(col) * 2 * ncol + k] /= d;
      for (int r = 0; r < ncol; ++r) {
        if (r == col) continue;
        const double f = a2[static_cast<std::size_t>(r) * 2 * ncol + col];
        if (f == 0.0) continue;
        for (int k = 0; k < 2 * ncol; ++k)
          a2[static_cast<std::size_t>(r) * 2 * ncol + k] -=
              f * a2[static_cast<std::size_t>(col) * 2 * ncol + k];
      }
    }
    for (int i = 0; i < ncol; ++i)
      for (int j = 0; j < ncol; ++j)
        mi[static_cast<std::size_t>(i) * ncol + j] =
            a2[static_cast<std::size_t>(i) * 2 * ncol + ncol + j];
  }
  g.var_f = mi[0];
  if (!(g.var_f > 0.0) || !std::isfinite(g.var_f)) {
    g.error = "non-positive [M^-1]_FF"; return g;
  }
  // W = e₀ᵀ M⁻¹ A_yᵀ C_y⁻¹（全部原始坐标; y 也是原始坐标）
  g.w_row.assign(nl, 0.0);
  for (int a = 0; a < ncol; ++a) {
    const double w0a = mi[static_cast<std::size_t>(a)];   // M⁻¹[0][a]
    if (w0a == 0.0) continue;
    for (int l = 0; l < nl; ++l) {
      double s = 0.0;
      for (int l2 = 0; l2 < nl; ++l2)
        s += ay[static_cast<std::size_t>(l2) * ncol + a] *
             cinv[static_cast<std::size_t>(l) * nl + l2];
      g.w_row[static_cast<std::size_t>(l)] += w0a * s;
    }
  }
  g.ok = true;
  return g;
}

}  // namespace

int main() {
  // ═══ Part 1: 独立像元足迹求积（复合辛普森 vs 生产 Gauss–Legendre） ═══════
  std::printf("== Part 1: independent pixel-quadrature cross-check ==\n");
  {
    g_case = "independent Simpson quadrature vs production Gauss-Legendre";
    struct Cfg { double sx, sy, th, px, py; const char* name; };
    const Cfg cfgs[] = {
        {kSigmaPx, kSigmaPx, 0.0, 0.0, 0.0, "isotropic production sigma"},
        {kSigmaPx, kSigmaPx, 0.7, 0.37, 0.61, "rotated+subpixel phase"},
        {kSigmaPx, kSigmaPx, 1.9, -0.5, 0.5, "rotated another phase"},
        {1.3 * kSigmaPx, 0.8 * kSigmaPx, 0.0, 0.13, -0.21, "anisotropic axis-aligned"},
        {0.6 * kSigmaPx, 1.7 * kSigmaPx, 1.1, -0.37, 0.44, "anisotropic rotated"},
        {1.0, 1.0, 0.0, 0.5, 0.5, "undersampled sigma=1 (worst seeing)"},
        {0.8, 1.3, 0.4, 0.0, 0.0, "undersampled anisotropic"},
    };
    double worst = 0.0;
    for (const auto& c : cfgs) {
      // 11x11 像元块, 中心 = (50+c.px, 60+c.py)
      std::vector<double> px, py;
      for (int y = 55; y <= 65; ++y)
        for (int x = 45; x <= 55; ++x) { px.push_back(x + 0.0); py.push_back(y + 0.0); }
      PixelIntegrationRequest r;
      r.sx = c.sx; r.sy = c.sy; r.theta = c.th;
      r.cx = 50.0 + c.px; r.cy = 60.0 + c.py;
      r.pixel_x = px.data(); r.pixel_y = py.data(); r.n_pixels = px.size();
      r.order = 10;
      std::vector<double> P;
      PixelIntegrationDiagnostics d;
      if (!pixel_integrated_moffat4_profile(r, &P, &d)) {
        check(false, "production integration succeeded");
        continue;
      }
      Moffat4Quad q;
      q.init(c.sx, c.sy, c.th, r.cx, r.cy);
      double cfgworst = 0.0;
      for (std::size_t i = 0; i < px.size(); ++i) {
        // 独立参考: 复合辛普森 256 子格（本配置下自身离散化误差 ~5e-12,
        // 由 128→256→512 收敛序列实测; 远小于被测差距）
        const double sim = q.pixel(px[i], py[i], 256);
        const double denom = std::max(1e-14, sim);
        cfgworst = std::max(cfgworst, std::abs(P[i] - sim) / denom);
      }
      worst = std::max(worst, cfgworst);
      std::printf("  %-32s max rel diff vs Simpson = %.3e (quad est err %.2e)\n",
                  c.name, cfgworst, d.estimated_error);
      check(cfgworst < 1e-9, "per-pixel response matches independent Simpson <1e-9");
    }
    std::printf("  worst over all configs: %.3e\n", worst);
  }
  {
    g_case = "large-window flux conservation (anisotropic, rotated)";
    // 椭圆/旋转模型的大窗 ΣP → 1（窗包含整个模型）; 与 θ 无关（2π sx sy/3 归一）
    for (double th : {0.0, 0.7, 1.9}) {
      PixelIntegrationRequest r;
      r.sx = 2.0; r.sy = 5.0; r.theta = th; r.cx = 0.0; r.cy = 0.0; r.order = 12;
      std::vector<double> px, py;
      const int half = 40;
      for (int y = -half; y <= half; ++y)
        for (int x = -half; x <= half; ++x) { px.push_back(x); py.push_back(y); }
      r.pixel_x = px.data(); r.pixel_y = py.data(); r.n_pixels = px.size();
      std::vector<double> P;
      PixelIntegrationDiagnostics d;
      if (!pixel_integrated_moffat4_profile(r, &P, &d)) {
        check(false, "integration succeeded");
        continue;
      }
      std::printf("  theta=%.2f f_enc=%.12f\n", th, d.f_enclosed);
      check(d.f_enclosed > 0.9999, "anisotropic/rotated large-window ΣP > 0.9999");
    }
  }

  // ═══ Part 2: 已知真值 photon+RN 前向模拟 vs GLS 条件 V_F ═══════════════════
  // 噪声模型（NOISE_SNR §2, 电子→ADU, g=1 e-/ADU, RN=9 e, 空sky 58 ADU/px）:
  //   物理世界（总通量语义, PSF.md §4 analytic_total）: v^sim_j = (F·P_j+B)/g + RN²/g²
  //   生产冻结条件模型（§3/§11.3）: v_j = v^blank_j + F_ref·P_j/(g·a_ref)
  // 调用方只向 production 提供 blank-sky 方差面（var_plane 契约）; 源泊松项由
  // production 自己按 §11.3 加入。9x9 窗 ΣP=0.9734 ⇒ 两种世界差 1/a_ref≈2.7%
  // （Poisson 份额约 79% ⇒ 方差约 2%, Part 3 用显式路径量化并登记待裁决）。
  // MC 在**条件模型世界**（v）与**物理世界**（v^sim）各跑一遍。
  std::printf("\n== Part 2: forward photon+RN simulation vs conditional V_F ==\n");
  {
    // 9x9 patch, σ=2.211089, 亚像元相位 (0.37,0.61), P 由生产 PSF 库给出
    std::vector<double> px, py;
    const int W = 9, cx0 = 100, cy0 = 200;
    for (int y = cy0 - W / 2; y <= cy0 + W / 2; ++y)
      for (int x = cx0 - W / 2; x <= cx0 + W / 2; ++x) { px.push_back(x); py.push_back(y); }
    const int m = static_cast<int>(px.size());
    PixelIntegrationRequest pr;
    pr.sx = pr.sy = kSigmaPx; pr.theta = 0.0;
    pr.cx = 100.37; pr.cy = 200.61;
    pr.pixel_x = px.data(); pr.pixel_y = py.data(); pr.n_pixels = px.size();
    pr.order = 10;
    std::vector<double> P;
    PixelIntegrationDiagnostics pd;
    if (!pixel_integrated_moffat4_profile(pr, &P, &pd)) {
      std::printf("FATAL: production PSF integration failed\n");
      return 2;
    }
    double a_ref = 0.0;
    for (double p : P) a_ref += p;
    std::printf("  patch m=%d ΣP=%.9f f_out=%.3e quad_err=%.2e\n",
                m, a_ref, pd.f_out, pd.estimated_error);
    check(a_ref > 0.95 && a_ref <= 1.0,
          "9x9 window encloses >95% of unit flux at sigma=2.21 (window physics)");

    // blank-sky 方差面（调用方契约只提供这一项; 生产自己加参考源 Poisson）
    const double blank =
        kSkyADUPerPx / kGainEPerADU +
        kReadNoiseE * kReadNoiseE / (kGainEPerADU * kGainEPerADU);
    // 生产冻结条件方差（§11.3 逐字: v_j = blank + F_ref·P_j/(g·a_ref)）
    std::vector<double> v_cond(m);
    for (int j = 0; j < m; ++j)
      v_cond[static_cast<std::size_t>(j)] =
          blank + kFRefADU * P[static_cast<std::size_t>(j)] /
                      (kGainEPerADU * a_ref);
    // 物理世界方差（总通量语义: 源泊松 = F·P_j/g, F=F_ref）
    std::vector<double> v_phys(m);
    for (int j = 0; j < m; ++j)
      v_phys[static_cast<std::size_t>(j)] =
          (kFRefADU * P[static_cast<std::size_t>(j)] + kSkyADUPerPx) / kGainEPerADU +
          kReadNoiseE * kReadNoiseE / (kGainEPerADU * kGainEPerADU);

    struct Scenario {
      const char* mode;
      BackgroundNuisance bg;
      bool with_background;   // 数据里是否含常量背景（known ⇒ 已扣除）
      const char* name;
    };
    const Scenario scen[] = {
        {"identity", BackgroundNuisance::kKnown, false,
         "R=I, known background (background-subtracted data)"},
        {"identity", BackgroundNuisance::kConstantFit, true,
         "R=I, constant nuisance fit"},
        {"sparse", BackgroundNuisance::kConstantFit, true,
         "drizzle-like sparse R (rank-deficient), nuisance fit"},
    };
    for (const auto& s : scen) {
      g_case = s.name;
      const int n_bg = (s.bg == BackgroundNuisance::kConstantFit) ? 1 : 0;
      const WindowSpec ws = make_window(m, s.mode);
      std::vector<std::int64_t> pix(m);
      for (int j = 0; j < m; ++j)
        pix[static_cast<std::size_t>(j)] =
            static_cast<std::int64_t>(py[static_cast<std::size_t>(j)]) * 1000 +
            static_cast<std::int64_t>(px[static_cast<std::size_t>(j)]);
      // production 只拿 blank-sky 方差面（源 Poisson 由 §11.3 路径自加）
      PlanarPatchWindow w =
          build_patch(P, std::vector<double>(m, blank), ws, pix, 0.37, 0.61, s.bg);
      SphericalCompletionConfig cfg;
      cfg.reference = make_ref(kFRefADU);
      cfg.background = s.bg;
      cfg.gain_e_per_adu = kGainEPerADU;
      cfg.master_uncertainty_modelled = true;   // 本模拟只检验随机噪声链
      cfg.template_fixed = true;
      const auto r = complete_spherical_reference({w}, cfg);
      if (r.precision.controls.empty() ||
          r.precision.controls[0].status.availability !=
              PrecisionAvailability::kAvailable ||
          !r.precision.controls[0].flux_variance.has_value()) {
        std::printf("  production GLS unavailable: reason=%s\n",
                    r.precision.controls.empty() ? "no control"
                        : r.precision.controls[0].status.reason.c_str());
        check(false, "production GLS available");
        continue;
      }
      const double v_f = *r.precision.controls[0].flux_variance;
      const double snr = *r.precision.controls[0].reference_snr;
      std::printf("  V_F=%.6g sqrt=%.6g SNR_ref=%.6g rank=%d bg_rank=%d\n",
                  v_f, std::sqrt(v_f), snr, r.windows[0].solver_rank,
                  r.windows[0].background_basis_rank);

      // 第三条路径: nl 空间显式 GLS（与生产 QR 行空间投影同一被估量, 不同代码）
      const ExplicitGls eg = explicit_nl_gls(w, v_cond, n_bg);
      if (eg.ok) {
        std::printf("  explicit nl-space [M^-1]_FF=%.6g (rel vs prod %.2e)\n",
                    eg.var_f, rel(eg.var_f, v_f));
        check(rel(eg.var_f, v_f) < 1e-12,
              "production V_F equals explicit nl-space GLS [M^-1]_FF "
              "(machine precision)");
      } else {
        std::printf("  explicit nl-space skipped: %s\n", eg.error.c_str());
        check(false, "explicit nl-space estimator available for MC");
        continue;
      }

      // 前向模拟（两个噪声世界; 已知真值 F=F_ref=1000 ADU）
      for (int world = 0; world < 2; ++world) {
        const std::vector<double>& vsim =
            (world == 0) ? v_cond : v_phys;
        const char* wname = (world == 0) ? "conditional-world" : "physical-world";
        const double F = kFRefADU;
        const int nl = ws.nl;
        std::mt19937_64 rng(20261016 + world);
        std::normal_distribution<double> nz(0.0, 1.0);
        const int nrep = 200000;
        double s1 = 0.0, s2 = 0.0;
        int cover = 0;
        const double halfwidth = 1.959963984540054 * std::sqrt(v_f);
        std::vector<double> y(nl, 0.0);
        // 每像元贡献表（避免每 rep 扫描全部三元组）
        std::vector<std::vector<std::pair<int, double>>> contrib(m);
        for (std::size_t k = 0; k < ws.op_c.size(); ++k)
          contrib[static_cast<std::size_t>(ws.op_pix[k])].emplace_back(
              ws.op_leaf[k], ws.op_c[k]);
        std::vector<double> sqrts(m);
        for (int j = 0; j < m; ++j)
          sqrts[static_cast<std::size_t>(j)] =
              std::sqrt(vsim[static_cast<std::size_t>(j)]);
        for (int rep = 0; rep < nrep; ++rep) {
          for (int l = 0; l < nl; ++l) y[static_cast<std::size_t>(l)] = 0.0;
          for (int j = 0; j < m; ++j) {
            // x_j = F·P_j (+β) + n_j, n_j ~ N(0, v^sim_j)
            double xj = F * P[static_cast<std::size_t>(j)] +
                        sqrts[static_cast<std::size_t>(j)] * nz(rng);
            if (s.with_background) xj += kSkyADUPerPx;
            for (const auto& e : contrib[static_cast<std::size_t>(j)])
              y[static_cast<std::size_t>(e.first)] += e.second * xj;
          }
          double fhat = 0.0;
          for (int l = 0; l < nl; ++l)
            fhat += eg.w_row[static_cast<std::size_t>(l)] *
                    y[static_cast<std::size_t>(l)];
          s1 += fhat;
          s2 += fhat * fhat;
          if (std::abs(fhat - F) <= halfwidth) ++cover;
        }
        const double mean = s1 / nrep;
        const double sd = std::sqrt(s2 / nrep - mean * mean);
        const double bias = mean - F;
        const double mc_se = sd / std::sqrt(2.0 * nrep);
        const double ratio = sd / std::sqrt(v_f);
        std::printf("  MC[%s] n=%d: bias=%+.4f (%.3f sigma_VF) sd=%.4f "
                    "sqrt(V_F)=%.4f ratio=%.5f (se %.4f%%) coverage=%.4f\n",
                    wname, nrep, bias, bias / std::sqrt(v_f), sd,
                    std::sqrt(v_f), ratio, 100 * mc_se / sd,
                    static_cast<double>(cover) / nrep);
        g_case = s.name;
        if (world == 0) {
          check(std::abs(bias) < 4.0 * std::sqrt(v_f) /
                                     std::sqrt(static_cast<double>(nrep)),
                "conditional-world estimator unbiased within 4 MC se");
          check(std::abs(ratio - 1.0) < 4.0 * mc_se,
                "conditional-world MC scatter == sqrt(V_F) within 4 sigma");
          const double cov_se = std::sqrt(0.95 * 0.05 / nrep);
          check(std::abs(static_cast<double>(cover) / nrep - 0.95) <
                    4.0 * cov_se,
                "conditional-world 95% interval coverage matches nominal");
        } else {
          // 物理世界: 仅作量化记录（1/a_ref 效应使条件 V_F 略保守; 幅度必须
          // 小于 Poisson 份额的 1/a_ref-1, 不设硬门, 登记供前台裁决）
          const double infl =
              std::abs(ratio * ratio - 1.0);   // 方差相对差
          std::printf("    physical-world variance rel dev = %.4f%% "
                      "(1/a_ref-1 = %.4f%%)\n",
                      100 * infl, 100 * (1.0 / a_ref - 1.0));
          check(std::abs(bias) < 4.0 * std::sqrt(v_f) /
                                     std::sqrt(static_cast<double>(nrep)),
                "physical-world estimator unbiased within 4 MC se");
          check(infl < 0.05,
                "physical-world scatter deviates <5% from conditional V_F "
                "(a_ref window effect quantified, not gated by physics)");
        }
      }
    }
  }

  // ═══ Part 3: 冻结条件模型的语义边界（§3: 条件 V_F 是 F=F_ref 处的量） ═════
  std::printf("\n== Part 3: frozen conditional model semantics (F_true != F_ref) ==\n");
  {
    g_case = "frozen conditional V_F is exact at F=F_ref, conservative below";
    std::vector<double> px, py;
    const int W = 9, cx0 = 100, cy0 = 200;
    for (int y = cy0 - W / 2; y <= cy0 + W / 2; ++y)
      for (int x = cx0 - W / 2; x <= cx0 + W / 2; ++x) { px.push_back(x); py.push_back(y); }
    const int m = static_cast<int>(px.size());
    PixelIntegrationRequest pr;
    pr.sx = pr.sy = kSigmaPx; pr.theta = 0.0; pr.cx = 100.37; pr.cy = 200.61;
    pr.pixel_x = px.data(); pr.pixel_y = py.data(); pr.n_pixels = px.size();
    pr.order = 10;
    std::vector<double> P;
    PixelIntegrationDiagnostics pd;
    (void)pixel_integrated_moffat4_profile(pr, &P, &pd);
    double a_ref = 0.0;
    for (double p : P) a_ref += p;
    const double blank =
        kSkyADUPerPx / kGainEPerADU +
        kReadNoiseE * kReadNoiseE / (kGainEPerADU * kGainEPerADU);
    std::vector<std::int64_t> pix(m);
    for (int j = 0; j < m; ++j)
      pix[static_cast<std::size_t>(j)] =
          static_cast<std::int64_t>(py[static_cast<std::size_t>(j)]) * 1000 +
          static_cast<std::int64_t>(px[static_cast<std::size_t>(j)]);
    for (double f_true : {0.0, 500.0, 1000.0, 2000.0}) {
      // production 只拿 blank-sky（源 Poisson 由 §11.3 自加, F_ref=1000 冻结）
      const WindowSpec ws = make_window(m, "identity");
      PlanarPatchWindow w0 = build_patch(P, std::vector<double>(m, blank), ws,
                                         pix, 0.37, 0.61,
                                         BackgroundNuisance::kKnown);
      SphericalCompletionConfig cfg;
      cfg.reference = make_ref(kFRefADU);
      cfg.background = BackgroundNuisance::kKnown;
      cfg.gain_e_per_adu = kGainEPerADU;
      const auto r = complete_spherical_reference({w0}, cfg);
      const double v_f = *r.precision.controls[0].flux_variance;
      // 真实方差: 同一估计量在物理世界 vsim(F_true) 下的精确方差
      std::vector<double> vsim(m);
      for (int j = 0; j < m; ++j)
        vsim[static_cast<std::size_t>(j)] =
            (f_true * P[static_cast<std::size_t>(j)] + kSkyADUPerPx) / kGainEPerADU +
            kReadNoiseE * kReadNoiseE / (kGainEPerADU * kGainEPerADU);
      PlanarPatchWindow ws2 = build_patch(P, vsim, ws, pix, 0.37, 0.61,
                                          BackgroundNuisance::kKnown);
      const ExplicitGls eg = explicit_nl_gls(ws2, vsim, 0);
      std::printf("  F_true=%7.1f: conditional V_F=%.6g  true Var(F_hat)=%.6g "
                  "ratio=%.4f\n", f_true, v_f, eg.var_f, eg.var_f / v_f);
      if (f_true == kFRefADU) {
        // 9x9 窗 a_ref=0.9734: 条件模型源泊松用 P/a_ref（窗归一）, 物理世界用 P
        // ⇒ 条件 V_F 系统性偏大约 1/a_ref 的 Poisson 份额。生产窗 m≈633
        // （a_ref≈1）时该效应 <0.1%; 此处量化登记, 由前台裁决是否需要把
        // §11.3 的 a_ref 配对从"窗归一"改为"总通量语义"。
        check(eg.var_f / v_f > 0.9 && eg.var_f / v_f < 1.0,
              "at F_true=F_ref conditional V_F is within the a_ref window "
              "effect of the exact variance (quantified, not exact)");
      }
      if (f_true < kFRefADU) {
        check(eg.var_f < v_f,
              "below F_ref the frozen V_F is conservative (source Poisson "
              "frozen at F_ref)");
      }
      if (f_true > kFRefADU) {
        check(eg.var_f > v_f,
              "above F_ref the frozen V_F understates the true variance "
              "(conditional model is not a global guarantee)");
      }
    }
  }

  std::printf("\n%s: %d failure(s)\n", g_failures ? "FAILED" : "ALL PASS",
              g_failures);
  return g_failures ? 1 : 0;
}
