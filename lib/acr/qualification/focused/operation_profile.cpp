// lib/acr/qualification/focused/operation_profile.cpp — OperationProfile 序列化/校验
#include "operation_profile.hpp"

#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace astro::compute::qualification::focused {

namespace {

std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out += c; break;
        }
    }
    return out;
}

void write_device_curve(std::ostringstream& os, const char* key,
                        const OperationProfile::DeviceCurve& c) {
    os << "\"" << key << "\":{"
       << "\"fixed_us\":" << c.fixed_us
       << ",\"ns_per_item\":" << c.ns_per_item
       << ",\"recommended_chunk_items\":" << c.recommended_chunk_items
       << ",\"minimum_chunk_items\":" << c.minimum_chunk_items
       << ",\"median_error_ratio\":" << c.median_error_ratio
       << ",\"p95_error_ratio\":" << c.p95_error_ratio
       << "}";
}

} // anonymous namespace

std::string serialize_operation_profile(const OperationProfile& p) {
    std::ostringstream os;
    os.precision(15);  // double 精度序列化（避免 roundtrip 误差）
    os << "{";
    os << "\"schema_version\":\"" << json_escape(p.schema_version) << "\"";
    os << ",\"profile_state\":\"" << json_escape(p.profile_state) << "\"";
    os << ",\"fingerprint\":{";
    os << "\"cpu\":\"" << json_escape(p.fingerprint_cpu) << "\"";
    os << ",\"gpus\":[";
    for (std::size_t i = 0; i < p.fingerprint_gpus.size(); ++i) {
        if (i > 0) os << ",";
        os << "\"" << json_escape(p.fingerprint_gpus[i]) << "\"";
    }
    os << "]";
    os << ",\"compiler\":\"" << json_escape(p.fingerprint_compiler) << "\"";
    os << ",\"runtime_kernel_hash\":\""
       << json_escape(p.fingerprint_runtime_kernel_hash) << "\"";
    os << "}";
    os << ",\"operations\":[";
    for (std::size_t i = 0; i < p.operations.size(); ++i) {
        const auto& op = p.operations[i];
        if (i > 0) os << ",";
        os << "{";
        os << "\"operation_id\":\"" << json_escape(op.operation_id) << "\"";
        os << ",\"precision\":\"" << json_escape(op.precision) << "\"";
        os << ",\"accumulator\":\"" << json_escape(op.accumulator) << "\"";
        os << ",\"qualified\":" << (op.qualified ? "true" : "false");
        os << ",\"sample_range\":{"
           << "\"min_items\":" << op.sample_range.min_items
           << ",\"max_items\":" << op.sample_range.max_items
           << ",\"repeats\":" << op.sample_range.repeats << "},";
        write_device_curve(os, "cpu", op.cpu);
        os << ",\"gpu\":{";
        os << "\"fixed_us\":" << op.gpu.fixed_us
           << ",\"ns_per_item\":" << op.gpu.ns_per_item
           << ",\"recommended_chunk_items\":" << op.gpu.recommended_chunk_items
           << ",\"minimum_chunk_items\":" << op.gpu.minimum_chunk_items
           << ",\"median_error_ratio\":" << op.gpu.median_error_ratio
           << ",\"p95_error_ratio\":" << op.gpu.p95_error_ratio
           << ",\"device_id\":\"" << json_escape(op.gpu.device_id) << "\""
           << ",\"launch_us\":" << op.gpu.launch_us
           << ",\"min_profitable_items_host\":" << op.gpu.min_profitable_items_host
           << ",\"min_profitable_items_resident\":"
           << op.gpu.min_profitable_items_resident
           << "}";
        os << ",\"transfer\":{"
           << "\"h2d_fixed_us\":" << op.transfer.h2d_fixed_us
           << ",\"h2d_gbps\":" << op.transfer.h2d_gbps
           << ",\"d2h_fixed_us\":" << op.transfer.d2h_fixed_us
           << ",\"d2h_gbps\":" << op.transfer.d2h_gbps
           << "}";
        os << ",\"memory\":{"
           << "\"host_bytes_per_item\":" << op.memory.host_bytes_per_item
           << ",\"device_bytes_per_item\":" << op.memory.device_bytes_per_item
           << ",\"fixed_host_bytes\":" << op.memory.fixed_host_bytes
           << ",\"fixed_device_bytes\":" << op.memory.fixed_device_bytes
           << "}";
        os << "}";
    }
    os << "]}";
    return os.str();
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

    // 轻量字段提取（本实现只读取本项目生成的 JSON）
    auto find_str = [&](const std::string& key) -> std::string {
        std::string pat = "\"" + key + "\":\"";
        std::size_t p = s.find(pat);
        if (p == std::string::npos) return "";
        p += pat.size();
        std::size_t q = s.find('"', p);
        if (q == std::string::npos) return "";
        return s.substr(p, q - p);
    };
    auto find_num = [&](const std::string& key) -> double {
        std::string pat = "\"" + key + "\":";
        std::size_t p = s.find(pat);
        if (p == std::string::npos) return 0.0;
        p += pat.size();
        return std::strtod(s.c_str() + p, nullptr);
    };
    auto find_int = [&](const std::string& key) -> std::size_t {
        return static_cast<std::size_t>(find_num(key));
    };
    auto find_bool = [&](const std::string& key) -> bool {
        std::string pat = "\"" + key + "\":";
        std::size_t p = s.find(pat);
        if (p == std::string::npos) return false;
        return s.compare(p + pat.size(), 4, "true") == 0;
    };

    out = OperationProfile{};
    out.schema_version = find_str("schema_version");
    out.profile_state = find_str("profile_state");
    out.fingerprint_cpu = find_str("cpu");
    out.fingerprint_compiler = find_str("compiler");
    out.fingerprint_runtime_kernel_hash = find_str("runtime_kernel_hash");

    // 逐 operation 提取（项目生成格式：每个 operation 一个连续 JSON 对象）
    std::string op_pat = "\"operation_id\":\"";
    std::size_t pos = 0;
    while ((pos = s.find(op_pat, pos)) != std::string::npos) {
        OperationProfile::Operation op;
        std::size_t id_start = pos + op_pat.size();
        std::size_t id_end = s.find('"', id_start);
        if (id_end == std::string::npos) break;
        op.operation_id = s.substr(id_start, id_end - id_start);
        // 该 operation 的局部搜索区间 [pos, 下一个 operation_id 或结尾)
        std::size_t next = s.find(op_pat, id_end);
        std::string seg = (next == std::string::npos)
            ? s.substr(pos) : s.substr(pos, next - pos);
        auto seg_find_str = [&](const std::string& key) -> std::string {
            std::string pat = "\"" + key + "\":\"";
            std::size_t p = seg.find(pat);
            if (p == std::string::npos) return "";
            p += pat.size();
            std::size_t q = seg.find('"', p);
            return (q == std::string::npos) ? "" : seg.substr(p, q - p);
        };
        auto seg_find_num = [&](const std::string& key) -> double {
            std::string pat = "\"" + key + "\":";
            std::size_t p = seg.find(pat);
            if (p == std::string::npos) return 0.0;
            p += pat.size();
            return std::strtod(seg.c_str() + p, nullptr);
        };
        auto seg_find_int = [&](const std::string& key) -> std::size_t {
            return static_cast<std::size_t>(seg_find_num(key));
        };
        auto seg_find_bool = [&](const std::string& key) -> bool {
            std::string pat = "\"" + key + "\":";
            std::size_t p = seg.find(pat);
            if (p == std::string::npos) return false;
            return seg.compare(p + pat.size(), 4, "true") == 0;
        };
        op.precision = seg_find_str("precision");
        op.accumulator = seg_find_str("accumulator");
        op.qualified = seg_find_bool("qualified");
        op.sample_range.min_items = seg_find_int("min_items");
        op.sample_range.max_items = seg_find_int("max_items");
        op.sample_range.repeats = seg_find_int("repeats");
        op.cpu.fixed_us = seg_find_num("fixed_us");
        op.cpu.ns_per_item = seg_find_num("ns_per_item");
        op.cpu.recommended_chunk_items = seg_find_int("recommended_chunk_items");
        op.cpu.minimum_chunk_items = seg_find_int("minimum_chunk_items");
        op.cpu.median_error_ratio = seg_find_num("median_error_ratio");
        op.cpu.p95_error_ratio = seg_find_num("p95_error_ratio");
        op.gpu.fixed_us = seg_find_num("fixed_us");
        op.gpu.ns_per_item = seg_find_num("ns_per_item");
        op.gpu.recommended_chunk_items = seg_find_int("recommended_chunk_items");
        op.gpu.minimum_chunk_items = seg_find_int("minimum_chunk_items");
        op.gpu.median_error_ratio = seg_find_num("median_error_ratio");
        op.gpu.p95_error_ratio = seg_find_num("p95_error_ratio");
        op.gpu.device_id = seg_find_str("device_id");
        op.gpu.launch_us = seg_find_num("launch_us");
        op.gpu.min_profitable_items_host =
            seg_find_int("min_profitable_items_host");
        op.gpu.min_profitable_items_resident =
            seg_find_int("min_profitable_items_resident");
        op.transfer.h2d_fixed_us = seg_find_num("h2d_fixed_us");
        op.transfer.h2d_gbps = seg_find_num("h2d_gbps");
        op.transfer.d2h_fixed_us = seg_find_num("d2h_fixed_us");
        op.transfer.d2h_gbps = seg_find_num("d2h_gbps");
        op.memory.host_bytes_per_item = seg_find_num("host_bytes_per_item");
        op.memory.device_bytes_per_item = seg_find_num("device_bytes_per_item");
        op.memory.fixed_host_bytes = seg_find_int("fixed_host_bytes");
        op.memory.fixed_device_bytes = seg_find_int("fixed_device_bytes");
        out.operations.push_back(std::move(op));
        pos = id_end;
    }
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
    }
    error.clear();
    return true;
}

} // namespace astro::compute::qualification::focused
