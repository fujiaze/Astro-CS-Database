// lib/acr/routing/route_profile_v2.cpp
#include "route_profile_v2.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <string>

namespace astro::compute::routing {

std::string scenario_id(InputResidency input,
                        OutputMaterialization output,
                        std::uint32_t reuse_count) {
    const std::string in =
        (input == InputResidency::DeviceResident) ? "resident" : "cold";
    const std::string out =
        (output == OutputMaterialization::KeepDevice) ? "device_output"
                                                       : "host_output";
    if (reuse_count > 1) {
        return in + "_reuse" + std::to_string(reuse_count) + "_" + out;
    }
    return in + "_" + out;
}

std::string RouteScenarioKey::id() const {
    return scenario_id(input, output, reuse_count_hint);
}

namespace {

nlohmann::json sample_json(const RouteSamplePoint& s) {
    nlohmann::json j;
    j["output_items"] = s.output_items;
    j["frame_count"] = s.frame_count;
    j["reuse_count"] = s.reuse_count;
    j["input_bytes"] = s.input_bytes;
    j["output_bytes"] = s.output_bytes;
    j["cpu_chunk_items"] = s.cpu_chunk_items;
    j["gpu_chunk_items"] = s.gpu_chunk_items;
    j["median_ms"] = s.median_ms;
    j["p90_ms"] = s.p90_ms;
    j["statistics_scope"] = "median_timed_sample";
    j["stats"]["cpu_items"] = s.cpu_items;
    j["stats"]["gpu_items"] = s.gpu_items;
    j["stats"]["cpu_chunks"] = s.cpu_chunks;
    j["stats"]["gpu_chunks"] = s.gpu_chunks;
    j["stats"]["setup_h2d_bytes"] = s.setup_h2d_bytes;
    j["stats"]["timed_h2d_bytes"] = s.timed_h2d_bytes;
    j["stats"]["timed_d2h_bytes"] = s.timed_d2h_bytes;
    j["stats"]["peak_ram_bytes"] = s.peak_ram_bytes;
    j["stats"]["absolute_peak_vram_bytes"] = s.absolute_peak_vram_bytes;
    return j;
}

RouteSamplePoint sample_from_json(const nlohmann::json& j) {
    RouteSamplePoint s;
    if (j.contains("output_items")) s.output_items = j["output_items"].get<std::uint64_t>();
    if (j.contains("frame_count")) s.frame_count = j["frame_count"].get<std::uint32_t>();
    if (j.contains("reuse_count")) s.reuse_count = j["reuse_count"].get<std::uint32_t>();
    if (j.contains("input_bytes")) s.input_bytes = j["input_bytes"].get<std::uint64_t>();
    if (j.contains("output_bytes")) s.output_bytes = j["output_bytes"].get<std::uint64_t>();
    if (j.contains("cpu_chunk_items")) s.cpu_chunk_items = j["cpu_chunk_items"].get<std::uint64_t>();
    if (j.contains("gpu_chunk_items")) s.gpu_chunk_items = j["gpu_chunk_items"].get<std::uint64_t>();
    if (j.contains("median_ms")) s.median_ms = j["median_ms"].get<double>();
    if (j.contains("p90_ms")) s.p90_ms = j["p90_ms"].get<double>();
    if (j.contains("stats")) {
        const auto& st = j["stats"];
        if (st.contains("cpu_items")) s.cpu_items = st["cpu_items"].get<std::uint64_t>();
        if (st.contains("gpu_items")) s.gpu_items = st["gpu_items"].get<std::uint64_t>();
        if (st.contains("cpu_chunks")) s.cpu_chunks = st["cpu_chunks"].get<std::uint64_t>();
        if (st.contains("gpu_chunks")) s.gpu_chunks = st["gpu_chunks"].get<std::uint64_t>();
        if (st.contains("setup_h2d_bytes")) s.setup_h2d_bytes = st["setup_h2d_bytes"].get<std::uint64_t>();
        if (st.contains("timed_h2d_bytes")) s.timed_h2d_bytes = st["timed_h2d_bytes"].get<std::uint64_t>();
        if (st.contains("timed_d2h_bytes")) s.timed_d2h_bytes = st["timed_d2h_bytes"].get<std::uint64_t>();
        if (st.contains("peak_ram_bytes")) s.peak_ram_bytes = st["peak_ram_bytes"].get<std::uint64_t>();
        if (st.contains("absolute_peak_vram_bytes")) {
            s.absolute_peak_vram_bytes = st["absolute_peak_vram_bytes"].get<std::uint64_t>();
        }
    }
    return s;
}

nlohmann::json path_json(const RoutePath& p) {
    nlohmann::json j;
    j["model_available"] = p.model_available;
    j["model_trusted"] = p.model_trusted;
    j["eligible"] = p.eligible;
    j["reason"] = p.reason;
    j["samples"] = nlohmann::json::array();
    for (const auto& s : p.samples) j["samples"].push_back(sample_json(s));
    j["validated_domain"]["min_output_items"] = p.min_output_items;
    j["validated_domain"]["max_output_items"] = p.max_output_items;
    j["validated_domain"]["frame_counts"] = p.frame_counts;
    j["validated_domain"]["allow_tail_extrapolation"] =
        p.allow_tail_extrapolation;
    j["interpolation_id"] = p.interpolation_id;
    j["holdout_count"] = p.holdout_count;
    j["median_error_ratio"] = p.median_error_ratio;
    j["max_error_ratio"] = p.max_error_ratio;
    j["refinement_probe_count"] = p.refinement_probe_count;
    j["final_holdout_count"] = p.final_holdout_count;
    j["adaptive_rounds_used"] = p.adaptive_rounds_used;
    j["final_median_error_ratio"] = p.final_median_error_ratio;
    j["final_max_error_ratio"] = p.final_max_error_ratio;
    j["p95_error_ratio"] = p.p95_error_ratio;
    j["metrics_complete"] = p.metrics_complete;
    return j;
}

RoutePath path_from_json(const nlohmann::json& j) {
    RoutePath p;
    if (j.contains("model_available")) {
        p.model_available = j["model_available"].get<bool>();
    }
    if (j.contains("model_trusted")) {
        p.model_trusted = j["model_trusted"].get<bool>();
    }
    if (j.contains("eligible")) p.eligible = j["eligible"].get<bool>();
    // 旧 Profile（只有 eligible）：向后兼容语义映射
    if (!j.contains("model_available") && j.contains("eligible")) {
        p.model_available = p.eligible;
    }
    if (!j.contains("model_trusted") && j.contains("eligible")) {
        p.model_trusted = p.eligible;
    }
    if (!j.contains("model_available") && !j.contains("eligible")) {
        p.model_available = !p.samples.empty();
        p.model_trusted = false;
    }
    if (j.contains("reason")) p.reason = j["reason"].get<std::string>();
    if (j.contains("samples") && j["samples"].is_array()) {
        for (const auto& s : j["samples"]) {
            p.samples.push_back(sample_from_json(s));
        }
    }
    if (j.contains("validated_domain")) {
        const auto& vd = j["validated_domain"];
        if (vd.contains("min_output_items")) {
            p.min_output_items = vd["min_output_items"].get<std::uint64_t>();
        }
        if (vd.contains("max_output_items")) {
            p.max_output_items = vd["max_output_items"].get<std::uint64_t>();
        }
        if (vd.contains("frame_counts") && vd["frame_counts"].is_array()) {
            for (const auto& f : vd["frame_counts"]) {
                p.frame_counts.push_back(f.get<std::uint32_t>());
            }
        }
        if (vd.contains("allow_tail_extrapolation")) {
            p.allow_tail_extrapolation =
                vd["allow_tail_extrapolation"].get<bool>();
        }
    }
    if (j.contains("median_error_ratio")) {
        p.median_error_ratio = j["median_error_ratio"].get<double>();
    }
    if (j.contains("max_error_ratio")) {
        p.max_error_ratio = j["max_error_ratio"].get<double>();
    }
    if (j.contains("interpolation_id")) {
        p.interpolation_id = j["interpolation_id"].get<std::string>();
    }
    if (j.contains("holdout_count")) {
        p.holdout_count = j["holdout_count"].get<std::size_t>();
    }
    if (j.contains("refinement_probe_count")) {
        p.refinement_probe_count =
            j["refinement_probe_count"].get<std::size_t>();
    }
    if (j.contains("final_holdout_count")) {
        p.final_holdout_count = j["final_holdout_count"].get<std::size_t>();
    }
    if (j.contains("adaptive_rounds_used")) {
        p.adaptive_rounds_used = j["adaptive_rounds_used"].get<std::size_t>();
    }
    if (j.contains("final_median_error_ratio")) {
        p.final_median_error_ratio =
            j["final_median_error_ratio"].get<double>();
    }
    if (j.contains("final_max_error_ratio")) {
        p.final_max_error_ratio = j["final_max_error_ratio"].get<double>();
    }
    if (j.contains("p95_error_ratio")) {
        p.p95_error_ratio = j["p95_error_ratio"].get<double>();
    }
    if (j.contains("metrics_complete")) {
        p.metrics_complete = j["metrics_complete"].get<bool>();
    }
    return p;
}

} // anonymous namespace

std::string serialize_route_profile_v2(const RouteProfileV2& profile) {
    nlohmann::json root;
    root["schema_version"] = profile.schema_version;
    root["profile_state"] = profile.profile_state;
    root["calibration_preset"] = profile.calibration_preset;
    root["calibration_head"] = profile.calibration_head;
    root["calibration_run_id"] = profile.calibration_run_id;
    root["generated_utc"] = profile.generated_utc;
    root["fingerprint"]["cpu"] = profile.fingerprint_cpu;
    root["fingerprint"]["gpus"] = profile.fingerprint_gpus;
    root["fingerprint"]["compiler"] = profile.fingerprint_compiler;
    root["fingerprint"]["runtime_kernel_hash"] =
        profile.fingerprint_runtime_kernel_hash;
    for (const auto& op : profile.operations) {
        nlohmann::json o;
        o["operation_id"] = op.operation_id;
        o["workload_axes"] = op.workload_axes;
        o["cpu_chunk_candidates"] = op.cpu_chunk_candidates;
        o["gpu_chunk_candidates"] = op.gpu_chunk_candidates;
        o["scenarios"] = nlohmann::json::array();
        for (const auto& sc : op.scenarios) {
            nlohmann::json sj;
            sj["scenario_id"] = sc.scenario_id;
            sj["supported"] = sc.supported;
            sj["scenario_qualified"] = sc.scenario_qualified;
            sj["routing_trusted"] = sc.routing_trusted;
            sj["qualification_reason"] = sc.qualification_reason;
            sj["openmp"] = path_json(sc.openmp);
            sj["gpu_direct"] = path_json(sc.gpu_direct);
            sj["mixed"] = path_json(sc.mixed);
            sj["final_holdout_count"] = sc.final_holdout_count;
            sj["route_replay_count"] = sc.route_replay_count;
            sj["route_replay_max_slowdown_ratio"] =
                sc.route_replay_max_slowdown_ratio;
            sj["route_replay"] = nlohmann::json::array();
            for (const auto& rp : sc.route_replay) {
                sj["route_replay"].push_back(
                    {{"output_items", rp.output_items},
                     {"frame_count", rp.frame_count},
                     {"chosen_route", rp.chosen_route},
                     {"best_route", rp.best_route},
                     {"chosen_actual_ms", rp.chosen_actual_ms},
                     {"actual_best_ms", rp.actual_best_ms},
                     {"predicted_ms", rp.predicted_ms},
                     {"within_best_10pct", rp.within_best_10pct}});
            }
            o["scenarios"].push_back(std::move(sj));
        }
        o["cpu_chunk_service"] = nlohmann::json::array();
        o["gpu_chunk_service"] = nlohmann::json::array();
        for (const auto& c : op.cpu_chunk_service) {
            o["cpu_chunk_service"].push_back(
                {{"chunk_items", c.chunk_items},
                 {"frame_count", c.frame_count},
                 {"median_service_ms", c.median_service_ms},
                 {"p90_service_ms", c.p90_service_ms},
                 {"sample_count", c.sample_count}});
        }
        for (const auto& c : op.gpu_chunk_service) {
            o["gpu_chunk_service"].push_back(
                {{"chunk_items", c.chunk_items},
                 {"frame_count", c.frame_count},
                 {"median_service_ms", c.median_service_ms},
                 {"p90_service_ms", c.p90_service_ms},
                 {"sample_count", c.sample_count}});
        }
        o["mixed_overhead"]["fixed_ms"] = op.mixed_fixed_overhead_ms;
        o["mixed_overhead"]["per_token_ms"] = op.mixed_per_token_ms;
        o["qualified"] = op.qualified;
        o["qualification_reason"] = op.qualification_reason;
        o["datasets"]["fit"]["items"] = op.datasets.fit_items;
        o["datasets"]["fit"]["frames"] = op.datasets.fit_frames;
        o["datasets"]["probe"]["items"] = op.datasets.probe_items;
        o["datasets"]["probe"]["frames"] = op.datasets.probe_frames;
        o["datasets"]["final"]["items"] = op.datasets.final_items;
        o["datasets"]["final"]["frames"] = op.datasets.final_frames;
        o["datasets"]["disjoint_verified"] = op.datasets.disjoint_verified;
        o["datasets"]["disjoint_reason"] = op.datasets.disjoint_reason;
        root["operations"].push_back(std::move(o));
    }
    return root.dump();
}

bool write_route_profile_v2_to_file(const std::string& path,
                                    const RouteProfileV2& profile) {
    std::FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;
    std::string s = serialize_route_profile_v2(profile);
    std::fwrite(s.data(), 1, s.size(), f);
    std::fputc('\n', f);
    std::fclose(f);
    return true;
}

bool read_route_profile_v2_from_file(const std::string& path,
                                     RouteProfileV2& out) {
    std::FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long len = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (len <= 0) { std::fclose(f); return false; }
    std::string s(static_cast<std::size_t>(len), '\0');
    std::fread(s.data(), 1, static_cast<std::size_t>(len), f);
    std::fclose(f);

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(s);
    } catch (...) {
        return false;
    }
    if (!root.is_object() || !root.contains("operations")) return false;

