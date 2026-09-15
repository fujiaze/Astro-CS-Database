/* tests/integration/v6_p3/p3_v6_export_e2e_test.cpp
 * P3-INTEGRATE-001 端点测试：三模式导出 + 流式 FITS + 原子发布 + 磁盘重开。
 *
 * 运行: v6_p3_export_test <positive|negative> <artifact_dir>
 *   positive : 三模式各写一个真实产品到 <artifact_dir>/<mode>/，写 expected.json
 *              供独立 Python Oracle 重开对照；断言 HDU 集合/重开/门。
 *   negative : 14+ 条违反冻结的注入逐条必红（返回非 Ok），且**不产生**产物目录。
 *
 * 独立 Oracle（同目录 p3_v6_export_oracle.py，纯 stdlib FITS 解析 + 冻结表反查）
 * 对 positive 产物做磁盘重开对照；本文件内的 Q/W/SB 期望值由独立稠密线性代数/
 * 手算双线性权重给出，不复用 p3_rsmp 实现（非同实现自证）。
 */
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "p3_v6_export.h"
#include "p3_proj_v6.h"
#include "p3_rsmp.h"

#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

using namespace astrocs::phase3::v6;
namespace p3rsmp = astrocs::p3rsmp;
namespace phase3proj = astrocs::phase3proj;
using nlohmann::json;

static int g_checks = 0;
static int g_fails = 0;

#define CHECK(cond, msg)                                                    \
  do {                                                                      \
    ++g_checks;                                                             \
    if (!(cond)) {                                                          \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, (msg)); \
      ++g_fails;                                                            \
    }                                                                       \
  } while (0)

namespace {

constexpr const char* kSoftwareSha =
    "95703e639eb0056d2702e407e01f453fd353010d";  // baseline HEAD（结构夹具）

void mkdir_p(const std::string& p) {
#if defined(_WIN32)
  ::_mkdir(p.c_str());
#else
  ::mkdir(p.c_str(), 0755);
#endif
}

bool dir_exists(const std::string& p) {
#if defined(_WIN32)
  return ::_access(p.c_str(), 0) == 0;
#else
  return ::access(p.c_str(), F_OK) == 0;
#endif
}

std::vector<double> gaussian_psf(int w, int h, double sigma) {
  std::vector<double> p(static_cast<std::size_t>(w) * h, 0.0);
  const double cx = (w - 1) / 2.0, cy = (h - 1) / 2.0;
  double s = 0.0;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const double dx = x - cx, dy = y - cy;
      const double v = std::exp(-(dx * dx + dy * dy) / (2.0 * sigma * sigma));
      p[static_cast<std::size_t>(y) * w + x] = v;
      s += v;
    }
  }
  for (double& v : p) v /= s;
  return p;
}

struct Scene {
  OutputGrid grid;
  OutputGrid in_grid;
  ExportInputs in;
  ResamplePlan plan;
};

// 独立双线性四象限权重（定义见后；此处前置声明）。
void quad(int ox, int oy, double origin, int* jx, int* jy, double* w);

