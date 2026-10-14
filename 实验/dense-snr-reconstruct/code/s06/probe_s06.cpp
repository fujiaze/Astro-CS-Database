// probe_s06.cpp — S06（P4 重建统计目标与域外失败）生产重建器探针
//
// 只读链接生产静态库 libacsd_phase2_integrate.a（build/linux-sci，HEAD e31b2be66；
// 该目录与当前 HEAD 9f1e99f2a 逐文件相同，见 run/ACSDSDOC-02/S06/REPRO_P4.md）。
// 直接调用生产 acsd::v6::p2weight::SparseSnrReconstructor::prepare/eval。
//
// 覆盖 S06 交付项中必须走生产代码的三个面：
//   S06-D1  域外可检测性：2×2 / 3×3 / 全幅网格的显式状态机计数
//           （IN_DOMAIN / EXTRAPOLATED / OUT_OF_DOMAIN_REJECTED / NOT_APPLICABLE）
//   S06-D2  静默自然外推的可检测性：覆盖率声明 vs 实测，守卫必须判红
//   S06-D3  控制值退化状态：零值 / 负值 / 全 NaN / 部分 NaN / 常量网格 / NaN·Inf 查询坐标
//   S06-D4  值域钳制的偏差：钳制咬合像素 vs 非咬合像素的带符号相对误差
//
// 全部输入为内存合成真值场 + 合成控制点（零真实数据），固定 seed，无网络。
// 输出：argv[1] JSON。
//
// 编译见 code/s06_run_all.sh。

#include "acsd/weight_chain.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

using acsd::v6::p2weight::SparseReconstruction;
using acsd::v6::p2weight::SparseSnrLayer;
using acsd::v6::p2weight::SparseSnrPoint;
using acsd::v6::p2weight::SparseSnrReconstructor;
using acsd::v6::p2weight::SparseSnrSemantics;

// ---------------------------------------------------------------------------
// JSON 小工具
// ---------------------------------------------------------------------------
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
static std::string jvecs(const std::vector<std::string>& v) {
  std::string s = "[";
  for (size_t i = 0; i < v.size(); ++i) { if (i) s += ","; s += "\"" + v[i] + "\""; }
  return s + "]";
}

static std::vector<std::string> g_results;

// ---------------------------------------------------------------------------
// 层与网格构造（cell_center_v1：node = grid_origin + i*dx + (dx-1)/2）
// ---------------------------------------------------------------------------
struct Grid { int nx, ny; double dx, dy, gox, goy, x0, y0; };
static Grid make_grid(int nx, int ny, double dx, double dy, double gox = 0.0, double goy = 0.0) {
  Grid g;
  g.nx = nx; g.ny = ny; g.dx = dx; g.dy = dy; g.gox = gox; g.goy = goy;
  g.x0 = gox + (dx - 1.0) / 2.0;
  g.y0 = goy + (dy - 1.0) / 2.0;
  return g;
}
static SparseSnrLayer make_layer(const Grid& g, const std::vector<double>& vals,
                                 const std::string& op = "") {
  SparseSnrLayer L;
  L.present = true;
  L.semantics = SparseSnrSemantics::kAbsoluteFluxTypeSnr;
  L.regular_grid = true;
  L.nx = g.nx; L.ny = g.ny;
  L.x0 = g.x0; L.y0 = g.y0; L.dx = g.dx; L.dy = g.dy;
  L.grid_origin_x = g.gox; L.grid_origin_y = g.goy;
  L.grid_tol = 1e-6;
  L.reconstruction_operator = op;
  L.points.reserve((size_t)g.nx * g.ny);
  for (int j = 0; j < g.ny; ++j)
    for (int i = 0; i < g.nx; ++i)
      L.points.push_back({g.x0 + i * g.dx, g.y0 + j * g.dy, vals[(size_t)j * g.nx + i]});
  return L;
}

