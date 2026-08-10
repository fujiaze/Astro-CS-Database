// lib/acr/qualification/focused/operation_profile.cpp — OperationProfile 序列化/校验
//
// 聚焦版 v2（08 号计划 §1/§7）：
//   - 使用 nlohmann/json 做可靠对象层级解析（禁止字符串搜索同名键）；
//   - 完整 roundtrip：CPU/GPU 曲线、transfer、memory、eligibility、nullable 阈值、
//     GPU 数组与指纹逐字段一致；
//   - 指纹来自实际运行环境（编译器宏 + 内核函数地址 hash）。
#include "operation_profile.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace astro::compute::qualification::focused {

namespace {

// 设备曲线 → JSON 对象
nlohmann::json device_curve_json(const OperationProfile::DeviceCurve& c) {
    nlohmann::json j;
    j["fixed_us"] = c.fixed_us;
    j["ns_per_item"] = c.ns_per_item;
    j["recommended_chunk_items"] = c.recommended_chunk_items;
    j["minimum_chunk_items"] = c.minimum_chunk_items;
    j["median_error_ratio"] = c.median_error_ratio;
    j["p95_error_ratio"] = c.p95_error_ratio;
    return j;
}

// 从 JSON 对象读取设备曲线（层级访问，避免同名键串读）
OperationProfile::DeviceCurve device_curve_from_json(const nlohmann::json& j) {
    OperationProfile::DeviceCurve c;
    if (j.contains("fixed_us")) c.fixed_us = j["fixed_us"].get<double>();
    if (j.contains("ns_per_item")) c.ns_per_item = j["ns_per_item"].get<double>();
    if (j.contains("recommended_chunk_items")) {
        c.recommended_chunk_items =
            j["recommended_chunk_items"].get<std::size_t>();
    }
    if (j.contains("minimum_chunk_items")) {
        c.minimum_chunk_items = j["minimum_chunk_items"].get<std::size_t>();
    }
    if (j.contains("median_error_ratio")) {
        c.median_error_ratio = j["median_error_ratio"].get<double>();
    }
    if (j.contains("p95_error_ratio")) {
        c.p95_error_ratio = j["p95_error_ratio"].get<double>();
    }
    return c;
}

// 可选阈值：null → std::nullopt
std::optional<std::size_t> opt_size_from_json(const nlohmann::json& j,
                                              const char* key) {
    if (!j.contains(key) || j[key].is_null()) return std::nullopt;
    return j[key].get<std::size_t>();
}

} // anonymous namespace

std::string serialize_operation_profile(const OperationProfile& p) {
    nlohmann::json root;
    root["schema_version"] = p.schema_version;
    root["profile_state"] = p.profile_state;
    root["fingerprint"]["cpu"] = p.fingerprint_cpu;
    root["fingerprint"]["gpus"] = p.fingerprint_gpus;
    root["fingerprint"]["compiler"] = p.fingerprint_compiler;
    root["fingerprint"]["runtime_kernel_hash"] =
        p.fingerprint_runtime_kernel_hash;
    for (const auto& op : p.operations) {
        nlohmann::json o;
        o["operation_id"] = op.operation_id;
        o["precision"] = op.precision;
        o["accumulator"] = op.accumulator;
        o["qualified"] = op.qualified;
        o["qualification_reason"] = op.qualification_reason;
        o["sample_range"]["min_items"] = op.sample_range.min_items;
        o["sample_range"]["max_items"] = op.sample_range.max_items;
        o["sample_range"]["repeats"] = op.sample_range.repeats;
        o["cpu"] = device_curve_json(op.cpu);
        o["gpu"] = device_curve_json(op.gpu);
        o["gpu"]["device_id"] = op.gpu.device_id;
        o["gpu"]["launch_us"] = op.gpu.launch_us;
        o["gpu"]["min_profitable_items_host"] =
            op.gpu.min_profitable_items_host.has_value()
                ? nlohmann::json(op.gpu.min_profitable_items_host.value())
                : nlohmann::json(nullptr);
        o["gpu"]["min_profitable_items_resident"] =
            op.gpu.min_profitable_items_resident.has_value()
                ? nlohmann::json(op.gpu.min_profitable_items_resident.value())
                : nlohmann::json(nullptr);
        o["gpu"]["host_path_eligible"] = op.gpu.host_path_eligible;
        o["gpu"]["resident_path_eligible"] = op.gpu.resident_path_eligible;
        o["transfer"]["h2d_fixed_us"] = op.transfer.h2d_fixed_us;
        o["transfer"]["h2d_gbps"] = op.transfer.h2d_gbps;
        o["transfer"]["d2h_fixed_us"] = op.transfer.d2h_fixed_us;
        o["transfer"]["d2h_gbps"] = op.transfer.d2h_gbps;
        o["memory"]["host_bytes_per_item"] = op.memory.host_bytes_per_item;
        o["memory"]["device_bytes_per_item"] =
            op.memory.device_bytes_per_item;
        o["memory"]["fixed_host_bytes"] = op.memory.fixed_host_bytes;
        o["memory"]["fixed_device_bytes"] = op.memory.fixed_device_bytes;
        root["operations"].push_back(std::move(o));
    }
    return root.dump();
}