// 输出 8x8、输入 10x10、sub-pixel origin=0.75（四象限权重 0.5625/0.1875/0.1875/0.0625，
// 全像素满覆盖，无边界缺失）。
bool build_scene(Scene* s) {
  std::string err;
  if (build_output_grid(phase3proj::v6::ProjectionId::kTAN, 10.0, 41.0, 0.1, 8, 8,
                        "east_left", 0.0, &s->grid, &err) !=
      phase3proj::v6::ProjStatus::kOk) {
    std::fprintf(stderr, "grid: %s\n", err.c_str());
    return false;
  }
  if (build_output_grid(phase3proj::v6::ProjectionId::kTAN, 10.0, 41.0, 0.1, 10, 10,
                        "east_left", 0.0, &s->in_grid, &err) !=
      phase3proj::v6::ProjStatus::kOk) {
    std::fprintf(stderr, "in grid: %s\n", err.c_str());
    return false;
  }
  // 采样计划必须先于 Ω_in 自洽计算设定（列归一条件依赖真实 origin）。
  s->plan.out_origin_x = 0.75;
  s->plan.out_origin_y = 0.75;
  s->plan.out_step = 1.0;
  ExportInputs& in = s->in;
  in.in_width = 10;
  in.in_height = 10;
  const int iw = 10, ih = 10, ow = 8, oh = 8;
  const std::size_t n = static_cast<std::size_t>(iw) * ih;
  // 列归一自洽的输入像素立体角：Omega_j = Sum_i w_ij * Omega'_i，使
  // Sum_i S_ij = (1/Omega_j) Sum_i w_ij Omega'_i = 1 精确成立（bilinear_4quad
  // 重叠模型 overlap=w*Omega'_i 的列归一条件；也是 FZ-P3-QW-RECOMPUTE 下
  // Sum(pi)=Sum(p) 的前提）。独立 Oracle 用同一 Omega_in，非自证。
  in.omega_in_sr.assign(n, 0.0);
  for (int oy = 0; oy < oh; ++oy) {
    for (int ox = 0; ox < ow; ++ox) {
      int jx[4], jy[4];
      double w[4];
      quad(ox, oy, s->plan.out_origin_x, jx, jy, w);
      const double omo = s->grid.omega_out_sr[static_cast<std::size_t>(oy) * ow + ox];
      for (int k = 0; k < 4; ++k) {
        in.omega_in_sr[static_cast<std::size_t>(jy[k]) * iw + jx[k]] += w[k] * omo;
      }
    }
  }
  double om_fallback = s->grid.omega_out_sr[0];
  for (double v : s->grid.omega_out_sr) om_fallback = std::min(om_fallback, v);
  for (double& v : in.omega_in_sr) {
    if (!(v > 0.0)) v = om_fallback;
  }
  in.x.resize(n);
  for (std::size_t i = 0; i < n; ++i) in.x[i] = 10.0 + 0.5 * static_cast<double>(i);
  in.c_in = p3rsmp::DenseMatrix(static_cast<int>(n), static_cast<int>(n));
  for (std::size_t i = 0; i < n; ++i) {
    in.c_in(static_cast<int>(i), static_cast<int>(i)) = 1.0 + 0.01 * static_cast<double>(i);
  }
  // 输入 PSF 仅置于列归一满覆盖内区（x,y in [1,7]）：边界输入像素列和 < 1，
  // 会使 Σπ = Σ_j p_j·colsum_j ≠ 1；生产路径同样要求 PSF 足迹落在满覆盖区。
  in.psf_p = gaussian_psf(10, 10, 1.2);
  for (int y = 0; y < 10; ++y) {
    for (int x = 0; x < 10; ++x) {
      if (x < 1 || x > 7 || y < 1 || y > 7) in.psf_p[y * 10 + x] = 0.0;
    }
  }
  {
    double s = 0.0;
    for (double v : in.psf_p) s += v;
    for (double& v : in.psf_p) v /= s;
  }
  in.psf_present = true;
  in.point_information_present = true;
  in.photometric_scale_a = 1.5;
  in.effective_psf_present = true;
  in.effective_psf_normalization_declared = true;
  in.algorithm_ids = {"ALG-P3-001", "ALG-P3-008"};
  in.software_sha = kSoftwareSha;
  in.run_id = "run-p3-intg-001";
  in.input_product_hashes = {"sha256:phase2-mosaic-input"};
  in.config_hash = "sha256:phase3-config";
  return true;
}

// 独立双线性四象限权重（不复用 p3_rsmp）。
void quad(int ox, int oy, double origin, int* jx, int* jy, double* w) {
  const double u = origin + ox - 0.5;
  const double v = origin + oy - 0.5;
  const int j0 = static_cast<int>(std::floor(u));
  const int k0 = static_cast<int>(std::floor(v));
  const double fx = u - j0, fy = v - k0;
  jx[0] = j0;     jy[0] = k0;     w[0] = (1 - fx) * (1 - fy);
  jx[1] = j0 + 1; jy[1] = k0;     w[1] = fx * (1 - fy);
  jx[2] = j0;     jy[2] = k0 + 1; w[2] = (1 - fx) * fy;
  jx[3] = j0 + 1; jy[3] = k0 + 1; w[3] = fx * fy;
}

