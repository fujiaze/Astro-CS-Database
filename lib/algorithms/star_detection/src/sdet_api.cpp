/**
 * @file sdet_api.cpp
 * @brief Star Detector API实现（ACSD phase1 star_detection 模块生产入口）
 *
 * 职责: 全图盲检测（诊断/初值，O1–O15）与星表引导检测（权威路径 O16，共用同一 flux 口径）；
 * 输入块: cleaned 帧（FP32/FP64）+ SDetParams（阈值倍数默认 5.0）+ 星表预测位置（引导路径）；
 * 输出块: star_det 十数组（经 sdet_free_detect_ex 整组释放）；线程模型: 候选拟合 OpenMP 并行
 * （dynamic + reduction，线程数由宿主注入），dedup/sort/截断串行，输出与线程数无关。
 */

#include "../include/star_detector.h"
#include "sdet_image.h"
#include "sdet_log.h"
#include "sdet_angle_guard.h"   // SDET-ANGLE-001: 有界/fail-closed 朝向角归一化
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <algorithm>
#include <vector>
#include <type_traits>
#include <cmath>
#include <set>
#include <unordered_map>
#include <omp.h>

// 拟合后端: 自研信赖域 Levenberg-Marquardt（见 src/nls_lm.h; 该头载有不得退回外部求解器的禁止性说明）
// 算法与行为对齐依据见 docs/science/algorithms/STAR_PSF_ALGORITHMS.md「参考文献与参考代码库（含许可证）」一节所列文献（LM 谱系含 Moré 1978）与
// src/nls_lm.cpp 顶部说明; 该后端不引入任何外部依赖（Windows 可编译, 无 GPL 传染）。
#include "nls_lm.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// LM 拟合收敛参数
#define LM_XTOL 1e-3
#define LM_GTOL 1e-3
// GSL-REPLACE-01 说明: 原后端 GSL 2.8 driver 的 ftol 判据实测**从不触发**
// （8 个解析问题 × 尺度 1/1e-2/1e-4 × 零噪声/相对噪声, 以及真实帧 236 次拟合
// 的终止码只出现 xtol/gtol; 证据见 run/GSL-REPLACE-01/REPORT.md §3.2）。
// 本后端把 ftol 实现为**有语义的** MINPACK 相对代价判据, 值仍取 1e-3, 使它在
// 生产 regime 下承担原 gtol 的"噪声平台上早停"角色（实测终止码分布与原后端
// 同量级）, 同时不改变任何调用面常数。若取 1e-8（MINPACK sqrt(eps)）则收敛更紧
// 但真实帧失败率由 0.8% 升到 14%（超迭代上限）, 故不取（REPORT §5.2）。
#define LM_FTOL 1e-3
#define LM_MAX_ITER_ANGLE 20
#define LM_MIN_HALF_RADIUS 1
#define LM_MIN_LARGE_SAMPLING 3

#define TWO_SQRT_2_LOG2 2.3548200450309493

// FIX-P174: 原语收编——旧 sdet_detector.h 的类型与连通域原语迁入本文件（O4a 饱和岛消费；SPEC: ALG §3 8-连通语义）
struct ConnectedComponent {
    int x0, y0, x1, y1;
    int count;
    std::vector<int> px;
    std::vector<int> py;
};

struct StarDetectorInternal {
    SDetParams params;
    int width = 0, height = 0;
    float* raw_detail = nullptr;
};

// 8-连通标记（BFS，扫描序发现；binary_map 非 0 视为前景）
static int sdet_find_connected_components(const float* binary_map, int w, int h,
                                          ConnectedComponent** out_components, int* out_count) {
    if (!binary_map || w <= 0 || h <= 0 || !out_components || !out_count) return -1;
    std::vector<char> visited(static_cast<size_t>(w) * h, 0);
    std::vector<ConnectedComponent> comps;
    std::vector<int> stack;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const size_t idx = static_cast<size_t>(y) * w + x;
            if (binary_map[idx] == 0.0f || visited[idx]) continue;
            ConnectedComponent c{x, y, x, y, 0, {}, {}};
            stack.clear();
            stack.push_back(static_cast<int>(idx));
            visited[idx] = 1;
            while (!stack.empty()) {
                const int cur = stack.back();
                stack.pop_back();
                const int cy = cur / w, cx = cur - cy * w;
                c.px.push_back(cx);
                c.py.push_back(cy);
                ++c.count;
                if (cx < c.x0) c.x0 = cx;
                if (cx > c.x1) c.x1 = cx;
                if (cy < c.y0) c.y0 = cy;
                if (cy > c.y1) c.y1 = cy;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        const int nx = cx + dx, ny = cy + dy;
                        if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                        const size_t nidx = static_cast<size_t>(ny) * w + nx;
                        if (binary_map[nidx] != 0.0f && !visited[nidx]) {
                            visited[nidx] = 1;
                            stack.push_back(static_cast<int>(nidx));
                        }
                    }
                }
            }
            comps.push_back(std::move(c));
        }
    }
    *out_count = static_cast<int>(comps.size());
    *out_components = new ConnectedComponent[comps.size()];
    for (size_t i = 0; i < comps.size(); ++i) (*out_components)[i] = std::move(comps[i]);
    return 0;
}

static void sdet_free_connected_components(ConnectedComponent* components, int count) {
    if (!components) return;
    for (int i = 0; i < count; ++i) {
        components[i].px.clear();
        components[i].py.clear();
    }
    delete[] components;
}

struct StarDetectorHandle_s {
    StarDetectorInternal internal;
};

