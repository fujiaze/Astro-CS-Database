/* Moffat4/PSF 拟合数值契约（不改算法，仅文档锚点；锚点按符号名给，不写行号）：
 * SCI-PSF-001 / ALG-STAR-PSF-*：I=B+A/(1+Q)⁴；Q/dx·dy 定见 STAR_PSF_ALGORITHMS.md 伪代码与 docs/science/PSF.md；
 * 守卫：gauss_solve_buf pivot 1e-30（数值奇异）、moffat4_residual 的 sx/sy≤0 与 Q<0 哨兵 1e10、
 *  lm_solve 求导步长 h=max(|x|·1e-6,1e-8)、moffat4_fit_tmpl_core 的 sx/sy 下界 0.3、
 *  fwhm>rect 拒绝、背景比 0.5（|B−bkg0|/max(bkg0,0.01)）；
 *  对照表见 docs/science/PSF.md / docs/science/algorithms/STAR_PSF_ALGORITHMS.md。
 */
#include "dpsf_psf.h"
#include "dpsf_log.h"
#include "dpsf_image.h"
#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>
#include <cstdlib>
#include <omp.h>

// ════════════════════════════════════════════════════════════════════════════
// PSF-DIAG-001 — opt-in 逐候选拟合诊断面（编译期 DPSF_FIT_DIAG + 运行期
//                DPSF_DIAG_PATH）
// ----------------------------------------------------------------------------
// 依据: AGENTS.md §3「重计算必须先把测量落盘」/ ENGINEERING_SPEC.md §7（运行
//       产物区登记）。用途: 把批拟合的**真实工作量**逐候选落盘（候选序 / 矩形
//       尺寸 / 逐候选耗时 / 状态 / 局部背景与峰值 / LM 迭代数 / 退化阶段），
//       供节点级门（p1stardet_node_gate）900 s 级超时做定量定位。
// 边界（先例 = lib/algorithms/star_detection/src/sdet_api.cpp 的 #ifdef
//       SDET_TESTING 审计钩子: 「生产构建不含此定义, ABI/行为零影响」）:
//   * 生产 target acsd_p1_dpsf **不定义** DPSF_FIT_DIAG ⇒ 本块整段不编入,
//     生产二进制的数值、状态码、时序与判据逐位不变;
//   * 编入后仍要运行期 DPSF_DIAG_PATH 非空才记录, 否则每次拟合只多一次
//     cached 环境查询;
//   * 只**读**拟合内部量, 不改任何判据、分支、返回值与浮点运算顺序。
// ════════════════════════════════════════════════════════════════════════════
// 拟合退化阶段编码（= moffat4_fit_tmpl_core 的返回点，便于按阶段归因）。
// 常量在两种编译形态下都存在（诊断面关闭时只是不被读取）。
enum DpsfDiagStage {
    DPSF_STAGE_OK            = 0,   // 过验证链一~三（status 由 lm_status 决定）
    DPSF_STAGE_RECT_TOO_SMALL= 1,
    DPSF_STAGE_RECT_OOB      = 2,
    DPSF_STAGE_ALL_NONFINITE = 3,
    DPSF_STAGE_AMPLITUDE_LE0 = 4,   // A0 = max - bkg0 <= 0
    DPSF_STAGE_INVALID_PARAMS= 5,   // 非有限 / A<=0 / sx<=0.3 / sy<=0.3
    DPSF_STAGE_FWHM_GT_RECT  = 6,
    DPSF_STAGE_BKG_CONSTRAINT= 7,
    DPSF_STAGE_ITER_LIMIT    = 8    // 过验证链但 LM 未收敛（ITERATION_LIMIT）
};

#ifdef DPSF_FIT_DIAG
#include "aio_atomic_file.h"

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>

namespace {

// 线程局部暂存: 核心函数写入, 包装层读取（thread_local ⇒ OpenMP 下无竞争）
struct DpsfDiagScratch {
    int    rw = 0, rh = 0;
    int    n_nonfinite = 0;
    double bkg0 = 0.0;
    double a0 = 0.0;
    double mad_lh = 0.0;
    double max_val = 0.0;
    int    lm_iter = 0;
    int    stall_max = 0;   // PSF-PERF-001: 最长"无进展"连续迭代数
    int    stage = DPSF_STAGE_OK;
};
thread_local DpsfDiagScratch t_dpsf_diag;

struct DpsfDiagRec {
    long long seq = 0;
    double cx = 0.0, cy = 0.0;
    int    rw = 0, rh = 0;
    int    stage = 0, lm_status = 0;
    int    lm_iter = 0;
    int    stall_max = 0;
    int    n_nonfinite = 0;
    double bkg0 = 0.0, a0 = 0.0, mad_lh = 0.0, max_val = 0.0;
    double B = 0.0, A = 0.0, x0 = 0.0, y0 = 0.0, sx = 0.0, sy = 0.0, theta = 0.0;
    double fwhm_x = 0.0, fwhm_y = 0.0, mad = 0.0, flux = 0.0;
    double ms = 0.0;
};

// 注意（故意的泄漏，不是疏忽）: 三个单例都用 new 且**永不 delete**。
// 原因: 落盘发生在 atexit（进程退出）阶段，而函数局部 static 的析构顺序与
// 构造顺序相反——互斥量与记录数组若作为普通 static，会先于 atexit 处理器被
// 析构，flush 就会读到已析构对象（实测表现: 段错误或静默不落盘）。
// 让它们活到进程尾声是 exit-time flush 的标准做法。
std::mutex& dpsf_diag_mutex() { static std::mutex* m = new std::mutex(); return *m; }
std::vector<DpsfDiagRec>& dpsf_diag_recs() {
    static std::vector<DpsfDiagRec>* v = new std::vector<DpsfDiagRec>();
    return *v;
}

const std::string& dpsf_diag_path() {
    static const std::string* p = new std::string([] {
        const char* e = std::getenv("DPSF_DIAG_PATH");
        return std::string(e ? e : "");
    }());
    return *p;
}

void dpsf_diag_flush() {
    const std::string& path = dpsf_diag_path();
    if (path.empty()) return;
    std::string out;
    {
        std::lock_guard<std::mutex> lock(dpsf_diag_mutex());
        auto& recs = dpsf_diag_recs();
        out.reserve(recs.size() * 192 + 256);
        out += "seq,cx,cy,rw,rh,stage,lm_status,lm_iter,stall_max,n_nonfinite,bkg0,a0,mad_lh,max_val,"
               "B,A,x0,y0,sx,sy,theta,fwhm_x,fwhm_y,mad,flux,ms\n";
        char buf[512];
        for (const DpsfDiagRec& r : recs) {
            const int n = std::snprintf(buf, sizeof(buf),
                "%lld,%.6f,%.6f,%d,%d,%d,%d,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,"
                "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                r.seq, r.cx, r.cy, r.rw, r.rh, r.stage, r.lm_status, r.lm_iter, r.stall_max, r.n_nonfinite,
                r.bkg0, r.a0, r.mad_lh, r.max_val,
                r.B, r.A, r.x0, r.y0, r.sx, r.sy, r.theta, r.fwhm_x, r.fwhm_y, r.mad, r.flux, r.ms);
            if (n > 0) out.append(buf, static_cast<size_t>(n));
        }
        recs.clear();
        recs.shrink_to_fit();
    }
    std::string err;
    aio_atomic::AppendSink* s = aio_atomic::write_open_trunc(path, &err);
    if (!s) {
        std::fprintf(stderr, "[DPSF-DIAG-001] flush open failed: %s (%s)\n",
                     path.c_str(), err.c_str());
        return;
    }
    const int wrc = aio_atomic::append_write_str(s, out);
    if (wrc != 0) std::fprintf(stderr, "[DPSF-DIAG-001] flush write rc=%d\n", wrc);
    (void)aio_atomic::append_close(s);
}

void dpsf_diag_record(const DpsfDiagRec& r) {
    static const bool registered = [] {
        std::atexit(dpsf_diag_flush);
        return true;
    }();
    (void)registered;
    std::lock_guard<std::mutex> lock(dpsf_diag_mutex());
    dpsf_diag_recs().push_back(r);
}

}  // namespace

// DPSF_DIAG_STAGE(阶段, 返回值): 记阶段但**返回真实状态码**。
// 注意: 曾经写成只返回阶段编码 —— 那会改掉 moffat4_fit/fit_d 的返回契约
// （例如 5/6/7 这类不存在的状态码），是"零影响插桩"的典型陷阱。见报告 §6。
#define DPSF_DIAG_ON()            (!dpsf_diag_path().empty())
#define DPSF_DIAG_SET(field, val) (t_dpsf_diag.field = (val))
#define DPSF_DIAG_STAGE(st, ret)  (t_dpsf_diag.stage = (st), (ret))
#define DPSF_DIAG_ITER(i)         (t_dpsf_diag.lm_iter = (i))
#else
#define DPSF_DIAG_ON()            (false)
#define DPSF_DIAG_SET(field, val) ((void)0)
#define DPSF_DIAG_STAGE(st, ret)  (ret)
#define DPSF_DIAG_ITER(i)         ((void)0)
#endif  // DPSF_FIT_DIAG