// ---------------------------------------------------------------------------
// S06-D1 域外可检测性：显式状态机
// ---------------------------------------------------------------------------
enum class QState : int {
  kInDomain = 0,           // eval ok 且 out_of_domain=false
  kExtrapolatedValue = 1,  // eval ok 但 out_of_domain=true（生产应为 0；有值却是域外 = 静默外推）
  kRejectedOutOfDomain = 2,// eval false（fail-closed 拒绝，无值）
  kNotApplicable = 3,      // prepare false ⇒ 整层不可重建
};
static const char* qstate_token(QState s) {
  switch (s) {
    case QState::kInDomain: return "IN_DOMAIN";
    case QState::kExtrapolatedValue: return "EXTRAPOLATED";
    case QState::kRejectedOutOfDomain: return "OUT_OF_DOMAIN_REJECTED";
    default: return "NOT_APPLICABLE";
  }
}

static void exp_domain_states(int W, int H) {
  struct C { const char* name; int nx, ny; double dx, dy; };
  const C cs[] = {
      {"2x2",   2, 2, 64.0, 64.0},
      {"3x3",   3, 3, 48.0, 48.0},
      {"3x3_full_cover", 3, 3, 64.0, 64.0},   // 192 视场上的全幅覆盖（零效应阴性）
      {"4x4",   4, 4, 48.0, 48.0},
      {"6x6",   6, 6, 32.0, 32.0},
      {"2x8_anisotropic", 2, 8, 96.0, 24.0},
  };
  for (const C& c : cs) {
    Grid g = make_grid(c.nx, c.ny, c.dx, c.dy);
    std::vector<double> v((size_t)g.nx * g.ny);
    for (int j = 0; j < g.ny; ++j)
      for (int i = 0; i < g.nx; ++i)
        v[(size_t)j * g.nx + i] = 10.0 + i + 2.0 * j;

    SparseSnrReconstructor rec;
    std::string err;
    bool ok = rec.prepare(make_layer(g, v), &err);

    long long cnt[4] = {0, 0, 0, 0};
    long long tot = 0;
    double in_min = 1e300, in_max = -1e300;
    if (ok) {
      for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
          ++tot;
          double val = 0.0;
          SparseReconstruction info;
          std::string e2;
          bool r = rec.eval((double)x, (double)y, &val, &info, &e2);
          if (!r) { cnt[(int)QState::kRejectedOutOfDomain]++; continue; }
          if (info.out_of_domain) { cnt[(int)QState::kExtrapolatedValue]++; continue; }
          cnt[(int)QState::kInDomain]++;
          in_min = std::min(in_min, val);
          in_max = std::max(in_max, val);
        }
    } else {
      cnt[(int)QState::kNotApplicable] = (long long)W * H;
      tot = (long long)W * H;
    }

    J j;
    j.kvs("id", "S06_D1_domain_states");
    j.kvs("case", c.name);
    j.kvi("nx", g.nx); j.kvi("ny", g.ny);
    j.kv("dx", g.dx); j.kv("dy", g.dy);
    j.kvs("declared_operator", "(default)");
    j.kvs("effective_operator_id", ok ? rec.operator_id() : "");
    j.kvb("prepare_ok", ok);
    j.kvs("err", err);
    j.kvs("status_token_in_domain", qstate_token(QState::kInDomain));
    j.kvs("status_token_extrapolated", qstate_token(QState::kExtrapolatedValue));
    j.kvs("status_token_rejected", qstate_token(QState::kRejectedOutOfDomain));
    j.kvs("status_token_not_applicable", qstate_token(QState::kNotApplicable));
    j.kvi("n_query", tot);
    j.kvi("n_in_domain", cnt[0]);
    j.kvi("n_extrapolated_value", cnt[1]);
    j.kvi("n_rejected_out_of_domain", cnt[2]);
    j.kvi("n_not_applicable", cnt[3]);
    j.kv("frac_in_domain", tot ? (double)cnt[0] / tot : std::nan(""));
    j.kv("frac_out_of_domain", tot ? (double)(cnt[1] + cnt[2]) / tot : std::nan(""));
    j.kv("est_min_in_domain", cnt[0] ? in_min : std::nan(""));
    j.kv("est_max_in_domain", cnt[0] ? in_max : std::nan(""));
    // 定义域盒（生产语义：cell 并集，最外半 cell 由端点节点常数延拓）
    j.kv("domain_lo_x", g.gox - 0.5);
    j.kv("domain_hi_x", g.gox + g.nx * g.dx - 0.5);
    j.kv("field_lo_x", 0.0);
    j.kv("field_hi_x", (double)(W - 1));
    // 守卫：prepare 成功 ⇒ 域外占比必须为 0（零效应阴性 + 已知注入阳性都要能分类）
    bool guard_pass = (!ok) || ((double)(cnt[1] + cnt[2]) / (double)tot == 0.0);
    j.kvb("G_domain_complete_prepare_implies_full_coverage", guard_pass);
    j.kvs("G_domain_complete_verdict", guard_pass ? "PASS" : "RED");
    j.kvs("status", ok ? "PASS" : "NOT_APPLICABLE");
    g_results.push_back(j.done());
  }
}

