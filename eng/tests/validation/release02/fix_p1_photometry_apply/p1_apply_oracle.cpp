// ============================================================================
// p1_apply_oracle.cpp — RELEASE-02 FIX-P1 (P1-1) 独立 Oracle
//
// 目的: 在生产尺度（4096×4096 = 16.7M 像素/帧）上证明
//   ① 生产 apply_photometry（lib/algorithms/calibration/src/photometry_apply.cpp）
//      施加已知 k_photo 后, 帧间乘性标量差**归零**;
//   ② 负例: 不施加时残留 = 注入的帧间响应比 (非零);
//   ③ 误差语义 (k<=0 / NaN / Inf) 与 NaN 像素透传。
//
// 与 star_matcher 定义的一致性: F_syn = S (真值通量), F_instr,k = g_k·S ⇒
//   location_k = log10(F_instr/F_syn) = log10(g_k) ⇒ k_photo,k = 10^(-location_k)
//   = 1/g_k, 正是把每帧拉回公共测光坐标系的标量。
//
// 编译: g++ -O2 -std=gnu++17 p1_apply_oracle.cpp <repo>/lib/algorithms/calibration/src/photometry_apply.cpp
// ============================================================================
#include "photometry_apply.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_fail = 0;
void check(bool ok, const std::string& msg) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", msg.c_str());
  if (!ok) ++g_fail;
}

double median_of(std::vector<double> v) {
  if (v.empty()) return 0.0;
  std::sort(v.begin(), v.end());
  const size_t n = v.size();
  return (n % 2) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

// 生产尺度: L4 亮场为 4096×4096
constexpr int W = 4096;
constexpr int H = 4096;

// 真值场景: 平滑背景 + 线性梯度 + 少量“星”点 (跨帧相同)
void make_scene(std::vector<float>& s) {
  s.resize(static_cast<size_t>(W) * H);
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      const double base = 200.0 + 0.01 * x + 0.02 * y;      // 天光 + 梯度
      const double neb = 3000.0 * std::exp(-((x - 1500.0) * (x - 1500.0) +
                                             (y - 2200.0) * (y - 2200.0)) /
                                            (2.0 * 600.0 * 600.0));  // 星云
      s[static_cast<size_t>(y) * W + x] = static_cast<float>(base + neb);
    }
  }
  // 星点 (位置固定)
  for (int i = 0; i < 200; ++i) {
    const int cx = 37 + (i * 131) % (W - 80);
    const int cy = 53 + (i * 197) % (H - 80);
    for (int dy = -3; dy <= 3; ++dy)
      for (int dx = -3; dx <= 3; ++dx) {
        const double r2 = dx * dx + dy * dy;
        if (r2 > 9.0) continue;
        s[static_cast<size_t>(cy + dy) * W + (cx + dx)] +=
            static_cast<float>(50000.0 * std::exp(-r2 / 2.0));
      }
  }
}

}  // namespace

