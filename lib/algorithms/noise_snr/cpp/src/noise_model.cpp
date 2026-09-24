// noise_model.cpp — SNR/Noise 科学重构实现
//
// 三层模型 (SNR_SCIENCE_DERIVATION.md / SNR_REDESIGN_CONTRACT.md):
// 1. PhotometricCalibrationQuality — 帧级测光定标质量 (dex/mag 单位正确化)
// 2. PsfFitQuality — 星点 PSF 拟合质量代理 (A/residual_scale)
// 3. NoiseWeightModelV1 — source-masked blank-sky 稳健方差 → ivar
//
// 设计要点:
// - 控制点来自空背景噪声, 与星亮度/星族解耦 (SNR-003/SNR-010)
// - 经验 blank-sky 为 production 基线; gain+readnoise 已知时用于
// Poisson 交叉验证 (SNR-005), 缺失时经验 fallback (SNR-014)
// - variance/ivar 传播遵循 x'=αx → var'=α²var, ivar'=ivar/α² (SNR-002)
// - 旧 (A-B)/mad 不再进入 science weight (SNR-008)

#include "snr_estimator.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>
#include <unordered_map>
#include <mutex>      // PERF-P1: floor 注册表的线程安全（帧级并行下多 worker 并发 build/fill/free）

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

static std::unordered_map<const NoiseWeightModelV1*, double> g_model_floor;
// G3-6: variance_floor 钳位触发计数（build 期间被 floor 抬升的数值个数:
// 全局兜底 + 逐控制点）。0 = 未触发。注册表与 g_model_floor 同生命周期。
static std::unordered_map<const NoiseWeightModelV1*, int64_t> g_model_floor_clamp;

// ── PERF-P1 (RELEASE-05): 注册表线程安全 ─────────────────────────────────────
// 上述两个注册表是**按模型指针键控的进程级 map**。生产 P1 节点在帧级并行下会由
// 多个 worker **并发**调用 snr_noise_model_v1[_f64]（build）/ _fill（读 floor）/
// _free（erase）：noise-snr 节点逐帧建模型，drizzle 节点逐帧建模型并 fill 逐像素
// variance。并发写 std::unordered_map 是未定义行为（可能崩溃或损坏），故全部
// 访问统一经本互斥量；**热路径零加锁**（build 期钳位计数改为局部累加，结束时
// 一次写入），科学数值与调用序列语义零改动。
static std::mutex g_model_registry_mutex;

// build 期登记 floor 并把钳位计数清零（原 :317-318 语义）。
static void registry_register_model(const NoiseWeightModelV1* m, double floor_var) {
    std::lock_guard<std::mutex> lk(g_model_registry_mutex);
    g_model_floor[m] = floor_var;
    g_model_floor_clamp[m] = 0;
}

// 显式绑定 floor，**不动**既有钳位计数（原 _bind_variance_floor 语义）。
static void registry_bind_floor(const NoiseWeightModelV1* m, double floor_var) {
    std::lock_guard<std::mutex> lk(g_model_registry_mutex);
    g_model_floor[m] = floor_var;
}

// build 结束时一次性累加钳位计数（n==0 时不产生访问，避免无谓加锁）。
static void registry_add_clamp(const NoiseWeightModelV1* m, int64_t n) {
    if (n == 0) return;
    std::lock_guard<std::mutex> lk(g_model_registry_mutex);
    g_model_floor_clamp[m] += n;
}

// 读 floor；未绑定或非法 ⇒ false（调用方按 SNR_FLOOR_UNBOUND fail-closed）。
static bool registry_get_floor(const NoiseWeightModelV1* m, double* out) {
    std::lock_guard<std::mutex> lk(g_model_registry_mutex);
    auto it = g_model_floor.find(m);
    if (it == g_model_floor.end() || !std::isfinite(it->second) ||
        it->second <= 0.0) {
        return false;
    }
    *out = it->second;
    return true;
}

// 读钳位计数；未注册 ⇒ -1（显式"未知"，不冒充 0）。
static int64_t registry_get_clamp(const NoiseWeightModelV1* m) {
    std::lock_guard<std::mutex> lk(g_model_registry_mutex);
    auto it = g_model_floor_clamp.find(m);
    return it == g_model_floor_clamp.end() ? (int64_t)-1 : it->second;
}

static void registry_erase(const NoiseWeightModelV1* m) {
    std::lock_guard<std::mutex> lk(g_model_registry_mutex);
    g_model_floor.erase(m);
    g_model_floor_clamp.erase(m);
}

constexpr double kLn10 = 2.302585092994045684017991454684; // NOISE_ESTIMATION.md / NOISE_MODEL.md 科学定义 log10↔ln 换算
// trimmed-mean-abs-residual → Gaussian σ 换算因子:
// E[10-90% trimmed mean |r|] = 0.731673 σ (Gaussian N(0,σ²)) — 锚点: docs/science/NOISE_MODEL.md 数值精度 / PSF.md
constexpr double kTrimMeanToSigma = 0.7316727929211932; // PSF.md 0.7316728 (7 位简写) ↔ NOISE_MODEL.md robust_residual_sigma

[[maybe_unused]] bool finite(double x) { return std::isfinite(x); }

// 稳健中位数 (输入会被重排)
double robust_median(std::vector<double>& v) {
    if (v.empty()) return 0.0;
    const std::size_t n = v.size();
    std::nth_element(v.begin(), v.begin() + n / 2, v.end());
    double med = v[n / 2];
    if (n % 2 == 0) {
        std::nth_element(v.begin(), v.begin() + n / 2 - 1, v.end());
        med = (v[n / 2 - 1] + med) * 0.5;
    }
    return med;
}

// 稳健尺度: 1.482602218505602 × median(|x − median(x)|) — 锚点: NOISE_ESTIMATION.md σ_bg / NOISE_MODEL.md §5
double robust_sigma(std::vector<double> v) {
    if (v.empty()) return 0.0;
    const double med = robust_median(v);
    std::vector<double> dev;
    dev.reserve(v.size());
    for (double x : v) dev.push_back(std::fabs(x - med));
    return 1.482602218505602 * robust_median(dev);
}

bool valid_pixel(double x, double saturation) {
    if (!std::isfinite(x)) return false;
    if (saturation > 0.0 && x >= saturation) return false;
    return true;
}

// patch 内 sky 样本收集 (含 cosmic 稳健裁剪)
// 返回收集到的 sky 像素值 (经过有限/饱和/星掩膜/cosmic 裁剪)
// SCI-VAR-ADAPT-01 (§5d): 同时回传生效的裁剪区间 [clip_lo, clip_hi]（未裁剪 = ±inf）,
//   使白噪声等价 σ 的差分样本与 σ_MAD 落在**同一总体**上 ⇒ R 的理论值 = 1 可比。
template <typename T>
bool collect_patch_sky(const T* data, int w,
                       int x0, int x1, int y0, int y1,
                       const std::vector<uint8_t>& source_mask,
                       double saturation, double clip_sigma,
                       int min_samples, int max_rounds,
                       std::vector<double>& out_samples,
                       double* out_clip_lo = nullptr,
                       double* out_clip_hi = nullptr) {
    out_samples.clear();
    out_samples.reserve((std::size_t)(x1 - x0) * (y1 - y0));
    for (int y = y0; y < y1; ++y) {
        const std::size_t row = (std::size_t)y * (std::size_t)w;
        for (int x = x0; x < x1; ++x) {
            if (!source_mask.empty() && source_mask[row + (std::size_t)x]) continue;
            const double v = (double)data[row + (std::size_t)x];
            if (!valid_pixel(v, saturation)) continue;
            out_samples.push_back(v);
        }
    }
    if ((int)out_samples.size() < min_samples) return false;
    double clip_lo = -std::numeric_limits<double>::infinity();
    double clip_hi =  std::numeric_limits<double>::infinity();
    // cosmic / hot-pixel 稳健裁剪 (以稳健中位数为位置, 稳健 σ 为尺度)
    for (int round = 0; round < max_rounds; ++round) {
        std::vector<double> cur = out_samples;
        const double med = robust_median(cur);
        const double sig = robust_sigma(cur);
        if (sig <= 0.0) break;
        const double lo = med - clip_sigma * sig;
        const double hi = med + clip_sigma * sig;
        std::vector<double> kept;
        kept.reserve(out_samples.size());
        for (double v : out_samples)
            if (v >= lo && v <= hi) kept.push_back(v);
        if ((int)kept.size() < min_samples) return false;
        if (kept.size() == out_samples.size()) { clip_lo = lo; clip_hi = hi; break; }
        out_samples.swap(kept);
        clip_lo = lo;
        clip_hi = hi;
    }
    if (out_clip_lo) *out_clip_lo = clip_lo;
    if (out_clip_hi) *out_clip_hi = clip_hi;
    return (int)out_samples.size() >= min_samples;
}

// 平面几何可用性判据 (M3-A-005 / DISP-NOISE-010): 控制点必须张成二维。
// 判据 = 中心化点云 Gram 矩阵 [[sxx,sxy],[sxy,syy]] 的特征值比 λlo/λhi,
// 无量纲; 等价于点云条件数 κ=√(λhi/λlo) ≤ 1/√kPlaneGeomRatio。
// 原绝对阈值 |det|>1e-24 (量纲 px⁴) 已废除: 共线点云 det==0 时它不阻止
// has_spatial_field=1; 近共线点云 det 虽 >1e-24, 平面仍把方差场病态外推 ±62%。
constexpr double kPlaneGeomRatio = 0.0625;  // λlo/λhi 下限 (=1/16, κ≤4)
double plane_geometry_ratio(const double* xs, const double* ys, std::size_t n) {
    if (!xs || !ys || n < 2) return 0.0;
    double mx = 0.0, my = 0.0;
    for (std::size_t i = 0; i < n; ++i) { mx += xs[i]; my += ys[i]; }
    mx /= (double)n; my /= (double)n;
    double sxx = 0.0, sxy = 0.0, syy = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double X = xs[i] - mx;
        const double Y = ys[i] - my;
        sxx += X * X; sxy += X * Y; syy += Y * Y;
    }
    const double tr = sxx + syy;
    const double disc = std::sqrt((sxx - syy) * (sxx - syy) + 4.0 * sxy * sxy);
    const double lam_hi = 0.5 * (tr + disc);
    const double lam_lo = 0.5 * (tr - disc);
    if (!(lam_hi > 0.0) || !std::isfinite(lam_hi) || !std::isfinite(lam_lo)) return 0.0;
    return lam_lo / lam_hi;
}