// 独立稠密高斯消元（部分主元）；a 为 n×n 行主序副本。
std::vector<double> solve_dense(std::vector<double> a, std::vector<double> b, int n) {
  for (int col = 0; col < n; ++col) {
    int piv = col;
    for (int r = col + 1; r < n; ++r) {
      if (std::fabs(a[r * n + col]) > std::fabs(a[piv * n + col])) piv = r;
    }
    if (piv != col) {
      for (int c = 0; c < n; ++c) std::swap(a[col * n + c], a[piv * n + c]);
      std::swap(b[col], b[piv]);
    }
    const double d = a[col * n + col];
    for (int r = col + 1; r < n; ++r) {
      const double f = a[r * n + col] / d;
      if (f == 0.0) continue;
      for (int c = col; c < n; ++c) a[r * n + c] -= f * a[col * n + c];
      b[r] -= f * b[col];
    }
  }
  std::vector<double> x(n, 0.0);
  for (int r = n - 1; r >= 0; --r) {
    double acc = b[r];
    for (int c = r + 1; c < n; ++c) acc -= a[r * n + c] * x[c];
    x[r] = acc / a[r * n + r];
  }
  return x;
}

struct Expected {
  std::vector<double> signal;    // SB 主面（bilinear R）
  std::vector<double> vis_signal;  // visualization 最近邻显示面
  std::vector<double> variance;  // diag(C_y)
  std::vector<double> flux;
  std::vector<double> flux_var;
  std::vector<double> psf;
  double Q = 0, W = 0, F_hat = 0, var_F_hat = 0;
};

// 独立 Oracle：双线性权重 + R/S + 稠密 C_y + 高斯消元解 Q/W。
Expected compute_expected(const Scene& s) {
  const int ow = s.grid.width, oh = s.grid.height;
  const int iw = s.in.in_width, ih = s.in.in_height;
  const std::size_t no = static_cast<std::size_t>(ow) * oh;
  const std::size_t ni = static_cast<std::size_t>(iw) * ih;
  Expected e;
  e.signal.assign(no, 0.0);
  e.variance.assign(no, 0.0);

  // 独立 R/S 稀疏（这里存稠密以便解 Q/W）。
  std::vector<double> S(no * ni, 0.0);
  std::vector<double> R(no * ni, 0.0);
  for (int oy = 0; oy < oh; ++oy) {
    for (int ox = 0; ox < ow; ++ox) {
      const std::size_t i = static_cast<std::size_t>(oy) * ow + ox;
      int jx[4], jy[4];
      double w[4];
      quad(ox, oy, s.plan.out_origin_x, jx, jy, w);
      const double omo = s.grid.omega_out_sr[i];
      double y = 0.0, var = 0.0;
      for (int k = 0; k < 4; ++k) {
        const std::size_t j = static_cast<std::size_t>(jy[k]) * iw + jx[k];
        y += w[k] * s.in.x[j];
        var += w[k] * w[k] * s.in.c_in(static_cast<int>(j), static_cast<int>(j));
        R[i * ni + j] += w[k];                          // R_ij = w
        S[i * ni + j] += w[k] * omo / s.in.omega_in_sr[j];  // S_ij = w Ω'_i/Ω_j
      }
      e.signal[i] = y;
      e.variance[i] = var;
      // visualization: 最近邻（floor(origin+o*step)），非科学无归一
      const int nx = static_cast<int>(std::floor(s.plan.out_origin_x + ox * s.plan.out_step));
      const int ny = static_cast<int>(std::floor(s.plan.out_origin_y + oy * s.plan.out_step));
      e.vis_signal.push_back(s.in.x[static_cast<std::size_t>(ny) * iw + nx]);
    }
  }

  // C_d = diag(Ω) C_x diag(Ω)（C_x 对角 -> 仍对角）
  std::vector<double> cd(ni, 0.0);
  for (std::size_t j = 0; j < ni; ++j) {
    const double om = s.in.omega_in_sr[j];
    cd[j] = om * s.in.c_in(static_cast<int>(j), static_cast<int>(j)) * om;
  }
  // f = S d, d_j = x_j Ω_j
  e.flux.assign(no, 0.0);
  e.psf.assign(no, 0.0);
  for (std::size_t i = 0; i < no; ++i) {
    double f = 0.0, p = 0.0;
    for (std::size_t j = 0; j < ni; ++j) {
      const double d = s.in.x[j] * s.in.omega_in_sr[j];
      f += S[i * ni + j] * d;
      p += S[i * ni + j] * s.in.psf_p[j];
    }
    e.flux[i] = f;
    e.psf[i] = p;
  }
  // C_y = S C_d Sᵀ
  std::vector<double> cy(no * no, 0.0);
  for (std::size_t a = 0; a < no; ++a) {
    for (std::size_t b = 0; b < no; ++b) {
      double acc = 0.0;
      for (std::size_t j = 0; j < ni; ++j) acc += S[a * ni + j] * cd[j] * S[b * ni + j];
      cy[a * no + b] = acc;
    }
  }
  e.flux_var.assign(no, 0.0);
  for (std::size_t a = 0; a < no; ++a) e.flux_var[a] = cy[a * no + a];

  // z = C_y^-1 f, y = C_y^-1 pi（独立稠密解）
  std::vector<double> bf(e.flux), bp(e.psf);
  const std::vector<double> z = solve_dense(cy, bf, static_cast<int>(no));
  const std::vector<double> yy = solve_dense(cy, bp, static_cast<int>(no));
  double pi_cinv_f = 0.0, pi_cinv_pi = 0.0;
  for (std::size_t i = 0; i < no; ++i) {
    pi_cinv_f += e.psf[i] * z[i];
    pi_cinv_pi += e.psf[i] * yy[i];
  }
  const double a = s.in.photometric_scale_a;
  e.Q = a * pi_cinv_f;
  e.W = a * a * pi_cinv_pi;
  e.F_hat = e.Q / e.W;
  e.var_F_hat = 1.0 / e.W;
  return e;
}