// Moffat4 (beta=4) FWHM 因子。sigma 约定(冻结, PSF.md §16): sigma = 模型参数
// Q = 0.5*r^2/sigma^2 中的 sigma, 等于 rms 半径 sqrt(<r^2>) = alpha/sqrt(2);
// 标准 Moffat: M = A/(1 + r^2/alpha^2)^beta, FWHM = 2*alpha*sqrt(2^(1/beta)-1)
// 由 Q = r^2/(2*sigma^2) = r^2/alpha^2 得 alpha = sqrt(2)*sigma
// 故 FWHM = 2*sqrt(2)*sigma*sqrt(2^(1/4)-1) ≈ 1.2303077*sigma (他域口径 sigma_g=alpha/2 -> 1.7399178, 禁用)
static const double MOFFAT4_FWHM_FACTOR = 1.230310;
static const int NPARAMS = 7;

struct SamplePixel {
    double dx;
    double dy;
    double val;
};

// PSF-BUF-001 (P2 性能, 分配策略零语义变化): 增补调用方自带缓冲的变体,
// 供 lm_solve 在迭代循环外一次性预分配 (旧实现每迭代 1 次堆分配;
// 145,884 星 × ≤200 迭代 ⇒ 最多 2,900 万次 malloc/free)。
static bool gauss_solve_buf(int n, const double* A, const double* b, double* x, double* aug) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++)
            aug[i * (n + 1) + j] = A[i * n + j];
        aug[i * (n + 1) + n] = b[i];
    }
    for (int col = 0; col < n; col++) {
        int max_row = col;
        double max_val = std::abs(aug[col * (n + 1) + col]);
        for (int row = col + 1; row < n; row++) {
            double v = std::abs(aug[row * (n + 1) + col]);
            if (v > max_val) {
                max_val = v;
                max_row = row;
            }
        }
        if (max_val < 1e-30) return false;
        if (max_row != col) {
            for (int j = col; j <= n; j++)
                std::swap(aug[col * (n + 1) + j], aug[max_row * (n + 1) + j]);
        }
        double pivot = aug[col * (n + 1) + col];
        for (int row = col + 1; row < n; row++) {
            double factor = aug[row * (n + 1) + col] / pivot;
            for (int j = col; j <= n; j++)
                aug[row * (n + 1) + j] -= factor * aug[col * (n + 1) + j];
        }
    }
    for (int i = n - 1; i >= 0; i--) {
        x[i] = aug[i * (n + 1) + n];
        for (int j = i + 1; j < n; j++)
            x[i] -= aug[i * (n + 1) + j] * x[j];
        x[i] /= aug[i * (n + 1) + i];
    }
    return true;
}

// 兼容入口 (旧签名, 单次调用自带缓冲): CodeQL #1 int 乘法提升到 size_t 再分配,
// 避免大矩阵尺寸溢出。lm_solve 走 gauss_solve_buf 预分配路径。
// [[maybe_unused]]: P2 后唯一调用方 lm_solve 改走预分配变体; 本入口保留为
// 文档锚点 (STAR_PSF_ALGORITHMS.md:174 / dynamic_psf/README.md:91,137 引用
// "gauss_solve 奇异 → λ×10 重试") 与后续单次调用入口, 不参与热路径。
[[maybe_unused]] static bool gauss_solve(int n, const double* A, const double* b, double* x) {
    std::vector<double> aug((std::size_t)n * (std::size_t)(n + 1));
    return gauss_solve_buf(n, A, b, x, aug.data());
}

static void moffat4_residual(double* params, int m, void* userdata, double* fvec) {
    const SamplePixel* samples = static_cast<const SamplePixel*>(userdata);
    double B = params[0], A = params[1], x0 = params[2], y0 = params[3];
    double sx = params[4], sy = params[5], theta = params[6];

    if (sx <= 0 || sy <= 0) {
        for (int i = 0; i < m; i++) fvec[i] = 1e10;
        return;
    }

    double cos_t = std::cos(theta), sin_t = std::sin(theta);
    double cos2 = cos_t * cos_t, sin2 = sin_t * sin_t;
    double sin2t = std::sin(2.0 * theta);
    double inv_sx2 = 1.0 / (2.0 * sx * sx);
    double inv_sy2 = 1.0 / (2.0 * sy * sy);
    double p1 = cos2 * inv_sx2 + sin2 * inv_sy2;
    double p2 = sin2t / (4.0 * sx * sx) - sin2t / (4.0 * sy * sy);
    double p3 = sin2 * inv_sx2 + cos2 * inv_sy2;

    for (int i = 0; i < m; i++) {
        double ddx = samples[i].dx - x0;
        double ddy = samples[i].dy - y0;
        double Q = p1 * ddx * ddx + 2.0 * p2 * ddx * ddy + p3 * ddy * ddy;
        if (Q < 0) {
            fvec[i] = 1e10;
            continue;
        }
        // PSF-POW-001 (P2 性能): (1+Q)^4 用 (t*t)*(t*t), t=1+Q 展开;
        // 公式与容差零改动, 仅末位 ulp 可能与 std::pow 不同 (REPORT.md §2 量化)。
        const double t = 1.0 + Q;
        const double t2 = t * t;
        double model = B + A / (t2 * t2);
        fvec[i] = samples[i].val - model;
    }
}

// ── PSF-PERF-001（DISP-PSF-003 整改之一）: 批路径的"无进展"提前退出 ───────────
// 动机（实测 run/FINAL-07/logs/PSF-DIAG-01）: 96k 盲候选中 36.86% 把 max_iter=200
// 耗满后被丢弃，占全部拟合 CPU 时间 56%（908/1613 core·s），而它们的产物是 NaN。
// 语义: stall_iters>0 时，若连续 stall_iters 次迭代都没把代价压到
//       best_cost*(1-kDpsfStallRel) 以下，判定无进展并提前结束，**返回码与
//       200 次耗尽相同 = DPSF_FIT_ITERATION_LIMIT(3)**。
// 等价性: §11.2 冻结语义——批接口对 status!=OK 一律写 NaN、不计 valid、out_status
//       置 FIT_FAILED ⇒ 批产物只取决于"是否 status==0"。本退出只可能把"本就会返回 3
//       的星提前结束"，且从不触碰 status==0 的星（单星路径 stall_iters=0，逐位不变）。
static constexpr double kDpsfStallRel = 1e-6;        // 相对代价进展阈值
// ── 批路径无进展提前退出的阈值。默认 **0 = 关闭**（判定: 不可采纳）────────────
// 结论（证据 run/FINAL-07/logs/PSF-DIAG-01/，报告 §5.4）: 真实 4096² 帧的 95,997 个
// 盲候选中，7,621 个 status=0 的"真 OK"拟合里有 30.78% 需要 ≥60 次迭代才收敛
// （lm_iter p50=33、p90=125、max=199）⇒ 任何 <200 的提前退出都会把它们从 status 0
// 改成 3，从而改变批产物（n_valid / out_status 数组）。
// p1psf_stall_equiv 的注入相（-DDPSF_BATCH_STALL_ITERS=2）实测必红，非恒真。
// 因此本机制在生产路径**关闭**；保留仅为 §8 变更流程评估用，生产 target
// acsd_p1_dpsf 与 DPSF_FIT_DIAG 均不定义该宏。关闭时全部相关分支不参与
// 任何算术语义 ⇒ 与整改前逐位相同（实测证据见报告 §5.5）。
#ifndef DPSF_BATCH_STALL_ITERS
#define DPSF_BATCH_STALL_ITERS 0
#endif
static constexpr int kDpsfBatchStallIters = DPSF_BATCH_STALL_ITERS;