namespace {

// Gaussian FWHM = 2*sqrt(2*ln2)*σ = 2.3548*σ
static const double GAUSSIAN_FWHM_FACTOR = 2.3548200450309493;
static const int NPARAMS = 7;

#define SDET_FIT_OK              0
#define SDET_FIT_NO_CONVERGENCE  1
#define SDET_FIT_INVALID_PARAMS  2
#define SDET_FIT_ITERATION_LIMIT 3

struct SamplePixel {
    double dx;
    double dy;
    double val;
};

struct InternalFitResult {
    int status;
    double B, A, cx, cy, sx, sy, theta;
    double fwhm_x, fwhm_y;
    double mad;
};

// PSF 拟合

// IPv 用 samples 数组 (已排除饱和像素), 等价于 mask=true 子集
// 保留 NbRows/NbCols 的概念, 但 samples 已预过滤
struct PSFFitData {
    size_t n;           // 样本数 (mask=true 的像素数)
    const double* y;    // 像素值数组 (mask=true 的像素)
    size_t NbRows;      // 矩阵行数 (未使用, 兼容保留)
    size_t NbCols;      // 矩阵列数 (未使用, 兼容保留)
    const SamplePixel* samples;  // IPv 样本 (dx, dy 已含 +0.5 偏移)
    double rmse;        // 输出: RMSE = sqrt(Σres²/n)
    // B4-4 (M3b-H-01): 拟合残差的真实 MAD = median(|res − median(res)|)。
    // 供 InternalFitResult.mad 使用 (NOISE_MODEL.md:17/:135 冻结的 MAD→σ
    // 换算 1.482602218505602 只作用于 MAD 而非 RMSE)。
    std::vector<double> residuals;
    double cx0, cy0;    // 样本偏移的参考中心（候选入参; f/df 内换算参数化坐标）
};

// 内部: 有代表性的中位数 (与 sdet_image.cpp 的 robust_median 同式:
// 奇数取中位元素, 偶数取两中位均值)
static double sdet_median_of(std::vector<double>& v) {
    const size_t n = v.size();
    if (n == 0) return 0.0;
    std::sort(v.begin(), v.end());
    return (n % 2 == 1) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}
// SPEC: 5.12 O12 / DISP-STAR-007 检测侧唯一母函数（椭圆高斯 + 常数天光）
//   f(x,y) = B + A*exp(-(1/2)[(x'/sx)^2 + (y'/sy)^2])
//   x' = dx*cos(th) + dy*sin(th), y' = -dx*sin(th) + dy*cos(th) (th 弧度)
// 参数序 (x0, y0, sx, sy, th, A, B)。样本 dx/dy 已含像素中心偏移
// (像素中心 = 索引 + 0.5)。残差 = 模型 - 观测。sx/sy 非正时钳 1e-8
// 数值保护（不产生 NaN/Inf 污染 LM 下降方向）。
static void sdet_gaussian_f(const double* x, void* params, double* f) {
    PSFFitData* d = static_cast<PSFFitData*>(params);
    const double cx = x[0], cy = x[1];
    const double sx = x[2] > 1e-8 ? x[2] : 1e-8;
    const double sy = x[3] > 1e-8 ? x[3] : 1e-8;
    const double th = std::atan2(std::sin(x[4]), std::cos(x[4]));  // theta 折算 [-pi,pi]（周期恒等; 防 LM 平坦方向漂移至大辐角致 cos/sin 精度崩坏）
    const double A = x[5], B = x[6];
    const double c = std::cos(th), s = std::sin(th);
    for (size_t k = 0; k < d->n; ++k) {
        // 参数化坐标: 样本偏移以候选中心为参考, 模型变量 (x0,y0) 进入坐标
        // (设计稿 5.12: (x0,y0,sx,sy,th,A,B) 为拟合参数)
        const double dxa = d->samples[k].dx + d->cx0 - cx;
        const double dya = d->samples[k].dy + d->cy0 - cy;
        const double xp = dxa * c + dya * s;
        const double yp = -dxa * s + dya * c;
        const double u = xp / sx, v = yp / sy;
        const double e = std::exp(-0.5 * (u * u + v * v));
        f[k] = (B + A * e) - d->y[k];
    }
}
// SPEC: 5.12 O12 解析雅可比（链式法则展开, 与 sdet_gaussian_f 同一参数化）:
//   df/dx0 = A*E*(u*c/sx - v*s/sy)      (x' 对 x0 偏导 = -c, y' 对 x0 = +s)
//   df/dy0 = A*E*(u*s/sx + v*c/sy)
//   df/dsx = A*E*u^2/sx,  df/dsy = A*E*v^2/sy
//   df/dth = A*E*(-u*y'/sx + v*x'/sy)   (x' 对 th = y', y' 对 th = -x')
//   df/dA = E, df/dB = 1;  E = exp(-(1/2)(u^2+v^2))
static void sdet_gaussian_df(const double* x, void* params, double* J) {
    PSFFitData* d = static_cast<PSFFitData*>(params);
    const double sx = x[2] > 1e-8 ? x[2] : 1e-8;
    const double sy = x[3] > 1e-8 ? x[3] : 1e-8;
    const double th = std::atan2(std::sin(x[4]), std::cos(x[4]));  // theta 折算（同 sdet_gaussian_f; 折算角处雅可比周期等价）
    const double A = x[5];
    const double c = std::cos(th), s = std::sin(th);
    for (size_t k = 0; k < d->n; ++k) {
        const double dxa = d->samples[k].dx + d->cx0 - x[0];
        const double dya = d->samples[k].dy + d->cy0 - x[1];
        const double xp = dxa * c + dya * s;
        const double yp = -dxa * s + dya * c;
        const double u = xp / sx, v = yp / sy;
        const double E = std::exp(-0.5 * (u * u + v * v));
        double* row = J + k * NPARAMS;
        row[0] = A * E * (u * c / sx - v * s / sy);
        row[1] = A * E * (u * s / sx + v * c / sy);
        row[2] = A * E * (u * u / sx);
        row[3] = A * E * (v * v / sy);
        row[4] = A * E * (-u * yp / sx + v * xp / sy);
        row[5] = E;
        row[6] = 1.0;
    }
}
// 数值保护（实现级数值保护）: theta 冻结的 6 参数包装。完美圆星（数据 sigma_x=sigma_y）
// theta 数学上不可辨识, LM 收敛路径回到对称致信赖域停滞; 冻结 theta=0 后 6 参数拟合
// 可收敛, theta=0 与任意等效角在合同判据下等价（fwhm/sx/sy/位置不变）。
static void sdet_gaussian_f6(const double* x6, void* params, double* f) {
    double x7[NPARAMS];
    x7[0] = x6[0]; x7[1] = x6[1]; x7[2] = x6[2]; x7[3] = x6[3];
    x7[4] = 0.0; x7[5] = x6[4]; x7[6] = x6[5];
    sdet_gaussian_f(x7, params, f);
}
static void sdet_gaussian_df6(const double* x6, void* params, double* J) {
    PSFFitData* d = static_cast<PSFFitData*>(params);
    const size_t m = d->n;
    std::vector<double> J7(m * (size_t)NPARAMS);
    double x7[NPARAMS];
    x7[0] = x6[0]; x7[1] = x6[1]; x7[2] = x6[2]; x7[3] = x6[3];
    x7[4] = 0.0; x7[5] = x6[4]; x7[6] = x6[5];
    sdet_gaussian_df(x7, params, J7.data());
    for (size_t k = 0; k < m; ++k) {
        double* row = J + k * 6;
        const double* row7 = &J7[k * (size_t)NPARAMS];
        row[0] = row7[0]; row[1] = row7[1]; row[2] = row7[2];
        row[3] = row7[3]; row[4] = row7[5]; row[5] = row7[6];
    }
}
// 5 参数包装: sigma 各向同性（sigma_x=sigma_y=s; 圆星数据下 sigma_x/sigma_y 仅联合
// 可辨识, 6 参数仍秩亏时冻结等效宽度; theta 已冻结 0）。
static void sdet_gaussian_f5(const double* x5, void* params, double* f) {
    double x7[NPARAMS];
    x7[0] = x5[0]; x7[1] = x5[1]; x7[2] = x5[2]; x7[3] = x5[2];
    x7[4] = 0.0; x7[5] = x5[3]; x7[6] = x5[4];
    sdet_gaussian_f(x7, params, f);
}
static void sdet_gaussian_df5(const double* x5, void* params, double* J) {
    PSFFitData* d = static_cast<PSFFitData*>(params);
    const size_t m = d->n;
    std::vector<double> J7(m * (size_t)NPARAMS);
    double x7[NPARAMS];
    x7[0] = x5[0]; x7[1] = x5[1]; x7[2] = x5[2]; x7[3] = x5[2];
    x7[4] = 0.0; x7[5] = x5[3]; x7[6] = x5[4];
    sdet_gaussian_df(x7, params, J7.data());
    for (size_t k = 0; k < m; ++k) {
        double* row = J + k * 5;
        const double* row7 = &J7[k * (size_t)NPARAMS];
        row[0] = row7[0]; row[1] = row7[1];
        row[2] = row7[2] + row7[3];   // dsigma = dsigma_x + dsigma_y 链式合并
        row[3] = row7[5]; row[4] = row7[6];
    }
}


// reject_star 验证错误码（接口面定义, 与 ALG 11.2 错误码语义对齐）
enum SfError {
    SF_OK = 0,
    SF_FWHM_NEG = 1,
    SF_FWHM_TOO_SMALL = 2,
    SF_ROUNDNESS_BELOW_CRIT = 3,
    SF_RMSE_TOO_LARGE = 4,
    SF_FWHM_TOO_LARGE = 5
};

// SPEC: 5.13 O13 排异门（五码判式冻结; ALG 11.2 错误码语义）
// (1) FWHM 非正/非有限 → SF_FWHM_NEG（fail-closed: NaN 一并落此码）
// (2) 任一轴 FWHM <= 0.5 px → SF_FWHM_TOO_SMALL（长焦窄带星点下限）
// (3) 圆度门 min/max FWHM < 0.5 → SF_ROUNDNESS_BELOW_CRIT
// (4) 排异门 sigma_res/A > 0.2 → SF_RMSE_TOO_LARGE, sigma_res = mad *
//     1.482602218505602（NOISE_MODEL 14.2; B4-4: 换算作用于 MAD 非 RMSE）;
//     饱和星豁免（平台残差分布失效, DISP-STAR-004 语义）
// (5) FIX-R1: FWHM 上限门 → SF_FWHM_TOO_LARGE（设计稿 §5.13(5); 等价分析 O13
//     第 5 判式恢复——旧注释"生产路径不触发"与设计稿/等价分析矛盾, 系重写期
//     漏译）。判式: se_smax = max(候选零交叉宽度读数 Sr, Sc) > 2.0 时,
//     fwhm_limit = se_smax * 2.3548200450309493 * (1 + 0.5*ln(se_smax/2)),
//     拟合 FWHM 任一轴 > fwhm_limit → 拒（"零交叉读数窄而拟合极宽"的结构
//     失配防御; 饱和平台星零交叉读数与拟合宽度同步增大, 判式天然不误伤）。
static SfError reject_star(const InternalFitResult& fit, bool has_saturated,
                    double cand_sx, double cand_sy) {
    if (!(fit.fwhm_x > 0.0) || !(fit.fwhm_y > 0.0)) return SF_FWHM_NEG;
    if (fit.fwhm_x <= 0.5 || fit.fwhm_y <= 0.5) return SF_FWHM_TOO_SMALL;
    const double fmax = std::max(fit.fwhm_x, fit.fwhm_y);
    const double fmin = std::min(fit.fwhm_x, fit.fwhm_y);
    if (fmin / fmax < 0.5) return SF_ROUNDNESS_BELOW_CRIT;
    if (!has_saturated) {
        const double sigma_res = fit.mad * 1.482602218505602;
        if (fit.A > 0.0 && sigma_res / fit.A > 0.2) return SF_RMSE_TOO_LARGE;
    }
    // FIX-R1: 第 5 判式（SF_FWHM_TOO_LARGE）—— 形参合同 cand_sx/cand_sy =
    // 候选 O7 零交叉宽度读数 (Sr, Sc), 由装配环/guided 调用点传 c.Sr, c.Sc。
    const double se_smax = std::max(cand_sx, cand_sy);
    if (se_smax > 2.0) {
        const double fwhm_limit = se_smax * 2.3548200450309493
                                * (1.0 + 0.5 * std::log(se_smax / 2.0));
        if ((double)fit.fwhm_x > fwhm_limit || (double)fit.fwhm_y > fwhm_limit)
            return SF_FWHM_TOO_LARGE;
    }
    return SF_OK;
}
// 统一星点数据结构（正常星+饱和星共用）
struct StarRecord {
    double cx, cy;
    float flux;          // 正常星=振幅A，饱和星=PSF拟合A或0(失败)
    int is_saturated;     // 0=正常星，1=饱和星
    // Moffat4拟合数据（正常星+饱和星均有，饱和星拟合失败时为0）
    float fwhm_x, fwhm_y;
    float sx, sy, theta;
    float background, amplitude;
    // 饱和星圆盘拟合数据
    float r;             // 等效半径，正常星=0
    // 新增字段：mag 和 has_saturated
    float mag;           // -2.5*log10(A)，拟合失败时为NaN
    int has_saturated;   // 1=该星检测到饱和平台，0=正常星
    float cand_R;        // 候选阶段 R (mag box 半径, star_finder.c:529)
};

// ============================================================================
// R-58-1: O13b 点源形状门（盲检测路径; O13 五码排异门之后的第二道形状判据）
// ----------------------------------------------------------------------------
// 依据: run/FINAL-07/审核包/端到端/五帧越闸定性报告.md §6.1（R-58 裁决 1）。
// 事实: M42_M5 帧上新检测器把星云核弥散发射切成一簇 fwhm 195-338 px 的"源"
// 并按星等排进最亮序列（帧 1 的 U=60 最亮非饱和里 5 颗 fwhm>50 px）, 同时把
// 次点源伪检（单像素尖峰, pf 0.5-0.99）从每帧约 20 颗抬到 200 颗以上; 这些
// 非点源成员进入 WCS 解算的 U/池 ⇒ 帧 M42_M5@035959 最终 rms 由 0.106 抬到
// 0.6554 px 触发冻结门 DISP-WCS-001。既有 O13 门拦不住: 次点源尖峰的拟合
// fwhm 0.52-0.87 px 高于 0.5 px 绝对下限, 星云团块的 fwhm 上限判式是
// se_smax 的对数函数（读数随尺度过宽同步放宽）。
// 判据设计: 全部相对本帧点源参考, 不用全局常数 ——
//   ① 半高宽比例窗 f0 = 过既有门的候选 fwhm 中位数（中位数对 <50% 的伪检污染
//      稳健; **不得用亮度作参考**: 单像素尖峰的孔径星等比真实星更"亮"）
//   ② 峰占比上限 pf = (峰值像素 - B)/(拟合盒内正通量和)
//   ③ 最小像素数 n_quarter = #{pixel > B + 0.25*(peak-B)}
// 参数约定与既有 maxAxisRatio 门一致: 0 = 用内置默认, 负值 = 显式关闭该子门
// （四个子门全关 ⇒ 本函数是零成本直通, 与改动前逐位等价）。
// 负例/正例回归锁: tests/p1star/p1star_psf_shape_gate_test.cpp。
// ============================================================================
static const double SDET_SHAPE_FWHM_LO_DEFAULT = 0.5;
static const double SDET_SHAPE_FWHM_HI_DEFAULT = 2.5;
static const double SDET_SHAPE_PF_DEFAULT = 0.35;
static const int    SDET_SHAPE_NQUARTER_DEFAULT = 4;
static const int    SDET_SHAPE_WINDOW_DEFAULT = 6;   // = 默认拟合盒半径 (13x13)
static const int    SDET_SHAPE_WINDOW_MAX = 12;

struct SdetShapeMetrics {
    double fwhm;            // 0.5*(fwhm_x+fwhm_y)
    double peak_fraction;   // 峰值像素占盒内正通量比
    int n_quarter;          // 高于（背景 + 1/4 峰）的像素数
};

// 盒内形状量（两遍扫描同一盒, 无分配）。中心像素 = lround(cx-0.5)
// （与 O14 孔径盒同约定: 像素索引 i 的中心在 i+0.5）。
template <typename T>
static SdetShapeMetrics sdet_shape_metrics(const T* src, int w, int h,
                                           double cx, double cy, double bg,
                                           int radius) {
    SdetShapeMetrics m;
    m.fwhm = 0.0;
    m.peak_fraction = 0.0;
    m.n_quarter = 0;
    if (!src || w <= 0 || h <= 0 || radius <= 0) return m;
    if (radius > SDET_SHAPE_WINDOW_MAX) radius = SDET_SHAPE_WINDOW_MAX;
    const int bx = (int)std::lround(cx - 0.5);
    const int by = (int)std::lround(cy - 0.5);
    double peak = 0.0, pos_sum = 0.0;
    for (int yy = by - radius; yy <= by + radius; ++yy) {
        if (yy < 0 || yy >= h) continue;
        const T* row = src + (size_t)yy * (size_t)w;
        for (int xx = bx - radius; xx <= bx + radius; ++xx) {
            if (xx < 0 || xx >= w) continue;
            const double v = (double)row[xx] - bg;
            if (v > peak) peak = v;
            if (v > 0.0) pos_sum += v;
        }
    }
    if (peak > 0.0) {
        if (pos_sum > 0.0) m.peak_fraction = peak / pos_sum;
        const double quarter = 0.25 * peak;
        for (int yy = by - radius; yy <= by + radius; ++yy) {
            if (yy < 0 || yy >= h) continue;
            const T* row = src + (size_t)yy * (size_t)w;
            for (int xx = bx - radius; xx <= bx + radius; ++xx) {
                if (xx < 0 || xx >= w) continue;
                if ((double)row[xx] - bg > quarter) ++m.n_quarter;
            }
        }
    }
    return m;
}

// 就地点源形状门: 返回被拒数（stars 已收缩）。
template <typename T>
static int sdet_apply_psf_shape_gate(std::vector<StarRecord>& stars, const T* src,
                                     int w, int h, const SDetParams& params) {
    const size_t n = stars.size();
    if (n == 0) return 0;
    const bool lo_on = !(params.psfFwhmLoRatio < 0.0f);
    const bool hi_on = !(params.psfFwhmHiRatio < 0.0f);
    const bool pf_on = !(params.maxPeakFraction < 0.0f);
    const bool np_on = !(params.minQuarterMaxPixels < 0);
    if (!lo_on && !hi_on && !pf_on && !np_on) return 0;   // 直通（零成本）
    const double lo = (params.psfFwhmLoRatio > 0.0f) ? (double)params.psfFwhmLoRatio
                                                     : SDET_SHAPE_FWHM_LO_DEFAULT;
    const double hi = (params.psfFwhmHiRatio > 0.0f) ? (double)params.psfFwhmHiRatio
                                                     : SDET_SHAPE_FWHM_HI_DEFAULT;
    const double pf_max = (params.maxPeakFraction > 0.0f) ? (double)params.maxPeakFraction
                                                          : SDET_SHAPE_PF_DEFAULT;
    const int n_min = (params.minQuarterMaxPixels > 0) ? params.minQuarterMaxPixels
                                                    : SDET_SHAPE_NQUARTER_DEFAULT;
    const int radius = (params.fitRadius > 0) ? params.fitRadius
                                              : SDET_SHAPE_WINDOW_DEFAULT;
    std::vector<double> fw(n);
    for (size_t i = 0; i < n; ++i)
        fw[i] = 0.5 * ((double)stars[i].fwhm_x + (double)stars[i].fwhm_y);
    const double f0 = sdet_median_of(fw);
    const double f_lo = f0 * lo, f_hi = f0 * hi;
    size_t wr = 0;
    for (size_t i = 0; i < n; ++i) {
        const StarRecord& s = stars[i];
        bool drop = false;
        if (lo_on || hi_on) {
            const double f = 0.5 * ((double)s.fwhm_x + (double)s.fwhm_y);
            if ((lo_on && f < f_lo) || (hi_on && f > f_hi)) drop = true;
        }
        if (!drop && (pf_on || np_on)) {
            const SdetShapeMetrics m = sdet_shape_metrics<T>(
                src, w, h, s.cx, s.cy, (double)s.background, radius);
            if ((pf_on && m.peak_fraction > pf_max) || (np_on && m.n_quarter < n_min))
                drop = true;
        }
        if (!drop) stars[wr++] = s;
    }
    const int n_rej = (int)(n - wr);
    stars.resize(wr);
    return n_rej;
}

struct LMWorkspace {
    std::vector<double> fvec, fvec_new, J, JtJ, Jtf, delta, x_new, rhs, A_aug;
    void resize(int m, int n) {
        fvec.resize(m);
        fvec_new.resize(m);
        J.resize((std::size_t)m * (std::size_t)n);
        JtJ.resize((std::size_t)n * (std::size_t)n);
        Jtf.resize(n);
        delta.resize(n);
        x_new.resize(n);
        rhs.resize(n);
        A_aug.resize((std::size_t)n * (std::size_t)n);
    }
};

// 输入: image (原始像素), rect (box), cx/cy (候选中心), bkg0, sat_threshold
// 输出: fit_result (B, A, cx, cy, sx, sy, theta, fwhm_x/y, mad=rmse)
// 返回: SDET_FIT_OK / SDET_FIT_NO_CONVERGENCE / SDET_FIT_INVALID_PARAMS
// SPEC: 5.12 O12 椭圆高斯 LM 拟合（nls_lm 后端; ALG 2 行为合同）
// 冻结参数: max_iter = 20（饱和 *3 = 60）, xtol = gtol = ftol = 1e-3;
// factor_up 3 / factor_down 2 / avmax 0.75 / xtol_abs 1e-6 与 nls_lm
// 默认一致（照录不覆盖）。初值（ALG 2 初始化条款）: halfA 边界搜索——
// 中心像素 max_val, A0 = max_val - bkg0, 沿中心行/列向外走至
// val - bkg0 <= A0/2, FWHM = 边界间距 → sigma = FWHM/2.3548200450309493;
// th0 = 0。输出: theta 归一到 (-90,90]（theta±90 与 sx/sy 交换同施,
// fail-closed 经 normalize_angle_deg_bounded）; mad =
// median(|res - median(res)|)（真实 MAD, B4-4）; 状态 != Success /
// 非有限 / A <= 0 → NO_CONVERGENCE（SDET_FIT_ITERATION_LIMIT 码位保留,
// 生产统一映射 NO_CONVERGENCE）。
template <typename T>
static int sdet_lm_fit(const T* image, int width,
                       int rect_x0, int rect_y0, int rect_x1, int rect_y1,
                       double cx, double cy, double bkg0, double /*sat_threshold*/,
                       const SamplePixel* samples, int m,
                       bool has_saturated, InternalFitResult* result,
                       double max_axis_ratio = 0.0) {
    std::memset(result, 0, sizeof(InternalFitResult));
    result->status = SDET_FIT_INVALID_PARAMS;
    if (!image || !samples || !result || m <= NPARAMS) return SDET_FIT_INVALID_PARAMS;

    // ---- 初值: halfA 边界搜索 ----
    double A0 = 0.0, fwhm_x0 = 2.0, fwhm_y0 = 2.0;
    const int ix = (int)std::floor(cx);
    const int iy = (int)std::floor(cy);
    if (ix >= rect_x0 && ix < rect_x1 && iy >= rect_y0 && iy < rect_y1) {
        // ALG O12 初值: A0 = max_val - bkg0（峰 ±1 邻域最大; 防平台星扫描序峰偏置
        //   下的低 A0 初值把 LM 拖入局部极小）; halfA 边界搜索自中心像素行/列。
        double max_val = (double)image[(size_t)iy * (size_t)width + (size_t)ix];
        for (int dy2 = -1; dy2 <= 1; ++dy2)
            for (int dx2 = -1; dx2 <= 1; ++dx2) {
                const int nx2 = ix + dx2, ny2 = iy + dy2;
                if (nx2 < rect_x0 || nx2 >= rect_x1 || ny2 < rect_y0 || ny2 >= rect_y1) continue;
                const double v2 = (double)image[(size_t)ny2 * (size_t)width + (size_t)nx2];
                if (v2 > max_val) max_val = v2;
            }
        A0 = max_val - bkg0;
        if (A0 > 0.0) {
            const double halfA = 0.5 * A0;
            // 行方向（y = iy）: 右行至 val - bkg0 <= A0/2
            int jj1 = rect_x1 - 1;
            for (int x = ix + 1; x < rect_x1; ++x) {
                if ((double)image[(size_t)iy * (size_t)width + (size_t)x] - bkg0 <= halfA) { jj1 = x; break; }
            }
            int jj2 = rect_x0;
            for (int x = ix - 1; x >= rect_x0; --x) {
                if ((double)image[(size_t)iy * (size_t)width + (size_t)x] - bkg0 <= halfA) { jj2 = x; break; }
            }
            if (jj1 > jj2) fwhm_x0 = (double)(jj1 - jj2);
            // 列方向（x = ix）同构
            int ii1 = rect_y1 - 1;
            for (int y = iy + 1; y < rect_y1; ++y) {
                if ((double)image[(size_t)y * (size_t)width + (size_t)ix] - bkg0 <= halfA) { ii1 = y; break; }
            }
            int ii2 = rect_y0;
            for (int y = iy - 1; y >= rect_y0; --y) {
                if ((double)image[(size_t)y * (size_t)width + (size_t)ix] - bkg0 <= halfA) { ii2 = y; break; }
            }
            if (ii1 > ii2) fwhm_y0 = (double)(ii1 - ii2);
        }
    }
    double x0[NPARAMS];
    x0[0] = cx;
    x0[1] = cy;
    x0[2] = std::min(std::max(fwhm_x0 / GAUSSIAN_FWHM_FACTOR, 0.3), 100.0);
    x0[3] = std::min(std::max(fwhm_y0 / GAUSSIAN_FWHM_FACTOR, 0.3), 100.0);
    x0[4] = 0.0;
    x0[5] = A0 > 1.0 ? A0 : 1.0;
    x0[6] = bkg0;

    // ---- nls_lm 求解（冻结参数; 非默认项显式照录）----
    std::vector<double> ybuf((size_t)m);
    for (int k = 0; k < m; ++k) ybuf[(size_t)k] = samples[k].val;
    PSFFitData pdata;
    pdata.n = (size_t)m;
    pdata.y = ybuf.data();
    pdata.NbRows = (size_t)m;
    pdata.NbCols = (size_t)NPARAMS;
    pdata.samples = samples;
    pdata.cx0 = cx;
    pdata.cy0 = cy;
    pdata.rmse = 0.0;

    acsd::star_detection::nls::Options opts;
    opts.max_iter = (size_t)(LM_MAX_ITER_ANGLE * (has_saturated ? 3 : 1));
    opts.xtol = LM_XTOL;
    opts.ftol = LM_FTOL;
    opts.gtol = LM_GTOL;

    double x0_init[NPARAMS];
    std::memcpy(x0_init, x0, sizeof(x0));
    acsd::star_detection::nls::Report rep =
        acsd::star_detection::nls::solve(&sdet_gaussian_f, &sdet_gaussian_df, &pdata,
                                            (size_t)m, (size_t)NPARAMS, x0, opts, nullptr);
    if (rep.status != acsd::star_detection::nls::Status::Success) {
        // 数值失效重试（实现级保护）: 圆星 theta 平坦方向 + 缺顶样本（邻星决定的
        // sat_threshold 剔除峰顶）可令信赖域内层重试耗尽; 以 sigma 反向扰动 5% 打破
        // 确定性停滞轨迹重试一次, 两次均失败才 NO_CONVERGENCE（判据合同不变）。
        double xr[NPARAMS];
        std::memcpy(xr, x0_init, sizeof(xr));
        xr[2] *= 1.05;
        xr[3] *= 0.97;
        xr[4] = 0.0;
        acsd::star_detection::nls::Report rep2 =
            acsd::star_detection::nls::solve(&sdet_gaussian_f, &sdet_gaussian_df, &pdata,
                                                (size_t)m, (size_t)NPARAMS, xr, opts, nullptr);
        if (rep2.status == acsd::star_detection::nls::Status::Success) {
            rep = rep2;
            std::memcpy(x0, xr, sizeof(x0));
        } else {
        // 第三层重试（实现级数值保护）: 完美圆星 theta 不可辨识,
        // 两次 7 参尝试均 NumericalFailure 时冻结 theta 以 6 参数收敛（其余
        // 可观测量不受 theta 等效角影响; 判据合同不变）。
        double x6[6];
        x6[0] = x0_init[0]; x6[1] = x0_init[1];
        x6[2] = x0_init[2] * 1.05; x6[3] = x0_init[3] * 0.97;
        x6[4] = x0_init[5]; x6[5] = x0_init[6];
        acsd::star_detection::nls::Report rep3 =
            acsd::star_detection::nls::solve(&sdet_gaussian_f6, &sdet_gaussian_df6, &pdata,
                                                (size_t)m, (size_t)6, x6, opts, nullptr);
        if (rep3.status == acsd::star_detection::nls::Status::Success &&
            x6[2] > 0.2122 && x6[3] > 0.2122) {   // 0.5 px FWHM 门等效 sigma 下限
            rep = rep3;
            x0[0] = x6[0]; x0[1] = x6[1]; x0[2] = x6[2]; x0[3] = x6[3];
            x0[4] = 0.0; x0[5] = x6[4]; x0[6] = x6[5];
        } else {
        // 第四层重试（实现级数值保护）: sigma 各向同性 5 参数（圆星数据
        // 下 sigma_x/sigma_y 仅联合可辨识, 6 参数仍秩亏时冻结等效宽度）。
        double x5[5];
        x5[0] = x0_init[0]; x5[1] = x0_init[1];
        x5[2] = 0.5 * (x0_init[2] + x0_init[3]);
        x5[3] = x0_init[5]; x5[4] = x0_init[6];
        acsd::star_detection::nls::Report rep4 =
            acsd::star_detection::nls::solve(&sdet_gaussian_f5, &sdet_gaussian_df5, &pdata,
                                                (size_t)m, (size_t)5, x5, opts, nullptr);
        if (rep4.status == acsd::star_detection::nls::Status::Success &&
            x5[2] > 0.2122) {   // 同上（sigma 各向同性）
            rep = rep4;
            x0[0] = x5[0]; x0[1] = x5[1]; x0[2] = x5[2]; x0[3] = x5[2];
            x0[4] = 0.0; x0[5] = x5[3]; x0[6] = x5[4];
        }
        }
        }
    }

    // ---- 终残差 → RMSE / 真实 MAD（B4-4: mad = median|res - med(res)|, 不预换算）----
    std::vector<double> res((size_t)m);
    sdet_gaussian_f(x0, &pdata, res.data());
    double ss = 0.0;
    for (int k = 0; k < m; ++k) ss += res[(size_t)k] * res[(size_t)k];
    pdata.rmse = std::sqrt(ss / (double)m);
    const double res_med = sdet_median_of(res);
    std::vector<double> absdev((size_t)m);
    for (int k = 0; k < m; ++k) absdev[(size_t)k] = std::fabs(res[(size_t)k] - res_med);
    const double mad = sdet_median_of(absdev);

    // ---- 状态装配（ALG :621-632 行为合同）----
    bool ok = (rep.status == acsd::star_detection::nls::Status::Success);
    for (int k = 0; k < NPARAMS; ++k) if (!std::isfinite(x0[k])) ok = false;
    if (!(x0[5] > 0.0) || !(x0[2] > 0.0) || !(x0[3] > 0.0)) ok = false;
    if (!ok) {
        result->status = SDET_FIT_NO_CONVERGENCE;
        result->mad = mad;
        return SDET_FIT_NO_CONVERGENCE;
    }

    // 简并流形保护（实现级数值保护）: 圆星数据下 theta/sigma 联合不可
    // 辨识, 收敛点可沿等效流形漂到 maxAxisRatio 门外的轴比; 已收敛但超出该门时
    // 以 sigma 各向同性 5 参数重拟合（位置/A/B 可观测量不受影响）。
    if (max_axis_ratio > 0.0 &&
        rep.status == acsd::star_detection::nls::Status::Success &&
        x0[2] > 0.0 && x0[3] > 0.0) {
        const double sxa = x0[2], sya = x0[3];
        if (std::max(sxa, sya) / std::min(sxa, sya) > max_axis_ratio) {
            double x5[5];
            x5[0] = x0_init[0]; x5[1] = x0_init[1];
            x5[2] = 0.5 * (x0_init[2] + x0_init[3]);
            x5[3] = x0_init[5]; x5[4] = x0_init[6];
            acsd::star_detection::nls::Report rep5 =
                acsd::star_detection::nls::solve(&sdet_gaussian_f5, &sdet_gaussian_df5, &pdata,
                                                    (size_t)m, (size_t)5, x5, opts, nullptr);
            if (rep5.status == acsd::star_detection::nls::Status::Success &&
                x5[2] > 0.2122) {
                rep = rep5;
                x0[0] = x5[0]; x0[1] = x5[1]; x0[2] = x5[2]; x0[3] = x5[2];
                x0[4] = 0.0; x0[5] = x5[3]; x0[6] = x5[4];
            }
        }
    }
    // ---- theta 装配: 等效 theta±90 与 sx/sy 交换同施, 归一 (-90,90] fail-closed ----
    double sxr = x0[2], syr = x0[3];
    double deg = x0[4] * 180.0 / 3.14159265358979323846;
    // 先折算到 [-180,180]（周期恒等; LM 平坦方向可漂移至大辐角, 步进循环无法海量迭代）
    deg = std::atan2(std::sin(deg * 3.14159265358979323846 / 180.0),
                     std::cos(deg * 3.14159265358979323846 / 180.0)) * 180.0 /
         3.14159265358979323846;
    if (!std::isfinite(deg)) {
        result->status = SDET_FIT_NO_CONVERGENCE;
        return SDET_FIT_NO_CONVERGENCE;
    }
    int guard = 0;
    while (deg > 90.0 && guard++ < 4) { deg -= 90.0; std::swap(sxr, syr); }
    while (deg <= -90.0 && guard++ < 4) { deg += 90.0; std::swap(sxr, syr); }
    double dnorm = 0.0;
    if (!acsd::star_detector::normalize_angle_deg_bounded(deg, &dnorm)) {
        result->status = SDET_FIT_NO_CONVERGENCE;
        return SDET_FIT_NO_CONVERGENCE;
    }

    result->status = SDET_FIT_OK;
    result->B = x0[6];
    result->A = x0[5];
    result->cx = x0[0];
    result->cy = x0[1];
    result->sx = sxr;
    result->sy = syr;
    result->theta = dnorm;
    result->fwhm_x = GAUSSIAN_FWHM_FACTOR * sxr;
    result->fwhm_y = GAUSSIAN_FWHM_FACTOR * syr;
    result->mad = mad;
    return SDET_FIT_OK;
}
// SPEC: 5.1 O1 背景噪声估计（Project-defined; ALG 5 并行冻结三处之一）
// 逐行差分 d[y][x] = I[y][x] - I[y][x-1]（Var(d) = 2*sigma^2*(1-rho), 邻域去相关）
// → 每行 3 轮 5*sigma clip（median/MAD 迭代, MAD→sigma 系数 1.482602218505602）
// → 行标准差（裁剪后样本, 总体式）→ 行中位 → *0.70710678118654752（1/sqrt(2)）。
// 行间 OpenMP static 并行; 行样本 < 8 不产出; isfinite 归约（NaN 行不产出
// 样本, NaN 输入不静默通过）; 全部行失效返回 0（返回值检查）。
template <typename T>
static T sdet_compute_bgnoise(const T* img, int width, int height) {
    if (!img || width < 2 || height < 1) return (T)0;
    std::vector<double> row_stdevs((size_t)height, -1.0);
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < height; ++y) {
        std::vector<double> d;
        d.reserve((size_t)(width - 1));
        const T* row = img + (size_t)y * (size_t)width;
        for (int x = 1; x < width; ++x) {
            const double diff = (double)row[x] - (double)row[x - 1];
            if (std::isfinite(diff)) d.push_back(diff);
        }
        for (int round = 0; round < 3 && (int)d.size() >= 8; ++round) {
            const double med = sdet_robust_median_d(d.data(), (int)d.size());
            const double sig = sdet_robust_mad_d(d.data(), (int)d.size());
            if (!(sig > 0.0)) break;
            const double lo = med - 5.0 * sig;
            const double hi = med + 5.0 * sig;
            std::vector<double> keep;
            keep.reserve(d.size());
            for (size_t k = 0; k < d.size(); ++k)
                if (d[k] >= lo && d[k] <= hi) keep.push_back(d[k]);
            d.swap(keep);
        }
        if ((int)d.size() < 8) continue;
        double mean = 0.0;
        for (size_t k = 0; k < d.size(); ++k) mean += d[k];
        mean /= (double)d.size();
        double var = 0.0;
        for (size_t k = 0; k < d.size(); ++k) {
            const double e = d[k] - mean;
            var += e * e;
        }
        var /= (double)d.size();
        row_stdevs[(size_t)y] = std::sqrt(var);
    }
    std::vector<double> valid;
    valid.reserve((size_t)height);
    for (int y = 0; y < height; ++y)
        if (row_stdevs[(size_t)y] >= 0.0) valid.push_back(row_stdevs[(size_t)y]);
    if (valid.empty()) return (T)0;
    const double med_std = sdet_robust_median_d(valid.data(), (int)valid.size());
    return (T)(med_std * 0.70710678118654752);
}
// SPEC: 5.9/5.12 拟合段: O9 拟合盒采样 + O12 调用（ALG 2 伪代码 :140-149）
// 采样: 盒内全部有限像素; sat_threshold > 0 时排除 val >= sat_threshold
// （ALG 2 饱和 mask 条款 :530-531; sat_threshold <= 0 时不启用 mask, 调试
// 路径合同）; 排除后样本数 <= NPARAMS+1 → INVALID_PARAMS。
// bkg0: 盒内下半像素截尾 MAD clip 后中位（ALG 2 初始化条款 :560-592）:
// 下半（<= 中位, 含中位元素）→ med0; MAD0 * 1.482602218505602 → sig0;
// 保留 |v - med0| <= 3*sig0 → 中位（sig0 为 0 时直接取 med0）。
// 饱和通道判定: 中心像素 >= sat_threshold（供 lm_fit 3* 迭代与 O13(4) 豁免）。
template <typename T>
int sdet_gauss_fit(const T* image, int width, int height,
                     double cx, double cy,
                     int rect_x0, int rect_y0, int rect_x1, int rect_y1,
                     InternalFitResult* result, LMWorkspace* /*ws*/ = nullptr,
                     double sat_threshold = 0.0, double /*init_sx*/ = 0.0,
                     double /*init_sy*/ = 0.0, double /*bg_init*/ = 0.0,
                     double max_axis_ratio = 0.0) {
    std::memset(result, 0, sizeof(InternalFitResult));
    result->status = SDET_FIT_INVALID_PARAMS;
    if (!image || !result || width <= 0 || height <= 0) return SDET_FIT_INVALID_PARAMS;

    // O9 边界收缩: 拟合盒钳位帧内（不出帧）
    int rx0 = rect_x0, ry0 = rect_y0, rx1 = rect_x1, ry1 = rect_y1;
    if (rx0 < 0) rx0 = 0;
    if (ry0 < 0) ry0 = 0;
    if (rx1 > width) rx1 = width;
    if (ry1 > height) ry1 = height;
    if (rx1 - rx0 < 1 || ry1 - ry0 < 1) return SDET_FIT_INVALID_PARAMS;

    std::vector<SamplePixel> samples;
    std::vector<double> allvals;
    samples.reserve((size_t)((rx1 - rx0) * (ry1 - ry0)));
    for (int y = ry0; y < ry1; ++y) {
        for (int x = rx0; x < rx1; ++x) {
            const double val = (double)image[(size_t)y * (size_t)width + (size_t)x];
            if (!std::isfinite(val)) continue;
            allvals.push_back(val);
            if (sat_threshold > 0.0 && val >= sat_threshold) continue;
            SamplePixel sp;
            sp.dx = (double)x + 0.5 - cx;
            sp.dy = (double)y + 0.5 - cy;
            sp.val = val;
            samples.push_back(sp);
        }
    }
    if ((int)samples.size() <= NPARAMS + 1) return SDET_FIT_INVALID_PARAMS;

    // bkg0: 下半截尾 MAD clip 后中位
    double bkg0;
    {
        std::sort(allvals.begin(), allvals.end());
        const size_t half = (allvals.size() + 1) / 2;
        std::vector<double> lower(allvals.begin(),
                                  allvals.begin() + (std::ptrdiff_t)half);
        bkg0 = sdet_median_of(lower);
        const double sig0 = sdet_robust_mad_d(lower.data(), (int)lower.size());
        if (sig0 > 0.0) {
            std::vector<double> keep;
            keep.reserve(lower.size());
            for (size_t k = 0; k < lower.size(); ++k)
                if (std::fabs(lower[k] - bkg0) <= 3.0 * sig0) keep.push_back(lower[k]);
            if (!keep.empty()) bkg0 = sdet_median_of(keep);
        }
    }

    // 饱和通道: 中心像素 >= sat_threshold
    bool has_sat = false;
    if (sat_threshold > 0.0) {
        const int ixc = (int)std::floor(cx);
        const int iyc = (int)std::floor(cy);
        if (ixc >= 0 && ixc < width && iyc >= 0 && iyc < height)
            has_sat = ((double)image[(size_t)iyc * (size_t)width + (size_t)ixc] >= sat_threshold);
    }

    return sdet_lm_fit<T>(image, width, rx0, ry0, rx1, ry1, cx, cy, bkg0,
                          sat_threshold, samples.data(), (int)samples.size(),
                          has_sat, result, max_axis_ratio);
}
// sat 为饱和阈值，pixel > sat 视为饱和平台内