// ---------------------------------------------------------------------------
// S06-D2 静默自然外推的可检测性（消费方不看 out_of_domain 的对照臂）
// ---------------------------------------------------------------------------
static void exp_silent_extrapolation(int W, int H) {
  Grid g = make_grid(2, 2, 64.0, 64.0);  // 历史 55.6% 域外构型
  std::vector<double> v = {10.0, 13.0, 11.0, 12.0};

  SparseSnrReconstructor rec;
  std::string err;
  bool ok = rec.prepare(make_layer(g, v), &err);

  long long n_in = 0, n_out = 0, n_rej = 0;
  double in_s1 = 0, in_s2 = 0;
  // 静默外推对照臂：域外像素用最近节点常数延拓（不看 out_of_domain 的消费方会做的事）
  double naive_s1 = 0, naive_s2 = 0;
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x) {
      double val = 0.0;
      SparseReconstruction info;
      std::string e2;
      bool r = rec.eval((double)x, (double)y, &val, &info, &e2);
      bool inside = r && !info.out_of_domain;
      if (inside) { n_in++; in_s1 += val; in_s2 += val * val; }
      else { if (r) n_out++; else n_rej++; }
      // naive：域外用最近控制点值（不报状态、不置 NaN）
      double nv;
      if (inside) nv = val;
      else {
        int i = (x < g.x0) ? 0 : (x > g.x0 + g.dx ? 1 : (x < g.x0 + g.dx * 0.5 ? 0 : 1));
        int jj = (y < g.y0) ? 0 : (y > g.y0 + g.dy ? 1 : (y < g.y0 + g.dy * 0.5 ? 0 : 1));
        nv = v[(size_t)jj * 2 + i];
      }
      naive_s1 += nv; naive_s2 += nv * nv;
    }
  long long tot = (long long)W * H;
  double in_mu = n_in ? in_s1 / n_in : std::nan("");
  double in_std = n_in ? std::sqrt(std::max(0.0, in_s2 / n_in - in_mu * in_mu)) : std::nan("");
  double nv_mu = naive_s1 / tot;
  double nv_std = std::sqrt(std::max(0.0, naive_s2 / tot - nv_mu * nv_mu));

  J j;
  j.kvs("id", "S06_D2_silent_extrapolation");
  j.kvs("case", "2x2_claims_full_coverage");
  j.kvb("prepare_ok", ok);
  j.kvi("n_query", tot);
  j.kvi("n_in_domain", n_in);
  j.kvi("n_extrapolated_value", n_out);
  j.kvi("n_rejected", n_rej);
  j.kv("measured_in_domain_fraction", (double)n_in / tot);
  j.kv("consumer_claimed_coverage", 1.0);
  j.kv("in_domain_only_spatial_std", in_std);
  j.kv("naive_silent_extrapolation_spatial_std", nv_std);
  // 可检测性守卫：覆盖率声明 1.0 与实测不符 ⇒ 判红
  bool detected = std::fabs(1.0 - (double)n_in / tot) > 1e-12;
  j.kvb("G_silent_extrapolation_detected", detected);
  j.kvs("G_silent_extrapolation_verdict", detected ? "RED" : "PASS");
  j.kvb("negative_control_zero_effect_expected_red", false);
  j.kvs("status", detected ? "PASS" : "FAIL");
  g_results.push_back(j.done());
}