    RouteProfileV2 p;
    if (root.contains("schema_version")) {
        p.schema_version = root["schema_version"].get<std::string>();
    }
    if (root.contains("profile_state")) {
        p.profile_state = root["profile_state"].get<std::string>();
    }
    if (root.contains("calibration_preset")) {
        p.calibration_preset = root["calibration_preset"].get<std::string>();
    }
    if (root.contains("calibration_head")) {
        p.calibration_head = root["calibration_head"].get<std::string>();
    }
    if (root.contains("calibration_run_id")) {
        p.calibration_run_id = root["calibration_run_id"].get<std::string>();
    }
    if (root.contains("generated_utc")) {
        p.generated_utc = root["generated_utc"].get<std::string>();
    }
    if (root.contains("fingerprint")) {
        const auto& fp = root["fingerprint"];
        if (fp.contains("cpu")) p.fingerprint_cpu = fp["cpu"].get<std::string>();
        if (fp.contains("compiler")) {
            p.fingerprint_compiler = fp["compiler"].get<std::string>();
        }
        if (fp.contains("runtime_kernel_hash")) {
            p.fingerprint_runtime_kernel_hash =
                fp["runtime_kernel_hash"].get<std::string>();
        }
        if (fp.contains("gpus") && fp["gpus"].is_array()) {
            for (const auto& g : fp["gpus"]) {
                p.fingerprint_gpus.push_back(g.get<std::string>());
            }
        }
    }
    for (const auto& o : root["operations"]) {
        OperationRouteProfile op;
        op.operation_id = o.at("operation_id").get<std::string>();
        if (o.contains("workload_axes") && o["workload_axes"].is_array()) {
            op.workload_axes.clear();
            for (const auto& a : o["workload_axes"]) {
                op.workload_axes.push_back(a.get<std::string>());
            }
        }
        if (o.contains("cpu_chunk_candidates") && o["cpu_chunk_candidates"].is_array()) {
            for (const auto& c : o["cpu_chunk_candidates"]) {
                op.cpu_chunk_candidates.push_back(c.get<std::uint64_t>());
            }
        }
        if (o.contains("gpu_chunk_candidates") && o["gpu_chunk_candidates"].is_array()) {
            for (const auto& c : o["gpu_chunk_candidates"]) {
                op.gpu_chunk_candidates.push_back(c.get<std::uint64_t>());
            }
        }
        if (o.contains("scenarios") && o["scenarios"].is_array()) {
            for (const auto& sc : o["scenarios"]) {
                RouteScenarioProfile sp;
                sp.scenario_id = sc.at("scenario_id").get<std::string>();
                if (sc.contains("supported")) {
                    sp.supported = sc["supported"].get<bool>();
                }
                if (sc.contains("scenario_qualified")) {
                    sp.scenario_qualified =
                        sc["scenario_qualified"].get<bool>();
                }
                if (sc.contains("routing_trusted")) {
                    sp.routing_trusted =
                        sc["routing_trusted"].get<bool>();
                }
                // 旧 Profile 兼容：无 routing_trusted 时回填 scenario_qualified
                if (!sc.contains("routing_trusted")) {
                    sp.routing_trusted = sp.scenario_qualified;
                }
                if (sc.contains("qualification_reason")) {
                    sp.qualification_reason =
                        sc["qualification_reason"].get<std::string>();
                }
                if (sc.contains("openmp")) sp.openmp = path_from_json(sc["openmp"]);
                if (sc.contains("gpu_direct")) sp.gpu_direct = path_from_json(sc["gpu_direct"]);
                if (sc.contains("mixed")) sp.mixed = path_from_json(sc["mixed"]);
                if (sc.contains("final_holdout_count")) {
                    sp.final_holdout_count =
                        sc["final_holdout_count"].get<std::size_t>();
                }
                if (sc.contains("route_replay_count")) {
                    sp.route_replay_count =
                        sc["route_replay_count"].get<std::size_t>();
                }
                if (sc.contains("route_replay_max_slowdown_ratio")) {
                    sp.route_replay_max_slowdown_ratio =
                        sc["route_replay_max_slowdown_ratio"].get<double>();
                }
                if (sc.contains("route_replay") &&
                    sc["route_replay"].is_array()) {
                    for (const auto& r : sc["route_replay"]) {
                        RouteReplayPoint rp;
                        rp.output_items =
                            r.value("output_items", 0ull);
                        rp.frame_count =
                            r.value("frame_count", 0u);
                        rp.chosen_route =
                            r.value("chosen_route", std::string(""));
                        rp.best_route =
                            r.value("best_route", std::string(""));
                        rp.chosen_actual_ms =
                            r.value("chosen_actual_ms", 0.0);
                        rp.actual_best_ms =
                            r.value("actual_best_ms", 0.0);
                        rp.predicted_ms =
                            r.value("predicted_ms", 0.0);
                        rp.within_best_10pct =
                            r.value("within_best_10pct", false);
                        sp.route_replay.push_back(std::move(rp));
                    }
                }
                op.scenarios.push_back(std::move(sp));
            }
        }
        auto read_chunk_service = [&](const nlohmann::json& arr,
                                      bool gpu) {
            if (arr.is_array()) {
                for (const auto& c : arr) {
                    if (gpu) {
                        op.gpu_chunk_service.push_back(
                            {c.value("chunk_items", 0u),
                             c.value("frame_count", 0u),
                             c.value("median_service_ms", 0.0),
                             c.value("p90_service_ms", 0.0),
                             c.value("sample_count", 0u)});
                    } else {
                        op.cpu_chunk_service.push_back(
                            {c.value("chunk_items", 0u),
                             c.value("frame_count", 0u),
                             c.value("median_service_ms", 0.0),
                             c.value("p90_service_ms", 0.0),
                             c.value("sample_count", 0u)});
                    }
                }
            }
        };
        if (o.contains("cpu_chunk_service")) {
            read_chunk_service(o["cpu_chunk_service"], false);
        }
        if (o.contains("gpu_chunk_service")) {
            read_chunk_service(o["gpu_chunk_service"], true);
        }
        if (o.contains("chunk_service_curves")) {  // 兼容旧布局
            const auto& cc = o["chunk_service_curves"];
            if (cc.contains("cpu") && cc["cpu"].is_array()) {
                for (const auto& c : cc["cpu"]) {
                    op.cpu_chunk_service.push_back(
                        {c.value("chunk_items", 0u),
                         c.value("frame_count", 0u),
                         c.value("median_service_ms", 0.0),
                         c.value("p90_service_ms", 0.0),
                         c.value("sample_count", 0u)});
                }
            }
            if (cc.contains("gpu") && cc["gpu"].is_array()) {
                for (const auto& c : cc["gpu"]) {
                    op.gpu_chunk_service.push_back(
                        {c.value("chunk_items", 0u),
                         c.value("frame_count", 0u),
                         c.value("median_service_ms", 0.0),
                         c.value("p90_service_ms", 0.0),
                         c.value("sample_count", 0u)});
                }
            }
        }
        if (o.contains("mixed_overhead")) {
            op.mixed_fixed_overhead_ms =
                o["mixed_overhead"].value("fixed_ms", 0.0);
            op.mixed_per_token_ms =
                o["mixed_overhead"].value("per_token_ms", 0.0);
        }
        if (o.contains("qualified")) op.qualified = o["qualified"].get<bool>();
        if (o.contains("qualification_reason")) {
            op.qualification_reason =
                o["qualification_reason"].get<std::string>();
        }
        if (o.contains("datasets")) {
            const auto& ds = o["datasets"];
            auto read_group = [&](const char* key,
                                  std::vector<std::uint64_t>& items,
                                  std::vector<std::uint32_t>& frames) {
                if (ds.contains(key)) {
                    const auto& g = ds[key];
                    if (g.contains("items") && g["items"].is_array()) {
                        for (const auto& v : g["items"]) {
                            items.push_back(v.get<std::uint64_t>());
                        }
                    }
                    if (g.contains("frames") && g["frames"].is_array()) {
                        for (const auto& v : g["frames"]) {
                            frames.push_back(v.get<std::uint32_t>());
                        }
                    }
                }
            };
            read_group("fit", op.datasets.fit_items, op.datasets.fit_frames);
            read_group("probe", op.datasets.probe_items,
                       op.datasets.probe_frames);
            read_group("final", op.datasets.final_items,
                       op.datasets.final_frames);
            if (ds.contains("disjoint_verified")) {
                op.datasets.disjoint_verified =
                    ds["disjoint_verified"].get<bool>();
            }
            if (ds.contains("disjoint_reason")) {
                op.datasets.disjoint_reason =
                    ds["disjoint_reason"].get<std::string>();
            }
        }
        p.operations.push_back(std::move(op));
    }
    out = std::move(p);
    return !out.operations.empty();
}