// 半阈值饱和星检测（edge-walking 消费; KEEP 区接口面定义）
struct SaturatedCandidate {
    double cx, cy;
    float r;
    int pixel_count;
};
// 接近图像边界（<2px）返回 false（丢弃该饱和星）
bool edge_walking_center(const float* fimg, int width, int height,
                         int xx, int yy, float sat,
                         double& center_x, double& center_y) {
    // 起点边界检查：距边界 <2px 直接丢弃
    if (xx < 2 || yy < 2 || xx >= width - 2 || yy >= height - 2)
        return false;

    // 向右走：找到最右侧 pixel > sat 的像素
    int xr = xx;
    for (int x = xx + 1; x < width - 1; x++) {
        if (fimg[yy * width + x] <= sat) break;
        xr = x;
    }
    if (xr >= width - 2) return false;

    // 向左走
    int xl = xx;
    for (int x = xx - 1; x >= 1; x--) {
        if (fimg[yy * width + x] <= sat) break;
        xl = x;
    }
    if (xl < 2) return false;

    // 向下走
    int yd = yy;
    for (int y = yy + 1; y < height - 1; y++) {
        if (fimg[y * width + xx] <= sat) break;
        yd = y;
    }
    if (yd >= height - 2) return false;

    // 向上走
    int yu = yy;
    for (int y = yy - 1; y >= 1; y--) {
        if (fimg[y * width + xx] <= sat) break;
        yu = y;
    }
    if (yu < 2) return false;

    // 几何中心 = (xr+xl)/2, (yd+yu)/2
    center_x = (double)(xr + xl) / 2.0;
    center_y = (double)(yd + yu) / 2.0;
    return true;
}

