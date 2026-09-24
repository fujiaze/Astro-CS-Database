// eng/tests/unit/v6_p2_sky/v6_p2_sky_geometry_test.cpp
//
// CHAIN-WIRE-ADAPT-01 / W4+W5：天光面节点间距所需的**输入几何**由产品自身导出
// （docs/science/PHASE2_UPM.md §7a：节点间距必须由输入几何导出，禁止标定常数）。
//
// 被测面 = lib/algorithms/coverage/include/astro/phase2/stage2_common.h 的
//   p2_sky_plane_geometry_from_controls（生产编排 module_adapters.cpp 的 upm-fit
//   节点与工具 astrocs-stage2 的**同一实现路径**），并与冻结的
//   p2_sky_plane_derive_node_spacing（sky_plane.h）对接自洽。
//
// 判据能红能绿：
//   正例 = 合成 2 指向 × 3 帧（含抖动）的产品几何 ⇒ 导出量必须复现构造值，
//          且 h_deg 与源像素尺度无关（h_px 才随它变）；
//   负例 = 缺控制点 / 缺像素尺度 / 空观测集 ⇒ 显式失败（不回退任何常数）。
#include "astro/phase2/stage2_common.h"
#include "astro/phase2/sky_plane.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_fail = 0;
int g_total = 0;
void check(bool ok, const std::string& what) {
    ++g_total;
    if (!ok) { ++g_fail; std::printf("  FAIL: %s\n", what.c_str()); }
    else std::printf("  ok  : %s\n", what.c_str());
}

constexpr double kPi = 3.14159265358979323846;
double rad(double deg) { return deg * kPi / 180.0; }

// 合成产品：两个指向（中心相距 1.0 度），各 3 帧（帧间抖动 0.002 度）；
// 控制点是**天球上共享的**网格（pitch 0.05 度），故两个指向在重叠区共享控制点。
struct SyntheticProduct {
    std::vector<double> obs_ra, obs_dec;
    std::vector<std::uint64_t> obs_fid, obs_cid;
    std::vector<double> ctrl_ra, ctrl_dec;
    std::vector<std::uint64_t> ctrl_tile;
};

SyntheticProduct make_product(bool single_pointing) {
    SyntheticProduct p;
    const double dec0 = 20.0;
    const double pitch = 0.05;                       // 控制点角间距（度）
    const double dra = pitch / std::cos(rad(dec0));  // RA 步长（真角距 = pitch）
    const double half_true = 0.7;                    // 每帧半宽（真角距，度）
    const int n_col = 14;                            // |m| <= 14 ⇒ ±0.745 度 RA
    const int n_row = 14;                            // |j| <= 14 ⇒ ±0.70 度 Dec
    const double centers[2] = {10.0, 10.0 + 1.0 / std::cos(rad(dec0))};
    const int n_point = single_pointing ? 1 : 2;
    const int n_frames_per_point = 3;
    const double dither[3] = {-0.002, 0.0, 0.002};

    // 控制点网格（天球共享；tile 按 RA 分带，模拟 HEALPix 叶 tile 的分组）。
    // 网格对称于第一个指向中心 ⇒ 帧中心的球面均值严格等于指向中心。
    std::vector<double> grid_ra, grid_dec;
    for (int m = -2 * n_col; m <= 4 * n_col; ++m)
        for (int j = -n_row; j <= n_row; ++j) {
            grid_ra.push_back(centers[0] + m * dra);
            grid_dec.push_back(dec0 + j * pitch);
        }
    for (std::size_t i = 0; i < grid_ra.size(); ++i) {
        p.ctrl_ra.push_back(grid_ra[i]);
        p.ctrl_dec.push_back(grid_dec[i]);
        p.ctrl_tile.push_back(static_cast<std::uint64_t>(std::floor(
            (grid_ra[i] - centers[0]) / (5.0 * dra) + 100.0)));
    }
    // 观测：指向 c 的第 k 帧看到窗口内的控制点。窗口判据用**整数格点下标**
    // （不是浮点角距比较）——后者在窗口边界上会因浮点舍入漏掉一整列，
    // 使构造的对称性被破坏（那会让「指向间距」的期望值不再是构造值）。
    const int m_center[2] = {0, 20};   // centers[1] - centers[0] = 20*dra
    for (int c = 0; c < n_point; ++c) {
        for (int k = 0; k < n_frames_per_point; ++k) {
            const std::uint64_t fid =
                static_cast<std::uint64_t>(1000 * (c + 1) + k + 1);
            for (int m = -2 * n_col; m <= 4 * n_col; ++m) {
                if (std::abs(m - m_center[c]) > n_col) continue;
                for (int j = -n_row; j <= n_row; ++j) {
                    const std::size_t i = static_cast<std::size_t>(
                        (m + 2 * n_col) * (2 * n_row + 1) + (j + n_row));
                    p.obs_ra.push_back(grid_ra[i] + dither[k]);
                    p.obs_dec.push_back(grid_dec[i] + dither[k]);
                    p.obs_fid.push_back(fid);
                    p.obs_cid.push_back(static_cast<std::uint64_t>(i));
                }
            }
        }
    }
    (void)half_true;
    return p;
}