double rel(double a, double b) {
  const double d = std::fabs(a - b);
  const double s = std::max(1.0, std::max(std::fabs(a), std::fabs(b)));
  return d / s;
}

bool allclose(const std::vector<double>& a, const std::vector<double>& b, double tol) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (!(std::fabs(a[i] - b[i]) <= tol * std::max(1.0, std::fabs(b[i])))) return false;
  }
  return true;
}

bool has_hdu(const astrocs::aio::FitsVerifyResult& r, const std::string& ext) {
  for (const auto& h : r.hdus) {
    if (h.extname == ext) return true;
  }
  return false;
}

std::string hdu_bunit(const astrocs::aio::FitsVerifyResult& r, const std::string& ext) {
  for (const auto& h : r.hdus) {
    if (h.extname == ext) return h.bunit;
  }
  return "";
}

void write_json(const std::string& path, const json& j) {
  std::ofstream f(path);
  f << j.dump(2) << "\n";
}

void write_expected(const std::string& dir, const Scene& s, const Expected& e,
                    const std::string& mode, const ExportResult& r) {
  (void)s;
  json j;
  j["mode"] = mode;
  j["signal"] = (mode == "visualization") ? e.vis_signal : e.signal;
  j["variance"] = e.variance;
  j["flux"] = e.flux;
  j["flux_variance"] = e.flux_var;
  j["effective_psf"] = e.psf;
  j["Q"] = e.Q;
  j["W"] = e.W;
  j["F_hat"] = e.F_hat;
  j["hdu_names"] = r.hdu_names;
  j["signal_bunit"] = "ADU/px^2";
  j["variance_bunit"] = "ADU^2/px^4";
  j["flux_bunit"] = "ADU";
  j["flux_variance_bunit"] = "ADU^2";
  j["effective_psf_bunit"] = "1";
  write_json(dir + "/expected.json", j);
}

bool no_tmp_residue(const std::string& parent) {
  const std::string cmd = "ls -a '" + parent + "' 2>/dev/null | grep -c 'staging.tmp' || true";
  std::FILE* p = ::popen(cmd.c_str(), "r");
  if (!p) return true;
  char buf[64] = {0};
  if (::fgets(buf, sizeof(buf), p) == nullptr) buf[0] = '\0';
  ::pclose(p);
  return std::atoi(buf) == 0;
}

