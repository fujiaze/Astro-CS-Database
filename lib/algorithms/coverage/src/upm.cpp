// lib/algorithms/coverage/src/upm.cpp — Phase2 UnifiedPhotometricModel CPU reference
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
// parameter_rows[index]↔frame_id_by_index[index] 同长无重复；save 经
// frames[]+C[] 行序显式持久化（原子写 aio_upm_write_sparse，ENG-IO-001），
// open 强校验 frames 存在/数组/无重复/类型非法一律拒绝，save→open 后
// frame_id→theta 绑定不变，禁止从有序容器遍历重建。
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
#include <atomic>
#include <thread>

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
    // RELEASE-02 P2a-2 末端残差场 gauge：每 control 的公共残差场 G_k，
    // 对所有帧施加同一扣除（corrected = y - C - G）。空 = 关闭（legacy，
    // 等价 G≡0），保证旧模型文件/旧行为逐位不变。
    std::vector<double> gauge;
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
    // M7-H-101: 收敛状态（1 = 在 cfg.max_iterations 内 max_dM/max_dC 均达到
    // cfg.tolerance；0 = 迭代耗尽未达容差，或旧模型文件未记录该标志）。
    int converged{0};
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
// RELEASE-02 P2a-2：把"按 tile 双线性/外推求值一个 control 场"从 C 行
// 抽成通用行求值，使末端残差场 gauge G（对所有帧相同）与 C 共用同一
// 科学求值语义（相位/外推逐位一致）。
double evaluate_field_row(const Model* m, const std::vector<double>& row,
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
        // 缺失 cell（tile 不在模型 control 图内）保持 0.0 = 无校正：
        // calibrate_block 的调用契约是"frame+leaf 在模型覆盖域内"，域外
        // leaf 属调用方错误而非模型不可用；把此处改为 NaN 会改变生产
        // apply 面对未覆盖 tile 的输出（G2PersistenceAndHashSensitivity
        // 冻结门实测红），超出 M7-H-103（null model / 未知 frame）范围。
        // 登记为开放项：是否把"域外 leaf"升级为显式错误需单独裁决（SC-005）。
        if (it == m->cell_index.end()) return 0.0;
        return row[it->second];
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

double evaluate_c_field(const Model* m, std::size_t frame_idx,
                        std::uint64_t tile, int x, int y) {
    return evaluate_field_row(m, m->C[frame_idx], tile, x, y);
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
        cfg.cpu_workers = 1;       // 默认 1(串行 reference); 生产由 p2_session 传 lease(budget.max_workers)
        // RELEASE-02 P2a：缺省 = W2 冻结 legacy 行为（显式列出，避免依赖
        // 结构体默认成员初始化而被 cfg{} 之外的路径绕过）。
        cfg.gs_damping = 1.0;
        cfg.m_full_frame = 0;
        cfg.final_gauge = 0;
        cfg.tolerance_relative = 0;
    }
    if (cfg.huber_delta <= 0.0) cfg.huber_delta = 1.345;
    if (cfg.max_iterations <= 0) cfg.max_iterations = 100;
    // 与 max_iterations 同源的健壮性缺口：调用方传零初始化 config 时
    // tolerance=0 会让收敛判据 `max_dM < tol && max_dC < tol` 永假
    // (生产显式设 1e-6，不受影响；此处补齐默认化以消除静默不收敛)。
    if (!(cfg.tolerance > 0.0)) cfg.tolerance = 1e-6;
    if (cfg.sigma_floor <= 0.0) cfg.sigma_floor = 1e-3;
    if (cfg.support_power < 0.0) cfg.support_power = 1.0;
    // use_ivar_weight 默认 1 (仅显式 0 关闭)
    // (cfg 拷贝后此处不强制, 保持调用方意图)
    if (cfg.control_reliability <= 0.0) cfg.control_reliability = 1.0;
    if (cfg.zero_anchor_weight < 0.0) cfg.zero_anchor_weight = 1e-3;
    if (cfg.grid != 8) return 3;   // M7-C-001: UPM 网格常数不一致 → 显式拒绝
    if (cfg.smoothing_lambda < 0.0) cfg.smoothing_lambda = 0.0;
    // RELEASE-02 P2a 归一化（零初始化/越域配置一律退回 legacy，不产生静默
    // 科学变更）：
    if (!(cfg.gs_damping > 0.0) || cfg.gs_damping > 1.0) cfg.gs_damping = 1.0;
    cfg.m_full_frame = (cfg.m_full_frame != 0) ? 1 : 0;
    cfg.final_gauge = (cfg.final_gauge != 0) ? 1 : 0;
    cfg.tolerance_relative = (cfg.tolerance_relative != 0) ? 1 : 0;
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
    m->grid = cfg.grid;   // M7-C-001: 实际 G 进入几何/模型（已校验 ==8）
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
    // 禁止跨 tile 边界节点合并（proximity-alias）。
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
        // cell 中心间距（target order 像素尺度推导; 用 ldexp 避免 1u<<k 对 k>=32 的 UB）
        const double nside = std::ldexp(1.0, cfg.target_order + 9);
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
    // 排异锚点：迭代剔除/阈值契约见 docs/science/REJECTION.md；权重来源见
    // docs/science/PHASE2_UPM.md（SCI-UPM-WEIGHT-001）。
    // 权重 raw_w = quality × control_ivar（SCI-UPM-WEIGHT-001）；
    // per-control 归一化后 × huber_w。legacy snr² 路径仅在
    // use_ivar_weight=0（ablation/诊断）时由 p2_upm_raw_weight 选择。
    // M 更新（固定 C）：M_k = Σ w (y - C) / Σ w
    // C 更新（固定 M，逐帧 CG）：
    // min Σ_k w_ik (r_ik - C_ik)^2 + λs Σ_{k~l} (C_ik - C_il)^2 + λ0 Σ C_ik^2
    std::vector<double> M(K, 0.0);
    const double anchor = std::max(0.0, cfg.zero_anchor_weight);
    const double lambda_s = std::max(0.0, cfg.smoothing_lambda);

    // RELEASE-02 FIX-REGRESS（P2a-4 收敛修正）：m_full_frame=1 的 joint-LS
    // 不动点（C_ref≡0 gauge 下）是 M_k = 参考帧观测均值。若 M 从 0 冷启动，
    // 参考帧残差 r_ref = y_ref − M 在首轮被判为离群 → Huber 权重 w_ref 塌缩到
    // ~1/300·w_nonref，M 沿 r_ref 方向以极慢速率漂移：100 轮后仍留数百 ADU
    // 帧间残差（p2001_real_nodes 实测 mean|Δ| 448.018，converged=0）。
    // 用「参考帧观测均值」做 M 初值（= legacy 的 gauge 一致解）使 r_ref≈0，
    // 权重回到对称；full-frame 加权不动点不变（收敛后 m_full_frame 0/1 同解），
    // 只修正收敛路径。m_full_frame=0（W2 冻结 legacy）不进入本分支 ⇒ 逐位不变。
    if (cfg.m_full_frame) {
        for (std::size_t k = 0; k < K; ++k) {
            if (m->controls[k].obs_idx.empty()) continue;
            const std::size_t comp = m->control_component[k];
            const bool use_ref = (comp != kNoData);
            const std::uint64_t rf =
                use_ref ? m->component_ref_frame[comp] : 0ULL;
            double num = 0.0;
            int n = 0;
            if (use_ref) {
                for (std::size_t ii : m->controls[k].obs_idx) {
                    const auto& o = obs[ii];
                    if (o.frame_id != rf) continue;
                    num += o.value;
                    ++n;
                }
            }
            if (n == 0) {
                for (std::size_t ii : m->controls[k].obs_idx) {
                    num += obs[ii].value;
                    ++n;
                }
            }
            if (n > 0) M[k] = num / (double)n;
        }
    }

    // per-control 归一化：需要先按 control 聚合（同 cell 多帧观测）
    // 这里直接按 obs 计算 raw 后按 control 归一化（与文档一致）
    std::vector<double> raw_w(n_obs, 0.0);
    auto compute_raw = [&]() -> int {
        std::vector<double> sums(K, 0.0);
        // 并行 worker 数来自 Runtime lease(cfg.cpu_workers, p2_session 传
        // budget.max_workers)。无 hardware_concurrency; 1 => 串行 reference。
        // 规约: worker-local tsums + 按 worker 顺序合并。确定性三档（冻结，
        // 见 docs/science/PHASE2_UPM.md §7/§9）：同配置重复=位精确+model_hash
        // exact；**跨 worker 数=1e-12 绝对容差，不是位精确**——本 per-control
        // 求和的结合顺序随 worker 切片变化（FP 加法非结合），实测 ΔC_max
        // 2.22e-15 ≈ 1 ulp @10 ADU；跨后端等价不允许。
        const int cworkers = (cfg.cpu_workers > 0) ? cfg.cpu_workers : 1;
        if (cworkers > 1) {
            std::vector<std::vector<double>> tsums((std::size_t)cworkers,
                                                   std::vector<double>(K, 0.0));
            std::atomic<int> rcfail{0};
            {
                std::vector<std::thread> pool;
                pool.reserve((std::size_t)cworkers);
                for (int tid = 0; tid < cworkers; ++tid) {
                    pool.emplace_back([&, tid]() {
                        const std::uint64_t start = (n_obs * (std::uint64_t)tid) / (std::uint64_t)cworkers;
                        const std::uint64_t end = (n_obs * (std::uint64_t)(tid + 1)) / (std::uint64_t)cworkers;
                        for (std::uint64_t i = start; i < end; ++i) {
                            const int rc = p2_upm_raw_weight(&obs[i], &cfg, &raw_w[i]);
                            if (rc != 0) { rcfail.store(rc); continue; }
                            tsums[(std::size_t)tid][m->control_by_id[obs[i].control_id]] += raw_w[i];
                        }
                    });
                }
                for (auto& th : pool) th.join();
            }
            if (rcfail.load() != 0) return rcfail.load();
            for (int t = 0; t < cworkers; ++t)
                for (std::size_t k = 0; k < K; ++k)
                    sums[k] += tsums[(std::size_t)t][k];
        } else
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
            // FIX-UPMSCALE（RELEASE-02）：尺度无关判据。per-control 归一化
            //   w_cell = w_UPM / (Σ_cell w_UPM) × control_reliability
            // 的数学定义域是「Σ_cell w_UPM > 0 且有限」，不是「Σ 大于某个
            // 绝对常数」。旧门 sums[ck] > 1e-12 是绝对阈值：生产
            // control_ivar 中位 ≈5.6e-22（Σ 中位 ≈5.1e-21）使全部 control
            // 判假 ⇒ 100% 权重清零 ⇒ C 场恒 0、p2_upm_fit 空操作、帧间不可
            // 比（接缝根因）。归一化表达式本身不变（raw_w / sums *
            // reliability），只把「是否执行归一化」的判据换成尺度无关形式，
            // 因此数学语义逐位不变。
            // - 真零权重（如 quality_factor=0 的全部观测）Σ=0 ⇒ 仍得 0，
            //   不被当作有效观测（不生成值）；
            // - 非法/缺失 ivar 已由 p2_upm_raw_weight rc=2 显式拦下，此处
            //   不承担 fail-closed 职责，不放宽任何校验。
            const double s = sums[ck];
            if (s > 0.0 && std::isfinite(s))
                raw_w[i] = raw_w[i] / s * m->controls[ck].reliability;
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

    // RELEASE-02 P2a-2：w 提升到迭代循环外——末端残差场 gauge 必须复用
    // 最后一轮的拟合权重（"拟合权重 = 叠加权重"同源，q2-snr-smooth §5）。
    std::vector<double> w(n_obs, 0.0);
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
        // 逐 obs 独立 w 计算(per-obs 写 w[i] 不相交); std::thread + lease worker。
        {
            const int cworkers = (cfg.cpu_workers > 0) ? cfg.cpu_workers : 1;
            if (cworkers > 1) {
                std::vector<std::thread> pool;
                pool.reserve((std::size_t)cworkers);
                for (int tid = 0; tid < cworkers; ++tid) {
                    pool.emplace_back([&, tid]() {
                        const std::uint64_t start = (n_obs * (std::uint64_t)tid) / (std::uint64_t)cworkers;
                        const std::uint64_t end = (n_obs * (std::uint64_t)(tid + 1)) / (std::uint64_t)cworkers;
                        for (std::uint64_t i = start; i < end; ++i) {
                            const std::size_t ck = m->control_by_id[obs[i].control_id];
                            const double r = obs[i].value - M[ck] -
                                             m->C[m->frame_index[obs[i].frame_id]][ck];
                            const double sigma_eff =
                                std::max(std::fabs(obs[i].uncertainty), cfg.sigma_floor);
                            w[i] = raw_w[i] * huber_w(r / sigma_eff, cfg.huber_delta);
                        }
                    });
                }
                for (auto& th : pool) th.join();
            } else {
                for (std::uint64_t i = 0; i < n_obs; ++i) {
                    const std::size_t ck = m->control_by_id[obs[i].control_id];
                    // Huber 作用于标准化残差 z = r / sigma_eff：
                    // sigma_eff = max(观测 uncertainty, sigma_floor)，delta 取
                    // 无量纲 1.345。不得用 raw residual 直接比较 delta——raw 尺度
                    // 下所有残差都落在线性区，robust 权重永不生效；污染观测
                    // （patch 星污染 → residual 大而 uncertainty 有限）被强烈降权。
                    const double r = obs[i].value - M[ck] -
                                     m->C[m->frame_index[obs[i].frame_id]][ck];
                    const double sigma_eff =
                        std::max(std::fabs(obs[i].uncertainty), cfg.sigma_floor);
                    w[i] = raw_w[i] * huber_w(r / sigma_eff, cfg.huber_delta);
                }
            }
        }
        // 2. M 更新（固定 C）：每分量 gauge = 分量内最小 frame_id C=0 →
        // M 由该分量参考帧观测定义；参考帧未覆盖节点用全部帧（延拓）。
        double max_dM = 0.0;
        // 逐 control 独立 M 更新(每 control 的 obs 聚合整块由单线程完成,
        // 逐 k 写 M[k] 不相交); max 归约用 per-worker 局部 + join 合并(位精确)。
        {
            const std::size_t nK = m->controls.size();
            const int cworkers = (cfg.cpu_workers > 0) ? cfg.cpu_workers : 1;
            if (cworkers > 1) {
                std::vector<double> tmax((std::size_t)cworkers, 0.0);
                std::vector<std::thread> pool;
                pool.reserve((std::size_t)cworkers);
                for (int tid = 0; tid < cworkers; ++tid) {
                    pool.emplace_back([&, tid]() {
                        const std::uint64_t start = (nK * (std::uint64_t)tid) / (std::uint64_t)cworkers;
                        const std::uint64_t end = (nK * (std::uint64_t)(tid + 1)) / (std::uint64_t)cworkers;
                        double lmax = 0.0;
                        for (std::size_t k = start; k < end; ++k) {
                            double num = 0.0, den = 0.0;
                            const std::size_t comp = m->control_component[k];
                            // 无观测几何节点不参与数据图，component=sentinel；
                            // 其 M 由全部帧加权（无参考帧语义）定义。
                            // RELEASE-02 P2a-4：m_full_frame=1 时所有节点
                            // 一律用全帧加权 M（joint-LS 不动点，方差更小、
                            // 无参考帧结构共模；c-delta-ruling §3.2/§6.2 M2）。
                            if (comp == kNoData || cfg.m_full_frame) {
                                for (std::size_t ii : m->controls[k].obs_idx) {
                                    const auto& o = obs[ii];
                                    const double c =
                                        m->C[m->frame_index[o.frame_id]][k];
                                    num += w[ii] * (o.value - c);
                                    den += w[ii];
                                }
                                if (den > 1e-12) {
                                    const double Mnew = num / den;
                                    lmax = std::max(lmax, std::fabs(Mnew - M[k]));
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
                                lmax = std::max(lmax, std::fabs(Mnew - M[k]));
                                M[k] = Mnew;
                            }
                        }
                        tmax[(std::size_t)tid] = lmax;
                    });
                }
                for (auto& th : pool) th.join();
                for (double v : tmax) max_dM = std::max(max_dM, v);
            } else {
                for (std::size_t k = 0; k < nK; ++k) {
                    double num = 0.0, den = 0.0;
                    const std::size_t comp = m->control_component[k];
                    // 无观测几何节点不参与数据图，component=sentinel；
                    // 其 M 由全部帧加权（无参考帧语义）定义。
                    // RELEASE-02 P2a-4：m_full_frame=1 时所有节点一律用
                    // 全帧加权 M（joint-LS 不动点）。
                    if (comp == kNoData || cfg.m_full_frame) {
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
            }
        }
        // 3. C 更新（固定 M；逐帧 CG + 参考帧 gauge）
        double max_dC = 0.0;
        std::vector<std::uint64_t> fids;
        fids.reserve(m->frame_index.size());
        for (const auto& kv : m->frame_index) fids.push_back(kv.first);
        // 逐 frame 独立 C 更新 + CG（每 frame 的 rhs/obs_w/C[f]/x 全 per-frame；
        // cg_solve_frame 读只读共享 adj/K/lambda_s/anchor，写各 frame 自身；仅 max 归约
        // 用 per-worker 局部 + join 合并）。取消点在迭代边界(上层循环)检查。
        {
            const std::size_t nf = fids.size();
            const int cworkers = (cfg.cpu_workers > 0) ? cfg.cpu_workers : 1;
            if (cworkers > 1) {
                std::vector<double> tmax((std::size_t)cworkers, 0.0);
                std::vector<std::thread> pool;
                pool.reserve((std::size_t)cworkers);
                for (int tid = 0; tid < cworkers; ++tid) {
                    pool.emplace_back([&, tid]() {
                        const std::uint64_t start = (nf * (std::uint64_t)tid) / (std::uint64_t)cworkers;
                        const std::uint64_t end = (nf * (std::uint64_t)(tid + 1)) / (std::uint64_t)cworkers;
                        double lmax = 0.0;
                        for (std::size_t fi_ = start; fi_ < end; ++fi_) {
                            const std::uint64_t frame_id = fids[fi_];
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
                            // RELEASE-02 P2a-2 阻尼 Gauss-Seidel：x ←
                            // (1-α)·x_old + α·x_new。naive α=1 在链式/二部
                            // 覆盖图上有特征值 -1（周期 2 振荡，q2-snr-smooth §4.2）。
                            const std::vector<double> x_old = m->C[f];
                            std::vector<double> x = m->C[f];
                            cg_solve_frame(f, x, rhs);
                            if (cfg.gs_damping < 1.0) {
                                const double gsa = cfg.gs_damping;
                                for (std::size_t k = 0; k < K; ++k)
                                    x[k] = (1.0 - gsa) * x_old[k] + gsa * x[k];
                            }
                            for (std::size_t k = 0; k < K; ++k)
                                lmax = std::max(lmax, std::fabs(x[k] - m->C[f][k]));
                            m->C[f] = std::move(x);
                        }
                        tmax[(std::size_t)tid] = lmax;
                    });
                }
                for (auto& th : pool) th.join();
                for (double v : tmax) max_dC = std::max(max_dC, v);
            } else {
                for (std::size_t fi_ = 0; fi_ < nf; ++fi_) {
                    const std::uint64_t frame_id = fids[fi_];
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
                    // RELEASE-02 P2a-2 阻尼 Gauss-Seidel（见并行分支同注）。
                    const std::vector<double> x_old = m->C[f];
                    std::vector<double> x = m->C[f];
                    cg_solve_frame(f, x, rhs);
                    if (cfg.gs_damping < 1.0) {
                        const double gsa = cfg.gs_damping;
                        for (std::size_t k = 0; k < K; ++k)
                            x[k] = (1.0 - gsa) * x_old[k] + gsa * x[k];
                    }
                    for (std::size_t k = 0; k < K; ++k)
                        max_dC = std::max(max_dC, std::fabs(x[k] - m->C[f][k]));
                    m->C[f] = std::move(x);
                }
            }
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
        // RELEASE-02 P2a-3：绝对 1e-6 在 max|M|~3e15 时低于 ULP(0.5)
        // 5-8 个数量级，原理上不可达（iterations=100,converged=0）。
        // tolerance_relative=1 时改为相对判据：
        //   阈值 = tolerance × max(scale, 1.0)，scale = max|M| / max|C|；
        // max(...,1.0) 保证小尺度合成数据与 legacy 绝对判据逐位等价。
        // tolerance_relative=0 时 tol_M=tol_C=cfg.tolerance（legacy，逐位不变）。
        double tol_M = cfg.tolerance;
        double tol_C = cfg.tolerance;
        if (cfg.tolerance_relative) {
            double scale_M = 0.0, scale_C = 0.0;
            for (std::size_t k = 0; k < K; ++k)
                scale_M = std::max(scale_M, std::fabs(M[k]));
            for (std::size_t f = 0; f < F; ++f)
                for (std::size_t k = 0; k < K; ++k)
                    scale_C = std::max(scale_C, std::fabs(m->C[f][k]));
            tol_M = cfg.tolerance * std::max(scale_M, 1.0);
            tol_C = cfg.tolerance * std::max(scale_C, 1.0);
        }
        if (max_dM < tol_M && max_dC < tol_C) {
            m->converged = 1;
            break;
        }
    }

    // ===== RELEASE-02 FIX-REGRESS：无观测几何节点的调和延拓 =====
    // build_geo 的全 coverage 节点里，单帧区/无覆盖 cell 没有 ≥2 帧观测
    // （obs_idx 为空），其 component=sentinel，M/C 更新无数据项 ⇒ 在 λs=0
    // 下恒为 0。双线性插值到这些 cell 时，"假 0" 会在相邻覆盖区制造大偏差
    // （p2001_real_nodes 退化场景实测角区 mean|Δ|≈4.2 ADU，占阈值 5 的 84%）。
    // SCI PHASE2_UPM「单帧区 harmonic continuation」要求用观测邻居延拓；
    // λs>0 时由 CG 平滑隐式完成，λs=0（生产缺省）时此路径不可达 ⇒ 显式用
    // 观测邻居做 Jacobi 调和平均（等价 λs→0+ 的延拓极限）。只改写无观测
    // 节点，有观测节点的解逐位不变。孤立（无观测邻居）节点保持 0。
    {
        std::vector<std::uint8_t> g_known(K, 0);
        for (std::size_t k = 0; k < K; ++k)
            if (!m->controls[k].obs_idx.empty()) g_known[k] = 1;
        auto harmonic_extend = [&](std::vector<double>& field) {
            std::vector<std::uint8_t> kn = g_known;
            for (int pass = 0; pass < 64; ++pass) {
                bool changed = false;
                for (std::size_t k = 0; k < K; ++k) {
                    if (kn[k] == 1) continue;
                    double s = 0.0;
                    int n = 0;
                    for (std::size_t nb : m->adj[k])
                        if (kn[nb] == 1) { s += field[nb]; ++n; }
                    if (n > 0) {
                        field[k] = s / (double)n;
                        kn[k] = 2;
                        changed = true;
                    }
                }
                for (std::size_t k = 0; k < K; ++k)
                    if (kn[k] == 2) kn[k] = 1;
                if (!changed) break;
            }
        };
        harmonic_extend(M);
        for (std::size_t f = 0; f < F; ++f) harmonic_extend(m->C[f]);
    }

    for (std::size_t k = 0; k < K; ++k) m->controls[k].M = M[k];
    // 连通分量已在求解前建立（component_count_solve / component_ref_frame），
    // 每分量独立 gauge；此处不重复统计。

    // ===== RELEASE-02 P2a-2 末端残差场 gauge（q2-snr-smooth §5）=====
    //   R_k = [Σ_i w_i (y_i − C_{f(i),k})] / Σ_i w_i   （复用最后一轮拟合 w，
    //         拟合权重 = 叠加权重同源）
    //   G_k = R_k − M_k
    // 校正变为 y − C − G ⇒ 叠加 Σ w (y−C−G)/Σw ≡ M（单一公共场）
    // ⇒ 任意覆盖子集 / 任意权重面下边界阶跃恒 0（q2 §4 代数 + §5 数值 0.0000）。
    // 注意 G 对全部帧相同，不是 gauge 常数（全局常数对接缝无效，c-delta §4）。
    if (cfg.final_gauge) {
        m->gauge.assign(K, 0.0);
        std::vector<std::uint8_t> g_known(K, 0);
        for (std::size_t k = 0; k < K; ++k) {
            double num = 0.0, den = 0.0;
            for (std::size_t ii : m->controls[k].obs_idx) {
                const auto& o = obs[ii];
                const std::size_t f = m->frame_index[o.frame_id];
                num += w[ii] * (o.value - m->C[f][k]);
                den += w[ii];
            }
            if (den > 0.0 && std::isfinite(den) && std::isfinite(num)) {
                const double g = num / den - M[k];
                if (std::isfinite(g)) {
                    m->gauge[k] = g;
                    g_known[k] = 1;
                }
            }
        }
        // 无观测几何节点的 G 由邻接图调和延拓（避免 0 在插值面上制造假台阶）。
        // Jacobi 平均；孤立节点保持 0（无数据面，无接缝贡献）。
        for (int pass = 0; pass < 64; ++pass) {
            bool changed = false;
            for (std::size_t k = 0; k < K; ++k) {
                if (g_known[k] == 1) continue;
                double s = 0.0;
                int n = 0;
                for (std::size_t nb : m->adj[k]) {
                    if (g_known[nb] == 1) {
                        s += m->gauge[nb];
                        ++n;
                    }
                }
                if (n > 0) {
                    m->gauge[k] = s / (double)n;
                    g_known[k] = 2;
                    changed = true;
                }
            }
            for (std::size_t k = 0; k < K; ++k)
                if (g_known[k] == 2) g_known[k] = 1;
            if (!changed) break;
        }
    }

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
        // final_gauge 关闭时 m->gauge 为空 ⇒ payload 与 legacy 逐位一致
        // （既有 model_hash 冻结门不受影响）；启用时 G 必须进入 hash。
        if (!m->gauge.empty()) {
            payload += "|G";
            for (std::size_t k = 0; k < K; ++k)
                payload += fmt(m->gauge[k]) + ";";
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
    j["grid"] = m->grid;
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
    // RELEASE-02 P2a provenance（求解器行为；旧读取方忽略未知键，向后兼容）
    j["gs_damping"] = m->cfg.gs_damping;
    j["m_full_frame"] = m->cfg.m_full_frame;
    j["final_gauge"] = m->cfg.final_gauge;
    j["tolerance_relative"] = m->cfg.tolerance_relative;
    j["model_hash"] = m->info.model_hash;
    j["input_manifest_hash"] = m->input_manifest_hash;
    j["iterations"] = m->iterations;
    j["objective"] = m->objective;
    j["converged"] = m->converged;
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
    // RELEASE-02 P2a-2：末端残差场 G（逐 control，对所有帧相同）必须持久化，
    // 否则 upm-apply 重开模型后不施加 gauge（sparse 路径）。空 = legacy。
    if (!m->gauge.empty()) {
        nlohmann::json Gj = nlohmann::json::array();
        for (std::size_t k = 0; k < m->gauge.size(); ++k) {
            const double v = m->gauge[k];
            if (v != 0.0) Gj.push_back({k, v});
        }
        j["gauge"] = Gj;
    }
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
        // RELEASE-02 P2a provenance（旧文件无键 → legacy 默认）
        m->cfg.gs_damping = j.value("gs_damping", 1.0);
        m->cfg.m_full_frame = j.value("m_full_frame", 0);
        m->cfg.final_gauge = j.value("final_gauge", 0);
        m->cfg.tolerance_relative = j.value("tolerance_relative", 0);
        m->input_manifest_hash =
            j.value("input_manifest_hash", std::string());
        const std::string h =
            j.value("model_hash", std::string(64, '0'));
        std::strncpy(m->info.model_hash, h.c_str(),
                     sizeof(m->info.model_hash) - 1);
        m->info.model_hash[sizeof(m->info.model_hash) - 1] = '\0';
        m->iterations = j.value("iterations", 0);
        m->objective = j.value("objective", 0.0);
        // 旧模型文件无该键 → 0（未经证明的收敛，fail-closed 读法）。
        m->converged = j.value("converged", 0);
        m->component_count = j.value("component_count", (std::size_t)1);
        m->info.component_count = (std::uint32_t)m->component_count;
        m->grid = (int)j.value("grid", 8);
        if (m->grid != 8) { delete m; return 1; }   // M7-C-001 打开处网格校验
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
        // RELEASE-02 P2a-2：恢复末端残差场 G。旧文件无 "gauge" 键 ⇒ 空向量
        // （legacy：calibrate_block 不施加 G，行为逐位不变）。类型非法一律拒绝。
        if (j.contains("gauge")) {
            if (!j["gauge"].is_array()) {
                delete m;
                return 1;
            }
            m->gauge.assign(K, 0.0);
            for (const auto& item : j["gauge"]) {
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
                m->gauge[k] = item[1].get<double>();
            }
            // 全零 gauge（序列化只写非零）→ 视作 legacy 空向量，保持
            // calibrate_block 的"G 空 = 不施加"语义与 hash 一致性。
            bool any = false;
            for (double v : m->gauge)
                if (v != 0.0) { any = true; break; }
            if (!any) m->gauge.clear();
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

// M7-H-101：迭代收敛状态的只读访问器（不改 P2ModelInfo 冻结布局）。
int p2_upm_convergence(const void* model, std::uint64_t* out_iterations,
                       double* out_objective, int* out_converged) {
    if (model == nullptr) return 1;
    const Model* m = static_cast<const Model*>(model);
    if (out_iterations != nullptr)
        *out_iterations = (std::uint64_t)m->iterations;
    if (out_objective != nullptr) *out_objective = m->objective;
    // 0 = 迭代耗尽（或旧模型未记录）；1 = 在 max_iterations 内达 tolerance。
    if (out_converged != nullptr) *out_converged = m->converged;
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
    // 未知 frame_id 必须显式失败，禁止回退 frame 0 参数
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
        // RELEASE-02 P2a-2：末端残差场 G 对全部帧施加同一扣除（G 为空 = legacy）。
        // corrected = y − C − G；叠加 Σw(y−C−G)/Σw ≡ M ⇒ 覆盖子集突变无阶跃。
        double g = 0.0;
        if (!m->gauge.empty())
            g = evaluate_field_row(m, m->gauge, tile, (int)x, (int)y);
        output_signal[i] = input_signal[i] - c - g;
    }
    return 0;
}

double p2_upm_evaluate_c(const void* model, std::uint64_t frame_id,
                         std::uint64_t leaf_ipix) {
    // 不可用一律 NaN（与"未知 frame_id"同哨兵）。0.0 是 gauge 参考帧的
    // 合法 C 值，用之作哨兵会让调用方把"无模型"读成"无校正"（fail-open）。
    if (model == nullptr) return std::numeric_limits<double>::quiet_NaN();
    const Model* m = static_cast<const Model*>(model);
    const auto it = m->frame_index.find(frame_id);
    // 未知 frame_id 返回 NaN（显式不可用），禁止用
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
    if (target_order < 0) target_order = static_cast<int>(m->info.target_order);
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
                    // RELEASE-02 P2a-2：稠密缓存必须与 sparse calibrate_block
                    // 逐位等价 ⇒ 同一公共 gauge G 折入缓存值（G 空 = legacy）。
                    if (!m->gauge.empty())
                        node[gy][gx] += m->gauge[it->second];
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
    // dense tile 求值并行(std::thread; workers 由调用方传 lease, 无 OpenMP)。
    const int nw = (workers > 0) ? workers : 1;
    for (std::size_t f = 0; f < m->C.size(); ++f) {
        for (std::size_t base = 0; base < tiles.size(); base += kChunk) {
            const std::size_t n = std::min(kChunk, tiles.size() - base);
            std::vector<double> buf(n * kLeafPx);
            if (nw > 1) {
                std::vector<std::thread> pool;
                pool.reserve((std::size_t)nw);
                for (int tid = 0; tid < nw; ++tid) {
                    pool.emplace_back([&, tid]() {
                        const std::int64_t start = (std::int64_t)((n * (std::uint64_t)tid) / (std::uint64_t)nw);
                        const std::int64_t end = (std::int64_t)((n * (std::uint64_t)(tid + 1)) / (std::uint64_t)nw);
                        for (std::int64_t j = start; j < end; ++j) {
                            compute_tile(f, tiles[base + (std::size_t)j],
                                         buf.data() + (std::size_t)j * kLeafPx, kLeafPx);
                        }
                    });
                }
                for (auto& th : pool) th.join();
            } else {
                for (std::int64_t j = 0; j < (std::int64_t)n; ++j) {
                    compute_tile(f, tiles[base + (std::size_t)j],
                                 buf.data() + (std::size_t)j * kLeafPx, kLeafPx);
                }
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

// ===========================================================================
// V6 目标态：UPM 乘法/加性分离求解器（ALG-P2S-UPM.1..8）
// 见 lib/algorithms/coverage/include/astro/phase2/upm.h 顶部契约与 docs/contracts/v6/frozen。
// ===========================================================================
namespace {

const double kMaPiHalf = 1.57079632679489661923132169163975144209858469968755;
const double kMaKCcorrFrozenInDomain = 1.4;   // FZ-PROV-KCORR-VALUE

struct MaDsu {
    std::vector<int> parent;
    void init(int n) {
        parent.resize((std::size_t)n);
        for (int i = 0; i < n; ++i) parent[(std::size_t)i] = i;
    }
    int find(int x) {
        while (parent[(std::size_t)x] != x) {
            parent[(std::size_t)x] = parent[(std::size_t)parent[(std::size_t)x]];
            x = parent[(std::size_t)x];
        }
        return x;
    }
    void unite(int a, int b) {
        const int ra = find(a), rb = find(b);
        if (ra != rb) parent[(std::size_t)rb] = ra;
    }
};

// Cyclic Jacobi eigen-decomposition（对称矩阵，row-major n×n）。
// A 按值传入可被覆盖；evals 输出特征值；evecs 非空时输出 n×n row-major，
// 第 k 列 = 第 k 个特征向量。返回 false 仅当 n<0。
bool ma_eig(std::vector<double> A, int n, std::vector<double>& evals,
            std::vector<double>* evecs) {
    if (n < 0) return false;
    evals.assign((std::size_t)(n > 0 ? n : 0), 0.0);
    std::vector<double> V;
    if (evecs != nullptr) {
        V.assign((std::size_t)n * (std::size_t)n, 0.0);
        for (int i = 0; i < n; ++i) V[(std::size_t)i * (std::size_t)n + (std::size_t)i] = 1.0;
    }
    const int max_sweeps = 200;
    for (int sweep = 0; sweep < max_sweeps; ++sweep) {
        double off = 0.0;
        for (int p = 0; p < n; ++p)
            for (int q = p + 1; q < n; ++q) {
                const double v = A[(std::size_t)p * (std::size_t)n + (std::size_t)q];
                off += v * v;
            }
        if (off <= 0.0) break;
        for (int p = 0; p < n; ++p) {
            for (int q = p + 1; q < n; ++q) {
                const double apq = A[(std::size_t)p * (std::size_t)n + (std::size_t)q];
                if (!std::isfinite(apq) || std::fabs(apq) < 1e-300) continue;
                const double app = A[(std::size_t)p * (std::size_t)n + (std::size_t)p];
                const double aqq = A[(std::size_t)q * (std::size_t)n + (std::size_t)q];
                const double theta = (aqq - app) / (2.0 * apq);
                const double sgn = (theta >= 0.0) ? 1.0 : -1.0;
                const double t = sgn / (std::fabs(theta) + std::sqrt(theta * theta + 1.0));
                const double c = 1.0 / std::sqrt(t * t + 1.0);
                const double s = t * c;
                // A <- A J（列 p,q），随后 A <- J^T A（行 p,q）：
                // 显式两步正交相似变换（Givens），保证 A_final = V^T A V，
                // 特征向量矩阵 V 可用于 V Λ^-1 V^T = A^-1。
                for (int i = 0; i < n; ++i) {
                    const double aip = A[(std::size_t)i * (std::size_t)n + (std::size_t)p];
                    const double aiq = A[(std::size_t)i * (std::size_t)n + (std::size_t)q];
                    A[(std::size_t)i * (std::size_t)n + (std::size_t)p] = c * aip - s * aiq;
                    A[(std::size_t)i * (std::size_t)n + (std::size_t)q] = s * aip + c * aiq;
                }
                for (int i = 0; i < n; ++i) {
                    const double api = A[(std::size_t)p * (std::size_t)n + (std::size_t)i];
                    const double aqi = A[(std::size_t)q * (std::size_t)n + (std::size_t)i];
                    A[(std::size_t)p * (std::size_t)n + (std::size_t)i] = c * api - s * aqi;
                    A[(std::size_t)q * (std::size_t)n + (std::size_t)i] = s * api + c * aqi;
                }
                if (evecs != nullptr) {
                    for (int i = 0; i < n; ++i) {
                        const double vip = V[(std::size_t)i * (std::size_t)n + (std::size_t)p];
                        const double viq = V[(std::size_t)i * (std::size_t)n + (std::size_t)q];
                        V[(std::size_t)i * (std::size_t)n + (std::size_t)p] = c * vip - s * viq;
                        V[(std::size_t)i * (std::size_t)n + (std::size_t)q] = s * vip + c * viq;
                    }
                }
                A[(std::size_t)p * (std::size_t)n + (std::size_t)q] = 0.0;
                A[(std::size_t)q * (std::size_t)n + (std::size_t)p] = 0.0;
                (void)app;
                (void)aqq;
            }
        }
    }
    for (int i = 0; i < n; ++i) evals[(std::size_t)i] = A[(std::size_t)i * (std::size_t)n + (std::size_t)i];
    if (evecs != nullptr) *evecs = V;
    return true;
}

// Cholesky 解 A x = b（A row-major n×n SPD，按值传入可加 jitter）。b 输出 x。
bool ma_chol_solve(std::vector<double> A, std::vector<double>& b, int n) {
    if (n <= 0) return true;
    for (int attempt = 0; attempt < 4; ++attempt) {
        std::vector<double> L((std::size_t)n * (std::size_t)n, 0.0);
        bool ok = true;
        for (int i = 0; i < n && ok; ++i) {
            for (int j = 0; j <= i; ++j) {
                double sum = A[(std::size_t)i * (std::size_t)n + (std::size_t)j];
                for (int k = 0; k < j; ++k)
                    sum -= L[(std::size_t)i * (std::size_t)n + (std::size_t)k] *
                           L[(std::size_t)j * (std::size_t)n + (std::size_t)k];
                if (i == j) {
                    if (!(sum > 0.0) || !std::isfinite(sum)) { ok = false; break; }
                    L[(std::size_t)i * (std::size_t)n + (std::size_t)i] = std::sqrt(sum);
                } else {
                    L[(std::size_t)i * (std::size_t)n + (std::size_t)j] =
                        sum / L[(std::size_t)j * (std::size_t)n + (std::size_t)j];
                }
            }
        }
        if (ok) {
            std::vector<double> y((std::size_t)n, 0.0);
            for (int i = 0; i < n; ++i) {
                double sum = b[(std::size_t)i];
                for (int k = 0; k < i; ++k)
                    sum -= L[(std::size_t)i * (std::size_t)n + (std::size_t)k] * y[(std::size_t)k];
                y[(std::size_t)i] = sum / L[(std::size_t)i * (std::size_t)n + (std::size_t)i];
            }
            for (int i = n - 1; i >= 0; --i) {
                double sum = y[(std::size_t)i];
                for (int k = i + 1; k < n; ++k)
                    sum -= L[(std::size_t)k * (std::size_t)n + (std::size_t)i] * b[(std::size_t)k];
                b[(std::size_t)i] = sum / L[(std::size_t)i * (std::size_t)n + (std::size_t)i];
            }
            return true;
        }
        double scale = 0.0;
        for (int i = 0; i < n; ++i)
            scale = std::max(scale, std::fabs(A[(std::size_t)i * (std::size_t)n + (std::size_t)i]));
        const double jit = (scale > 0.0 ? scale : 1.0) * 1e-12 * (double)(attempt + 1);
        for (int i = 0; i < n; ++i) A[(std::size_t)i * (std::size_t)n + (std::size_t)i] += jit;
    }
    return false;
}

struct MaObsRec {
    int fi;
    int ci;
    double y;
    double ivar;
};

struct MaModel {
    P2UpmMaConfig cfg;
    P2UpmMaInfo info;
    std::vector<std::uint64_t> frame_ids;    // 升序
    std::vector<std::uint64_t> control_ids;  // 升序
    std::map<std::uint64_t, std::size_t> frame_index;
    std::map<std::uint64_t, std::size_t> control_index;
    std::vector<std::size_t> frame_component;
    std::vector<std::size_t> control_component;
    std::vector<std::uint64_t> component_ref_frame;   // 分量 -> 参考 frame_id
    std::vector<int> component_additive_only;
    std::vector<long long> free_of_full;     // full 下标 -> free 下标（-1 = gauge 固定）
    std::vector<std::size_t> full_of_free;   // free 下标 -> full 下标
    std::size_t n_full{0};
    std::size_t n_free{0};
    std::vector<double> theta_full;          // size n_full（gauge 固定项写入约定值）
    std::vector<double> C_theta;             // n_free × n_free row-major
    std::vector<double> sigma;               // 奇异值降序
    double kappa{0.0};
    std::size_t rank{0};
    int iterations{0};
    std::string model_hash;
};

// 在给定 theta_full 与观测上计算（统计）正规矩阵 H = Jᵀ W J（W=diag(ivar)）
// 与（可选）robust 目标。cols 复用见调用方。
void ma_normal_matrix(const MaModel& m, const std::vector<MaObsRec>& recs,
                      std::vector<double>& H) {
    const std::size_t np = m.control_ids.size();
    const std::size_t nf = m.frame_ids.size();
    const std::size_t n = m.n_free;
    H.assign(n * n, 0.0);
    for (const MaObsRec& o : recs) {
        const double gk = m.theta_full[np + (std::size_t)o.fi];
        const double sp = m.theta_full[(std::size_t)o.ci];
        long long js[3];
        double ja[3];
        int nc = 0;
        const long long a_s = m.free_of_full[(std::size_t)o.ci];
        if (a_s >= 0) { js[nc] = a_s; ja[nc] = gk; ++nc; }
        const long long a_g = m.free_of_full[np + (std::size_t)o.fi];
        if (a_g >= 0) { js[nc] = a_g; ja[nc] = sp; ++nc; }
        const long long a_b = m.free_of_full[np + nf + (std::size_t)o.fi];
        if (a_b >= 0) { js[nc] = a_b; ja[nc] = 1.0; ++nc; }
        for (int a = 0; a < nc; ++a) {
            for (int b = 0; b < nc; ++b)
                H[(std::size_t)js[a] * n + (std::size_t)js[b]] += o.ivar * ja[a] * ja[b];
        }
    }
}

// 加权 Jacobian J_w = W^{1/2} J（m×n row-major，W=C_in^-1=diag(control_ivar)）。
void ma_weighted_jacobian(const MaModel& m, const std::vector<MaObsRec>& recs,
                          std::vector<double>& Jw) {
    const std::size_t np = m.control_ids.size();
    const std::size_t nf = m.frame_ids.size();
    const std::size_t n = m.n_free;
    Jw.assign(recs.size() * n, 0.0);
    for (std::size_t i = 0; i < recs.size(); ++i) {
        const MaObsRec& o = recs[i];
        const double gk = m.theta_full[np + (std::size_t)o.fi];
        const double sp = m.theta_full[(std::size_t)o.ci];
        const double sw = std::sqrt(o.ivar);
        const long long a_s = m.free_of_full[(std::size_t)o.ci];
        if (a_s >= 0) Jw[i * n + (std::size_t)a_s] = sw * gk;
        const long long a_g = m.free_of_full[np + (std::size_t)o.fi];
        if (a_g >= 0) Jw[i * n + (std::size_t)a_g] = sw * sp;
        const long long a_b = m.free_of_full[np + nf + (std::size_t)o.fi];
        if (a_b >= 0) Jw[i * n + (std::size_t)a_b] = sw;
    }
}

// J_w 的奇异值（全精度）：一步 Jacobi（Hestenes one-sided）直接正交化 J_w 的列，
// 收敛后列范数即奇异值。对 J_w 直接求 σ_i，避免经 (JᵀJ) 平方导致小奇异值精度
// 损失（FZ-AP2S-RANK-RTOL=1e-10 需要 σ 自身精度）。mrows<n 时对 J_wᵀ 做（σ 不变）。
bool ma_singular_values(const std::vector<double>& Jw, std::size_t mrows,
                        std::size_t n, std::vector<double>& sigma) {
    const bool transposed = mrows < n;
    const std::size_t R = transposed ? n : mrows;
    const std::size_t C = transposed ? mrows : n;
    if (R == 0 || C == 0) { sigma.clear(); return true; }
    std::vector<double> A(R * C, 0.0);
    if (!transposed) {
        A = Jw;
    } else {
        for (std::size_t i = 0; i < mrows; ++i)
            for (std::size_t j = 0; j < n; ++j) A[j * C + i] = Jw[i * n + j];
    }
    for (int sweep = 0; sweep < 100; ++sweep) {
        double maxoff = 0.0;
        for (std::size_t p = 0; p < C; ++p) {
            for (std::size_t q = p + 1; q < C; ++q) {
                double alpha = 0.0, beta = 0.0, gamma = 0.0;
                for (std::size_t i = 0; i < R; ++i) {
                    const double ap = A[i * C + p];
                    const double aq = A[i * C + q];
                    alpha += ap * ap;
                    beta += aq * aq;
                    gamma += ap * aq;
                }
                if (!(alpha > 0.0) || !(beta > 0.0)) continue;
                const double g = std::fabs(gamma) / std::sqrt(alpha * beta);
                maxoff = std::max(maxoff, g);
                if (!(g > 1e-15) || !std::isfinite(g)) continue;
                const double zeta = (beta - alpha) / (2.0 * gamma);
                const double sgn = (zeta >= 0.0) ? 1.0 : -1.0;
                const double t = sgn / (std::fabs(zeta) + std::sqrt(1.0 + zeta * zeta));
                const double c = 1.0 / std::sqrt(1.0 + t * t);
                const double s = c * t;
                for (std::size_t i = 0; i < R; ++i) {
                    const double ap = A[i * C + p];
                    const double aq = A[i * C + q];
                    A[i * C + p] = c * ap - s * aq;
                    A[i * C + q] = s * ap + c * aq;
                }
            }
        }
        if (maxoff <= 1e-15) break;
    }
    sigma.assign(C, 0.0);
    for (std::size_t j = 0; j < C; ++j) {
        double s = 0.0;
        for (std::size_t i = 0; i < R; ++i) s += A[i * C + j] * A[i * C + j];
        sigma[j] = std::sqrt(s);
    }
    std::sort(sigma.begin(), sigma.end(), std::greater<double>());
    return true;
}

double ma_objective(const MaModel& m, const std::vector<MaObsRec>& recs) {
    const std::size_t np = m.control_ids.size();
    const std::size_t nf = m.frame_ids.size();
    double obj = 0.0;
    for (const MaObsRec& o : recs) {
        const double model = m.theta_full[np + (std::size_t)o.fi] * m.theta_full[(std::size_t)o.ci] +
                             m.theta_full[np + nf + (std::size_t)o.fi];
        const double r = o.y - model;
        const double sigma_eff = std::max(std::sqrt(1.0 / o.ivar), m.cfg.sigma_floor);
        obj += o.ivar * huber_rho(r / sigma_eff, m.cfg.huber_delta);
    }
    return obj;
}

} // namespace

extern "C" {

int p2_upm_ma_build(const P2UpmMaObservation* obs, std::uint64_t n_obs,
                    const P2UpmMaConfig* cfg_in, void** out_model) {
    if (out_model == nullptr || obs == nullptr || n_obs == 0) return 1;
    *out_model = nullptr;

    MaModel* m = new MaModel();
    std::memset(&m->cfg, 0, sizeof(m->cfg));
    if (cfg_in != nullptr) m->cfg = *cfg_in;
    P2UpmMaConfig& cfg = m->cfg;
    if (cfg.min_frames <= 0) cfg.min_frames = 2;                       // FZ-AP2S-UPM-MINFRAMES
    if (!(cfg.rank_rtol > 0.0) || !std::isfinite(cfg.rank_rtol)) cfg.rank_rtol = 1e-10;
    if (!(cfg.kappa_max > 0.0) || !std::isfinite(cfg.kappa_max)) cfg.kappa_max = 1e6;
    if (!(cfg.huber_delta > 0.0)) cfg.huber_delta = 1.345;            // FZ-UPM-CONVERGENCE
    if (cfg.max_iterations <= 0) cfg.max_iterations = 100;
    if (!(cfg.tolerance > 0.0)) cfg.tolerance = 1e-6;
    if (!(cfg.sigma_floor > 0.0)) cfg.sigma_floor = 1e-3;
    if (cfg.zero_anchor_weight < 0.0) cfg.zero_anchor_weight = 1e-3;
    if (cfg.gauge_mode != 0) { delete m; return 1; }   // 仅 min_frame_id gauge

    // ---- 观测校验（rc=2：缺/非法 control_ivar，production 禁静默回退）----
    std::set<std::uint64_t> frame_set, control_set;
    for (std::uint64_t i = 0; i < n_obs; ++i) {
        const P2UpmMaObservation& o = obs[i];
        if (!std::isfinite(o.value)) { delete m; return 1; }
        if (!(o.control_ivar > 0.0) || !std::isfinite(o.control_ivar)) { delete m; return 2; }
        frame_set.insert(o.frame_id);
        control_set.insert(o.control_id);
    }
    if (frame_set.empty() || control_set.empty()) { delete m; return 1; }

    // ---- 共享系统项按独立处理 → rc=6（ADJ-OBS-01 / ALG-P2S-UPM.8）----
    if (cfg.c_in_has_unrepresented_shared_terms != 0) { delete m; return 6; }

    // ---- k_corr provenance（FZ-PROV-KCORR；rc=7）----
    if (cfg.k_corr > 0.0) {
        if (!std::isfinite(cfg.k_corr)) { delete m; return 7; }
        // k_corr ≥ 1 定义性约束（N_eff ≤ N_retained）；k<1 越域 → rc=7。
        if (cfg.k_corr < 1.0) { delete m; return 7; }
        if (cfg.k_corr == 1.0) { delete m; return 7; }   // 忽略相关，禁
        if (cfg.k_corr_applicability_domain == nullptr ||
            cfg.k_corr_applicability_domain[0] == '\0') { delete m; return 7; }
        if (cfg.k_corr != kMaKCcorrFrozenInDomain &&
            (cfg.k_corr_calibration_run_id == nullptr ||
             cfg.k_corr_calibration_run_id[0] == '\0')) { delete m; return 7; }
    }

    m->frame_ids.assign(frame_set.begin(), frame_set.end());
    m->control_ids.assign(control_set.begin(), control_set.end());
    const std::size_t nf = m->frame_ids.size();
    const std::size_t np = m->control_ids.size();
    for (std::size_t i = 0; i < nf; ++i) m->frame_index[m->frame_ids[i]] = i;
    for (std::size_t i = 0; i < np; ++i) m->control_index[m->control_ids[i]] = i;

    // ---- 观测规范化（确定性排序，顺序无关）----
    std::vector<MaObsRec> recs;
    recs.reserve((std::size_t)n_obs);
    for (std::uint64_t i = 0; i < n_obs; ++i) {
        MaObsRec r;
        r.fi = (int)m->frame_index[obs[i].frame_id];
        r.ci = (int)m->control_index[obs[i].control_id];
        r.y = obs[i].value;
        r.ivar = obs[i].control_ivar;
        recs.push_back(r);
    }
    std::sort(recs.begin(), recs.end(), [](const MaObsRec& a, const MaObsRec& b) {
        if (a.fi != b.fi) return a.fi < b.fi;
        if (a.ci != b.ci) return a.ci < b.ci;
        if (a.y != b.y) return a.y < b.y;
        return a.ivar < b.ivar;
    });

    // ---- overlap graph：frame-control 二分图连通分量（确定性边序）----
    MaDsu dsu;
    dsu.init((int)(nf + np));
    std::vector<std::pair<int, int>> edges;
    edges.reserve(recs.size());
    for (const MaObsRec& r : recs) edges.push_back({r.fi, (int)nf + r.ci});
    std::sort(edges.begin(), edges.end());
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
    for (const auto& e : edges) dsu.unite(e.first, e.second);
    std::vector<int> roots((std::size_t)(nf + np), 0);
    std::vector<int> unique_roots;
    for (int i = 0; i < (int)(nf + np); ++i) {
        roots[(std::size_t)i] = dsu.find(i);
        unique_roots.push_back(roots[(std::size_t)i]);
    }
    std::sort(unique_roots.begin(), unique_roots.end());
    unique_roots.erase(std::unique(unique_roots.begin(), unique_roots.end()), unique_roots.end());
    const std::size_t ncomp = unique_roots.size();
    std::map<int, std::size_t> comp_of_root;
    for (std::size_t c = 0; c < ncomp; ++c) comp_of_root[unique_roots[c]] = c;
    m->frame_component.assign(nf, 0);
    m->control_component.assign(np, 0);
    for (std::size_t i = 0; i < nf; ++i) m->frame_component[i] = comp_of_root[roots[i]];
    for (std::size_t i = 0; i < np; ++i) m->control_component[i] = comp_of_root[roots[nf + i]];

    std::vector<int> comp_nframes(ncomp, 0);
    for (std::size_t i = 0; i < nf; ++i) comp_nframes[m->frame_component[i]] += 1;
    m->component_ref_frame.assign(ncomp, 0);
    m->component_additive_only.assign(ncomp, 0);
    for (std::size_t c = 0; c < ncomp; ++c) {
        bool first = true;
        for (std::size_t i = 0; i < nf; ++i) {
            if (m->frame_component[i] != c) continue;
            if (first || m->frame_ids[i] < m->component_ref_frame[c]) {
                m->component_ref_frame[c] = m->frame_ids[i];
                first = false;
            }
        }
    }

    // ---- min_frames（FZ-AP2S-UPM-MINFRAMES；rc=5）----
    for (std::size_t c = 0; c < ncomp; ++c) {
        if (comp_nframes[c] < cfg.min_frames) {
            if (cfg.allow_additive_only_single_frame != 0) {
                m->component_additive_only[c] = 1;   // 显式 additive-only 降级
            } else {
                delete m;
                return 5;
            }
        }
    }

    // ---- gauge（ALG-P2S-UPM.2）：g_ref=1, b_ref=0 ----
    m->n_full = np + 2 * nf;
    m->free_of_full.assign(m->n_full, -1);
    for (std::size_t c = 0; c < ncomp; ++c) {
        std::size_t rfi = 0;
        bool found = false;
        for (std::size_t i = 0; i < nf; ++i) {
            if (m->frame_component[i] == c && m->frame_ids[i] == m->component_ref_frame[c]) {
                rfi = i;
                found = true;
                break;
            }
        }
        if (!found) { delete m; return 1; }
        m->free_of_full[np + rfi] = -2;          // scale gauge 固定
        m->free_of_full[np + nf + rfi] = -2;     // level gauge 固定
    }
    m->full_of_free.clear();
    {
        long long k = 0;
        for (std::size_t j = 0; j < m->n_full; ++j) {
            if (m->free_of_full[j] == -1) {
                m->free_of_full[j] = k++;
                m->full_of_free.push_back(j);
            }
        }
        m->n_free = (std::size_t)k;
        for (std::size_t j = 0; j < m->n_full; ++j)
            if (m->free_of_full[j] == -2) m->free_of_full[j] = -1;
    }
    if (m->n_free == 0) { delete m; return 1; }

    // ---- 初值：g=1, b=0, s=control 加权均值 ----
    m->theta_full.assign(m->n_full, 0.0);
    for (std::size_t k = 0; k < nf; ++k) m->theta_full[np + k] = 1.0;
    for (std::size_t p = 0; p < np; ++p) {
        double sw = 0.0, sy = 0.0;
        for (const MaObsRec& r : recs) {
            if ((std::size_t)r.ci != p) continue;
            sw += r.ivar;
            sy += r.ivar * r.y;
        }
        m->theta_full[p] = (sw > 0.0) ? sy / sw : 0.0;
    }
    for (std::size_t c = 0; c < ncomp; ++c) {
        for (std::size_t i = 0; i < nf; ++i) {
            if (m->frame_component[i] == c && m->frame_ids[i] == m->component_ref_frame[c]) {
                m->theta_full[np + nf + i] = 0.0;
            }
        }
    }

    // ---- GN-IRLS（Huber，FZ-UPM-CONVERGENCE）----
    double lambda = 1e-6;
    double obj_old = ma_objective(*m, recs);
    int iters = 0;
    for (int iter = 0; iter < cfg.max_iterations; ++iter) {
        ++iters;
        // GN/IRLS 正规矩阵只含 robust 权重 w=ivar*huber（不得叠加统计权重）
        std::vector<double> H(m->n_free * m->n_free, 0.0);
        std::vector<double> gv(m->n_free, 0.0);
        for (const MaObsRec& o : recs) {
            const double gk = m->theta_full[np + (std::size_t)o.fi];
            const double sp = m->theta_full[(std::size_t)o.ci];
            const double model = gk * sp + m->theta_full[np + nf + (std::size_t)o.fi];
            const double r = o.y - model;
            const double sigma_eff = std::max(std::sqrt(1.0 / o.ivar), cfg.sigma_floor);
            const double w = o.ivar * huber_w(r / sigma_eff, cfg.huber_delta);
            long long js[3];
            double ja[3];
            int nc = 0;
            const long long a_s = m->free_of_full[(std::size_t)o.ci];
            if (a_s >= 0) { js[nc] = a_s; ja[nc] = gk; ++nc; }
            const long long a_g = m->free_of_full[np + (std::size_t)o.fi];
            if (a_g >= 0) { js[nc] = a_g; ja[nc] = sp; ++nc; }
            const long long a_b = m->free_of_full[np + nf + (std::size_t)o.fi];
            if (a_b >= 0) { js[nc] = a_b; ja[nc] = 1.0; ++nc; }
            for (int a = 0; a < nc; ++a) {
                gv[(std::size_t)js[a]] += w * ja[a] * r;
                for (int b = 0; b < nc; ++b)
                    H[(std::size_t)js[a] * m->n_free + (std::size_t)js[b]] += w * ja[a] * ja[b];
            }
        }
        for (std::size_t j = 0; j < m->n_free; ++j)
            H[j * m->n_free + j] *= (1.0 + lambda);
        std::vector<double> delta = gv;
        if (!ma_chol_solve(H, delta, (int)m->n_free)) {
            lambda *= 10.0;
            if (lambda > 1e12) break;
            continue;
        }
        const std::vector<double> theta_before = m->theta_full;
        double maxstep = 0.0;
        for (std::size_t j = 0; j < m->n_free; ++j) {
            const double d = delta[j];
            if (!std::isfinite(d)) { maxstep = std::numeric_limits<double>::infinity(); break; }
            m->theta_full[m->full_of_free[j]] += d;
            maxstep = std::max(maxstep, std::fabs(d));
        }
        const double obj_new = ma_objective(*m, recs);
        if (std::isfinite(obj_new) && obj_new <= obj_old) {
            obj_old = obj_new;
            lambda = std::max(lambda * 0.3, 1e-12);
            if (maxstep < cfg.tolerance) break;
        } else {
            m->theta_full = theta_before;
            lambda *= 10.0;
            if (lambda > 1e12) break;
        }
    }
    // ---- 数值 polish：主循环收敛门（tol=1e-6，FZ-UPM-CONVERGENCE，不改）
    //      满足后，在同 robust 权重下再做少量纯 Gauss-Newton 步压到机器精度；
    //      只收紧、不放宽任何冻结阈值。----
    for (int polish = 0; polish < 8; ++polish) {
        std::vector<double> Hp(m->n_free * m->n_free, 0.0);
        std::vector<double> gvp(m->n_free, 0.0);
        for (const MaObsRec& o : recs) {
            const double gk = m->theta_full[np + (std::size_t)o.fi];
            const double sp = m->theta_full[(std::size_t)o.ci];
            const double model = gk * sp + m->theta_full[np + nf + (std::size_t)o.fi];
            const double r = o.y - model;
            const double sigma_eff = std::max(std::sqrt(1.0 / o.ivar), cfg.sigma_floor);
            const double w = o.ivar * huber_w(r / sigma_eff, cfg.huber_delta);
            long long js[3];
            double ja[3];
            int nc = 0;
            const long long a_s = m->free_of_full[(std::size_t)o.ci];
            if (a_s >= 0) { js[nc] = a_s; ja[nc] = gk; ++nc; }
            const long long a_g = m->free_of_full[np + (std::size_t)o.fi];
            if (a_g >= 0) { js[nc] = a_g; ja[nc] = sp; ++nc; }
            const long long a_b = m->free_of_full[np + nf + (std::size_t)o.fi];
            if (a_b >= 0) { js[nc] = a_b; ja[nc] = 1.0; ++nc; }
            for (int a = 0; a < nc; ++a) {
                gvp[(std::size_t)js[a]] += w * ja[a] * r;
                for (int b = 0; b < nc; ++b)
                    Hp[(std::size_t)js[a] * m->n_free + (std::size_t)js[b]] += w * ja[a] * ja[b];
            }
        }
        std::vector<double> dp = gvp;
        if (!ma_chol_solve(Hp, dp, (int)m->n_free)) break;
        double maxstep = 0.0;
        bool finite = true;
        for (std::size_t j = 0; j < m->n_free; ++j) {
            if (!std::isfinite(dp[j])) { finite = false; break; }
            m->theta_full[m->full_of_free[j]] += dp[j];
            maxstep = std::max(maxstep, std::fabs(dp[j]));
        }
        if (!finite || maxstep < 1e-13) break;
    }
    m->iterations = iters;
    for (std::size_t j = 0; j < m->n_full; ++j)
        if (!std::isfinite(m->theta_full[j])) { delete m; return 8; }

    // ---- 解处统计正规矩阵（W = C_in^-1 = diag(control_ivar)）----
    std::vector<double> H;
    ma_normal_matrix(*m, recs, H);
    const std::size_t n = m->n_free;

    // 秩（FZ-AP2S-RANK-RTOL）：J_w 奇异值（对称嵌入，保留小奇异值精度）
    std::vector<double> sigma;
    {
        std::vector<double> Jw;
        ma_weighted_jacobian(*m, recs, Jw);
        if (!ma_singular_values(Jw, recs.size(), n, sigma)) { delete m; return 8; }
    }
    const double smax = sigma.empty() ? 0.0 : sigma[0];
    std::size_t rank = 0;
    if (smax > 0.0) {
        for (std::size_t i = 0; i < sigma.size(); ++i)
            if (sigma[i] / smax > cfg.rank_rtol) ++rank;
    }
    if (rank < n) { delete m; return 3; }   // 秩亏 / 恒常 s 导致 g-b 退化

    // C_theta = (JᵀWJ)^-1（gauge 消除后可辨识子空间）
    std::vector<double> evals;
    std::vector<double> evecs;
    if (!ma_eig(H, (int)n, evals, &evecs)) { delete m; return 8; }
    std::vector<double> Cth((std::size_t)n * n, 0.0);
    bool inv_ok = true;
    for (std::size_t a = 0; a < n && inv_ok; ++a) {
        for (std::size_t b = 0; b < n && inv_ok; ++b) {
            double sum = 0.0;
            for (std::size_t k = 0; k < n; ++k) {
                const double ek = evals[k];
                if (!(ek > 0.0) || !std::isfinite(ek)) { inv_ok = false; break; }
                sum += evecs[a * n + k] * (1.0 / ek) * evecs[b * n + k];
            }
            Cth[a * n + b] = sum;
        }
    }
    if (!inv_ok) { delete m; return 8; }

    // 条件数（ALG-P2S-UPM.4；FZ-AP2S-KAPPA-MAX）
    // kappa = cond_2( D^-1 (JᵀWJ) D^-1 ), D=diag(列范数)（列均衡消除单位伪病态）
    std::vector<double> M((std::size_t)n * n, 0.0);
    bool col_ok = true;
    for (std::size_t j = 0; j < n; ++j)
        if (!(H[j * n + j] > 0.0) || !std::isfinite(H[j * n + j])) { col_ok = false; break; }
    if (!col_ok) { delete m; return 3; }
    for (std::size_t i = 0; i < n; ++i) {
        const double di = std::sqrt(H[i * n + i]);
        for (std::size_t j = 0; j < n; ++j) {
            const double dj = std::sqrt(H[j * n + j]);
            M[i * n + j] = H[i * n + j] / (di * dj);
        }
    }
    std::vector<double> mevals;
    if (!ma_eig(M, (int)n, mevals, nullptr)) { delete m; return 8; }
    double lmin = std::numeric_limits<double>::infinity();
    double lmax = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        lmin = std::min(lmin, mevals[i]);
        lmax = std::max(lmax, mevals[i]);
    }
    const double kappa = (lmin > 0.0) ? (lmax / lmin)
                                      : std::numeric_limits<double>::infinity();
    if (!std::isfinite(kappa) || kappa > cfg.kappa_max) { delete m; return 4; }

    m->sigma = sigma;
    m->rank = rank;
    m->kappa = kappa;
    m->C_theta = Cth;

    // ---- model hash（确定性）----
    {
        std::ostringstream p;
        p << std::setprecision(17);
        p << "upm-ma-v1|F=" << nf << "|P=" << np << "|C=" << ncomp
          << "|gauge=min_frame_id|min_frames=" << cfg.min_frames
          << "|rank_rtol=" << cfg.rank_rtol << "|kappa_max=" << cfg.kappa_max;
        for (std::size_t c = 0; c < ncomp; ++c)
            p << "|ref" << c << "=" << m->component_ref_frame[c]
              << "|addonly" << c << "=" << m->component_additive_only[c];
        for (const MaObsRec& r : recs)
            p << "|o" << r.fi << "," << r.ci << "," << r.y << "," << r.ivar;
        for (std::size_t j = 0; j < m->n_full; ++j) p << "|t" << j << "=" << m->theta_full[j];
        const std::string payload = p.str();
        const std::string hx = astrocs::crypto::sha256_hex(payload.data(), payload.size());
        std::memset(m->info.model_hash, 0, sizeof(m->info.model_hash));
        std::memcpy(m->info.model_hash, hx.c_str(), std::min<std::size_t>(64, hx.size()));
    }

    int addonly = 0;
    for (std::size_t c = 0; c < ncomp; ++c) addonly += m->component_additive_only[c];
    m->info.version = 1;
    m->info.n_controls = (std::uint64_t)np;
    m->info.n_frames = (std::uint64_t)nf;
    m->info.n_components = (std::uint64_t)ncomp;
    m->info.n_observations = n_obs;
    m->info.n_params = (std::uint64_t)n;
    m->info.rank = (std::uint64_t)rank;
    m->info.rank_rtol = cfg.rank_rtol;
    m->info.kappa = kappa;
    m->info.kappa_max = cfg.kappa_max;
    m->info.min_frames = cfg.min_frames;
    m->info.gauge_mode = 0;
    m->info.iterations = iters;
    m->info.additive_only_components = addonly;

    *out_model = m;
    return 0;
}

int p2_upm_ma_info(const void* model, P2UpmMaInfo* out_info) {
    if (model == nullptr || out_info == nullptr) return 1;
    *out_info = static_cast<const MaModel*>(model)->info;
    return 0;
}

int p2_upm_ma_solution(const void* model, std::uint64_t frame_id,
                       std::uint64_t control_id, double* out_g, double* out_b,
                       double* out_s) {
    if (model == nullptr) return 1;
    const MaModel* m = static_cast<const MaModel*>(model);
    if (out_s != nullptr) {
        const auto it = m->control_index.find(control_id);
        if (it == m->control_index.end()) return 1;
        *out_s = m->theta_full[it->second];
    }
    if (out_g != nullptr || out_b != nullptr) {
        const auto it = m->frame_index.find(frame_id);
        if (it == m->frame_index.end()) return 1;
        const std::size_t np = m->control_ids.size();
        const std::size_t nf = m->frame_ids.size();
        if (out_g != nullptr) *out_g = m->theta_full[np + it->second];
        if (out_b != nullptr) *out_b = m->theta_full[np + nf + it->second];
    }
    return 0;
}

int p2_upm_ma_component_of_frame(const void* model, std::uint64_t frame_id,
                                 std::uint64_t* out_component) {
    if (model == nullptr || out_component == nullptr) return 1;
    const MaModel* m = static_cast<const MaModel*>(model);
    const auto it = m->frame_index.find(frame_id);
    if (it == m->frame_index.end()) return 1;
    *out_component = (std::uint64_t)m->frame_component[it->second];
    return 0;
}

int p2_upm_ma_component_of_control(const void* model, std::uint64_t control_id,
                                   std::uint64_t* out_component) {
    if (model == nullptr || out_component == nullptr) return 1;
    const MaModel* m = static_cast<const MaModel*>(model);
    const auto it = m->control_index.find(control_id);
    if (it == m->control_index.end()) return 1;
    *out_component = (std::uint64_t)m->control_component[it->second];
    return 0;
}

int p2_upm_ma_component_ref_frame(const void* model, std::uint64_t component,
                                  std::uint64_t* out_ref_frame_id) {
    if (model == nullptr || out_ref_frame_id == nullptr) return 1;
    const MaModel* m = static_cast<const MaModel*>(model);
    if (component >= m->component_ref_frame.size()) return 1;
    *out_ref_frame_id = m->component_ref_frame[(std::size_t)component];
    return 0;
}

int p2_upm_ma_param_cov(const void* model, double* out_C, std::uint64_t ld) {
    if (model == nullptr || out_C == nullptr) return 1;
    const MaModel* m = static_cast<const MaModel*>(model);
    if (ld < m->n_free) return 1;
    for (std::size_t i = 0; i < m->n_free; ++i)
        for (std::size_t j = 0; j < m->n_free; ++j)
            out_C[i * (std::size_t)ld + j] = m->C_theta[i * m->n_free + j];
    return 0;
}

int p2_upm_ma_c_out(const void* model, const double* J_out, std::uint64_t ld_J,
                    std::uint64_t m_rows, const double* C_stat, std::uint64_t ld_C,
                    double* out_C_out, std::uint64_t ld_out) {
    if (model == nullptr || J_out == nullptr || C_stat == nullptr || out_C_out == nullptr) return 1;
    const MaModel* mm = static_cast<const MaModel*>(model);
    const std::size_t n = mm->n_free;
    if (ld_J < n || ld_C < m_rows || ld_out < m_rows) return 1;
    for (std::uint64_t a = 0; a < m_rows; ++a) {
        for (std::uint64_t b = 0; b < m_rows; ++b) {
            double s = C_stat[a * ld_C + b];
            for (std::size_t i = 0; i < n; ++i) {
                const double jai = J_out[a * ld_J + i];
                if (jai == 0.0) continue;
                for (std::size_t j = 0; j < n; ++j)
                    s += jai * mm->C_theta[i * n + j] * J_out[b * ld_J + j];
            }
            out_C_out[a * ld_out + b] = s;
        }
    }
    return 0;
}

int p2_upm_ma_provenance(const void* model, char* out_json, std::size_t buf_size) {
    if (model == nullptr || out_json == nullptr || buf_size == 0) return 1;
    const MaModel* m = static_cast<const MaModel*>(model);
    const P2UpmMaConfig& cfg = m->cfg;
    nlohmann::json j;
    j["version"] = m->info.version;
    j["gauge_mode"] = "min_frame_id";
    j["gauge_mode_id"] = 0;
    j["min_frames"] = m->info.min_frames;
    j["n_frames"] = m->info.n_frames;
    j["n_controls"] = m->info.n_controls;
    j["n_components"] = m->info.n_components;
    j["n_observations"] = m->info.n_observations;
    j["n_params"] = m->info.n_params;
    j["rank"] = m->info.rank;
    j["rank_rtol"] = m->info.rank_rtol;
    j["kappa"] = m->info.kappa;
    j["kappa_max"] = m->info.kappa_max;
    j["additive_only_components"] = m->info.additive_only_components;
    j["iterations"] = m->info.iterations;
    nlohmann::json refs = nlohmann::json::array();
    for (std::size_t c = 0; c < m->component_ref_frame.size(); ++c)
        refs.push_back(m->component_ref_frame[c]);
    j["component_ref_frame_ids"] = refs;
    j["covariance_method"] = "propagated_from_composite_coefficients";
    j["variance_from"] = "combination_coefficients";
    j["variance_from_weight"] = false;
    j["uses_relative_weight_as_ivar"] = false;
    j["J_C_theta_JT_present"] = true;
    j["C_theta"] = {{"method", "(J^T W J)^-1"},
                    {"dims", {m->n_free, m->n_free}},
                    {"digest", astrocs::crypto::sha256_hex(
                                   m->C_theta.data(),
                                   m->C_theta.size() * sizeof(double))}};
    if (cfg.k_corr > 0.0) {
        j["k_corr"] = cfg.k_corr;
        j["k_corr_applicability_domain"] =
            cfg.k_corr_applicability_domain ? cfg.k_corr_applicability_domain : "";
        j["k_corr_calibration_run_id"] =
            cfg.k_corr_calibration_run_id ? cfg.k_corr_calibration_run_id : "";
    } else {
        j["k_corr"] = nullptr;
    }
    j["flux_conservation_factor"] =
        cfg.flux_conservation_factor ? nlohmann::json(cfg.flux_conservation_factor)
                                     : nlohmann::json(nullptr);
    j["model_hash"] = m->info.model_hash;
    j["any_fail_closed_reason"] = "";
    const std::string s = j.dump();
    if (s.size() + 1 > buf_size) return 2;
    std::memcpy(out_json, s.data(), s.size());
    out_json[s.size()] = '\0';
    return 0;
}

void p2_upm_ma_close(void* model) {
    if (model == nullptr) return;
    delete static_cast<MaModel*>(model);
}

int p2_upm_control_variance(double k_corr, double sigma_bg,
                            std::uint64_t n_retained,
                            const char* applicability_domain,
                            const char* calibration_run_id,
                            double* out_control_variance,
                            double* out_control_ivar) {
    if (n_retained < 1) return 1;
    if (!(sigma_bg > 0.0) || !std::isfinite(sigma_bg)) return 1;
    if (!(k_corr > 0.0) || !std::isfinite(k_corr)) return 1;
    // k_corr ≥ 1 是定义性约束：k_corr 表征 Drizzle 输出协方差使
    // N_eff ≤ N_retained；k_corr < 1 等价于 N_eff > N_retained（正相关样本
    // 的有效样本量不可能大于样本数），物理上不可达，必须显式拒（rc=1）。
    if (k_corr < 1.0) return 1;
    if (k_corr == 1.0) return 2;                       // 忽略相关 → REJECT
    if (k_corr != kMaKCcorrFrozenInDomain &&
        (calibration_run_id == nullptr || calibration_run_id[0] == '\0')) {
        return 3;                                      // 域外推（DI-04 未复跑标定）
    }
    if (applicability_domain == nullptr || applicability_domain[0] == '\0') return 4;
    const double cv = k_corr * kMaPiHalf * sigma_bg * sigma_bg / (double)n_retained;
    if (!(cv > 0.0) || !std::isfinite(cv)) return 1;
    if (out_control_variance != nullptr) *out_control_variance = cv;
    if (out_control_ivar != nullptr) *out_control_ivar = 1.0 / cv;
    return 0;
}

} // extern "C"