// ---------------------------------------------------------------------------
// S06-D3 控制值退化状态（零 / 负 / 全 NaN / 部分 NaN / 常量 / NaN·Inf 查询坐标）
// ---------------------------------------------------------------------------
static void exp_control_value_states(int W, int H) {
  Grid g = make_grid(4, 4, 48.0, 48.0);
  const size_t n = (size_t)g.nx * g.ny;

  auto emit = [&](const char* cname, bool prepare_ok, const std::string& err,
                  long long n_filled, double node_resid, const char* expect) {
    J j;
    j.kvs("id", "S06_D3_control_value_state");
    j.kvs("case", cname);
    j.kvb("prepare_ok", prepare_ok);
    j.kvs("err", err);
    j.kvi("n_invalid_control_points_filled", n_filled);
    j.kv("node_reproduction_max_abs", node_resid);
    j.kvs("expect", expect);
    bool verdict_ok = (std::string(expect) == "reject") ? (!prepare_ok)
                    : (std::string(expect) == "fill")   ? (prepare_ok && n_filled > 0)
                                                        : prepare_ok;
    j.kvs("verdict", verdict_ok ? "PASS" : "RED");
    g_results.push_back(j.done());
  };

  // 1) 任一控制值为 0
  {
    std::vector<double> v(n, 10.0); v[n / 2] = 0.0;
    SparseSnrReconstructor rec; std::string err;
    bool ok = rec.prepare(make_layer(g, v), &err);
    emit("single_zero_value", ok, err, (long long)rec.n_invalid_control_points_filled(),
         rec.node_reproduction_max_abs(), "reject");
  }
  // 2) 任一控制值为负
  {
    std::vector<double> v(n, 10.0); v[n / 2] = -1.0;
    SparseSnrReconstructor rec; std::string err;
    bool ok = rec.prepare(make_layer(g, v), &err);
    emit("single_negative_value", ok, err, (long long)rec.n_invalid_control_points_filled(),
         rec.node_reproduction_max_abs(), "reject");
  }
  // 3) 全 NaN
  {
    std::vector<double> v(n, std::nan(""));
    SparseSnrReconstructor rec; std::string err;
    bool ok = rec.prepare(make_layer(g, v), &err);
    emit("all_nan", ok, err, (long long)rec.n_invalid_control_points_filled(),
         rec.node_reproduction_max_abs(), "reject");
  }
  // 4) 部分 NaN（生产按最近有效控制点填充）
  {
    std::vector<double> v(n, 10.0);
    v[0] = std::nan(""); v[n - 1] = std::nan(""); v[n / 2] = std::nan("");
    SparseSnrReconstructor rec; std::string err;
    bool ok = rec.prepare(make_layer(g, v), &err);
    emit("partial_nan_nearest_fill", ok, err,
         (long long)rec.n_invalid_control_points_filled(),
         rec.node_reproduction_max_abs(), "fill");
  }
  // 5) 常量网格（退化为帧级常数）
  {
    std::vector<double> v(n, 7.0);
    SparseSnrReconstructor rec; std::string err;
    bool ok = rec.prepare(make_layer(g, v), &err);
    double s1 = 0, s2 = 0; long long cnt = 0;
    if (ok)
      for (int y = 0; y < H; y += 5)
        for (int x = 0; x < W; x += 5) {
          double val = 0; SparseReconstruction info; std::string e2;
          if (rec.eval((double)x, (double)y, &val, &info, &e2) && !info.out_of_domain) {
            s1 += val; s2 += val * val; ++cnt;
          }
        }
    double mu = cnt ? s1 / cnt : std::nan("");
    double sd = cnt ? std::sqrt(std::max(0.0, s2 / cnt - mu * mu)) : std::nan("");
    J j;
    j.kvs("id", "S06_D3_control_value_state");
    j.kvs("case", "constant_grid_degenerates_to_frame");
    j.kvb("prepare_ok", ok);
    j.kvs("err", err);
    j.kv("field_mean", mu);
    j.kv("field_std", sd);
    j.kvb("spatially_constant", sd == 0.0);
    j.kvs("verdict", (ok && sd == 0.0) ? "PASS" : "RED");
    g_results.push_back(j.done());
  }
  // 6) NaN / Inf 查询坐标
  {
    std::vector<double> v(n, 10.0);
    SparseSnrReconstructor rec; std::string err;
    bool ok = rec.prepare(make_layer(g, v), &err);
    struct Q { const char* name; double x, y; };
    const Q qs[] = {{"nan_x", std::nan(""), 10.0}, {"nan_y", 10.0, std::nan("")},
                    {"inf_x", std::numeric_limits<double>::infinity(), 10.0},
                    {"neg_x", -1e9, 10.0}};
    for (const Q& q : qs) {
      double val = 0; SparseReconstruction info; std::string e2;
      bool r = ok && rec.eval(q.x, q.y, &val, &info, &e2);
      J j;
      j.kvs("id", "S06_D3_query_state");
      j.kvs("case", q.name);
      j.kvb("prepare_ok", ok);
      j.kvb("eval_ok", r);
      j.kvb("out_of_domain", r ? info.out_of_domain : true);
      j.kv("value", r ? val : std::nan(""));
      j.kvs("state", (!ok) ? "NOT_APPLICABLE"
                           : (r ? (info.out_of_domain ? "EXTRAPOLATED" : "IN_DOMAIN")
                                : "OUT_OF_DOMAIN_REJECTED"));
      bool good = (!ok) || (!r) || info.out_of_domain;
      j.kvs("verdict", good ? "PASS" : "RED");
      g_results.push_back(j.done());
    }
  }
}