// 饱和星检测：阈值 70% 动态范围 + 连通域 + edge-walking 中心
// out_sat_threshold 输出饱和阈值，供后续 PSF mask 拟合使用
// out_img_median 输出全局中位数背景，供饱和星 mag 计算使用
// 返回 false: 连通域分析内部 malloc 失败 (无法区分"无饱和星"与分配失败, 不得静默吞掉)
bool sdet_detect_saturated_stars(const float* fimg, int width, int height,
                                  std::vector<SaturatedCandidate>& sat_stars,
                                  float& out_sat_threshold,
                                  float& out_img_median) {
    auto t0 = std::chrono::high_resolution_clock::now();

    size_t n = (size_t)width * height;

    // 计算动态范围 + median (:, bg 用 median 而非 img_min)
    float img_min = 1e30f, img_max = -1e30f;
    // WIN-PORT: MSVC 传统 OpenMP (=2.0) 不支持 reduction(min:/max:) ⇒ C7660
    // ("需要 -openmp:llvm")。改为 2.0 可表达的等价形式: 线程私有极值 + critical 归并。
    // 语义等价: min/max 只做比较与赋值, 无浮点重结合误差 ⇒ 结果与并行归约逐位相同。
    #pragma omp parallel
    {
        float lmin = 1e30f, lmax = -1e30f;
        #pragma omp for schedule(static) nowait
        for (int i = 0; i < (int)n; i++) {
            if (fimg[i] < lmin) lmin = fimg[i];
            if (fimg[i] > lmax) lmax = fimg[i];
        }
        #pragma omp critical
        {
            if (lmin < img_min) img_min = lmin;
            if (lmax > img_max) img_max = lmax;
        }
    }
    // 计算 median 作为背景估计
    std::vector<float> img_copy(fimg, fimg + n);
    std::nth_element(img_copy.begin(), img_copy.begin() + n / 2, img_copy.end());
    float img_median = img_copy[n / 2];
    out_img_median = img_median;  // 输出供饱和星 mag 计算

    // 饱和阈值：median + dynrange * 0.7

    float bg = img_median;  // , 用 median 替代 img_min
    float dynrange = img_max - bg;
    float minsatlevel = dynrange * 0.7f;
    float sat_threshold = bg + minsatlevel;
    float satrange = dynrange * 0.1f;  // 平台判定范围（保留）

    out_sat_threshold = sat_threshold;

    sdet_log(SDET_LOG_INFO, "SDET", "Saturated star threshold: %.1f (bg=%.1f dynrange=%.1f minsatlevel=%.1f satrange=%.1f)",
             sat_threshold, bg, dynrange, minsatlevel, satrange);

    // 二值化：pixel > sat_threshold
    std::vector<float> binary(n, 0.0f);
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < (int)n; i++) {
        if (fimg[i] > sat_threshold) binary[i] = 1.0f;
    }

    // 连通域分析
    ConnectedComponent* components = nullptr;
    int comp_count = 0;
    if (sdet_find_connected_components(binary.data(), width, height, &components, &comp_count) < 0) {
        // 连通域数组 malloc 失败: 静默返回 0 个饱和星会伪造科学结果, 上报失败
        sdet_log(SDET_LOG_ERROR, "SDET", "Saturated star detection: connected component allocation failed");
        return false;
    }

    sdet_log(SDET_LOG_INFO, "SDET", "Saturated star connected components: %d", comp_count);

    // 对每个连通域计算 edge-walking 几何中心
    // 岛级几何过滤 (与 sdet_mark_sat_islands 的 O4a 判据同族, 但本函数面向
    // 半阈值 CC, 故按其自身阈值重新定阈, 不共用 O4a 的 sat_threshold)。
    // IPv: meanhigh = 连通域内像素平均值(等价于 3x3 邻域对小连通域)
    int ew_fail_count = 0;
    int meanhigh_filtered = 0;
    for (int i = 0; i < comp_count; i++) {
        if (components[i].count <= 1) continue;
        int bw = components[i].x1 - components[i].x0 + 1;
        int bh = components[i].y1 - components[i].y0 + 1;
        if (bw < 1 || bh < 1) continue;
        float ar = (float)std::max(bw, bh) / std::max(std::min(bw, bh), 1);
        if (ar > 3.0f) continue;

        // 加权重心作为 edge-walking 起点
        double sum_wx = 0, sum_wy = 0, sum_w = 0;
        for (int j = 0; j < components[i].count; j++) {
            const std::size_t idx =
                (std::size_t)components[i].py[j] * (std::size_t)width +
                (std::size_t)components[i].px[j];
            float val = fimg[idx];
            sum_wx += (double)components[i].px[j] * val;
            sum_wy += (double)components[i].py[j] * val;
            sum_w += val;
        }

        int start_x = (sum_w > 0) ? (int)(sum_wx / sum_w + 0.5) : (components[i].x0 + components[i].x1) / 2;
        int start_y = (sum_w > 0) ? (int)(sum_wy / sum_w + 0.5) : (components[i].y0 + components[i].y1) / 2;

        // edge-walking 几何中心
        double ew_cx, ew_cy;
        if (!edge_walking_center(fimg, width, height, start_x, start_y, sat_threshold, ew_cx, ew_cy)) {
            ew_fail_count++;
            continue;  // 接近边界，丢弃
        }

        // 等效半径 r = sqrt(pixel_count / π)
        float r = std::sqrt((float)components[i].count / (float)M_PI);

        sat_stars.push_back({ew_cx, ew_cy, r, components[i].count});
    }
    sdet_free_connected_components(components, comp_count);

    auto t1 = std::chrono::high_resolution_clock::now();
    sdet_log(SDET_LOG_INFO, "SDET", "Saturated stars: %d (meanhigh filtered: %d, edge-walking failed: %d, %.1f ms)",
             (int)sat_stars.size(), meanhigh_filtered, ew_fail_count, std::chrono::duration<double, std::milli>(t1 - t0).count());
    return true;
}

// 可选输出参数名解析
enum ExtraField {
    EXTRA_FWHM_X = 0,
    EXTRA_FWHM_Y,
    EXTRA_SX,
    EXTRA_SY,
    EXTRA_THETA,
    EXTRA_BACKGROUND,
    EXTRA_AMPLITUDE,
    EXTRA_R,
    EXTRA_CAND_R,
    EXTRA_UNKNOWN
};

ExtraField parse_extra_name(const char* name) {
    if (strcmp(name, "fwhm_x") == 0) return EXTRA_FWHM_X;
    if (strcmp(name, "fwhm_y") == 0) return EXTRA_FWHM_Y;
    if (strcmp(name, "sx") == 0) return EXTRA_SX;
    if (strcmp(name, "sy") == 0) return EXTRA_SY;
    if (strcmp(name, "theta") == 0) return EXTRA_THETA;
    if (strcmp(name, "background") == 0) return EXTRA_BACKGROUND;
    if (strcmp(name, "amplitude") == 0) return EXTRA_AMPLITUDE;
    if (strcmp(name, "r") == 0) return EXTRA_R;
    if (strcmp(name, "cand_R") == 0) return EXTRA_CAND_R;
    return EXTRA_UNKNOWN;
}

float get_extra_field(const StarRecord& star, ExtraField field) {
    // 饱和星现在也有 PSF 拟合数据，不再填 -1.0
    // 拟合失败时 fwhm_x 等为 0.0
    switch (field) {
        case EXTRA_FWHM_X:     return star.fwhm_x;
        case EXTRA_FWHM_Y:     return star.fwhm_y;
        case EXTRA_SX:         return star.sx;
        case EXTRA_SY:         return star.sy;
        case EXTRA_THETA:      return star.theta;
        case EXTRA_BACKGROUND: return star.background;
        case EXTRA_AMPLITUDE:  return star.amplitude;
        case EXTRA_R:          return star.r;
        case EXTRA_CAND_R:     return star.cand_R;
        default:               return 0.0f;
    }
}
// SPEC: 5.15 O15 post-fit 去重（判式冻结, ALG 2 :150-151）
// 饱和-正常: 距离^2 < 4.0（2 px 网格）→ 丢正常星保饱和星;
// 饱和-饱和: 距离^2 < 4.0 → 保 O4a 等效半径 r 大者;
// 正常-正常: 距离^2 <= 1.0（1 px 网格）→ 保先者（记录序 = 候选拟合序）。
// maxStars 截断在输出段执行（本函数不做）。
void sdet_dedup_stars(std::vector<StarRecord>& stars) {
    if (stars.size() < 2) return;
    std::vector<char> drop(stars.size(), 0);
    for (size_t i = 0; i < stars.size(); ++i) {
        if (drop[i]) continue;
        for (size_t j = i + 1; j < stars.size(); ++j) {
            if (drop[j]) continue;
            const double dx = stars[i].cx - stars[j].cx;
            const double dy = stars[i].cy - stars[j].cy;
            const double d2 = dx * dx + dy * dy;
            if (stars[i].is_saturated && stars[j].is_saturated) {
                if (d2 < 4.0) drop[stars[i].r >= stars[j].r ? j : i] = 1;
            } else if (stars[i].is_saturated || stars[j].is_saturated) {
                if (d2 < 4.0) drop[stars[i].is_saturated ? j : i] = 1;
            } else {
                if (d2 <= 1.0) drop[j] = 1;
            }
        }
    }
    std::vector<StarRecord> keep;
    keep.reserve(stars.size());
    for (size_t i = 0; i < stars.size(); ++i)
        if (!drop[i]) keep.push_back(stars[i]);
    stars.swap(keep);
}
// NaN mag (box_sum<=0) 排到最后
// 注: plate solver (ipv_select) 自己按 mag 重排, 此处排序仅供 API 输出一致性
// SPEC: 5.15 O15 排序: mag 升序 stable_sort, NaN 恒排末尾（全序确定）。
void sdet_sort_stars(std::vector<StarRecord>& stars) {
    std::stable_sort(stars.begin(), stars.end(), [](const StarRecord& a, const StarRecord& b) {
        const bool a_nan = std::isnan((double)a.mag);
        const bool b_nan = std::isnan((double)b.mag);
        if (a_nan || b_nan) return !a_nan && b_nan;
        return a.mag < b.mag;
    });
}
} // anonymous namespace

SDET_EXPORT StarDetectorHandle sdet_create(const SDetParams *params)
{
    StarDetectorHandle_s *sd = (StarDetectorHandle_s *)malloc(sizeof(StarDetectorHandle_s));
    if (!sd) return nullptr;

    if (params) {
        sd->internal.params = *params;
    } else {
        SDetParams defaults;
        defaults.structureLayers = 5;
        defaults.hotPixelFilterRadius = 1;
        defaults.iterativeClipSigma = 9.0f;
        defaults.iterativeMaxRounds = 5;
        defaults.medianFilterDetail = 1;
        defaults.maxStars = 2000;
        defaults.fitRadius = 6;
        defaults.fwhmClipSigma = 3.0f;
        defaults.maxAxisRatio = 2.0f;
        // R-58-1 点源形状门默认（0 = 用内置默认; 详见 star_detector.h 与
        // sdet_apply_psf_shape_gate 抬头）
        defaults.psfFwhmLoRatio = 0.5f;
        defaults.psfFwhmHiRatio = 2.5f;
        defaults.maxPeakFraction = 0.35f;
        defaults.minQuarterMaxPixels = 4;
        sd->internal.params = defaults;
    }

    sd->internal.width = 0;
    sd->internal.height = 0;
    sd->internal.raw_detail = nullptr;

    sdet_log(SDET_LOG_INFO, "SDET", "StarDetector created (fitRadius=%d, fwhmClipSigma=%.1f, maxAxisRatio=%.1f)",
             sd->internal.params.fitRadius, sd->internal.params.fwhmClipSigma, sd->internal.params.maxAxisRatio);
    return sd;
}

SDET_EXPORT void sdet_destroy(StarDetectorHandle handle)
{
    if (!handle) return;
    delete[] handle->internal.raw_detail;
    sdet_log(SDET_LOG_INFO, "SDET", "StarDetector destroyed");
    free(handle);
}

