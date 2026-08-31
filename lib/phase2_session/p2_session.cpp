// lib/phase2_session/p2_session.cpp — Phase2 进程内会话实现 (API-P2-001) — CLI-005
// 错误映射(合同 §4): rc=1/INVALID→ACS_ERR_PARAM; rc=2→ACS_ERR_STATE; 其余→ACS_ERR_IO/INTERNAL。
#include "p2_session.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "astro/phase2/coverage.h"
#include "astro/phase2/sampler.h"
#include "astro/phase2/upm.h"

namespace {

using json = nlohmann::json;

struct SessionState {
    const astrocs_host_services_v1* host = nullptr;
    std::string last_error;
    json manifest;
    bool ran = false;

    void log(int level, const char* component, const std::string& msg) const {
        if (host && host->logger.log)
            host->logger.log(host->logger.user_data, level, component, msg.c_str());
    }
    bool cancelled() const {
        return host && host->cancel.is_cancelled &&
               host->cancel.is_cancelled(host->cancel.user_data);
    }
    void stage(const char* name, const char* status, json extra = json::object()) {
        json st = {{"name", name}, {"status", status}};
        for (auto it = extra.begin(); it != extra.end(); ++it) st[it.key()] = it.value();
        manifest["stages"].push_back(st);
    }
};

acs_status map_rc(int rc, SessionState* s, const char* what) {
    if (rc == 0) return ACS_OK;
    s->last_error = std::string(what) + " rc=" + std::to_string(rc);
    s->manifest["error_kind"] = "input";
    if (rc == 1) return ACS_ERR_PARAM;
    if (rc == 2) return ACS_ERR_STATE;   // 合同 §4: build fail(production 显式缺 ivar 等)
    return ACS_ERR_INTERNAL;
}

}  // namespace

