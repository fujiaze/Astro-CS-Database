// eng/tests/unit/v6_p2_upm/v6_p2_upm_geo_diag_test.cpp
//
// UPM-KAPPA-UNIFY-01 回归锁（生产链 geo 侧）：唯一可辨识性判据 + 拟合诊断 +
// **不收敛只报警告、不阻塞**。
//
// 依据：
//   docs/science/PHASE2_UPM.md §7a（判据按矩阵谱自身定、不得是两个标定值；
//     不收敛必须对机器消费者可见）；docs/science/NOISE_MODEL.md §5d。
//   Andrae, Schulze-Hartung & Melchior (2010) arXiv:1012.3754 式(8)(9)：
//     dof = N − rank(X)，不是 N − P。
// 被测面：lib/algorithms/coverage/src/upm.cpp 的 p2_upm_build_geo 末端诊断 +
//         p2_upm_identifiability / p2_upm_warnings_json / p2_upm_save / p2_upm_open。
//
// 关键事实（本测试锁死）：生产 geo 求解器在 λs = smoothing_lambda = 0 时，联合法方程
// 在 (M_k, C_{f,k}) 参数化下按 control 块对角；块内定 gauge 后设计行 a_f = e_M + e_{C_f}
// 张满整块 ⇒ **每个有观测的块恒满秩**。若某 control 不被分量参考帧观测，则块内还残留
// 一个**纯 gauge** 零方向（M_k→M_k+c, C_{f,k}→C_{f,k}−c，不改变任何预测），
// 必须先定 gauge 再判——否则会把 gauge 自由度误报成"未约束方向"。
#include "astro/phase2/sampler.h"
#include "astro/phase2/upm.h"

#include "healpix_core.h"   // xy_to_nested_local（与生产同源）

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {
int g_fail = 0;
int g_total = 0;
void check(bool ok, const std::string& what) {
    ++g_total;
    if (!ok) { ++g_fail; std::printf("FAIL %s\n", what.c_str()); }
    else std::printf("ok   %s\n", what.c_str());
}

// 与生产同源的 fixture：单一 tile + 8×8 cell 格架，leaf 由 (tile, x, y) 算出
// （leaf_ipix 必须是 order(target_order+9)=18 的 NESTED 像素；随便填会让 geo 路径越界）。
constexpr int kGrid = 8;
constexpr int kCell = 64;
constexpr std::uint64_t kTile = 4;
constexpr std::uint64_t kFrameA = 10, kFrameB = 20, kFrameC = 30;

std::uint64_t leaf_of(int gx, int gy) {
    const std::uint64_t local = astrocs::healpix::xy_to_nested_local(
        static_cast<std::uint32_t>(gx * kCell + kCell / 2),
        static_cast<std::uint32_t>(gy * kCell + kCell / 2), 9u);
    return (kTile << 18u) + local;
}

// 观测：cell (gx,gy) 的 control。A 覆盖 gx<5，B 覆盖全部，C 覆盖 gx>=5。
// ⇒ 每个 control 恰好被 2 帧观测（gx<5: A+B；gx>=5: B+C，无单帧块）；
//    gx>=5 的 control 不含帧 A（= 分量参考帧，frame_id 最小）⇒ 需要块内 gauge 选择。
//    cell (7,7) 完全无观测（几何节点，harmonic continuation 填）。
bool covers(std::uint64_t f, int gx) {
    if (f == kFrameA) return gx < 5;
    if (f == kFrameB) return true;
    return gx >= 5;
}

std::vector<P2ControlObservation> make_obs() {
    std::vector<P2ControlObservation> v;
    const std::uint64_t frames[3] = {kFrameA, kFrameB, kFrameC};
    for (int gx = 0; gx < kGrid; ++gx) {
        for (int gy = 0; gy < kGrid; ++gy) {
            if (gx == kGrid - 1 && gy == kGrid - 1) continue;   // 无观测几何节点
            for (std::uint64_t f : frames) {
                if (!covers(f, gx)) continue;
                P2ControlObservation o{};
                o.frame_id = f;
                o.control_id = static_cast<std::uint64_t>(gx * kGrid + gy + 1);
                o.leaf_ipix = leaf_of(gx, gy);
                o.ra_deg = 0.1 + 0.001 * gx;
                o.dec_deg = -0.2 + 0.001 * gy;
                // 加性模型 y = M_k + C_{f,k}（M_k 随 cell 缓变，C 只与帧有关）
                o.value = 100.0 + 0.5 * gx - 0.25 * gy + 5.0 * (double)(f / 10);
                o.uncertainty = 1.0;
                o.snr = 50.0;
                o.control_variance = 1.0;
                o.control_ivar = 1.0;
                o.snr_available = 1;
                o.support = 1.0;
                o.quality_flags = 0;
                v.push_back(o);
            }
        }
    }
    return v;
}

std::vector<P2ControlNode> make_nodes() {
    std::vector<P2ControlNode> n;
    for (int gx = 0; gx < kGrid; ++gx) {
        for (int gy = 0; gy < kGrid; ++gy) {
            P2ControlNode x{};
            x.control_id = static_cast<std::uint64_t>(gx * kGrid + gy + 1);
            x.tile_ipix = kTile;
            x.gx = gx; x.gy = gy;
            x.ra_deg = 0.1 + 0.001 * gx;
            x.dec_deg = -0.2 + 0.001 * gy;
            x.leaf_ipix = leaf_of(gx, gy);
            n.push_back(x);
        }
    }
    return n;
}

P2UpmBuildConfig prod_cfg() {
    P2UpmBuildConfig c{};
    std::memset(&c, 0, sizeof(c));
    c.robust_loss = 0;
    c.snr_weight_mode = 0;
    c.huber_delta = 1.345;
    c.max_iterations = 100;
    c.tolerance = 1e-6;
    c.tolerance_relative = 1;
    c.gs_damping = 0.5;
    c.m_full_frame = 1;
    c.final_gauge = 1;
    c.sigma_floor = 1e-3;
    c.zero_anchor_weight = 1e-3;
    c.support_power = 1.0;
    c.use_ivar_weight = 1;
    c.control_reliability = 1.0;
    c.cpu_workers = 1;
    c.grid = 8;
    c.target_order = 0;   // 与生产同款：tile_ipix 即 control leaf 层级 order
    return c;
}

std::string read_all(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f.good()) return std::string();
    std::ostringstream os; os << f.rdbuf();
    return os.str();
}