bool write_operation_profile_to_file(const std::string& path,
                                     const OperationProfile& profile) {
    std::FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;
    std::string s = serialize_operation_profile(profile);
    std::fwrite(s.data(), 1, s.size(), f);
    std::fputc('\n', f);
    std::fclose(f);
    return true;
}

bool read_operation_profile_from_file(const std::string& path,
                                      OperationProfile& out) {
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

    OperationProfile p;
    if (root.contains("schema_version")) {
        p.schema_version = root["schema_version"].get<std::string>();
    }
    if (root.contains("profile_state")) {
        p.profile_state = root["profile_state"].get<std::string>();
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
        OperationProfile::Operation op;
        op.operation_id = o.at("operation_id").get<std::string>();
        if (o.contains("precision")) op.precision = o["precision"].get<std::string>();
        if (o.contains("accumulator")) op.accumulator = o["accumulator"].get<std::string>();
        if (o.contains("qualified")) op.qualified = o["qualified"].get<bool>();
        if (o.contains("qualification_reason")) {
            op.qualification_reason =
                o["qualification_reason"].get<std::string>();
        }
        if (o.contains("sample_range")) {
            const auto& sr = o["sample_range"];
            if (sr.contains("min_items")) op.sample_range.min_items = sr["min_items"].get<std::size_t>();
            if (sr.contains("max_items")) op.sample_range.max_items = sr["max_items"].get<std::size_t>();
            if (sr.contains("repeats")) op.sample_range.repeats = sr["repeats"].get<std::size_t>();
        }
        if (o.contains("cpu")) op.cpu = device_curve_from_json(o["cpu"]);
        if (o.contains("gpu")) {
            const auto& g = o["gpu"];
            const OperationProfile::DeviceCurve base =
                device_curve_from_json(g);
            // 基类字段逐项复制（GpuCurve 继承 DeviceCurve，不能直接切片赋值）
            op.gpu.fixed_us = base.fixed_us;
            op.gpu.ns_per_item = base.ns_per_item;
            op.gpu.recommended_chunk_items = base.recommended_chunk_items;
            op.gpu.minimum_chunk_items = base.minimum_chunk_items;
            op.gpu.median_error_ratio = base.median_error_ratio;
            op.gpu.p95_error_ratio = base.p95_error_ratio;
            if (g.contains("device_id")) op.gpu.device_id = g["device_id"].get<std::string>();
            if (g.contains("launch_us")) op.gpu.launch_us = g["launch_us"].get<double>();
            op.gpu.min_profitable_items_host =
                opt_size_from_json(g, "min_profitable_items_host");
            op.gpu.min_profitable_items_resident =
                opt_size_from_json(g, "min_profitable_items_resident");
            if (g.contains("host_path_eligible")) {
                op.gpu.host_path_eligible = g["host_path_eligible"].get<bool>();
            }
            if (g.contains("resident_path_eligible")) {
                op.gpu.resident_path_eligible =
                    g["resident_path_eligible"].get<bool>();
            }
        }
        if (o.contains("transfer")) {
            const auto& t = o["transfer"];
            if (t.contains("h2d_fixed_us")) op.transfer.h2d_fixed_us = t["h2d_fixed_us"].get<double>();
            if (t.contains("h2d_gbps")) op.transfer.h2d_gbps = t["h2d_gbps"].get<double>();
            if (t.contains("d2h_fixed_us")) op.transfer.d2h_fixed_us = t["d2h_fixed_us"].get<double>();
            if (t.contains("d2h_gbps")) op.transfer.d2h_gbps = t["d2h_gbps"].get<double>();
        }
        if (o.contains("memory")) {
            const auto& m = o["memory"];
            if (m.contains("host_bytes_per_item")) op.memory.host_bytes_per_item = m["host_bytes_per_item"].get<double>();
            if (m.contains("device_bytes_per_item")) op.memory.device_bytes_per_item = m["device_bytes_per_item"].get<double>();
            if (m.contains("fixed_host_bytes")) op.memory.fixed_host_bytes = m["fixed_host_bytes"].get<std::size_t>();
            if (m.contains("fixed_device_bytes")) op.memory.fixed_device_bytes = m["fixed_device_bytes"].get<std::size_t>();
        }
        p.operations.push_back(std::move(op));
    }
    out = std::move(p);
    return !out.operations.empty();
}

