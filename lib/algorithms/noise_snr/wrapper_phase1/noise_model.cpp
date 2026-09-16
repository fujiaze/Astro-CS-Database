// P1-005 NoiseModel 实现 (SCI-NOISE-001)
#include "noise_model.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace astrocs::phase1 {

using astrocs::core::Error;
using astrocs::core::ErrorDomain;

namespace {

// ── P10-UTIL2-002 (2026-09-14): 确定性并行 k-th 次序统计量 ───────────────
// 返回 vals 中第 k 小 (0-based) 的**精确值**; 不修改 vals。
// 前置: vals 非空且元素全为有限值 (调用方已过滤)。
//
// 算法 (分位枢轴 + 精确整数直方图 + 桶内选择):
//   1) 以**固定步长**从 vals 取固定数量 (<= kSampleMax) 样本 -> 串行排序 ->
//      按固定分位取固定桶边界。样本/边界只依赖 vals 内容与长度, 与线程数无关。
//   2) 按固定块 (kBlock 个元素 = 1 个 task) 并行统计桶计数; 每 task 只在栈上/
//      局部申请定长直方图 (kBucket*8 B), 再在 critical 区做**整数**累加。
//      => 计数精确、与任务/线程顺序无关; **内存与线程数完全解耦** (无逐线程
//         常驻缓冲): 常驻额外内存 = O(kBucket + kSampleMax)。
//   3) 定位 k 所在桶, 只在候选集中做 nth_element (同一多重集 -> 唯一次序统计量)
//      -> 结果与算法/线程数无关。候选集无实质缩小或已足够小 -> 直接 nth_element
//      (保证终止与最坏 O(n))。
//
// 科学等价性: 与旧实现 (std::sort 后取 [n/2]) 是**同一多重集的同一次序统计量**,
// 取值逐位相同。唯一浮点边角 = ±0.0 的同值不同符号 (operator< 视为相等), 由
// 调用方在结果为 0.0 时回退原排序路径保证逐位一致。
constexpr std::size_t kSelectDirect = 1u << 16;  // <= 该规模直接 nth_element
constexpr std::size_t kSelectSampleMax = 8192;   // 抽样上界 (固定, 与线程数无关)
constexpr std::size_t kSelectBuckets = 1024;     // 桶数 (固定)
constexpr std::size_t kSelectBlock = 1u << 14;   // 直方图 task 块 (固定)

double kth_smallest_deterministic(const std::vector<double>& vals, std::size_t k) {
  const std::size_t n = vals.size();
  if (n <= kSelectDirect) {
    std::vector<double> c = vals;
    std::nth_element(c.begin(), c.begin() + static_cast<std::ptrdiff_t>(k), c.end());
    return c[k];
  }
  // 1) 固定步长抽样 -> 固定分位桶边界 (与线程数无关)
  const std::size_t m = std::min(n, kSelectSampleMax);
  std::vector<double> samples;
  samples.reserve(m);
  const std::size_t stride = n / m;   // >= 1 (n > kSelectDirect >= m)
  for (std::size_t i = 0; i < m; ++i) samples.push_back(vals[i * stride]);
  std::sort(samples.begin(), samples.end());
  const std::size_t B = kSelectBuckets;
  std::vector<double> bd;               // B-1 个上界 (升序)
  bd.reserve(B - 1);
  for (std::size_t r = 1; r < B; ++r) {
    std::size_t idx = (m * r) / B;
    if (idx >= m) idx = m - 1;
    bd.push_back(samples[idx]);
  }
  const double* bd0 = bd.data();
  const std::size_t nb = bd.size();
  auto bucket_upper = [&](double v) -> std::size_t {
    std::size_t lo = 0, hi = nb;
    while (lo < hi) {
      const std::size_t mid = lo + (hi - lo) / 2;
      if (bd0[mid] <= v) lo = mid + 1; else hi = mid;
    }
    return lo;  // 第 1 个 > v 的上界下标 = 桶号 [0, B-1]
  };
  // 2) 分块并行直方图 (整数计数; critical 区整数累加 -> 与顺序无关)
  std::vector<uint64_t> hist(B, 0);
  const std::size_t nblk = (n + kSelectBlock - 1) / kSelectBlock;
#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic, 1)
#endif
  for (long long blk = 0; blk < static_cast<long long>(nblk); ++blk) {
    std::vector<uint64_t> local(B, 0);
    const std::size_t i0 = static_cast<std::size_t>(blk) * kSelectBlock;
    const std::size_t i1 = std::min(n, i0 + kSelectBlock);
    for (std::size_t i = i0; i < i1; ++i) local[bucket_upper(vals[i])]++;
#ifdef _OPENMP
#pragma omp critical(p10_kth_hist)
#endif
    {
      for (std::size_t b = 0; b < B; ++b) hist[b] += local[b];
    }
  }
  std::size_t tb = 0;
  uint64_t cum = 0;
  for (; tb < B; ++tb) {
    if (cum + hist[tb] > static_cast<uint64_t>(k)) break;
    cum += hist[tb];
  }
  if (tb >= B) {  // 防御: 计数异常 -> 直接 nth_element
    std::vector<double> c = vals;
    std::nth_element(c.begin(), c.begin() + static_cast<std::ptrdiff_t>(k), c.end());
    return c[k];
  }
  const std::size_t k2 = k - static_cast<std::size_t>(cum);
  // 3) 收集候选桶 (边界比较, 2 次浮点比较; 收集顺序无关)
  std::vector<double> cand;
  cand.reserve(static_cast<std::size_t>(hist[tb]));
#ifdef _OPENMP
#pragma omp parallel for schedule(dynamic, 1)
#endif
  for (long long blk = 0; blk < static_cast<long long>(nblk); ++blk) {
    std::vector<double> local;
    local.reserve(kSelectBlock / 8);
    const std::size_t i0 = static_cast<std::size_t>(blk) * kSelectBlock;
    const std::size_t i1 = std::min(n, i0 + kSelectBlock);
    for (std::size_t i = i0; i < i1; ++i) {
      const double v = vals[i];
      const bool ge_lo = (tb == 0) || !(v < bd[tb - 1]);
      const bool lt_hi = (tb == B - 1) || (v < bd[tb]);
      if (ge_lo && lt_hi) local.push_back(v);
    }
#ifdef _OPENMP
#pragma omp critical(p10_kth_cand)
#endif
    {
      cand.insert(cand.end(), local.begin(), local.end());
    }
  }
  if (cand.empty()) {
    std::vector<double> c = vals;
    std::nth_element(c.begin(), c.begin() + static_cast<std::ptrdiff_t>(k), c.end());
    return c[k];
  }
  if (cand.size() >= n || cand.size() * 2 >= n || cand.size() <= kSelectDirect) {
    std::nth_element(cand.begin(), cand.begin() + static_cast<std::ptrdiff_t>(k2),
                     cand.end());
    return cand[k2];
  }
  return kth_smallest_deterministic(cand, k2);
}

}  // namespace

