// lib/phase2/src/upm.cpp — Phase2 UnifiedPhotometricModel CPU reference
//

// - 空间 additive UPM：M(p_k) latent reference + C_i(p) 每帧空间校正场；
// - 观测模型 y_ik = M(p_k) + C_i(p_k) + noise；
// - 控制拓扑 = coverage union 上的 HEALPix control cells（8×8/tile 网格），
// basis/topology 只由 geometry/coverage/配置决定（与 SNR 解耦）；
// - 图拉普拉斯平滑 lambda_s 真正进入联合求解；
// - 观测权重 raw_w = quality_factor * control_ivar（ 冻结，
// SCI-UPM-WEIGHT-001；control_ivar = 1/control_variance，
// ALG-UPM-CONTROL-IVAR-001），并在每个 control node 内归一；
// - ivar 状态机（DATA-UPM-CONTROL-UNC-001 / SCI-UPM-WEIGHT-001）：
// use_ivar_weight=1 时 control_ivar≤0/非有限→p2_upm_raw_weight rc=2→build rc=2
// 显式拒绝（禁止静默回退 legacy）；use_ivar_weight=0 仅 ablation/诊断
// （SNR-015）；config weight_mode auto/ivar→use_ivar_weight=1（默认）
// 等价于 CONFIG_SCHEMA weight_mode(auto) + stage2_common wm=="auto"||"ivar"；
// - legacy snr^2/(1+snr^2)/unc^2 仅 ablation/诊断（use_ivar_weight=0，
// SNR-015），禁止进入 production science 路径；
// - 弱零校正锚 lambda_0：单覆盖节点由同帧其他节点 + smoothness 延拓；
// - gauge：参考帧（最小内容稳定 frame_id）C=0，输入顺序无关；
// - calibrate_block 真正使用 leaf_ipix 查找所在 control cell；
// - 断开分量各自 gauge（不虚构跨组件约束）。
// - 持久化绑定 SCI-UPM-PERSIST-001/ALG-UPM-FRAME-BIND-001/DATA-UPM-MODEL-001：
//   parameter_rows[index]↔frame_id_by_index[index] 同长无重复；save 经
//   frames[]+C[] 行序显式持久化（原子写 aio_upm_write_sparse，ENG-IO-001），
//   open 强校验 frames 存在/数组/无重复/类型非法一律拒绝，save→open 后
//   frame_id→theta 绑定不变，禁止从有序容器遍历重建。
#include "astro/phase2/upm.h"
#include "astro/phase2/sampler.h"

#include "crypto/sha256.h"
#include "healpix/healpix_core.h"

extern "C" {
#include "aio_upm.h"
}

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <nlohmann/json.hpp>
#include <sstream>
#include <set>
#include <string>
#include <vector>

#if defined(P2_ENABLE_OPENMP) && defined(_OPENMP)
#include <omp.h>
#include <atomic>
#include <thread>
#endif

namespace {

struct ControlNode {
    std::vector<std::uint64_t> obs_idx;  // 参与该节点的观测
    double M{0.0};                       // latent unified reference（待求）
    std::uint64_t leaf_ipix{0};          // cell 中心 leaf（NESTED order L）
    std::uint64_t tile_ipix{0};          // target_order tile
    int gx{0}, gy{0};                    // cell 网格坐标
    double ra_deg{0.0};
    double dec_deg{0.0};
    std::uint64_t id{0};                 // 外部 control_id
    double reliability{1.0};             // control_reliability_k
};

struct Model {
    P2ModelInfo info;
    P2UpmBuildConfig cfg;
    std::vector<ControlNode> controls;
    std::map<std::uint64_t, std::size_t> control_by_id;
    std::map<std::uint64_t, std::size_t> frame_index;
    std::vector<std::uint64_t> frame_id_by_index;   // index -> stable frame_id
    std::vector<std::vector<double>> C;  // [frame][control] 空间校正
    std::vector<std::vector<std::size_t>> adj;  // control 邻接（tile 内网格）
    std::vector<std::vector<double>> obs_w;     // 最终每轮权重缓存
    std::map<std::pair<std::uint64_t, std::pair<int, int>>, std::size_t>
        cell_index;                      // (tile, gx, gy) -> control
    // 每 tile 覆盖 cell 范围（外推锚点只引用真实存在的 cell）
    std::map<std::uint64_t, std::pair<int, int>> tile_gx_bounds;
    std::map<std::uint64_t, std::pair<int, int>> tile_gy_bounds;
    // 求解前连通分量（frame-control 二分图）；每分量独立 gauge
    std::vector<std::size_t> control_component;   // control -> component
    std::vector<std::size_t> frame_component;     // frame index -> component
    std::vector<std::uint64_t> component_ref_frame;  // 每分量最小 frame_id
    std::size_t component_count_solve{1};
    std::string input_manifest_hash;
    int grid{8};                         // cells per tile side
    int cell_side{64};
    // 诊断
    int iterations{0};
    double objective{0.0};
    std::size_t component_count{1};
    // 几何/无观测节点独立统计（不混入数据分量）
    std::size_t geometry_component_count{1};
    std::uint64_t unobserved_geometry_nodes{0};
};

// evaluate_C —— centered bilinear basis（同一科学求值
// 语义，sparse/dense 共用）。控制观测与系数节点位于 **cell 中心**
// （gx*cell+cell/2, gy*cell+cell/2）；旧实现把节点值当作 cell 角点
// 求值，导致恢复场相对叶坐标产生半 cell（cell/2）相位偏移，且
// evaluate(观测中心) != 节点系数（basis 坐标不自洽）。本实现按叶
// 位置两侧最近的 cell 中心双线性插值：
// - evaluate(center) == C[cell]（坐标自洽）；
// - 线性场在正确相位恢复（half-cell phase truth）；
// - tile 最外缘线性外推：v 低于首中心用前两个 centered nodes，
// 高于末中心用最后两个 centered nodes（双轴 corner 由双线性公式
// 自然外推）；不再 clamp 成常数。
// - 外推锚点限制在本 tile **真实覆盖**的 cell 范围内；缺失 cell
// （部分覆盖/单 cell）不会引用为 0。
double evaluate_c_field(const Model* m, std::size_t frame_idx,
                        std::uint64_t tile, int x, int y) {
    const int cell = m->cell_side;
    const int half = cell / 2;
    // v 轴（0..512）：返回定义线性段的两个 cell 中心坐标。
    // 内部：夹住 v 的两个相邻中心；边缘：前两个/后两个中心（外推段）。
    auto gx_bounds = [&]() -> std::pair<int, int> {
        const auto it = m->tile_gx_bounds.find(tile);
        if (it != m->tile_gx_bounds.end()) return it->second;
        return {0, m->grid - 1};
    };
    auto gy_bounds = [&]() -> std::pair<int, int> {
        const auto it = m->tile_gy_bounds.find(tile);
        if (it != m->tile_gy_bounds.end()) return it->second;
        return {0, m->grid - 1};
    };
    auto axis = [&](int v, int* c0, int* c1, const std::pair<int, int>& b) {
        const int gmin = b.first;
        const int gmax = b.second;
        const int lo = gmin * cell + half;
        const int hi = gmax * cell + half;
        if (gmin == gmax) {
            *c0 = *c1 = lo;               // 单列覆盖 → 常数（无外推数据）
            return;
        }
        if (v <= lo) {
            *c0 = lo;
            *c1 = lo + cell;              // 前两个 covered nodes
        } else if (v >= hi) {
            *c0 = hi - cell;
            *c1 = hi;                     // 最后两个 covered nodes
        } else {
            const int idx = std::clamp(v / cell, gmin, gmax);
            const int cc = idx * cell + half;
            if (v <= cc) {
                *c0 = cc - cell;
                *c1 = cc;
            } else {
                *c0 = cc;
                *c1 = cc + cell;
            }
        }
    };
    int x0 = 0, x1 = 0, y0 = 0, y1 = 0;
    axis(x, &x0, &x1, gx_bounds());
    axis(y, &y0, &y1, gy_bounds());
    auto at = [&](int cx, int cy) -> double {
        const int gxi = std::clamp((cx - half) / cell, 0, m->grid - 1);
        const int gyi = std::clamp((cy - half) / cell, 0, m->grid - 1);
        const auto key = std::make_pair(tile, std::make_pair(gxi, gyi));
        const auto it = m->cell_index.find(key);
        if (it == m->cell_index.end()) return 0.0;
        return m->C[frame_idx][it->second];
    };
    const double c00 = at(x0, y0);
    const double c10 = at(x1, y0);
    const double c01 = at(x0, y1);
    const double c11 = at(x1, y1);
    const double tx = (x1 != x0) ? (double)(x - x0) / (double)(x1 - x0) : 0.0;
    const double ty = (y1 != y0) ? (double)(y - y0) / (double)(y1 - y0) : 0.0;
    const double top = c00 + tx * (c10 - c00);
    const double bot = c01 + tx * (c11 - c01);
    return top + ty * (bot - top);
}

inline double quality_factor(std::uint32_t flags, int mode) {
    (void)mode;
    // Phase1 quality_flags：1=PSF_OK 2=saturated 4=has_saturated
    // 8=photo_matched 16=photo_rejected
    if (flags & 16u) return 0.0;          // photo_rejected -> 不可信
    if (flags & 2u) return 0.1;           // saturated -> 低可信
    if (flags & 1u) return 1.0;           // PSF_OK
    if (flags == 0u) return 0.5;          // 未知 -> 中性偏低（禁止默认为高权）
    return 0.5;
}

inline std::uint64_t leaf_to_tile(std::uint64_t leaf, int shift) {
    return leaf >> (2u * (unsigned)shift);
}

inline std::uint64_t leaf_local(std::uint64_t leaf, int shift) {
    return leaf & ((1ULL << (2u * (unsigned)shift)) - 1ULL);
}

inline double huber_rho(double r, double d) {
    const double a = std::fabs(r);
    if (a <= d) return 0.5 * r * r;
    return d * (a - 0.5 * d);
}

inline double huber_w(double r, double d) {
    const double a = std::fabs(r);
    if (a <= d) return 1.0;
    return d / a;
}

} // namespace