bool validate_route_profile_v2(const RouteProfileV2& profile,
                               std::string& error) {
    if (profile.schema_version != "acr-operation-route-profile-2") {
        error = "schema_version must be acr-operation-route-profile-2";
        return false;
    }
    if (profile.profile_state != "qualified" &&
        profile.profile_state != "partial" &&
        profile.profile_state != "diagnostic" &&
        profile.profile_state != "stale") {
        error = "invalid profile_state";
        return false;
    }
    if (profile.fingerprint_cpu.empty() ||
        profile.fingerprint_compiler.empty() ||
        profile.fingerprint_runtime_kernel_hash.size() < 16) {
        error = "fingerprint incomplete";
        return false;
    }
    if (profile.operations.empty()) {
        error = "operations empty";
        return false;
    }
    for (const auto& op : profile.operations) {
        if (op.operation_id.empty()) { error = "operation_id empty"; return false; }
        if (op.workload_axes != std::vector<std::string>{"output_items", "frame_count"}) {
            error = "workload_axes must be [output_items, frame_count]";
            return false;
        }
        if (op.scenarios.empty()) { error = "scenarios empty"; return false; }
        for (const auto& sc : op.scenarios) {
            if (sc.scenario_id.empty()) { error = "scenario_id empty"; return false; }
            const auto check_path = [&](const RoutePath& p, const char* name) {
                if (p.model_trusted) {
                    if (p.samples.empty()) {
                        error = "trusted path without samples: " + sc.scenario_id + "/" + name;
                        return false;
                    }
                    if (p.min_output_items == 0 || p.max_output_items == 0) {
                        error = "validated domain missing: " + sc.scenario_id + "/" + name;
                        return false;
                    }
                    if (p.frame_counts.empty()) {
                        error = "frame_counts missing: " + sc.scenario_id + "/" + name;
                        return false;
                    }
                }
                if (p.model_available && p.samples.empty()) {
                    error = "available path without samples: " + sc.scenario_id + "/" + name;
                    return false;
                }
                for (const auto& s : p.samples) {
                    if (s.output_items == 0 || s.frame_count == 0 ||
                        s.median_ms <= 0.0 || s.p90_ms <= 0.0) {
                        error = "invalid sample in " + sc.scenario_id + "/" + name;
                        return false;
                    }
                }
                return true;
            };
            if (!check_path(sc.openmp, "openmp")) return false;
            if (!check_path(sc.gpu_direct, "gpu_direct")) return false;
            if (!check_path(sc.mixed, "mixed")) return false;
            // eligible 兼容字段必须与 model_trusted 一致
            if (sc.openmp.eligible != sc.openmp.model_trusted ||
                sc.gpu_direct.eligible != sc.gpu_direct.model_trusted ||
                sc.mixed.eligible != sc.mixed.model_trusted) {
                error = "eligible != model_trusted in scenario " +
                        sc.scenario_id;
                return false;
            }
        }
        // Dispatcher Finalization（08 计划 1）：Route-centric 资格。
        // Operation 资格 = required 场景全部 routing_trusted
        // （旧 Profile 回填后 routing_trusted == scenario_qualified）。
        const std::vector<std::string> required{
            "cold_host_output", "resident_host_output",
            "resident_reuse4_host_output"};
        for (const auto& rid : required) {
            const auto it = std::find_if(
                op.scenarios.begin(), op.scenarios.end(),
                [&](const RouteScenarioProfile& s) {
                    return s.scenario_id == rid;
                });
            if (it == op.scenarios.end()) {
                error = "required scenario missing: " + rid;
                return false;
            }
        }
        const bool op_qualified =
            std::all_of(required.begin(), required.end(),
                        [&](const std::string& rid) {
                            const auto it = std::find_if(
                                op.scenarios.begin(), op.scenarios.end(),
                                [&](const RouteScenarioProfile& s) {
                                    return s.scenario_id == rid;
                                });
                            return it != op.scenarios.end() &&
                                   (it->routing_trusted ||
                                    it->scenario_qualified);
                        });
        // 场景内一致性：routing_trusted 与 scenario_qualified 必须一致
        for (const auto& sc : op.scenarios) {
            if (sc.routing_trusted != sc.scenario_qualified) {
                error = "routing_trusted != scenario_qualified in " +
                        sc.scenario_id;
                return false;
            }
        }
        if (op.qualified != op_qualified) {
            error = "operation.qualified inconsistent with scenario "
                    "qualification";
            return false;
        }
    }
    // 权威发布校验（05_PROFILE_PUBLICATION.md）：
    // qualified 状态必须由 preset=standard 标定产生，且带 head/run_id。
    // partial/diagnostic 允许（quick smoke / 诊断 Profile 不发布）。
    if (profile.profile_state == "qualified") {
        if (profile.calibration_preset != "standard") {
            error = "qualified profile requires calibration_preset=standard";
            return false;
        }
        if (profile.calibration_head.empty()) {
            error = "qualified profile requires calibration_head";
            return false;
        }
        if (profile.calibration_run_id.empty()) {
            error = "qualified profile requires calibration_run_id";
            return false;
        }
    }
    error.clear();
    return true;
}

} // namespace astro::compute::routing