// ============================================================================
// SCI-VAR-ADAPT-01 (SCI-NOISE-001 §5d): 控制点有效性的自校准判据 + 稳健非负平面拟合
// ----------------------------------------------------------------------------
// 病根（只读诊断 run/SCI-VAR-ZERO-01）: 生产 drizzle 路径在 α·ADU 整帧上建模型,
// 控制点方差动态范围实测 60,756×（σ 12.6→3104 ADU）。星云 patch 的 MAD-σ 量的是
// **空间结构**而不是噪声（实测 R = σ_MAD/σ_白噪声等价 = 33.4, 暗天区只有 1.01–1.05）
// ⇒ 违反 §6「空背景在 patch 尺度局部平稳」。无权最小二乘把权重全给了最亮的几个
// patch ⇒ 平面在帧**内部**穿过零点 ⇒ 30.42% 像素预测 ≤ 0 ⇒ ivar=0 ⇒ 权重 0
// ⇒ 信号静默丢失（马赛克 13.22%）。本段实现 §5d 的两条正向约束, 二者都**不引入
// 新的标定常数**: ① patch 有效性用自校准 R 判据; ② 拟合用相对误差加权 + 凸包内
// 非负约束。§5d 同时明文禁止「平面不可用就退回某个全局常量场」这类常量兜底。
// ============================================================================

// 白噪声等价逐像素 σ（§5d）: 相邻像素一阶差分的稳健尺度 / √2。
//   d = x_{i+1} − x_i, 平稳白噪声下 Var(d) = 2σ² ⇒ σ_d/√2 是 σ 的一致估计。
//   稳健化用与 σ_bg 同一冻结常数 1.482602218505602（MAD→σ, §9）; 1/√2 是恒等式。
//   对**平滑空间结构**（星云/梯度）差分几乎为 0 ⇒ σ_white ≪ σ_MAD ⇒ R ≫ 1;
//   对白噪声二者估计同一个 σ ⇒ R 的理论值 = 1（恒等式, 不是标定值）。
//   依据: §5b「噪声实现空间平滑 —— 对全部随机项不成立（ρ₁ ≈ 0）」;
//         §5b「帧内常数或平面模型能移除噪声 —— 不成立」;
//         §5d「R = σ_MAD/σ_white-equiv 的纯噪声理论值为 1」。
//   样本域与 σ_MAD 一致（同一掩膜、同一饱和域、同一 5σ 裁剪区间）⇒ 二者可比。
//   差分样本数 < min_samples ⇒ 返回 0（R 不可用, 调用方不得据此剔除）。
template <typename T>
double white_noise_equiv_sigma(const T* data, int w, int x0, int x1, int y0, int y1,
                               const std::vector<uint8_t>& mask, double saturation,
                               double clip_lo, double clip_hi, int min_samples,
                               std::vector<double>& scratch) {
    scratch.clear();
    scratch.reserve((std::size_t)2 * (std::size_t)std::max(0, x1 - x0) *
                    (std::size_t)std::max(0, y1 - y0));
    const auto usable = [&](std::size_t idx, double* out) {
        if (!mask.empty() && mask[idx]) return false;
        const double v = (double)data[idx];
        if (!valid_pixel(v, saturation)) return false;
        if (v < clip_lo || v > clip_hi) return false;
        *out = v;
        return true;
    };
    for (int y = y0; y < y1; ++y) {
        const std::size_t row = (std::size_t)y * (std::size_t)w;
        for (int x = x0; x < x1; ++x) {
            const std::size_t i = row + (std::size_t)x;
            double v = 0.0;
            if (!usable(i, &v)) continue;
            if (x + 1 < x1) {                       // 水平相邻对
                double u = 0.0;
                if (usable(i + 1, &u)) scratch.push_back(u - v);
            }
            if (y + 1 < y1) {                       // 垂直相邻对
                double u = 0.0;
                if (usable(i + (std::size_t)w, &u)) scratch.push_back(u - v);
            }
        }
    }
    if ((int)scratch.size() < min_samples) return 0.0;
    const double sig_d = robust_sigma(scratch);
    if (!std::isfinite(sig_d) || sig_d <= 0.0) return 0.0;
    return sig_d / std::sqrt(2.0);
}

// §5d ① 自校准剔除判据（无标定常数）:
//   r_i = ln R_i（乘性误差 ⇒ ln 是自然坐标）, 升序 r_(1) ≤ … ≤ r_(n)。
//   取**最大**的 k ∈ [min_keep, n−1] 使
//       gap_k = r_(k+1) − r_(k)  >  S_k = r_(k) − r_(1)
//   （跳变 = 被剔尾部与保留主体之间的间隔; S_k = 保留主体自身的极差 = 样本自身散布）
//   ⇒ 剔除 R 最大的 n−k 个 patch; 不存在这样的 k ⇒ 一个也不剔。
// 为什么不是标定值:
//   * 判据两侧都是**样本自身的序统计量**, 比较是**尺度无关**的（r → a·r+b 不变）;
//   * 阈值是**恒等倍数 1**——"跳变必须超过主体自身的全部散布"。1 不是可调门限:
//     任何其它倍数都需要外部依据, 而 1 是唯一不引入外部尺度的自然比较;
//   * 纯噪声下 R 的理论值为 1（恒等式）⇒ 真值无效应时上尾与主体不可分离 ⇒ 判据不动;
//   * min_keep = max(⌈n/2⌉, budget_patches): ⌈n/2⌉ 是稳健统计的 50% 崩溃点
//     （噪声总体是多数派, 与所有稳健估计器同一假设）; budget_patches 是 §5a
//     **既有**天空预算常数 mask_budget_min_patches（不是本判据新增的标定）。
//   纯噪声误剔率（20,000 次蒙特卡洛, 见 run/SCI-VAR-ADAPT-01）: n=32 为 1.5e-4,
//   n=64/200 为 0; 备择（少数 patch 被结构污染, ln R = 3.5）检出率 100%。
// 返回: 需剔除的 patch 数（R 最大的那些）; out_fence_ln 为生效栅栏（ln R, 未触发时
//       记录所考察的最紧栅栏, 供审计）。
std::size_t select_structure_tail(std::vector<double> r_ln, std::size_t min_keep,
                                  double* out_fence_ln) {
    const std::size_t n = r_ln.size();
    if (out_fence_ln) *out_fence_ln = 0.0;
    if (n < 3) return 0;
    std::sort(r_ln.begin(), r_ln.end());
    if (min_keep < 2) min_keep = 2;
    // patch 数不足以自校准（保留主体连 min_keep 个都凑不齐）⇒ 判据**不武装**:
    // 显式返回 0（不剔除）, 由 provenance 的 R 分布供审计。**不得**把 min_keep
    // 压到 n−1 —— 那会让 <天空预算 的小样本帧也去剔 patch（自校准失去样本基础）。
    if (min_keep > n - 1) return 0;
    if (out_fence_ln) *out_fence_ln = 2.0 * r_ln[min_keep - 1] - r_ln[0];
    for (std::size_t k = n - 1; k >= min_keep; --k) {
        const double gap = r_ln[k] - r_ln[k - 1];
        const double spread = r_ln[k - 1] - r_ln[0];
        if (gap > spread) {
            if (out_fence_ln) *out_fence_ln = r_ln[k - 1] + spread;
            return n - k;
        }
        if (k == min_keep) break;                    // 防止无符号下溢
    }
    return 0;
}


// §5d ② 稳健非负平面拟合（无标定常数）
//   min_β Σ w_i (a + b·x_i + c·y_i − v_i)²   s.t.  a + b·x_i + c·y_i ≥ 0 ∀i
//   权重 w_i = (v_med / v_i)²: §5a 的误差预算给出 SE(σ̂)/σ ≈ 1.44/√N（**相对**标准误,
//     与 σ 无关）⇒ v̂ 的误差是**常数相对误差** ⇒ Aitken/Gauss-Markov 最优权重 ∝ 1/v²。
//     该权重同时把「被结构污染（方差被高估）的控制点」的杠杆压到相对误差量级,
//     即 §5d 要求的「降低被污染控制点的杠杆」——无权最小二乘正是本缺陷的放大器。
//   约束: 线性函数在凸包上的最小值在**顶点**取到, 而控制点凸包的顶点是控制点的子集
//     ⇒ 「对全部控制点 ≥ 0」⇔「凸包内恒 ≥ 0」（§5d）。凸包**外**仍可能为负, 那是
//     真正的边缘外推, 由 §5/§9④ 的三态表逐像素编码（variance=0 ∧ ivar=0, 不 clamp）。
//   求解: 凸二次规划, 活跃集规模 ≤ 3（3 未知量 + LICQ）⇒ 枚举活跃子集取可行最优,
//     外层用割平面把工作约束集从 4 个极值点长到全部控制点。子问题（等式约束最小二乘）
//     用**零空间 + Householder QR**（β = Zγ, 对加权缩减设计正交三角化）, **不建正规方程**:
//     控制点方差动态范围 D ⇒ 权重跨度 D² ⇒ 正规方程的条件数是设计矩阵的**平方**。实测
//     D=1.2e6 的 fixture 上正规方程解相对偏差达 40%（QR 路径与 SVD 一致到 1e-12）。
//     唯一的常数是浮点比较余量（见下）, 不是科学阈值。
//   返回 false ⇒ 数值上不可解（非科学退化）; 调用方不得据此产出负平面。
constexpr double kFitEpsScale = 8.0;   // 纯浮点比较余量倍数（见下）, 不是科学阈值