// ---------------------------------------------------------------------------
// A. 判据读数（生产链 geo 侧）
// ---------------------------------------------------------------------------
int run_identifiability() {
    const std::vector<P2ControlObservation> obs = make_obs();
    const std::vector<P2ControlNode> nodes = make_nodes();
    P2UpmBuildConfig cfg = prod_cfg();
    void* m = nullptr;
    const int rc = p2_upm_build_geo(obs.data(), obs.size(), nodes.data(), nodes.size(), &cfg, &m);
    check(rc == 0, "A1 build_geo rc==0 (got " + std::to_string(rc) + ")");
    if (rc != 0 || !m) return g_fail;

    P2UpmIdentifiability d{};
    check(p2_upm_identifiability(m, &d) == 0, "A2 p2_upm_identifiability rc==0");
    const std::uint64_t n_observed_controls = (std::uint64_t)kGrid * kGrid - 1;   // (7,7) 无观测
    check(d.n_blocks == n_observed_controls,
          "A3 n_blocks == 63 (controls with >=1 observation), got " +
              std::to_string(d.n_blocks));
    check(d.n_unobserved_geometry_nodes == 1,
          "A4 unobserved geometry node counted separately (not in n_blocks), got " +
              std::to_string(d.n_unobserved_geometry_nodes));
    // 定 gauge 后每块参数数 == 观测帧数 ⇒ n_params == 观测对数 == n_obs
    check(d.n_params == obs.size(),
          "A5 n_params == sum_k #frames(k) == n_obs == " + std::to_string(obs.size()) +
              ", got " + std::to_string(d.n_params));
    check(d.rank_eff == d.n_params && d.n_unidentified == 0,
          "A6 gauge-pinned blocks are full rank: rank_eff == n_params == " +
              std::to_string(d.rank_eff));
    check(d.identifiable == 1, "A7 identifiable == 1 (the single verdict bit)");
    check(d.n_blocks_rank_deficient == 0, "A8 no genuinely rank-deficient block");
    // 帧 A（frame_id 最小）是分量参考帧；gx >= 5 的 control 不含帧 A ⇒ 需要块内 gauge。
    // gx=5,6,7 共 3 行 × 8 列 = 24 个，扣掉无观测的 (7,7) ⇒ 23 个。
    check(d.n_blocks_gauge_pinned == 23,
          "A9 blocks needing a block-local gauge choice == 23, got " +
              std::to_string(d.n_blocks_gauge_pinned));
    check(d.n_unidentified_raw == d.n_unidentified + d.n_blocks_gauge_pinned,
          "A10 n_unidentified_raw == n_unidentified + n_blocks_gauge_pinned (gauge bookkeeping)");
    check(d.n_blocks_single_frame == 0, "A11 no single-frame block in this fixture");
    check(d.rank_eff == obs.size(),
          "A11b rank(X) == n_obs (the additive model is saturated on this fixture)");
    check(d.coupling_assembled == 1, "A12 smoothing_lambda == 0 => exact block-diagonal");
    check(d.rank_rtol == 1e-10 && d.rank_rtol_effective >= d.rank_rtol,
          "A13 tau = FZ-AP2S-RANK-RTOL (1e-10) and tau_eff >= tau");
    check(d.kappa > 0.0 && std::isfinite(d.kappa),
          "A14 kappa(H_eq) finite > 0 = " + std::to_string(d.kappa));
    check(d.chi2 > 0.0, "A15 chi2 = sum (r/sigma_eff)^2 > 0 = " + std::to_string(d.chi2));
    check(std::fabs(d.dof_eff - ((double)obs.size() - (double)d.rank_eff)) < 1e-9,
          "A16 dof_eff == n_obs - rank(X) (Andrae 2010 eq.9)");
    check(d.chi2_red_defined == (d.dof_eff > 0.0 ? 1 : 0),
          "A17 chi2_red_defined consistent with dof_eff (dof<=0 => undefined, NOT 0)");
    if (d.chi2_red_defined == 0)
        check(std::isnan(d.chi2_red), "A18 undefined chi2_red is NaN (never a fake 0)");
    check(d.converged >= 0 && d.iterations >= 0 && d.objective >= 0.0,
          "A19 convergence diagnostics present (converged=" + std::to_string(d.converged) +
              ", iterations=" + std::to_string(d.iterations) + ")");
    std::printf("  INFO blocks=%llu params=%llu rank=%llu gauge_pinned=%llu raw_unident=%llu "
                "kappa=%.6g chi2=%.6g dof=%.6g converged=%d iters=%d\n",
                (unsigned long long)d.n_blocks, (unsigned long long)d.n_params,
                (unsigned long long)d.rank_eff, (unsigned long long)d.n_blocks_gauge_pinned,
                (unsigned long long)d.n_unidentified_raw, d.kappa, d.chi2, d.dof_eff,
                d.converged, d.iterations);
    p2_upm_close(m);
    return g_fail;
}

