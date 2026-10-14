// probe_p3_s02.cpp — S02 生产修复回归探针（P3 逐 drop 完备性 ε_j）
//
// 目的：用**同一份探针源码**分别链接「修前」与「修后」的 spherical_overlap.cpp
// 目标文件（见 code/build_and_run_s02.sh），给出修前/修后成对读数：
//   · E12_seam_ra_survey   —— 复刻 run/ACSDSDOC-01/T06 的 E12（可比）
//   · E13_perdrop_close    —— 复刻 T06 的 E13 位置×尺度矩阵（可比）
//   · S02_matrix_cell      —— 位置类 × nside × pixfrac × drop 尺度 的全矩阵
//   · S02_large_drop_reg      —— max_angle ≥ 1e-3 rad 的默认（叉积）路径回归锁
//
// 被估量（与 docs/science/drizzle/DRIZZLE.md「通量守恒门」逐字同口径）：
//   epsilon_j = Σ_p a_jp / A_drop,j − 1                （无量纲）
//   · 分子 Σ_p a_jp = 引擎累加量 Σ_p sumArea（sr）
//   · 分母 A_drop,j = 生产 build_drop_geometry 的 g.drop_area（sr，与被测路径同源）
//   · 1×1 帧、custom_x ≡ 1 ⇒ Σ_p sumFlux / x ≡ Σ_p w_jp = Σ_p a_jp/A_drop,j
//     （drizzle_engine.cpp: weight = overlap_area / drop_area）⇒ 生产直读等价
// 独立 oracle：l'Huilier 对角剖分（A_drop_lh，sr），不复用生产裁剪/面积例程。
//
// 全部输入为内存合成帧；无网络；无随机数（确定性）。
// 输出：argv[1] JSON。

#include "drizzle_engine.h"
#include "spherical_overlap.h"
#include "wcs_sip.h"
#include "healpix_core.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

using drizzle::DrizzleConfig;
using drizzle::DrizzleEngine;
using drizzle::DrizzleStats;
using drizzle::FitsImage;
using drizzle::PixelAccumulator;
using drizzle::WcsParams;
using drizzle::WcsSip;

static constexpr double PI = 3.14159265358979323846;
static constexpr double D2R = PI / 180.0;
static constexpr double R2D = 180.0 / PI;
static constexpr double A_FACE = PI / 3.0;
static constexpr double SEAM_DEC = 41.810314895778596;  // asin(2/3) 度

// ============================================================================
// JSON 小工具
// ============================================================================
struct J {
  std::string s = "{";
  bool first = true;
  void comma() { if (!first) s += ","; first = false; }
  void kv(const std::string& k, double v) {
    comma();
    if (!std::isfinite(v)) { s += "\"" + k + "\":null"; return; }
    char b[64]; snprintf(b, sizeof b, "%.17g", v); s += "\"" + k + "\":" + b;
  }
  void kvi(const std::string& k, long long v) { comma(); s += "\"" + k + "\":" + std::to_string(v); }
  void kvs(const std::string& k, const std::string& v) { comma(); s += "\"" + k + "\":\"" + v + "\""; }
  void kvb(const std::string& k, bool v) { comma(); s += "\"" + k + "\":" + (v ? "true" : "false"); }
  void raw(const std::string& k, const std::string& v) { comma(); s += "\"" + k + "\":" + v; }
  std::string done() { return s + "}"; }
};
static std::string jvec(const std::vector<double>& v) {
  std::string s = "[";
  for (size_t i = 0; i < v.size(); ++i) {
    if (i) s += ",";
    if (!std::isfinite(v[i])) { s += "null"; continue; }
    char b[64]; snprintf(b, sizeof b, "%.17g", v[i]); s += b;
  }
  return s + "]";
}