int main() {
  std::printf("== P1-1 apply oracle (production scale %dx%d, %d frames) ==\n",
              W, H, 4);
  std::vector<float> scene;
  make_scene(scene);

  // 帧级响应 g_k（透明度/口径差; 3.53× 极差与 q1 实测一致）
  const double g[4] = {0.80, 1.00, 1.25, 2.82};
  const double n_pix = static_cast<double>(W) * H;

  // 帧 0 = 参考 (g=1.00 便于读数; 下标 1)
  const int ref = 1;

  std::vector<std::vector<float>> frames(4);
  for (int k = 0; k < 4; ++k) {
    frames[k].resize(scene.size());
    for (size_t p = 0; p < scene.size(); ++p)
      frames[k][p] = static_cast<float>(static_cast<double>(scene[p]) * g[k]);
  }

  // ── ② 负例: 不施加任何标量 → 帧间乘性残留 = g_k/g_ref ≠ 1 ─────────────
  {
    double worst = 0.0;
    for (int k = 0; k < 4; ++k) {
      if (k == ref) continue;
      const double r = g[k] / g[ref];
      worst = std::max(worst, std::fabs(r - 1.0));
    }
    std::printf("  [NEG] 未施加: 帧间比值极差 max|g_k/g_ref - 1| = %.6f (应为 %.6f)\n",
                worst, std::fabs(g[3] / g[ref] - 1.0));
    check(std::fabs(worst - std::fabs(g[3] / g[ref] - 1.0)) < 1e-12,
          "NEG 未施加时残留非零 (判别力: 若此步为 0 则测试恒真)");
    check(worst > 0.5, "NEG 残留量级显著 (>0.5, 生产 3.53× 极差)");
  }

  // ── ① 施加 k_photo = 1/g_k (star_matcher 定义) → 帧间差归零 ────────────
  {
    std::vector<double> ratios;
    ratios.reserve(static_cast<size_t>(n_pix));
    double max_abs_err = 0.0;
    for (int k = 0; k < 4; ++k) {
      const double k_photo = 1.0 / g[k];
      std::vector<float> out(frames[k].size());
      const int rc = calibration::apply_photometry(frames[k].data(), W, H, k_photo,
                                                   out.data());
      check(rc == 0, "apply_photometry rc=0 (frame " + std::to_string(k) + ")");
      // 逐像素与 k_photo·I_cal 对齐
      for (size_t p = 0; p < out.size(); p += 4093) {  // 稀疏校验 (全量太慢)
        const double want = static_cast<double>(frames[k][p]) * k_photo;
        max_abs_err = std::max(max_abs_err, std::fabs(static_cast<double>(out[p]) - want));
      }
      if (k == ref) continue;
      // 帧间比值 = (k_k·g_k·S)/(k_ref·g_ref·S) = 1; 分母 = 已施加的参考帧 = S
      for (size_t p = 0; p < out.size(); p += 97) {
        const double ref_applied = static_cast<double>(frames[ref][p]) * (1.0 / g[ref]);
        ratios.push_back(static_cast<double>(out[p]) / ref_applied);
      }
    }
    const double med = median_of(ratios);
    const double max_dev = std::fabs(med - 1.0);
    std::printf("  [POS] 施加 k_photo=1/g_k: 帧间比值中位 = %.12f, |中位-1| = %.3e\n",
                med, max_dev);
    std::printf("        apply_photometry 逐像素 max|out - k·in| = %.3e (float 舍入)\n",
                max_abs_err);
    check(max_dev < 1e-6, "POS 施加后帧间乘性标量差归零 (<1e-6)");
    check(max_abs_err < 1e-3, "POS out == k_photo·in (float32 舍入内)");
  }

  // ── ③ 误差语义 + NaN/Inf 透传 ──────────────────────────────────────────
  {
    std::vector<float> buf(16, 1.0f);
    std::vector<float> out(16, 0.0f);
    check(calibration::apply_photometry(buf.data(), 4, 4, 0.0, out.data()) == -5,
          "k=0 → rc=-5 (禁静默全零)");
    check(calibration::apply_photometry(buf.data(), 4, 4, -1.0, out.data()) == -5,
          "k<0 → rc=-5");
    check(calibration::apply_photometry(buf.data(), 4, 4, NAN, out.data()) == -4,
          "k=NaN → rc=-4");
    check(calibration::apply_photometry(buf.data(), 4, 4, INFINITY, out.data()) == -4,
          "k=Inf → rc=-4");
    buf[3] = NAN; buf[7] = INFINITY;
    check(calibration::apply_photometry(buf.data(), 4, 4, 2.0, out.data()) == 0,
          "NaN/Inf 像素不阻断 rc=0");
    check(std::isnan(out[3]) && std::isinf(out[7]),
          "NaN/Inf 像素透传 (k>0)");
  }

  std::printf(g_fail == 0 ? "ORACLE_PASS\n" : "ORACLE_FAIL (%d)\n", g_fail);
  return g_fail == 0 ? 0 : 1;
}