// ============================================================================
// 生产检测主链（设计稿 §5, 16 算子; FP32/FP64 双模板, 算术各自冻结）
// 算子链: O2 平滑 → O1 背景统计 → O4a 饱和岛 → O3 阈值化 + O11 deblending
// 树 + O8 成分内极大 → mag_est 降序 → 逐候选 [O4b 饱和判定 → O5 质心 /
// O6 行走 → O7 零交叉 → O9 拟合盒 → O10 对称门] → O12 并行 LM → O13 排异 +
// O14 孔径测光 → O15 dedup/排序/截断 → 事务化输出。
// ============================================================================
namespace {

// O4a 饱和岛（等效中心 + 等效半径, 供 O6 与饱和星 r 列）
struct SdetSatIsland {
    double cx, cy, r;
};

// 候选（检测段产出, 拟合段消费）
struct SdetCand {
    int ix, iy;              // O8 峰位像素索引（平滑图）
    double x, y;             // 初值中心（像素中心坐标）: 非饱和 = O5 质心, 饱和 = O6 等效中心
    float mag_est;           // O4b meanhigh（候选排序键, ALG :2170-2171 降序不截断）
    double Sr, Sc;           // O7 零交叉宽度（平滑图 sigma 读数）
    double Ar, Ac;           // O7 零交叉振幅读数（已乘 sqrt(e)）
    double srl, srr;         // O7 行向左右零交叉距离（O10 门）
    double scl, scr;         // O7 列向上下零交叉距离（O10 门）
    int R;                   // O9 拟合盒半径
    int rx0, ry0, rx1, ry1;  // O9 钳位帧内拟合盒
    int sat;                 // O4b 饱和判定
    double sat_r;            // O4a 岛等效半径（饱和星 r 列; 非岛内峰为 0）
};

const int    SDET_MAX_BOX_RADIUS  = 200;                    // O9 上限（MAX_BOX_RADIUS）
const int    SDET_DEBLEND_LEVELS  = 30;                     // O11 指数间隔阈值层数（冻结）
const double SDET_DELTA_C         = 5e-3;                   // O11 分流系数 delta_c（冻结）
const int    SDET_DEBLEND_DEPTH   = 8;                      // O11 历史参数: 实为迭代实现, 深度参数未使用（P-140; 常量现无消费者, 保留占位）
const double SDET_SQRT_EXP1       = 1.6487212707001281468;  // sqrt(e)（O7 振幅换算恒等式）

// O9 半径系数 s_factor = sqrt(2 ln 1000)（解析式入常量, 设计稿 §8-7）
const double SDET_S_FACTOR = std::sqrt(2.0 * std::log(1000.0));

// ---- SPEC: §5.11 O11 deblending 树（B&A96 §4; 设计稿 §5.11）----
// t(i) = thr*(Smax/thr)^(i/30), i = 1..30 自高向低; 首个满足「>= 2 枝且枝在
// 当前层阈值之上的积分流量 > delta_c * 父组分检出阈上总流量」的层执行分裂:
// 存活枝 → 独立成分（迭代实现, 权重基准 = 父组分 total_flux = Σ(smooth − thr),
// 见 sdet_deblend_leaf）; 未存活/低于分离阈值
// 的像素按双变量高斯 argmax 重分配
// （mu = 叶峰位, sigma^2 = 叶内权重二阶矩 + 0.25 下限）, 平局判归登记序靠前
// （确定性, 项目定义）。全层不满足 → 单成分（B&A96 §4.3: 间隔 < 2 sigma 不可分,
// 不触发即保持单星）。
struct SdetLeaf {
    std::vector<int> pix;  // 成分像素（升序 = 扫描序）
    int peak;              // O8 成分内极大（平局取扫描序最先）
};

template <typename T>
static void sdet_deblend_leaf(const T* smooth, int w,
                              const std::vector<int>& pix,
                              double thr, double total_flux, int depth,
                              std::vector<SdetLeaf>* out_leaves) {
    // P-140: depth（历史「递归深度上限」）形参在现行迭代实现中未使用, 以 (void)
    // 显式弃用; total_flux（父组分在检出阈值之上的总流量 = Σ(smooth − thr)）为
    // δc 的权重基准（步 (1)）。
    // 判据两侧刻意不对称（B&A96 §4）: 分子 = 枝在**当前层阈值**之上的流量,
    // 基准 = 父组分在**检出阈值**之上的总流量。参照实现:
    //   SExtractor 2.28.2 src/refine.c parcelout() :97 value0 =
    //     objlist[0].obj[0].fdflux * prefs.deblend_mincont; :159/:166 判式
    //     obj[j].fdflux − obj[j].dthresh*obj[j].fdnpix > value0; :163 if (m>1);
    //   SEP 1.4.1 src/deblend.c deblend() :116/:173-190 同式。
    // 形参保留以不改动调用面。
    (void)depth;
    // FIX-R3: 成分内极大 = O11 树阈值层基准 smax（B&A96 §4.1, t(i) 以其为标度）;
    // 候选峰选择不再消费 leaf.peak——O8 冻结语义 = 检测段叶循环 11x11 局部极大
    // 扫描（见 sdet_detect_impl）, peak 字段保留仅供树基准与调试消费。
    int peak = pix[0];
    for (size_t k = 1; k < pix.size(); ++k)
        if ((double)smooth[pix[k]] > (double)smooth[peak]) peak = pix[k];
    const double smax = (double)smooth[peak];
    if (pix.size() < 3 || !(smax > thr)) {
        SdetLeaf leaf;
        leaf.pix = pix;
        leaf.peak = peak;
        out_leaves->push_back(leaf);
        return;
    }
    // ---- B&A96 §4.1 亮度树, 自叶向根评估（设计稿 O11 操作化规则）----
    // 建 30 层指数间隔阈值枝; 自最细层（高阈）向根（低阈）逐层归并:
    //   结点（层 i 枝 b）下被包含的当前成分若 >=2 且其中 >=2 个满足
    //   flow_{t(i)}(子) > delta_c * 组分总流量(b, 检出阈底) 则保持分离,
    //   否则未存活成分并入父枝继续向根评估; 根层仍分离的成分即独立星。
    std::vector<std::vector<std::vector<int>>> layers(
        (size_t)SDET_DEBLEND_LEVELS + 1);
    for (int i = SDET_DEBLEND_LEVELS - 1; i >= 1; --i) {
        const double t = thr * std::pow(smax / thr,
                                        (double)i / (double)SDET_DEBLEND_LEVELS);
        std::vector<int> pix_t;
        pix_t.reserve(pix.size());
        for (size_t k = 0; k < pix.size(); ++k)
            if ((double)smooth[pix[k]] > t) pix_t.push_back(pix[k]);
        if (pix_t.size() < 2) continue;
        std::vector<std::vector<int>>& branches = layers[(size_t)i];
        std::vector<int> blabel(pix_t.size(), -1);
        std::vector<int> queue;
        for (size_t s = 0; s < pix_t.size(); ++s) {
            if (blabel[s] >= 0) continue;
            blabel[s] = (int)branches.size();
            queue.clear();
            queue.push_back((int)s);
            std::vector<int> comp;
            for (size_t qi = 0; qi < queue.size(); ++qi) {
                const int si = queue[qi];
                comp.push_back(pix_t[(size_t)si]);
                const int px = pix_t[(size_t)si] % w;
                const int py = pix_t[(size_t)si] / w;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (!dx && !dy) continue;
                        const int nx = px + dx, ny = py + dy;
                        if (nx < 0 || nx >= w || ny < 0) continue;
                        const int np = ny * w + nx;
                        const std::vector<int>::const_iterator it =
                            std::lower_bound(pix_t.begin(), pix_t.end(), np);
                        if (it == pix_t.end() || *it != np) continue;
                        const int nsi = (int)(it - pix_t.begin());
                        if (blabel[(size_t)nsi] < 0) {
                            blabel[(size_t)nsi] = blabel[s];
                            queue.push_back(nsi);
                        }
                    }
            }
            std::sort(comp.begin(), comp.end());
            branches.push_back(comp);
        }
    }
    std::vector<int> assigned((size_t)pix.back() + 1, -1);
    std::vector<std::vector<int>> comps;
    std::vector<int> comp_peak;
    for (int i = SDET_DEBLEND_LEVELS - 1; i >= 1; --i) {
        const std::vector<std::vector<int>>& brs = layers[(size_t)i];
        if (brs.empty()) continue;
        const double t = thr * std::pow(smax / thr,
                                        (double)i / (double)SDET_DEBLEND_LEVELS);
        // (1) 每枝积分流量判据: 分子 = 枝在当前层阈值 t 之上的积分流量,
        //     权重基准 = 父组分在检出阈值之上的总流量 total_flux（B&A96 §4;
        //     参照实现 SExtractor 2.28.2 src/refine.c:97/:159 与 SEP 1.4.1
        //     src/deblend.c:116/:179 —— 两侧刻意不对称, 与「枝自身流量比」
        //     形态不同, 后者会把任意枝判为显著）。
        std::vector<char> sat1(brs.size(), 0);
        int n_sat1 = 0;
        for (size_t b = 0; b < brs.size(); ++b) {
            double fsum = 0.0;
            for (size_t k = 0; k < brs[b].size(); ++k) {
                fsum += (double)smooth[brs[b][k]] - t;
            }
            if (fsum > SDET_DELTA_C * total_flux) { sat1[b] = 1; ++n_sat1; }
        }
        const bool can_spawn = (n_sat1 >= 2);
        for (size_t b = 0; b < brs.size(); ++b) {
            const std::vector<int>& bp = brs[b];
            std::vector<size_t> contained;
            for (size_t c = 0; c < comps.size(); ++c) {
                bool inside = true;
                if (comps[c].empty()) continue;
                for (size_t k = 0; k < comps[c].size(); ++k) {
                    if (assigned[comps[c][k]] != (int)c) { inside = false; break; }
                    const std::vector<int>::const_iterator it =
                        std::lower_bound(bp.begin(), bp.end(), comps[c][k]);
                    if (it == bp.end() || *it != comps[c][k]) { inside = false; break; }
                }
                if (inside) contained.push_back(c);
            }
            if (contained.empty()) {
                if (can_spawn && sat1[b]) {
                    const int cid = (int)comps.size();
                    std::vector<int> npix;
                    int pk = -1;
                    for (size_t k = 0; k < bp.size(); ++k) {
                        if (assigned[bp[k]] >= 0) continue;
                        assigned[bp[k]] = cid;
                        npix.push_back(bp[k]);
                        if (pk < 0 || (double)smooth[bp[k]] > (double)smooth[pk]) pk = bp[k];
                    }
                    if (!npix.empty()) { comps.push_back(npix); comp_peak.push_back(pk); }
                }
                continue;
            }
            if (contained.size() == 1) {
                const size_t c = contained[0];
                for (size_t k = 0; k < bp.size(); ++k) {
                    if (assigned[bp[k]] >= 0) continue;
                    assigned[bp[k]] = (int)c;
                    comps[c].push_back(bp[k]);
                }
                std::sort(comps[c].begin(), comps[c].end());
                continue;
            }
            // >=2 成分: 未分配像素双变量高斯 argmax（mu=成分峰, sigma^2=权重二阶矩+0.25）
            std::vector<double> mux(contained.size()), muy(contained.size());
            std::vector<double> sx2(contained.size(), 0.25), sy2(contained.size(), 0.25);
            for (size_t s2 = 0; s2 < contained.size(); ++s2) {
                const std::vector<int>& cp = comps[contained[s2]];
                const int pk = comp_peak[contained[s2]];
                const double mxc = (double)(pk % w) + 0.5, myc = (double)(pk / w) + 0.5;
                double wsum = 0.0, wx2 = 0.0, wy2 = 0.0;
                for (size_t k = 0; k < cp.size(); ++k) {
                    const int p = cp[k];
                    const double wgt = std::max((double)smooth[p] - t, 0.0);
                    const double pdx = (double)(p % w) + 0.5 - mxc;
                    const double pdy = (double)(p / w) + 0.5 - myc;
                    wsum += wgt; wx2 += wgt * pdx * pdx; wy2 += wgt * pdy * pdy;
                }
                if (wsum > 0.0) { sx2[s2] = wx2 / wsum + 0.25; sy2[s2] = wy2 / wsum + 0.25; }
                mux[s2] = mxc; muy[s2] = myc;
            }
            for (size_t k = 0; k < bp.size(); ++k) {
                const int p = bp[k];
                if (assigned[p] >= 0) continue;
                const double px = (double)(p % w) + 0.5, py = (double)(p / w) + 0.5;
                int best = -1; double bestp = 0.0;
                for (size_t s2 = 0; s2 < contained.size(); ++s2) {
                    const double ax = (px - mux[s2]) / std::sqrt(sx2[s2]);
                    const double ay = (py - muy[s2]) / std::sqrt(sy2[s2]);
                    const double pr = -0.5 * (ax * ax + ay * ay);
                    if (best < 0 || pr > bestp) { best = (int)s2; bestp = pr; }
                }
                if (best >= 0) {
                    const size_t c = contained[(size_t)best];
                    assigned[p] = (int)c;
                    comps[c].push_back(p);
                }
            }
            for (size_t s2 = 0; s2 < contained.size(); ++s2)
                std::sort(comps[contained[s2]].begin(), comps[contained[s2]].end());
        }
    }
    // 根层后仍未分配像素 → 全成分 argmax（同一判据; 设计稿重分配条款收尾）
    if (!comps.empty()) {
        std::vector<double> mux(comps.size()), muy(comps.size());
        std::vector<double> sx2(comps.size(), 0.25), sy2(comps.size(), 0.25);
        for (size_t c = 0; c < comps.size(); ++c) {
            const int pk = comp_peak[c];
            const double mxc = (double)(pk % w) + 0.5, myc = (double)(pk / w) + 0.5;
            double wsum = 0.0, wx2 = 0.0, wy2 = 0.0;
            for (size_t k = 0; k < comps[c].size(); ++k) {
                const int p = comps[c][k];
                const double wgt = std::max((double)smooth[p] - thr, 0.0);
                const double pdx = (double)(p % w) + 0.5 - mxc;
                const double pdy = (double)(p / w) + 0.5 - myc;
                wsum += wgt; wx2 += wgt * pdx * pdx; wy2 += wgt * pdy * pdy;
            }
            if (wsum > 0.0) { sx2[c] = wx2 / wsum + 0.25; sy2[c] = wy2 / wsum + 0.25; }
            mux[c] = mxc; muy[c] = myc;
        }
        for (size_t p = 0; p < pix.size(); ++p) {
            const int pp = pix[p];
            if (assigned[pp] >= 0) continue;
            const double px = (double)(pp % w) + 0.5, py = (double)(pp / w) + 0.5;
            int best = -1; double bestp = 0.0;
            for (size_t c = 0; c < comps.size(); ++c) {
                const double ax = (px - mux[c]) / std::sqrt(sx2[c]);
                const double ay = (py - muy[c]) / std::sqrt(sy2[c]);
                const double pr = -0.5 * (ax * ax + ay * ay);
                if (best < 0 || pr > bestp) { best = (int)c; bestp = pr; }
            }
            if (best >= 0) {
                assigned[pp] = best;
                comps[(size_t)best].push_back(pp);
            }
        }
        for (size_t c = 0; c < comps.size(); ++c)
            std::sort(comps[c].begin(), comps[c].end());
    }
    if (comps.empty()) {   // 根不分裂 -> 单星（B&A96）
        SdetLeaf leaf;
        leaf.pix = pix;
        leaf.peak = peak;
        out_leaves->push_back(leaf);
        return;
    }
    for (size_t c = 0; c < comps.size(); ++c) {
        if (comps[c].empty()) continue;
        SdetLeaf leaf;
        leaf.pix = comps[c];
        leaf.peak = comp_peak[c];
        out_leaves->push_back(leaf);
    }
}



// ---- SPEC: §5.7 O7 单方向零交叉扫描 ----
// 从峰沿 (dx,dy) 找二阶差分 d2(x) = S[x-1] - 2S[x] + S[x+1] 首次 >= 0（峰侧
// d2 < 0）的像素中心; 返回 dist = 峰到零交叉的像素距离（缺失 → 1.0）、
// raw = 零交叉处平滑读数 - bkg（调用方 * sqrt(e) 换算振幅; 缺失 → 0）。
// 饱和候选（sat_skip）从峰起先跳过原图 >= sat_threshold 的像素段。
// 零交叉位置不内插（半像素偏差由 O9 的 ceil 承载）。
template <typename T>
static void sdet_zero_cross_dir(const T* smooth, int w, int h,
                                const T* src, double sat_threshold,
                                int ix, int iy, int dx, int dy,
                                double bkg, bool sat_skip,
                                double* dist_out, double* raw_out) {
    int x = ix, y = iy;
    if (sat_skip && sat_threshold > 0.0) {
        for (int g = 0; g < 4096; ++g) {
            const int nx = x + dx, ny = y + dy;
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) { *dist_out = 1.0; *raw_out = 0.0; return; }
            x = nx; y = ny;
            if (!((double)src[(size_t)y * (size_t)w + (size_t)x] >= sat_threshold)) break;
        }
    }
    double prev = 0.0;
    bool have = false;
    for (int g = 0; g < 4096; ++g) {
        const int px = x - dx, py = y - dy;
        const int nx = x + dx, ny = y + dy;
        if (px < 0 || px >= w || py < 0 || py >= h) break;
        if (nx < 0 || nx >= w || ny < 0 || ny >= h) break;
        const double d2 =
            (double)smooth[(size_t)py * (size_t)w + (size_t)px]
            - 2.0 * (double)smooth[(size_t)y * (size_t)w + (size_t)x]
            + (double)smooth[(size_t)ny * (size_t)w + (size_t)nx];
        if (have && prev < 0.0 && d2 >= 0.0) {
            double dist = (double)(std::abs(x - ix) + std::abs(y - iy));
            if (dist < 1.0) dist = 1.0;
            *dist_out = dist;
            *raw_out = (double)smooth[(size_t)y * (size_t)w + (size_t)x] - bkg;
            return;
        }
        prev = d2;
        have = true;
        x = nx; y = ny;
    }
    *dist_out = 1.0;
    *raw_out = 0.0;
}

// ---- SPEC: §5.4 O4a 饱和岛预标记 ----
// sat_threshold 单阈二值化（pixel > sat_threshold）→ 4-连通域; 岛级几何过滤:
// 面积 > 1、外接盒长宽比 <= 3; 每岛亮度加权重心 → 四方向 edge-walking 至
// pixel <= sat_threshold 边缘 → 等效中心 = 边缘盒中心; r = sqrt(面积/π)。
// label 语义: >0 = 岛编号（islands 下标 + 1）, -1 = 已访问不合格, 0 = 未访问。
template <typename T>
static void sdet_mark_sat_islands(const T* src, int w, int h, double sat_threshold,
                                  std::vector<int>* label_out,
                                  std::vector<SdetSatIsland>* islands_out) {
    const std::size_t np = (std::size_t)w * (std::size_t)h;
    label_out->assign(np, 0);
    islands_out->clear();
    std::vector<int> queue;
    for (int y0 = 0; y0 < h; ++y0) {
        for (int x0 = 0; x0 < w; ++x0) {
            const int p0 = y0 * w + x0;
            if (!((double)src[p0] > sat_threshold) || (*label_out)[(size_t)p0] != 0) continue;
            const int label = (int)islands_out->size() + 1;
            (*label_out)[(size_t)p0] = label;
            queue.clear();
            queue.push_back(p0);
            int area = 0;
            int minx = w, maxx = -1, miny = h, maxy = -1;
            double wsum = 0.0, wx = 0.0, wy = 0.0;
            for (size_t qi = 0; qi < queue.size(); ++qi) {
                const int p = queue[qi];
                const int px = p % w, py = p / w;
                ++area;
                if (px < minx) minx = px;
                if (px > maxx) maxx = px;
                if (py < miny) miny = py;
                if (py > maxy) maxy = py;
                const double wgt = (double)src[p];
                wsum += wgt;
                wx += wgt * ((double)px + 0.5);
                wy += wgt * ((double)py + 0.5);
                const int nb[4] = { p - 1, p + 1, p - w, p + w };
                const bool ok[4] = { px > 0, px < w - 1, py > 0, py < h - 1 };
                for (int d = 0; d < 4; ++d) {
                    if (!ok[d]) continue;
                    const int q = nb[d];
                    if ((double)src[q] > sat_threshold && (*label_out)[(size_t)q] == 0) {
                        (*label_out)[(size_t)q] = label;
                        queue.push_back(q);
                    }
                }
            }
            const int bw = maxx - minx + 1, bh = maxy - miny + 1;
            const int bmax = bw > bh ? bw : bh;
            const int bmin = bw > bh ? bh : bw;
            if (area <= 1 || (bmin > 0 && (double)bmax / (double)bmin > 3.0)) {
                for (size_t qi = 0; qi < queue.size(); ++qi)
                    (*label_out)[(size_t)queue[qi]] = -1;
                continue;
            }
            SdetSatIsland isl;
            // 亮度加权重心 → 四方向 edge-walking（沿行/列走至 pixel <= sat_threshold）
            int sx0 = (int)std::lround(wx / wsum - 0.5);
            int sy0 = (int)std::lround(wy / wsum - 0.5);
            if (sx0 < minx) sx0 = minx;
            if (sx0 > maxx) sx0 = maxx;
            if (sy0 < miny) sy0 = miny;
            if (sy0 > maxy) sy0 = maxy;
            int xl = sx0, xr = sx0, yd = sy0, yu = sy0;
            while (xl - 1 >= 0 && (double)src[(size_t)sy0 * (size_t)w + (size_t)(xl - 1)] > sat_threshold) --xl;
            while (xr + 1 < w && (double)src[(size_t)sy0 * (size_t)w + (size_t)(xr + 1)] > sat_threshold) ++xr;
            while (yd - 1 >= 0 && (double)src[(size_t)(yd - 1) * (size_t)w + (size_t)sx0] > sat_threshold) --yd;
            while (yu + 1 < h && (double)src[(size_t)(yu + 1) * (size_t)w + (size_t)sx0] > sat_threshold) ++yu;
            // P-135 (Y-7): isl.cx/cy（岛等效中心）当前无消费者——下游仅消费
            // sat_label 与 isl.r（O4b 消费 :2061-2062; O6 饱和中心行走自峰独立
            // 进行, 不读本字段）。字段预留, 在接通消费面前不得宣称已消费。
            isl.cx = 0.5 * ((double)xl + (double)xr) + 0.5;
            isl.cy = 0.5 * ((double)yd + (double)yu) + 0.5;
            isl.r = std::sqrt((double)area / 3.14159265358979323846);
            islands_out->push_back(isl);
        }
    }
}