static int lm_solve(int m, int n, double* x, void* userdata,
                    void (*residual_func)(double*, int, void*, double*),
                    double tol, int max_iter, int stall_iters = 0,
                    int* out_stall_max = nullptr) {
    std::vector<double> fvec(m), fvec_new(m);
    std::vector<double> J((std::size_t)m * (std::size_t)n);
    std::vector<double> JtJ((std::size_t)n * (std::size_t)n), Jtf(n),
        delta(n), x_new(n);
    // PSF-BUF-001 (P2 性能, 分配策略零语义变化): A / rhs / gauss_solve 的 aug
    // 由"每迭代 3 次堆分配"改为循环外一次性预分配 (值语义逐位不变:
    // A 仍由 JtJ 逐元素拷贝后再加 lambda, rhs 仍为 -Jtf)。
    std::vector<double> A((std::size_t)n * (std::size_t)n), rhs(n),
        aug((std::size_t)n * (std::size_t)(n + 1));

    double lambda = 1e-3;

    residual_func(x, m, userdata, fvec.data());
    double cost = 0;
    for (int i = 0; i < m; i++) cost += fvec[i] * fvec[i];

    // PSF-PERF-001: 无进展检测状态（stall_iters<=0 时全为惰性变量）
    double best_cost = cost;
    int stall = 0, stall_max = 0, iters_used = max_iter;
    bool aborted = false;

    for (int iter = 0; iter < max_iter; iter++) {
        for (int j = 0; j < n; j++) {
            double h = std::max(std::abs(x[j]) * 1e-6, 1e-8);
            double xj_orig = x[j];
            x[j] = xj_orig + h;
            residual_func(x, m, userdata, fvec_new.data());
            x[j] = xj_orig;
            for (int i = 0; i < m; i++)
                J[i * n + j] = (fvec_new[i] - fvec[i]) / h;
        }

        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++) {
                double sum = 0;
                for (int k = 0; k < m; k++)
                    sum += J[k * n + i] * J[k * n + j];
                JtJ[i * n + j] = sum;
            }

        for (int i = 0; i < n; i++) {
            double sum = 0;
            for (int k = 0; k < m; k++)
                sum += J[k * n + i] * fvec[k];
            Jtf[i] = sum;
        }

        std::copy(JtJ.begin(), JtJ.end(), A.begin());
        for (int i = 0; i < n; i++) A[i * n + i] += lambda;

        for (int i = 0; i < n; i++) rhs[i] = -Jtf[i];

        if (!gauss_solve_buf(n, A.data(), rhs.data(), delta.data(), aug.data())) {
            lambda *= 10.0;
            // 奇异矩阵连发: λ 只增 ⇒ 步长趋 0 会先被收敛判据接住；长连发说明
            // JᵀJ 含 NaN/Inf，永远不可能收敛 ⇒ 可安全提前结束。
            if (stall_iters > 0 && ++stall >= stall_iters) {
                aborted = true;
                iters_used = iter + 1;
                break;
            }
            if (stall > stall_max) stall_max = stall;
            continue;
        }

        double norm_delta = 0, norm_x = 0;
        for (int i = 0; i < n; i++) {
            norm_delta += delta[i] * delta[i];
            norm_x += x[i] * x[i];
        }
        norm_delta = std::sqrt(norm_delta);
        norm_x = std::sqrt(norm_x);

        if (norm_delta < tol * (norm_x + 1e-30)) {
            dpsf_log(LOG_DEBUG, "DPSF", "LM converged at iter %d, cost=%.6f", iter, cost);
            DPSF_DIAG_ITER(iter);
            return DPSF_FIT_OK;
        }

        for (int i = 0; i < n; i++) x_new[i] = x[i] + delta[i];
        residual_func(x_new.data(), m, userdata, fvec_new.data());
        double cost_new = 0;
        for (int i = 0; i < m; i++) cost_new += fvec_new[i] * fvec_new[i];

        if (cost_new < cost) {
            for (int i = 0; i < n; i++) x[i] = x_new[i];
            if (x[4] < 0.3) x[4] = 0.3;
            if (x[5] < 0.3) x[5] = 0.3;
            if (x[1] < 0.0) x[1] = 0.0;
            for (int i = 0; i < m; i++) fvec[i] = fvec_new[i];
            cost = cost_new;
            lambda *= 0.1;
        } else {
            lambda *= 10.0;
        }

        // PSF-PERF-001: 无进展计数。best_cost<=0（残差全 0）时任何 cost 都不算进展，
        // 但那时 J=0 ⇒ delta=0 ⇒ 上面的收敛判据必然先行返回 OK，故不构成风险。
        if (stall_iters > 0) {
            if (cost < best_cost * (1.0 - kDpsfStallRel)) {
                best_cost = cost;
                stall = 0;
            } else if (++stall >= stall_iters) {
                aborted = true;
                iters_used = iter + 1;
                break;
            }
            if (stall > stall_max) stall_max = stall;
        }
    }

    if (out_stall_max) *out_stall_max = stall_max;
    dpsf_log(LOG_DEBUG, "DPSF", "LM hit iteration limit%s, cost=%.6f, stall_max=%d",
           aborted ? " (no-progress abort)" : "", cost, stall_max);
    DPSF_DIAG_ITER(iters_used);
    return DPSF_FIT_ITERATION_LIMIT;
}

// B4-P1-1: NaN-safe 全序比较器 (严格弱序)。
// 有限值上与 a < b 逐位等价 (正常路径行为零变化); NaN 视为最大, 排在末尾。
// 保证含 NaN 输入时 std::sort 不再违反严格弱序 (否则 UB), NaN 永不参与
// a<b 语义比较。注意: NaN 本身仍不应进入统计——上游采样阶段已过滤 (见下)。
static bool dpsf_nan_safe_less(double a, double b) {
    const bool a_nan = std::isnan(a);
    const bool b_nan = std::isnan(b);
    if (!a_nan && !b_nan) return a < b;   // 与原排序语义完全一致
    if (a_nan && b_nan) return false;     // NaN ~ NaN 等价
    return a_nan;                          // NaN > 任何有限值
}

static double compute_trimmed_mad(const SamplePixel* samples, int m, const double* params) {
    double B = params[0], A = params[1], x0 = params[2], y0 = params[3];
    double sx = params[4], sy = params[5], theta = params[6];

    double cos_t = std::cos(theta), sin_t = std::sin(theta);
    double cos2 = cos_t * cos_t, sin2 = sin_t * sin_t;
    double sin2t = std::sin(2.0 * theta);
    double inv_sx2 = 1.0 / (2.0 * sx * sx);
    double inv_sy2 = 1.0 / (2.0 * sy * sy);
    double p1 = cos2 * inv_sx2 + sin2 * inv_sy2;
    double p2 = sin2t / (4.0 * sx * sx) - sin2t / (4.0 * sy * sy);
    double p3 = sin2 * inv_sx2 + cos2 * inv_sy2;

    std::vector<double> abs_res(m);
    for (int i = 0; i < m; i++) {
        double ddx = samples[i].dx - x0;
        double ddy = samples[i].dy - y0;
        double Q = p1 * ddx * ddx + 2.0 * p2 * ddx * ddy + p3 * ddy * ddy;
        // PSF-POW-001 (P2 性能): 与 moffat4_residual 同式同改, 公式零改动。
        const double t = 1.0 + std::max(Q, 0.0);
        const double t2 = t * t;
        double model = B + A / (t2 * t2);
        abs_res[i] = std::abs(samples[i].val - model);
    }

    std::sort(abs_res.begin(), abs_res.end(), dpsf_nan_safe_less);
    int lo = static_cast<int>(m * 0.1);
    int hi = static_cast<int>(m * 0.9);
    if (lo >= hi) return abs_res[m / 2];
    double sum = 0;
    for (int i = lo; i < hi; i++) sum += abs_res[i];
    return sum / (hi - lo);
}