// ---------------------------------------------------------------------------
// B. 不收敛 ⇒ 警告但不阻塞（rc 仍为 0，警告必须能从产品 JSON 读出来）
// ---------------------------------------------------------------------------
int run_warning() {
    const std::vector<P2ControlObservation> obs = make_obs();
    const std::vector<P2ControlNode> nodes = make_nodes();
    P2UpmBuildConfig cfg = prod_cfg();
    cfg.max_iterations = 1;      // 强制"未收敛"
    cfg.tolerance = 1e-14;       // 1 次迭代不可能达到
    void* m = nullptr;
    const int rc = p2_upm_build_geo(obs.data(), obs.size(), nodes.data(), nodes.size(), &cfg, &m);
    check(rc == 0, "B1 non-convergence does NOT block the build: rc==0 (got " +
                       std::to_string(rc) + ")");
    if (rc != 0 || !m) return g_fail;

    std::uint64_t iters = 0; double obj = 0.0; int conv = -1;
    check(p2_upm_convergence(m, &iters, &obj, &conv) == 0, "B2 convergence accessor rc==0");
    check(conv != 1, "B3 fixture really did not converge (converged=" + std::to_string(conv) + ")");

    std::vector<char> buf(16384, 0);
    check(p2_upm_warnings_json(m, buf.data(), buf.size()) == 0, "B4 warnings_json rc==0");
    const std::string j(buf.data());
    check(j.find("P2-UPM-NOT-CONVERGED") != std::string::npos,
          "B5 warning code P2-UPM-NOT-CONVERGED present");
    check(j.find("\"upm_converged_warning\":true") != std::string::npos,
          "B6 upm_converged_warning == true");
    check(j.find("\"warning_codes\"") != std::string::npos, "B7 warning_codes[] present");
    for (const char* k : {"\"iterations\"", "\"objective\"", "\"rank_eff\"", "\"n_params\"",
                          "\"dof_eff\"", "\"converged\""}) {
        check(j.find(k) != std::string::npos,
              std::string("B8 warning payload carries ") + k);
    }
    check(j.find("P2-UPM-NOT-IDENTIFIABLE") == std::string::npos,
          "B9 no identifiability warning on a structurally identifiable fixture");
    check(j.find("\"severity\":\"warning\"") != std::string::npos,
          "B10 severity is 'warning' (not error/blocking)");
    char tiny[8];
    check(p2_upm_warnings_json(m, tiny, sizeof(tiny)) == 2,
          "B11 small buffer -> rc=2 (no silent truncation)");
    std::printf("  INFO warnings = %s\n", j.c_str());
    p2_upm_close(m);
    return g_fail;
}