// ---- 域准备（impl/guided 共用）: 非有限填充 + O2 平滑 + O1 统计族 ----
template <typename T>
struct SdetPrep {
    std::vector<T> finite_buf;
    std::vector<T> smooth;
    const T* src;
    const T* smooth_data;
    double bg, bgnoise, dynrange, sat_threshold, satrange, locthreshold, thr;
};

template <typename T>
static SdetPrep<T> sdet_prepare_field(const T* image, int w, int h) {
    SdetPrep<T> pr;
    const std::size_t np = (std::size_t)w * (std::size_t)h;
    // 非有限输入域处理（ALG §4 NaN 条款推广）: 工作副本将非有限像素以全图
    // 有限中位填充; 无非有限时零拷贝直通。估计器入口逐像素 isfinite 归约,
    // 全非有限帧由调用方经 bg/maxi 检查判空。
    pr.src = image;
    {
        bool bad = false;
        for (std::size_t i = 0; i < np; ++i)
            if (!std::isfinite((double)image[i])) { bad = true; break; }
        if (bad) {
            pr.finite_buf.assign(image, image + np);
            T med;
            if constexpr (std::is_same<T, float>::value)
                med = sdet_robust_median(pr.finite_buf.data(), (int)np);
            else
                med = (T)sdet_robust_median_d(pr.finite_buf.data(), (int)np);
            for (std::size_t i = 0; i < np; ++i)
                if (!std::isfinite((double)pr.finite_buf[i])) pr.finite_buf[i] = med;
            pr.src = pr.finite_buf.data();
        }
    }
    // O2: 高斯平滑（sigma = 2.0 冻结; float/double 变体算术各自冻结）
    pr.smooth.resize(np);
    if constexpr (std::is_same<T, float>::value)
        sdet_gaussian_blur_yvv((const float*)pr.src, (float*)pr.smooth.data(), w, h, 2.0);
    else
        sdet_gaussian_blur_yvv_d((const double*)pr.src, (double*)pr.smooth.data(), w, h, 2.0);
    pr.smooth_data = pr.smooth.data();
    // O1: 背景统计族
    pr.bgnoise = (double)sdet_compute_bgnoise<T>(pr.src, w, h);
    if constexpr (std::is_same<T, float>::value)
        pr.bg = (double)sdet_robust_median(pr.src, (int)np);
    else
        pr.bg = sdet_robust_median_d(pr.src, (int)np);
    double maxi = -1e300;
    for (std::size_t i = 0; i < np; ++i) {
        const double v = (double)pr.src[i];
        if (std::isfinite(v) && v > maxi) maxi = v;
    }
    if (!std::isfinite(pr.bg) || maxi <= -1e300) {
        // 全非有限帧: 判空域（调用方经 thr 有效性检查出空输出）
        pr.dynrange = 0.0;
        pr.sat_threshold = 0.0;
        pr.satrange = 0.0;
        pr.locthreshold = 0.0;
        pr.thr = std::nan("");
        return pr;
    }
    // DISP-STAR-006: 归一化上限 65535.0f; dynrange = min(max, 65535) - bg
    // （相对背景形态, 非绝对 0.7*65535）
    pr.dynrange = std::min(maxi, (double)(float)65535.0f) - pr.bg;
    pr.sat_threshold = pr.bg + 0.7 * pr.dynrange;
    pr.satrange = 0.1 * pr.dynrange;
    pr.locthreshold = 5.0 * pr.bgnoise;
    pr.thr = pr.bg + 5.0 * pr.bgnoise;   // O3: kappa = 5.0 冻结
    return pr;
}

// ---- O15 输出整理 + 事务化输出构造（impl/guided 共用; §3 输出合同）----
// OOM 注入检查点 = 本段连续 malloc 序列（输出六数组 + extras）: 任一失败 →
// 释放全部 → 输出全 NULL + *out_count = -1 + rc = -1; 空场 → 全 NULL +
// *out_count = 0 + rc = 0（非错误）。maxStars 截断在此（输出段, ALG :2383-2385）。
static int sdet_emit_records(std::vector<StarRecord>& stars,
                             long long max_stars,
                             const char** extra_names, int extra_count,
                             double** out_x, double** out_y, float** out_flux,
                             int** out_saturated, float** out_mag,
                             int** out_has_saturated, int* out_count,
                             float*** out_extras) {
    sdet_dedup_stars(stars);
    sdet_sort_stars(stars);
    if (max_stars > 0 && (long long)stars.size() > max_stars)
        stars.resize((size_t)max_stars);
    const int nout = (int)stars.size();
    if (nout == 0) {
        *out_count = 0;
        return 0;
    }
    double* ox = (double*)std::malloc(sizeof(double) * (size_t)nout);
    double* oy = ox ? (double*)std::malloc(sizeof(double) * (size_t)nout) : NULL;
    float* ofl = oy ? (float*)std::malloc(sizeof(float) * (size_t)nout) : NULL;
    int* osa = ofl ? (int*)std::malloc(sizeof(int) * (size_t)nout) : NULL;
    float* omg = osa ? (float*)std::malloc(sizeof(float) * (size_t)nout) : NULL;
    int* ohs = omg ? (int*)std::malloc(sizeof(int) * (size_t)nout) : NULL;
    float** oex = NULL;
    int oex_bad = 0;
    if (ohs && extra_count > 0) {
        oex = (float**)std::malloc(sizeof(float*) * (size_t)extra_count);
        if (oex) {
            for (int k = 0; k < extra_count; ++k) oex[k] = NULL;
            for (int k = 0; k < extra_count; ++k) {
                oex[k] = (float*)std::malloc(sizeof(float) * (size_t)nout);
                if (!oex[k]) { oex_bad = 1; break; }
            }
        } else {
            oex_bad = 1;
        }
    }
    if (!ox || !oy || !ofl || !osa || !omg || !ohs || oex_bad ||
        (extra_count > 0 && !oex)) {
        std::free(ox);
        std::free(oy);
        std::free(ofl);
        std::free(osa);
        std::free(omg);
        std::free(ohs);
        if (oex) {
            for (int k = 0; k < extra_count; ++k) std::free(oex[k]);
            std::free(oex);
        }
        *out_x = NULL;
        *out_y = NULL;
        *out_flux = NULL;
        *out_saturated = NULL;
        *out_mag = NULL;
        *out_has_saturated = NULL;
        if (out_extras) *out_extras = NULL;
        *out_count = -1;
        return -1;
    }
    for (int i = 0; i < nout; ++i) {
        ox[i] = stars[(size_t)i].cx;
        oy[i] = stars[(size_t)i].cy;
        ofl[i] = stars[(size_t)i].flux;
        osa[i] = stars[(size_t)i].is_saturated;
        omg[i] = stars[(size_t)i].mag;
        ohs[i] = stars[(size_t)i].has_saturated;
    }
    if (extra_count > 0) {
        for (int k = 0; k < extra_count; ++k) {
            const ExtraField ef = (extra_names && extra_names[k])
                                      ? parse_extra_name(extra_names[k])
                                      : EXTRA_UNKNOWN;
            for (int i = 0; i < nout; ++i)
                oex[k][i] = get_extra_field(stars[(size_t)i], ef);
        }
        if (out_extras) *out_extras = oex;
    } else if (out_extras) {
        *out_extras = NULL;
    }
    *out_x = ox;
    *out_y = oy;
    *out_flux = ofl;
    *out_saturated = osa;
    *out_mag = omg;
    *out_has_saturated = ohs;
    *out_count = nout;
    return 0;
}

}  // namespace