// 模板版本: 支持 float/double 输入 ( 双精度 ABI 改造)
// ImageT = float -> moffat4_fit (向后兼容)
// ImageT = double -> moffat4_fit_d (双精度, 不降级)
//
// PSF-DIAG-001: 原函数体改名为 moffat4_fit_tmpl_core，外面套一层**测量包装**
// （只在编译期开启 DPSF_FIT_DIAG 时记录；关闭时包装层是纯透传，编译器可内联
//  消除 ⇒ 生产路径与修复前逐位等价）。判据逻辑全部在 core 内，包装层不参与。
template<typename ImageT>
static int moffat4_fit_tmpl_core(const ImageT* image, int width, int height,
                            double cx, double cy,
                            int rect_x0, int rect_y0, int rect_x1, int rect_y1,
                            DPSFFitResult* result, int stall_iters = 0) {
    auto t0 = std::chrono::high_resolution_clock::now();
    DPSF_DIAG_SET(rw, rect_x1 - rect_x0);
    DPSF_DIAG_SET(rh, rect_y1 - rect_y0);
    DPSF_DIAG_SET(n_nonfinite, 0);
    DPSF_DIAG_SET(bkg0, 0.0);
    DPSF_DIAG_SET(a0, 0.0);
    DPSF_DIAG_SET(mad_lh, 0.0);
    DPSF_DIAG_SET(max_val, 0.0);
    DPSF_DIAG_SET(lm_iter, 0);
    DPSF_DIAG_SET(stage, DPSF_STAGE_OK);
    std::memset(result, 0, sizeof(DPSFFitResult));
    result->status = DPSF_FIT_INVALID_PARAMS;

    int rw = rect_x1 - rect_x0;
    int rh = rect_y1 - rect_y0;

    if (rw * rh < 9) {
        dpsf_log(LOG_WARN, "DPSF", "Rect area too small: %d", rw * rh);
        return DPSF_DIAG_STAGE(DPSF_STAGE_RECT_TOO_SMALL, DPSF_FIT_INVALID_PARAMS);
    }
    if (rect_x0 < 0 || rect_y0 < 0 || rect_x1 > width || rect_y1 > height) {
        dpsf_log(LOG_WARN, "DPSF", "Rect out of image bounds: [%d,%d]-[%d,%d] img=%dx%d",
               rect_x0, rect_y0, rect_x1, rect_y1, width, height);
        return DPSF_DIAG_STAGE(DPSF_STAGE_RECT_OOB, DPSF_FIT_INVALID_PARAMS);
    }

    // B4-P1-1: 采样阶段过滤非有限像素 (NaN/Inf, 含坏像元标记)。
    // NaN 不得进入后续 median/MAD 统计与 LM 拟合 (统计语义污染), 也不得
    // 进入任何 std::sort (比较语义)。跳过计数登记到日志, 不改变有限样本路径。
    int n_nonfinite = 0;
    std::vector<SamplePixel> samples;
    samples.reserve((std::size_t)rw * (std::size_t)rh);
    for (int y = rect_y0; y < rect_y1; y++) {
        for (int x = rect_x0; x < rect_x1; x++) {
            double v = static_cast<double>(image[y * width + x]);
            if (!std::isfinite(v)) {
                ++n_nonfinite;
                continue;
            }
            SamplePixel sp;
            sp.dx = static_cast<double>(x) - cx;
            sp.dy = static_cast<double>(y) - cy;
            sp.val = v;
            samples.push_back(sp);
        }
    }
    if (n_nonfinite > 0) {
        dpsf_log(LOG_WARN, "DPSF", "Skipped %d non-finite pixels in rect [%d,%d]-[%d,%d]",
               n_nonfinite, rect_x0, rect_y0, rect_x1, rect_y1);
    }
    int m = static_cast<int>(samples.size());
    if (m == 0) {
        dpsf_log(LOG_WARN, "DPSF", "All pixels non-finite in rect [%d,%d]-[%d,%d]",
               rect_x0, rect_y0, rect_x1, rect_y1);
        result->status = DPSF_FIT_INVALID_PARAMS;
        return DPSF_DIAG_STAGE(DPSF_STAGE_ALL_NONFINITE, DPSF_FIT_INVALID_PARAMS);
    }

    dpsf_log(LOG_DEBUG, "DPSF", "Sampled %d pixels from rect [%d,%d]-[%d,%d], center=(%.2f,%.2f)",
           m, rect_x0, rect_y0, rect_x1, rect_y1, cx, cy);

    std::vector<double> vals(m);
    for (int i = 0; i < m; i++) vals[i] = samples[i].val;
    std::sort(vals.begin(), vals.end(), dpsf_nan_safe_less);

    double median_val = (m % 2 == 0)
        ? (vals[m / 2 - 1] + vals[m / 2]) / 2.0
        : vals[m / 2];

    std::vector<double> lower_half;
    lower_half.reserve(m / 2);
    for (int i = 0; i < m; i++) {
        if (vals[i] < median_val) lower_half.push_back(vals[i]);
    }
    if (lower_half.empty()) lower_half.push_back(median_val);

    int nh = static_cast<int>(lower_half.size());
    double med_lh = (nh % 2 == 0)
        ? (lower_half[nh / 2 - 1] + lower_half[nh / 2]) / 2.0
        : lower_half[nh / 2];

    std::vector<double> abs_dev_lh(nh);
    for (int i = 0; i < nh; i++) abs_dev_lh[i] = std::abs(lower_half[i] - med_lh);
    std::sort(abs_dev_lh.begin(), abs_dev_lh.end(), dpsf_nan_safe_less);
    double mad_lh = (nh % 2 == 0)
        ? (abs_dev_lh[nh / 2 - 1] + abs_dev_lh[nh / 2]) / 2.0
        : abs_dev_lh[nh / 2];

    double threshold = 2.0 * 1.482602218505602 * mad_lh;
    std::vector<double> filtered;
    filtered.reserve(nh);
    for (int i = 0; i < nh; i++) {
        if (std::abs(lower_half[i] - med_lh) <= threshold)
            filtered.push_back(lower_half[i]);
    }
    if (filtered.empty()) filtered.push_back(med_lh);

    int nf = static_cast<int>(filtered.size());
    std::sort(filtered.begin(), filtered.end(), dpsf_nan_safe_less);
    double bkg0 = (nf % 2 == 0)
        ? (filtered[nf / 2 - 1] + filtered[nf / 2]) / 2.0
        : filtered[nf / 2];

    double max_val = -1e30;
    for (int i = 0; i < m; i++)
        if (samples[i].val > max_val) max_val = samples[i].val;

    double A0 = max_val - bkg0;
    DPSF_DIAG_SET(mad_lh, mad_lh);
    DPSF_DIAG_SET(max_val, max_val);
    DPSF_DIAG_SET(bkg0, bkg0);
    DPSF_DIAG_SET(a0, A0);
    if (A0 <= 0) {
        dpsf_log(LOG_WARN, "DPSF", "Amplitude <= 0: A=%.2f max=%.2f bkg=%.2f", A0, max_val, bkg0);
        return DPSF_DIAG_STAGE(DPSF_STAGE_AMPLITUDE_LE0, DPSF_FIT_INVALID_PARAMS);
    }

    double sx0 = 0.15 * rw;
    double params[7] = { bkg0, A0, 0.0, 0.0, sx0, sx0, 0.0 };

    dpsf_log(LOG_INFO, "DPSF", "Initial params: B=%.2f A=%.2f x0=0 y0=0 sx=%.2f sy=%.2f theta=0",
           bkg0, A0, sx0, sx0);

    int stall_max_local = 0;
    int lm_status = lm_solve(m, NPARAMS, params, static_cast<void*>(samples.data()),
                              moffat4_residual, 1e-8, 200, stall_iters, &stall_max_local);
    DPSF_DIAG_SET(stall_max, stall_max_local);

    dpsf_log(LOG_INFO, "DPSF", "LM result: status=%d B=%.2f A=%.2f x0=%.4f y0=%.4f sx=%.4f sy=%.4f theta=%.4f",
           lm_status, params[0], params[1], params[2], params[3], params[4], params[5], params[6]);

    double B = params[0], A = params[1], x0 = params[2], y0 = params[3];
    double sx = params[4], sy = params[5], theta = params[6];

    bool all_finite = std::isfinite(B) && std::isfinite(A) && std::isfinite(x0) &&
                      std::isfinite(y0) && std::isfinite(sx) && std::isfinite(sy) &&
                      std::isfinite(theta);
    if (!all_finite || A <= 0 || sx <= 0.3 || sy <= 0.3) {
        dpsf_log(LOG_WARN, "DPSF", "Invalid fit params: finite=%d A=%.2f sx=%.4f sy=%.4f",
               all_finite, A, sx, sy);
        result->status = DPSF_FIT_NO_CONVERGENCE;
        return DPSF_DIAG_STAGE(DPSF_STAGE_INVALID_PARAMS, DPSF_FIT_NO_CONVERGENCE);
    }

    double fwhm_x = MOFFAT4_FWHM_FACTOR * sx;
    double fwhm_y = MOFFAT4_FWHM_FACTOR * sy;

    if (fwhm_x > rw || fwhm_y > rh) {
        dpsf_log(LOG_WARN, "DPSF", "FWHM exceeds rect: fwhm_x=%.2f fwhm_y=%.2f rect=%dx%d",
               fwhm_x, fwhm_y, rw, rh);
        result->status = DPSF_FIT_NO_CONVERGENCE;
        return DPSF_DIAG_STAGE(DPSF_STAGE_FWHM_GT_RECT, DPSF_FIT_NO_CONVERGENCE);
    }

    double bkg_range = std::max(bkg0, 0.01);
    if (std::abs(B - bkg0) / bkg_range > 0.5) {
        dpsf_log(LOG_WARN, "DPSF", "Background constraint violated: B=%.4f bkg0=%.4f ratio=%.4f",
               B, bkg0, std::abs(B - bkg0) / bkg_range);
        result->status = DPSF_FIT_NO_CONVERGENCE;
        return DPSF_DIAG_STAGE(DPSF_STAGE_BKG_CONSTRAINT, DPSF_FIT_NO_CONVERGENCE);
    }

    double thetas[4] = { theta, M_PI / 2.0 - theta, M_PI / 2.0 + theta, M_PI - theta };
    double best_mad = 1e30;
    double best_theta = theta;
    for (int t = 0; t < 4; t++) {
        double test_params[7] = { B, A, x0, y0, sx, sy, thetas[t] };
        double mad = compute_trimmed_mad(samples.data(), m, test_params);
        dpsf_log(LOG_DEBUG, "DPSF", "Theta disambig [%d]: theta=%.4f mad=%.4f", t, thetas[t], mad);
        if (mad < best_mad) {
            best_mad = mad;
            best_theta = thetas[t];
        }
    }
    theta = best_theta;

    double final_params[7] = { B, A, x0, y0, sx, sy, theta };
    double mad = compute_trimmed_mad(samples.data(), m, final_params);

    // Moffat4 (beta=4) 解析积分: flux = 2 * pi * A * sx * sy / (beta - 1) = 2 * pi * A * sx * sy / 3
    double flux = 2.0 * M_PI * A * sx * sy / 3.0;

    double sx_max = std::max(sx, sy), sx_min = std::min(sx, sy);
    double eccentricity = std::sqrt(1.0 - (sx_min / sx_max) * (sx_min / sx_max));

    // Phase1 Final Closure (PSF-001): 样本 dx = pixel_x - cx,
    // 拟合 x0 是相对传入中心 cx 的偏移, 正确还原为 cx + x0。
    // 原实现用 rect 中心近似 cx, 对奇数宽 rect 引入 ~0.5px 系统偏差。
    double img_cx = cx + x0;
    double img_cy = cy + y0;

    auto t1 = std::chrono::high_resolution_clock::now();
    dpsf_log(LOG_INFO, "DPSF", "Fit done: %.1f ms status=%d cx=%.2f cy=%.2f fwhm_x=%.2f fwhm_y=%.2f mad=%.4f ecc=%.4f",
           std::chrono::duration<double, std::milli>(t1 - t0).count(),
           lm_status, img_cx, img_cy, fwhm_x, fwhm_y, mad, eccentricity);

    result->status = lm_status;
    result->B = B;
    result->A = A;
    result->cx = img_cx;
    result->cy = img_cy;
    result->sx = sx;
    result->sy = sy;
    result->theta = theta;
    result->fwhm_x = fwhm_x;
    result->fwhm_y = fwhm_y;
    result->mad = mad;
    result->flux = flux;
    result->eccentricity = eccentricity;

    return result->status;
}

