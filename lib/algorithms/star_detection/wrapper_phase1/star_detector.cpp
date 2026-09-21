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

// ── RETIRED-CODE-RETAINED (ENGINEERING_SPEC §2 保留则注释) ─────────────
// WHAT:       StarDetector::estimate_background 的第三 σ 估计器（:67 的裁剪后 RMS，
//             即 StarCatalog::noise_sigma）；同函数另有 σ① 原始 MAD（:50）与 σ②
//             1.482602218505602·MAD（:51，仅用于 3σ 裁剪）。GAP_AUDIT G2-3 点名项。
// WHY-KEPT:   经 CLEAN-401 合成实验裁决为**保留**（不是退役、不是死代码）：
//             ① 生产可达 100%：module_adapters.cpp:10573 注册表 → :8882 execute →
//                :8884 p1_op_star_psf → :2329 impl → :2114 StarDetector(5.0) → :2126 detect()
//                → 本函数；下游 :82 检测阈值、:183 逐源 SNR、
//                module_adapters.cpp:2239-2240 写 p1_sources.json frames[].noise_sigma、
//                :4267 p1_op_noise 读作 cfg.sigma_sky_adu → 帧 SNR/深度 → drizzle → HiCS
//                ASTROCS_FRAME_SNR。删除它 = 改产品数值，违反数值等价前置。
//             ② 增益成立（合成星场，固定 seed=20260919，n=210 帧池化，C++ 探针直调生产实现
//                270/270 帧 σ 逐位一致）：相对冻结式 1.4826·MAD 的 mean|rel err| 增益
//                **+78.47%**（bootstrap 95% CI [+76.58%, +80.40%]）；偏差 +0.774%（星场）/
//                −1.401%（纯噪声，与 ±3σ 截断高斯解析值 −1.346% 差 0.06pp ⇒ 定义性低偏）。
//                注：GAP_AUDIT/TASK_LIST 记的「增益 +71.8%」**未复现**且全仓查无出处，
//                应以本实验口径为准（订正归 DOC-402）。
//             ③ 未改写成 1.4826·MAD：那正是本实验的基线（星场下差 78%）。
// STATUS:     在役生产（非退役件）；本块登记的是「为什么它不是待删死代码」与已发现缺陷。
//             **已知缺陷（本轮新发现；CLEAN-401 已按 fail-closed 修，见本函数 :107-110）**：
//             原 NaN 输入非 fail-closed ——
//             nth_element 遇 NaN 破坏严格弱序（UB）+ :68 的 `*sigma < 1e-9` 挡不住 NaN +
//             阈值退化为 ≈bg ⇒ 2% NaN 帧产生 6844 个假源（含 239 个非有限字段），
//             25% NaN ⇒ 8748（3721 非有限），全 NaN ⇒ 15564（全非有限）；
//             p1_read_image(module_adapters.cpp:1131) → det.detect(:2126) 之间无 isfinite 门。
//             已实施修法（两道）：① **入口**逐像素 isfinite 归约（与既有 O(n) 转换循环同趟，
//             `reduction(|:nonfinite)`），任一非有限 ⇒ return false —— 这是必需的，因为 NaN 占比
//             <50% 时中位数仍是有限值，只查返回值挡不住；② 返回前
//             `if (!std::isfinite(*bg) || !std::isfinite(*sigma)) return false;` 且把原
//             `*sigma < 1e-9` 改为 `!(*sigma > 0.0)`。detect()（:120-123）已对该 false 做
//             fail-closed（ErrorDomain::DATA "background estimation failed"）。有限输入的
//             逐位结果不变（转换循环的数值路径未改）。
//             回归锁定：eng/tests/unit/p1_stars_test.cpp 的 noise_sigma 组（NaN ⇒ 必败；
//             25 星 |σ/σ_true−1| ≤ 2%；纯噪声 −3% ≤ bias ≤ 0%）。
// EXIT:       本块无删除条件（该项裁决为保留）。仅当出现下列情形时改写：
//             ① 若 FIX 域另行改造 NaN 处置 ⇒ 同步本块「已知缺陷」段（当前状态：已修）；
//             ② 若 DOC-402 把 docs/science/NOISE_MODEL.md 的 σ 口径改为裁剪后 RMS ⇒ 本块
//                的「增益 vs MAD」对照口径同步更新；
//             ③ 若将来决定统一 σ 口径（改调 1.4826·MAD）⇒ 必须先做数值等价验证并同步
//                p1_sources.json / p1_snr 的容差与基线，不得直接替换。
// AUTHORITY:  ENGINEERING_SPEC.md §2（历史实现处置：保留则注释）/§3（科学代码红线）；
//             工程控制/RELEASE-04/GAP_AUDIT.md G2-3；
//             实验证据 run/CLEAN-401/third_sigma/{README.md,verify.log,results/metrics.json}；
//             docs/science/NOISE_MODEL.md:46（冻结的 σ_bg=1.4826·MAD 仍是注册口径，未改）。
// ──────────────────────────────────────────────────────────────────────
bool StarDetector::estimate_background(const float* image, int w, int h,
                                       double* bg, double* sigma) {
  if (!image || w <= 0 || h <= 0 || !bg || !sigma) return false;
  const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
  // 逐元素转换 (独立于候选的纯逐像素写, 可并行; 见 detect 扫描的并行注记)
  std::vector<double> keep(n);
  // CLEAN-401 缺陷修（fail-closed）：非有限像素必须在入口拦截 —— 下游 nth_element 对 NaN
  // 破坏严格弱序（UB），且 NaN 占比 <50% 时中位数仍是有限值 ⇒ 仅查返回值挡不住。
  // 本循环本就逐像素写 keep，加一个 | 归约不改变任何有限输入的逐位结果，也不改复杂度
  // （O(n)，与既有 O(n log n) 排序同阶）。
  int nonfinite = 0;
  #pragma omp parallel for schedule(static) reduction(| : nonfinite)
  for (std::ptrdiff_t i = 0; i < static_cast<std::ptrdiff_t>(n); ++i) {
    const double v = static_cast<double>(image[i]);
    keep[static_cast<size_t>(i)] = v;
    if (!std::isfinite(v)) nonfinite = 1;
  }
  if (nonfinite) return false;
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
    const double s = 1.482602218505602 * mad;
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
  // CLEAN-401 缺陷修（fail-closed）：原 `if (*sigma < 1e-9)` 对 NaN 判假 ⇒ NaN 帧
  // 会把 NaN 当 σ 放行（假源爆炸，见函数上方保留块「已知缺陷」）。非有限 ⇒ 显式失败。
  if (!std::isfinite(*bg) || !std::isfinite(*sigma)) return false;
  if (!(*sigma > 0.0)) *sigma = 1e-9;
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

  // 动态范围与饱和水平 (SCI-P1-STAR-001 §1 :20-23 / ALG-STARDET-001 §2 :37-42):
  //   maxi        = max(img)                       (逐帧由数据算, 非编译期常数)
  //   norm        = 65535                          (规范硬编码的 uint16 满量程, :39)
  //   dynrange    = min(maxi, norm) − bg           (bg = 帧背景中位数)
  //   minsatlevel = 0.7·dynrange ; satrange = 0.1·dynrange
  // 旧实现用绝对字面量 peak>50000.0 (CONFORM-SWEEP-1-005) 与上述三套规范机制都不同,
  // 对满井 <50000 的相机漏标、对高本底帧过标 ⇒ 已删除。
  double maxi = 0.0;
  {
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    for (size_t i = 0; i < n; ++i)
      if (image[i] > maxi) maxi = image[i];
  }
  const double kNorm = 65535.0;   // 规范常数 (ALG-STARDET-001 :39, DISP-STAR-006)
  const double dynrange = std::min(maxi, kNorm) - cat.background;
  const double minsatlevel = 0.7 * dynrange;
  const double satrange = 0.1 * dynrange;

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
    // 饱和判定 = 3×3 邻域双条件 (SCI-P1-STAR-001 §1 :20-23, ALG-STARDET-001 §2 :41-42):
    //   meanhigh/minhigh 取峰值 3×3 邻域中 ≥ thr 的像素 (超阈值像素)
    //   saturated ⇔ (meanhigh − bg ≥ 0.7·dynrange) ∧ (pixel0 − minhigh ≤ 0.1·dynrange)
    // 平台平坦性条件 (第二式) 使未达满井的纯高斯峰不再被误标; 满井平台星被正确标出。
    // 邻域恒在界内 (候选来自 x∈[1,w−2], y∈[1,h−2])。
    {
      double meanhigh = 0.0;
      double minhigh = 0.0;
      int nhigh = 0;
      for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
          if (dx == 0 && dy == 0) continue;
          const double v = image[static_cast<size_t>(c.y + dy) * static_cast<size_t>(w) +
                                 static_cast<size_t>(c.x + dx)];
          if (v >= thr) {
            if (nhigh == 0 || v < minhigh) minhigh = v;
            meanhigh += v;
            ++nhigh;
          }
        }
      if (nhigh > 0) {
        meanhigh /= static_cast<double>(nhigh);
        if (meanhigh - cat.background >= minsatlevel &&
            peak - minhigh <= satrange) {
          s.quality |= 1;
        }
      }
    }
    s.id = "src-" + std::to_string(idx++);
    cat.sources.push_back(std::move(s));
    if (s.quality & 1) ++cat.n_saturated;
    if (s.quality & 2) ++cat.n_edge;
  }
  cat.n_detected = static_cast<uint32_t>(cat.sources.size());
  return astrocs::core::Result<StarCatalog>::ok(std::move(cat));
}

}  // namespace astrocs::phase1
