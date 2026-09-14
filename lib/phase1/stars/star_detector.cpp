// P1-003 StarDetector 实现
#include "star_detector.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>

namespace astrocs::phase1 {

using astrocs::core::Error;
using astrocs::core::ErrorDomain;

namespace {
// PSF-BG-001 (P2 性能, 判定值零变化, 负责人批准 2026-09-14):
// 分位数选择替代"每轮整图 std::sort"(16.2 M 像素 × 5 次)。
// std::nth_element(begin, begin+k, end) 保证位置 k 上的元素 == 整序后该位置
// 的元素, 因此 median / MAD / bg **逐位等于旧实现**(非近似直方图), 下游
// p1_sources.json / p1_flux.json 逐字节不变(REPORT.md §3 给 bit-pattern 对照)。
// 保留**精确**选择是硬约束: 近似分位会移动 5σ 阈值 → 候选集变 → p1_flux 变。
inline double nth_value(std::vector<double>& v, size_t count, size_t k) {
  std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(k),
                   v.begin() + static_cast<std::ptrdiff_t>(count));
  return v[k];
}
}  // namespace

StarDetector::StarDetector(double detection_sigma) : detection_sigma_(detection_sigma) {}

bool StarDetector::estimate_background(const float* image, int w, int h,
                                       double* bg, double* sigma) {
  if (!image || w <= 0 || h <= 0 || !bg || !sigma) return false;
  const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
  // 逐元素转换 (独立于候选的纯逐像素写, 可并行; 见 detect 扫描的并行注记)
  std::vector<double> keep(n);
  #pragma omp parallel for schedule(static)
  for (std::ptrdiff_t i = 0; i < static_cast<std::ptrdiff_t>(n); ++i)
    keep[static_cast<size_t>(i)] = image[i];
  std::vector<double> scratch(n);
  // sigma-clip 2 轮: median ± 3σ  (与旧实现同序、同值)
  for (int round = 0; round < 2; ++round) {
    const size_t kn = keep.size();
    std::copy(keep.begin(), keep.end(), scratch.begin());
    const double med = nth_value(scratch, kn, kn / 2);
    std::vector<double> dev(kn);
    #pragma omp parallel for schedule(static)
    for (std::ptrdiff_t i = 0; i < static_cast<std::ptrdiff_t>(kn); ++i)
      dev[static_cast<size_t>(i)] =
          std::fabs(keep[static_cast<size_t>(i)] - med);
    const double mad = nth_value(dev, kn, kn / 2);
    const double s = 1.4826 * mad;
    std::vector<double> filtered;
    filtered.reserve(kn);
    // 顺序保序过滤 (逐元素谓词独立, 但输出顺序承载 keep 的原序 → 串行 push_back)
    for (size_t i = 0; i < kn; ++i)
      if (std::fabs(keep[i] - med) <= 3.0 * (s > 0 ? s : 1e-9))
        filtered.push_back(keep[i]);
    if (filtered.empty()) break;
    keep = std::move(filtered);
  }
  const size_t kn = keep.size();
  std::copy(keep.begin(), keep.end(), scratch.begin());
  *bg = nth_value(scratch, kn, kn / 2);
  // 归一化顺序求和 (保持旧实现的串行求和顺序 → 逐位相同; 并行归约会改末位)
  double sum = 0;
  for (size_t i = 0; i < kn; ++i) sum += (keep[i] - *bg) * (keep[i] - *bg);
  *sigma = std::sqrt(sum / static_cast<double>(kn > 0 ? kn : 1));
  if (*sigma < 1e-9) *sigma = 1e-9;
  return true;
}