// PSF-DIAG-001: 测量包装层（编译期关闭时 = 纯透传；开启时逐候选落一条记录）
template<typename ImageT>
static int moffat4_fit_tmpl(const ImageT* image, int width, int height,
                            double cx, double cy,
                            int rect_x0, int rect_y0, int rect_x1, int rect_y1,
                            DPSFFitResult* result, int stall_iters = 0) {
    if (!DPSF_DIAG_ON()) {
        return moffat4_fit_tmpl_core<ImageT>(image, width, height, cx, cy,
                                             rect_x0, rect_y0, rect_x1, rect_y1, result,
                                             stall_iters);
    }
#ifdef DPSF_FIT_DIAG
    const auto t0 = std::chrono::high_resolution_clock::now();
    const int st = moffat4_fit_tmpl_core<ImageT>(image, width, height, cx, cy,
                                                 rect_x0, rect_y0, rect_x1, rect_y1, result,
                                                 stall_iters);
    const auto t1 = std::chrono::high_resolution_clock::now();
    static std::atomic<long long> seq{0};
    DpsfDiagRec r;
    r.seq = seq.fetch_add(1, std::memory_order_relaxed);
    r.cx = cx; r.cy = cy;
    r.rw = t_dpsf_diag.rw; r.rh = t_dpsf_diag.rh;
    r.stage = t_dpsf_diag.stage;
    r.lm_status = st;
    r.lm_iter = t_dpsf_diag.lm_iter;
    r.stall_max = t_dpsf_diag.stall_max;
    r.n_nonfinite = t_dpsf_diag.n_nonfinite;
    r.bkg0 = t_dpsf_diag.bkg0; r.a0 = t_dpsf_diag.a0;
    r.mad_lh = t_dpsf_diag.mad_lh; r.max_val = t_dpsf_diag.max_val;
    r.B = result->B; r.A = result->A; r.x0 = result->cx; r.y0 = result->cy;
    r.sx = result->sx; r.sy = result->sy; r.theta = result->theta;
    r.fwhm_x = result->fwhm_x; r.fwhm_y = result->fwhm_y;
    r.mad = result->mad; r.flux = result->flux;
    r.ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    dpsf_diag_record(r);
    return st;
#else
    return moffat4_fit_tmpl_core<ImageT>(image, width, height, cx, cy,
                                         rect_x0, rect_y0, rect_x1, rect_y1, result,
                                         stall_iters);
#endif
}

// float 版本 (向后兼容, 原有签名)
int moffat4_fit(const float* image, int width, int height,
                double cx, double cy,
                int rect_x0, int rect_y0, int rect_x1, int rect_y1,
                DPSFFitResult* result) {
    return moffat4_fit_tmpl<float>(image, width, height, cx, cy,
                                    rect_x0, rect_y0, rect_x1, rect_y1, result);
}

// double 版本 (双精度 ABI, 新增)
// FP64 模式下采样像素值直接为 double, 不降级到 float32 (精度关键路径)
int moffat4_fit_d(const double* image, int width, int height,
                  double cx, double cy,
                  int rect_x0, int rect_y0, int rect_x1, int rect_y1,
                  DPSFFitResult* result) {
    return moffat4_fit_tmpl<double>(image, width, height, cx, cy,
                                     rect_x0, rect_y0, rect_x1, rect_y1, result);
}

// ── PSF-PERF-001: 批拟合**专用**入口（开启无进展提前退出）─────────────────────
// 为什么只给批路径开: §11.2 里 status=3 在**单星**接口下"仍回填当前最优参数"
// （对调用方可观测），而**批**接口对 status!=OK 一律 NaN + FIT_FAILED（不可观测）。
// 故只有批路径能给出"按位不变"的保证；单星 ABI 保持 200 次耗尽的旧行为。
static int moffat4_fit_batch_cell_f(const float* image, int width, int height,
                                    double cx, double cy,
                                    int rect_x0, int rect_y0, int rect_x1, int rect_y1,
                                    DPSFFitResult* result) {
    return moffat4_fit_tmpl<float>(image, width, height, cx, cy, rect_x0, rect_y0,
                                   rect_x1, rect_y1, result, kDpsfBatchStallIters);
}

static int moffat4_fit_batch_cell_d(const double* image, int width, int height,
                                    double cx, double cy,
                                    int rect_x0, int rect_y0, int rect_x1, int rect_y1,
                                    DPSFFitResult* result) {
    return moffat4_fit_tmpl<double>(image, width, height, cx, cy, rect_x0, rect_y0,
                                    rect_x1, rect_y1, result, kDpsfBatchStallIters);
}

DPSF_EXPORT int dpsf_fit(const uint16_t *image, int width, int height,
                          double cx, double cy,
                          const DPSFFitParams *params,
                          DPSFFitResult *result) {
    if (!image || !params || !result || width <= 0 || height <= 0) {
        dpsf_log(LOG_ERROR, "DPSF", "dpsf_fit: invalid arguments");
        return DPSF_FIT_INVALID_PARAMS;
    }

    int fitRadius = params->fitRadius;

    int x0 = std::max(0, static_cast<int>(cx) - fitRadius);
    int y0 = std::max(0, static_cast<int>(cy) - fitRadius);
    int x1 = std::min(width, static_cast<int>(cx) + fitRadius + 1);
    int y1 = std::min(height, static_cast<int>(cy) + fitRadius + 1);

    int rw = x1 - x0;
    int rh = y1 - y0;
    if (rw <= 0 || rh <= 0) {
        dpsf_log(LOG_WARN, "DPSF", "dpsf_fit: empty rect for cx=%.2f cy=%.2f fitRadius=%d", cx, cy, fitRadius);
        std::memset(result, 0, sizeof(DPSFFitResult));
        result->status = DPSF_FIT_INVALID_PARAMS;
        return DPSF_FIT_INVALID_PARAMS;
    }

    std::vector<float> float_patch((size_t)rw * rh);
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            float_patch[(y - y0) * rw + (x - x0)] =
                static_cast<float>(image[y * width + x]);
        }
    }

    double local_cx = cx - x0;
    double local_cy = cy - y0;

    dpsf_log(LOG_INFO, "DPSF", "dpsf_fit: cx=%.2f cy=%.2f rect=[%d,%d]-[%d,%d] local_cx=%.2f local_cy=%.2f fitRadius=%d",
           cx, cy, x0, y0, x1, y1, local_cx, local_cy, fitRadius);

    int ret = moffat4_fit(float_patch.data(), rw, rh, local_cx, local_cy, 0, 0, rw, rh, result);

    if (ret == DPSF_FIT_OK || ret == DPSF_FIT_ITERATION_LIMIT) {
        result->cx += x0;
        result->cy += y0;
    }

    return ret;
}

// PSF-001: 批 ABI 尺寸守卫 —— ①w/h≤0 确定性拒绝 (0/-1/INT_MIN 等全部
// 非正值, 修复前 batch/batch_f/batch_d 伪成功 rc=0 或 (size_t)w*h 下溢
// → length_error → SIGABRT, README §2/§6 声称的 -1 语义); ②w*h 寻址上界
// —— 逐像素索引 y*width+x 为 int 乘加, w*h>INT_MAX 时必然符号溢出 UB,
// 入口即拒绝 → 零整图分配零读取。通过时返回 0; 否则记 LOG_ERROR, 调用方
// 直接 return -1 (不触碰任何输出 sentinel)。
static int dpsf_batch_dims_check(const char *api, int width, int height) {
    if (width <= 0 || height <= 0) {
        dpsf_log(LOG_ERROR, "DPSF", "%s: invalid dimensions w=%d h=%d", api, width, height);
        return -1;
    }
    const long long wh = (long long)width * (long long)height;
    if (wh > (long long)INT_MAX) {
        dpsf_log(LOG_ERROR, "DPSF", "%s: w*h=%lld exceeds INT_MAX int-index addressing bound",
                 api, wh);
        return -1;
    }
    return 0;
}

// 前向声明 (float32 拟合核心, 定义在 dpsf_fit_batch 之后)
static int fit_batch_float_image(const float *float_image, int width, int height,
                                 const double *cx_array, const double *cy_array, int count,
                                 const DPSFFitParams *params,
                                 DPSFFitResult **out_results);