// 小型稠密最小二乘内核: min_γ ‖B γ − rhs‖₂, B 行主序 n×p（p ≤ 3）。
// Householder QR（正交变换）⇒ 前向误差 ~ eps·cond(B), 不做正规方程的平方放大。
// 返回 false ⇒ 缩减设计列线性相关（该活跃子集退化, 由枚举中的等价小子集覆盖）。
bool lstsq_qr(const double* B, std::size_t n, std::size_t p, const double* rhs,
              double* gamma) {
    if (!B || !rhs || !gamma || n == 0 || p == 0 || p > 3) return false;
    std::vector<double> R(B, B + n * p);
    std::vector<double> y(rhs, rhs + n);
    std::vector<double> v(n, 0.0);
    for (std::size_t j = 0; j < p; ++j) {
        double norm = 0.0;
        for (std::size_t i = j; i < n; ++i) norm += R[i * p + j] * R[i * p + j];
        norm = std::sqrt(norm);
        if (!std::isfinite(norm) || !(norm > 0.0)) return false;
        const double alpha = (R[j * p + j] >= 0.0) ? -norm : norm;
        double vv = 0.0;
        for (std::size_t i = j; i < n; ++i) v[i] = R[i * p + j];
        v[j] -= alpha;
        for (std::size_t i = j; i < n; ++i) vv += v[i] * v[i];
        if (!(vv > 0.0) || !std::isfinite(vv)) return false;
        for (std::size_t k = j; k < p; ++k) {           // H = I − 2vvᵀ/vᵀv 作用于剩余列
            double dot = 0.0;
            for (std::size_t i = j; i < n; ++i) dot += v[i] * R[i * p + k];
            const double f = 2.0 * dot / vv;
            for (std::size_t i = j; i < n; ++i) R[i * p + k] -= f * v[i];
        }
        double dot = 0.0;                                // 同一变换作用于右端项
        for (std::size_t i = j; i < n; ++i) dot += v[i] * y[i];
        const double f = 2.0 * dot / vv;
        for (std::size_t i = j; i < n; ++i) y[i] -= f * v[i];
    }
    for (std::size_t jj = p; jj-- > 0;) {                // 后向代入 R γ = Qᵀy
        double s = y[jj];
        for (std::size_t k = jj + 1; k < p; ++k) s -= R[jj * p + k] * gamma[k];
        const double d = R[jj * p + jj];
        if (!(std::fabs(d) > 0.0) || !std::isfinite(d)) return false;
        gamma[jj] = s / d;
        if (!std::isfinite(gamma[jj])) return false;
    }
    return true;
}

// A_S 的零空间基: 第 j 列 = 第 j 个基向量（行主序 3×3）, 返回基向量个数 q = 3 − k。
// k ≤ 3 且 A_S 行满秩。k = 3 ⇒ q = 0（唯一解 β = 0, 调用方直接取零向量）。
std::size_t null_space_basis(const double rows[3][3], std::size_t k, double Z[3][3]) {
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) Z[i][j] = (i == j) ? 1.0 : 0.0;
    if (k == 0) return 3;
    if (k == 1) {
        const double* a = rows[0];
        std::size_t piv = 0;
        for (std::size_t j = 1; j < 3; ++j)
            if (std::fabs(a[j]) > std::fabs(a[piv])) piv = j;
        if (!(std::fabs(a[piv]) > 0.0)) return 0;
        std::size_t q = 0;
        for (std::size_t i = 0; i < 3; ++i) {
            if (i == piv) continue;
            for (std::size_t r = 0; r < 3; ++r) Z[r][q] = 0.0;
            Z[i][q] = a[piv];
            Z[piv][q] = -a[i];
            ++q;
        }
        return q;
    }
    if (k == 2) {
        const double* a = rows[0];
        const double* b = rows[1];
        const double z[3] = {a[1] * b[2] - a[2] * b[1],
                             a[2] * b[0] - a[0] * b[2],
                             a[0] * b[1] - a[1] * b[0]};
        const double na = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
        const double nb = std::sqrt(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);
        const double nz = std::sqrt(z[0] * z[0] + z[1] * z[1] + z[2] * z[2]);
        // 相对退化判据（纯数值: 两约束平行 ⇒ 等价于单个约束, 由 k=1 子集覆盖）
        if (!(nz > 1e-12 * na * nb)) return 0;
        for (std::size_t r = 0; r < 3; ++r) Z[r][0] = z[r];
        return 1;
    }
    return 0;
}


// ── §5d 审计量: 凸包内 {预测 ≤ 0} 的面积占比 ────────────────────────────────
// 数据凸包 = 控制点凸包（Andrew monotone chain, 逆时针, 不含重复首点）。
void convex_hull_indices(const double* xs, const double* ys, std::uint32_t n,
                         std::vector<std::uint32_t>& out) {
    out.clear();
    if (n < 3) { for (std::uint32_t i = 0; i < n; ++i) out.push_back(i); return; }
    std::vector<std::uint32_t> idx(n);
    for (std::uint32_t i = 0; i < n; ++i) idx[i] = i;
    std::sort(idx.begin(), idx.end(), [&](std::uint32_t p, std::uint32_t q) {
        if (xs[p] != xs[q]) return xs[p] < xs[q];
        return ys[p] < ys[q];
    });
    const auto cross = [&](std::uint32_t o, std::uint32_t p, std::uint32_t q) {
        return (xs[p] - xs[o]) * (ys[q] - ys[o]) - (ys[p] - ys[o]) * (xs[q] - xs[o]);
    };
    std::vector<std::uint32_t> h(2 * n);
    std::size_t k = 0;
    for (std::size_t i = 0; i < n; ++i) {                       // 下凸壳
        while (k >= 2 && cross(h[k - 2], h[k - 1], idx[i]) <= 0.0) --k;
        h[k++] = idx[i];
    }
    for (std::size_t i = n - 1, t = k + 1; i-- > 0;) {          // 上凸壳
        while (k >= t && cross(h[k - 2], h[k - 1], idx[i]) <= 0.0) --k;
        h[k++] = idx[i];
    }
    out.assign(h.begin(), h.begin() + (std::size_t)std::max<std::size_t>(0, k - 1));
}

// 把凸多边形按半平面 p(x,y) = a + b·x + c·y ≥ 0 裁剪（Sutherland–Hodgman）,
// 返回裁剪后多边形的面积（鞋带公式）。凸多边形的裁剪结果仍为凸多边形。
double clip_area_nonneg(const std::vector<double>& px, const std::vector<double>& py,
                        double a, double b, double c) {
    const std::size_t m = px.size();
    if (m < 3) return 0.0;
    std::vector<double> qx, qy;
    qx.reserve(m + 2);
    qy.reserve(m + 2);
    for (std::size_t i = 0; i < m; ++i) {
        const std::size_t j = (i + 1) % m;
        const double di = a + b * px[i] + c * py[i];
        const double dj = a + b * px[j] + c * py[j];
        if (di >= 0.0) { qx.push_back(px[i]); qy.push_back(py[i]); }
        if ((di >= 0.0) != (dj >= 0.0)) {
            const double t = di / (di - dj);
            qx.push_back(px[i] + t * (px[j] - px[i]));
            qy.push_back(py[i] + t * (py[j] - py[i]));
        }
    }
    const std::size_t q = qx.size();
    if (q < 3) return 0.0;
    double s = 0.0;
    for (std::size_t i = 0; i < q; ++i) {
        const std::size_t j = (i + 1) % q;
        s += qx[i] * qy[j] - qx[j] * qy[i];
    }
    return std::fabs(s) * 0.5;
}

