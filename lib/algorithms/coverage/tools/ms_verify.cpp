// lib/algorithms/coverage/tools/ms_verify.cpp — task-4 验证程序（源码入仓）。
//
// 门定义（合成自门 ≠ 生产门）：
//   合成自门 = 本程序内五项（V1 逐位一致 / V2 0<D<bias / V3 ratio≤1.01 /
//     V4 逐位一致 / V5 五类拒绝），全部 PASS 才 exit 0；
//   生产门 = 缝台阶 ≤5e-06（M42 真缝验证，见 PHASE2_SAMPLER §5.11）。
// /tmp 二进制只是黑盒复核，不得作为交付证据。本程序是交付证据的源码载体。
//
// 五项自验：
//   V1 缺省关闭逐位一致：默认 cfg（ms_enabled=0）build 两次 ⇒ model_hash 逐位一致，
//      且 calibrate 输出逐位一致。
//   V2 合成台阶（合成自门，非生产门）：2 帧 × 8×8 control 网格，
//      f2 = 偏置 b=0.5 + 右半低频坡 0.3，启用 ms（ms_enabled=1）后 ms 场
//      台阶 Δ 须 0<Δ<bias（明显小于偏置；旧口径 r=y−M 的双重扣除会给出
//      Δ≈+b）。实测 D≈5.35e-04 ≈ 生产门 5e-06 的约 107 倍 ⇒ V2 只判合成
//      自门 PASS，生产门待 M42 真缝验证；不得把 D 说成过生产门。
//   V3 缝带窗 C-spread 不恶化：对照窗取缝两侧各两列
//      （gx∈{2,3,4,5} × 全 gy，共 32 点；台阶横贯 ⇒ off 方差恒>0，
//      无零分母退化；旧右半窗 off spread 恒零、ratio 置零 PASS 的退化口径
//      已退役），比较 C 场 spread（med/p90 of |C-med|），容差门
//      ratio = on/off ≤ 1.01（ms 微纹波 ±3e-04 在 0.3 台阶上的 spread
//      增量约 0.2%，判不恶化；C-spread 只作辅助）；任一 off spread
//      为零 ⇒ 退化窗，判 FAIL（fail-closed，不得置零 PASS）。
//   V4 dense/sparse 逐位一致：同一模型 sparse calibrate 与 dense 物化回读
//      逐像素逐位一致（差值恒 0；dense 逐像素调同一求值语义）。
//   V5 配置门：缺省 ms_enabled=0/sigma=16/thresh=2.0；make_upm_cfg 透传；
//      五类拒绝自足（enabled 越界 / sigma 下探 8 / sigma 上探 32 /
//      thresh 下限 / 未知键 ms_foo）fail-closed，无补测旧账。
//
// 残差口径：r = y − M − C（求解后残差；求解前 r=y−M 为双重扣除，禁用）。
// σ 口径：ρ=σ_px/cell_side，passes=round(ρ²/0.16)，clamp [1,3]（见 §5.11）。

#include "astro/phase2/upm.h"
#include "astro/phase2/sampler.h"
#include "astro/phase2/stage2_common.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// 最小 control 网格：1 tile, 8×8 cells（grid=8, cell_side=64）。
static constexpr int kGrid = 8;
static constexpr std::uint64_t kTile = 1;

static P2ControlNode make_node(std::uint64_t cid, int gx, int gy) {
    P2ControlNode n{};
    n.control_id = cid;
    n.tile_ipix = kTile;
    n.gx = gx;
    n.gy = gy;
    // control 中心 leaf（tile 内 512×512，cell_side=64）：中心=(gx*64+32, gy*64+32)。
    const std::uint32_t x = (std::uint32_t)(gx * 64 + 32);
    const std::uint32_t y = (std::uint32_t)(gy * 64 + 32);
    // NESTED local = 交织（与 nested_local_to_xy 互逆；此处用 Morton 编码）。
    std::uint64_t local = 0;
    for (int b = 0; b < 9; ++b) {
        local |= ((std::uint64_t)((x >> b) & 1u) << (2 * b));
        local |= ((std::uint64_t)((y >> b) & 1u) << (2 * b + 1));
    }
    n.leaf_ipix = (kTile << 18) | local;
    // ra/dec 仅用于邻接跨 tile（同 tile 网格邻接足矣），给单调值即可。
    n.ra_deg = 10.0 + gx * 0.01;
    n.dec_deg = -5.0 + gy * 0.01;
    return n;
}