DPSF_EXPORT int dpsf_fit_batch(const uint16_t *image, int width, int height,
                                const double *cx_array, const double *cy_array, int count,
                                const DPSFFitParams *params,
                                DPSFFitResult **out_results) {
    auto t0 = std::chrono::high_resolution_clock::now();
    (void)t0;  /* 非 DPSF_PROFILE 构建时避免 unused 告警 */

    if (!image || !cx_array || !cy_array || !params || !out_results || count <= 0 ||
        dpsf_batch_dims_check("dpsf_fit_batch", width, height) != 0) {
        dpsf_log(LOG_ERROR, "DPSF", "dpsf_fit_batch: invalid arguments (w=%d h=%d)", width, height);
        return -1;
    }

    dpsf_log(LOG_INFO, "DPSF", "dpsf_fit_batch: %d points, %dx%d image, fitRadius=%d",
           count, width, height, params->fitRadius);

    // PSF-001: 兜底异常屏障 —— 乘积上界内仍可能因内存不足分配失败
    // (w*h ≤ INT_MAX → float 副本 ≤ 8.6 GB); C ABI 边界禁止异常外抛,
    // 分配失败转为稳定 -1 (输出 sentinel 未触碰)。
    try {
        size_t n_pixels = (size_t)width * height;
        std::vector<float> float_image(n_pixels);
        for (size_t i = 0; i < n_pixels; i++) {
            float_image[i] = static_cast<float>(image[i]);
        }

        return fit_batch_float_image(float_image.data(), width, height,
                                     cx_array, cy_array, count, params, out_results);
    } catch (const std::exception &e) {
        dpsf_log(LOG_ERROR, "DPSF", "dpsf_fit_batch: allocation failed (%s) -> rc=-1", e.what());
        return -1;
    } catch (...) {
        dpsf_log(LOG_ERROR, "DPSF", "dpsf_fit_batch: unknown failure -> rc=-1");
        return -1;
    }
}

// ============================================================================
// fit_batch_float_image - float32 图像批量 PSF 拟合核心
// 输出 DPSFFitResult* (status/B/flux/cx/cy/fwhm/A/mad/eccentricity), 与 dpsf_fit_batch 一致
// ============================================================================
static int fit_batch_float_image(const float *float_image, int width, int height,
                                 const double *cx_array, const double *cy_array, int count,
                                 const DPSFFitParams *params,
                                 DPSFFitResult **out_results) {
    if (!float_image || !cx_array || !cy_array || !params || !out_results || count <= 0 ||
        dpsf_batch_dims_check("fit_batch_float_image", width, height) != 0) {
        dpsf_log(LOG_ERROR, "DPSF", "fit_batch_float_image: invalid arguments (w=%d h=%d)", width, height);
        return -1;
    }
    DPSFFitResult *results = (DPSFFitResult *)malloc(count * sizeof(DPSFFitResult));
    if (!results) {
        dpsf_log(LOG_ERROR, "DPSF", "fit_batch_float_image: failed to allocate results");
        return -1;
    }

    int fitRadius = params->fitRadius;
    int success_count = 0;

#pragma omp parallel for schedule(dynamic) reduction(+:success_count)
    for (int i = 0; i < count; i++) {
        double cx = cx_array[i];
        double cy = cy_array[i];

        int x0 = std::max(0, static_cast<int>(cx) - fitRadius);
        int y0 = std::max(0, static_cast<int>(cy) - fitRadius);
        int x1 = std::min(width, static_cast<int>(cx) + fitRadius + 1);
        int y1 = std::min(height, static_cast<int>(cy) + fitRadius + 1);

        int rw = x1 - x0;
        int rh = y1 - y0;

        if (rw <= 0 || rh <= 0) {
            std::memset(&results[i], 0, sizeof(DPSFFitResult));
            results[i].status = DPSF_FIT_INVALID_PARAMS;
            continue;
        }

        // PSF-001: 星级异常隔离 —— patch 分配失败 (fitRadius 巨大 → 裁窗
        // 钳到整图, 单星 patch ≤ w*h ≤ INT_MAX 像素) 不得外抛跨 OpenMP/C
        // ABI 边界, 降级为该星 INVALID_PARAMS (memset 0), 其余星不受影响。
        try {
            std::vector<float> patch((size_t)rw * rh);
            for (int y = y0; y < y1; y++) {
                for (int x = x0; x < x1; x++) {
                    patch[(y - y0) * rw + (x - x0)] = float_image[y * width + x];
                }
            }

            double local_cx = cx - x0;
            double local_cy = cy - y0;

            moffat4_fit_batch_cell_f(patch.data(), rw, rh, local_cx, local_cy, 0, 0, rw, rh, &results[i]);
        } catch (const std::exception &e) {
            dpsf_log(LOG_ERROR, "DPSF", "fit_batch_float_image: star %d allocation failed (%s)",
                     i, e.what());
            std::memset(&results[i], 0, sizeof(DPSFFitResult));
            results[i].status = DPSF_FIT_INVALID_PARAMS;
            continue;
        } catch (...) {
            dpsf_log(LOG_ERROR, "DPSF", "fit_batch_float_image: star %d unknown failure", i);
            std::memset(&results[i], 0, sizeof(DPSFFitResult));
            results[i].status = DPSF_FIT_INVALID_PARAMS;
            continue;
        }

        if (results[i].status == DPSF_FIT_OK || results[i].status == DPSF_FIT_ITERATION_LIMIT) {
            results[i].cx += x0;
            results[i].cy += y0;
        }

        if (results[i].status == DPSF_FIT_OK) {
            success_count++;
        }
    }

    *out_results = results;

    dpsf_log(LOG_INFO, "DPSF", "fit_batch_float_image done: %d/%d success",
           success_count, count);

    return 0;
}

// ============================================================================
// dpsf_fit_batch_f - float32 图像直通拟合 (, PREC-105: 无 uint16 有损转换)
// ============================================================================
DPSF_EXPORT int dpsf_fit_batch_f(const float *image, int width, int height,
                                 const double *cx_array, const double *cy_array, int count,
                                 const DPSFFitParams *params,
                                 DPSFFitResult **out_results) {
    auto t0 = std::chrono::high_resolution_clock::now();
    if (!image || !cx_array || !cy_array || !params || !out_results || count <= 0 ||
        dpsf_batch_dims_check("dpsf_fit_batch_f", width, height) != 0) {
        dpsf_log(LOG_ERROR, "DPSF", "dpsf_fit_batch_f: invalid arguments (w=%d h=%d)", width, height);
        return -1;
    }
    dpsf_log(LOG_INFO, "DPSF", "dpsf_fit_batch_f: %d points, %dx%d image, fitRadius=%d (float32)",
             count, width, height, params->fitRadius);
    int ret = fit_batch_float_image(image, width, height,
                                    cx_array, cy_array, count, params, out_results);
    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();
    dpsf_log(LOG_INFO, "DPSF", "dpsf_fit_batch_f done: %.3f s", elapsed);
    return ret;
}

DPSF_EXPORT void dpsf_free_results(DPSFFitResult *results) {
    free(results);
}

// ============================================================================
// dpsf_fit_batch_d (双精度 ABI, 新增)
//
// 与 dpsf_fit_batch (uint16) 逻辑一致, 仅 image 数据类型从 uint16 改为 double。
// FP64 模式下直接在 double 图像上裁剪局部 patch 送入 moffat4_fit_d (double 拟合),
// 不创建整张 uint16/float 图像, 不降级 (精度关键路径)。
// 返回完整 DPSFFitResult 结构体 (含 status/flux/mad/eccentricity 等全部字段),
// 供 orchestrator 写出与 FP32 路径一致的 psf 块布局。
// ============================================================================
DPSF_EXPORT int dpsf_fit_batch_d(const double *image, int width, int height,
                                 const double *cx_array, const double *cy_array, int count,
                                 const DPSFFitParams *params,
                                 DPSFFitResult **out_results) {
    auto t0 = std::chrono::high_resolution_clock::now();

    if (!image || !cx_array || !cy_array || !params || !out_results || count <= 0 ||
        dpsf_batch_dims_check("dpsf_fit_batch_d", width, height) != 0) {
        dpsf_log(LOG_ERROR, "DPSF", "dpsf_fit_batch_d: invalid arguments (w=%d h=%d)", width, height);
        return -1;
    }

    dpsf_log(LOG_INFO, "DPSF", "dpsf_fit_batch_d: %d points, %dx%d image (FP64), fitRadius=%d",
           count, width, height, params->fitRadius);

    DPSFFitResult *results = (DPSFFitResult *)malloc(count * sizeof(DPSFFitResult));
    if (!results) {
        dpsf_log(LOG_ERROR, "DPSF", "dpsf_fit_batch_d: failed to allocate results");
        return -1;
    }

    int fitRadius = params->fitRadius;
    int success_count = 0;

#pragma omp parallel for schedule(dynamic) reduction(+:success_count)
    for (int i = 0; i < count; i++) {
        double cx = cx_array[i];
        double cy = cy_array[i];

        int x0 = std::max(0, static_cast<int>(cx) - fitRadius);
        int y0 = std::max(0, static_cast<int>(cy) - fitRadius);
        int x1 = std::min(width, static_cast<int>(cx) + fitRadius + 1);
        int y1 = std::min(height, static_cast<int>(cy) + fitRadius + 1);

        int rw = x1 - x0;
        int rh = y1 - y0;

        if (rw <= 0 || rh <= 0) {
            std::memset(&results[i], 0, sizeof(DPSFFitResult));
            results[i].status = DPSF_FIT_INVALID_PARAMS;
            continue;
        }

        // PSF-001: 星级异常隔离 (同 fit_batch_float_image —— patch 分配
        // 失败降级为该星 INVALID_PARAMS, 不外抛跨 OpenMP/C ABI 边界)。
        try {
            // FP64 路径: 直接从 double 图像裁剪 patch (不降级到 float32)
            std::vector<double> patch((size_t)rw * rh);
            for (int y = y0; y < y1; y++) {
                for (int x = x0; x < x1; x++) {
                    patch[(y - y0) * rw + (x - x0)] = image[(size_t)y * width + x];
                }
            }

            double local_cx = cx - x0;
            double local_cy = cy - y0;

            moffat4_fit_batch_cell_d(patch.data(), rw, rh, local_cx, local_cy, 0, 0, rw, rh, &results[i]);
        } catch (const std::exception &e) {
            dpsf_log(LOG_ERROR, "DPSF", "dpsf_fit_batch_d: star %d allocation failed (%s)",
                     i, e.what());
            std::memset(&results[i], 0, sizeof(DPSFFitResult));
            results[i].status = DPSF_FIT_INVALID_PARAMS;
            continue;
        } catch (...) {
            dpsf_log(LOG_ERROR, "DPSF", "dpsf_fit_batch_d: star %d unknown failure", i);
            std::memset(&results[i], 0, sizeof(DPSFFitResult));
            results[i].status = DPSF_FIT_INVALID_PARAMS;
            continue;
        }

        if (results[i].status == DPSF_FIT_OK || results[i].status == DPSF_FIT_ITERATION_LIMIT) {
            results[i].cx += x0;
            results[i].cy += y0;
        }

        if (results[i].status == DPSF_FIT_OK) {
            success_count++;
        }
    }

    *out_results = results;

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();
    dpsf_log(LOG_INFO, "DPSF", "dpsf_fit_batch_d done: %d/%d success, %.3f s (FP64)",
           success_count, count, elapsed);

    return 0;
}

