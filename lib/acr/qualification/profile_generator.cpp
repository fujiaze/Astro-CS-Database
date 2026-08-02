// lib/acr/qualification/profile_generator.cpp — Profile 生成实现
// Phase E：聚合 + 选路 + JSON 序列化 + SHA-256 指纹。
#include "profile_generator.hpp"
#include "benchmark_driver.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

#include "astro/compute/topology.hpp"

namespace astro::compute::qualification {

namespace {

// ===== SHA-256 实现（FIPS 180-4，纯 C++，无外部依赖）=====
// 用于设备指纹哈希；为简洁用一次性实现，不追求性能。
struct SHA256 {
    std::uint32_t h[8];
    std::uint64_t total_len{0};
    std::uint8_t buf[64];
    std::size_t buf_len{0};

    SHA256() {
        h[0]=0x6a09e667; h[1]=0xbb67ae85; h[2]=0x3c6ef372; h[3]=0xa54ff53a;
        h[4]=0x510e527f; h[5]=0x9b05688c; h[6]=0x1f83d9ab; h[7]=0x5be0cd19;
    }

    static std::uint32_t rotr(std::uint32_t x, int n) {
        return (x >> n) | (x << (32 - n));
    }

    void process_block(const std::uint8_t* p) {
        static const std::uint32_t K[64] = {
            0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
            0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
            0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
            0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
            0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
            0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
            0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
            0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
        };
        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<std::uint32_t>(p[i*4]) << 24) |
                   (static_cast<std::uint32_t>(p[i*4+1]) << 16) |
                   (static_cast<std::uint32_t>(p[i*4+2]) << 8) |
                   (static_cast<std::uint32_t>(p[i*4+3]));
        }
        for (int i = 16; i < 64; ++i) {
            std::uint32_t s0 = rotr(w[i-15], 7) ^ rotr(w[i-15], 18) ^ (w[i-15] >> 3);
            std::uint32_t s1 = rotr(w[i-2], 17) ^ rotr(w[i-2], 19) ^ (w[i-2] >> 10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }
        std::uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for (int i = 0; i < 64; ++i) {
            std::uint32_t S1 = rotr(e,6) ^ rotr(e,11) ^ rotr(e,25);
            std::uint32_t ch = (e & f) ^ ((~e) & g);
            std::uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            std::uint32_t S0 = rotr(a,2) ^ rotr(a,13) ^ rotr(a,22);
            std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            std::uint32_t t2 = S0 + maj;
            hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d;
        h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }

    void update(const std::uint8_t* data, std::size_t len) {
        total_len += len;
        while (len > 0) {
            std::size_t copy = 64 - buf_len;
            if (copy > len) copy = len;
            std::memcpy(buf + buf_len, data, copy);
            buf_len += copy;
            data += copy;
            len -= copy;
            if (buf_len == 64) {
                process_block(buf);
                buf_len = 0;
            }
        }
    }

    void update(const std::string& s) {
        update(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
    }

    std::string final_hex() {
        std::uint64_t bitlen = total_len * 8;
        std::uint8_t pad = 0x80;
        update(&pad, 1);
        std::uint8_t zero = 0;
        while (buf_len != 56) update(&zero, 1);
        std::uint8_t lenbuf[8];
        for (int i = 0; i < 8; ++i) {
            lenbuf[i] = static_cast<std::uint8_t>((bitlen >> (56 - i*8)) & 0xFF);
        }
        update(lenbuf, 8);
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (int i = 0; i < 8; ++i) {
            oss << std::setw(8) << h[i];
        }
        return oss.str();
    }
};

// JSON 字符串转义
std::string esc(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(c);
                }
        }
    }
    return out;
}

// 计算 median（输入会被修改）
template<class T>
T median_inplace(std::vector<T>& v) {
    if (v.empty()) return T{};
    std::sort(v.begin(), v.end());
    std::size_t n = v.size();
    return (n % 2 == 1) ? v[n/2] : (v[n/2 - 1] + v[n/2]) / 2;
}

// 从 hardware_report JSON 字符串粗略抽取字段（避免引入完整 JSON 库）
// 这里用极简字符串匹配（仅用于指纹）。如果 hardware_report 格式变更，此函数需同步更新。
std::string extract_json_field(const std::string& json, const std::string& key) {
    std::string pat = "\"" + key + "\":\"";
    auto pos = json.find(pat);
    if (pos == std::string::npos) return "";
    pos += pat.size();
    auto end = json.find('"', pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
}

std::string extract_json_number(const std::string& json, const std::string& key) {
    std::string pat = "\"" + key + "\":";
    auto pos = json.find(pat);
    if (pos == std::string::npos) return "";
    pos += pat.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    auto end = pos;
    while (end < json.size() &&
           (std::isdigit(static_cast<unsigned char>(json[end])) ||
            json[end] == '.' || json[end] == '-' || json[end] == '+')) {
        ++end;
    }
    return json.substr(pos, end - pos);
}

} // anonymous namespace

