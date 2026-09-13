// P1-WCS-TEST · FIX-WCS-A..E 合成 fixture generator
//
// 合同锚: docs/algorithms/PLATESOLVE.md §11.4 TEST-WCS-DESIGN-001
// (P1-WCS-DOC 冻结, 2026-09-07, wave W1); 矩阵行 P1-WCS (MOD
// astrocs.p1.plate_solve, TEST-WCS-001/INV/FAIL)。
//
// 规则 (模板 <prefix>-TEST §2, 对齐 p1cal 谱系):
//   - 全 fixture 由固定 seed + 参数确定生成, 零随机硬件依赖, 不提交大二进制。
//   - 本仓统一 splitmix64 PRNG 约定 (与 core_artifact_test.cpp / p1cal 谱系
//     一致): 64 位状态, 输出 [0,1) double / 正态 double。
//   - fixture 值只进测试面与 oracle 的"输入"侧; 期望值一律由 oracle 独立
//     推导, 绝不经过被测函数 (模板 <prefix>-TEST §3)。
//
// 坐标系约定 (与被测实现一致, 来源 PLATESOLVE.md §11.4 + SRC-WCS-001 实测):
//   - solver 内部 Y-up: U = (px - cx, cy - py)  [px/py 0-based, cx=w/2, cy=h/2]
//   - W = (xi, eta) 角秒, gnomonic 平面坐标, 原点 (ra0, dec0)
//   - extract_wcs_sip 输出边界 Y-up → Y-down (cd12/cd22 取反, A×(-1)^j,
//     B×-(-1)^j, AP/BP 同规则; ipv_wcs.cpp 步骤 8)
//
// 容差来源注记 (F1-F6 全部冻结于 PLATESOLVE.md §11.4, 本面不得放宽):
//   F1: n_pairs≥12, rms_arcsec≤0.5", CD 相对误差≤2%, |ΔCRVAL|≤1"
//   F2: SIP 前向/逆向 roundtrip |Δ|≤1e-4 px (中心 90% 区域)
//   F3: CRPIX=(w/2+0.5, h/2+0.5) 精确; Y 翻转 CD 第 2 列符号翻转
//   F5: 同输入 3 次 bitwise; 线程 1/2/4 bitwise
//   F6: WcsTan roundtrip < 1e-6 deg (tests/unit/p1_wcs_phot_test.cpp:50 冻结值)
//       + B2-A1 绝对前向交叉 ≤1e-9 deg (与 oracle_wcs_forward 独立 TAN 逆投影
//         对拍; roundtrip 对"ξ/η deg 当 rad"成对单位错零鉴别力, AUD-COORD F-01/F-06)
//   §9 容差来源: 收敛 0.01" (pixel/3600), 尺度容差 0.002, Huber 1.345。
#ifndef P1WCS_FIXTURES_HPP
#define P1WCS_FIXTURES_HPP

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "ipv_types.h"  // ipv::StarPoint / MatchPair (fixture 输入侧类型)