// 主拟合入口（§5d ②）。relative_weights=false 只在相对加权子问题数值不可解时作
// **数值回退**使用（仍然是"拟合", 不是常量场兜底; 非负约束同样生效）。
bool fit_variance_plane_nn(const double* xs, const double* ys, const double* vs,
                           std::uint32_t n, bool relative_weights,
                           double* out_a, double* out_b, double* out_c) {
    if (!xs || !ys || !vs || !out_a || !out_b || !out_c || n < 4) return false;
    // 归一化（纯数值: 改善条件数、避免上下溢; 不改变模型与解）
    double mx = 0.0, my = 0.0;
    for (std::uint32_t i = 0; i < n; ++i) { mx += xs[i]; my += ys[i]; }
    mx /= (double)n; my /= (double)n;
    double L = 0.0;
    for (std::uint32_t i = 0; i < n; ++i) {
        L = std::max(L, std::fabs(xs[i] - mx));
        L = std::max(L, std::fabs(ys[i] - my));
    }
    if (!(L > 0.0) || !std::isfinite(L)) return false;
    std::vector<double> xn(n), yn(n), w(n), sw(n), rhs(n);
    std::vector<double> vbuf(vs, vs + n);
    const double vmed = robust_median(vbuf);
    if (!(vmed > 0.0) || !std::isfinite(vmed)) return false;
    for (std::uint32_t i = 0; i < n; ++i) {
        xn[i] = (xs[i] - mx) / L;
        yn[i] = (ys[i] - my) / L;
        if (relative_weights) {
            const double ratio = vmed / vs[i];
            double wi = ratio * ratio;
            // 纯 IEEE 上溢饱和（需要 v_med/v_i > 1e154 才触发）, 不是科学标定值
            if (!std::isfinite(wi)) wi = std::numeric_limits<double>::max();
            w[i] = wi;
        } else {
            w[i] = 1.0;
        }
        sw[i] = std::sqrt(w[i]);
        rhs[i] = sw[i] * vs[i];
    }
    // 目标: Σ w_i (p_i − v_i)² —— 直接求和（不走 ½βᵀHβ − gᵀβ 的抵消式, 病态下更稳）
    const auto objective = [&](const double beta[3]) {
        double f = 0.0;
        for (std::uint32_t i = 0; i < n; ++i) {
            const double r = beta[0] + beta[1] * xn[i] + beta[2] * yn[i] - vs[i];
            f += w[i] * r * r;
        }
        return f;
    };
    // 可行性: p_i ≥ 0, 浮点比较余量 = 8·DBL_EPSILON·Σ|项|（纯数值, 不是科学阈值）
    const auto feasible = [&](const double beta[3]) {
        for (std::uint32_t i = 0; i < n; ++i) {
            const double t0 = beta[0], t1 = beta[1] * xn[i], t2 = beta[2] * yn[i];
            const double p = t0 + t1 + t2;
            const double guard = kFitEpsScale * std::numeric_limits<double>::epsilon() *
                                 (std::fabs(t0) + std::fabs(t1) + std::fabs(t2));
            if (!(p >= -guard)) return false;
        }
        return true;
    };
    // 子问题: min Σ w (p − v)² s.t. A_S β = 0 —— 零空间 + Householder QR
    std::vector<double> B(static_cast<std::size_t>(n) * 3);
    const auto solve_subset = [&](const double rows[3][3], std::size_t k,
                                  double beta[3]) -> bool {
        double Z[3][3];
        const std::size_t q = null_space_basis(rows, k, Z);
        if (k > 0 && q == 0) return false;      // 约束退化 ⇒ 由等价小子集覆盖
        if (q == 0) {                           // k = 3: 唯一解 β = 0（可行, 目标最大）
            beta[0] = beta[1] = beta[2] = 0.0;
            return true;
        }
        for (std::uint32_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j < q; ++j) {
                const double d = Z[0][j] + Z[1][j] * xn[i] + Z[2][j] * yn[i];
                B[static_cast<std::size_t>(i) * q + j] = sw[i] * d;
            }
        }
        double gamma[3] = {0.0, 0.0, 0.0};
        if (!lstsq_qr(B.data(), n, q, rhs.data(), gamma)) return false;
        for (int r = 0; r < 3; ++r) {
            double s = 0.0;
            for (std::size_t j = 0; j < q; ++j) s += Z[r][j] * gamma[j];
            beta[r] = s;
            if (!std::isfinite(beta[r])) return false;
        }
        return true;
    };
    // 工作约束集: 4 个极值点起步（凸包顶点必在其中）, 割平面增长到全部控制点
    std::vector<std::uint32_t> work;
    {
        std::uint32_t ix0 = 0, ix1 = 0, iy0 = 0, iy1 = 0;
        for (std::uint32_t i = 1; i < n; ++i) {
            if (xs[i] < xs[ix0]) ix0 = i;
            if (xs[i] > xs[ix1]) ix1 = i;
            if (ys[i] < ys[iy0]) iy0 = i;
            if (ys[i] > ys[iy1]) iy1 = i;
        }
        const std::uint32_t cand[4] = {ix0, ix1, iy0, iy1};
        for (int c = 0; c < 4; ++c) {
            bool dup = false;
            for (std::size_t j = 0; j < work.size(); ++j) if (work[j] == cand[c]) dup = true;
            if (!dup) work.push_back(cand[c]);
        }
    }
    double best[3] = {0.0, 0.0, 0.0};
    for (std::size_t round = 0; round <= n; ++round) {
        bool have = false;
        double bestf = 0.0;
        const std::size_t m = work.size();
        const auto consider = [&](const std::uint32_t* idx, std::size_t k) {
            double rows[3][3] = {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}};
            for (std::size_t j = 0; j < k; ++j) {
                const std::uint32_t i = work[idx[j]];
                rows[j][0] = 1.0;
                rows[j][1] = xn[i];
                rows[j][2] = yn[i];
            }
            double beta[3];
            if (!solve_subset(rows, k, beta)) return;
            // 工作集内可行（子集是活跃集的必要条件）
            for (std::size_t j = 0; j < m; ++j) {
                const std::uint32_t i = work[j];
                const double p = beta[0] + beta[1] * xn[i] + beta[2] * yn[i];
                const double guard = kFitEpsScale * std::numeric_limits<double>::epsilon() *
                                     (std::fabs(beta[0]) + std::fabs(beta[1] * xn[i]) +
                                      std::fabs(beta[2] * yn[i]));
                if (!(p >= -guard)) return;
            }
            const double f = objective(beta);
            if (!std::isfinite(f)) return;
            if (!have || f < bestf) {
                have = true; bestf = f;
                best[0] = beta[0]; best[1] = beta[1]; best[2] = beta[2];
            }
        };
        std::uint32_t idx[3];
        consider(idx, 0);                                  // 无活跃约束
        for (std::size_t i = 0; i < m; ++i) { idx[0] = (std::uint32_t)i; consider(idx, 1); }
        for (std::size_t i = 0; i < m; ++i)
            for (std::size_t j = i + 1; j < m; ++j) {
                idx[0] = (std::uint32_t)i; idx[1] = (std::uint32_t)j; consider(idx, 2);
            }
        for (std::size_t i = 0; i < m; ++i)
            for (std::size_t j = i + 1; j < m; ++j)
                for (std::size_t k = j + 1; k < m; ++k) {
                    idx[0] = (std::uint32_t)i; idx[1] = (std::uint32_t)j;
                    idx[2] = (std::uint32_t)k; consider(idx, 3);
                }
        if (!have) return false;
        if (feasible(best)) {                              // 工作集最优对全部控制点可行 ⇒ 全局最优
            *out_b = best[1] / L;
            *out_c = best[2] / L;
            *out_a = best[0] - (*out_b) * mx - (*out_c) * my;
            return std::isfinite(*out_a) && std::isfinite(*out_b) && std::isfinite(*out_c);
        }
        // 加入最违反的控制点（必不在 work 中: best 对 work 可行）
        std::uint32_t worst = n;
        double worst_p = 0.0;
        for (std::uint32_t i = 0; i < n; ++i) {
            const double p = best[0] + best[1] * xn[i] + best[2] * yn[i];
            if (worst == n || p < worst_p) { worst = i; worst_p = p; }
        }
        if (worst == n) return false;
        work.push_back(worst);
    }
    return false;   // 未收敛（数值上不可达）: 调用方按 §4 几何退化路径处置, 不得产出负平面
}

// ============================================================================
// MASK-002 (claim SC-009 / SCI-NOISE-001 §5a): 逐星掩膜半径 + 天空预算收缩
// ----------------------------------------------------------------------------
// 掩膜的唯一目的是让天空样本「无源」; 半径由「掩膜边缘源面亮度 = k·σ_bg」导出:
//   Moffat β: r_local = α·sqrt((F(β−1)/(π α² k σ_bg))^(1/β) − 1),
//             α = FWHM/(2·sqrt(2^(1/β)−1))
//   Gaussian 极限 r_local = σ_p·sqrt(2·ln(F/(2π σ_p² k σ_bg)))  (本实现取 Moffat)
// 掩膜专用保守翼指数 β=2.5 (比默认 Moffat4 β=4 的翼更宽 ⇒ 半径更大, 保守)。
// 硬上界 rmax = max(1,source_mask_radius_px)·max(1,mask_radius_scale) 只是**上界**,
// 不是操作默认半径 (实测: 256²/50 星取 60 px ⇒ 整帧 rc=1; 1024²/320 星 60 px 的
// RMSE 是 10 px 的 4.6 倍)。「掩膜半径与星亮度解耦」为错误陈述, MASK-001 §3.3 实测
// F 10³→10⁶ ADU 使 σ_bg 偏差 0.02%→2.29%。
// ============================================================================
constexpr double kMaskMoffatBeta = 2.5;   // 掩膜专用保守翼指数 (SCI §5a)
constexpr uint32_t kMaskLegacy   = 1u;    // bit0: 半径信息缺失, 按 §5a 回调规则降级
constexpr uint32_t kMaskDegraded = 2u;    // bit1: 天空预算收缩后仍 nq<预算 / 全局兜底

double mask_local_radius(double flux, double fwhm, double k_sigma_bg) {
    if (!(flux > 0.0) || !(fwhm > 0.0) || !(k_sigma_bg > 0.0)) return 0.0;
    if (!std::isfinite(flux) || !std::isfinite(fwhm) || !std::isfinite(k_sigma_bg)) return 0.0;
    const double beta = kMaskMoffatBeta;
    const double alpha = fwhm / (2.0 * std::sqrt(std::pow(2.0, 1.0 / beta) - 1.0));
    const double num = flux * (beta - 1.0);
    const double den = M_PI * alpha * alpha * k_sigma_bg;
    if (!(num > 0.0) || !(den > 0.0)) return 0.0;
    const double q = std::pow(num / den, 1.0 / beta);
    if (!(q > 1.0)) return 0.0;
    const double r = alpha * std::sqrt(q - 1.0);
    return std::isfinite(r) && r > 0.0 ? r : 0.0;
}

void rasterize_disk(std::vector<uint8_t>& mask, int h, int w,
                    double cx, double cy, double r) {
    if (!(r > 0.0) || !std::isfinite(cx) || !std::isfinite(cy)) return;
    const int icx = (int)std::lround(cx);
    const int icy = (int)std::lround(cy);
    const int ir = (int)std::ceil(r);
    const double r2 = r * r;
    for (int dy = -ir; dy <= ir; ++dy) {
        const int py = icy + dy;
        if (py < 0 || py >= h) continue;
        const double ddy = (double)dy;
        const double rem = r2 - ddy * ddy;
        if (rem < 0.0) continue;
        const int dxmax = (int)std::floor(std::sqrt(rem));
        for (int dx = -dxmax; dx <= dxmax; ++dx) {
            const int px = icx + dx;
            if (px < 0 || px >= w) continue;
            mask[(std::size_t)py * (std::size_t)w + (std::size_t)px] = 1;
        }
    }
}

// 半径集 × 收缩因子 s → 圆盘掩膜 (r_i(s) = max(r_min_i, s·r_i))
void build_disk_mask(std::vector<uint8_t>& mask, int h, int w,
                     const std::vector<double>& cx, const std::vector<double>& cy,
                     const std::vector<double>& radii,
                     const std::vector<double>& rmin, double s) {
    mask.assign((std::size_t)h * (std::size_t)w, 0);
    for (std::size_t i = 0; i < cx.size(); ++i) {
        const double r = std::max(rmin[i], s * radii[i]);
        rasterize_disk(mask, h, w, cx[i], cy[i], r);
    }
}

struct SkyBudget {
    uint32_t nq = 0;      // 未掩膜合法样本 ≥ min_samples 的 patch 数 (廉价代理)
    uint64_t n_sky = 0;   // 未掩膜且合法 (有限/未饱和) 的像素数
    double   frac = 0.0;  // 掩膜覆盖比 (掩膜像素 / h·w), 与合法性无关
};

template <typename T>
SkyBudget sky_budget(const T* data, int h, int w,
                     const std::vector<uint8_t>& mask,
                     int gx, int gy, int min_samples, double saturation) {
    SkyBudget b;
    for (int py = 0; py < gy; ++py) {
        const int y0 = (int)((std::int64_t)py * h / gy);
        const int y1 = (int)((std::int64_t)(py + 1) * h / gy);
        for (int px = 0; px < gx; ++px) {
            const int x0 = (int)((std::int64_t)px * w / gx);
            const int x1 = (int)((std::int64_t)(px + 1) * w / gx);
            std::int64_t cnt = 0;
            for (int y = y0; y < y1; ++y) {
                const std::size_t row = (std::size_t)y * (std::size_t)w;
                for (int x = x0; x < x1; ++x) {
                    if (!mask.empty() && mask[row + (std::size_t)x]) continue;
                    if (!valid_pixel((double)data[row + (std::size_t)x], saturation)) continue;
                    ++cnt;
                }
            }
            b.n_sky += (uint64_t)cnt;
            if (cnt >= (std::int64_t)min_samples) ++b.nq;
        }
    }
    std::size_t masked = 0;
    for (std::size_t i = 0; i < mask.size(); ++i) if (mask[i]) ++masked;
    const std::size_t total = (std::size_t)h * (std::size_t)w;
    b.frac = total ? (double)masked / (double)total : 0.0;
    return b;
}