// ---------------------------------------------------------------------------
// S06-D4 值域钳制的偏差（钳制咬合像素 vs 非咬合像素）
// ---------------------------------------------------------------------------
static void exp_clip_bias(int W, int H) {
  // 真值场：节点尺度可分辨的棋盘式振荡（周期 32 px、节点间距 16 px ⇒ 节点在峰谷交替）
  auto truth = [](double x, double y) {
    return 10.0 + 5.0 * std::sin(2.0 * M_PI * x / 32.0) * std::sin(2.0 * M_PI * y / 32.0);
  };
  Grid g = make_grid(W / 16, H / 16, 16.0, 16.0);
  std::vector<double> v((size_t)g.nx * g.ny);
  for (int j = 0; j < g.ny; ++j)
    for (int i = 0; i < g.nx; ++i)
      v[(size_t)j * g.nx + i] = truth(g.x0 + i * g.dx, g.y0 + j * g.dy);

  struct Arm { const char* name; const char* op; };
  const Arm arms[] = {{"natural_bicubic_spline_clip_v1", ""},
                      {"bilinear_regular_grid_v1", "bilinear_regular_grid_v1"}};
  for (const Arm& a : arms) {
    SparseSnrReconstructor rec;
    std::string err;
    bool ok = rec.prepare(make_layer(g, v, a.op), &err);
    J j;
    j.kvs("id", "S06_D4_clip_bias");
    j.kvs("arm", a.name);
    j.kvb("prepare_ok", ok);
    j.kvs("err", err);
    j.kvs("effective_operator_id", ok ? rec.operator_id() : "");
    j.kvb("value_range_clipped", ok ? rec.value_range_clipped() : false);
    j.kv("clip_low", ok ? rec.clip_low() : std::nan(""));
    j.kv("clip_high", ok ? rec.clip_high() : std::nan(""));
    j.kv("node_min", *std::min_element(v.begin(), v.end()));
    j.kv("node_max", *std::max_element(v.begin(), v.end()));
    if (ok) {
      long long n_clip = 0, n_free = 0;
      long double sum_clip = 0, sum_free = 0;
      double worst_clip = 0.0;
      double worst_loc[2] = {0, 0};
      for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
          double val = 0; SparseReconstruction info; std::string e2;
          if (!rec.eval((double)x, (double)y, &val, &info, &e2)) continue;
          if (info.out_of_domain) continue;
          double t = truth((double)x, (double)y);
          double rel = (val - t) / t;
          bool at_bound = info.value_range_clipped &&
                          (val <= info.clip_low || val >= info.clip_high);
          if (at_bound) {
            ++n_clip; sum_clip += rel;
            if (std::fabs(rel) > std::fabs(worst_clip)) {
              worst_clip = rel; worst_loc[0] = x; worst_loc[1] = y;
            }
          } else { ++n_free; sum_free += rel; }
        }
      double mean_clip = n_clip ? (double)(sum_clip / n_clip) : std::nan("");
      double mean_free = n_free ? (double)(sum_free / n_free) : std::nan("");
      j.kvi("n_at_clip_bound", n_clip);
      j.kvi("n_not_at_clip_bound", n_free);
      j.kv("mean_rel_err_at_clip_bound", mean_clip);
      j.kv("mean_rel_err_not_at_clip_bound", mean_free);
      j.kv("clip_bias_excess", mean_clip - mean_free);
      j.kv("worst_rel_err_at_clip_bound", worst_clip);
      j.kv("worst_loc_x", worst_loc[0]);
      j.kv("worst_loc_y", worst_loc[1]);
      // 门：若钳制无偏，两个均值应当一致（容差 1e-3）；差异显著 ⇒ 钳制引入偏差 ⇒ 判红。
      // 不钳制的算子（bilinear）无可咬合像素 ⇒ 该门对其为 NOT_APPLICABLE（不得记 PASS，
      // 否则是无判别力的恒真绿）。
      bool clips = rec.value_range_clipped();
      bool biased = std::isfinite(mean_clip) && (std::fabs(mean_clip - mean_free) > 1e-3);
      j.kvb("G_clamp_is_unbiased", !biased);
      j.kvs("G_clamp_is_unbiased_verdict",
            !clips ? "NOT_APPLICABLE" : (biased ? "RED" : "PASS"));
      j.kvs("G_clamp_note",
            !clips ? "operator does not clip: gate has no discriminating power"
                   : "clipped vs unclipped signed relative error compared at 1e-3");
    }
    j.kvs("status", ok ? "PASS" : "NOT_APPLICABLE");
    g_results.push_back(j.done());
  }
}