// ---------------------------------------------------------------------------
int run_positive(const std::string& art) {
  Scene s;
  if (!build_scene(&s)) return 1;
  const Expected e = compute_expected(s);
  mkdir_p(art);

  // ---- 模式 1: surface_brightness ----
  {
    const std::string dir = art + "/surface_brightness";
    ExportInputs in = s.in;
    in.measurement_capable = true;
    in.uncertainty_available = true;
    const ExportResult r = export_product(ExportMode::kSurfaceBrightness, s.grid, in,
                                          s.plan, dir, astrocs::aio::PublishOptions(),
                                          astrocs::aio::CancelFn());
    std::fprintf(stderr, "SB: status=%d code=%s reason=%s\n", (int)r.status, r.code.c_str(), r.reason.c_str());
    CHECK(r.status == p3rsmp::Status::Ok, "SB status Ok");
    CHECK(r.publish.status == astrocs::aio::PublishStatus::kOk, "SB publish ok");
    CHECK(r.publish.renamed, "SB renamed");
    CHECK(r.reopen.ok, "SB reopen ok");
    CHECK(r.provenance_reopen_check.ok(), "SB provenance reopen ok");
    CHECK(r.hdu_names.size() == 3, "SB 3 HDUs");
    CHECK(has_hdu(r.reopen, "VARIANCE"), "SB has VARIANCE");
    CHECK(!has_hdu(r.reopen, "FLUX"), "SB has no FLUX");
    CHECK(hdu_bunit(r.reopen, "") == "ADU/px^2", "SB primary BUNIT");
    CHECK(hdu_bunit(r.reopen, "VARIANCE") == "ADU^2/px^4", "SB variance BUNIT");
    CHECK(allclose(r.signal, e.signal, 1e-12), "SB signal vs independent R");
    CHECK(allclose(r.variance, e.variance, 1e-12), "SB var_out == sum c^2 u");
    write_expected(dir, s, e, "surface_brightness", r);
  }

  // ---- 模式 2: point_source_flux ----
  {
    const std::string dir = art + "/point_source_flux";
    ExportInputs in = s.in;
    in.measurement_capable = true;
    in.uncertainty_available = true;
    const ExportResult r = export_product(ExportMode::kPointSourceFlux, s.grid, in,
                                          s.plan, dir, astrocs::aio::PublishOptions(),
                                          astrocs::aio::CancelFn());
    std::fprintf(stderr, "PSF: status=%d code=%s reason=%s\n", (int)r.status, r.code.c_str(), r.reason.c_str());
    CHECK(r.status == p3rsmp::Status::Ok, "PSF status Ok");
    CHECK(r.reopen.ok, "PSF reopen ok");
    CHECK(r.provenance_reopen_check.ok(), "PSF provenance reopen ok");
    CHECK(r.hdu_names.size() == 6, "PSF 6 HDUs");
    CHECK(has_hdu(r.reopen, "FLUX"), "PSF has FLUX");
    CHECK(has_hdu(r.reopen, "FLUX_VARIANCE"), "PSF has FLUX_VARIANCE");
    CHECK(has_hdu(r.reopen, "EFFECTIVE_PSF"), "PSF has EFFECTIVE_PSF");
    CHECK(hdu_bunit(r.reopen, "FLUX") == "ADU", "PSF flux BUNIT ADU");
    CHECK(hdu_bunit(r.reopen, "FLUX_VARIANCE") == "ADU^2", "PSF flux var BUNIT ADU^2");
    CHECK(hdu_bunit(r.reopen, "EFFECTIVE_PSF") == "1", "PSF epsf BUNIT 1");
    CHECK(r.frame_is_output_recompute, "PSF Q/W output-frame recompute");
    CHECK(rel(r.Q, e.Q) < 1e-8, "PSF Q vs independent dense solve");
    CHECK(rel(r.W, e.W) < 1e-8, "PSF W vs independent dense solve");
    CHECK(rel(r.F_hat, e.F_hat) < 1e-8, "PSF F_hat == Q/W");
    CHECK(rel(r.var_F_hat, e.var_F_hat) < 1e-8, "PSF Var(F)==1/W");
    CHECK(allclose(r.flux, e.flux, 1e-9), "PSF flux vs independent S d");
    CHECK(allclose(r.effective_psf, e.psf, 1e-9), "PSF pi == S p");
    write_expected(dir, s, e, "point_source_flux", r);
  }

  // ---- 模式 3: visualization ----
  {
    const std::string dir = art + "/visualization";
    ExportInputs in = s.in;
    in.measurement_capable = false;
    in.uncertainty_available = false;
    const ExportResult r = export_product(ExportMode::kVisualization, s.grid, in,
                                          s.plan, dir, astrocs::aio::PublishOptions(),
                                          astrocs::aio::CancelFn());
    std::fprintf(stderr, "VIS: status=%d code=%s reason=%s\n", (int)r.status, r.code.c_str(), r.reason.c_str());
    CHECK(r.status == p3rsmp::Status::Ok, "VIS status Ok");
    CHECK(r.reopen.ok, "VIS reopen ok");
    CHECK(r.provenance_reopen_check.ok(), "VIS provenance reopen ok");
    CHECK(r.hdu_names.size() == 1, "VIS single HDU");
    CHECK(!has_hdu(r.reopen, "VARIANCE"), "VIS no VARIANCE");
    CHECK(!has_hdu(r.reopen, "IVAR"), "VIS no IVAR");
    CHECK(!has_hdu(r.reopen, "FLUX"), "VIS no FLUX");
    CHECK(!has_hdu(r.reopen, "EFFECTIVE_PSF"), "VIS no EFFECTIVE_PSF");
    CHECK(hdu_bunit(r.reopen, "") == "ADU/px^2", "VIS primary BUNIT");
    CHECK(allclose(r.signal, e.vis_signal, 1e-12), "VIS display == independent nearest");
    write_expected(dir, s, e, "visualization", r);
  }

  // ---- 模式 token 解析：legacy/Deferred 必须拒 ----
  {
    ExportMode m;
    CHECK(parse_export_mode("surface_brightness", &m) && m == ExportMode::kSurfaceBrightness,
          "parse sb");
    CHECK(parse_export_mode("point_source_flux", &m), "parse psf");
    CHECK(parse_export_mode("visualization", &m), "parse vis");
    CHECK(!parse_export_mode("auto", &m), "legacy auto rejected");
    CHECK(!parse_export_mode("psf_snr_power", &m), "deferred psf_snr_power rejected");
    CHECK(!parse_export_mode("0", &m), "legacy integer rejected");
  }
  return g_fails == 0 ? 0 : 1;
}