// 掩膜通道元数据 (逐星半径 / 预算 / 降级位标)
struct MaskPlan {
    std::vector<uint8_t> mask;
    std::vector<double> cx, cy;      // 有效星坐标 (0-based px)
    std::vector<double> flux, fwhm;  // 逐星通量 [ADU] / FWHM [px] (缺失 = 0)
    std::vector<double> radii, rmin; // 逐星半径与下界
    double   radius_p50 = 0.0;
    double   frac = 0.0;
    double   shrink = 1.0;           // 天空预算收缩因子 s
    uint32_t flags = 0;
    bool     infeasible = false;     // 预算在 r_min 仍不可行 ⇒ rc=1
};

// 由给定掩膜估 σ_bg 种子 (两遍法第一遍; §5a)。合格 patch 稳健中位数优先,
// 否则全帧未掩膜稳健尺度。ok=false ⇒ 无可用样本。
template <typename T>
double sigma_from_mask(const T* data, int h, int w,
                       const std::vector<uint8_t>& mask,
                       int gx, int gy, double clip_sigma, int min_samples,
                       int max_rounds, double saturation, bool* ok) {
    std::vector<double> patch_sigma, samples;
    for (int py = 0; py < gy; ++py) {
        const int y0 = (int)((std::int64_t)py * h / gy);
        const int y1 = (int)((std::int64_t)(py + 1) * h / gy);
        for (int px = 0; px < gx; ++px) {
            const int x0 = (int)((std::int64_t)px * w / gx);
            const int x1 = (int)((std::int64_t)(px + 1) * w / gx);
            if (!collect_patch_sky(data, w, x0, x1, y0, y1, mask, saturation,
                                   clip_sigma, min_samples, max_rounds, samples)) continue;
            const double sig = robust_sigma(samples);
            if (std::isfinite(sig) && sig > 0.0) patch_sigma.push_back(sig);
        }
    }
    if (!patch_sigma.empty()) { *ok = true; return robust_median(patch_sigma); }
    std::vector<double> all;
    all.reserve((std::size_t)h * (std::size_t)w);
    for (int y = 0; y < h; ++y) {
        const std::size_t row = (std::size_t)y * (std::size_t)w;
        for (int x = 0; x < w; ++x) {
            if (!mask.empty() && mask[row + (std::size_t)x]) continue;
            const double v = (double)data[row + (std::size_t)x];
            if (valid_pixel(v, saturation)) all.push_back(v);
        }
    }
    if ((int)all.size() < std::max(1, min_samples)) { *ok = false; return 0.0; }
    const double sig = robust_sigma(all);
    *ok = std::isfinite(sig) && sig > 0.0;
    return *ok ? sig : 0.0;
}