// ============================================================================
// dpsf_fit_batch_f32 (, v1.1)
//
// float32 PSF 批量拟合, 消费 star_det v1 (FLOAT64 [N,6])。
// 不做 0-65535 clip, 不创建整张 uint16 图像, 不调用 sdet_detect_ex。
// 内部直接在 float32 图像上裁剪局部 patch 送入 moffat4_fit (float/double 拟合)。
// ============================================================================
DPSF_EXPORT int dpsf_fit_batch_f32(
    const float *image,
    int width,
    int height,
    const double *detections,
    int n_detections,
    const DPSFFitParams *params,
    double *out_psf_params,
    int *out_n_valid,
    int *out_status
) {
    auto t0 = std::chrono::high_resolution_clock::now();

    // ---- 参数校验 ----
    if (!image || !detections || !out_psf_params || !out_n_valid ||
        width <= 0 || height <= 0 || n_detections <= 0 ||
        // PSF-001: w*h int 寻址上界 + N*9 int 乘法溢出 (NaN 初始化循环
        // i < n_detections*9 为 int 乘法, N > INT_MAX/9 时符号溢出 UB)
        dpsf_batch_dims_check("dpsf_fit_batch_f32", width, height) != 0 ||
        n_detections > INT_MAX / 9) {
        dpsf_log(LOG_ERROR, "DPSF",
                 "dpsf_fit_batch_f32: invalid arguments (image=%p detections=%p out=%p n_valid=%p status=%p w=%d h=%d n=%d)",
                 image, detections, out_psf_params, out_n_valid, out_status, width, height, n_detections);
        return -1;
    }

    // ---- 默认参数 ----
    DPSFFitParams default_params;
    default_params.fitRadius = 8;
    default_params.maxIter = 200;
    default_params.tolerance = 1e-8;
    const DPSFFitParams *p = params ? params : &default_params;
    int fitRadius = p->fitRadius;

    // ---- 记录消费的 schema / count (§8 要求) ----
    dpsf_log(LOG_INFO, "DPSF",
             "dpsf_fit_batch_f32: consume schema=%s count=%d img=%dx%d fitRadius=%d",
             DPSF_STAR_DET_SCHEMA_V1, n_detections, width, height, fitRadius);

    // ---- 初始化输出: 全部置 NaN, n_valid=0, 逐星状态默认拟合失败 ----
    const double nan_val = std::numeric_limits<double>::quiet_NaN();
    // PSF-001: N*9 以 int64 计算 (入口已保证 n ≤ INT_MAX/9, 双保险防 int 乘法)
    const long long nan_fill = (long long)n_detections * 9;
    for (long long i = 0; i < nan_fill; i++) {
        out_psf_params[i] = nan_val;
    }
    *out_n_valid = 0;
    // B2-A2: 逐星状态初始化为 fit-failed; 仅成功星改写为 OK 并 compact 写入
    // out_psf_params (紧凑语义见 dynamic_psf.h DPSF_PSF_PARAMS_SCHEMA 注释)。
    for (int i = 0; i < n_detections; i++) {
        if (out_status) out_status[i] = DPSF_PSF_STATUS_FIT_FAILED;
    }

    int success_count = 0;

    // ---- OpenMP 并行批量拟合 ----
    // B2-A2: 成功行写入下标 = 该星拟合成功时的全局成功序 k (= success_count),
    // 失败星不再在 out_psf_params 中留下 NaN 洞; 星↔行映射由 out_status 承载。
    // 该下标只读/递增 (原子捕获), OpenMP 下无数据竞争。
    #pragma omp parallel for schedule(dynamic) reduction(+:success_count)
    for (int i = 0; i < n_detections; i++) {
        // star_det v1: [0]=x_px, [1]=y_px, [2]=flux, [3]=mag,
        // [4]=saturated, [5]=has_saturated
        const double *row = detections + (size_t)i * 6;
        double cx = row[0];
        double cy = row[1];

        // 计算 local rect (与 dpsf_fit_batch 保持一致)
        int x0 = std::max(0, static_cast<int>(cx) - fitRadius);
        int y0 = std::max(0, static_cast<int>(cy) - fitRadius);
        int x1 = std::min(width,  static_cast<int>(cx) + fitRadius + 1);
        int y1 = std::min(height, static_cast<int>(cy) + fitRadius + 1);

        int rw = x1 - x0;
        int rh = y1 - y0;

        double *out_row = out_psf_params + (size_t)i * 9;

        if (rw <= 0 || rh <= 0) {
            dpsf_log(LOG_DEBUG, "DPSF",
                     "dpsf_fit_batch_f32: star %d empty rect cx=%.2f cy=%.2f", i, cx, cy);
            if (out_status) out_status[i] = DPSF_PSF_STATUS_RECT_EMPTY;
            // out_row 已为 NaN
            continue;
        }

        // PSF-001: 星级异常隔离 —— patch 分配失败降级为该星 NaN 占位
        // (out_row 保持初始化 NaN, 不外抛跨 OpenMP/C ABI 边界)。
        try {
            // 直接从 float32 图像裁剪 patch (不创建整张 uint16 图像, 不 clip)
            std::vector<float> patch((size_t)rw * rh);
            for (int y = y0; y < y1; y++) {
                for (int x = x0; x < x1; x++) {
                    patch[(y - y0) * rw + (x - x0)] = image[(size_t)y * width + x];
                }
            }

            double local_cx = cx - x0;
            double local_cy = cy - y0;

            DPSFFitResult result;
            moffat4_fit_batch_cell_f(patch.data(), rw, rh, local_cx, local_cy, 0, 0, rw, rh, &result);

            if (result.status == DPSF_FIT_OK || result.status == DPSF_FIT_ITERATION_LIMIT) {
                // 把局部坐标转回图像坐标
                result.cx += x0;
                result.cy += y0;
            }

            if (result.status == DPSF_FIT_OK) {
                // B2-A2: 先按检测下标原位写 (逐星独立, 无竞争), 循环后顺序 compact
                out_row[0] = result.B;
                out_row[1] = result.A;
                out_row[2] = result.cx;
                out_row[3] = result.cy;
                out_row[4] = result.sx;
                out_row[5] = result.sy;
                out_row[6] = result.theta;
                out_row[7] = result.fwhm_x;
                out_row[8] = result.fwhm_y;
                if (out_status) out_status[i] = DPSF_PSF_STATUS_OK;
                success_count++;
            } else {
                dpsf_log(LOG_DEBUG, "DPSF",
                         "dpsf_fit_batch_f32: star %d fit failed status=%d cx=%.2f cy=%.2f",
                         i, result.status, cx, cy);
                // out_row 已为 NaN; out_status[i] 保持 FIT_FAILED
            }
        } catch (const std::exception &e) {
            dpsf_log(LOG_ERROR, "DPSF", "dpsf_fit_batch_f32: star %d allocation failed (%s)",
                     i, e.what());
            if (out_status) out_status[i] = DPSF_PSF_STATUS_ALLOC_FAILED;
            // out_row 已为 NaN
        } catch (...) {
            dpsf_log(LOG_ERROR, "DPSF", "dpsf_fit_batch_f32: star %d unknown failure", i);
            if (out_status) out_status[i] = DPSF_PSF_STATUS_ALLOC_FAILED;
            // out_row 已为 NaN
        }
    }

    // ---- B2-A2: 顺序 compact 成功行 (按检测下标升序 → 行 0..success_count-1) ----
    // 确定性: 串行、按 i 升序取行; 与线程数无关 (消除原子写序不确定性)。
    // 就地左移 (write <= i) 不丢失尚未读取的成功行。
    {
        int write = 0;
        for (int i = 0; i < n_detections; ++i) {
            const bool ok = out_status ? (out_status[i] == DPSF_PSF_STATUS_OK)
                                       : std::isfinite(out_psf_params[(size_t)i * 9 + 1]);
            if (!ok) continue;
            if (write != i) {
                double *dst = out_psf_params + (size_t)write * 9;
                const double *src = out_psf_params + (size_t)i * 9;
                for (int k = 0; k < 9; ++k) dst[k] = src[k];
            }
            ++write;
        }
    }

    *out_n_valid = success_count;

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();
    dpsf_log(LOG_INFO, "DPSF",
             "dpsf_fit_batch_f32 done: %d/%d valid, schema=%s, %.3f s",
             success_count, n_detections, DPSF_STAR_DET_SCHEMA_V1, elapsed);

    return 0;
}