// 每个负例：注入 -> 必须非 Ok，且产物目录不得出现。
struct NegCase {
  const char* name;
  ExportMode mode;
  void (*mutate)(ExportInputs*);
  const char* expect_code_prefix;  // 允许的 code（逗号分隔）；空=仅要求非 Ok
};

void m_vis_meas(ExportInputs* in) { in->measurement_capable = true; in->uncertainty_available = true; }
void m_psf_no_epsf(ExportInputs* in) { in->effective_psf_present = false; }
void m_psf_fwhm(ExportInputs* in) { in->effective_psf_fwhm_only = true; }
void m_psf_qw(ExportInputs* in) { in->input_qw_resampled = true; }
void m_psf_no_a(ExportInputs* in) { in->photometric_scale_a = 0.0; }
void m_psf_psf_sum(ExportInputs* in) { for (double& v : in->psf_p) v *= 2.0; }
void m_sb_omega(ExportInputs* in) { in->flux_conversion_requested = true; in->force_omega_absent = true; }
void m_sb_diag(ExportInputs* in) { in->covariance_diagonal = true; }
void m_sb_bunit(ExportInputs* in) { in->force_variance_unit = "ADU^2"; }
void m_prov_key(ExportInputs* in) { in->omit_provenance_key = "k_corr"; }
void m_weight(ExportInputs* in) { in->inject_weight_source = "support"; }
void m_sb_unc(ExportInputs* in) { in->measurement_capable = true; in->uncertainty_available = false; in->uncertainty_unavailable_reason.clear(); }
void m_verify(ExportInputs* in) { in->force_publish_verify_fail = true; }
void m_kernel_nearest(ExportInputs* in) { in->kernel_id = "nearest"; }
void m_kernel_unreg(ExportInputs* in) { in->kernel_id = "bicubic"; }
void m_prov_key_flux(ExportInputs* in) { in->omit_provenance_key = "flux_conservation_factor"; }

bool match_code(const std::string& code, const std::string& prefixes) {
  if (prefixes.empty()) return true;
  std::string cur;
  for (std::size_t i = 0; i <= prefixes.size(); ++i) {
    if (i == prefixes.size() || prefixes[i] == ',') {
      if (!cur.empty() && code == cur) return true;
      cur.clear();
    } else {
      cur.push_back(prefixes[i]);
    }
  }
  return false;
}