// ---------------------------------------------------------------------------
// C. 警告与实际收敛状态一致（不冒充"没问题"，也不无中生有）
// ---------------------------------------------------------------------------
int run_warning_consistency() {
    const std::vector<P2ControlObservation> obs = make_obs();
    const std::vector<P2ControlNode> nodes = make_nodes();
    P2UpmBuildConfig cfg = prod_cfg();
    void* m = nullptr;
    const int rc = p2_upm_build_geo(obs.data(), obs.size(), nodes.data(), nodes.size(), &cfg, &m);
    check(rc == 0, "C1 build rc==0");
    if (rc != 0 || !m) return g_fail;
    std::uint64_t iters = 0; double obj = 0.0; int conv = -1;
    p2_upm_convergence(m, &iters, &obj, &conv);
    std::vector<char> buf(16384, 0);
    p2_upm_warnings_json(m, buf.data(), buf.size());
    const std::string j(buf.data());
    const bool has_conv_warn = j.find("P2-UPM-NOT-CONVERGED") != std::string::npos;
    check(has_conv_warn == (conv != 1),
          "C2 warning presence == (converged != 1)  [converged=" + std::to_string(conv) + "]");
    std::printf("  INFO converged=%d iters=%llu objective=%.6g warning=%d\n", conv,
                (unsigned long long)iters, obj, (int)has_conv_warn);
    p2_upm_close(m);
    return g_fail;
}

// ---------------------------------------------------------------------------
// D. 诊断随模型持久化（算法侧产品 artifact 自带判决与读数）
// ---------------------------------------------------------------------------
int run_persistence() {
    const std::vector<P2ControlObservation> obs = make_obs();
    const std::vector<P2ControlNode> nodes = make_nodes();
    P2UpmBuildConfig cfg = prod_cfg();
    cfg.max_iterations = 1;
    cfg.tolerance = 1e-14;
    void* m = nullptr;
    const int rc = p2_upm_build_geo(obs.data(), obs.size(), nodes.data(), nodes.size(), &cfg, &m);
    check(rc == 0, "D1 build rc==0");
    if (rc != 0 || !m) return g_fail;
    P2UpmIdentifiability a{};
    p2_upm_identifiability(m, &a);

    const std::string path = "/tmp/upm_geo_diag_roundtrip.json";
    check(p2_upm_save(m, path.c_str()) == 0, "D2 save rc==0");
    const std::string txt = read_all(path);
    check(!txt.empty(), "D3 model file written");
    for (const char* k : {"\"identifiability\"", "\"rank_eff\"", "\"n_blocks_gauge_pinned\"",
                          "\"n_unidentified_raw\"", "\"identifiable\"", "\"chi2_red_defined\"",
                          "\"dof_eff\"", "\"iterations\"", "\"converged\""}) {
        check(txt.find(k) != std::string::npos,
              std::string("D4 model file carries ") + k + " (diagnostics are persisted)");
    }
    void* m2 = nullptr;
    check(p2_upm_open(path.c_str(), &m2) == 0, "D5 open rc==0");
    if (m2) {
        P2UpmIdentifiability b{};
        check(p2_upm_identifiability(m2, &b) == 0, "D6 diagnostics survive open");
        check(b.rank_eff == a.rank_eff && b.n_params == a.n_params &&
                  b.n_blocks == a.n_blocks && b.identifiable == a.identifiable &&
                  b.n_blocks_gauge_pinned == a.n_blocks_gauge_pinned,
              "D7 roundtrip preserves the criterion readings");
        check(b.chi2_red_defined == a.chi2_red_defined, "D8 chi2_red_defined roundtrips");
        p2_upm_close(m2);
    }
    std::remove(path.c_str());
    check(p2_upm_identifiability(nullptr, &a) == 1, "D9 null model -> rc=1 (named failure)");
    p2_upm_close(m);
    return g_fail;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string mode = (argc > 1) ? argv[1] : "identifiability";
    if (mode == "identifiability") run_identifiability();
    else if (mode == "warning") run_warning();
    else if (mode == "warning_consistency") run_warning_consistency();
    else if (mode == "persistence") run_persistence();
    else { std::printf("unknown mode: %s\n", mode.c_str()); return 2; }
    std::printf("[%s] checks=%d failures=%d\n", mode.c_str(), g_total, g_fail);
    return g_fail;
}