// ============================================================================
// dpsf_fit_batch_f64 (双精度 ABI, 新增)
//
// double PSF 批量拟合, 消费 star_det v1 (FLOAT64 [N,6])。
// 与 dpsf_fit_batch_f32 逻辑一致, 仅 image 数据类型从 float 改为 double。
// 内部调用 moffat4_fit_d, 采样像素值直接为 double (不降级到 float32)。
// ============================================================================
DPSF_EXPORT int dpsf_fit_batch_f64(
    const double *image,
    int width,
    int height,
    const double *detections,
    int n_detections,
    const DPSFFitParams *params,
    double *out_psf_params,
    int *out_n_valid,
    int *out_status
) {
    auto t0 = std::chrono::high_resolution_clock::now();

    // ---- 参数校验 ----
    if (!image || !detections || !out_psf_params || !out_n_valid ||
        width <= 0 || height <= 0 || n_detections <= 0 ||
        // PSF-001: 同 f32 —— w*h int 寻址上界 + N*9 int 乘法溢出
        dpsf_batch_dims_check("dpsf_fit_batch_f64", width, height) != 0 ||
        n_detections > INT_MAX / 9) {
        dpsf_log(LOG_ERROR, "DPSF",
                 "dpsf_fit_batch_f64: invalid arguments (image=%p detections=%p out=%p n_valid=%p status=%p w=%d h=%d n=%d)",
                 image, detections, out_psf_params, out_n_valid, out_status, width, height, n_detections);
        return -1;
    }

    // ---- 默认参数 ----
    DPSFFitParams default_params;
    default_params.fitRadius = 8;
    default_params.maxIter = 200;
    default_params.tolerance = 1e-8;
    const DPSFFitParams *p = params ? params : &default_params;
    int fitRadius = p->fitRadius;

    // ---- 记录消费的 schema / count ----
    dpsf_log(LOG_INFO, "DPSF",
             "dpsf_fit_batch_f64: consume schema=%s count=%d img=%dx%d fitRadius=%d (FP64)",
             DPSF_STAR_DET_SCHEMA_V1, n_detections, width, height, fitRadius);

    // ---- 初始化输出: 全部置 NaN, n_valid=0, 逐星状态默认拟合失败 ----
    const double nan_val = std::numeric_limits<double>::quiet_NaN();
    // PSF-001: N*9 以 int64 计算 (入口已保证 n ≤ INT_MAX/9, 双保险防 int 乘法)
    const long long nan_fill = (long long)n_detections * 9;
    for (long long i = 0; i < nan_fill; i++) {
        out_psf_params[i] = nan_val;
    }
    *out_n_valid = 0;
    // B2-A2: 逐星状态初始化为 fit-failed; 仅成功星改写为 OK 并 compact 写入
    // out_psf_params (紧凑语义见 dynamic_psf.h DPSF_PSF_PARAMS_SCHEMA 注释)。
    for (int i = 0; i < n_detections; i++) {
        if (out_status) out_status[i] = DPSF_PSF_STATUS_FIT_FAILED;
    }

    int success_count = 0;

    // ---- OpenMP 并行批量拟合 (FP64: 直接从 double 图像裁剪 patch) ----
    // B2-A2: 成功行先按检测下标原位写, 循环后顺序 compact; 星↔行映射由
    // out_status 承载 (失败星不在 out_psf_params 中留 NaN 洞)。
    #pragma omp parallel for schedule(dynamic) reduction(+:success_count)
    for (int i = 0; i < n_detections; i++) {
        const double *row = detections + (size_t)i * 6;
        double cx = row[0];
        double cy = row[1];

        int x0 = std::max(0, static_cast<int>(cx) - fitRadius);
        int y0 = std::max(0, static_cast<int>(cy) - fitRadius);
        int x1 = std::min(width,  static_cast<int>(cx) + fitRadius + 1);
        int y1 = std::min(height, static_cast<int>(cy) + fitRadius + 1);

        int rw = x1 - x0;
        int rh = y1 - y0;

        double *out_row = out_psf_params + (size_t)i * 9;

        if (rw <= 0 || rh <= 0) {
            dpsf_log(LOG_DEBUG, "DPSF",
                     "dpsf_fit_batch_f64: star %d empty rect cx=%.2f cy=%.2f", i, cx, cy);
            if (out_status) out_status[i] = DPSF_PSF_STATUS_RECT_EMPTY;
            continue;
        }

        // PSF-001: 星级异常隔离 —— patch 分配失败降级为该星 NaN 占位。
        try {
            // FP64 路径: 直接从 double 图像裁剪 patch (不降级到 float32)
            std::vector<double> patch((size_t)rw * rh);
            for (int y = y0; y < y1; y++) {
                for (int x = x0; x < x1; x++) {
                    patch[(y - y0) * rw + (x - x0)] = image[(size_t)y * width + x];
                }
            }

            double local_cx = cx - x0;
            double local_cy = cy - y0;

            DPSFFitResult result;
            moffat4_fit_batch_cell_d(patch.data(), rw, rh, local_cx, local_cy, 0, 0, rw, rh, &result);

            if (result.status == DPSF_FIT_OK || result.status == DPSF_FIT_ITERATION_LIMIT) {
                result.cx += x0;
                result.cy += y0;
            }

            if (result.status == DPSF_FIT_OK) {
                // B2-A2: 先按检测下标原位写 (逐星独立, 无竞争), 循环后顺序 compact
                out_row[0] = result.B;
                out_row[1] = result.A;
                out_row[2] = result.cx;
                out_row[3] = result.cy;
                out_row[4] = result.sx;
                out_row[5] = result.sy;
                out_row[6] = result.theta;
                out_row[7] = result.fwhm_x;
                out_row[8] = result.fwhm_y;
                if (out_status) out_status[i] = DPSF_PSF_STATUS_OK;
                success_count++;
            } else {
                dpsf_log(LOG_DEBUG, "DPSF",
                         "dpsf_fit_batch_f64: star %d fit failed status=%d cx=%.2f cy=%.2f",
                         i, result.status, cx, cy);
                // out_status[i] 保持 FIT_FAILED
            }
        } catch (const std::exception &e) {
            dpsf_log(LOG_ERROR, "DPSF", "dpsf_fit_batch_f64: star %d allocation failed (%s)",
                     i, e.what());
            if (out_status) out_status[i] = DPSF_PSF_STATUS_ALLOC_FAILED;
        } catch (...) {
            dpsf_log(LOG_ERROR, "DPSF", "dpsf_fit_batch_f64: star %d unknown failure", i);
            if (out_status) out_status[i] = DPSF_PSF_STATUS_ALLOC_FAILED;
        }
    }

    // ---- B2-A2: 顺序 compact 成功行 (按检测下标升序 → 行 0..success_count-1) ----
    // 确定性: 串行、按 i 升序取行; 与线程数无关。
    {
        int write = 0;
        for (int i = 0; i < n_detections; ++i) {
            const bool ok = out_status ? (out_status[i] == DPSF_PSF_STATUS_OK)
                                       : std::isfinite(out_psf_params[(size_t)i * 9 + 1]);
            if (!ok) continue;
            if (write != i) {
                double *dst = out_psf_params + (size_t)write * 9;
                const double *src = out_psf_params + (size_t)i * 9;
                for (int k = 0; k < 9; ++k) dst[k] = src[k];
            }
            ++write;
        }
    }

    *out_n_valid = success_count;

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();
    dpsf_log(LOG_INFO, "DPSF",
             "dpsf_fit_batch_f64 done: %d/%d valid, schema=%s, %.3f s (FP64)",
             success_count, n_detections, DPSF_STAR_DET_SCHEMA_V1, elapsed);

    return 0;
}