int run_negative(const std::string& art) {
  Scene s;
  if (!build_scene(&s)) return 1;
  mkdir_p(art);
  const NegCase cases[] = {
      {"vis_measurement_capable", ExportMode::kVisualization, m_vis_meas, "G-P3-VIS-01"},
      {"psf_missing_epsf", ExportMode::kPointSourceFlux, m_psf_no_epsf, "G-P3-PSF-06"},
      {"psf_fwhm_only", ExportMode::kPointSourceFlux, m_psf_fwhm, "G-P3-PSF-06,G-P3-EPSF-01"},
      {"psf_qw_resampled", ExportMode::kPointSourceFlux, m_psf_qw, "G-P3-QW-01"},
      {"psf_missing_a", ExportMode::kPointSourceFlux, m_psf_no_a, "G-P3-PSF-05"},
      {"psf_psf_not_normalized", ExportMode::kPointSourceFlux, m_psf_psf_sum, "G-P3-PSF-02"},
      {"sb_flux_conversion_no_omega", ExportMode::kSurfaceBrightness, m_sb_omega, "G-P3-SB-01"},
      {"sb_diagonal_no_kernel", ExportMode::kSurfaceBrightness, m_sb_diag, "G-P3-SB-04,G-P3-COV-01"},
      {"sb_variance_bunit_not_quadratic", ExportMode::kSurfaceBrightness, m_sb_bunit, "G-P3-PROV"},
      {"prov_missing_kcorr", ExportMode::kSurfaceBrightness, m_prov_key, "G-P3-PROV"},
      {"prov_missing_flux_factor", ExportMode::kSurfaceBrightness, m_prov_key_flux, "G-P3-PROV"},
      {"weight_source_support", ExportMode::kSurfaceBrightness, m_weight, "G-P3-GLB-01"},
      {"sb_measurement_unc_unavailable_no_reason", ExportMode::kSurfaceBrightness, m_sb_unc, "G-P3-SB-02"},
      {"atomic_verify_fail_removes_product", ExportMode::kSurfaceBrightness, m_verify, "G-P3-IO"},
      {"kernel_nearest_continuous_field", ExportMode::kSurfaceBrightness, m_kernel_nearest, "G-P3-KRN-02"},
      {"kernel_unregistered_bicubic", ExportMode::kSurfaceBrightness, m_kernel_unreg, "G-P3-KRN-01"},
  };
  for (const NegCase& c : cases) {
    Scene sc;
    if (!build_scene(&sc)) return 1;
    ExportInputs in = sc.in;
    in.measurement_capable = (c.mode == ExportMode::kVisualization) ? false : true;
    in.uncertainty_available = (c.mode == ExportMode::kVisualization) ? false : true;
    c.mutate(&in);
    const std::string dir = art + "/neg_" + c.name;
    const ExportResult r = export_product(c.mode, sc.grid, in, sc.plan, dir,
                                          astrocs::aio::PublishOptions(),
                                          astrocs::aio::CancelFn());
    const std::string label = std::string("neg[") + c.name + "]";
    CHECK(r.status != p3rsmp::Status::Ok, (label + " must be red").c_str());
    CHECK(match_code(r.code, c.expect_code_prefix),
          (label + " expected code " + c.expect_code_prefix + " got " + r.code).c_str());
    // 失败不留可见半成品（目录不存在）；原子发布验证失败同理。
    CHECK(!dir_exists(dir + "/product.fits"), (label + " no visible product.fits").c_str());
    CHECK(no_tmp_residue(dir_exists(dir) ? dir : art),
          (label + " no tmp residue").c_str());
  }
  std::fprintf(stderr, "negative cases: %d, checks: %d, fails: %d\n",
               static_cast<int>(sizeof(cases) / sizeof(cases[0])), g_checks, g_fails);
  return g_fails == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s <positive|negative> <artifact_dir>\n", argv[0]);
    return 2;
  }
  const std::string cmd = argv[1];
  const std::string art = argv[2];
  if (cmd == "positive") return run_positive(art);
  if (cmd == "negative") return run_negative(art);
  std::fprintf(stderr, "unknown subcommand %s\n", cmd.c_str());
  return 2;
}