extern "C" {

static int build_impl(const P2ControlObservation* obs, std::uint64_t n_obs,
                      const P2ControlNode* nodes, std::uint64_t n_nodes,
                      const P2UpmBuildConfig* cfg_in, void** out_model) {
    if (out_model == nullptr || obs == nullptr || n_obs == 0) return 1;
    // 无观测几何节点的 component sentinel（不参与数据图/gauge）
    const std::size_t kNoData = ~std::size_t(0);
    P2UpmBuildConfig cfg;
    if (cfg_in != nullptr) {
        cfg = *cfg_in;
    } else {
        cfg.robust_loss = 0;
        cfg.snr_weight_mode = 0;
        cfg.huber_delta = 1.345;
        cfg.smoothing_lambda = 0.0;
        cfg.zero_anchor_weight = 1e-3;
        cfg.max_iterations = 100;
        cfg.tolerance = 1e-6;
        cfg.target_order = -1;
        cfg.sigma_floor = 1e-3;
        cfg.support_power = 1.0;
        cfg.quality_mode = 0;
        cfg.use_ivar_weight = 1;   // ivar 科学权重默认开启
        cfg.control_reliability = 1.0;
        cfg.cpu_workers = 0;       // CON-005: 默认 0(auto) => 默认构建 P2_ENABLE_OPENMP=OFF 串行
    }
    if (cfg.huber_delta <= 0.0) cfg.huber_delta = 1.345;
    if (cfg.max_iterations <= 0) cfg.max_iterations = 100;
    if (cfg.sigma_floor <= 0.0) cfg.sigma_floor = 1e-3;
    if (cfg.support_power < 0.0) cfg.support_power = 1.0;
    // use_ivar_weight 默认 1 (仅显式 0 关闭)
    // (cfg 拷贝后此处不强制, 保持调用方意图)
    if (cfg.control_reliability <= 0.0) cfg.control_reliability = 1.0;
    if (cfg.zero_anchor_weight < 0.0) cfg.zero_anchor_weight = 1e-3;
    if (cfg.smoothing_lambda < 0.0) cfg.smoothing_lambda = 0.0;
    if (cfg.target_order < 0) {
        // 空间 UPM 必须知道 control leaf 层级（order = target+9）
        return 1;
    }

    Model* m = new Model();
    m->cfg = cfg;
    if (cfg.input_manifest_hash != nullptr)
        m->input_manifest_hash = cfg.input_manifest_hash;
    m->info.version = 2;              // 空间 UPM
    m->info.precision = 1;  // fp64 reference
    m->info.observation_count = n_obs;
    m->info.target_order = (std::uint32_t)cfg.target_order;
    m->grid = 8;
    m->cell_side = 512 / m->grid;
    const int tile_shift = 9;         // leaf order = target+9 -> tile shift 18/2

    // 收集 control（按 (tile,gx,gy) cell 去重）/ frame 索引
    // nodes 提供全 coverage 几何（含单帧区）；obs 提供数据项。
    std::set<std::uint64_t> frame_ids;
    auto add_control = [&](std::uint64_t cid, std::uint64_t tile,
                           int gx, int gy, double ra, double dec,
                           std::uint64_t leaf) {
        const auto key = std::make_pair(tile, std::make_pair(gx, gy));
        auto it = m->cell_index.find(key);
        if (it == m->cell_index.end()) {
            const std::size_t idx = m->controls.size();
            m->controls.push_back(ControlNode{});
            m->controls.back().leaf_ipix = leaf;
            m->controls.back().ra_deg = ra;
            m->controls.back().dec_deg = dec;
            m->controls.back().id = cid;
            m->controls.back().tile_ipix = tile;
            m->controls.back().gx = gx;
            m->controls.back().gy = gy;
            m->controls.back().reliability =
                std::max(0.0, cfg.control_reliability);
            m->cell_index[key] = idx;
            m->control_by_id[cid] = idx;
            it = m->cell_index.find(key);
        }
        return it->second;
    };
    if (nodes && n_nodes > 0) {
        for (std::uint64_t i = 0; i < n_nodes; ++i) {
            const P2ControlNode& nd = nodes[i];
            add_control(nd.control_id, nd.tile_ipix, nd.gx, nd.gy,
                        nd.ra_deg, nd.dec_deg, nd.leaf_ipix);
        }
    }
    for (std::uint64_t i = 0; i < n_obs; ++i) {
        const auto& o = obs[i];
        const std::uint64_t tile = leaf_to_tile(o.leaf_ipix, tile_shift);
        const std::uint64_t local = leaf_local(o.leaf_ipix, tile_shift);
        std::uint32_t x = 0, y = 0;
        astrocs::healpix::nested_local_to_xy(local, (std::uint32_t)tile_shift,
                                             x, y);
        const int gx = (int)(x / (std::uint32_t)m->cell_side);
        const int gy = (int)(y / (std::uint32_t)m->cell_side);
        if (!nodes) {
            add_control(o.control_id, tile, gx, gy, o.ra_deg, o.dec_deg,
                        o.leaf_ipix);
        }
        const auto key = std::make_pair(tile, std::make_pair(gx, gy));
        const auto it = m->cell_index.find(key);
        if (it == m->cell_index.end()) continue;
        m->controls[it->second].obs_idx.push_back(i);
        frame_ids.insert(o.frame_id);
    }
    // 移除的跨 tile 边界节点合并（proximity-alias）。
    // 边界两侧 cell 中心是**不同 sky 位置**，共享系数会强制 seam 两侧
    // C 恒等（jump=0），导致跨 tile 真实梯度无法恢复。现在边界节点保持
    // 独立系数，跨 tile 连续性由 平滑邻接（弱先验）提供：
    // 平滑场 seam 跳变受拟合噪声约束，非零梯度场的恢复 delta 跟随
    // truth delta。save/open 的 cell_index 退化为恒等映射（向后兼容读取）。
    m->info.control_count = m->controls.size();
    const std::size_t K = m->controls.size();
    // 每 tile 覆盖 cell 范围（evaluate 外推锚点用）
    for (const auto& kv : m->cell_index) {
        const std::uint64_t t = kv.first.first;
        const int gx = kv.first.second.first;
        const int gy = kv.first.second.second;
        auto it = m->tile_gx_bounds.find(t);
        if (it == m->tile_gx_bounds.end()) {
            m->tile_gx_bounds[t] = {gx, gx};
            m->tile_gy_bounds[t] = {gy, gy};
        } else {
            it->second.first = std::min(it->second.first, gx);
            it->second.second = std::max(it->second.second, gx);
            auto& gyb = m->tile_gy_bounds[t];
            gyb.first = std::min(gyb.first, gy);
            gyb.second = std::max(gyb.second, gy);
        }
    }
    for (std::uint64_t f : frame_ids) {
        if (m->frame_index.find(f) == m->frame_index.end()) {
            const std::size_t idx = m->frame_index.size();
            m->frame_index[f] = idx;
            m->frame_id_by_index.push_back(f);
        }
    }
    const std::size_t F = m->frame_index.size();
    m->C.assign(F, std::vector<double>(K, 0.0));
    m->obs_w.assign(F, std::vector<double>(K, 0.0));

    // 邻接图：网格邻接经 cell_index 全键遍历（含跨 tile 合并别名键，
    // 使合并节点两侧邻居都连到同一系数），随后跨 tile 边界平滑链接。
    m->adj.assign(K, {});
    auto add_edge = [&](std::size_t a, std::size_t b) {
        if (a == b) return;
        m->adj[a].push_back(b);
        m->adj[b].push_back(a);
    };
    for (const auto& kv : m->cell_index) {
        const std::uint64_t tile = kv.first.first;
        const int gx = kv.first.second.first;
        const int gy = kv.first.second.second;
        const std::size_t k = kv.second;
        auto link = [&](int nx, int ny) {
            if (nx < 0 || ny < 0 || nx >= m->grid || ny >= m->grid) return;
            const auto key = std::make_pair(tile, std::make_pair(nx, ny));
            const auto it = m->cell_index.find(key);
            if (it != m->cell_index.end()) add_edge(k, it->second);
        };
        link(gx + 1, gy);
        link(gx - 1, gy);
        link(gx, gy + 1);
        link(gx, gy - 1);
    }

    // 跨 tile 几何邻接（tile 边界 control cells 角距 < 阈值连接，
    // 使跨 tile 几何相邻区域的 correction 受同一平滑约束）
    {
        // cell 中心间距（target order 像素尺度推导）
        const double nside = (double)(1u << (unsigned)(cfg.target_order + 9));
        const double pix_rad = std::sqrt(4.0 * 3.141592653589793 /
                                         (12.0 * nside * nside));
        const double cell_dist_rad = (double)m->cell_side * pix_rad;
        const double link_rad = cell_dist_rad * 1.6;
        const double link_deg = link_rad * 180.0 / 3.141592653589793;
        std::vector<std::size_t> boundary;
        for (std::size_t k = 0; k < K; ++k) {
            const auto& cn = m->controls[k];
            if (cn.gx == 0 || cn.gx == m->grid - 1 ||
                cn.gy == 0 || cn.gy == m->grid - 1)
                boundary.push_back(k);
        }
        for (std::size_t i = 0; i < boundary.size(); ++i) {
            const auto& a = m->controls[boundary[i]];
            for (std::size_t j = i + 1; j < boundary.size(); ++j) {
                const auto& b = m->controls[boundary[j]];
                if (a.tile_ipix == b.tile_ipix) continue;
                // 粗筛：|Δra|/|Δdec| 先于角距（避免 O(B²) 全角距）
                if (std::fabs(a.ra_deg - b.ra_deg) > link_deg) continue;
                if (std::fabs(a.dec_deg - b.dec_deg) > link_deg) continue;
                if (astrocs::healpix::angular_distance_deg(
                        a.ra_deg, a.dec_deg, b.ra_deg, b.dec_deg) < link_deg) {
                    m->adj[boundary[i]].push_back(boundary[j]);
                    m->adj[boundary[j]].push_back(boundary[i]);
                }
            }
        }
        // 去重（同一邻接可能被网格与跨 tile 同时加入）
        for (auto& v : m->adj) {
            std::sort(v.begin(), v.end());
            v.erase(std::unique(v.begin(), v.end()), v.end());
        }
    }

    // 求解前连通分量（frame-control 二分图），每分量独立 gauge
    // 只有带 observation 的 frame/control 参与数据分量统计；
    // 纯几何节点（无 obs）不进入 frame-control 图，component 标记为
    // sentinel（SIZE_MAX），不参与 reference-frame gauge。
    {
        const std::size_t Fn = m->frame_index.size();
        std::vector<std::vector<std::size_t>> fc_adj(Fn + K);
        for (std::uint64_t i = 0; i < n_obs; ++i) {
            const std::size_t f = m->frame_index[obs[i].frame_id];
            const std::size_t ck = m->control_by_id[obs[i].control_id];
            fc_adj[f].push_back(Fn + ck);
            fc_adj[Fn + ck].push_back(f);
        }
        std::vector<std::uint8_t> seen(Fn + K, 0);
        m->control_component.assign(K, kNoData);
        m->frame_component.assign(Fn, 0);
        m->unobserved_geometry_nodes = 0;
        std::vector<std::vector<std::uint64_t>> comp_frames;
        for (std::size_t start = 0; start < Fn; ++start) {
            if (seen[start]) continue;
            const std::size_t comp = comp_frames.size();
            comp_frames.emplace_back();
            std::vector<std::size_t> stack{start};
            seen[start] = 1;
            while (!stack.empty()) {
                const std::size_t u = stack.back();
                stack.pop_back();
                if (u < Fn)
                    m->frame_component[u] = comp;
                else
                    m->control_component[u - Fn] = comp;
                if (u < Fn)
                    comp_frames.back().push_back(
                        m->frame_id_by_index[u]);
                for (std::size_t v : fc_adj[u]) {
                    if (!seen[v]) {
                        seen[v] = 1;
                        stack.push_back(v);
                    }
                }
            }
        }
        // 只统计有观测的 control；无观测节点单列
        for (std::size_t ck = 0; ck < K; ++ck) {
            if (m->controls[ck].obs_idx.empty()) {
                ++m->unobserved_geometry_nodes;
            }
        }
        m->component_count_solve = comp_frames.size();
        m->component_ref_frame.resize(comp_frames.size());
        for (std::size_t c = 0; c < comp_frames.size(); ++c) {
            std::uint64_t mn = ~0ULL;
            for (std::uint64_t fid : comp_frames[c]) mn = std::min(mn, fid);
            m->component_ref_frame[c] = mn;
        }
        m->component_count = comp_frames.size();       // data_component_count
        // geometry_component_count：平滑邻接图（含无观测节点）
        std::vector<std::uint8_t> gseen(K, 0);
        std::size_t gcomp = 0;
        for (std::size_t s = 0; s < K; ++s) {
            if (gseen[s]) continue;
            ++gcomp;
            std::vector<std::size_t> st{s};
            gseen[s] = 1;
            while (!st.empty()) {
                const std::size_t u = st.back();
                st.pop_back();
                for (std::size_t v : m->adj[u]) {
                    if (!gseen[v]) {
                        gseen[v] = 1;
                        st.push_back(v);
                    }
                }
            }
        }
        m->geometry_component_count = gcomp;
    }

    // ===== Huber IRLS 坐标下降求解 =====
    // 残差 r_ik = y_ik - M_k - C_i,k
    // [B4-27 排异锚点 — REJECTION.md 迭代剔除/阈值契约, SCI-UPM 权重 SCI-UPM-WEIGHT-001 关联, 不改语义]
    // 权重 raw_w = quality × control_ivar（SCI-UPM-WEIGHT-001）；
    // per-control 归一化后 × huber_w。legacy snr² 路径仅在
    // use_ivar_weight=0（ablation/诊断）时由 p2_upm_raw_weight 选择。
    // M 更新（固定 C）：M_k = Σ w (y - C) / Σ w
    // C 更新（固定 M，逐帧 CG）：
    // min Σ_k w_ik (r_ik - C_ik)^2 + λs Σ_{k~l} (C_ik - C_il)^2 + λ0 Σ C_ik^2
    std::vector<double> M(K, 0.0);
    const double anchor = std::max(0.0, cfg.zero_anchor_weight);
    const double lambda_s = std::max(0.0, cfg.smoothing_lambda);

    // per-control 归一化：需要先按 control 聚合（同 cell 多帧观测）
    // 这里直接按 obs 计算 raw 后按 control 归一化（与文档一致）
    std::vector<double> raw_w(n_obs, 0.0);
    auto compute_raw = [&]() -> int {
        std::vector<double> sums(K, 0.0);
#if defined(P2_ENABLE_OPENMP) && !defined(_MSC_VER)
        // CON-005: 逐 observation 并行 + 逐 control 定序规约（tid 升序 = obs 索引升序求和顺序）。
        const int cworkers = (cfg.cpu_workers > 0) ? cfg.cpu_workers
                             : (int)std::max(1u, std::thread::hardware_concurrency());
        const bool rpar = (cworkers > 1);
        if (rpar) {
            std::vector<std::vector<double>> tsums((std::size_t)cworkers,
                                                   std::vector<double>(K, 0.0));
            std::atomic<int> rcfail{0};
            #pragma omp parallel num_threads(cworkers)
            {
                const int tid = omp_get_thread_num();
                const std::uint64_t start = (n_obs * (std::uint64_t)tid) / (std::uint64_t)cworkers;
                const std::uint64_t end = (n_obs * (std::uint64_t)(tid + 1)) / (std::uint64_t)cworkers;
                for (std::uint64_t i = start; i < end; ++i) {
                    const int rc = p2_upm_raw_weight(&obs[i], &cfg, &raw_w[i]);
                    if (rc != 0) { rcfail.store(rc); continue; }
                    tsums[(std::size_t)tid][m->control_by_id[obs[i].control_id]] += raw_w[i];
                }
            }
            if (rcfail.load() != 0) return rcfail.load();
            for (int t = 0; t < cworkers; ++t)
                for (std::size_t k = 0; k < K; ++k)
                    sums[k] += tsums[(std::size_t)t][k];
        } else
#endif
        {
            for (std::uint64_t i = 0; i < n_obs; ++i) {
                // 单一 production raw weight 实现（与
                // p2_upm_raw_weight 同一公式）
                const int rc = p2_upm_raw_weight(&obs[i], &cfg, &raw_w[i]);
                if (rc != 0) {
                    // production 缺 control ivar 是显式科学错误，不静默降级。
                    return rc;
                }
                sums[m->control_by_id[obs[i].control_id]] += raw_w[i];
            }
        }
        for (std::uint64_t i = 0; i < n_obs; ++i) {
            const std::size_t ck = m->control_by_id[obs[i].control_id];
            if (sums[ck] > 1e-12)
                raw_w[i] = raw_w[i] / sums[ck] * m->controls[ck].reliability;
            else
                raw_w[i] = 0.0;
        }
        return 0;
    };

    auto cg_solve_frame = [&](std::size_t fi, std::vector<double>& x,
                              const std::vector<double>& rhs) {
        // (W + λs L + λ0 I) x = rhs；未知数 = 覆盖该帧的 control 子集
        // 简化为全 K 维 CG（K 几千，100 迭代可控）
        const std::size_t max_cg = 200;
        // 每轮目标随 M 更新变化：从 0 开始解，避免沿用旧解
        std::fill(x.begin(), x.end(), 0.0);
        std::vector<double> r = rhs;
        std::vector<double> p = r;
        for (std::size_t s = 0; s < max_cg; ++s) {
            // Ap = W p + λs L p + λ0 p
            std::vector<double> Ap(K, 0.0);
            for (std::size_t k = 0; k < K; ++k) {
                double lp = 0.0;
                for (std::size_t nb : m->adj[k]) lp += p[k] - p[nb];
                Ap[k] = m->obs_w[fi][k] * p[k] + lambda_s * lp +
                        anchor * p[k];
            }
            double pAp = 0.0, num = 0.0;
            for (std::size_t k = 0; k < K; ++k) {
                pAp += p[k] * Ap[k];
                num += r[k] * r[k];
            }
            if (pAp <= 1e-30) break;
            const double alpha_v = num / pAp;
            double rs_new = 0.0;
            for (std::size_t k = 0; k < K; ++k) {
                x[k] += alpha_v * p[k];
                r[k] -= alpha_v * Ap[k];
                rs_new += r[k] * r[k];
            }
            if (rs_new < 1e-24) break;
            const double beta = rs_new / num;
            for (std::size_t k = 0; k < K; ++k)
                p[k] = r[k] + beta * p[k];
        }
    };

    for (int iter = 0; iter < cfg.max_iterations; ++iter) {
        // 1. 权重（每轮：raw per-control 归一化 + Huber）
        {
            const int rc = compute_raw();
            if (rc != 0) {
                // production 缺 control ivar 属于显式科学错误（DATA-UPM-
                // CONTROL-UNC-001），build 失败而非静默改变权重语义。
                p2_upm_close((void*)m);
                return 2;
            }
        }
        std::vector<double> w(n_obs);
#if defined(P2_ENABLE_OPENMP) && !defined(_MSC_VER)
        // CON-005: 逐 obs 独立 w 计算（per-obs 写 w[i] 不相交）
        #pragma omp parallel for schedule(static)
#endif
        for (std::uint64_t i = 0; i < n_obs; ++i) {
            const std::size_t ck = m->control_by_id[obs[i].control_id];
            // Huber 作用于标准化残差 z = r / sigma_eff。
            // 此前 huber_delta=1.345 直接与 ~0.002 raw residual 比较，
            // 所有残差都落在线性区，robust 几乎永不生效。现在
            // sigma_eff = max(观测 uncertainty, sigma_floor)，delta 保持
            // dimensionless 1.345；污染观测（patch 星污染 → residual 大
            // 而 uncertainty 有限）会被强烈降权。
            const double r = obs[i].value - M[ck] -
                             m->C[m->frame_index[obs[i].frame_id]][ck];
            const double sigma_eff =
                std::max(std::fabs(obs[i].uncertainty), cfg.sigma_floor);
            w[i] = raw_w[i] * huber_w(r / sigma_eff, cfg.huber_delta);
        }
        // 2. M 更新（固定 C）：每分量 gauge = 分量内最小 frame_id C=0 →
        // M 由该分量参考帧观测定义；参考帧未覆盖节点用全部帧（延拓）。
        double max_dM = 0.0;
#if defined(P2_ENABLE_OPENMP) && !defined(_MSC_VER)
        // CON-005: 逐 control 独立 M 更新（每 control 的 obs 聚合整块由单线程完成，
        // 逐 k 写 M[k] 不相交；仅 max 归约 => 1T/2T 位精确）
        #pragma omp parallel for schedule(static) reduction(max:max_dM)
#endif
        for (std::size_t k = 0; k < m->controls.size(); ++k) {
            double num = 0.0, den = 0.0;
            const std::size_t comp = m->control_component[k];
            // 无观测几何节点不参与数据图，component=sentinel；
            // 其 M 由全部帧加权（无参考帧语义）定义。
            if (comp == kNoData) {
                for (std::size_t ii : m->controls[k].obs_idx) {
                    const auto& o = obs[ii];
                    const double c =
                        m->C[m->frame_index[o.frame_id]][k];
                    num += w[ii] * (o.value - c);
                    den += w[ii];
                }
                if (den > 1e-12) {
                    const double Mnew = num / den;
                    max_dM =
                        std::max(max_dM, std::fabs(Mnew - M[k]));
                    M[k] = Mnew;
                }
                continue;
            }
            const std::size_t rf =
                m->frame_index[m->component_ref_frame[comp]];
            for (std::size_t ii : m->controls[k].obs_idx) {
                if (m->frame_index[obs[ii].frame_id] != rf) continue;
                const auto& o = obs[ii];
                const double c = m->C[rf][k];
                num += w[ii] * (o.value - c);
                den += w[ii];
            }
            if (den <= 1e-12) {
                // 参考帧未覆盖：全部帧加权（含 C 补偿）
                for (std::size_t ii : m->controls[k].obs_idx) {
                    const auto& o = obs[ii];
                    const double c = m->C[m->frame_index[o.frame_id]][k];
                    num += w[ii] * (o.value - c);
                    den += w[ii];
                }
            }
            if (den > 1e-12) {
                const double Mnew = num / den;
                max_dM = std::max(max_dM, std::fabs(Mnew - M[k]));
                M[k] = Mnew;
            }
        }
        // 3. C 更新（固定 M；逐帧 CG + 参考帧 gauge）
        double max_dC = 0.0;
        std::vector<std::uint64_t> fids;
        fids.reserve(m->frame_index.size());
        for (const auto& kv : m->frame_index) fids.push_back(kv.first);
#if defined(P2_ENABLE_OPENMP) && !defined(_MSC_VER)
        // CON-005: 逐 frame 独立 C 更新 + CG（每 frame 的 rhs/obs_w/C[f]/x 全 per-frame；
        // cg_solve_frame 读只读共享 adj/K/lambda_s/anchor，写各 frame 自身；仅 max 归约）
        #pragma omp parallel for schedule(static) reduction(max:max_dC)
        for (std::size_t fi_ = 0; fi_ < fids.size(); ++fi_) {
            const std::uint64_t frame_id = fids[fi_];
#else
        for (std::size_t fi_ = 0; fi_ < fids.size(); ++fi_) {
            const std::uint64_t frame_id = fids[fi_];
#endif
            const std::size_t f = m->frame_index[frame_id];
            if (frame_id ==
                m->component_ref_frame[m->frame_component[f]]) {
                // 该分量参考帧 gauge：C=0（每分量独立，非全局最小帧）
                for (std::size_t k = 0; k < K; ++k) m->C[f][k] = 0.0;
                continue;
            }
            // rhs[k] = Σ_i w_ik (y_ik - M_k)（仅该帧观测）
            std::vector<double> rhs(K, 0.0);
            for (std::size_t k = 0; k < K; ++k) {
                for (std::size_t ii : m->controls[k].obs_idx) {
                    const auto& o = obs[ii];
                    if (m->frame_index[o.frame_id] != f) continue;
                    rhs[k] += w[ii] * (o.value - M[k]);
                }
            }
            // obs_w 按当前权重更新（per-frame per-control 聚合）
            for (std::size_t k = 0; k < K; ++k) {
                m->obs_w[f][k] = 0.0;
                for (std::size_t ii : m->controls[k].obs_idx) {
                    const auto& o = obs[ii];
                    if (m->frame_index[o.frame_id] != f) continue;
                    m->obs_w[f][k] += w[ii];
                }
            }
            std::vector<double> x = m->C[f];
            cg_solve_frame(f, x, rhs);
            for (std::size_t k = 0; k < K; ++k)
                max_dC = std::max(max_dC, std::fabs(x[k] - m->C[f][k]));
            m->C[f] = std::move(x);
        }
        // 4. objective + 收敛
        m->iterations = iter + 1;
        m->objective = 0.0;
        for (std::uint64_t i = 0; i < n_obs; ++i) {
            const auto& o = obs[i];
            const std::size_t ck = m->control_by_id[o.control_id];
            const double c = m->C[m->frame_index[o.frame_id]][ck];
            const double r = o.value - M[ck] - c;
            const double sigma_eff =
                std::max(std::fabs(o.uncertainty), cfg.sigma_floor);
            m->objective += raw_w[i] *
                            huber_rho(r / sigma_eff, cfg.huber_delta);
        }
        if (max_dM < cfg.tolerance && max_dC < cfg.tolerance) break;
    }

    for (std::size_t k = 0; k < K; ++k) m->controls[k].M = M[k];
    // 连通分量已在求解前建立（component_count_solve / component_ref_frame），
    // 每分量独立 gauge；此处不重复统计。

    // 模型哈希：精确序列化（max_digits10）+ frame manifest + 拓扑 + 系数
    {
        std::string payload;
        auto fmt = [](double v) {
            std::ostringstream os;
            os << std::setprecision(std::numeric_limits<double>::max_digits10)
               << v;
            return os.str();
        };
        payload += std::to_string(m->info.version) + "|";
        payload += std::to_string(m->info.target_order) + "|";
        payload += fmt(cfg.smoothing_lambda) + "|";
        payload += fmt(cfg.zero_anchor_weight) + "|";
        payload += fmt(cfg.sigma_floor) + "|";
        payload += fmt(cfg.support_power) + "|";
        payload += std::to_string(cfg.use_ivar_weight) + "|";
        payload += m->input_manifest_hash + "|";
        for (const auto& kv : m->frame_index) {
            payload += std::to_string(kv.first) + ";";
        }
        payload += "|";
        for (const auto& cn : m->controls) {
            payload += std::to_string(cn.tile_ipix) + "," +
                        std::to_string(cn.gx) + "," + std::to_string(cn.gy) +
                        "," + fmt(cn.M) + ";";
        }
        payload += "|T";
        for (const auto& kv : m->cell_index) {
            payload += std::to_string(kv.first.first) + "," +
                        std::to_string(kv.first.second.first) + "," +
                        std::to_string(kv.first.second.second) + "," +
                        std::to_string(kv.second) + ";";
        }
        payload += "|C";
        for (std::size_t f = 0; f < F; ++f) {
            for (std::size_t k = 0; k < K; ++k)
                payload += fmt(m->C[f][k]) + ";";
        }
        const std::string h = astrocs::crypto::sha256_hex(
            payload.data(), payload.size());
        std::memcpy(m->info.model_hash, h.c_str(), 64);
        m->info.model_hash[64] = '\0';
    }

    m->info.component_count = (std::uint32_t)m->component_count;

    *out_model = static_cast<void*>(m);
    return 0;
}

int p2_upm_build(const P2ControlObservation* obs, std::uint64_t n_obs,
                 const P2UpmBuildConfig* cfg, void** out_model) {
    return build_impl(obs, n_obs, nullptr, 0, cfg, out_model);
}

int p2_upm_build_geo(const P2ControlObservation* obs, std::uint64_t n_obs,
                     const P2ControlNode* nodes, std::uint64_t n_nodes,
                     const P2UpmBuildConfig* cfg, void** out_model) {
    return build_impl(obs, n_obs, nodes, n_nodes, cfg, out_model);
}

int p2_upm_save(const void* model, const char* path) {
    if (model == nullptr || path == nullptr) return 1;
    const Model* m = static_cast<const Model*>(model);
    // ALG-UPM-FRAME-BIND-001：参数行数必须与 frame_id_by_index 一致，
    // 否则拒绝写盘，防止生成绑定已损坏的模型文件。
    if (m->frame_id_by_index.size() != m->C.size()) return 1;
    nlohmann::json j;
    j["format"] = "astrocs-upm-v2";
    j["version"] = m->info.version;
    j["target_order"] = m->info.target_order;
    j["precision"] = m->info.precision;
    j["robust_loss"] = m->cfg.robust_loss;
    j["snr_weight_mode"] = m->cfg.snr_weight_mode;
    j["use_ivar_weight"] = m->cfg.use_ivar_weight;
    j["huber_delta"] = m->cfg.huber_delta;
    j["smoothing_lambda"] = m->cfg.smoothing_lambda;
    j["zero_anchor_weight"] = m->cfg.zero_anchor_weight;
    j["max_iterations"] = m->cfg.max_iterations;
    j["tolerance"] = m->cfg.tolerance;
    j["sigma_floor"] = m->cfg.sigma_floor;
    j["support_power"] = m->cfg.support_power;
    j["model_hash"] = m->info.model_hash;
    j["input_manifest_hash"] = m->input_manifest_hash;
    j["iterations"] = m->iterations;
    j["objective"] = m->objective;
    j["component_count"] = m->component_count;
    j["geometry_component_count"] = m->geometry_component_count;
    j["unobserved_geometry_nodes"] = m->unobserved_geometry_nodes;
    nlohmann::json refs = nlohmann::json::array();
    for (std::size_t c = 0; c < m->component_count; ++c)
        refs.push_back(m->component_ref_frame[c]);
    j["component_ref_frame"] = refs;
    nlohmann::json fcomp = nlohmann::json::array();
    for (std::size_t f = 0; f < m->frame_component.size(); ++f)
        fcomp.push_back(m->frame_component[f]);
    j["frame_component"] = fcomp;
    j["control_count"] = m->info.control_count;
    j["observation_count"] = m->info.observation_count;
    nlohmann::json frames = nlohmann::json::array();
    for (std::uint64_t fid : m->frame_id_by_index) frames.push_back(fid);
    j["frames"] = frames;
    nlohmann::json controls = nlohmann::json::array();
    for (std::size_t k = 0; k < m->controls.size(); ++k) {
        const auto& cn = m->controls[k];
        controls.push_back({cn.tile_ipix, cn.gx, cn.gy, cn.ra_deg, cn.dec_deg,
                            cn.M, cn.leaf_ipix});
    }
    j["controls"] = controls;
    nlohmann::json ci = nlohmann::json::array();
    for (const auto& kv : m->cell_index)
        ci.push_back({kv.first.first, kv.first.second.first,
                      kv.first.second.second, kv.second});
    j["cell_index"] = ci;
    nlohmann::json Cj = nlohmann::json::array();
    for (std::size_t f = 0; f < m->C.size(); ++f) {
        nlohmann::json row = nlohmann::json::array();
        for (std::size_t k = 0; k < m->controls.size(); ++k) {
            const double v = m->C[f][k];
            if (v != 0.0) row.push_back({k, v});
        }
        Cj.push_back(row);
    }
    j["C"] = Cj;
    // 唯一 AIO：模型稀疏持久化走 aio_upm_write_sparse
    const std::string text = j.dump(2);
    return aio_upm_write_sparse(path, text.c_str());
}

int p2_upm_open(const char* path, void** out_model) {
    if (path == nullptr || out_model == nullptr) return 1;
    AioUpmSparse* aio = aio_upm_open(path);
    if (!aio) return 1;
    char* buf = nullptr;
    std::size_t len = 0;
    if (aio_upm_read_all_dynamic(aio, &buf, &len) != 0) {
        aio_upm_close(aio);
        return 1;
    }
    aio_upm_close(aio);
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(buf);
    } catch (...) {
        delete[] buf;
        return 1;
    }
    delete[] buf;
    if (j.value("format", std::string()) != "astrocs-upm-v2")
        return 1;
    Model* m = new Model();
    try {
        m->info.version = j.value("version", 1u);
        m->info.precision = j.value("precision", 1u);
        m->info.target_order = j.value("target_order", 0u);
        m->info.control_count = j.value("control_count", 0ull);
        m->info.observation_count = j.value("observation_count", 0ull);
        m->component_count = j.value("component_count", 1ull);
        m->geometry_component_count =
            j.value("geometry_component_count", 1ull);
        m->unobserved_geometry_nodes =
            j.value("unobserved_geometry_nodes", 0ull);
        m->cfg.robust_loss = j.value("robust_loss", 0);
        m->cfg.snr_weight_mode = j.value("snr_weight_mode", 0);
        m->cfg.use_ivar_weight = j.value("use_ivar_weight", 1);
        m->cfg.huber_delta = j.value("huber_delta", 1.345);
        m->cfg.smoothing_lambda = j.value("smoothing_lambda", 0.0);
        m->cfg.zero_anchor_weight = j.value("zero_anchor_weight", 1e-3);
        m->cfg.max_iterations = j.value("max_iterations", 100);
        m->cfg.tolerance = j.value("tolerance", 1e-6);
        m->cfg.sigma_floor = j.value("sigma_floor", 1e-3);
        m->cfg.support_power = j.value("support_power", 1.0);
        m->input_manifest_hash =
            j.value("input_manifest_hash", std::string());
        const std::string h =
            j.value("model_hash", std::string(64, '0'));
        std::strncpy(m->info.model_hash, h.c_str(),
                     sizeof(m->info.model_hash) - 1);
        m->info.model_hash[sizeof(m->info.model_hash) - 1] = '\0';
        m->iterations = j.value("iterations", 0);
        m->objective = j.value("objective", 0.0);
        m->component_count = j.value("component_count", (std::size_t)1);
        m->info.component_count = (std::uint32_t)m->component_count;
        m->grid = 8;
        m->cell_side = 512 / m->grid;
        // DATA-UPM-MODEL-001：frame_id_by_index 必须显式持久化；
        // 缺失、非数组、含重复或非法项的文件一律拒绝，禁止猜测顺序。
        if (!j.contains("frames") || !j["frames"].is_array()) {
            delete m;
            return 1;
        }
        std::set<std::uint64_t> seen_frames;
        std::size_t fi = 0;
        for (const auto& fr : j["frames"]) {
            if (!fr.is_number_unsigned()) {
                delete m;
                return 1;
            }
            const std::uint64_t fid = fr.get<std::uint64_t>();
            if (!seen_frames.insert(fid).second) {
                delete m;
                return 1;
            }
            m->frame_index[fid] = fi++;
            m->frame_id_by_index.push_back(fid);
        }
        // 每分量 gauge frame id + frame→component 映射持久化
        if (j.contains("component_ref_frame") &&
            j.contains("frame_component")) {
            if (!j["component_ref_frame"].is_array() ||
                j["component_ref_frame"].size() != m->component_count ||
                !j["frame_component"].is_array() ||
                j["frame_component"].size() != fi) {
                delete m;
                return 1;
            }
            m->component_ref_frame.resize(m->component_count);
            for (std::size_t c = 0; c < m->component_count; ++c)
                m->component_ref_frame[c] =
                    j["component_ref_frame"][c].get<std::uint64_t>();
            m->frame_component.assign(fi, 0);
            for (std::size_t f = 0; f < fi; ++f)
                m->frame_component[f] =
                    j["frame_component"][f].get<std::size_t>();
        }
        // controls：必须存在且为数组，字段类型非法即拒绝（防损坏文件崩溃）
        if (!j.contains("controls") || !j["controls"].is_array()) {
            delete m;
            return 1;
        }
        for (const auto& ct : j["controls"]) {
            if (!ct.is_array() || ct.size() < 7 ||
                !ct[0].is_number_unsigned() || !ct[1].is_number_integer() ||
                !ct[2].is_number_integer() || !ct[3].is_number() ||
                !ct[4].is_number() || !ct[5].is_number() ||
                !ct[6].is_number_unsigned()) {
                delete m;
                return 1;
            }
            ControlNode cn;
            cn.tile_ipix = ct[0].get<std::uint64_t>();
            cn.gx = ct[1].get<int>();
            cn.gy = ct[2].get<int>();
            cn.ra_deg = ct[3].get<double>();
            cn.dec_deg = ct[4].get<double>();
            cn.M = ct[5].get<double>();
            cn.leaf_ipix = ct[6].get<std::uint64_t>();
            cn.reliability = 1.0;
            cn.id = cn.tile_ipix * 1000 +
                    (std::uint64_t)(cn.gy * 8 + cn.gx);
            const auto key =
                std::make_pair(cn.tile_ipix, std::make_pair(cn.gx, cn.gy));
            m->cell_index[key] = m->controls.size();
            m->control_by_id[cn.id] = m->controls.size();
            m->controls.push_back(cn);
        }
        // 恢复完整 cell_index（含跨 tile 合并别名键，保证 seam
        // 求值与保存前一致）；旧文件无该键时退化为 controls 推导的拓扑。
        if (j.contains("cell_index")) {
            if (!j["cell_index"].is_array()) {
                delete m;
                return 1;
            }
            m->cell_index.clear();
            for (const auto& e : j["cell_index"]) {
                if (!e.is_array() || e.size() < 4 ||
                    !e[0].is_number_unsigned() ||
                    !e[1].is_number_integer() ||
                    !e[2].is_number_integer() ||
                    !e[3].is_number_unsigned()) {
                    delete m;
                    return 1;
                }
                const std::uint64_t tile = e[0].get<std::uint64_t>();
                const int gx = e[1].get<int>();
                const int gy = e[2].get<int>();
                const std::size_t idx = e[3].get<std::size_t>();
                m->cell_index[std::make_pair(
                    tile, std::make_pair(gx, gy))] = idx;
            }
        }
        // 恢复每 tile 覆盖范围（evaluate 外推锚点用）
        for (const auto& kv : m->cell_index) {
            const std::uint64_t t = kv.first.first;
            const int gx = kv.first.second.first;
            const int gy = kv.first.second.second;
            auto it = m->tile_gx_bounds.find(t);
            if (it == m->tile_gx_bounds.end()) {
                m->tile_gx_bounds[t] = {gx, gx};
                m->tile_gy_bounds[t] = {gy, gy};
            } else {
                it->second.first = std::min(it->second.first, gx);
                it->second.second = std::max(it->second.second, gx);
                auto& gyb = m->tile_gy_bounds[t];
                gyb.first = std::min(gyb.first, gy);
                gyb.second = std::max(gyb.second, gy);
            }
        }
        // DATA-UPM-MODEL-001：参数矩阵行数必须等于 frame 数；行列越界、
        // 类型非法一律拒绝，不静默截断/置零。
        const std::size_t F = m->frame_index.size();
        const std::size_t K = m->controls.size();
        if (!j.contains("C") || !j["C"].is_array() || j["C"].size() != F) {
            delete m;
            return 1;
        }
        m->C.assign(F, std::vector<double>(K, 0.0));
        for (std::size_t f = 0; f < j["C"].size(); ++f) {
            const auto& crow = j["C"][f];
            if (!crow.is_array()) {
                delete m;
                return 1;
            }
            for (const auto& item : crow) {
                if (!item.is_array() || item.size() < 2 ||
                    !item[0].is_number_unsigned() ||
                    !item[1].is_number()) {
                    delete m;
                    return 1;
                }
                const std::size_t k = item[0].get<std::size_t>();
                if (k >= K) {
                    delete m;
                    return 1;
                }
                m->C[f][k] = item[1].get<double>();
            }
        }
        m->adj.assign(K, {});
        for (std::size_t k = 0; k < K; ++k) {
            const auto& cn = m->controls[k];
            auto link = [&](int nx, int ny) {
                if (nx < 0 || ny < 0 || nx >= m->grid || ny >= m->grid)
                    return;
                const auto key =
                    std::make_pair(cn.tile_ipix, std::make_pair(nx, ny));
                const auto it = m->cell_index.find(key);
                if (it != m->cell_index.end())
                    m->adj[k].push_back(it->second);
            };
            link(cn.gx + 1, cn.gy);
            link(cn.gx - 1, cn.gy);
            link(cn.gx, cn.gy + 1);
            link(cn.gx, cn.gy - 1);
        }
    } catch (...) {
        // DATA-UPM-MODEL-001：任何字段损坏都返回稳定错误，禁止异常越界。
        delete m;
        return 1;
    }
    *out_model = static_cast<void*>(m);
    return 0;
}

int p2_upm_info(const void* model, P2ModelInfo* out_info) {
    if (model == nullptr || out_info == nullptr) return 1;
    const Model* m = static_cast<const Model*>(model);
    *out_info = m->info;
    return 0;
}

int p2_upm_calibrate_block(const void* model, std::uint64_t frame_id,
                           const std::uint64_t* leaf_ipix,
                           const double* input_signal,
                           double* output_signal, std::uint64_t count) {
    if (model == nullptr || input_signal == nullptr ||
        output_signal == nullptr) {
        return 1;
    }
    const Model* m = static_cast<const Model*>(model);
    const auto it = m->frame_index.find(frame_id);
    //未知 frame_id 必须显式失败，禁止回退 frame 0 参数
    // （错误帧校准会静默制造错误科学结果）。
    if (it == m->frame_index.end()) return 1;
    const std::size_t fi = it->second;
    const int tile_shift = 9;
    const std::uint64_t mask = (1ULL << (2u * (unsigned)tile_shift)) - 1ULL;
    for (std::uint64_t i = 0; i < count; ++i) {
        const std::uint64_t tile =
            leaf_ipix[i] >> (2u * (unsigned)tile_shift);
        const std::uint64_t local = leaf_ipix[i] & mask;
        std::uint32_t x = 0, y = 0;
        astrocs::healpix::nested_local_to_xy(local, (std::uint32_t)tile_shift,
                                             x, y);
        // 双线性空间校正场求值（cell 内随位置连续）
        const double c =
            evaluate_c_field(m, fi, tile, (int)x, (int)y);
        output_signal[i] = input_signal[i] - c;
    }
    return 0;
}

double p2_upm_evaluate_c(const void* model, std::uint64_t frame_id,
                         std::uint64_t leaf_ipix) {
    if (model == nullptr) return 0.0;
    const Model* m = static_cast<const Model*>(model);
    const auto it = m->frame_index.find(frame_id);
    //未知 frame_id 返回 NaN（显式不可用），禁止用
    // frame 0 参数伪装有效结果。
    if (it == m->frame_index.end())
        return std::numeric_limits<double>::quiet_NaN();
    const std::size_t fi = it->second;
    const int tile_shift = 9;
    const std::uint64_t mask = (1ULL << (2u * (unsigned)tile_shift)) - 1ULL;
    const std::uint64_t tile = leaf_ipix >> (2u * (unsigned)tile_shift);
    const std::uint64_t local = leaf_ipix & mask;
    std::uint32_t x = 0, y = 0;
    astrocs::healpix::nested_local_to_xy(local, (std::uint32_t)tile_shift,
                                         x, y);
    return evaluate_c_field(m, fi, tile, (int)x, (int)y);
}

// production UPM 观测 raw weight（单一实现，build 内部复用）
int p2_upm_raw_weight(const P2ControlObservation* obs,
                      const P2UpmBuildConfig* cfg_in, double* out_raw) {
    if (obs == nullptr || out_raw == nullptr) return 1;
    P2UpmBuildConfig cfg;
    if (cfg_in != nullptr) {
        cfg = *cfg_in;
    } else {
        cfg.sigma_floor = 1e-3;
        cfg.support_power = 1.0;
        cfg.quality_mode = 0;
        cfg.use_ivar_weight = 1;   // production 默认 control-ivar
    }
    if (cfg.sigma_floor <= 0.0) cfg.sigma_floor = 1e-3;
    if (cfg.support_power < 0.0) cfg.support_power = 1.0;
    const double qf = quality_factor(obs->quality_flags, cfg.quality_mode);
    if (cfg.use_ivar_weight != 0) {
        // SCI-UPM-WEIGHT-001：science 权重只含 quality × control_ivar。
        // 无 star-SNR / support^p / 单像素 ivar 因子。
        const double civ = obs->control_ivar;
        if (!std::isfinite(civ) || civ <= 0.0) return 2;  // 显式缺 control ivar
        *out_raw = qf * civ;
        return 0;
    }
    // legacy ablation/diagnostic（SNR-015）：snr²/(1+snr²) 路径。
    const double sp = std::clamp(obs->support, 0.0, 1.0);
    const double snr2 = obs->snr * obs->snr;
    const double unc =
        std::max(std::fabs(obs->uncertainty), cfg.sigma_floor);
    *out_raw = qf * std::pow(sp, cfg.support_power) *
               (snr2 / (1.0 + snr2)) / (unc * unc);
    return 0;
}

int p2_upm_normalized_weights(const P2ControlObservation* obs,
                              std::uint64_t n_obs,
                              const P2UpmBuildConfig* cfg,
                              double* out_norm) {
    if (obs == nullptr || out_norm == nullptr || n_obs == 0) return 1;
    std::map<std::uint64_t, double> sums;
    std::vector<double> raw(n_obs);
    for (std::uint64_t i = 0; i < n_obs; ++i) {
        if (p2_upm_raw_weight(&obs[i], cfg, &raw[i]) != 0) return 1;
        sums[obs[i].control_id] += raw[i];
    }
    const double rel = (cfg && cfg->control_reliability > 0.0)
                           ? cfg->control_reliability : 1.0;
    for (std::uint64_t i = 0; i < n_obs; ++i) {
        const auto it = sums.find(obs[i].control_id);
        const double s = (it != sums.end()) ? it->second : 0.0;
        out_norm[i] = (s > 1e-12) ? raw[i] / s * rel : 0.0;
    }
    return 0;
}

int p2_upm_geometry_hash(const void* model, char* out, int buf_size) {
    if (model == nullptr || out == nullptr || buf_size <= 0) return 1;
    const Model* m = static_cast<const Model*>(model);
    std::string payload;
    payload += "order=" + std::to_string(m->info.target_order) + ";";
    payload += "grid=" + std::to_string(m->grid) + ";";
    payload += "cell=" + std::to_string(m->cell_side) + ";";
    // 仅 geometry/coverage 拓扑：control cell (tile,gx,gy) + 邻接
    for (const auto& cn : m->controls)
        payload += std::to_string(cn.tile_ipix) + "," +
                   std::to_string(cn.gx) + "," + std::to_string(cn.gy) + ";";
    payload += "|adj";
    for (const auto& v : m->adj) {
        for (std::size_t nb : v) payload += std::to_string(nb) + ",";
        payload += ";";
    }
    const std::string h =
        astrocs::crypto::sha256_hex(payload.data(), payload.size());
    std::strncpy(out, h.c_str(), (std::size_t)buf_size - 1);
    out[buf_size - 1] = '\0';
    return 0;
}

int p2_upm_component_gauges(const void* model,
                            std::uint64_t* out_component_count,
                            std::uint64_t* out_ref_frame_ids) {
    if (model == nullptr) return 1;
    const Model* m = static_cast<const Model*>(model);
    if (out_component_count) *out_component_count = m->component_count;
    if (out_ref_frame_ids && m->component_count > 0) {
        if (m->component_ref_frame.size() < m->component_count) return 2;
        for (std::size_t c = 0; c < m->component_count; ++c)
            out_ref_frame_ids[c] = m->component_ref_frame[c];
    }
    return 0;
}

// CON-010：并行化稠密缓存物化。每个 (f,tile) 的双线性求值相互独立，但
// aio_upm_dense_write_tile 强制 frame_index 单调递增 + 单 FILE* 顺序 fwrite，
// 因此采用"分批并行求值 -> 块内按 (f,tile) 单调序串行写"。不改变每像素值
// => 稠密缓存 bit-identical；内存上界 = kChunk*kLeafPx*8 字节。
// workers<=0 => auto(omp_get_max_threads)；串行构建走同路径(并行度=1)。
// 说明：既保留公开 API p2_upm_materialize_dense 的既有签名（外部/测试不破坏），
// 又让生产 CLI(stage2) 传入与积分一致的 worker 数。
int p2_upm_materialize_dense_n(const void* model, int target_order,
                               const char* cache_path, int workers) {
    if (model == nullptr || cache_path == nullptr) return 1;
    const Model* m = static_cast<const Model*>(model);
    if (target_order < 0) target_order = m->info.target_order;
    // 唯一 AIO：稠密缓存 = 空间求值缓存（frame × tile 的 C_i(p) 值）
    // 收集 coverage tiles（cell_index 的 tile 键，排序）
    std::set<std::uint64_t> tile_set;
    for (const auto& kv : m->cell_index) tile_set.insert(kv.first.first);
    if (tile_set.empty()) return 1;
    const std::vector<std::uint64_t> tiles(tile_set.begin(), tile_set.end());
    AioUpmDense* d = aio_upm_dense_begin(
        cache_path, m->info.model_hash, target_order, 1 /* fp64 缓存 */,
        m->C.size(), tiles.size());
    if (!d) return 1;
    const int tile_shift = 9;
    const std::size_t kLeafPx = 512ull * 512ull;
    const std::size_t kChunk = 16;
    // 逐 tile 求值体：像素级双线性（cell 中心 + axis 外推/夹取），
    // 语义与 evaluate_c_field 一致；读写均为只读输入(m->cell_index/C)+独立 out。
    auto compute_tile = [&](std::size_t f, std::uint64_t tile,
                            double* out, std::size_t npx) {
        double node[8][8];
        bool node_ok[8][8];
        for (int gy = 0; gy < 8; ++gy)
            for (int gx = 0; gx < 8; ++gx) {
                const auto key =
                    std::make_pair(tile, std::make_pair(gx, gy));
                const auto it = m->cell_index.find(key);
                if (it != m->cell_index.end()) {
                    node[gy][gx] = m->C[f][it->second];
                    node_ok[gy][gx] = true;
                } else {
                    node[gy][gx] = 0.0;
                    node_ok[gy][gx] = false;
                }
            }
        const int cell = m->cell_side;
        const int half = cell / 2;
        const auto itb = m->tile_gx_bounds.find(tile);
        const int gmin = (itb != m->tile_gx_bounds.end())
                             ? itb->second.first : 0;
        const int gmax = (itb != m->tile_gx_bounds.end())
                             ? itb->second.second : 7;
        const auto itb2 = m->tile_gy_bounds.find(tile);
        const int vmin = (itb2 != m->tile_gy_bounds.end())
                             ? itb2->second.first : 0;
        const int vmax = (itb2 != m->tile_gy_bounds.end())
                             ? itb2->second.second : 7;
        auto axis = [&](int v, int* c0, int* c1, int lo, int hi) {
            if (lo == hi) { *c0 = *c1 = lo * cell + half; return; }
            if (v <= lo * cell + half) { *c0 = lo * cell + half;
                                         *c1 = lo * cell + half + cell; }
            else if (v >= hi * cell + half) { *c0 = hi * cell + half - cell;
                                              *c1 = hi * cell + half; }
            else {
                const int idx = std::clamp(v / cell, lo, hi);
                const int cc = idx * cell + half;
                if (v <= cc) { *c0 = cc - cell; *c1 = cc; }
                else { *c0 = cc; *c1 = cc + cell; }
            }
        };
        auto at = [&](int cx, int cy) {
            const int gxi = std::clamp((cx - half) / cell, 0, 7);
            const int gyi = std::clamp((cy - half) / cell, 0, 7);
            return node_ok[gyi][gxi] ? node[gyi][gxi] : 0.0;
        };
        for (std::uint64_t local = 0; local < npx; ++local) {
            std::uint32_t x = 0, y = 0;
            astrocs::healpix::nested_local_to_xy(
                local, (std::uint32_t)tile_shift, x, y);
            int x0, x1, y0, y1;
            axis((int)x, &x0, &x1, gmin, gmax);
            axis((int)y, &y0, &y1, vmin, vmax);
            const double c00 = at(x0, y0), c10 = at(x1, y0);
            const double c01 = at(x0, y1), c11 = at(x1, y1);
            const double tx = (x1 != x0)
                                  ? (double)((int)x - x0) / (double)(x1 - x0)
                                  : 0.0;
            const double ty = (y1 != y0)
                                  ? (double)((int)y - y0) / (double)(y1 - y0)
                                  : 0.0;
            const double top = c00 + tx * (c10 - c00);
            const double bot = c01 + tx * (c11 - c01);
            out[local] = top + ty * (bot - top);
        }
    };
#if defined(P2_ENABLE_OPENMP) && defined(_OPENMP)
    int nw = (workers > 0) ? workers : omp_get_max_threads();
    if (nw < 1) nw = 1;
#else
    int nw = 1;
#endif
    for (std::size_t f = 0; f < m->C.size(); ++f) {
        for (std::size_t base = 0; base < tiles.size(); base += kChunk) {
            const std::size_t n = std::min(kChunk, tiles.size() - base);
            std::vector<double> buf(n * kLeafPx);
#if defined(P2_ENABLE_OPENMP) && defined(_OPENMP)
            #pragma omp parallel for schedule(dynamic) num_threads(nw)
#endif
            for (std::int64_t j = 0; j < (std::int64_t)n; ++j) {
                compute_tile(f, tiles[base + (std::size_t)j],
                             buf.data() + (std::size_t)j * kLeafPx, kLeafPx);
            }
            for (std::size_t j = 0; j < n; ++j) {
                const std::uint64_t tile = tiles[base + j];
                if (aio_upm_dense_write_tile(d, (std::uint64_t)f, tile,
                                             buf.data() + j * kLeafPx,
                                             kLeafPx) != 0) {
                    aio_upm_dense_abort(d);
                    return 1;
                }
            }
        }
    }
    return aio_upm_dense_end(d);
}

// 既有公开 API：wrap 到 worker 数 auto 的并行实现。
int p2_upm_materialize_dense(const void* model, int target_order,
                             const char* cache_path) {
    return p2_upm_materialize_dense_n(model, target_order, cache_path, 0);
}

int p2_upm_dense_info(const void* model, const char* cache_path,
                      int* out_target_order, std::uint64_t* out_pixels,
                      char* out_source_hash, std::size_t hash_buf_size) {
    if (model == nullptr || cache_path == nullptr) return 1;
    const Model* m = static_cast<const Model*>(model);
    char checksum[65] = {0};
    std::uint64_t tile_count = 0;
    const int rc = aio_upm_dense_info(
        cache_path, m->info.model_hash, out_target_order, &tile_count,
        checksum, (int)sizeof(checksum));
    if (rc != 0) return rc;
    if (out_pixels) *out_pixels = tile_count * (512ull * 512ull);
    if (out_source_hash && hash_buf_size > 0) {
        std::strncpy(out_source_hash, m->info.model_hash, hash_buf_size - 1);
        out_source_hash[hash_buf_size - 1] = '\0';
    }
    return 0;
}

int p2_upm_dense_read_block(const void* model, const char* cache_path,
                            std::uint64_t frame_id,
                            const std::uint64_t* leaf_ipix,
                            const double* input_signal,
                            double* output_signal, std::uint64_t count) {
    if (model == nullptr || cache_path == nullptr || input_signal == nullptr ||
        output_signal == nullptr) {
        return 1;
    }
    const Model* m = static_cast<const Model*>(model);
    const auto it = m->frame_index.find(frame_id);
    if (it == m->frame_index.end()) return 1;
    return aio_upm_read_dense_block(cache_path, m->info.model_hash, it->second,
                                    leaf_ipix, input_signal, output_signal,
                                    count);
}

int p2_upm_close(void* model) {
    if (model == nullptr) return 0;
    delete static_cast<Model*>(model);
    return 0;
}

} // extern "C"
