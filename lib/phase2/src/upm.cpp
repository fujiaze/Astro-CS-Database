// lib/phase2/src/upm.cpp — Phase2 UnifiedPhotometricModel CPU reference
//
// W4（控制包 34A532A2...B2EB308 + wiki Phase2_Unified_Photometric_Model）：
//   - 输入控制观测（值/不确定度/SNR/support/quality + frame_id/control_id）；
//   - Huber IRLS 联合求解：每个控制节点 k 求解参考值 z_k，每帧一个加性
//     联合系数 a_f（曝光残余背景，同一模型版本内联合求解，不暴露独立产品）；
//   - SNR-aware 数据权重：w = snr^2/(1+snr^2) 归一化（低 SNR 不拉偏高 SNR）；
//   - 弱零校正锚：a_f 先验中心 0（默认 1e-3 权重），处理连通分量；
//   - calibrate_block：output = input - a_frame（加性校准）；
//   - 输出诊断：iterations/objective/control/observation/component。
#include "astro/phase2/upm.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

struct ControlNode {
    std::vector<std::uint64_t> obs_idx;  // 参与该节点的观测
    double z{0.0};                       // 参考值（待求）
};

struct Model {
    P2ModelInfo info;
    P2UpmBuildConfig cfg;
    std::vector<ControlNode> controls;
    std::map<std::uint64_t, std::size_t> control_by_id;
    std::map<std::uint64_t, std::size_t> frame_index;
    std::vector<double> frame_offset;    // a_f
    // 诊断
    int iterations{0};
    double objective{0.0};
    std::size_t component_count{1};
};

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

std::string sha256_hex(const void*, std::size_t) {
    // W4 占位：真实哈希在 W4 完整版（持久化）实现；此处固定长度占位避免
    // 引入新依赖。缓存校验门在 W5 实现时使用。
    return std::string(64, '0');
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
    }
    if (cfg.huber_delta <= 0.0) cfg.huber_delta = 1.345;
    if (cfg.max_iterations <= 0) cfg.max_iterations = 100;

    Model* m = new Model();
    m->cfg = cfg;
    m->info.version = 1;
    m->info.precision = 1;  // fp64 reference
    m->info.observation_count = n_obs;

    // 收集 control / frame 索引
    std::set<std::uint64_t> frame_ids;
    for (std::uint64_t i = 0; i < n_obs; ++i) {
        const auto& o = obs[i];
        if (m->control_by_id.find(o.control_id) == m->control_by_id.end()) {
            const std::size_t idx = m->controls.size();
            m->control_by_id[o.control_id] = idx;
            m->controls.push_back(ControlNode{});
        }
        m->controls[m->control_by_id[o.control_id]].obs_idx.push_back(i);
        frame_ids.insert(o.frame_id);
    }
    m->info.control_count = m->controls.size();
    for (std::uint64_t f : frame_ids) {
        if (m->frame_index.find(f) == m->frame_index.end()) {
            const std::size_t idx = m->frame_offset.size();
            m->frame_index[f] = idx;
            m->frame_offset.push_back(0.0);
        }
    }

    // ===== Huber IRLS 联合求解 =====
    // 残差 r_ik = y_ik - z_k - a_f
    // 权重 w_ik = snr2_normalized * huber_w(r, delta)
    // 方程（z、a 联合）：
    //   z_k: sum_i w (y - a) - z_k * sum_i w = 0
    //   a_f: sum_i w (y - z) - a_f * sum_i w + anchor_w * a_f = 0
    std::vector<double> z(m->controls.size(), 0.0);
    std::vector<double> a(m->frame_offset.size(), 0.0);
    const double anchor = std::max(0.0, cfg.zero_anchor_weight);

    for (int iter = 0; iter < cfg.max_iterations; ++iter) {
        // 更新权重
        std::vector<double> w(n_obs, 1.0);
        for (std::uint64_t i = 0; i < n_obs; ++i) {
            const auto& o = obs[i];
            const std::size_t ck = m->control_by_id[o.control_id];
            const std::size_t fi = m->frame_index[o.frame_id];
            const double r = o.value - z[ck] - a[fi];
            const double snr2 = o.snr * o.snr;
            w[i] = snr2 / (1.0 + snr2);           // snr2_normalized
            w[i] *= huber_w(r, cfg.huber_delta);
        }
        // 求解 z（固定 a）
        double max_dz = 0.0;
        for (std::size_t k = 0; k < m->controls.size(); ++k) {
            double num = 0.0, den = 0.0;
            for (std::size_t ii : m->controls[k].obs_idx) {
                const auto& o = obs[ii];
                const std::size_t fi = m->frame_index[o.frame_id];
                num += w[ii] * (o.value - a[fi]);
                den += w[ii];
            }
            if (den > 1e-12) {
                const double znew = num / den;
                max_dz = std::max(max_dz, std::fabs(znew - z[k]));
                z[k] = znew;
            }
        }
        // 求解 a（固定 z；弱锚）。参考帧（frame_index==0，即最小 frame_id）
        // offset 固定为 0，消除加性歧义（统一模型相对参考帧校准）。
        double max_da = 0.0;
        for (const auto& kv : m->frame_index) {
            const std::size_t f = kv.second;
            if (f == 0) { a[f] = 0.0; continue; }  // 参考帧锚定
            double num = 0.0, den = anchor;
            for (std::uint64_t i = 0; i < n_obs; ++i) {
                if (m->frame_index[obs[i].frame_id] != f) continue;
                const std::size_t ck = m->control_by_id[obs[i].control_id];
                num += w[i] * (obs[i].value - z[ck]);
                den += w[i];
            }
            if (den > 1e-12) {
                const double anew = num / den;
                max_da = std::max(max_da, std::fabs(anew - a[f]));
                a[f] = anew;
            }
        }
        m->iterations = iter + 1;
        m->objective = 0.0;
        for (std::uint64_t i = 0; i < n_obs; ++i) {
            const auto& o = obs[i];
            const std::size_t ck = m->control_by_id[o.control_id];
            const std::size_t fi = m->frame_index[o.frame_id];
            const double r = o.value - z[ck] - a[fi];
            const double snr2 = o.snr * o.snr;
            m->objective += (snr2 / (1.0 + snr2)) *
                            huber_rho(r, cfg.huber_delta);
        }
        if (max_dz < cfg.tolerance && max_da < cfg.tolerance) break;
    }

    for (std::size_t k = 0; k < m->controls.size(); ++k) m->controls[k].z = z[k];
    m->frame_offset = std::move(a);
    m->component_count = 1;  // 连通分量分析在 W4 完整版（图遍历）实现

    // 模型哈希（占位）
    std::string h = sha256_hex(nullptr, 0);
    std::memcpy(m->info.model_hash, h.c_str(), 64);
    m->info.model_hash[64] = '\0';

    *out_model = static_cast<void*>(m);
    return 0;
}