// ===== 公开 sha256_hex =====
std::string sha256_hex(const std::string& input) {
    SHA256 h;
    h.update(input);
    return h.final_hex();
}

// ===== DeviceFingerprint::to_json =====
std::string DeviceFingerprint::to_json() const {
    std::ostringstream os;
    os << "{";
    os << "\"cpu_model\":\"" << esc(cpu_model) << "\"";
    os << ",\"cpu_cores\":" << cpu_cores;
    os << ",\"isa_mask\":" << isa_mask;
    os << ",\"gpu_name\":\"" << esc(gpu_name) << "\"";
    os << ",\"gpu_memory_bytes\":" << gpu_memory_bytes;
    os << ",\"gpu_driver_version\":\"" << esc(gpu_driver_version) << "\"";
    os << ",\"sha256\":\"" << sha256 << "\"";
    os << "}";
    return os.str();
}

// ===== ProfileGenerator =====
ProfileGenerator::ProfileGenerator() = default;
ProfileGenerator::~ProfileGenerator() = default;

void ProfileGenerator::aggregate(KernelBenchmarkResult& r) {
    if (r.samples.empty()) return;
    std::vector<std::uint64_t> ks, ts;
    std::vector<double> tps;
    ks.reserve(r.samples.size());
    ts.reserve(r.samples.size());
    tps.reserve(r.samples.size());
    for (const auto& s : r.samples) {
        ks.push_back(s.kernel_ns);
        ts.push_back(s.total_ns);
        tps.push_back(s.throughput_gbps);
    }
    r.median_kernel_ns = median_inplace(ks);
    r.median_total_ns  = median_inplace(ts);
    r.median_throughput_gbps = median_inplace(tps);
    // stddev（基于样本，无偏估计）
    if (r.samples.size() > 1) {
        double mean = 0.0;
        for (auto v : ks) mean += static_cast<double>(v);
        mean /= static_cast<double>(ks.size());
        double var = 0.0;
        for (auto v : ks) {
            double d = static_cast<double>(v) - mean;
            var += d * d;
        }
        var /= static_cast<double>(ks.size() - 1);
        r.stddev_kernel_ns = std::sqrt(var);
    }
}

DeviceFingerprint ProfileGenerator::build_fingerprint() const {
    DeviceFingerprint fp;
    std::string hw = generate_hardware_report();
    // 从 topology JSON 抽取关键字段（容错：缺失时为空/0）
    fp.cpu_model = extract_json_field(hw, "model");
    if (fp.cpu_model.empty()) fp.cpu_model = extract_json_field(hw, "cpu_model");
    std::string cores = extract_json_number(hw, "cores");
    if (cores.empty()) cores = extract_json_number(hw, "cpu_cores");
    if (!cores.empty()) {
        try { fp.cpu_cores = static_cast<std::uint32_t>(std::stoul(cores)); } catch (...) {}
    }
    std::string isa = extract_json_number(hw, "mask");
    if (isa.empty()) isa = extract_json_number(hw, "isa_mask");
    if (!isa.empty()) {
        try { fp.isa_mask = std::stoull(isa); } catch (...) {}
    }
    // GPU 字段（无 GPU 时为空）
    fp.gpu_name = extract_json_field(hw, "gpu_name");
    if (fp.gpu_name.empty()) fp.gpu_name = extract_json_field(hw, "name");
    std::string vmem = extract_json_number(hw, "total_memory");
    if (vmem.empty()) vmem = extract_json_number(hw, "gpu_memory_bytes");
    if (!vmem.empty()) {
        try { fp.gpu_memory_bytes = std::stoull(vmem); } catch (...) {}
    }
    fp.gpu_driver_version = extract_json_field(hw, "driver_version");
    // 计算指纹哈希：关键字段拼接 → SHA-256
    std::ostringstream fp_input;
    fp_input << fp.cpu_model << "|" << fp.cpu_cores << "|" << fp.isa_mask
             << "|" << fp.gpu_name << "|" << fp.gpu_memory_bytes
             << "|" << fp.gpu_driver_version;
    fp.sha256 = sha256_hex(fp_input.str());
    return fp;
}