astrocs::core::Result<StarCatalog> StarDetector::detect(const float* image, int w, int h) const {
  if (!image || w <= 0 || h <= 0) {
    return astrocs::core::Result<StarCatalog>::fail(
        Error(ErrorDomain::DATA, "star_detector: bad image dims"));
  }
  StarCatalog cat;
  if (!estimate_background(image, w, h, &cat.background, &cat.noise_sigma)) {
    return astrocs::core::Result<StarCatalog>::fail(
        Error(ErrorDomain::DATA, "star_detector: background estimation failed"));
  }
  const double thr = cat.background + detection_sigma_ * cat.noise_sigma;

  // 1) 局部峰候选: 3x3 局部最大且 > thr
  struct Cand { int x, y; double val; };
  std::vector<Cand> cands;
  // PSF-DET-001 (P2 性能, 输出逐位不变): 扫描行并行 + 每线程本地缓冲后合并。
  // 与线程数无关的论证: 后续排序键 (val 降序 → x 升序 → y 升序) 在 (x,y) 唯一
  // 时构成**全序** (无相等键), 故 std::sort 的输出序列与输入顺序无关; 其后
  // kept / 质心 / 二阶矩 / id 全部按该固定序串行产出 ⇒ catalog 逐位确定。
  // 线程数不在此硬编码 (宪章 §10.4): 由 OpenMP 环境 (Runtime 线程预算) 决定;
  // 无 OpenMP 构建时 pragma 被忽略 → 串行回退, 结果不变。
  #pragma omp parallel
  {
    std::vector<Cand> local;
    #pragma omp for schedule(static) nowait
    for (int y = 1; y < h - 1; ++y) {
      for (int x = 1; x < w - 1; ++x) {
        const double v = image[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)];
        if (v < thr) continue;
        bool is_local_max = true;
        for (int dy = -1; dy <= 1 && is_local_max; ++dy)
          for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            if (image[static_cast<size_t>(y + dy) * static_cast<size_t>(w) + static_cast<size_t>(x + dx)] >= v) { is_local_max = false; break; }
          }
        if (is_local_max) local.push_back({x, y, v});
      }
    }
    #pragma omp critical(psf_det_cand_merge)
    cands.insert(cands.end(), local.begin(), local.end());
  }

  // 2) 去重: flux 降序 (tie breaker: 更左优先); 邻域 3x3 内只留最强
  std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) {
    if (a.val != b.val) return a.val > b.val;
    return a.x < b.x || (a.x == b.x && a.y < b.y);
  });
  std::vector<Cand> kept;
  std::vector<std::vector<bool>> taken(static_cast<size_t>(h),
                                       std::vector<bool>(static_cast<size_t>(w), false));
  for (const auto& c : cands) {
    if (taken[static_cast<size_t>(c.y)][static_cast<size_t>(c.x)]) continue;
    kept.push_back(c);
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        int ny = c.y + dy, nx = c.x + dx;
        if (ny >= 0 && ny < h && nx >= 0 && nx < w)
          taken[static_cast<size_t>(ny)][static_cast<size_t>(nx)] = true;
      }
  }

  // 3) 每候选: 质心 + 二阶矩 (FWHM/ellipticity) + 质量位
  uint32_t idx = 0;
  for (const auto& c : kept) {
    StarSource s;
    s.x = c.x; s.y = c.y;
    // 5x5 窗口质心 (背景扣除)
    double m00 = 0, m10 = 0, m01 = 0, m20 = 0, m02 = 0, m11 = 0;
    for (int dy = -2; dy <= 2; ++dy)
      for (int dx = -2; dx <= 2; ++dx) {
        int ny = c.y + dy, nx = c.x + dx;
        if (ny < 0 || ny >= h || nx < 0 || nx >= w) { s.quality |= 2; continue; }  // 边缘
        const double v = image[static_cast<size_t>(ny) * static_cast<size_t>(w) + static_cast<size_t>(nx)] - cat.background;
        if (v <= 0) continue;
        const double px = nx, py = ny;
        m00 += v; m10 += v * px; m01 += v * py;
        m20 += v * px * px; m02 += v * py * py; m11 += v * px * py;
      }
    if (m00 <= 0) continue;
    s.flux = m00;
    s.x = m10 / m00; s.y = m01 / m00;
    const double mu20 = m20 / m00 - s.x * s.x;
    const double mu02 = m02 / m00 - s.y * s.y;
    const double mu11 = m11 / m00 - s.x * s.y;
    const double theta = 0.5 * std::atan2(2 * mu11, mu20 - mu02);
    const double cos2 = std::cos(theta), sin2 = std::sin(theta);
    const double a2 = mu20 * cos2 * cos2 + 2 * mu11 * sin2 * cos2 + mu02 * sin2 * sin2;
    const double b2 = mu20 * sin2 * sin2 - 2 * mu11 * sin2 * cos2 + mu02 * cos2 * cos2;
    const double a = std::sqrt(std::max(a2, 1e-12));
    const double b = std::sqrt(std::max(b2, 1e-12));
    s.fwhm_px = 2.3548 * 0.5 * (a + b);
    s.ellipticity = (a >= b) ? (1.0 - b / a) : (1.0 - a / b);
    const double peak = image[static_cast<size_t>(c.y) * static_cast<size_t>(w) + static_cast<size_t>(c.x)];
    s.snr = (peak - cat.background) / cat.noise_sigma;
    // 饱和: 绝对幅值接近/超过 16bit 满井 (ADU 域; 不因高 SNR 误判)
    if (peak > 50000.0) s.quality |= 1;
    s.id = "src-" + std::to_string(idx++);
    cat.sources.push_back(std::move(s));
    if (s.quality & 1) ++cat.n_saturated;
    if (s.quality & 2) ++cat.n_edge;
  }
  cat.n_detected = static_cast<uint32_t>(cat.sources.size());
  return astrocs::core::Result<StarCatalog>::ok(std::move(cat));
}

}  // namespace astrocs::phase1