int p2_upm_save(const void* model, const char* path) {
    (void)model; (void)path;
    return 1;  // W5 持久化实现
}

int p2_upm_open(const char* path, void** out_model) {
    (void)path; (void)out_model;
    return 1;  // W5
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
    const double offset = (it != m->frame_index.end())
        ? m->frame_offset[it->second] : 0.0;
    (void)leaf_ipix;
    for (std::uint64_t i = 0; i < count; ++i) {
        output_signal[i] = input_signal[i] - offset;
    }
    return 0;
}

int p2_upm_materialize_dense(const void* model, int target_order,
                             const char* cache_path) {
    if (model == nullptr || cache_path == nullptr) return 1;
    const Model* m = static_cast<const Model*>(model);
    if (target_order < 0) target_order = m->info.target_order;
    // W5 首版：dense cache = 稀疏控制点值（按 control_id 索引）写 JSON。
    // sparse=dense Gate：对同一控制点，materialize 后的取值必须等于稀疏
    // 模型 calibrate(参考帧) 的取值。
    std::FILE* f = std::fopen(cache_path, "w");
    if (!f) return 1;
    std::fprintf(f, "{\"target_order\":%d,\"source_hash\":\"%s\",",
                 target_order, m->info.model_hash);
    std::fprintf(f, "\"controls\":[");
    for (std::size_t k = 0; k < m->controls.size(); ++k) {
        if (k) std::fprintf(f, ",");
        std::fprintf(f, "[%llu,%.17g]",
                     static_cast<unsigned long long>(k), m->controls[k].z);
    }
    std::fprintf(f, "]}\n");
    std::fclose(f);
    return 0;
}

int p2_upm_dense_info(const void* model, const char* cache_path,
                      int* out_target_order, std::uint64_t* out_pixels,
                      char* out_source_hash, std::size_t hash_buf_size) {
    if (model == nullptr || cache_path == nullptr) return 1;
    const Model* m = static_cast<const Model*>(model);
    std::FILE* f = std::fopen(cache_path, "r");
    if (!f) return 1;
    char line[4096];
    if (std::fgets(line, sizeof(line), f) == nullptr) {
        std::fclose(f);
        return 1;
    }
    std::fclose(f);
    if (out_target_order) *out_target_order = m->info.target_order;
    if (out_pixels) *out_pixels = m->controls.size();
    if (out_source_hash && hash_buf_size > 0) {
        std::strncpy(out_source_hash, m->info.model_hash, hash_buf_size - 1);
        out_source_hash[hash_buf_size - 1] = '\0';
    }
    (void)line;
    return 0;
}

int p2_upm_close(void* model) {
    if (model == nullptr) return 0;
    delete static_cast<Model*>(model);
    return 0;
}

} // extern "C"