namespace p1wcs {

// ipv 输入侧类型在本 namespace 内直用 (fixture 结构体字段声明)
using ipv::MatchPair;
using ipv::StarPoint;

// 统一 PRNG: splitmix64 (fixture 谱系约定, seed 完全决定序列)
inline std::uint64_t splitmix64(std::uint64_t& state) {
    std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

inline double uniform01(std::uint64_t& state) {
    return static_cast<double>(splitmix64(state) >> 11) * (1.0 / 9007199254740992.0);
}

// ---- 真值线性场参数 (Y-up 弧秒域: W = M·U + t) ----
struct TruthLinear {
    double m00, m01, m10, m11;  // 弧秒/像素
    double t0, t1;              // 弧秒
    double ra0, dec0;           // 度 (gnomonic 原点)
    double s0;                  // 像素尺度 (角秒/像素)
};

// 真值 trans 求值器 (fixture 构造域, 非 ipv::apply_trans 路径)
inline void truth_apply(const TruthLinear& tr, double ux, double uy,
                        double* wx, double* wy) {
    *wx = tr.m00 * ux + tr.m01 * uy + tr.t0;
    *wy = tr.m10 * ux + tr.m11 * uy + tr.t1;
}

// ---- FIX-WCS-A 线性合成场 (F1/F3/F5/F6 素材) ----
// 16×16 网格抖动 = 256 星; M = s0·R(theta), t 可选小平移 (A2 位移语义素材)。
// 期望: iter_trans(order=1) 精确恢复 M,t (无噪声, rms ~1e-9"); extract_wcs_sip
// CD = M·diag(1,-1)/3600 (Y-down), CRVAL=(ra0,dec0) 或位移收敛中心。
struct FixWcsA {
    std::vector<StarPoint> U;        // 像素坐标 (Y-up, 原点图像中心)
    std::vector<StarPoint> W;        // 弧秒坐标 (gnomonic)
    std::vector<MatchPair> pairs;    // 构造已知 identity 对应
    TruthLinear truth;
    int width, height;
};

inline FixWcsA fix_wcs_a_linear(unsigned seed, double t0, double t1,
                                int width = 1024, int height = 1024) {
    FixWcsA fx;
    fx.width = width;
    fx.height = height;
    TruthLinear tr;
    tr.s0 = 0.4;                       // 角秒/像素 (§9 尺度量级)
    const double theta = 0.05;         // 位置角 (rad)
    tr.m00 = tr.s0 * std::cos(theta);
    tr.m01 = -tr.s0 * std::sin(theta);
    tr.m10 = tr.s0 * std::sin(theta);
    tr.m11 = tr.s0 * std::cos(theta);
    tr.t0 = t0;
    tr.t1 = t1;
    tr.ra0 = 150.0;
    tr.dec0 = 2.0;
    fx.truth = tr;

    const double cx = width / 2.0, cy = height / 2.0;
    std::uint64_t st = seed;
    const int N = 16;
    for (int gi = 0; gi < N; ++gi) {
        for (int gj = 0; gj < N; ++gj) {
            const double jx = (uniform01(st) - 0.5) * 16.0;
            const double jy = (uniform01(st) - 0.5) * 16.0;
            const double px = 32.0 + gj * (960.0 / (N - 1)) + jx;
            const double py = 32.0 + gi * (960.0 / (N - 1)) + jy;
            StarPoint u;
            u.x = px - cx;             // Y-up
            u.y = cy - py;
            u.flux = 1000.0 + uniform01(st) * 9000.0;
            u.saturated = false;
            StarPoint w;
            truth_apply(tr, u.x, u.y, &w.x, &w.y);
            w.flux = u.flux;
            w.saturated = false;
            MatchPair mp;
            mp.u = static_cast<int>(fx.U.size());
            mp.w = static_cast<int>(fx.W.size());
            fx.U.push_back(u);
            fx.W.push_back(w);
            fx.pairs.push_back(mp);
        }
    }
    return fx;
}

// ---- FIX-WCS-B SIP 场 (F2 素材) ----
// 线性部分同 A, 叠加已知二次畸变 (弧秒/px²):
//   Wx += qx20·ux² + qx11·ux·uy + qx02·uy²
//   Wy += qy20·ux² + qy11·ux·uy + qy02·uy²
// 期望 SIP (oracle 独立推导, 见 p1wcs_oracle.hpp):
//   A_up = inv(M)·(qx, qy)  (px/弧秒 · 弧秒/px² = 1/px)
//   Y-down: A[i][j] = A_up[i][j]·(-1)^j, B[i][j] = -B_up[i][j]·(-1)^j
struct FixWcsB {
    std::vector<StarPoint> U;
    std::vector<StarPoint> W;
    std::vector<MatchPair> pairs;
    TruthLinear truth;
    double qx20, qx11, qx02;
    double qy20, qy11, qy02;
    int width, height;
};

inline void truth_apply_b(const FixWcsB& fx, double ux, double uy,
                          double* wx, double* wy) {
    const TruthLinear& tr = fx.truth;
    const double lx = tr.m00 * ux + tr.m01 * uy + tr.t0;
    const double ly = tr.m10 * ux + tr.m11 * uy + tr.t1;
    *wx = lx + fx.qx20 * ux * ux + fx.qx11 * ux * uy + fx.qx02 * uy * uy;
    *wy = ly + fx.qy20 * ux * ux + fx.qy11 * ux * uy + fx.qy02 * uy * uy;
}

inline FixWcsB fix_wcs_b_sip(unsigned seed, int width = 1024, int height = 1024) {
    FixWcsB fx;
    fx.width = width;
    fx.height = height;
    fx.truth.s0 = 0.4;
    const double theta = -0.03;
    fx.truth.m00 = fx.truth.s0 * std::cos(theta);
    fx.truth.m01 = -fx.truth.s0 * std::sin(theta);
    fx.truth.m10 = fx.truth.s0 * std::sin(theta);
    fx.truth.m11 = fx.truth.s0 * std::cos(theta);
    fx.truth.t0 = 0.0;
    fx.truth.t1 = 0.0;
    fx.truth.ra0 = 210.0;
    fx.truth.dec0 = 45.0;
    // 二次畸变系数 (弧秒/px²): 边缘 512px 处贡献 ~47" (宽视场量级)
    fx.qx20 = 1.8e-4; fx.qx11 = 1.2e-4; fx.qx02 = -7.0e-5;
    fx.qy20 = 5.0e-5; fx.qy11 = -1.1e-4; fx.qy02 = 2.2e-4;

    const double cx = width / 2.0, cy = height / 2.0;
    std::uint64_t st = seed;
    const int N = 16;
    for (int gi = 0; gi < N; ++gi) {
        for (int gj = 0; gj < N; ++gj) {
            const double jx = (uniform01(st) - 0.5) * 16.0;
            const double jy = (uniform01(st) - 0.5) * 16.0;
            const double px = 32.0 + gj * (960.0 / (N - 1)) + jx;
            const double py = 32.0 + gi * (960.0 / (N - 1)) + jy;
            StarPoint u;
            u.x = px - cx;
            u.y = cy - py;
            u.flux = 1000.0 + uniform01(st) * 9000.0;
            u.saturated = false;
            StarPoint w;
            truth_apply_b(fx, u.x, u.y, &w.x, &w.y);
            w.flux = u.flux;
            w.saturated = false;
            MatchPair mp;
            mp.u = static_cast<int>(fx.U.size());
            mp.w = static_cast<int>(fx.W.size());
            fx.U.push_back(u);
            fx.W.push_back(w);
            fx.pairs.push_back(mp);
        }
    }
    return fx;
}

// ---- FIX-WCS-C 稀疏/空场 (F4 负例: 0 星 / <3 星) ----
inline void fix_wcs_c_few_stars(std::vector<StarPoint>* U,
                                std::vector<StarPoint>* W,
                                std::vector<MatchPair>* pairs, int n) {
    U->clear();
    W->clear();
    pairs->clear();
    std::uint64_t st = 90210u;
    for (int i = 0; i < n; ++i) {
        StarPoint u;
        u.x = (uniform01(st) - 0.5) * 800.0;
        u.y = (uniform01(st) - 0.5) * 800.0;
        u.flux = 5000.0;
        u.saturated = false;
        StarPoint w;
        w.x = u.x * 0.4;  // 任意非退化映射
        w.y = u.y * 0.4;
        w.flux = 5000.0;
        w.saturated = false;
        MatchPair mp;
        mp.u = i;
        mp.w = i;
        U->push_back(u);
        W->push_back(w);
        pairs->push_back(mp);
    }
}

// ---- FIX-WCS-D 无对应星表场 (F4: 指向偏差 >FOV → 星表-图像无真实对应) ----
// 现实语义: 指向偏差 1800"≈0.5° >> FOV≈0.11° 时, 锥查返回的 Gaia 星与图像
// 星是两批不同的星 (同一批星整体平移会被 TRANS 平移项吸收, probe 实测
// triangle/itertrans 对平移免疫 — 三角形描述符 ba/ca 平移不变)。等价注入:
// W 场由独立 seed 生成随机散布星表 (与 U 无任何一致线性关系) →
// triangle_match 投票崩塌 / iter_trans 内点枯竭 → success=0。
inline void fix_wcs_d_unmatched(std::vector<StarPoint>* U,
                                std::vector<StarPoint>* W,
                                std::vector<MatchPair>* pairs, int n) {
    U->clear();
    W->clear();
    pairs->clear();
    std::uint64_t st_u = 424242u;   // U: 网格抖动 (同 FIX-WCS-A 拓扑)
    std::uint64_t st_w = 777777u;   // W: 独立随机散布 (无对应)
    for (int i = 0; i < n; ++i) {
        StarPoint u;
        u.x = (uniform01(st_u) - 0.5) * 960.0;
        u.y = (uniform01(st_u) - 0.5) * 960.0;
        u.flux = 1000.0 + uniform01(st_u) * 9000.0;
        u.saturated = false;
        StarPoint w;
        w.x = (uniform01(st_w) - 0.5) * 800.0;
        w.y = (uniform01(st_w) - 0.5) * 800.0;
        w.flux = 1000.0 + uniform01(st_w) * 9000.0;
        w.saturated = false;
        MatchPair mp;
        mp.u = i;
        mp.w = i;  // 伪对应 (噪声对, 真实链路来自 triangle 投票)
        U->push_back(u);
        W->push_back(w);
        pairs->push_back(mp);
    }
}

// ---- FIX-WCS-E 共线退化场 (F4/DISP-WCS-001: CD det 退化注入) ----
// 全部星点位于 U 的 x 轴 (uy=0) → 线性正规方程奇异 → 冻结失败-置信度语义:
// 退化必须 success=0, 禁止以坍缩值冒充解。
inline void fix_wcs_e_collinear(std::vector<StarPoint>* U,
                                std::vector<StarPoint>* W,
                                std::vector<MatchPair>* pairs, int n) {
    U->clear();
    W->clear();
    pairs->clear();
    std::uint64_t st = 31337u;
    for (int i = 0; i < n; ++i) {
        StarPoint u;
        u.x = -400.0 + i * (800.0 / (n - 1));
        u.y = 0.0;
        u.flux = 5000.0;
        u.saturated = false;
        StarPoint w;
        w.x = u.x * 0.4;
        w.y = u.x * 0.4 * 0.01;  // 轻微倾斜仍共线 (y 无独立信息)
        w.flux = 5000.0;
        w.saturated = false;
        MatchPair mp;
        mp.u = i;
        mp.w = i;
        U->push_back(u);
        W->push_back(w);
        pairs->push_back(mp);
    }
}

// ---- FIX-WCS-F 三档畸变场 (WCS-003: low/mid/high, 冻结 FIX-WCS-B 原样保留) ----
// 形状与 FIX-WCS-B 同源 (二次畸变, 弧秒/px² 系数), 档位 dist_scale 缩放:
//   low  = 0.1  (边缘前向畸变 ~11.8 px @1024²)
//   mid  = 0.5  (~59 px)
//   high = 1.0  (~118 px, 与冻结 F2 fixture 同量级 — WCS-002 探针口径
//                边缘畸变 ~117px, 像素域 47"/0.4"≈118)
// 原 FIX-WCS-B (fix_wcs_b_sip) 不替换不改动, 继续承载既有 F2 锚;
// 本生成器供 WCS-003 迭代反演冻结门验收 (center90/边界/随机/拒绝/parity)。
// 坐标约定与 FIX-WCS-B 完全一致 (Y-up 内部 + W 弧秒 gnomonic)。
struct FixWcsF {
    std::vector<StarPoint> U;
    std::vector<StarPoint> W;
    std::vector<MatchPair> pairs;
    TruthLinear truth;
    double dist_scale;   // 畸变档位 (0.1/0.5/1.0)
    double q_x20, q_x11, q_x02;  // 畸变系数 (弧秒/px², 基准形状 × 档位)
    double q_y20, q_y11, q_y02;
    int width, height;
};

inline void truth_apply_f(const FixWcsF& fx, double ux, double uy,
                          double* wx, double* wy) {
    const TruthLinear& tr = fx.truth;
    const double lx = tr.m00 * ux + tr.m01 * uy + tr.t0;
    const double ly = tr.m10 * ux + tr.m11 * uy + tr.t1;
    *wx = lx + fx.q_x20 * ux * ux + fx.q_x11 * ux * uy + fx.q_x02 * uy * uy;
    *wy = ly + fx.q_y20 * ux * ux + fx.q_y11 * ux * uy + fx.q_y02 * uy * uy;
}

inline FixWcsF fix_wcs_f_distortion(unsigned seed, double dist_scale,
                                    int width = 1024, int height = 1024) {
    FixWcsF fx;
    fx.width = width;
    fx.height = height;
    fx.dist_scale = dist_scale;
    fx.truth.s0 = 0.4;
    const double theta = -0.03;
    fx.truth.m00 = fx.truth.s0 * std::cos(theta);
    fx.truth.m01 = -fx.truth.s0 * std::sin(theta);
    fx.truth.m10 = fx.truth.s0 * std::sin(theta);
    fx.truth.m11 = fx.truth.s0 * std::cos(theta);
    fx.truth.t0 = 0.0;
    fx.truth.t1 = 0.0;
    fx.truth.ra0 = 210.0;
    fx.truth.dec0 = 45.0;
    // 二次畸变系数 = FIX-WCS-B 基准形状 × 档位 (弧秒/px²)
    fx.q_x20 = 1.8e-4 * dist_scale; fx.q_x11 = 1.2e-4 * dist_scale;
    fx.q_x02 = -7.0e-5 * dist_scale;
    fx.q_y20 = 5.0e-5 * dist_scale; fx.q_y11 = -1.1e-4 * dist_scale;
    fx.q_y02 = 2.2e-4 * dist_scale;

    const double cx = width / 2.0, cy = height / 2.0;
    std::uint64_t st = seed;
    const int N = 16;
    for (int gi = 0; gi < N; ++gi) {
        for (int gj = 0; gj < N; ++gj) {
            const double jx = (uniform01(st) - 0.5) * 16.0;
            const double jy = (uniform01(st) - 0.5) * 16.0;
            const double px = 32.0 + gj * (960.0 / (N - 1)) + jx;
            const double py = 32.0 + gi * (960.0 / (N - 1)) + jy;
            StarPoint u;
            u.x = px - cx;
            u.y = cy - py;
            u.flux = 1000.0 + uniform01(st) * 9000.0;
            u.saturated = false;
            StarPoint w;
            truth_apply_f(fx, u.x, u.y, &w.x, &w.y);
            w.flux = u.flux;
            w.saturated = false;
            MatchPair mp;
            mp.u = static_cast<int>(fx.U.size());
            mp.w = static_cast<int>(fx.W.size());
            fx.U.push_back(u);
            fx.W.push_back(w);
            fx.pairs.push_back(mp);
        }
    }
    return fx;
}

}  // namespace p1wcs

#endif  // P1WCS_FIXTURES_HPP