extern "C" {

acs_status p2_session_create(const astrocs_host_services_v1* host, acs_handle* out) {
    if (!host || host->struct_size != sizeof(astrocs_host_services_v1) ||
        host->abi_version != ACS_ABI_VERSION_V1)
        return ACS_ERR_ABI_MISMATCH;
    if (!out) return ACS_ERR_PARAM;
    auto* s = new (std::nothrow) SessionState();
    if (!s) return ACS_ERR_NOMEM;
    s->host = host;
    s->manifest = {{"kind", "astrocs_phase2_session"}, {"stages", json::array()}};
    *out = reinterpret_cast<acs_handle>(s);
    return ACS_OK;
}

acs_status p2_session_validate(acs_handle h, const acs_span_u8 config_json) {
    auto* s = reinterpret_cast<SessionState*>(h);
    if (!s || !config_json.data || config_json.count == 0) return ACS_ERR_PARAM;
    json doc;
    try {
        doc = json::parse(std::string(reinterpret_cast<const char*>(config_json.data),
                                      static_cast<size_t>(config_json.count)));
    } catch (const json::parse_error& e) {
        s->last_error = std::string("config parse: ") + e.what();
        return ACS_ERR_PARAM;
    }
    if (!doc.is_object()) { s->last_error = "config must be an object"; return ACS_ERR_PARAM; }
    for (const char* req : {"hips_paths", "output_dir"})
        if (!doc.contains(req)) {
            s->last_error = std::string("missing key '") + req + "'";
            return ACS_ERR_PARAM;
        }
    if (!doc["hips_paths"].is_array() || doc["hips_paths"].empty() ||
        !doc["output_dir"].is_string()) {
        s->last_error = "hips_paths must be non-empty array; output_dir must be string";
        return ACS_ERR_PARAM;
    }
    for (const auto& p : doc["hips_paths"])
        if (!p.is_string()) { s->last_error = "hips_paths items must be strings"; return ACS_ERR_PARAM; }
    if (doc.contains("upm") && !doc["upm"].is_object()) {
        s->last_error = "upm must be an object";
        return ACS_ERR_PARAM;
    }
    return ACS_OK;
}

acs_status p2_session_run(acs_handle h, const acs_span_u8 config_json) {
    auto* s = reinterpret_cast<SessionState*>(h);
    if (!s || !config_json.data || config_json.count == 0) return ACS_ERR_PARAM;
    json doc;
    try {
        doc = json::parse(std::string(reinterpret_cast<const char*>(config_json.data),
                                      static_cast<size_t>(config_json.count)));
    } catch (...) {
        s->last_error = "config parse failed (validate first)";
        return ACS_ERR_PARAM;
    }
    std::vector<std::string> paths;
    for (const auto& p : doc["hips_paths"]) paths.push_back(p.get<std::string>());
    std::vector<const char*> hips(paths.size());
    for (size_t i = 0; i < paths.size(); ++i) hips[i] = paths[i].c_str();
    s->log(ACS_LOG_INFO, "phase2",
           "session run: budget workers=" + std::to_string(s->host->budget.max_workers) +
               " (cpus=" + std::to_string(s->host->budget.available_cpus) + ")");

    // ── 阶段 1: coverage(取消点=阶段边界) ──
    if (s->cancelled()) { s->manifest["stages"].push_back({{"name", "coverage"}, {"status", "cancelled"}}); return ACS_ERR_CANCELLED; }
    s->stage("coverage", "running");
    P2CoverageResult cov{};
    cov.n_inputs = hips.size();
    cov.inputs = nullptr;   // 合同: inputs 由调用方分配 — 首查时仅查询 union 容量
    int rc = p2_coverage_build(hips.data(), hips.size(), &cov);
    if (rc != 0 && cov.n_union_cells == 0) {
        s->stage("coverage", "fail", {{"rc", rc}});
        return map_rc(rc, s, "coverage_build");
    }
    std::vector<P2HipsInputInfo> inputs(hips.size());
    for (size_t i = 0; i < hips.size(); ++i) {
        std::memset(&inputs[i], 0, sizeof(P2HipsInputInfo));
        std::snprintf(inputs[i].hips_path, sizeof(inputs[i].hips_path), "%s", paths[i].c_str());
    }
    cov.inputs = inputs.data();
    std::vector<P2MocCell> cells(cov.n_union_cells > 0 ? cov.n_union_cells : 1);
    cov.union_cells = cells.data();
    rc = p2_coverage_build(hips.data(), hips.size(), &cov);
    if (rc != 0) {
        s->stage("coverage", "fail", {{"rc", rc}});
        return map_rc(rc, s, "coverage_build");
    }
    std::unique_ptr<P2CoverageResult, void (*)(P2CoverageResult*)> cov_guard(
        &cov, [](P2CoverageResult* c) { p2_coverage_free(c); });
    s->stage("coverage", "ok", {{"n_inputs", cov.n_inputs},
                                {"n_union_cells", cov.n_union_cells},
                                {"target_order", cov.target_order}});
    s->log(ACS_LOG_INFO, "phase2", "stage coverage ok: cells=" + std::to_string(cov.n_union_cells));

    // ── 阶段 2: sample(P2-001: 预算绑定 §3 — sampler 走 Runtime lease 多 worker;
    // 1 worker 仅作 reference; 生产 N-worker 并行同生产符号) ──
    if (s->cancelled()) { s->stage("sample", "cancelled"); return ACS_ERR_CANCELLED; }
    s->stage("sample", "running");
    P2SamplerConfig sc = p2_sampler_default_config();
    sc.cpu_workers = static_cast<int>(s->host->budget.max_workers);
    std::uint64_t n_obs = 0, n_controls = 0;
    P2SampleStats stats{};
    rc = p2_sample_controls(&cov, hips.data(), &sc, nullptr, 0, &n_obs, &n_controls, &stats,
                            nullptr, 0, nullptr, 0);
    if (rc != 0 && n_obs == 0) {
        s->stage("sample", "fail", {{"rc", rc}});
        return map_rc(rc, s, "sample_controls(query)");
    }
    std::vector<P2ControlObservation> obs(n_obs > 0 ? n_obs : 1);
    std::vector<P2ControlNode> nodes(n_controls > 0 ? n_controls : 1);
    char err[512] = {0};
    rc = p2_sample_controls(&cov, hips.data(), &sc, obs.data(), n_obs, &n_obs, &n_controls,
                            &stats, nodes.data(), n_controls, err, sizeof(err));
    if (rc != 0) {
        s->stage("sample", "fail", {{"rc", rc}, {"err", err}});
        return map_rc(rc, s, "sample_controls");
    }
    obs.resize(n_obs);
    s->stage("sample", "ok", {{"n_obs", n_obs}, {"n_controls", n_controls},
                              {"accepted_obs", stats.accepted_observations},
                              {"overlap_controls", stats.overlap_controls}});
    s->log(ACS_LOG_INFO, "phase2", "stage sample ok: obs=" + std::to_string(n_obs) +
               " overlap_controls=" + std::to_string(stats.overlap_controls));

    // ── 阶段 3: upm build(预算绑定: blocks=budget; 取消=整模型不写半成品) ──
    if (s->cancelled()) { s->stage("upm_build", "cancelled"); return ACS_ERR_CANCELLED; }
    s->stage("upm_build", "running");
    P2UpmBuildConfig uc{};
    uc.robust_loss = 0;              // huber(首版冻结)
    uc.snr_weight_mode = 0;          // snr2_normalized
    uc.huber_delta = 1.345;
    uc.max_iterations = 100;
    uc.tolerance = 1e-6;
    uc.target_order = static_cast<int>(cov.target_order);
    // 空间 UPM 必须显式 control leaf 层级(order=target+9); 取 coverage 实测值
    uc.sigma_floor = 1e-3;
    uc.support_power = 1.0;
    uc.use_ivar_weight = 1;
    uc.control_reliability = 1.0;
    uc.cpu_workers = static_cast<int>(s->host->budget.max_workers);   // §3: blocks(budget)
    const json& upm_cfg = doc.contains("upm") ? doc["upm"] : json::object();
    if (upm_cfg.contains("max_iterations"))
        uc.max_iterations = upm_cfg["max_iterations"].get<int>();
    if (upm_cfg.contains("huber_delta"))
        uc.huber_delta = upm_cfg["huber_delta"].get<double>();
    if (upm_cfg.contains("smoothing_lambda"))
        uc.smoothing_lambda = upm_cfg["smoothing_lambda"].get<double>();
    void* model = nullptr;
    rc = p2_upm_build(obs.data(), n_obs, &uc, &model);
    if (rc != 0 || !model) {
        s->stage("upm_build", "fail", {{"rc", rc}});
        return map_rc(rc, s, "upm_build");
    }
    P2ModelInfo info{};
    if (p2_upm_info(model, &info) == 0) {
        json node = {{"control_count", info.control_count},
                     {"observation_count", info.observation_count},
                     {"component_count", info.component_count},
                     {"target_order", info.target_order}};
        if (info.model_hash[0]) node["model_hash"] = std::string(info.model_hash);
        s->stage("upm_build", "ok", node);
    } else {
        s->stage("upm_build", "ok");
    }

    // ── 阶段 4: persist(可选; 串行 IO; 整模型取消点) ──
    if (doc.value("persist_upm", false) && doc.contains("upm_save_path")) {
        if (s->cancelled()) {
            p2_upm_close(model);
            s->stage("persist", "cancelled");
            return ACS_ERR_CANCELLED;
        }
        s->stage("persist", "running");
        const std::string save_path = doc["upm_save_path"].get<std::string>();
        if (p2_upm_save(model, save_path.c_str()) != 0) {
            p2_upm_close(model);
            s->stage("persist", "fail");
            s->last_error = "upm_save failed: " + save_path;
            s->manifest["error_kind"] = "output";
            return ACS_ERR_IO;
        }
        s->manifest["artifacts"] = s->manifest.value("artifacts", json::array());
        s->manifest["artifacts"].push_back(save_path);
        s->stage("persist", "ok", {{"path", save_path}});
    }
    p2_upm_close(model);   // 所有权合同 §1: session 持有, p2_upm_close 释放

    s->manifest["n_inputs"] = cov.n_inputs;
    s->manifest["n_obs"] = n_obs;
    s->manifest["status"] = "complete";
    s->ran = true;
    return ACS_OK;
}

acs_status p2_session_inspect(acs_handle h, acs_span_u8* out) {
    auto* s = reinterpret_cast<SessionState*>(h);
    if (!s || !out) return ACS_ERR_PARAM;
    if (!s->ran && s->last_error.empty()) s->manifest["status"] = "created";
    else if (!s->ran) {
        s->manifest["status"] = "failed";
        s->manifest["error"] = s->last_error;
    }
    const std::string t = s->manifest.dump(2);
    const uint64_t n = t.size() + 1;
    void* p = s->host->allocator.alloc(s->host->allocator.user_data, n, 16);
    if (!p) return ACS_ERR_NOMEM;
    std::memcpy(p, t.c_str(), n);
    out->data = static_cast<uint8_t*>(p);
    out->count = n - 1;
    return ACS_OK;
}

acs_status p2_session_destroy(acs_handle h) {
    auto* s = reinterpret_cast<SessionState*>(h);
    if (!s) return ACS_ERR_PARAM;
    delete s;
    return ACS_OK;
}

}  // extern "C"

namespace astrocs::phase2 {
std::string last_error(acs_handle h) {
    auto* s = reinterpret_cast<SessionState*>(h);
    return s ? s->last_error : std::string();
}
}  // namespace astrocs::phase2