// 模板内核: 估计 blank-sky 方差模型 (float/double 数据)
template <typename T>
int noise_model_impl(const T* data, int h, int w,
                     const float* source_mask,
                     const double* star_x, const double* star_y,
                     const double* star_flux, const double* star_fwhm,
                     int n_stars,
                     const SnrNoiseModelConfig* cfg,
                     NoiseWeightModelV1* out_model) {
    if (!data || !out_model || h <= 0 || w <= 0) return 3;
    SnrNoiseModelConfig c{};
    if (cfg) {
        c = *cfg;
    } else {
        snr_noise_model_v1_default_config(&c);
    }
    std::memset(out_model, 0, sizeof(NoiseWeightModelV1));
    out_model->struct_size = (uint32_t)sizeof(NoiseWeightModelV1);
    out_model->abi_version = SNR_NOISE_MODEL_ABI_VERSION;
    // G3-6 (钳位 fail-open → fail-closed): variance_floor 是**保护下限**,
    // 不是可选项。NaN/≤0 的 floor 会让 std::max(x, floor) 静默失效
    // （NaN 比较恒 false ⇒ 返回 x; ≤0 ⇒ 等于不设防），并使 fill 侧注册表
    // 判据 it->second > 0 落空 → 静默回退 1e-12（配置被无声忽略）。
    // 故此处显式拒绝: 非法 floor ⇒ SNR_FLOOR_UNBOUND(-10), 不产出模型。
    if (!std::isfinite(c.variance_floor) || c.variance_floor <= 0.0) {
        return SNR_FLOOR_UNBOUND;
    }
    registry_register_model(out_model, c.variance_floor);
    // 钳位计数随 build 清零 (触发即计数)；build 期只累加**局部**变量，返回前
    // 一次性写入注册表 ⇒ 热路径（逐 patch / 控制点）零加锁。
    int64_t clamp_count = 0;

    const int gx = std::max(2, c.patch_grid_x);
    const int gy = std::max(2, c.patch_grid_y);
    const double clip_sigma = std::max(1.0, c.cosmic_clip_sigma);
    const int min_samples = std::max(1, c.min_patch_samples);
    const int max_rounds = std::max(0, c.max_clip_rounds);

    // 星点掩膜 (MASK-002 / SCI-NOISE-001 §5a): 逐星半径
    //   r_i = clip(r_local(F_i, FWHM_i, k·σ_bg), r_min_i, rmax)  + 天空预算收缩。
    // 手工 source_mask 通道优先 (DISP-NOISE-006 互斥保持): 调用方显式给的掩膜
    // **不收缩、不覆盖**, 预算违反只置 MASK_DEGRADED 诊断位 (SCI §5a 手工通道例外)。
    MaskPlan plan;
    const double r0 = std::max(1.0, c.source_mask_radius_px);
    const double rmax = r0 * std::max(1.0, c.mask_radius_scale);
    const double k_sigma = (c.mask_k_sigma > 0.0) ? c.mask_k_sigma : 0.1;
    const double r_min_default = (c.mask_r_min_px > 0.0) ? c.mask_r_min_px : 1.5;
    const double fwhm_floor = (c.mask_fwhm_floor_scale > 0.0) ? c.mask_fwhm_floor_scale : 0.75;
    const uint32_t budget_patches = (c.mask_budget_min_patches > 0) ? c.mask_budget_min_patches : 8u;
    const uint64_t budget_sky = (c.mask_budget_min_sky > 0) ? (uint64_t)c.mask_budget_min_sky : 9216ull;
    const bool manual_mask = (source_mask != nullptr);

    if (manual_mask) {
        plan.mask.assign((std::size_t)h * (std::size_t)w, 0);
        for (std::size_t i = 0; i < plan.mask.size(); ++i)
            plan.mask[i] = source_mask[i] != 0.0f;
    } else if (star_x && star_y && n_stars > 0) {
        for (int i = 0; i < n_stars; ++i) {
            if (!std::isfinite(star_x[i]) || !std::isfinite(star_y[i])) continue;
            const double f = star_flux ? star_flux[i] : 0.0;
            const double wf = star_fwhm ? star_fwhm[i] : 0.0;
            const bool has_w = std::isfinite(wf) && wf > 0.0;
            plan.cx.push_back(star_x[i]);
            plan.cy.push_back(star_y[i]);
            plan.flux.push_back(std::isfinite(f) && f > 0.0 ? f : 0.0);
            plan.fwhm.push_back(has_w ? wf : 0.0);
            plan.rmin.push_back(has_w ? std::max(r_min_default, fwhm_floor * wf)
                                       : r_min_default);
            plan.radii.push_back(0.0);   // 第二遍填
            if (!has_w) plan.flags |= kMaskLegacy;   // §5a 回调: 无 FWHM ⇒ 统一 rmax
        }
        if (!plan.cx.empty()) {
            // 第一遍 (§5a 两遍法): 只用 PSF 尺度 4·FWHM 的保守掩膜估 σ_bg 种子
            std::vector<double> seed;
            seed.reserve(plan.cx.size());
            for (std::size_t i = 0; i < plan.cx.size(); ++i) {
                const double wf = plan.fwhm[i];
                const double r = (wf > 0.0) ? 4.0 * wf : rmax;
                seed.push_back(std::min(r, rmax));
            }
            build_disk_mask(plan.mask, h, w, plan.cx, plan.cy, seed, plan.rmin, 1.0);
            bool seed_ok = false;
            const double sigma_seed =
                sigma_from_mask(data, h, w, plan.mask, gx, gy, clip_sigma,
                                min_samples, max_rounds, c.saturation_level, &seed_ok);
            if (!seed_ok || !(sigma_seed > 0.0)) {
                // 无法估种子 ⇒ σ_bg 不可知, 按 §5a 回调用统一 rmax (置 MASK_LEGACY)
                for (std::size_t i = 0; i < plan.radii.size(); ++i) plan.radii[i] = rmax;
                plan.flags |= kMaskLegacy;
            } else {
                const double k_bg = k_sigma * sigma_seed;
                for (std::size_t i = 0; i < plan.cx.size(); ++i) {
                    const double f = plan.flux[i];
                    const double wf = plan.fwhm[i];
                    double r = 0.0;
                    if (f > 0.0 && wf > 0.0) {
                        r = mask_local_radius(f, wf, k_bg);
                    } else if (wf > 0.0) {
                        r = 4.0 * wf;                  // §5a: F_i 缺失 ⇒ 4·FWHM_i
                        plan.flags |= kMaskLegacy;
                    }
                    if (!(r > 0.0)) { r = rmax; plan.flags |= kMaskLegacy; }
                    plan.radii[i] = std::min(r, rmax);
                }
            }
            // 天空预算收缩: 取最大 s∈[0,1] 使 nq(s) ≥ budget_patches 且 N_sky(s) ≥ budget_sky
            const auto feasible = [&](double s, SkyBudget* out) {
                build_disk_mask(plan.mask, h, w, plan.cx, plan.cy, plan.radii, plan.rmin, s);
                const SkyBudget b =
                    sky_budget(data, h, w, plan.mask, gx, gy, min_samples, c.saturation_level);
                if (out) *out = b;
                return (b.nq >= budget_patches) && (b.n_sky >= budget_sky);
            };
            SkyBudget budget;
            if (!feasible(1.0, &budget)) {
                if (!feasible(0.0, nullptr)) {
                    plan.infeasible = true;   // 收缩到 r_min 仍不可行 ⇒ rc=1 (空 support 不传播)
                } else {
                    double lo = 0.0, hi = 1.0, best = 0.0;
                    for (int it = 0; it < 16; ++it) {
                        const double mid = 0.5 * (lo + hi);
                        if (feasible(mid, nullptr)) { best = mid; lo = mid; } else { hi = mid; }
                    }
                    plan.shrink = best;
                    plan.flags |= kMaskDegraded;   // 收缩生效 ≠ 无代价, 显式标
                    feasible(best, &budget);
                }
            }
            if (!plan.infeasible) {
                std::vector<double> rr;
                rr.reserve(plan.radii.size());
                for (std::size_t i = 0; i < plan.radii.size(); ++i)
                    rr.push_back(std::max(plan.rmin[i], plan.shrink * plan.radii[i]));
                plan.radius_p50 = robust_median(rr);
            }
        }
    }

    if (plan.infeasible) {
        out_model->degenerate = 1;
        out_model->mask_degraded = plan.flags;
        return 1;
    }

    const std::vector<uint8_t>& mask = plan.mask;
    const SkyBudget final_budget =
        sky_budget(data, h, w, mask, gx, gy, min_samples, c.saturation_level);
    plan.frac = final_budget.frac;
    if (manual_mask &&
        (final_budget.nq < budget_patches || final_budget.n_sky < budget_sky)) {
        plan.flags |= kMaskDegraded;   // 手工通道: 只诊断, 不覆盖调用方掩膜
    }

    std::vector<double> patch_sigma;
    std::vector<double> patch_var;
    std::vector<double> patch_r;      // §5d ①: 逐 patch R = σ_MAD / σ_white-equiv
    std::vector<double> ctrl_x;
    std::vector<double> ctrl_y;
    std::vector<double> samples;
    std::vector<double> diffs;        // 差分样本 scratch（与 samples 复用同一 patch）
    int n_rejected = 0;
    int n_r_unavailable = 0;          // 差分样本不足 ⇒ R 不可用（不据此剔除, 只登记）

    for (int py = 0; py < gy; ++py) {
        const int y0 = (int)((std::int64_t)py * h / gy);
        const int y1 = (int)((std::int64_t)(py + 1) * h / gy);
        for (int px = 0; px < gx; ++px) {
            const int x0 = (int)((std::int64_t)px * w / gx);
            const int x1 = (int)((std::int64_t)(px + 1) * w / gx);
            double clip_lo = 0.0, clip_hi = 0.0;
            if (!collect_patch_sky(data, w, x0, x1, y0, y1, mask,
                                   c.saturation_level, clip_sigma,
                                   min_samples, max_rounds, samples,
                                   &clip_lo, &clip_hi)) {
                ++n_rejected;
                continue;
            }
            const double sig = robust_sigma(samples);
            if (!std::isfinite(sig) || sig <= 0.0) {
                ++n_rejected;
                continue;
            }
            // §5d ①: 同一总体上的白噪声等价 σ（lag-1 差分稳健尺度 / √2）
            const double sig_white = white_noise_equiv_sigma(
                data, w, x0, x1, y0, y1, mask, c.saturation_level, clip_lo, clip_hi,
                min_samples, diffs);
            patch_sigma.push_back(sig);
            patch_var.push_back(sig * sig);
            ctrl_x.push_back((double)(x0 + x1) * 0.5);
            ctrl_y.push_back((double)(y0 + y1) * 0.5);
            if (sig_white > 0.0 && std::isfinite(sig_white)) {
                patch_r.push_back(sig / sig_white);
            } else {
                patch_r.push_back(std::numeric_limits<double>::quiet_NaN());
                ++n_r_unavailable;
            }
        }
    }

    // ── §5d ① 控制点有效性: 自校准 R 判据（推导与"为什么不是标定值"见
    //    select_structure_tail 的注释; 纯噪声下 R 的理论值 = 1 是恒等式）────────
    std::size_t n_structure_rejected = 0;
    {
        std::vector<double> r_ln;      // 可算 R 的 patch（NaN = R 不可用, 不参与判据）
        r_ln.reserve(patch_r.size());
        for (double rv : patch_r)
            if (std::isfinite(rv) && rv > 0.0) r_ln.push_back(std::log(rv));
        if (!r_ln.empty()) {
            std::vector<double> sorted = r_ln;
            std::sort(sorted.begin(), sorted.end());
            out_model->r_min = std::exp(sorted.front());
            out_model->r_median = std::exp(robust_median(sorted));
            out_model->r_max = std::exp(sorted.back());
        }
        const std::size_t min_keep =
            std::max<std::size_t>((r_ln.size() + 1) / 2, (std::size_t)budget_patches);
        double fence_ln = 0.0;
        n_structure_rejected = select_structure_tail(r_ln, min_keep, &fence_ln);
        out_model->r_fence = std::exp(fence_ln);
        if (n_structure_rejected > 0) {
            const double fence_r = std::exp(fence_ln);
            std::size_t keep = 0;
            for (std::size_t i = 0; i < patch_var.size(); ++i) {
                if (std::isfinite(patch_r[i]) && patch_r[i] > fence_r) continue;
                patch_sigma[keep] = patch_sigma[i];
                patch_var[keep] = patch_var[i];
                patch_r[keep] = patch_r[i];
                ctrl_x[keep] = ctrl_x[i];
                ctrl_y[keep] = ctrl_y[i];
                ++keep;
            }
            patch_sigma.resize(keep);
            patch_var.resize(keep);
            patch_r.resize(keep);
            ctrl_x.resize(keep);
            ctrl_y.resize(keep);
            n_rejected += (int)n_structure_rejected;   // I5: qualified + rejected = patch 总数
        }
    }
    out_model->n_structure_rejected_patches = (uint32_t)n_structure_rejected;
    out_model->n_r_unavailable_patches = (uint32_t)n_r_unavailable;

    out_model->n_qualified_patches = (uint32_t)patch_var.size();
    out_model->n_rejected_patches = (uint32_t)n_rejected;

    // MASK-002 诊断标: 预算收缩后仍 nq<预算 ⇒ MASK_DEGRADED (显式降级, 消费方可 fail-closed)
    if (patch_var.size() < budget_patches || final_budget.n_sky < budget_sky) {
        plan.flags |= kMaskDegraded;
    }
    out_model->mask_degraded = plan.flags;
    out_model->mask_radius_p50 = plan.radius_p50;
    out_model->mask_frac = plan.frac;

    // 全局兜底: 合格 patch variance 的稳健中位数
    if (!patch_var.empty()) {
        std::vector<double> vc = patch_var;
        const double vmed = robust_median(vc);
        // G3-6: 钳位触发即计数（不改变数值, 只把静默钳位变为可登记状态）
        if (vmed < c.variance_floor) ++clamp_count;
        out_model->variance_bg_global = std::max(vmed, c.variance_floor);
        out_model->sigma_bg_global = std::sqrt(out_model->variance_bg_global);
        out_model->ivar_bg_global = 1.0 / out_model->variance_bg_global;
    } else {
        // 整帧退化兜底: 全帧 sky 样本 robust scale (忽略空间结构)
        std::vector<double> all;
        all.reserve((std::size_t)h * (std::size_t)w);
        for (int y = 0; y < h; ++y) {
            const std::size_t row = (std::size_t)y * (std::size_t)w;
            for (int x = 0; x < w; ++x) {
                if (!mask.empty() && mask[row + (std::size_t)x]) continue;
                const double v = (double)data[row + (std::size_t)x];
                if (valid_pixel(v, c.saturation_level)) all.push_back(v);
            }
        }
        // MASK-002: 全帧兜底阈由 min_samples/2 (=32 px) **收紧**为
        // max(min_samples, budget_sky): 32 个像素不得为整帧定权重。
        // 依据 SE(σ̂)/σ ≈ 1.144/√N_sky (MAD 路径) ≤ 1.5% ⇒ N_sky ≥ 9216。
        const std::int64_t fallback_min =
            (std::int64_t)std::max(min_samples, (int)std::min<std::uint64_t>(budget_sky, 1000000000ull));
        if ((std::int64_t)all.size() < std::max<std::int64_t>(1, fallback_min)) {
            out_model->degenerate = 1;
            out_model->mask_degraded = plan.flags;
            registry_add_clamp(out_model, clamp_count);
            return 1;
        }
        const double sig = robust_sigma(all);
        if (!std::isfinite(sig) || sig <= 0.0) {
            out_model->degenerate = 1;
            out_model->mask_degraded = plan.flags;
            registry_add_clamp(out_model, clamp_count);
            return 1;
        }
        out_model->sigma_bg_global = sig;
        if (sig * sig < c.variance_floor) ++clamp_count;
        out_model->variance_bg_global = std::max(sig * sig, c.variance_floor);
        out_model->ivar_bg_global = 1.0 / out_model->variance_bg_global;
        out_model->degenerate = 1;  // 无空间分辨, 全局兜底
        out_model->mask_degraded = plan.flags | kMaskDegraded;
        registry_add_clamp(out_model, clamp_count);
        return 0;
    }

    // 控制点数组
    const std::size_t n = patch_var.size();
    out_model->n_control_points = (uint32_t)n;
    out_model->ctrl_x_px = (double*)std::malloc(n * sizeof(double));
    out_model->ctrl_y_px = (double*)std::malloc(n * sizeof(double));
    out_model->ctrl_sigma = (double*)std::malloc(n * sizeof(double));
    out_model->ctrl_variance = (double*)std::malloc(n * sizeof(double));
    out_model->ctrl_ivar = (double*)std::malloc(n * sizeof(double));
    if (!out_model->ctrl_x_px || !out_model->ctrl_y_px ||
        !out_model->ctrl_sigma || !out_model->ctrl_variance ||
        !out_model->ctrl_ivar) {
        snr_noise_model_v1_free(out_model);
        return 3;
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (patch_var[i] < c.variance_floor) ++clamp_count;
        const double var = std::max(patch_var[i], c.variance_floor);
        out_model->ctrl_x_px[i] = ctrl_x[i];
        out_model->ctrl_y_px[i] = ctrl_y[i];
        out_model->ctrl_sigma[i] = patch_sigma[i];
        out_model->ctrl_variance[i] = var;
        out_model->ctrl_ivar[i] = 1.0 / var;
    }
    // 平面场启用条件 = enable_spatial_field && n>=4 && 控制点几何张成二维
    // (M3-A-005: 与 fill 同一无量纲判据; 几何退化 ⇒ §4/§5 既有的全局常量兜底路径)
    // SCI-VAR-ADAPT-01 (§5d ②): 再加一条 —— 拟合必须**数值可解**。非负由约束在
    //   求解中保证（不是事后 clamp）; 法方程数值不可解时（生产配置不可达）按几何
    //   退化路径处置, **不得**产出可能为负的平面。
    double plane_a = 0.0, plane_b = 0.0, plane_c = 0.0;
    bool fit_ok = false;
    if (c.enable_spatial_field && n >= 4 &&
        plane_geometry_ratio(ctrl_x.data(), ctrl_y.data(), n) >= kPlaneGeomRatio) {
        fit_ok = fit_variance_plane_nn(ctrl_x.data(), ctrl_y.data(),
                                       out_model->ctrl_variance, (std::uint32_t)n,
                                       /*relative_weights=*/true,
                                       &plane_a, &plane_b, &plane_c);
        if (!fit_ok)   // 数值回退: 均匀权重（仍是拟合 + 非负约束, 不是常量场兜底）
            fit_ok = fit_variance_plane_nn(ctrl_x.data(), ctrl_y.data(),
                                           out_model->ctrl_variance, (std::uint32_t)n,
                                           /*relative_weights=*/false,
                                           &plane_a, &plane_b, &plane_c);
    }
    out_model->has_spatial_field = fit_ok ? 1 : 0;
    if (fit_ok) {
        out_model->plane_a = plane_a;
        out_model->plane_b = plane_b;
        out_model->plane_c = plane_c;
        // 凸包内最小预测: 线性函数在凸包上的最小值在**顶点**取到, 顶点是控制点子集
        // ⇒ 控制点上的最小值即凸包内的最小值（§5d 审计量, 与 fill 同一表达式）。
        double pmin = std::numeric_limits<double>::infinity();
        for (std::size_t i = 0; i < n; ++i) {
            const double p = plane_a + plane_b * ctrl_x[i] + plane_c * ctrl_y[i];
            if (p < pmin) pmin = p;
        }
        out_model->hull_min_pred = std::isfinite(pmin) ? pmin : 0.0;
        // 凸包内 {预测 ≤ 0} 的面积占比（§5d 审计量; 约束下恒 0, 此处如实**实测**）
        std::vector<std::uint32_t> hull;
        convex_hull_indices(ctrl_x.data(), ctrl_y.data(), (std::uint32_t)n, hull);
        if (hull.size() >= 3) {
            std::vector<double> hx, hy;
            hx.reserve(hull.size());
            hy.reserve(hull.size());
            for (std::uint32_t hi : hull) { hx.push_back(ctrl_x[hi]); hy.push_back(ctrl_y[hi]); }
            const double area_hull = clip_area_nonneg(hx, hy, 0.0, 0.0, 0.0);  // 全平面 ⇒ 凸包面积
            const double area_pos = clip_area_nonneg(hx, hy, plane_a, plane_b, plane_c);
            if (area_hull > 0.0 && std::isfinite(area_hull) && std::isfinite(area_pos)) {
                double frac = 1.0 - area_pos / area_hull;
                if (!(frac > 0.0)) frac = 0.0;      // 数值残差归零（诊断量, 非科学阈值）
                if (frac > 1.0) frac = 1.0;
                out_model->hull_nonpositive_frac = frac;
            }
        }
    }
    // 控制点方差动态范围（§5d 审计量; 拟合输入 = 剔除结构污染后的控制点）
    if (n > 0) {
        double vlo = out_model->ctrl_variance[0], vhi = out_model->ctrl_variance[0];
        for (std::size_t i = 1; i < n; ++i) {
            vlo = std::min(vlo, out_model->ctrl_variance[i]);
            vhi = std::max(vhi, out_model->ctrl_variance[i]);
        }
        out_model->ctrl_variance_range = (vlo > 0.0) ? (vhi / vlo) : 0.0;
    }
    out_model->source = 0;  // empirical blank-sky (production 基线)
    registry_add_clamp(out_model, clamp_count);
    return 0;
}

}  // namespace