// ============================================================================
// 独立几何 oracle（l'Huilier 对角剖分；不复用生产面积/裁剪例程）
// ============================================================================
struct V3d { double x, y, z; };
static V3d v3(double ra_deg, double dec_deg) {
  double ra = ra_deg * D2R, de = dec_deg * D2R;
  return {std::cos(de) * std::cos(ra), std::cos(de) * std::sin(ra), std::sin(de)};
}
static V3d cross3(const V3d& a, const V3d& b) { return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; }
static double dot3(const V3d& a, const V3d& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static double norm3(const V3d& a) { return std::sqrt(dot3(a, a)); }
static V3d unit3(const V3d& a) { double n = norm3(a); return {a.x/n, a.y/n, a.z/n}; }
static V3d add3(const V3d& a, const V3d& b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
static double angdist(const V3d& a, const V3d& b) {
  return std::atan2(norm3(cross3(a, b)), dot3(a, b));
}
static double lhuilier(double a, double b, double c) {
  double s = 0.5 * (a + b + c);
  double t = std::tan(0.5*s) * std::tan(0.5*(s-a)) * std::tan(0.5*(s-b)) * std::tan(0.5*(s-c));
  if (!(t > 0.0)) return 0.0;
  return 4.0 * std::atan(std::sqrt(t));
}
static double area_lhuilier(const std::vector<V3d>& v) {
  if (v.size() < 3) return 0.0;
  V3d c{0,0,0};
  for (auto& p : v) c = add3(c, p);
  double n = norm3(c);
  if (n < 1e-300) return 0.0;
  c = {c.x/n, c.y/n, c.z/n};
  double A = 0.0;
  for (size_t i = 0; i < v.size(); ++i) {
    const V3d& p = v[i]; const V3d& q = v[(i+1) % v.size()];
    A += lhuilier(angdist(p,q), angdist(q,c), angdist(c,p));
  }
  return A;
}

// ============================================================================
// 帧构造 / 运行（与 T06 探针同构，保证 E12/E13 可与 T06 逐条对照）
// ============================================================================
struct FrameSpec {
  int w = 64, h = 64;
  double ra = 0.0, dec = 0.0;
  double scale_arcsec = 10.0;
  double rot_deg = 0.0;
  int sip_order = 0;
};
static WcsParams make_wcs(const FrameSpec& f) {
  WcsParams p{};
  p.has_wcs = true;
  p.crval[0] = f.ra; p.crval[1] = f.dec;
  p.crpix[0] = f.w / 2.0 + 0.5;
  p.crpix[1] = f.h / 2.0 + 0.5;
  double s = f.scale_arcsec / 3600.0;
  double t = f.rot_deg * D2R;
  p.cd[0] =  s * std::cos(t);  p.cd[1] = -s * std::sin(t);
  p.cd[2] =  s * std::sin(t);  p.cd[3] =  s * std::cos(t);
  strncpy(p.ctype1, "RA---TAN", sizeof(p.ctype1) - 1);
  strncpy(p.ctype2, "DEC--TAN", sizeof(p.ctype2) - 1);
  return p;
}

struct RunOut {
  bool ok = false;
  std::string err;
  std::unordered_map<uint64_t, PixelAccumulator> acc;
  DrizzleStats st;
};
struct Case {
  FrameSpec f;
  int nside = 4096;
  double pixfrac = 0.8;
  int precision = 1;
  int threads = 4;
  int mode = 0;                                     // 3 = custom_x
  std::function<double(double,double)> custom_x;
};
static RunOut run_case(const Case& c) {
  RunOut o;
  FitsImage img;
  img.width = c.f.w; img.height = c.f.h; img.channels = 1;
  img.wcs = make_wcs(c.f);
  img.pixels_f64.assign((size_t)c.f.w * c.f.h, 0.0);
  img.pixels.assign((size_t)c.f.w * c.f.h, 0.0f);
  img.use_f64 = (c.precision == 1);
  for (int y = 0; y < c.f.h; ++y)
    for (int x = 0; x < c.f.w; ++x) {
      size_t k = (size_t)y * c.f.w + x;
      double xv = (c.mode == 3 && c.custom_x) ? c.custom_x(x, y) : 1.0;
      img.pixels_f64[k] = xv;
      img.pixels[k] = (float)xv;
    }
  DrizzleConfig cfg;
  cfg.nside = c.nside; cfg.nested = true; cfg.pixfrac = c.pixfrac;
  cfg.precision_mode = (uint8_t)c.precision; cfg.threads = c.threads;
  DrizzleEngine eng;
  o.ok = eng.drizzle_f64(img, cfg, nullptr, nullptr, nullptr, o.acc, o.st, o.err);
  return o;
}

// ============================================================================
// 单 drop 逐 drop 完备性：1x1 帧 ⇒ Σ_p sumFlux / x ≡ Σ_p w_jp
// ============================================================================
struct DropClose {
  bool ok = false;
  double sumw_m1 = 0.0;      // eps_j（引擎侧，无量纲）
  double abs_err_sr = 0.0;   // Σ_p a_jp − A_drop_lh（sr，独立 oracle 分母）
  size_t nleaf = 0;          // 有贡献叶数
  double sum_area = 0.0;     // Σ_p a_jp（sr）
  double adrop_lh = 0.0;     // 独立 l'Huilier A_drop（sr）
  double adrop_prod = 0.0;   // 生产 build_drop_geometry g.drop_area（sr，ε 的分母）
  double max_angle = 0.0;    // 生产 max_angle（rad）
  long long n_cand = 0;      // query_candidate_pixels 候选数
  long long n_cand_fast = 0; // query_candidate_pixels_fast 候选数
  bool fast_fallback = false;
};

static void drop_corners_sky(const WcsSip& wcs, double pixfrac, std::vector<V3d>& out) {
  out.clear();
  const double dx[4] = {-0.5, +0.5, +0.5, -0.5};
  const double dy[4] = {-0.5, -0.5, +0.5, +0.5};
  for (int i = 0; i < 4; ++i) {
    double ra, dec;
    wcs.pixelToSky(pixfrac * dx[i], pixfrac * dy[i], ra, dec);
    out.push_back(v3(ra, dec));
  }
}

static DropClose drop_close(double ra, double dec, double scale_arcsec, int nside,
                            double pixfrac, double rot_deg = 0.0) {
  FrameSpec f; f.w = 1; f.h = 1; f.ra = ra; f.dec = dec;
  f.scale_arcsec = scale_arcsec; f.rot_deg = rot_deg;
  Case c; c.f = f; c.nside = nside; c.pixfrac = pixfrac; c.mode = 3;
  c.custom_x = [](double, double) { return 1.0; };
  RunOut o = run_case(c);
  DropClose d;
  d.ok = o.ok; d.nleaf = o.acc.size();
  double fsum = 0, asum = 0;
  for (auto& kv : o.acc) { fsum += kv.second.sumFlux; asum += kv.second.sumArea; }
  d.sumw_m1 = fsum - 1.0;
  WcsSip wcs(make_wcs(f));
  std::vector<V3d> cor;
  drop_corners_sky(wcs, pixfrac, cor);
  d.adrop_lh = area_lhuilier(cor);
  {
    std::vector<spherical::Vec3> cv(cor.size());
    for (size_t i = 0; i < cor.size(); ++i) cv[i] = spherical::Vec3{cor[i].x, cor[i].y, cor[i].z};
    spherical::DropGeometryT<double> g = spherical::build_drop_geometry<double>(cv);
    d.adrop_prod = g.drop_area;
    d.max_angle = g.max_angle;
    // 候选枚举（生产公开 API；与引擎内部 fast/queryDisc 路径分别读）
    healpix::HealpixCore hp(nside);
    std::vector<uint64_t> cand;
    spherical::query_candidate_pixels<double>(cv, hp, cand);
    d.n_cand = (long long)cand.size();
    std::vector<uint64_t> cand2;
    bool fb = false;
    spherical::query_candidate_pixels_fast<double>(cv, hp, cand2, &fb);
    d.n_cand_fast = (long long)cand2.size();
    d.fast_fallback = fb;
  }
  d.sum_area = asum;
  d.abs_err_sr = asum - d.adrop_lh;
  return d;
}

static std::vector<std::string> g_results;

// 单位换算：像素角边长 scale_arcsec [″] → rad
static double arcsec_to_rad(double as) { return as / 3600.0 * D2R; }

// ============================================================================
// E12 复刻：|z|=2/3 面缝上按 RA 扫描（0.04″、nside=8192、pixfrac=0.8）
// ============================================================================
static void exp_e12_seam_ra_survey() {
  const std::vector<double> raList = {0, 5, 10, 15, 30, 45, 60, 90, 135, 180, 225, 270};
  std::vector<double> nlv, rel, cand;
  for (double ra : raList) {
    DropClose d = drop_close(ra, SEAM_DEC, 0.04, 8192, 0.8);
    nlv.push_back((double)d.nleaf); rel.push_back(d.sumw_m1); cand.push_back((double)d.n_cand);
  }
  J j; j.kvs("id", "E12_seam_ra_survey");
  j.kv("dec", SEAM_DEC); j.kvi("nside", 8192); j.kv("scale_arcsec", 0.04); j.kv("pixfrac", 0.8);
  j.raw("ra", jvec(raList)); j.raw("n_leaves", jvec(nlv)); j.raw("sumw_m1", jvec(rel));
  j.raw("n_candidates_query", jvec(cand));
  g_results.push_back(j.done());
}

// ============================================================================
// E13 复刻：三位置类 × 9 尺度 × 3 nside（pixfrac=0.8）
// ============================================================================
static void exp_e13_perdrop_close() {
  const std::vector<double> scales = {0.005, 0.02, 0.1, 0.5, 2.0, 10.0, 60.0, 150.0, 290.0};
  for (int nside : {512, 4096, 16384}) {
    uint64_t p0 = (uint64_t)nside * nside / 4;
    double raA, decA, raB, decB;
    acsd::healpix::pix2ang_nest((uint32_t)nside, p0 * 4 + 0, raA, decA);
    acsd::healpix::pix2ang_nest((uint32_t)nside, p0 * 4 + 1, raB, decB);
    V3d mid = unit3(add3(v3(raA, decA), v3(raB, decB)));
    double raM = std::atan2(mid.y, mid.x) * R2D; if (raM < 0) raM += 360;
    double decM = std::asin(mid.z) * R2D;
    struct Spot { const char* cls; double ra, dec; };
    Spot spots[3] = {{"interior", 123.456, -33.333}, {"leaf_edge", raM, decM}, {"face_seam", 0.0, SEAM_DEC}};
    for (auto& sp : spots) {
      std::vector<double> rel, absv, nlv;
      for (double sc : scales) {
        DropClose d = drop_close(sp.ra, sp.dec, sc, nside, 0.8);
        rel.push_back(d.sumw_m1); absv.push_back(d.abs_err_sr); nlv.push_back((double)d.nleaf);
      }
      J j; j.kvs("id", "E13_perdrop_close"); j.kvs("pos_class", sp.cls);
      j.kvi("nside", nside); j.kv("pixfrac", 0.8);
      j.kv("ra", sp.ra); j.kv("dec", sp.dec);
      j.raw("scale_arcsec", jvec(scales));
      j.raw("sumw_m1", jvec(rel));
      j.raw("abs_err_sr", jvec(absv));
      j.raw("n_leaves", jvec(nlv));
      g_results.push_back(j.done());
    }
  }
}

// ============================================================================
// S02 全矩阵：位置类 × nside × pixfrac × drop 尺度
// ============================================================================
struct PosCell { const char* cls; double ra, dec; };
static void exp_s02_matrix(const std::vector<int>& nsides,
                           const std::vector<double>& pfs,
                           const std::vector<double>& scales) {
  double seam_dec = SEAM_DEC;
  for (int nside : nsides) {
    // 位置集：每次 nside 重新解析（叶边中点/叶心随 nside 变）
    uint64_t p0 = (uint64_t)nside * nside / 4;
    double raA, decA, raB, decB;
    acsd::healpix::pix2ang_nest((uint32_t)nside, p0 * 4 + 0, raA, decA);
    acsd::healpix::pix2ang_nest((uint32_t)nside, p0 * 4 + 1, raB, decB);
    V3d mid = unit3(add3(v3(raA, decA), v3(raB, decB)));
    double raM = std::atan2(mid.y, mid.x) * R2D; if (raM < 0) raM += 360;
    double decM = std::asin(mid.z) * R2D;
    double raC, decC;
    acsd::healpix::pix2ang_nest((uint32_t)nside, p0 * 4 + 0, raC, decC);  // 叶心

    std::vector<PosCell> cells;
    for (double ra : {45.0, 135.0, 225.0, 315.0}) cells.push_back({"face_vertex_n", ra, seam_dec});
    for (double ra : {45.0, 135.0, 225.0, 315.0}) cells.push_back({"face_vertex_s", ra, -seam_dec});
    cells.push_back({"leaf_edge_mid", raM, decM});
    cells.push_back({"leaf_center", raC, decC});
    cells.push_back({"leaf_interior_generic", 123.456, -33.333});
    for (double ra : {0.0, 5.0, 10.0, 15.0, 30.0, 60.0, 90.0, 180.0, 270.0, 300.0, 330.0, 350.0})
      cells.push_back({"face_seam", ra, seam_dec});
    cells.push_back({"polar_89p99", 0.0, 89.99});
    cells.push_back({"polar_exact_90", 0.0, 90.0});

    for (const auto& pc : cells) {
      for (double pf : pfs) {
        for (double sc : scales) {
          DropClose d = drop_close(pc.ra, pc.dec, sc, nside, pf);
          J j; j.kvs("id", "S02_matrix_cell");
          j.kvs("pos_class", pc.cls); j.kv("ra", pc.ra); j.kv("dec", pc.dec);
          j.kvi("nside", nside); j.kv("pixfrac", pf); j.kv("scale_arcsec", sc);
          j.kv("drop_side_arcsec", pf * sc);
          j.kv("max_angle_rad", d.max_angle);
          j.kvb("ok", d.ok);
          j.kv("epsilon_j", d.sumw_m1);
          j.kv("abs_resid_sr_engine_vs_lh", d.abs_err_sr);
          j.kv("sum_area_sr", d.sum_area);
          j.kv("adrop_prod_sr", d.adrop_prod);
          j.kv("adrop_lhuilier_sr", d.adrop_lh);
          j.kvi("n_leaves", (long long)d.nleaf);
          j.kvi("n_candidates_query", d.n_cand);
          j.kvi("n_candidates_fast", d.n_cand_fast);
          j.kvb("fast_used_fallback", d.fast_fallback);
          g_results.push_back(j.done());
        }
      }
    }
  }
}

// ============================================================================
// 大 drop 默认（叉积）路径回归锁：max_angle ≥ 1e-3 rad
// ============================================================================
static void exp_large_drop_regression() {
  // 1e-3 rad 阈值 ⇒ scale* = 291.7025/pixfrac ″；取明显高于阈值的点
  const std::vector<double> scales = {400.0, 700.0, 1200.0};
  const double B0 = 1000.0;
  for (int nside : {512, 4096, 16384}) {
    for (double pf : {0.5, 0.8, 1.0}) {
      for (double sc : scales) {
        // (a) 逐 drop 完备性（1x1）
        DropClose d = drop_close(45.0, 20.0, sc, nside, pf);
        // (b) 常量面亮度场（16x16，走同一 overlap 路径）
        FrameSpec f; f.w = 16; f.h = 16; f.scale_arcsec = sc; f.ra = 45.0; f.dec = 20.0;
        Case c; c.f = f; c.nside = nside; c.pixfrac = pf; c.mode = 3;
        c.custom_x = [=](double, double) { return B0; };  // 每像元常量 ADU（仅作路径回归锁）
        RunOut o = run_case(c);
        double fsum = 0, nsum = 0;
        for (auto& kv : o.acc) { fsum += kv.second.sumFlux; nsum += kv.second.sumNorm; }
        J j; j.kvs("id", "S02_large_drop_reg");
        j.kvi("nside", nside); j.kv("pixfrac", pf); j.kv("scale_arcsec", sc);
        j.kv("max_angle_rad", d.max_angle);
        j.kvb("n_tangent_path", d.max_angle < 1e-3);
        j.kv("sumw_m1", d.sumw_m1);
        j.kvi("n_leaves", (long long)d.nleaf);
        j.kv("const_adu_sumFlux", fsum);
        j.kv("const_adu_sumNorm", nsum);
        j.kvb("ok", o.ok);
        g_results.push_back(j.done());
      }
    }
  }
}

int main(int argc, char** argv) {
  const char* outp = (argc > 1) ? argv[1] : "probe_p3_s02.json";
  const char* tag = (argc > 2) ? argv[2] : "unspecified";
  fprintf(stderr, "probe_p3_s02: start (%s)\n", tag);
  exp_e12_seam_ra_survey();
  exp_e13_perdrop_close();
  exp_s02_matrix({512, 4096, 8192, 16384}, {0.5, 0.8, 1.0}, {0.04, 0.5, 2.0, 10.0, 60.0});
  exp_large_drop_regression();
  FILE* f = fopen(outp, "w");
  if (!f) { fprintf(stderr, "cannot open %s\n", outp); return 2; }
  fprintf(f, "{\"probe\":\"probe_p3_s02\",\"run_tag\":\"%s\",\"experiments\":[", tag);
  for (size_t i = 0; i < g_results.size(); ++i) {
    if (i) fprintf(f, ",");
    fprintf(f, "%s", g_results[i].c_str());
  }
  fprintf(f, "]}\n");
  fclose(f);
  fprintf(stderr, "probe_p3_s02: wrote %s (%zu experiments)\n", outp, g_results.size());
  return 0;
}
