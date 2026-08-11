// lib/phase2/src/upm.cpp — Phase2 UnifiedPhotometricModel CPU reference
//
// V2（控制包 AstroCS_Phase2_AuditFix_Control_Package_V2，SHA 2971F9A7...CBBC176）：
//   - 空间 additive UPM：M(p_k) latent reference + C_i(p) 每帧空间校正场；
//   - 观测模型 y_ik = M(p_k) + C_i(p_k) + noise；
//   - 控制拓扑 = coverage union 上的 HEALPix control cells（8×8/tile 网格），
//     basis/topology 只由 geometry/coverage/配置决定（与 SNR 解耦）；
//   - 图拉普拉斯平滑 lambda_s 真正进入联合求解；
//   - 观测权重 raw_w = quality_factor * support^p * snr^2/(1+snr^2) *
//     1/max(unc^2, sigma_floor^2)，并在每个 control node 内归一；
//   - 弱零校正锚 lambda_0：单覆盖节点由同帧其他节点 + smoothness 延拓；
//   - gauge：参考帧（最小内容稳定 frame_id）C=0，输入顺序无关；
//   - calibrate_block 真正使用 leaf_ipix 查找所在 control cell；
//   - 断开分量各自 gauge（不虚构跨组件约束）。
#include "astro/phase2/upm.h"

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
    std::vector<std::vector<double>> C;  // [frame][control] 空间校正
    std::vector<std::vector<std::size_t>> adj;  // control 邻接（tile 内网格）
    std::vector<std::vector<double>> obs_w;     // 最终每轮权重缓存
    std::map<std::pair<std::uint64_t, std::pair<int, int>>, std::size_t>
        cell_index;                      // (tile, gx, gy) -> control
    std::string input_manifest_hash;
    int grid{8};                         // cells per tile side
    int cell_side{64};
    // 诊断
    int iterations{0};
    double objective{0.0};
    std::size_t component_count{1};
};

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