astrocs::core::Result<NoiseResult> NoiseModel::estimate(
    const std::vector<float>& pixels) const {
  NoiseResult r;
  if (pixels.empty()) {
    r.valid = false;
    r.reason = "empty pixel set";
    return astrocs::core::Result<NoiseResult>::ok(r);
  }
  // median + MAD -> sigma (SCI 公式)
  std::vector<double> vals;
  vals.reserve(pixels.size());
  for (float v : pixels) {
    if (std::isfinite(v)) vals.push_back(v);
  }
  if (vals.size() < 3) {
    r.valid = false;
    r.reason = "insufficient finite pixels";
    return astrocs::core::Result<NoiseResult>::ok(r);
  }
  const std::size_t n = vals.size();
  const std::size_t k = n / 2;
  // P10-UTIL2-002: 次序统计量 = 精确值, 与线程数无关; 结果 0.0 时回退串行排序
  // 路径以消除 ±0.0 符号造成的逐位差异 (见 helper 注释)。
  double bg = kth_smallest_deterministic(vals, k);
  if (bg == 0.0) {
    std::vector<double> s = vals;
    std::sort(s.begin(), s.end());
    bg = s[k];
  }
  const double bgv = bg;
  std::vector<double> dev(n);
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
  for (long long i = 0; i < static_cast<long long>(n); ++i)
    dev[static_cast<std::size_t>(i)] =
        std::fabs(vals[static_cast<std::size_t>(i)] - bgv);
  double mad = kth_smallest_deterministic(dev, k);
  if (mad == 0.0) {
    std::vector<double> s = dev;
    std::sort(s.begin(), s.end());
    mad = s[k];
  }
  r.background = bg;
  r.sigma = kMadToSigma * mad;
  r.variance = r.sigma * r.sigma;
  if (r.variance < kVarianceFloor) r.variance = kVarianceFloor;  // clamp (SCI §4)
  r.ivar = 1.0 / r.variance;
  r.valid = true;
  return astrocs::core::Result<NoiseResult>::ok(r);
}

astrocs::core::Result<NoiseResult> NoiseModel::gain_variance(
    double signal, double gain, double read_noise_e) {
  NoiseResult r;
  // 零 gain 或无效 read noise (<=0): 诊断路径不入生产 (SCI §4)
  if (gain <= 0.0 || read_noise_e <= 0.0 || !std::isfinite(signal)) {
    r.valid = false;
    r.reason = (gain <= 0.0) ? "zero/invalid gain"
              : (read_noise_e <= 0.0) ? "invalid read_noise" : "invalid signal";
    return astrocs::core::Result<NoiseResult>::ok(r);
  }
  // variance = signal/gain + read_noise² (e⁻ 域 → ADU²)
  double var = signal / gain + read_noise_e * read_noise_e / (gain * gain);
  if (var < kVarianceFloor) var = kVarianceFloor;
  r.variance = var;
  r.ivar = 1.0 / var;
  r.sigma = std::sqrt(var);
  r.valid = true;
  return astrocs::core::Result<NoiseResult>::ok(r);
}

}  // namespace astrocs::phase1