int derive(const SyntheticProduct& p, double px_scale, P2SkyPlaneGeometry* out,
           P2SkyPlaneGeometryProvenance* prov, std::string* err) {
    P2SkyPlaneGeometryInputs gi{};
    gi.obs_ra_deg = p.obs_ra.data();
    gi.obs_dec_deg = p.obs_dec.data();
    gi.obs_frame_id = p.obs_fid.data();
    gi.obs_control_id = p.obs_cid.data();
    gi.n_obs = p.obs_ra.size();
    gi.ctrl_ra_deg = p.ctrl_ra.data();
    gi.ctrl_dec_deg = p.ctrl_dec.data();
    gi.ctrl_tile_ipix = p.ctrl_tile.data();
    gi.n_ctrl = p.ctrl_ra.size();
    gi.pixel_scale_arcsec = px_scale;
    char e[512] = {0};
    const int rc = p2_sky_plane_geometry_from_controls(gi, out, prov, e, sizeof(e));
    if (err) *err = e;
    return rc;
}

// 正例：导出量必须复现构造几何，且与冻结的 derive_node_spacing 自洽。
void test_geometry_positive() {
    const SyntheticProduct p = make_product(false);
    P2SkyPlaneGeometry g{};
    P2SkyPlaneGeometryProvenance pv{};
    std::string err;
    check(derive(p, 0.989016, &g, &pv, &err) == 0, "2 指向合成产品必须导出成功: " + err);
    check(pv.n_pointings == 2,
          "自校准切分必须把同指向的抖动帧并成一族（n_pointings=" +
              std::to_string(pv.n_pointings) + "，构造值 2）");
    // 容差 1e-3：构造用「Δra = 1.0/cos(dec)」摆放中心，其**大圆**距离与 1.0 度
    // 相差 O(sep²) 的相对修正（实测 5e-6）；1e-3 比该修正高两个数量级，
    // 又比本用例的判别尺度（重叠带 0.4 vs 指向间距 1.0）低三个数量级。
    check(std::fabs(g.pointing_spacing_deg - 1.0) < 1e-3,
          "指向间距必须复现构造值 1.0 度（得到 " +
              std::to_string(g.pointing_spacing_deg) + "）");
    check(std::fabs(g.sample_pitch_deg - 0.05) < 0.02 * 0.05,
          "控制点角间距必须复现构造值 0.05 度（得到 " +
              std::to_string(g.sample_pitch_deg) + "）");
    check(g.overlap_band_width_deg > 0.30 && g.overlap_band_width_deg < 0.45,
          "重叠带宽度必须落在构造的重叠区宽度附近（得到 " +
              std::to_string(g.overlap_band_width_deg) + "，构造 0.4）");
    check(pv.overlap_band_full_deg >= g.overlap_band_width_deg,
          "全幅宽度必须不小于稳健宽度（full=" +
              std::to_string(pv.overlap_band_full_deg) + " trimmed=" +
              std::to_string(g.overlap_band_width_deg) + "）");
    check(pv.n_overlap_pairs >= 1, "必须至少有一对共享控制点的指向对");

    // 与冻结的导出函数自洽：upper = min(重叠带, 指向间距)/2
    P2SkyPlaneNodeSpacing ns{};
    check(p2_sky_plane_derive_node_spacing(&g, &ns, nullptr, 0) == P2_SKY_NODE_SPACING_OK,
          "导出的几何必须能过冻结的 derive_node_spacing");
    const double want_upper =
        0.5 * std::min(g.overlap_band_width_deg, g.pointing_spacing_deg);
    check(std::fabs(ns.upper_deg - want_upper) < 1e-12,
          "h 上界必须严格等于 min(重叠带, 指向间距)/2（7a 规则 1）");

    // h_deg 与源像素尺度无关；h_px 与之成反比（换仪器自洽性）
    P2SkyPlaneGeometry g2{};
    P2SkyPlaneGeometryProvenance pv2{};
    check(derive(p, 0.494508, &g2, &pv2, &err) == 0, "半像素尺度下同样必须导出成功");
    check(std::fabs(g2.overlap_band_width_deg - g.overlap_band_width_deg) < 1e-12 &&
              std::fabs(g2.pointing_spacing_deg - g.pointing_spacing_deg) < 1e-12 &&
              std::fabs(g2.sample_pitch_deg - g.sample_pitch_deg) < 1e-12,
          "角量导出与源像素尺度无关（换仪器后度数不变）");
    P2SkyPlaneNodeSpacing ns2{};
    check(p2_sky_plane_derive_node_spacing(&g2, &ns2, nullptr, 0) == P2_SKY_NODE_SPACING_OK,
          "半像素尺度几何必须能过 derive_node_spacing");
    check(std::fabs(ns2.upper_deg - ns.upper_deg) < 1e-12,
          "h_deg 不随像素尺度变化");
    check(std::fabs(ns2.node_spacing_px - 2.0 * ns.node_spacing_px) < 1e-6 * ns2.node_spacing_px,
          "h_px 与像素尺度成反比（像素尺度减半 ⇒ 像素数加倍）");
}