int p2_upm_build(const P2ControlObservation* obs, std::uint64_t n_obs,
                 const P2UpmBuildConfig* cfg_in, void** out_model) {
    if (out_model == nullptr || obs == nullptr || n_obs == 0) return 1;
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
        cfg.control_reliability = 1.0;
    }
    if (cfg.huber_delta <= 0.0) cfg.huber_delta = 1.345;
    if (cfg.max_iterations <= 0) cfg.max_iterations = 100;
    if (cfg.sigma_floor <= 0.0) cfg.sigma_floor = 1e-3;
    if (cfg.support_power < 0.0) cfg.support_power = 1.0;
    if (cfg.control_reliability <= 0.0) cfg.control_reliability = 1.0;
    if (cfg.zero_anchor_weight < 0.0) cfg.zero_anchor_weight = 1e-3;
    if (cfg.smoothing_lambda < 0.0) cfg.smoothing_lambda = 0.0;
    if (cfg.target_order < 0) {
        // 空间 UPM 必须知道 control leaf 层级（order = target+9）
        return 1;
    }

    Model* m = new Model();
    m->cfg = cfg;
    m->info.version = 2;              // V2 空间 UPM
    m->info.precision = 1;  // fp64 reference
    m->info.observation_count = n_obs;
    m->info.target_order = (std::uint32_t)cfg.target_order;
    m->grid = 8;
    m->cell_side = 512 / m->grid;
    const int tile_shift = 9;         // leaf order = target+9 -> tile shift 18/2
    const int leaf_shift = 18;

    // 收集 control（按 (tile,gx,gy) cell 去重）/ frame 索引
    std::set<std::uint64_t> frame_ids;
    for (std::uint64_t i = 0; i < n_obs; ++i) {
        const auto& o = obs[i];
        const std::uint64_t tile = leaf_to_tile(o.leaf_ipix, tile_shift);
        const std::uint64_t local = leaf_local(o.leaf_ipix, tile_shift);
        std::uint32_t x = 0, y = 0;
        astrocs::healpix::nested_local_to_xy(local, (std::uint32_t)tile_shift,
                                             x, y);
        const int gx = (int)(x / (std::uint32_t)m->cell_side);
        const int gy = (int)(y / (std::uint32_t)m->cell_side);
        const auto key = std::make_pair(tile, std::make_pair(gx, gy));
        auto it = m->cell_index.find(key);
        if (it == m->cell_index.end()) {
            const std::size_t idx = m->controls.size();
            m->controls.push_back(ControlNode{});
            m->controls.back().leaf_ipix = o.leaf_ipix;
            m->controls.back().ra_deg = o.ra_deg;
            m->controls.back().dec_deg = o.dec_deg;
            m->controls.back().id = o.control_id;
            m->controls.back().tile_ipix = tile;
            m->controls.back().gx = gx;
            m->controls.back().gy = gy;
            m->controls.back().reliability =
                std::max(0.0, cfg.control_reliability);
            m->cell_index[key] = idx;
            m->control_by_id[o.control_id] = idx;
            it = m->cell_index.find(key);
        }
        m->controls[it->second].obs_idx.push_back(i);
        frame_ids.insert(o.frame_id);
    }
    m->info.control_count = m->controls.size();
    const std::size_t K = m->controls.size();
    for (std::uint64_t f : frame_ids) {
        if (m->frame_index.find(f) == m->frame_index.end()) {
            const std::size_t idx = m->frame_index.size();
            m->frame_index[f] = idx;
        }
    }
    const std::size_t F = m->frame_index.size();
    m->C.assign(F, std::vector<double>(K, 0.0));
    m->obs_w.assign(F, std::vector<double>(K, 0.0));

    // 邻接图：tile 内 8×8 网格（上下左右）
    m->adj.assign(K, {});
    for (std::size_t k = 0; k < K; ++k) {
        const auto& cn = m->controls[k];
        auto link = [&](int nx, int ny) {
            if (nx < 0 || ny < 0 || nx >= m->grid || ny >= m->grid) return;
            const auto key =
                std::make_pair(cn.tile_ipix, std::make_pair(nx, ny));
            const auto it = m->cell_index.find(key);
            if (it != m->cell_index.end()) m->adj[k].push_back(it->second);
        };
        link(cn.gx + 1, cn.gy);
        link(cn.gx - 1, cn.gy);
        link(cn.gx, cn.gy + 1);
        link(cn.gx, cn.gy - 1);
    }

    // ===== Huber IRLS 坐标下降求解 =====
    // 残差 r_ik = y_ik - M_k - C_i,k
    // 权重 raw_w = quality * support^p * snr^2/(1+snr^2) / max(unc^2, floor^2)
    //           per-control 归一化后 × huber_w
    // M 更新（固定 C）：M_k = Σ w (y - C) / Σ w
    // C 更新（固定 M，逐帧 CG）：
    //   min Σ_k w_ik (r_ik - C_ik)^2 + λs Σ_{k~l} (C_ik - C_il)^2 + λ0 Σ C_ik^2
    std::vector<double> M(K, 0.0);
    const double anchor = std::max(0.0, cfg.zero_anchor_weight);
    const double lambda_s = std::max(0.0, cfg.smoothing_lambda);
    const double sigma_floor = cfg.sigma_floor;
    const double support_power = cfg.support_power;
    const std::uint64_t ref_frame_id = *frame_ids.begin();  // 最小 frame_id

    // per-control 归一化：需要先按 control 聚合（同 cell 多帧观测）
    // 这里直接按 obs 计算 raw 后按 control 归一化（与文档一致）
    std::vector<double> raw_w(n_obs, 0.0);
    auto compute_raw = [&]() {
        std::vector<double> sums(K, 0.0);
        for (std::uint64_t i = 0; i < n_obs; ++i) {
            const auto& o = obs[i];
            const std::size_t ck = m->control_by_id[o.control_id];
            const double qf = quality_factor(o.quality_flags, cfg.quality_mode);
            const double sp = std::clamp(o.support, 0.0, 1.0);
            const double snr2 = o.snr * o.snr;
            const double unc = std::max(std::fabs(o.uncertainty), sigma_floor);
            raw_w[i] = qf * std::pow(sp, support_power) *
                       (snr2 / (1.0 + snr2)) / (unc * unc);
            sums[ck] += raw_w[i];
        }
        for (std::uint64_t i = 0; i < n_obs; ++i) {
            const std::size_t ck = m->control_by_id[obs[i].control_id];
            if (sums[ck] > 1e-12)
                raw_w[i] = raw_w[i] / sums[ck] * m->controls[ck].reliability;
            else
                raw_w[i] = 0.0;
        }
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
        compute_raw();
        std::vector<double> w(n_obs);
        for (std::uint64_t i = 0; i < n_obs; ++i) {
            const std::size_t ck = m->control_by_id[obs[i].control_id];
            const double r = obs[i].value - M[ck] - m->C[m->frame_index[obs[i].frame_id]][ck];
            w[i] = raw_w[i] * huber_w(r, cfg.huber_delta);
        }
        // 2. M 更新（固定 C）：gauge = 参考帧 C=0 → M 由参考帧观测定义；
        //    参考帧未覆盖的节点用全部帧（延拓），不虚构约束。
        double max_dM = 0.0;
        for (std::size_t k = 0; k < m->controls.size(); ++k) {
            double num = 0.0, den = 0.0;
            const std::size_t rf = m->frame_index[ref_frame_id];
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
        for (const auto& kv : m->frame_index) {
            const std::size_t f = kv.second;
            if (kv.first == ref_frame_id) {
                // 参考帧 gauge：C=0
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
            m->objective += raw_w[i] * huber_rho(r, cfg.huber_delta);
        }
        if (max_dM < cfg.tolerance && max_dC < cfg.tolerance) break;
    }

    for (std::size_t k = 0; k < K; ++k) m->controls[k].M = M[k];
    // 连通分量：frame-control 二分图（共同观测约束即边）
    {
        std::vector<std::vector<std::size_t>> adj(F + K);
        for (std::uint64_t i = 0; i < n_obs; ++i) {
            const std::size_t f = m->frame_index[obs[i].frame_id];
            const std::size_t ck = m->control_by_id[obs[i].control_id];
            adj[f].push_back(F + ck);
            adj[F + ck].push_back(f);
        }
        std::vector<std::uint8_t> seen(F + K, 0);
        std::size_t comps = 0;
        for (std::size_t start = 0; start < F + K; ++start) {
            if (seen[start]) continue;
            ++comps;
            std::vector<std::size_t> stack{start};
            seen[start] = 1;
            while (!stack.empty()) {
                const std::size_t u = stack.back();
                stack.pop_back();
                for (std::size_t v : adj[u]) {
                    if (!seen[v]) {
                        seen[v] = 1;
                        stack.push_back(v);
                    }
                }
            }
        }
        m->component_count = comps;
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

int p2_upm_save(const void* model, const char* path) {
    if (model == nullptr || path == nullptr) return 1;
    const Model* m = static_cast<const Model*>(model);
    nlohmann::json j;
    j["format"] = "astrocs-upm-v2";
    j["version"] = m->info.version;
    j["target_order"] = m->info.target_order;
    j["precision"] = m->info.precision;
    j["robust_loss"] = m->cfg.robust_loss;
    j["snr_weight_mode"] = m->cfg.snr_weight_mode;
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
    j["control_count"] = m->info.control_count;
    j["observation_count"] = m->info.observation_count;
    nlohmann::json frames = nlohmann::json::array();
    for (const auto& kv : m->frame_index) frames.push_back(kv.first);
    j["frames"] = frames;
    nlohmann::json controls = nlohmann::json::array();
    for (std::size_t k = 0; k < m->controls.size(); ++k) {
        const auto& cn = m->controls[k];
        controls.push_back({cn.tile_ipix, cn.gx, cn.gy, cn.ra_deg, cn.dec_deg,
                            cn.M, cn.leaf_ipix});
    }
    j["controls"] = controls;
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
    m->info.version = j.value("version", 1u);
    m->info.precision = j.value("precision", 1u);
    m->info.target_order = j.value("target_order", 0u);
    m->info.control_count = j.value("control_count", 0ull);
    m->info.observation_count = j.value("observation_count", 0ull);
    m->cfg.robust_loss = j.value("robust_loss", 0);
    m->cfg.snr_weight_mode = j.value("snr_weight_mode", 0);
    m->cfg.huber_delta = j.value("huber_delta", 1.345);
    m->cfg.smoothing_lambda = j.value("smoothing_lambda", 0.0);
    m->cfg.zero_anchor_weight = j.value("zero_anchor_weight", 1e-3);
    m->cfg.max_iterations = j.value("max_iterations", 100);
    m->cfg.tolerance = j.value("tolerance", 1e-6);
    m->cfg.sigma_floor = j.value("sigma_floor", 1e-3);
    m->cfg.support_power = j.value("support_power", 1.0);
    m->input_manifest_hash = j.value("input_manifest_hash", std::string());
    const std::string h = j.value("model_hash", std::string(64, '0'));
    std::strncpy(m->info.model_hash, h.c_str(), sizeof(m->info.model_hash) - 1);
    m->info.model_hash[sizeof(m->info.model_hash) - 1] = '\0';
    m->iterations = j.value("iterations", 0);
    m->objective = j.value("objective", 0.0);
    m->component_count = j.value("component_count", (std::size_t)1);
    m->info.component_count = (std::uint32_t)m->component_count;
    m->grid = 8;
    m->cell_side = 512 / m->grid;
    // frames
    std::size_t fi = 0;
    for (const auto& fr : j["frames"]) {
        const std::uint64_t fid = fr.get<std::uint64_t>();
        m->frame_index[fid] = fi++;
    }
    // controls
    for (const auto& ct : j["controls"]) {
        ControlNode cn;
        cn.tile_ipix = ct[0].get<std::uint64_t>();
        cn.gx = ct[1].get<int>();
        cn.gy = ct[2].get<int>();
        cn.ra_deg = ct[3].get<double>();
        cn.dec_deg = ct[4].get<double>();
        cn.M = ct[5].get<double>();
        cn.leaf_ipix = ct[6].get<std::uint64_t>();
        cn.reliability = 1.0;
        cn.id = cn.tile_ipix * 1000 + (std::uint64_t)(cn.gy * 8 + cn.gx);
        const auto key =
            std::make_pair(cn.tile_ipix, std::make_pair(cn.gx, cn.gy));
        m->cell_index[key] = m->controls.size();
        m->control_by_id[cn.id] = m->controls.size();
        m->controls.push_back(cn);
    }
    const std::size_t F = m->frame_index.size();
    const std::size_t K = m->controls.size();
    m->C.assign(F, std::vector<double>(K, 0.0));
    if (j.contains("C")) {
        for (std::size_t f = 0; f < j["C"].size() && f < F; ++f) {
            for (const auto& item : j["C"][f]) {
                const std::size_t k = item[0].get<std::size_t>();
                if (k < K) m->C[f][k] = item[1].get<double>();
            }
        }
    }
    m->adj.assign(K, {});
    for (std::size_t k = 0; k < K; ++k) {
        const auto& cn = m->controls[k];
        auto link = [&](int nx, int ny) {
            if (nx < 0 || ny < 0 || nx >= m->grid || ny >= m->grid) return;
            const auto key =
                std::make_pair(cn.tile_ipix, std::make_pair(nx, ny));
            const auto it = m->cell_index.find(key);
            if (it != m->cell_index.end()) m->adj[k].push_back(it->second);
        };
        link(cn.gx + 1, cn.gy);
        link(cn.gx - 1, cn.gy);
        link(cn.gx, cn.gy + 1);
        link(cn.gx, cn.gy - 1);
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
    const std::size_t fi = (it != m->frame_index.end()) ? it->second : 0;
    const int tile_shift = 9;
    const std::uint64_t mask = (1ULL << (2u * (unsigned)tile_shift)) - 1ULL;
    for (std::uint64_t i = 0; i < count; ++i) {
        double c = 0.0;
        const std::uint64_t tile =
            leaf_ipix[i] >> (2u * (unsigned)tile_shift);
        const std::uint64_t local = leaf_ipix[i] & mask;
        std::uint32_t x = 0, y = 0;
        astrocs::healpix::nested_local_to_xy(local, (std::uint32_t)tile_shift,
                                             x, y);
        const int gx = (int)(x / (std::uint32_t)m->cell_side);
        const int gy = (int)(y / (std::uint32_t)m->cell_side);
        const auto key = std::make_pair(tile, std::make_pair(gx, gy));
        const auto cit = m->cell_index.find(key);
        if (cit != m->cell_index.end()) c = m->C[fi][cit->second];
        output_signal[i] = input_signal[i] - c;
    }
    return 0;
}

int p2_upm_materialize_dense(const void* model, int target_order,
                             const char* cache_path) {
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
    const std::uint64_t mask = (1ULL << (2u * (unsigned)tile_shift)) - 1ULL;
    std::vector<double> values(512ull * 512ull);
    for (std::size_t f = 0; f < m->C.size(); ++f) {
        for (std::uint64_t tile : tiles) {
            for (std::uint64_t local = 0; local < values.size(); ++local) {
                std::uint32_t x = 0, y = 0;
                astrocs::healpix::nested_local_to_xy(
                    local, (std::uint32_t)tile_shift, x, y);
                const int gx = (int)(x / (std::uint32_t)m->cell_side);
                const int gy = (int)(y / (std::uint32_t)m->cell_side);
                const auto key = std::make_pair(tile, std::make_pair(gx, gy));
                const auto cit = m->cell_index.find(key);
                values[(std::size_t)local] =
                    (cit != m->cell_index.end()) ? m->C[f][cit->second] : 0.0;
            }
            if (aio_upm_dense_write_tile(d, (std::uint64_t)f, tile,
                                         values.data(), values.size()) != 0) {
                aio_upm_dense_abort(d);
                return 1;
            }
        }
    }
    return aio_upm_dense_end(d);
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