extern "C" {

// ============================================================================
// 1. PhotometricCalibrationQuality
// ============================================================================
SNR_API int snr_phot_cal_quality(double sigma_logflux_dex, int n_matches,
                                 PhotometricCalibrationQuality* out) {
    if (!out) return 3;
    std::memset(out, 0, sizeof(PhotometricCalibrationQuality));
    out->n_matches = n_matches;
    if (!std::isfinite(sigma_logflux_dex) || sigma_logflux_dex <= 0.0) {
        out->fit_status = 2;
        return 0;
    }
    out->sigma_logflux_dex = sigma_logflux_dex;
    out->sigma_mag         = 2.5 * sigma_logflux_dex;
    out->sigma_cal_rel     = kLn10 * sigma_logflux_dex;
    // P5-SNR 订正 (2026-09-14, 负责人授权; PHOTOMETRY_LITERATURE_REVIEW §C.2.2):
    // sigma_residual 是逐星定标散度, 不是零点误差。高斯下 median 的标准误
    // sigma_kappa,stat ~ 1.253*sigma_residual/sqrt(N_eff); 旧实现未除 sqrt(N),
    // N=200 时把零点误差高估约 11 倍。
    if (n_matches > 0) {
        out->sigma_location_se_dex =
            snr_calib_zero_point_standard_error(sigma_logflux_dex, n_matches);
        out->sigma_location_se_mag = 2.5 * out->sigma_location_se_dex;
    }
    out->fit_status        = (n_matches > 0) ? 0 : 1;
    return 0;
}

// ============================================================================
// 2. PsfFitQuality
// ============================================================================
SNR_API int snr_psf_fit_quality(const double* psf, int n_stars,
                                const int64_t* star_ids,
                                const uint32_t* quality_flags,
                                PsfFitQualityRow* out) {
    if (!psf || !out || n_stars < 0) return 3;
    for (int i = 0; i < n_stars; ++i) {
        const double* row = psf + i * 9;
        PsfFitQualityRow& r = out[i];
        std::memset(&r, 0, sizeof(r));
        r.flux               = row[2];
        r.amplitude_above_bg = row[6];
        r.background         = row[1];
        r.fwhm               = row[5];
        r.eccentricity       = row[8];
        r.residual_scale     = row[7];   // 原 mad 列 → 准确语义
        const uint32_t qf = quality_flags ? quality_flags[i] : 0u;
        const double status = row[0];
        if (status != 0.0) {
            r.fit_status = 1;
        } else if (qf & (SNR_QF_SATURATED | SNR_QF_HAS_SATURATED)) {
            r.fit_status = 2;
        } else if (!std::isfinite(row[6]) || !std::isfinite(row[7]) || row[7] <= 0.0) {
            r.fit_status = 3;
        } else {
            r.fit_status = 0;
        }
        if (r.residual_scale > 0.0 && std::isfinite(r.residual_scale)) {
            r.robust_residual_sigma = r.residual_scale / kTrimMeanToSigma;
            r.q_psf = r.amplitude_above_bg / r.residual_scale;
        }
        (void)star_ids;
    }
    return 0;
}

// ============================================================================
// 3. NoiseWeightModelV1
// ============================================================================
SNR_API int snr_noise_model_v1_default_config(SnrNoiseModelConfig* cfg) {
    if (!cfg) return 3;
    std::memset(cfg, 0, sizeof(SnrNoiseModelConfig));
    cfg->struct_size = (uint32_t)sizeof(SnrNoiseModelConfig);
    cfg->abi_version = SNR_NOISE_CONFIG_ABI_VERSION;
    cfg->patch_grid_x = 8;
    cfg->patch_grid_y = 8;
    cfg->source_mask_radius_px = 10.0;   // 与 mask_radius_scale 相乘 = 60 px 硬上界
    cfg->mask_radius_scale = 6.0;
    cfg->cosmic_clip_sigma = 5.0;
    cfg->min_patch_samples = 64;
    cfg->max_clip_rounds = 2;
    cfg->enable_spatial_field = 1;
    cfg->variance_floor = 1e-12;
    // MASK-002 (claim SC-009 / SCI-NOISE-001 §5a): 逐星掩膜半径参数
    cfg->mask_k_sigma = 0.1;              // 掩膜边缘残余面亮度 = 0.1·σ_bg  ⇒  σ_bg 偏差 ≤ 0.5%
    cfg->mask_r_min_px = 1.5;             // r_min = max(1.5 px, 0.75·FWHM_i)
    cfg->mask_fwhm_floor_scale = 0.75;
    cfg->mask_budget_min_patches = 8;     // 天空预算: n_qualified ≥ 8
    cfg->mask_budget_min_sky = 9216;      // 天空预算: N_sky ≥ 9216 (SE ≤ 1.5%)
    return 0;
}

// ---- ABI 头部助手 (claim SC-009) -------------------------------------------
SNR_API void snr_noise_model_v1_abi_stamp_config(SnrNoiseModelConfig* cfg) {
    if (!cfg) return;
    cfg->struct_size = (uint32_t)sizeof(SnrNoiseModelConfig);
    cfg->abi_version = SNR_NOISE_CONFIG_ABI_VERSION;
}

SNR_API void snr_noise_model_v1_abi_stamp_model(NoiseWeightModelV1* model) {
    if (!model) return;
    model->struct_size = (uint32_t)sizeof(NoiseWeightModelV1);
    model->abi_version = SNR_NOISE_MODEL_ABI_VERSION;
}

SNR_API int snr_noise_model_v1_abi_check_config(const SnrNoiseModelConfig* cfg) {
    if (!cfg) return SNR_ABI_MISMATCH;
    if (cfg->struct_size != (uint32_t)sizeof(SnrNoiseModelConfig)) return SNR_ABI_MISMATCH;
    if (cfg->abi_version != (uint32_t)SNR_NOISE_CONFIG_ABI_VERSION) return SNR_ABI_MISMATCH;
    return 0;
}

SNR_API int snr_noise_model_v1_abi_check_model(const NoiseWeightModelV1* model) {
    if (!model) return SNR_ABI_MISMATCH;
    if (model->struct_size != (uint32_t)sizeof(NoiseWeightModelV1)) return SNR_ABI_MISMATCH;
    if (model->abi_version != (uint32_t)SNR_NOISE_MODEL_ABI_VERSION) return SNR_ABI_MISMATCH;
    return 0;
}

SNR_API int snr_noise_model_v1(const float* data, int h, int w,
                               const float* source_mask,
                               const double* star_x, const double* star_y,
                               const double* star_flux, const double* star_fwhm,
                               int n_stars,
                               const SnrNoiseModelConfig* cfg,
                               NoiseWeightModelV1* out_model) {
    // ABI fail-closed: 非空 cfg 必须带匹配头部 (无头部 = 旧调用方 ⇒ 拒绝)
    if (cfg && snr_noise_model_v1_abi_check_config(cfg) != 0) return SNR_ABI_MISMATCH;
    try { return noise_model_impl(data, h, w, source_mask, star_x, star_y, star_flux, star_fwhm, n_stars, cfg, out_model); } catch (const std::exception& e) { (void)e; return 3; } catch (...) { return 3; }
}

SNR_API int snr_noise_model_v1_f64(const double* data, int h, int w,
                                   const float* source_mask,
                                   const double* star_x, const double* star_y,
                                   const double* star_flux, const double* star_fwhm,
                                   int n_stars,
                                   const SnrNoiseModelConfig* cfg,
                                   NoiseWeightModelV1* out_model) {
    if (cfg && snr_noise_model_v1_abi_check_config(cfg) != 0) return SNR_ABI_MISMATCH;
    try { return noise_model_impl(data, h, w, source_mask, star_x, star_y, star_flux, star_fwhm, n_stars, cfg, out_model); } catch (const std::exception& e) { (void)e; return 3; } catch (...) { return 3; }
}

namespace {



// 最小二乘平面填充内核（variance field， 冻结；float 输出）
// G3-6: 返回 0=成功, SNR_FLOOR_UNBOUND=floor 未绑定/非法 (fail-closed)。
// 原签名 void + 入口恒 return 0 会把显式拒绝码静默吞掉。
int fill_impl(const NoiseWeightModelV1* m, int h, int w,
              float* out_variance, float* out_ivar) {
    if (m->has_spatial_field && m->n_control_points >= 4) {
        // 平滑方差场 var(x,y) = a + b·x + c·y —— 模型形式不变（§5 冻结）；
        // SCI-VAR-ADAPT-01 (§5d ②) 只改**估计量**: 相对误差加权 + 凸包内非负约束。
        //   权重 w_i = (v_med/v_i)² 由 §5a 的误差预算导出（SE(σ̂)/σ ≈ 1.44/√N 是
        //   **相对**标准误 ⇒ 常数相对误差 ⇒ Aitken 最优权重）, 同时把被结构污染的
        //   高方差控制点的杠杆压到相对误差量级（§5d「降低被污染控制点的杠杆」）。
        //   约束 a+b·x_i+c·y_i ≥ 0 在全部控制点上 ⇒ 凸包内恒 ≥ 0（线性函数的最小值
        //   在凸包顶点取到, 顶点是控制点子集）。凸包**外**仍可能 ≤ 0, 那是真正的
        //   边缘外推, 由下方逐像素循环按 §5/§9④ 三态表编码（0∧0, 不 clamp）。
        // 与 build 阶段同一函数 ⇒ 同一控制点数组上逐位可复现。
        double a = 0.0, b = 0.0, c = 0.0;
        const uint32_t n = m->n_control_points;
        if (!fit_variance_plane_nn(m->ctrl_x_px, m->ctrl_y_px, m->ctrl_variance, n,
                                   /*relative_weights=*/true, &a, &b, &c)) {
            if (!fit_variance_plane_nn(m->ctrl_x_px, m->ctrl_y_px, m->ctrl_variance, n,
                                       /*relative_weights=*/false, &a, &b, &c)) {
                // 法方程数值不可解（生产配置不可达）: 按 §4/§5 几何退化路径退回全局
                // 常量场（该场恒正, 不是"用常量掩盖负平面"）。build 侧同判据 ⇒ 一致。
                const double var = m->variance_bg_global;
                const double ivar = m->ivar_bg_global;
                for (std::size_t i = 0; i < (std::size_t)h * (std::size_t)w; ++i) {
                    if (out_variance) out_variance[i] = (float)var;
                    if (out_ivar) out_ivar[i] = (float)ivar;
                }
                return 0;
            }
        }
        // W1-NOISE-002 / G3-6: floor 传播只经内部注册表（按模型指针键控）。
        // **fail-closed**: 模型未绑定 floor（或绑定值非法）时显式拒绝
        // （SNR_FLOOR_UNBOUND），不再静默回退 1e-12 —— 后者会让配置的
        // variance_floor 在生产 fill 路径上被无声忽略。
        // 绑定途径: snr_noise_model_v1[_f64] build, 或
        // snr_noise_model_v1_bind_variance_floor。
        // PERF-P1: 查询移到像素循环**之前**（floor 在循环内恒定）——原实现在逐像素
        // 循环内查全局 map，既白付每像素一次哈希查找，又是帧级并行下的并发访问点。
        // 判据与拒绝时机（未绑定/非法 ⇒ SNR_FLOOR_UNBOUND）逐字不变。
        double floor = 0.0;
        if (!registry_get_floor(m, &floor)) return SNR_FLOOR_UNBOUND;
        // SCI-NOISE-001 §5/§7/§9 + DATA_SEMANTICS §4a 三态表:
        //   平面是**外推**模型（控制点方差恒正，最小二乘平面可在帧内取非正值）。
        //   预测 > 0 ⇒ 该处方差**可用**：取 max(预测, floor)，floor 是数值保护，
        //               保证 ivar 有限（§7「clamp 只作用于可用方差」）。
        //   预测 ≤ 0 ⇒ 该处方差**不可用**：产品面写 variance=0 ∧ ivar=0
        //               （显式不可用）。**不得** clamp 成 floor —— 那会把「模型在
        //               此处失效」伪造成 ivar=1/floor 的极大权重（floor=1e-12 时
        //               比物理 ivar 大 ~1e12 倍），且在按 α² 换算的标度下
        //               1/floor 在 float32 中溢出为 +inf（非有限产品值）。
        //               也不得写 NaN（NaN 保留给产品损坏，§4a F-UNC-001）。
        // 产品 dtype 一致性守卫: 输出面 (variance, ivar) 必须落在两态之一 ——
        //   可用: variance > 0 ∧ isfinite(variance) ∧ ivar = 1/variance;
        //   不可用: variance == 0 ∧ ivar == 0。
        // 生效 floor 由配置给出（单位 ADU²）; 调用方若以别的标度消费数组,
        // 必须把 floor 一并按 α² 换算。换算后 floor 可在 float32 中下溢为 0,
        // 此时 1/floor 上溢为 +inf ⇒ 该像素在产品 dtype 中方差不可表示,
        // 取不可用态（禁发布 (0, +inf) 这种自相矛盾的对）。
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const std::size_t idx = (std::size_t)y * w + x;
                const double pred = a + b * (double)x + c * (double)y;
                float v_out = 0.0f, i_out = 0.0f;
                if (pred > 0.0) {
                    const double var = (pred < floor) ? floor : pred;
                    const double ivar = 1.0 / var;
                    const float vc = (float)var, ic = (float)ivar;
                    if (std::isfinite(vc) && vc > 0.0f &&
                        std::isfinite(ic) && ic > 0.0f) {
                        v_out = vc;
                        i_out = ic;
                    }
                }
                if (out_variance) out_variance[idx] = v_out;
                if (out_ivar) out_ivar[idx] = i_out;
            }
        }
    } else {
        const double var = m->variance_bg_global;
        const double ivar = m->ivar_bg_global;
        for (std::size_t i = 0; i < (std::size_t)h * (std::size_t)w; ++i) {
            if (out_variance) out_variance[i] = (float)var;
            if (out_ivar) out_ivar[i] = (float)ivar;
        }
    }
    return 0;
}

}  // namespace