// 单一指向：自校准判据不武装 ⇒ 公共控制区 = 整幅足迹（由数据算出的长度）
void test_geometry_single_pointing() {
    const SyntheticProduct p = make_product(true);
    P2SkyPlaneGeometry g{};
    P2SkyPlaneGeometryProvenance pv{};
    std::string err;
    check(derive(p, 0.989016, &g, &pv, &err) == 0,
          "单一指向产品必须仍能导出（退化路径不得回退常数）: " + err);
    check(pv.n_pointings == 1, "单一指向必须被判为 1 个指向族");
    check(g.pointing_spacing_deg > 1.3 && g.pointing_spacing_deg < 1.5,
          "单一指向时指向间距退化为帧角尺寸（得到 " +
              std::to_string(g.pointing_spacing_deg) + "，构造 1.4）");
    check(std::fabs(g.overlap_band_width_deg - g.pointing_spacing_deg) < 1e-12,
          "单一指向时重叠带 = 帧角尺寸（同一物理含义）");
}

// 负例：任一必需输入缺失 ⇒ 显式失败（err 点名），**不得**回退标定常数
void test_geometry_negative() {
    const SyntheticProduct p = make_product(false);
    P2SkyPlaneGeometry g{};
    P2SkyPlaneGeometryProvenance pv{};
    std::string err;

    // 注意：先调用再判据（函数实参求值次序未指定，否则诊断文本会读到调用前的旧值）
    SyntheticProduct no_ctrl = p;
    no_ctrl.ctrl_ra.clear(); no_ctrl.ctrl_dec.clear(); no_ctrl.ctrl_tile.clear();
    int rc = derive(no_ctrl, 0.989016, &g, &pv, &err);
    check(rc != 0 && err.find("control") != std::string::npos,
          "缺控制点集合必须显式失败并点名（rc=" + std::to_string(rc) +
              " err='" + err + "'）");

    err.clear();
    rc = derive(p, 0.0, &g, &pv, &err);
    check(rc != 0 && err.find("pixel scale") != std::string::npos,
          "缺源像素尺度必须显式失败并点名（rc=" + std::to_string(rc) +
              " err='" + err + "'）");

    SyntheticProduct no_obs = p;
    no_obs.obs_ra.clear(); no_obs.obs_dec.clear(); no_obs.obs_fid.clear();
    no_obs.obs_cid.clear();
    err.clear();
    rc = derive(no_obs, 0.989016, &g, &pv, &err);
    check(rc != 0 && err.find("observation") != std::string::npos,
          "空观测集必须显式失败并点名（rc=" + std::to_string(rc) +
              " err='" + err + "'）");

    // 单帧：无法分离指向 ⇒ 显式失败（不得凭一个帧猜几何）
    SyntheticProduct one = p;
    std::vector<std::size_t> keep;
    for (std::size_t i = 0; i < one.obs_fid.size(); ++i)
        if (one.obs_fid[i] == one.obs_fid[0]) keep.push_back(i);
    SyntheticProduct single;
    for (std::size_t i : keep) {
        single.obs_ra.push_back(one.obs_ra[i]);
        single.obs_dec.push_back(one.obs_dec[i]);
        single.obs_fid.push_back(one.obs_fid[i]);
        single.obs_cid.push_back(one.obs_cid[i]);
    }
    single.ctrl_ra = one.ctrl_ra; single.ctrl_dec = one.ctrl_dec;
    single.ctrl_tile = one.ctrl_tile;
    err.clear();
    rc = derive(single, 0.989016, &g, &pv, &err);
    check(rc != 0 && err.find("2 distinct frames") != std::string::npos,
          "单帧产品必须显式失败（rc=" + std::to_string(rc) + " err='" + err + "'）");
}

}  // namespace

int main(int argc, char** argv) {
    const std::string t = argc > 1 ? argv[1] : "all";
    if (t == "geometry_positive" || t == "all") test_geometry_positive();
    if (t == "geometry_single_pointing" || t == "all") test_geometry_single_pointing();
    if (t == "geometry_negative" || t == "all") test_geometry_negative();
    if (g_fail) { std::printf("RESULT: FAIL (%d/%d checks)\n", g_fail, g_total); return 1; }
    std::printf("RESULT: PASS (%d checks)\n", g_total);
    return 0;
}