ProfileBundle ProfileGenerator::generate(
    const std::vector<KernelBenchmarkResult>& results, ProfileKind kind) const {
    ProfileBundle bundle;
    bundle.profile_kind = kind;
    bundle.fingerprint = build_fingerprint();
    // 时间戳（ISO 8601 简化：YYYYMMDDTHHMMSSZ）
    {
        std::time_t now = std::time(nullptr);
        std::tm* tm = std::gmtime(&now);
        char buf[32];
        if (tm) {
            std::strftime(buf, sizeof(buf), "%Y%m%dT%H%M%SZ", tm);
            bundle.generated_at = buf;
        }
    }
    // 复制 + 聚合
    bundle.raw_results = results;
    for (auto& r : bundle.raw_results) {
        aggregate(r);
    }
    // 路由选择：每个 (kernel_id, precision) 选 throughput 最大的 backend
    // 按 kernel_id 分组
    std::vector<std::uint32_t> seen_kernels;
    for (const auto& r : bundle.raw_results) {
        if (std::find(seen_kernels.begin(), seen_kernels.end(), r.kernel_id)
            == seen_kernels.end()) {
            seen_kernels.push_back(r.kernel_id);
        }
    }
    for (std::uint32_t kid : seen_kernels) {
        // 在该 kernel 的所有 backend × size 中，选最大 throughput 的 backend
        const KernelBenchmarkResult* best = nullptr;
        double best_tp = -1.0;
        std::string best_name;
        for (const auto& r : bundle.raw_results) {
            if (r.kernel_id != kid) continue;
            // 取最大 problem_size 的 median throughput 作为代表
            if (r.median_throughput_gbps > best_tp) {
                best_tp = r.median_throughput_gbps;
                best = &r;
                best_name = r.kernel_name;
            }
        }
        if (best != nullptr) {
            RouteEntry e;
            e.kernel_id = kid;
            e.kernel_name = best_name;
            e.precision = best->precision;
            e.preferred_backend = best->backend;
            e.expected_throughput_gbps = best->median_throughput_gbps;
            e.reason = (best->backend == "cpu") ? "only-avail" : "fastest";
            bundle.routes.push_back(std::move(e));
        } else {
            // 无 benchmark 结果，回退 CPU baseline
            RouteEntry e;
            e.kernel_id = kid;
            e.kernel_name = "unknown";
            e.precision = "fp32";
            e.preferred_backend = "cpu";
            e.expected_throughput_gbps = 0.0;
            e.reason = "cpu-baseline";
            bundle.routes.push_back(std::move(e));
        }
    }
    return bundle;
}

// ===== ProfileBundle::to_json =====
std::string ProfileBundle::to_json() const {
    return ProfileGenerator::serialize(*this);
}

// ===== ProfileGenerator::serialize =====
std::string ProfileGenerator::serialize(const ProfileBundle& bundle) {
    std::ostringstream os;
    os << "{";
    os << "\"schema_version\":\"" << esc(bundle.schema_version) << "\"";
    os << ",\"generated_at\":\"" << esc(bundle.generated_at) << "\"";
    os << ",\"profile_kind\":\"" << profile_kind_str(bundle.profile_kind) << "\"";
    os << ",\"fingerprint\":" << bundle.fingerprint.to_json();
    os << ",\"routes\":[";
    for (std::size_t i = 0; i < bundle.routes.size(); ++i) {
        const auto& r = bundle.routes[i];
        if (i > 0) os << ",";
        os << "{";
        os << "\"kernel_id\":" << r.kernel_id;
        os << ",\"kernel_name\":\"" << esc(r.kernel_name) << "\"";
        os << ",\"precision\":\"" << esc(r.precision) << "\"";
        os << ",\"preferred_backend\":\"" << esc(r.preferred_backend) << "\"";
        os << ",\"expected_throughput_gbps\":" << r.expected_throughput_gbps;
        os << ",\"reason\":\"" << esc(r.reason) << "\"";
        os << "}";
    }
    os << "]";
    // raw_results 仅 Full profile 输出（避免 routes.json 过大）
    if (bundle.profile_kind == ProfileKind::Full && !bundle.raw_results.empty()) {
        os << ",\"raw_results\":[";
        for (std::size_t i = 0; i < bundle.raw_results.size(); ++i) {
            const auto& r = bundle.raw_results[i];
            if (i > 0) os << ",";
            os << "{";
            os << "\"kernel_id\":" << r.kernel_id;
            os << ",\"kernel_name\":\"" << esc(r.kernel_name) << "\"";
            os << ",\"backend\":\"" << esc(r.backend) << "\"";
            os << ",\"precision\":\"" << esc(r.precision) << "\"";
            os << ",\"problem_size\":" << r.problem_size;
            os << ",\"bytes_per_element\":" << r.bytes_per_element;
            os << ",\"samples\":[";
            for (std::size_t j = 0; j < r.samples.size(); ++j) {
                const auto& s = r.samples[j];
                if (j > 0) os << ",";
                os << "{\"kernel_ns\":" << s.kernel_ns
                   << ",\"transfer_ns\":" << s.transfer_ns
                   << ",\"resident_ns\":" << s.resident_ns
                   << ",\"total_ns\":" << s.total_ns
                   << ",\"throughput_gbps\":" << s.throughput_gbps
                   << "}";
            }
            os << "]";
            os << ",\"median_kernel_ns\":" << r.median_kernel_ns;
            os << ",\"median_total_ns\":" << r.median_total_ns;
            os << ",\"median_throughput_gbps\":" << r.median_throughput_gbps;
            os << ",\"stddev_kernel_ns\":" << r.stddev_kernel_ns;
            os << "}";
        }
        os << "]";
    }
    os << "}";
    return os.str();
}

bool ProfileGenerator::write_to_file(const std::string& path,
                                      const ProfileBundle& bundle) {
    std::ofstream f(path, std::ios::out | std::ios::trunc);
    if (!f.is_open()) return false;
    f << serialize(bundle);
    return f.good();
}

} // namespace astro::compute::qualification