SNR_API int snr_noise_model_v1_fill(const NoiseWeightModelV1* model,
                                    int h, int w,
                                    float* out_variance, float* out_ivar) {
    if (!model || h <= 0 || w <= 0) return 3;
    if (!out_variance && !out_ivar) return 3;
    // ABI fail-closed: 模型必须由本版本 build 产出 (无头部 = 旧/手工拼装 ⇒ 拒绝)
    if (snr_noise_model_v1_abi_check_model(model) != 0) return SNR_ABI_MISMATCH;
    // G3-6: fill_impl 的显式拒绝码 (SNR_FLOOR_UNBOUND) 必须原样上抛 ——
    // 原实现丢弃返回值恒 return 0, 会把 fail-closed 门静默吞掉。
    int frc = 0;
    try { frc = fill_impl(model, h, w, out_variance, out_ivar); }
    catch (const std::exception& e) { (void)e; return 3; }
    catch (...) { return 3; }
    return frc;
}

// G3-6: 显式绑定 variance_floor（与 build 内部登记同一注册表）。
// floor 必须有限且 > 0 ⇒ 否则 SNR_FLOOR_UNBOUND(-10) 显式拒绝（禁静默通过）。
SNR_API int snr_noise_model_v1_bind_variance_floor(NoiseWeightModelV1* model,
                                                   double floor_var) {
    if (!model) return 3;
    if (!std::isfinite(floor_var) || floor_var <= 0.0) return SNR_FLOOR_UNBOUND;
    if (snr_noise_model_v1_abi_check_model(model) != 0) return SNR_ABI_MISMATCH;
    registry_bind_floor(model, floor_var);
    return 0;
}

// G3-6: 钳位触发计数（build 期间被 floor 抬升的数值个数; 0 = 未触发）。
// 未注册模型返回 -1（显式"未知", 不冒充 0）。
SNR_API int64_t snr_noise_model_v1_floor_clamp_count(
    const NoiseWeightModelV1* model) {
    if (!model) return -1;
    return registry_get_clamp(model);
}

SNR_API void snr_noise_model_v1_free(NoiseWeightModelV1* model) {
    if (!model) return;
    registry_erase(model);
    std::free(model->ctrl_x_px);
    std::free(model->ctrl_y_px);
    std::free(model->ctrl_sigma);
    std::free(model->ctrl_variance);
    std::free(model->ctrl_ivar);
    model->ctrl_x_px = nullptr;
    model->ctrl_y_px = nullptr;
    model->ctrl_sigma = nullptr;
    model->ctrl_variance = nullptr;
    model->ctrl_ivar = nullptr;
    model->n_control_points = 0;
}

SNR_API void snr_noise_scale_law(double alpha,
                                 double* variance, double* ivar) {
    if (variance) *variance = (*variance) * alpha * alpha;
    if (ivar) {
        const double a2 = alpha * alpha;
        if (a2 > 0.0 && std::isfinite(*ivar)) *ivar = (*ivar) / a2;
    }
}

SNR_API double snr_noise_gain_variance(double signal, // NOISE_ESTIMATION.md Gain/Readnoise 仅诊断(SNR-005), NOISE_MODEL.md 诊断模型
                                       double gain_e_per_adu,
                                       double read_noise_e) {
    if (gain_e_per_adu <= 0.0) return 0.0;
    const double s = std::max(0.0, signal);
    return s / gain_e_per_adu +
           (read_noise_e * read_noise_e) /
               (gain_e_per_adu * gain_e_per_adu);
}

}  // extern "C"