// sdet_detect_impl - 星点检测核心 (模板双实例)
// T=float: uint16→float 由入口包装完成; T=double: FP64 全程不降级
template <typename T>
static int sdet_detect_impl(StarDetectorHandle handle,
                            const T *image, int width, int height,
                            double **out_x, double **out_y, float **out_flux, int **out_saturated,
                            float **out_mag, int **out_has_saturated,
                            int *out_count,
                            const char **extra_names, int extra_count, float ***out_extras)
{
    if (!handle || !image || !out_x || !out_y || !out_flux || !out_saturated ||
        !out_mag || !out_has_saturated || !out_count) return -1;
    if (width <= 0 || height <= 0) return -1;
    if (extra_count > 0 && (!extra_names || !out_extras)) return -1;
    *out_x = NULL; *out_y = NULL; *out_flux = NULL;
    *out_saturated = NULL; *out_mag = NULL; *out_has_saturated = NULL;
    *out_count = 0;
    if (out_extras) *out_extras = NULL;

    const SDetParams& params = handle->internal.params;
    const int w = width, h = height;

    // ---- 域准备: 非有限填充 + O2 平滑 + O1 统计族 ----
    SdetPrep<T> pr = sdet_prepare_field<T>(image, w, h);
    const T* src = pr.src;
    const T* smooth = pr.smooth_data;
    if (!std::isfinite(pr.thr)) return 0;   // 全非有限帧 → 空输出（rc = 0, count = 0）
    const double& bg = pr.bg;
    // 原先此处有 `const double& bgnoise = pr.bgnoise;` —— 该别名**全函数零引用**
    // （MSVC C4189）。背景噪声并未被丢弃：它在 sdet_prepare_field 经
    // pr.locthreshold = 5*pr.bgnoise / pr.thr = pr.bg + 5*pr.bgnoise 正常进入判定。
    // 删的是冗余引用别名（绑定到已有对象，无副作用），不是删噪声 ⇒ 零行为变化。
    const double& dynrange = pr.dynrange;
    const double& sat_threshold = pr.sat_threshold;
    const double& locthreshold = pr.locthreshold;
    const double& thr = pr.thr;

    // ---- O4a: 饱和岛预标记 ----
    std::vector<int> sat_label;
    std::vector<SdetSatIsland> islands;
    sdet_mark_sat_islands<T>(src, w, h, sat_threshold, &sat_label, &islands);

    // ---- O3 阈值化 + 8-连通组分 → O11 deblending 树 → O8 峰（检测段）----
    std::vector<SdetCand> cands;
    // FIX-R3: 叶内 11x11 局部极大扫描（O8 冻结语义恢复, ALG §3 :127-129;
    // 设计稿 §5.8——重写保持仅修越界读）。
    // 数学等价性论证: local_max 判定窗 = 全图 ±5（与叶归属无关）, 峰
    // （smooth > thr 的局部极大）必属某组分; 树分裂叶像素表 ∪ 树成分峰
    // 覆盖组分内全部局部极大候选（漏峰防御见 lambda 内注）; 逐像素扫描与
    // 旧 x+=5 跳步行为等价（跳步位置 = 已确认峰 ±5 窗内 → 必被窗内更高/
    // 决胜者否决, 不可能同时是局部极大）。pixel_count 统计窗（等价分析
    // :2101-2106 越界带 = pixel_count 窗钳位 + 右端 xx'∈{w−5..w−3} 带）
    // 仅旧日志面（med_pixel_count → auto_fit_radius）消费, 重写无该消费面,
    // 不重构入。平局决胜 = 左上优先（字典序先到者胜, 与旧码否决式
    // (xx'<=x&&yy'<=y)||(xx'>x&&yy'<y) 等价）。等价分析 :1866 count 门
    // （boxsize 值无权威来源）按权威链序（ALG §3 → 设计稿 §5.8）省略。
    auto leaf_local_maxima = [&](const SdetLeaf& lf, std::vector<int>* peaks) {
        // 漏峰防御: O11 argmax 重分配可把局部极大像素划出其自然叶像素表
        // （叶像素表 ⊉ 全帧峰集）→ 扫描集 = 叶像素 ∪ 树成分峰 lf.peak:
        // 先扫成分峰保证不漏峰, 叶像素遍历跳过 peak 防重复候选。
        const int lp = lf.peak;
        if (lp >= 0) {
            const int pxx = lp % w;
            const int pyy = lp / w;
            if (pyy >= 5 && pyy < h - 5 && pxx >= 5 && pxx < w - 5) {
                const double v = (double)smooth[(size_t)lp];
                if (v > thr) {
                    bool local_max = true;
                    for (int ddy = -5; ddy <= 5 && local_max; ++ddy)
                        for (int ddx = -5; ddx <= 5; ++ddx) {
                            if (!ddx && !ddy) continue;
                            const double nv = (double)smooth[(size_t)(pyy + ddy) * (size_t)w
                                                           + (size_t)(pxx + ddx)];
                            if (nv > v || (nv == v && (ddy < 0 || (ddy == 0 && ddx < 0)))) {
                                local_max = false;
                                break;
                            }
                        }
                    if (local_max) peaks->push_back(lp);
                }
            }
        }
        for (size_t pi = 0; pi < lf.pix.size(); ++pi) {
            const int p = lf.pix[pi];
            if (p == lp) continue;
            const int pxx = p % w;
            const int pyy = p / w;
            // ALG :127 冻结扫描域 [r, h-r) x [r, w-r), r = 5 — 域外不产峰
            if (pyy < 5 || pyy >= h - 5 || pxx < 5 || pxx >= w - 5) continue;
            const double v = (double)smooth[(size_t)p];
            if (v <= thr) continue;
            bool local_max = true;
            for (int ddy = -5; ddy <= 5 && local_max; ++ddy)
                for (int ddx = -5; ddx <= 5; ++ddx) {
                    if (!ddx && !ddy) continue;
                    const double nv = (double)smooth[(size_t)(pyy + ddy) * (size_t)w
                                                   + (size_t)(pxx + ddx)];
                    if (nv > v || (nv == v && (ddy < 0 || (ddy == 0 && ddx < 0)))) {
                        local_max = false;
                        break;
                    }
                }
            if (local_max) peaks->push_back(p);
        }
    };
    {
        std::vector<char> above((size_t)w * (size_t)h, 0);
        for (std::size_t i = 0; i < (size_t)w * (size_t)h; ++i)
            above[i] = ((double)smooth[i] > thr) ? 1 : 0;
        std::vector<int> label((size_t)w * (size_t)h, 0);
        std::vector<int> queue;
        for (int y0 = 0; y0 < h; ++y0) {
            for (int x0 = 0; x0 < w; ++x0) {
                const int p0 = y0 * w + x0;
                if (!above[(size_t)p0] || label[(size_t)p0]) continue;
                std::vector<int> comp;
                queue.clear();  // FIX-P128: 清空上一组分残留（queue 声明在组分循环外;
                                // 漏清空则历史像素在下方 qi 循环中无条件并入 comp,
                                // 组分累积合并 O(C²); 对照 :1345 饱和岛 BFS 风格）
                queue.push_back(p0);
                label[(size_t)p0] = 1;
                for (size_t qi = 0; qi < queue.size(); ++qi) {
                    const int p = queue[qi];
                    comp.push_back(p);
                    const int px = p % w, py = p / w;
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx) {
                            if (!dx && !dy) continue;
                            const int nx = px + dx, ny = py + dy;
                            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                            const int np2 = ny * w + nx;
                            if (above[(size_t)np2] && !label[(size_t)np2]) {
                                label[(size_t)np2] = 1;
                                queue.push_back(np2);
                            }
                        }
                }
std::sort(comp.begin(), comp.end());
                double total_flux = 0.0;
                for (size_t k = 0; k < comp.size(); ++k)
                    total_flux += (double)smooth[comp[k]] - thr;
                std::vector<SdetLeaf> leaves;
                sdet_deblend_leaf<T>(smooth, w, comp, thr, total_flux, 0, &leaves);
                // FIX-R3: 每叶做 11x11 局部极大扫描——一叶可产多候选（B&A96
                // 根不分裂 → 单星 = 常规域单叶单局部极大的特例）
                std::vector<int> leaf_peaks;
                for (size_t li = 0; li < leaves.size(); ++li) {
                    const SdetLeaf& lf = leaves[li];
                    leaf_peaks.clear();
                    leaf_local_maxima(lf, &leaf_peaks);
                    for (size_t qi = 0; qi < leaf_peaks.size(); ++qi) {
                    const int pxx = leaf_peaks[qi] % w;
                    const int pyy = leaf_peaks[qi] / w;
                    // ALG :1892-1893 行为合同: 峰 ±2 邻域越界丢弃
                    // （扫描域 r=5 已蕴含 ±2 界, 保留作行为合同锚）
                    if (pxx < 2 || pxx >= w - 2 || pyy < 2 || pyy >= h - 2) continue;

                    SdetCand c;
                    std::memset(&c, 0, sizeof(c));
                    c.ix = pxx;
                    c.iy = pyy;

                    // ---- O4b: 3x3 双条件饱和判定（原图; 中心除外）----
                    // 高像素计数 < 3 → 剔除; 饱和 ⇔ meanhigh - bg >= 0.7*dynrange
                    // 且 pixel0 - minhigh <= 0.1*dynrange。
                    int counthigh = 0;
                    double sumhigh = 0.0, minhigh = 0.0;
                    bool has_high = false;
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx) {
                            if (!dx && !dy) continue;
                            const double v = (double)src[(size_t)(pyy + dy) * (size_t)w + (size_t)(pxx + dx)];
                            if (v > thr) {
                                ++counthigh;
                                sumhigh += v;
                                if (!has_high || v < minhigh) minhigh = v;
                                has_high = true;
                            }
                        }
                    if (counthigh < 3) continue;
                    const double pixel0 = (double)src[(size_t)pyy * (size_t)w + (size_t)pxx];
                    const double meanhigh = sumhigh / (double)counthigh;
                    const bool is_sat = (meanhigh - bg >= 0.7 * dynrange) &&
                                        (pixel0 - minhigh <= 0.1 * dynrange);
                    c.mag_est = (float)meanhigh;
                    c.sat = is_sat ? 1 : 0;

                    // ---- 初值中心: 非饱和 = O5 质心; 饱和 = O6 行走等效中心 ----
                    if (!is_sat) {
                        // SPEC: §5.5 O5 一阶导过零（平滑图; |分母| < 1e-20 → 偏移 -0.5）
                        const double d1rl = (double)smooth[(size_t)pyy * (size_t)w + (size_t)pxx]
                                          - (double)smooth[(size_t)pyy * (size_t)w + (size_t)(pxx - 1)];
                        const double d1rr = (double)smooth[(size_t)pyy * (size_t)w + (size_t)(pxx + 1)]
                                          - (double)smooth[(size_t)pyy * (size_t)w + (size_t)pxx];
                        const double r0 = (std::fabs(d1rr - d1rl) < 1e-20)
                                              ? -0.5
                                              : (-0.5 - d1rl / (d1rr - d1rl));
                        const double d1cu = (double)smooth[(size_t)pyy * (size_t)w + (size_t)pxx]
                                          - (double)smooth[(size_t)(pyy - 1) * (size_t)w + (size_t)pxx];
                        const double d1cd = (double)smooth[(size_t)(pyy + 1) * (size_t)w + (size_t)pxx]
                                          - (double)smooth[(size_t)pyy * (size_t)w + (size_t)pxx];
                        const double c0 = (std::fabs(d1cd - d1cu) < 1e-20)
                                              ? -0.5
                                              : (-0.5 - d1cu / (d1cd - d1cu));
                        c.x = (double)pxx + 0.5 + r0;
                        c.y = (double)pyy + 0.5 + c0;
                    } else {
                        // SPEC: §5.6 O6 饱和中心 edge-walking（ALG :1918-1978 行为合同）:
                        // 从峰沿行/列四方向走到 pixel > sat_threshold 平台的边缘,
                        // 等效中心 = 边缘盒几何中心（(xl+xr)/2+0.5, (yu+yd)/2+0.5）。
                        int xl2 = pxx, xr2 = pxx, yu2 = pyy, yd2 = pyy;
                        while (xl2 - 1 >= 0 && (double)src[(size_t)pyy * (size_t)w + (size_t)(xl2 - 1)] > sat_threshold) --xl2;
                        while (xr2 + 1 < w && (double)src[(size_t)pyy * (size_t)w + (size_t)(xr2 + 1)] > sat_threshold) ++xr2;
                        while (yu2 - 1 >= 0 && (double)src[(size_t)(yu2 - 1) * (size_t)w + (size_t)pxx] > sat_threshold) --yu2;
                        while (yd2 + 1 < h && (double)src[(size_t)(yd2 + 1) * (size_t)w + (size_t)pxx] > sat_threshold) ++yd2;
                        c.x = 0.5 * ((double)xl2 + (double)xr2) + 0.5;
                        c.y = 0.5 * ((double)yu2 + (double)yd2) + 0.5;
                        // O4a 岛等效半径（饱和星 r 列）
                        const int lab = sat_label[(size_t)(pyy * w + pxx)];
                        if (lab > 0) c.sat_r = islands[(size_t)(lab - 1)].r;
                    }

                    // ---- O7: 二阶导零交叉（平滑图 4 方向）----
                    double drl, drr, dcu, dcd, arl, arr, acu, acd;
                    sdet_zero_cross_dir<T>(smooth, w, h, src, sat_threshold,
                                           pxx, pyy, -1, 0, bg, is_sat, &drl, &arl);
                    sdet_zero_cross_dir<T>(smooth, w, h, src, sat_threshold,
                                           pxx, pyy, +1, 0, bg, is_sat, &drr, &arr);
                    sdet_zero_cross_dir<T>(smooth, w, h, src, sat_threshold,
                                           pxx, pyy, 0, -1, bg, is_sat, &dcu, &acu);
                    sdet_zero_cross_dir<T>(smooth, w, h, src, sat_threshold,
                                           pxx, pyy, 0, +1, bg, is_sat, &dcd, &acd);
                    c.srl = drl; c.srr = drr; c.scl = dcu; c.scr = dcd;
                    c.Sr = 0.5 * (drl + drr);
                    c.Sc = 0.5 * (dcu + dcd);
                    c.Ar = SDET_SQRT_EXP1 * 0.5 * (arl + arr);
                    c.Ac = SDET_SQRT_EXP1 * 0.5 * (acu + acd);

                    // ---- O9: 拟合盒半径（s_factor 解析; 钳 [5, 200]; 钳位帧内）----
                    int R = (int)std::ceil(SDET_S_FACTOR * std::max(c.Sr, c.Sc));
                    if (R < 5) R = 5;
                    if (R > SDET_MAX_BOX_RADIUS) R = SDET_MAX_BOX_RADIUS;
                    c.R = R;
                    {
                        const int wcxi = (int)std::lround(c.x - 0.5);
                        const int wcyi = (int)std::lround(c.y - 0.5);
                        c.rx0 = wcxi - R < 0 ? 0 : wcxi - R;
                        c.ry0 = wcyi - R < 0 ? 0 : wcyi - R;
                        c.rx1 = wcxi + R + 1 > w ? w : wcxi + R + 1;
                        c.ry1 = wcyi + R + 1 > h ? h : wcyi + R + 1;
                    }
                    if (c.rx1 - c.rx0 < 1 || c.ry1 - c.ry0 < 1) continue;

                    // ---- O10: 形状预门（对称性; 分母 < 1e-20 → 比值 1e30;
                    // 饱和候选豁免——平台象限差分失真, 质量责任由 O13(4)
                    // 饱和豁免通道与 F2 平台星验收承载）----
                    if (!is_sat) {
                        const double aabs_r = std::fabs(c.Ar), aabs_c = std::fabs(c.Ac);
                        const double amin = std::min(aabs_r, aabs_c);
                        const double amax = std::max(aabs_r, aabs_c);
                        const double dA = (amin < 1e-20) ? 1e30 : (amax / amin);
                        const double drmin = std::min(drl, drr);
                        const double drmax = std::max(drl, drr);
                        const double dSr = (drmin < 1e-20) ? 1e30 : (drmax / drmin);
                        const double dcmin = std::min(dcu, dcd);
                        const double dcmax = std::max(dcu, dcd);
                        const double dSc = (dcmin < 1e-20) ? 1e30 : (dcmax / dcmin);
                        if (dA > 2.0 || dSr > 2.0 || dSc > 2.0) continue;
                        if (std::max(aabs_r, aabs_c) < locthreshold) continue;
                    }
                    // FIX-Y1: O11 候选段曼哈顿去重（ALG §2 :92-93 / §3 :138 冻结;
                    // 保守恢复旧语义——开放子项实验另行安排, 设计稿 §8-10）。
                    // matchradius = max(1, floor(0.2*R)), R = 本候选 O9 拟合盒半径;
                    // 先到先留（扫描序）。比较坐标: 非饱和 = O8 峰位 (ix, iy);
                    // 饱和 = O6 行走中心整型化 floor(x-0.5)（对齐旧码 (xr+xl)/2
                    // 整型除法语义）。可达性注记: 冻结 O8 下局部极大互距 >= 6,
                    // 而正常域 matchradius = floor(0.2*ceil(3.7172*Sr)) < 6 恒成立
                    // （Sr > 8.07 才可能 >= 6, 但 sigma_eff > 4.2 时近距双峰在
                    // 平滑图融合为单局部极大）——本门为防御性结构。
                    {
                        const int mr = (c.R > 0) ? std::max(1, (int)(0.2 * (double)c.R)) : 1;
                        const int mcx = c.sat ? (int)std::floor(c.x - 0.5) : c.ix;
                        const int mcy = c.sat ? (int)std::floor(c.y - 0.5) : c.iy;
                        bool dup = false;
                        for (std::size_t k2 = cands.size(); k2-- > 0;) {
                            const SdetCand& o = cands[k2];
                            const int ox = o.sat ? (int)std::floor(o.x - 0.5) : o.ix;
                            const int oy = o.sat ? (int)std::floor(o.y - 0.5) : o.iy;
                            const int ddx2 = mcx - ox, ddy2 = mcy - oy;
                            if ((ddx2 < 0 ? -ddx2 : ddx2) + (ddy2 < 0 ? -ddy2 : ddy2) <= mr) {
                                dup = true;
                                break;
                            }
                        }
                        if (dup) continue;
                    }
                    cands.push_back(c);
                    }  // FIX-R3: 扫描峰循环闭合
                }
            }
        }
    }

    // ---- mag_est 降序（候选阶段不截断, ALG :2170-2171）----
    std::stable_sort(cands.begin(), cands.end(),
                     [](const SdetCand& a, const SdetCand& b) { return a.mag_est > b.mag_est; });

    // ---- O12 并行 LM（OpenMP 冻结三处之三; 结果按索引写回, 位级线程无关）----
    const int ncand = (int)cands.size();
    std::vector<InternalFitResult> fit_results((size_t)ncand);
    std::vector<unsigned char> fit_ok((size_t)ncand, 0);
    #pragma omp parallel for schedule(dynamic)
    for (int ci = 0; ci < ncand; ++ci) {
        const SdetCand& c = cands[(size_t)ci];
        InternalFitResult fr;
        std::memset(&fr, 0, sizeof(fr));
        const int st = sdet_gauss_fit<T>(src, w, h, c.x, c.y,
                                         c.rx0, c.ry0, c.rx1, c.ry1, &fr,
                                         NULL, sat_threshold, 0.0, 0.0, 0.0, params.maxAxisRatio);
        fit_results[(size_t)ci] = fr;
        fit_ok[(size_t)ci] = (st == SDET_FIT_OK) ? 1 : 0;
    }

    // ---- O13 排异 + O14 孔径测光 + O15 装配（串行; 记录序 = 候选序）----
    std::vector<StarRecord> stars;
    stars.reserve((size_t)ncand);
    for (int ci = 0; ci < ncand; ++ci) {
        if (!fit_ok[(size_t)ci]) continue;   // 拟合失败丢弃（ALG §4: 不产 NaN 行）
        const InternalFitResult& f = fit_results[(size_t)ci];
        const SdetCand& c = cands[(size_t)ci];
        // FIX-R2: maxAxisRatio 门激活（ALG :2282-2284; 与 guided 路径 :2658-2665
        // 活门对称——原死块只计算 smax/smin 未判定, 系重写期漏译）。
        if (params.maxAxisRatio > 0.0f) {
            const double smax = std::max(f.sx, f.sy);
            const double smin = std::min(f.sx, f.sy);
            if (smin > 0.0 && smax / smin > (double)params.maxAxisRatio) continue;
        }
        // FIX-R1: O13 五码排异门（ALG §3 :143-148; 判式序 NEG→TOO_SMALL→
        // ROUNDNESS→RMSE→TOO_LARGE; has_saturated = O4b 判定, 与 guided 同源）。
        if (reject_star(f, c.sat != 0, c.Sr, c.Sc) != SF_OK) continue;
        StarRecord rec;
        std::memset(&rec, 0, sizeof(rec));
        rec.cx = f.cx;
        rec.cy = f.cy;
        rec.flux = (float)f.A;                        // DISP-STAR-004: flux = PSF 振幅
        rec.is_saturated = (f.A > dynrange) ? 1 : 0;  // ALG :2298
        rec.has_saturated = rec.is_saturated;         // ALG :2342（列语义未分化, DISP-STAR-004）
        rec.fwhm_x = (float)f.fwhm_x;
        rec.fwhm_y = (float)f.fwhm_y;
        rec.sx = (float)f.sx;
        rec.sy = (float)f.sy;
        rec.theta = (float)f.theta;
        rec.background = (float)f.B;
        rec.amplitude = (float)f.A;
        rec.r = (float)(c.sat ? c.sat_r : 0.0);
        rec.cand_R = (float)c.R;
        // ---- O14: 孔径测光（mag = -2.5 log10(Σ_box(pixel - B_fit)), box =
        // (2R+1)^2 用候选 R（钳 [5,200]）与候选中心; box_sum <= 0 → NaN）----
        {
            int Rb = c.R;
            if (Rb < 5) Rb = 5;
            if (Rb > SDET_MAX_BOX_RADIUS) Rb = SDET_MAX_BOX_RADIUS;
            const int bx = (int)std::lround(c.x - 0.5);
            const int by = (int)std::lround(c.y - 0.5);
            double box_sum = 0.0;
            for (int yy = by - Rb; yy <= by + Rb; ++yy) {
                if (yy < 0 || yy >= h) continue;
                for (int xx = bx - Rb; xx <= bx + Rb; ++xx) {
                    if (xx < 0 || xx >= w) continue;
                    box_sum += (double)src[(size_t)yy * (size_t)w + (size_t)xx] - f.B;
                }
            }
            rec.mag = (box_sum > 0.0) ? (float)(-2.5 * std::log10(box_sum))
                                      : (float)std::nan("");
        }
        stars.push_back(rec);
    }

    // ---- O13b 点源形状门（R-58-1; 只作用于盲检测路径, 去重/截断之前）----
    {
        const size_t n_before = stars.size();
        const int n_shape_rej = sdet_apply_psf_shape_gate<T>(stars, src, w, h, params);
        if (n_shape_rej > 0) {
            sdet_log(SDET_LOG_INFO, "SDET",
                     "O13b point-source shape gate: rejected %d of %zu stars",
                     n_shape_rej, n_before);
        }
    }

    // ---- O15 dedup/排序/截断 + 事务化输出 ----
    return sdet_emit_records(stars, (long long)params.maxStars, extra_names,
                             extra_count, out_x, out_y, out_flux, out_saturated,
                             out_mag, out_has_saturated, out_count, out_extras);
}
// ============================================================================
// sdet_detect_guided_impl - 星表引导检测（权威路径; SPEC: §5.16 O16）
// 规范: 检测定义域 = 星表位置（本帧 WCS 反投影）, 只对预测位置做质心/PSF 拟合。
// 逐位置处理（六计数守恒: 每位置恰归一类）:
//   ① 非有限 / 距边 < 2px（0-based 索引语义）/ 拟合盒越界 → n_dropped
//   ② O4b 3x3 双条件饱和判定（阈值 median + 5*bgnoise）; 高像素 < 3 → n_fit_failed
//      （语义映射: 拟合前置判据失败; 头文件 n_dropped 三类不含此情形）
//   ③ 初值: 预测位置直接作拟合初值（星表位置亚像素精度, 不再做 O5）;
//      饱和位置 → O6 行走等效中心
//   ④ O7 零交叉初值（平滑图, at 预测位最近像素）+ O9 拟合盒（钳 [5,200] 帧内）
//   ⑤ O10 对称门（非饱和; 拒 → n_rejected）
//   ⑥ O12 sdet_gauss_fit → 未收敛/非法 → n_fit_failed
//   ⑦ 质量门 maxAxisRatio / reject_star → 未过 → n_rejected
//   ⑧ O14 mag 同式 → O15 dedup/排序/截断 → n_output
// 确定性: 并行逐位置按索引写回类判, 串行汇总计数/装配/去重/排序 → 线程数无关。
// 无 2*maxStars 预截断（破坏六计数守恒, 项目定义）。
// ============================================================================
template <typename T>
static int sdet_detect_guided_impl(StarDetectorHandle handle,
                                   const T *image, int width, int height,
                                   const double *pred_x, const double *pred_y, int n_pred,
                                   double **out_x, double **out_y, float **out_flux, int **out_saturated,
                                   float **out_mag, int **out_has_saturated,
                                   int *out_count,
                                   const char **extra_names, int extra_count, float ***out_extras,
                                   SDetGuidedStats *out_stats)
{
    if (out_stats) { std::memset(out_stats, 0, sizeof(*out_stats)); out_stats->n_predicted = n_pred; }
    if (!handle || !image || !out_x || !out_y || !out_flux || !out_saturated ||
        !out_mag || !out_has_saturated || !out_count) return -1;
    if (width <= 0 || height <= 0) return -1;
    if (n_pred < 0 || (n_pred > 0 && (!pred_x || !pred_y))) return -1;
    if (extra_count > 0 && (!extra_names || !out_extras)) return -1;
    *out_x = NULL; *out_y = NULL; *out_flux = NULL;
    *out_saturated = NULL; *out_mag = NULL; *out_has_saturated = NULL;
    *out_count = 0;
    if (out_extras) *out_extras = NULL;
    if (n_pred == 0) return 0;   // G6: 空预测 → rc = 0, count = 0

    const SDetParams& params = handle->internal.params;
    const int w = width, h = height;

    SdetPrep<T> pr = sdet_prepare_field<T>(image, w, h);
    const T* src = pr.src;
    const T* smooth = pr.smooth_data;
    if (!std::isfinite(pr.thr)) return 0;
    const double& bg = pr.bg;
    // 同上：bgnoise 别名在本函数零引用（MSVC C4189）；噪声本身经 pr.thr 正常消费。
    const double& dynrange = pr.dynrange;
    const double& sat_threshold = pr.sat_threshold;
    const double& thr = pr.thr;

    std::vector<int> sat_label;
    std::vector<SdetSatIsland> islands;
    sdet_mark_sat_islands<T>(src, w, h, sat_threshold, &sat_label, &islands);

    // 类判枚举（verdict 数组 + 串行汇总 → 位级确定）
    enum { SDET_GD_DROP = 0, SDET_GD_FITFAIL = 1, SDET_GD_REJECT = 2, SDET_GD_OK = 3 };
    std::vector<unsigned char> verdict((size_t)n_pred, SDET_GD_DROP);
    std::vector<InternalFitResult> g_fits((size_t)n_pred);
    std::vector<SdetCand> g_cands((size_t)n_pred);

    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < n_pred; ++i) {
        const double px = pred_x[i], py = pred_y[i];
        // ① 非有限预测 → n_dropped（verdict 初值即 DROP）
        if (!std::isfinite(px) || !std::isfinite(py)) continue;
        // ① 距边 < 2 px（0-based 索引语义; 含出帧）→ n_dropped
        const double marg = std::min(std::min(px, (double)(w - 1) - px),
                                     std::min(py, (double)(h - 1) - py));
        if (!(marg >= 2.0)) continue;
        const int ix = (int)std::lround(px - 0.5);
        const int iy = (int)std::lround(py - 0.5);
        if (ix < 1 || ix >= w - 1 || iy < 1 || iy >= h - 1) continue;

        SdetCand c;
        std::memset(&c, 0, sizeof(c));
        c.ix = ix;
        c.iy = iy;

        // ② O4b 3x3 双条件（原图; 中心除外）; 高像素 < 3 → n_fit_failed
        int counthigh = 0;
        double sumhigh = 0.0, minhigh = 0.0;
        bool has_high = false;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                if (!dx && !dy) continue;
                const double v = (double)src[(size_t)(iy + dy) * (size_t)w + (size_t)(ix + dx)];
                if (v > thr) {
                    ++counthigh;
                    sumhigh += v;
                    if (!has_high || v < minhigh) minhigh = v;
                    has_high = true;
                }
            }
        if (counthigh < 3) { verdict[(size_t)i] = SDET_GD_FITFAIL; continue; }
        const double pixel0 = (double)src[(size_t)iy * (size_t)w + (size_t)ix];
        const double meanhigh = sumhigh / (double)counthigh;
        const bool is_sat = (meanhigh - bg >= 0.7 * dynrange) &&
                            (pixel0 - minhigh <= 0.1 * dynrange);
        c.mag_est = (float)meanhigh;
        c.sat = is_sat ? 1 : 0;

        // ③ 初值: 预测位置直接作拟合初值; 饱和 → O6 行走等效中心
        if (!is_sat) {
            c.x = px;
            c.y = py;
        } else {
            // ALG :1918-1978（同主链 O6）: 峰四方向 edge-walking 至 pixel >
            // sat_threshold 平台边缘, 等效中心 = 边缘盒几何中心。
            int xl3 = ix, xr3 = ix, yu3 = iy, yd3 = iy;
            while (xl3 - 1 >= 0 && (double)src[(size_t)iy * (size_t)w + (size_t)(xl3 - 1)] > sat_threshold) --xl3;
            while (xr3 + 1 < w && (double)src[(size_t)iy * (size_t)w + (size_t)(xr3 + 1)] > sat_threshold) ++xr3;
            while (yu3 - 1 >= 0 && (double)src[(size_t)(yu3 - 1) * (size_t)w + (size_t)ix] > sat_threshold) --yu3;
            while (yd3 + 1 < h && (double)src[(size_t)(yd3 + 1) * (size_t)w + (size_t)ix] > sat_threshold) ++yd3;
            c.x = 0.5 * ((double)xl3 + (double)xr3) + 0.5;
            c.y = 0.5 * ((double)yu3 + (double)yd3) + 0.5;
            const int lab = sat_label[(size_t)(iy * w + ix)];
            if (lab > 0) c.sat_r = islands[(size_t)(lab - 1)].r;
        }

        // ④ O7 零交叉初值（平滑图 4 方向, at (ix,iy)）
        double drl, drr, dcu, dcd, arl, arr, acu, acd;
        sdet_zero_cross_dir<T>(smooth, w, h, src, sat_threshold,
                               ix, iy, -1, 0, bg, is_sat, &drl, &arl);
        sdet_zero_cross_dir<T>(smooth, w, h, src, sat_threshold,
                               ix, iy, +1, 0, bg, is_sat, &drr, &arr);
        sdet_zero_cross_dir<T>(smooth, w, h, src, sat_threshold,
                               ix, iy, 0, -1, bg, is_sat, &dcu, &acu);
        sdet_zero_cross_dir<T>(smooth, w, h, src, sat_threshold,
                               ix, iy, 0, +1, bg, is_sat, &dcd, &acd);
        c.srl = drl; c.srr = drr; c.scl = dcu; c.scr = dcd;
        c.Sr = 0.5 * (drl + drr);
        c.Sc = 0.5 * (dcu + dcd);
        c.Ar = SDET_SQRT_EXP1 * 0.5 * (arl + arr);
        c.Ac = SDET_SQRT_EXP1 * 0.5 * (acu + acd);

        // ④ O9 拟合盒（钳 [5,200]; 钳位帧内; 盒空 → n_dropped）
        int R = (int)std::ceil(SDET_S_FACTOR * std::max(c.Sr, c.Sc));
        if (R < 5) R = 5;
        if (R > SDET_MAX_BOX_RADIUS) R = SDET_MAX_BOX_RADIUS;
        c.R = R;
        {
            const int wcxi = (int)std::lround(c.x - 0.5);
            const int wcyi = (int)std::lround(c.y - 0.5);
            c.rx0 = wcxi - R < 0 ? 0 : wcxi - R;
            c.ry0 = wcyi - R < 0 ? 0 : wcyi - R;
            c.rx1 = wcxi + R + 1 > w ? w : wcxi + R + 1;
            c.ry1 = wcyi + R + 1 > h ? h : wcyi + R + 1;
        }
        if (c.rx1 - c.rx0 < 1 || c.ry1 - c.ry0 < 1) continue;   // n_dropped

        // ⑤ O10 对称门（非饱和; 拒 → n_rejected）
        if (!is_sat) {
            const double aabs_r = std::fabs(c.Ar), aabs_c = std::fabs(c.Ac);
            const double amin = std::min(aabs_r, aabs_c);
            const double amax = std::max(aabs_r, aabs_c);
            const double dA = (amin < 1e-20) ? 1e30 : (amax / amin);
            const double drmin = std::min(drl, drr);
            const double drmax = std::max(drl, drr);
            const double dSr = (drmin < 1e-20) ? 1e30 : (drmax / drmin);
            const double dcmin = std::min(dcu, dcd);
            const double dcmax = std::max(dcu, dcd);
            const double dSc = (dcmin < 1e-20) ? 1e30 : (dcmax / dcmin);
            if (dA > 2.0 || dSr > 2.0 || dSc > 2.0) {
                verdict[(size_t)i] = SDET_GD_REJECT;
                continue;
            }
            if (std::max(aabs_r, aabs_c) < pr.locthreshold) {
                verdict[(size_t)i] = SDET_GD_REJECT;
                continue;
            }
        }

        // ⑥ O12 拟合 → 未收敛/非法 → n_fit_failed
        InternalFitResult fr;
        std::memset(&fr, 0, sizeof(fr));
        const int st = sdet_gauss_fit<T>(src, w, h, c.x, c.y,
                                         c.rx0, c.ry0, c.rx1, c.ry1, &fr,
                                         NULL, sat_threshold, 0.0, 0.0, 0.0, params.maxAxisRatio);
        if (st != SDET_FIT_OK) { verdict[(size_t)i] = SDET_GD_FITFAIL; continue; }

        // ⑦ 质量门 → 未过 → n_rejected
        if (params.maxAxisRatio > 0.0f) {
            const double smax = std::max(fr.sx, fr.sy);
            const double smin = std::min(fr.sx, fr.sy);
            if (smin > 0.0 && smax / smin > (double)params.maxAxisRatio) {
                verdict[(size_t)i] = SDET_GD_REJECT;
                continue;
            }
        }
        if (reject_star(fr, is_sat, c.Sr, c.Sc) != SF_OK) {
            verdict[(size_t)i] = SDET_GD_REJECT;
            continue;
        }
        verdict[(size_t)i] = SDET_GD_OK;
        g_fits[(size_t)i] = fr;
        g_cands[(size_t)i] = c;
    }

    // 串行汇总六计数（守恒: dropped + fit_failed + rejected + fit_ok == n_predicted）
    if (out_stats) {
        for (int i = 0; i < n_pred; ++i) {
            switch (verdict[(size_t)i]) {
                case SDET_GD_DROP:     ++out_stats->n_dropped; break;
                case SDET_GD_FITFAIL:  ++out_stats->n_fit_failed; break;
                case SDET_GD_REJECT:   ++out_stats->n_rejected; break;
                default:               ++out_stats->n_fit_ok; break;
            }
        }
    }

    // ⑧ 装配（记录序 = 预测序）→ O14 → O15
    std::vector<StarRecord> stars;
    stars.reserve((size_t)n_pred);
    for (int i = 0; i < n_pred; ++i) {
        if (verdict[(size_t)i] != SDET_GD_OK) continue;
        const InternalFitResult& f = g_fits[(size_t)i];
        const SdetCand& c = g_cands[(size_t)i];
        StarRecord rec;
        std::memset(&rec, 0, sizeof(rec));
        rec.cx = f.cx;
        rec.cy = f.cy;
        rec.flux = (float)f.A;
        rec.is_saturated = (f.A > dynrange) ? 1 : 0;
        rec.has_saturated = rec.is_saturated;
        rec.fwhm_x = (float)f.fwhm_x;
        rec.fwhm_y = (float)f.fwhm_y;
        rec.sx = (float)f.sx;
        rec.sy = (float)f.sy;
        rec.theta = (float)f.theta;
        rec.background = (float)f.B;
        rec.amplitude = (float)f.A;
        rec.r = (float)(c.sat ? c.sat_r : 0.0);
        rec.cand_R = (float)c.R;
        {
            int Rb = c.R;
            if (Rb < 5) Rb = 5;
            if (Rb > SDET_MAX_BOX_RADIUS) Rb = SDET_MAX_BOX_RADIUS;
            const int bx = (int)std::lround(c.x - 0.5);
            const int by = (int)std::lround(c.y - 0.5);
            double box_sum = 0.0;
            for (int yy = by - Rb; yy <= by + Rb; ++yy) {
                if (yy < 0 || yy >= h) continue;
                for (int xx = bx - Rb; xx <= bx + Rb; ++xx) {
                    if (xx < 0 || xx >= w) continue;
                    box_sum += (double)src[(size_t)yy * (size_t)w + (size_t)xx] - f.B;
                }
            }
            rec.mag = (box_sum > 0.0) ? (float)(-2.5 * std::log10(box_sum))
                                      : (float)std::nan("");
        }
        stars.push_back(rec);
    }

    const int rc = sdet_emit_records(stars, (long long)params.maxStars, extra_names,
                                     extra_count, out_x, out_y, out_flux,
                                     out_saturated, out_mag, out_has_saturated,
                                     out_count, out_extras);
    if (out_stats && out_count)
        out_stats->n_output = (rc == 0) ? *out_count : 0;
    return rc;
}