bool validate_operation_profile(const OperationProfile& p,
                                std::string& error) {
    if (p.schema_version != "acr-operation-profile-1") {
        error = "schema_version must be acr-operation-profile-1";
        return false;
    }
    if (p.profile_state != "diagnostic" && p.profile_state != "qualified" &&
        p.profile_state != "stale" && p.profile_state != "partial") {
        error = "invalid profile_state";
        return false;
    }
    if (p.fingerprint_cpu.empty() || p.fingerprint_compiler.empty() ||
        p.fingerprint_runtime_kernel_hash.size() < 16) {
        error = "fingerprint incomplete (cpu/compiler/runtime_kernel_hash)";
        return false;
    }
    if (p.operations.empty()) {
        error = "operations empty";
        return false;
    }
    for (const auto& op : p.operations) {
        if (op.operation_id.empty()) { error = "operation_id empty"; return false; }
        if (op.precision != "fp32" && op.precision != "fp64") {
            error = "invalid precision: " + op.operation_id;
            return false;
        }
        if (op.accumulator != "fp32" && op.accumulator != "fp64" &&
            op.accumulator != "none") {
            error = "invalid accumulator: " + op.operation_id;
            return false;
        }
        if (op.sample_range.repeats < 3) {
            error = "sample_range.repeats < 3: " + op.operation_id;
            return false;
        }
        if (op.qualified && op.qualification_reason.empty()) {
            error = "qualified but no qualification_reason: " + op.operation_id;
            return false;
        }
        if (op.cpu.ns_per_item <= 0.0) {
            error = "cpu.ns_per_item <= 0: " + op.operation_id;
            return false;
        }
        if (op.cpu.recommended_chunk_items == 0 ||
            op.cpu.minimum_chunk_items == 0) {
            error = "cpu chunk size missing: " + op.operation_id;
            return false;
        }
        if (op.gpu.ns_per_item <= 0.0) {
            error = "gpu.ns_per_item <= 0: " + op.operation_id;
            return false;
        }
        if (op.gpu.recommended_chunk_items == 0 ||
            op.gpu.minimum_chunk_items == 0) {
            error = "gpu chunk size missing: " + op.operation_id;
            return false;
        }
        if (op.transfer.h2d_gbps <= 0.0 || op.transfer.d2h_gbps <= 0.0) {
            error = "transfer bandwidth missing: " + op.operation_id;
            return false;
        }
        // eligibility 一致性：eligible 路径必须有有限收益阈值；反之必须 null
        if (op.gpu.host_path_eligible &&
            !op.gpu.min_profitable_items_host.has_value()) {
            error = "host eligible but threshold null: " + op.operation_id;
            return false;
        }
        if (op.gpu.resident_path_eligible &&
            !op.gpu.min_profitable_items_resident.has_value()) {
            error = "resident eligible but threshold null: " + op.operation_id;
            return false;
        }
        if (!op.gpu.host_path_eligible &&
            op.gpu.min_profitable_items_host.has_value()) {
            error = "host ineligible but threshold set: " + op.operation_id;
            return false;
        }
        if (!op.gpu.resident_path_eligible &&
            op.gpu.min_profitable_items_resident.has_value()) {
            error = "resident ineligible but threshold set: " + op.operation_id;
            return false;
        }
    }
    error.clear();
    return true;
}

} // namespace astro::compute::qualification::focused