static P2ControlObservation make_obs(std::uint64_t fid, std::uint64_t cid,
                                     double v) {
    P2ControlObservation o{};
    o.frame_id = fid;
    o.control_id = cid;
    o.leaf_ipix = 0;
    o.ra_deg = 0.0;
    o.dec_deg = 0.0;
    o.value = v;
    o.uncertainty = 0.5;
    o.snr = 5.0;
    o.ivar = 0.0;
    // control_variance = k_corr(π/2)σ²/N：σ=0.5,N=289,k=1.4 ⇒ ≈0.00191。
    o.control_variance = 1.4 * 1.5707963267948966 * 0.25 / 289.0;
    o.control_ivar = 1.0 / o.control_variance;
    o.snr_available = 0;
    o.support = 1.0;
    o.quality_flags = 0;
    return o;
}

static double median_sorted(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    return (n % 2) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

// 缝台阶：右半均值 − 左半均值（校准后同一帧内）。
static double seam_step(const std::vector<double>& cal) {
    double l = 0.0, r = 0.0;
    int nl = 0, nr = 0;
    for (int gy = 0; gy < kGrid; ++gy)
        for (int gx = 0; gx < kGrid; ++gx) {
            const double v = cal[(std::size_t)gy * kGrid + gx];
            if (gx < kGrid / 2) { l += v; ++nl; }
            else { r += v; ++nr; }
        }
    return r / nr - l / nl;
}

int main() {
    // B 项语义：验证程序内的 off 基线必须走显式 cfg（不受测试覆写环境
    // 变量污染）。若外层带 ACSD_UPM_MS_ENABLE=1 运行，本程序先清掉，
    // 使 V1/V2 的 off 基线恒为"缺省关闭"；环境覆写能力由 B 项单独断言。
    ::unsetenv("ACSD_UPM_MS_ENABLE");
    ::unsetenv("ACSD_UPM_MS_SIGMA_PX");
    ::unsetenv("ACSD_UPM_MS_THRESH");
    int fails = 0;
    const std::uint64_t f1 = 101, f2 = 102;
    // 观测：f1 全 0；f2 = 均匀偏置 b=0.5 + 右半宽缓低频坡 P=0.3（低频、
    // 不过 mask：坡高 < 2σ 阈，mask 只 veto 亮端离群）。
    // 求解前 r=y−M 的双重扣除会把偏置 b 再派生一次 ⇒ ms 台阶 Δ≈+b；
    // 求解后 r=y−M−C 只剩坡的低频尾：要求 0<Δ<kBias（明显小于偏置）。
    const double kBias = 0.5, kSlope = 0.3;

    // 几何：64 nodes。
    std::vector<P2ControlNode> nodes;
    for (int gy = 0; gy < kGrid; ++gy)
        for (int gx = 0; gx < kGrid; ++gx)
            nodes.push_back(make_node((std::uint64_t)(gy * kGrid + gx), gx, gy));

    // 观测：f1 全 0；f2 左半 0/右半 +6（台阶 S=6）。leaf_ipix 必须填
    // 对应 node 的 leaf（build 按 obs leaf 定 cell，不过 nodes 面则挂不上）。
    std::vector<P2ControlObservation> obs;
    for (const auto& n : nodes) {
        P2ControlObservation a = make_obs(f1, n.control_id, 0.0);
        a.leaf_ipix = n.leaf_ipix;
        a.ra_deg = n.ra_deg;
        a.dec_deg = n.dec_deg;
        obs.push_back(a);
        const int gx = (int)(n.control_id % kGrid);
        double v2 = kBias;
        if (gx >= kGrid / 2) v2 += kSlope;
        P2ControlObservation b = make_obs(f2, n.control_id, v2);
        b.leaf_ipix = n.leaf_ipix;
        b.ra_deg = n.ra_deg;
        b.dec_deg = n.dec_deg;
        obs.push_back(b);
    }

    P2UpmBuildConfig base{};
    base.robust_loss = 0;
    base.snr_weight_mode = 0;
    base.huber_delta = 1.345;
    base.smoothing_lambda = 0.0;
    base.zero_anchor_weight = 1e-3;
    base.max_iterations = 100;
    base.tolerance = 1e-6;
    base.target_order = 9;
    base.sigma_floor = 1e-3;
    base.support_power = 1.0;
    base.quality_mode = 0;
    base.use_ivar_weight = 1;
    base.control_reliability = 1.0;
    base.cpu_workers = 1;
    base.gs_damping = 1.0;
    base.m_full_frame = 0;
    base.final_gauge = 0;
    base.tolerance_relative = 0;
    base.grid = 8;
    base.ms_enabled = 0;
    base.ms_sigma_px = 16.0;
    base.ms_thresh = 2.0;

    // ---- V1：缺省关闭逐位一致 ----
    void *m1 = nullptr, *m2 = nullptr;
    if (p2_upm_build_geo(obs.data(), obs.size(), nodes.data(), nodes.size(),
                         &base, &m1) != 0 || !m1) {
        std::printf("V1 build1 failed\n");
        return 1;
    }
    if (p2_upm_build_geo(obs.data(), obs.size(), nodes.data(), nodes.size(),
                         &base, &m2) != 0 || !m2) {
        std::printf("V1 build2 failed\n");
        p2_upm_close(m1);
        return 1;
    }
    P2ModelInfo i1{}, i2{};
    p2_upm_info(m1, &i1);
    p2_upm_info(m2, &i2);
    const bool v1_hash =
        std::memcmp(i1.model_hash, i2.model_hash, 64) == 0;
    // calibrate 逐位一致（control 中心 leaf）。
    std::vector<std::uint64_t> leaves;
    for (const auto& n : nodes) leaves.push_back(n.leaf_ipix);
    std::vector<double> in(leaves.size(), 1.0), o1(leaves.size()),
        o2(leaves.size());
    p2_upm_calibrate_block(m1, f2, leaves.data(), in.data(), o1.data(),
                           leaves.size());
    p2_upm_calibrate_block(m2, f2, leaves.data(), in.data(), o2.data(),
                           leaves.size());
    double v1_maxd = 0.0;
    for (std::size_t i = 0; i < o1.size(); ++i)
        v1_maxd = std::max(v1_maxd, std::fabs(o1[i] - o2[i]));
    const bool v1 = v1_hash && v1_maxd == 0.0;
    std::printf("V1 default-bitwise: hash=%d maxd=%.3e %s\n", (int)v1_hash,
                v1_maxd, v1 ? "PASS" : "FAIL");
    if (!v1) ++fails;

    // ---- V2/V3：启用 ms ----
    P2UpmBuildConfig on = base;
    on.ms_enabled = 1;
    void* mm = nullptr;
    if (p2_upm_build_geo(obs.data(), obs.size(), nodes.data(), nodes.size(),
                         &on, &mm) != 0 || !mm) {
        std::printf("V2 build-on failed\n");
        p2_upm_close(m1);
        p2_upm_close(m2);
        return 1;
    }
    std::vector<double> cal(leaves.size());
    p2_upm_calibrate_block(mm, f2, leaves.data(), in.data(), cal.data(),
                           leaves.size());
    // 合成台阶 Δ：右半 ms 均值 − 左半 ms 均值（evaluate_c on−off）。
    // 求解前 r=y−M 的双重扣除会给出 Δ≈+b（整帧偏置被再扣一次）；
    // 求解后 r=y−M−C 只剩坡的低频尾：要求 0<Δ<kBias（明显小于偏置）。
    extern double p2_upm_evaluate_c(const void*, std::uint64_t, std::uint64_t);
    auto ms_at = [&](std::size_t k) {
        const double c_on =
            p2_upm_evaluate_c(mm, f2, nodes[k].leaf_ipix);
        const double c_off =
            p2_upm_evaluate_c(m1, f2, nodes[k].leaf_ipix);
        return c_on - c_off;
    };
    double dl = 0.0, dr = 0.0;
    int nl = 0, nr = 0;
    for (int gy = 0; gy < kGrid; ++gy)
        for (int gx = 0; gx < kGrid; ++gx) {
            const double v = ms_at((std::size_t)gy * kGrid + gx);
            if (gx < kGrid / 2) { dl += v; ++nl; }
            else { dr += v; ++nr; }
        }
    const double step = dr / nr - dl / nl;
    // 合成自门（非生产门）：实测 step≈5.35e-04 ≈ 生产门 5e-06 的约 107 倍
    // ⇒ 此处 PASS 仅为合成自门，生产门待 M42 真缝验证。
    const bool v2 = (step > 0.0) && (step < kBias);
    std::printf("V2 synth-step: bias=%.2f slope=%.2f D=%.6f (=%.2e; "
                "prod-gate 5e-06 x%.1f; synth-gate 0<D<bias) %s\n",
                kBias, kSlope, step, step, step / 5e-06,
                v2 ? "PASS" : "FAIL");
    if (!v2) ++fails;

    // V3：缝两侧对照窗 C-spread 不恶化。对照窗 = 缝两侧各两列
    //（gx∈{2,3,4,5} × 全 gy，共 32 点）：台阶横贯窗内 ⇒ off 基线必有
    // 方差，无零分母退化（旧右半窗 off spread 恒零、ratio 置零 PASS 的
    // 退化口径已退役）；同时窗内含坡区低频结构，检验 ms 不吃结构。
    // 容差门 ratio≤1.01（实测 ms 微纹波 ±3e-04 在 0.3 台阶上带来约 0.2%
    // spread 增量，判不恶化；C-spread 只作辅助）；任一 off spread 为零 ⇒
    // 退化窗判 FAIL（fail-closed，不得置零 PASS）。
    std::vector<double> coff, con;
    for (int gy = 0; gy < kGrid; ++gy)
        for (int gx = kGrid / 2 - 2; gx < kGrid / 2 + 2; ++gx) {
            const std::size_t k = (std::size_t)gy * kGrid + gx;
            coff.push_back(o1[k]);
            con.push_back(cal[k]);
        }
    auto spread = [](const std::vector<double>& v) {
        const double med = median_sorted(v);
        std::vector<double> d;
        for (double x : v) d.push_back(std::fabs(x - med));
        std::sort(d.begin(), d.end());
        const double m = median_sorted(d);
        const double p90 = d[std::min(d.size() - 1, (d.size() * 9) / 10)];
        return std::make_pair(m, p90);
    };
    const auto s_off = spread(coff), s_on = spread(con);
    // 零分母退化 ⇒ 判 FAIL（fail-closed；0.0 ratio 会伪装成 PASS）。
    bool v3 = false;
    double r_med = -1.0, r_p90 = -1.0;
    if (s_off.first > 0.0 && s_off.second > 0.0) {
        r_med = s_on.first / s_off.first;
        r_p90 = s_on.second / s_off.second;
        v3 = r_med <= 1.01 && r_p90 <= 1.01;
    } else {
        std::printf("V3 degenerate-window (off spread med=%.6f p90=%.6f) "
                    "FAIL\n", s_off.first, s_off.second);
    }
    std::printf("V3 seam-Cspread: med %.6f/%.6f=%.4f p90 %.6f/%.6f=%.4f %s\n",
                s_on.first, s_off.first, r_med, s_on.second, s_off.second,
                r_p90, v3 ? "PASS" : "FAIL");
    if (!v3) ++fails;

    // ---- V4：dense/sparse 逐位一致 ----
    const char* cache = "/tmp/ms_verify_dense.cache";
    std::remove(cache);
    bool v4 = false;
    double v4_maxd = -1.0;
    extern int p2_upm_materialize_dense_n(const void*, int, const char*, int);
    extern int p2_upm_dense_read_block(const void*, const char*,
                                      std::uint64_t, const std::uint64_t*,
                                      const double*, double*, std::uint64_t);
    if (p2_upm_materialize_dense_n(mm, 9, cache, 1) == 0) {
        std::vector<double> od(leaves.size());
        if (p2_upm_dense_read_block(mm, cache, f2, leaves.data(), in.data(),
                                    od.data(), leaves.size()) == 0) {
            v4_maxd = 0.0;
            for (std::size_t i = 0; i < cal.size(); ++i)
                v4_maxd = std::max(v4_maxd, std::fabs(cal[i] - od[i]));
            v4 = (v4_maxd == 0.0);
        }
    }
    std::printf("V4 dense-sparse: maxd=%.3e %s\n", v4_maxd,
                v4 ? "PASS" : "FAIL");
    if (!v4) ++fails;
    std::remove(cache);

    // ---- V5：配置门（缺省/透传/拒绝） ----
    // Stage2 model 面三键经 stage2_common 解析。缺省关闭；非法 fail-closed。
    {
        bool v5 = true;
        std::string err;
        {
            P2Stage2Config c{};
            const nlohmann::json j = nlohmann::json::parse(
                R"({"version":1,"inputs":{"hips":["a","b"]},)"
                R"("output":{"hips":"o"},"model":{}})");
            extern bool p2_stage2_parse_config(const nlohmann::json&,
                                              P2Stage2Config*, std::string*);
            extern P2UpmBuildConfig p2_stage2_make_upm_cfg(
                const P2Stage2Config&, int, const char*);
            if (!p2_stage2_parse_config(j, &c, &err)) v5 = false;
            v5 = v5 && (c.ms_enabled == 0 && c.ms_sigma_px == 16.0 &&
                        c.ms_thresh == 2.0);
            const P2UpmBuildConfig u = p2_stage2_make_upm_cfg(c, 9, nullptr);
            v5 = v5 && (u.ms_enabled == 0);
            std::printf("V5a default-off+passthrough: %s\n",
                        v5 ? "PASS" : "FAIL");
            if (!v5) ++fails;
        }
        // 五类拒绝自足（无补测旧账）：① enabled 越界 ② sigma 下探 8
        // ③ sigma 上探 32 ④ thresh 下限 1.0 ⑤ 未知键 ms_foo。
        const char* bad[] = {
            R"({"version":1,"inputs":{"hips":["a","b"]},)"
            R"("output":{"hips":"o"},"model":{"ms_enabled":2}})",
            R"({"version":1,"inputs":{"hips":["a","b"]},)"
            R"("output":{"hips":"o"},"model":{"ms_sigma_px":8}})",
            R"({"version":1,"inputs":{"hips":["a","b"]},)"
            R"("output":{"hips":"o"},"model":{"ms_sigma_px":32}})",
            R"({"version":1,"inputs":{"hips":["a","b"]},)"
            R"("output":{"hips":"o"},"model":{"ms_thresh":1.0}})",
            R"({"version":1,"inputs":{"hips":["a","b"]},)"
            R"("output":{"hips":"o"},"model":{"ms_foo":1}})",
        };
        for (const char* s : bad) {
            P2Stage2Config c{};
            const nlohmann::json j = nlohmann::json::parse(s);
            extern bool p2_stage2_parse_config(const nlohmann::json&,
                                              P2Stage2Config*, std::string*);
            if (p2_stage2_parse_config(j, &c, &err)) {
                v5 = false;
                std::printf("V5b reject FAIL (accepted): %s\n", s);
            }
        }
        std::printf("V5b reject-gates: %s\n", v5 ? "PASS" : "FAIL");
        if (!v5) ++fails;
    }

    p2_upm_close(m1);
    p2_upm_close(m2);
    p2_upm_close(mm);
    std::printf("MS-VERIFY %s (%d fails)\n", fails == 0 ? "PASS" : "FAIL",
                fails);
    return fails == 0 ? 0 : 1;
}