SDET_EXPORT int sdet_detect_guided_ex_f64(StarDetectorHandle handle,
                                          const double *image, int width, int height,
                                          const double *pred_x, const double *pred_y,
                                          int n_pred,
                                          double **out_x, double **out_y, float **out_flux,
                                          int **out_saturated, float **out_mag,
                                          int **out_has_saturated, int *out_count,
                                          const char **extra_names, int extra_count,
                                          float ***out_extras,
                                          SDetGuidedStats *out_stats)
{
    if (out_stats) std::memset(out_stats, 0, sizeof(*out_stats));
    if (!handle || !image || width <= 0 || height <= 0) return -1;
    return sdet_detect_guided_impl<double>(handle, image, width, height,
                                           pred_x, pred_y, n_pred,
                                           out_x, out_y, out_flux, out_saturated,
                                           out_mag, out_has_saturated, out_count,
                                           extra_names, extra_count, out_extras, out_stats);
}

// ============================================================================
// sdet_detect_ex - FP32 入口 (uint16 原始图像, 兼容旧 ABI)
// 内部 uint16→float32 转换后调用 sdet_detect_impl<float> (行为与旧版逐位一致)
// ============================================================================
SDET_EXPORT int sdet_detect_ex(StarDetectorHandle handle,
                               const uint16_t *image, int width, int height,
                               double **out_x, double **out_y, float **out_flux, int **out_saturated,
                               float **out_mag, int **out_has_saturated,
                               int *out_count,
                               const char **extra_names, int extra_count, float ***out_extras)
{
    if (!handle || !image || width <= 0 || height <= 0) return -1;
    size_t n = (size_t)width * height;
    std::vector<float> fimg(n);
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < (int)n; i++) {
        fimg[i] = static_cast<float>(image[i]);
    }
    return sdet_detect_impl<float>(handle, fimg.data(), width, height,
                                   out_x, out_y, out_flux, out_saturated,
                                   out_mag, out_has_saturated, out_count,
                                   extra_names, extra_count, out_extras);
}

// ============================================================================
// sdet_detect_ex_f64 - FP64 入口 (double 校准图像)
// 全程 double 检测, 不降级 float32
// 输出接口与 sdet_detect_ex 一致 (out_flux/mag float, 下游协议兼容)
// ============================================================================
SDET_EXPORT int sdet_detect_ex_f64(StarDetectorHandle handle,
                                   const double *image, int width, int height,
                                   double **out_x, double **out_y, float **out_flux, int **out_saturated,
                                   float **out_mag, int **out_has_saturated,
                                   int *out_count,
                                   const char **extra_names, int extra_count, float ***out_extras)
{
    if (!handle || !image || width <= 0 || height <= 0) return -1;
    return sdet_detect_impl<double>(handle, image, width, height,
                                    out_x, out_y, out_flux, out_saturated,
                                    out_mag, out_has_saturated, out_count,
                                    extra_names, extra_count, out_extras);
}

SDET_EXPORT void sdet_free_detect_ex(double *x, double *y, float *flux, int *saturated,
                                       float *mag, int *has_saturated,
                                       float **extras, int extra_count)
{
    free(x);
    free(y);
    free(flux);
    free(saturated);
    free(mag);
    free(has_saturated);
    if (extras && extra_count > 0) {
        for (int i = 0; i < extra_count; i++) {
            free(extras[i]);
        }
        free(extras);
    }
}