// ---------------------------------------------------------------------------
// S06-D5 生产算子上的过冲检出：[0,2,0,0] 的平移模式 [1,3,1,1] 沿 x，y 向常数
//   ⇒ 可分离 2D 场退化为 1D 自然样条；未钳制样条在 x∈(11.5,19.5) 过冲 > 3
//   与 x∈(19.5,27.5) 下冲 < 1，被值域钳制咬合 ⇒ 钳制上下界像素计数 > 0。
//   （[0,2,0,0] 本身被生产拒绝：零值 non-positive ⇒ NOT_APPLICABLE。）
// ---------------------------------------------------------------------------
static void exp_overshoot_production(int W, int H) {
  // 4×4 控制点、dx=dy=8，节点 x = 3.5, 11.5, 19.5, 27.5
  Grid g = make_grid(4, 4, 8.0, 8.0);
  std::vector<double> v((size_t)g.nx * g.ny);
  const double pat[4] = {1.0, 3.0, 1.0, 1.0};
  for (int j = 0; j < g.ny; ++j)
    for (int i = 0; i < g.nx; ++i) v[(size_t)j * g.nx + i] = pat[i];

  SparseSnrReconstructor rec;
  std::string err;
  bool ok = rec.prepare(make_layer(g, v), &err);

  J j;
  j.kvs("id", "S06_D5_overshoot_production");
  j.kvs("pattern", "x=[1,3,1,1] (=[0,2,0,0]+1), y=const");
  j.kvb("prepare_ok", ok);
  j.kvs("err", err);
  j.kvs("effective_operator_id", ok ? rec.operator_id() : "");
  j.kv("node_min", *std::min_element(v.begin(), v.end()));
  j.kv("node_max", *std::max_element(v.begin(), v.end()));
  j.kv("clip_low", ok ? rec.clip_low() : std::nan(""));
  j.kv("clip_high", ok ? rec.clip_high() : std::nan(""));
  j.kvb("value_range_clipped", ok ? rec.value_range_clipped() : false);
  if (ok) {
    long long n_hi = 0, n_lo = 0, n = 0;
    double vmax = -1e300, vmin = 1e300;
    int xmax = 0;
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < W; ++x) {
        double val = 0; SparseReconstruction info; std::string e2;
        if (!rec.eval((double)x, (double)y, &val, &info, &e2)) continue;
        if (info.out_of_domain) continue;
        ++n;
        if (val > vmax) { vmax = val; xmax = x; }
        vmin = std::min(vmin, val);
        if (val >= info.clip_high) ++n_hi;
        if (val <= info.clip_low) ++n_lo;
      }
    j.kvi("n_in_domain", n);
    j.kvi("n_at_clip_high", n_hi);   // >0 ⇔ 未钳制样条在内部越过控制点最大值 3
    j.kvi("n_at_clip_low", n_lo);    // >0 ⇔ 未钳制样条在内部跌破控制点最小值 1
    j.kv("frac_at_clip_high", n ? (double)n_hi / n : std::nan(""));
    j.kv("frac_at_clip_low", n ? (double)n_lo / n : std::nan(""));
    j.kv("clipped_field_max", vmax);
    j.kv("clipped_field_min", vmin);
    j.kvi("argmax_x", xmax);
    j.kvb("unclipped_overshoot_above_node_max_detected", n_hi > 0);
    j.kvb("unclipped_undershoot_below_node_min_detected", n_lo > 0);
    j.kvs("verdict_upper_bound_guaranteed_by_spline_property", "CONTRADICTED");
    j.kvs("verdict_upper_bound_guaranteed_by_explicit_clipping",
          (n_hi > 0) ? "SUPPORTED" : "NOT_DEMONSTRATED");
    j.kvs("note", "[0,2,0,0] literal pattern is rejected by prepare "
                  "(non-positive control value); [+1] affine shift preserves the "
                  "shape comparison exactly.");
  }
  j.kvs("status", ok ? "PASS" : "NOT_APPLICABLE");
  g_results.push_back(j.done());
}

int main(int argc, char** argv) {
  const char* outp = (argc > 1) ? argv[1] : "probe_s06_results.json";
  int W = 192, H = 192;
  if (argc > 2) W = H = atoi(argv[2]);
  fprintf(stderr, "probe_s06: start (W=H=%d)\n", W);

  exp_domain_states(W, H);
  exp_silent_extrapolation(W, H);
  exp_control_value_states(W, H);
  exp_clip_bias(W, H);
  exp_overshoot_production(32, 32);

  FILE* f = fopen(outp, "w");
  if (!f) { fprintf(stderr, "cannot open %s\n", outp); return 2; }
  fprintf(f, "{\"probe\":\"probe_s06\",\"experiments\":[");
  for (size_t i = 0; i < g_results.size(); ++i) {
    if (i) fprintf(f, ",");
    fprintf(f, "%s", g_results[i].c_str());
  }
  fprintf(f, "]}\n");
  fclose(f);
  fprintf(stderr, "probe_s06: wrote %s (%zu experiments)\n", outp, g_results.size());
  return 0;
}
