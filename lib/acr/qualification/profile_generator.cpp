// lib/acr/qualification/profile_generator.cpp — Profile 生成实现
// Phase E：聚合 + 选路 + JSON 序列化 + SHA-256 指纹。
// Phase E3：hardware-profile.json 生成（多维能力曲线）。
#include "profile_generator.hpp"
#include "benchmark_driver.hpp"

#include "../backends/cuda/bridge/cuda_bridge_api.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <thread>
#include <vector>

#include "astro/compute/topology.hpp"

namespace astro::compute::qualification {

namespace {

// GPU 名称：优先 bridge 真实设备名（主 MinGW 构建无 CudaBackend 回调，
// hardware_report 的 gpu 字段为 null，不能 fallback 到 compiler 的 "name"）
std::string detect_gpu_name() {
    cuda::bridge::ensure_bridge_loaded();
    auto& api = cuda::bridge::api();
    if (!api.loaded()) return "";
    const char* err = nullptr;
    if (api.init(&err) <= 0) return "";
    const char* n = api.device_name(0);
    return n ? std::string(n) : "";
}

} // anonymous namespace

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
    if (fp.gpu_name.empty()) fp.gpu_name = detect_gpu_name();
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

// ============================================================================
// Phase E3：hardware-profile.json 生成实现
// ============================================================================
// 将 benchmark 结果映射到 HardwareProfile 的多维能力曲线。
// KernelId → 能力曲线族映射：
//   Copy    (id=1) → memory[MainMem:host:copy]
//   Triad   (id=2) → memory[MainMem:host:triad]
//   AXPY    (id=3) → arithmetic[fp32:add:baseline]
//   Dot     (id=4) → reduction[dot:fp32]
//   其他           → arithmetic[<precision>:add:baseline]（兜底）

namespace {

// CurvePoint 序列化
void serialize_curve_points(std::ostringstream& os, const Curve& curve) {
    os << "[";
    for (std::size_t i = 0; i < curve.points.size(); ++i) {
        const auto& p = curve.points[i];
        if (i > 0) os << ",";
        os << "{\"size\":" << p.size;
        os << ",\"median\":" << p.median;
        os << ",\"p95\":" << p.p95;
        os << ",\"mad\":" << p.mad;
        os << ",\"sample_count\":" << p.sample_count;
        os << ",\"confidence\":" << p.confidence;
        os << "}";
    }
    os << "]";
}

// 曲线对象序列化（含来源与资格标记，25 号计划 §3.3）
void serialize_curve(std::ostringstream& os, const Curve& curve) {
    os << "{\"source\":\"" << esc(curve.source) << "\"";
    os << ",\"qualified\":" << (curve.qualified ? "true" : "false");
    os << ",\"points\":";
    serialize_curve_points(os, curve);
    os << "}";
}

// FixedOverhead 序列化
void serialize_overhead(std::ostringstream& os, const std::string& name,
                        const FixedOverhead& oh) {
    os << "\"" << name << "\":{";
    os << "\"median_ns\":" << oh.median_ns;
    os << ",\"p95_ns\":" << oh.p95_ns;
    os << ",\"cold_start_ns\":" << oh.cold_start_ns;
    os << ",\"warm_ns\":" << oh.warm_ns;
    os << ",\"source\":\"" << esc(oh.source) << "\"";
    os << "}";
}

// 判断 backend 是否为 GPU
bool is_gpu_backend(const std::string& backend) {
    return backend.rfind("cuda", 0) == 0;
}

// backend → device_id（CPU=0；cuda:N → N+1）
DeviceId backend_to_device_id_local(const std::string& backend) {
    if (backend == "cpu" || backend.empty()) return kHwCpuDeviceId;
    if (backend.rfind("cuda:", 0) == 0) {
        try {
            int idx = std::stoi(backend.substr(5));
            return static_cast<DeviceId>(idx + 1);
        } catch (...) { return kHwInvalidDeviceId; }
    }
    return kHwInvalidDeviceId;
}

// 精度字符串 → HwPrecision
HwPrecision parse_hw_precision(const std::string& s) {
    return (s == "fp64") ? HwPrecision::Fp64 : HwPrecision::Fp32;
}

} // anonymous namespace

std::vector<DeviceProfile> ProfileGenerator::build_device_profiles(
    const std::vector<KernelBenchmarkResult>& results,
    ProfileKind kind) const {
    // 按 backend 分组
    std::map<std::string, std::vector<const KernelBenchmarkResult*>> by_backend;
    for (const auto& r : results) {
        by_backend[r.backend].push_back(&r);
    }

    // 收集 hardware_report 提取容量信息
    std::string hw = generate_hardware_report();
    std::string cpu_model = extract_json_field(hw, "model");
    if (cpu_model.empty()) cpu_model = extract_json_field(hw, "cpu_model");
    std::string cores_str = extract_json_number(hw, "cores");
    if (cores_str.empty()) cores_str = extract_json_number(hw, "cpu_cores");
    std::uint32_t cpu_cores = 0;
    if (!cores_str.empty()) {
        try { cpu_cores = static_cast<std::uint32_t>(std::stoul(cores_str)); } catch (...) {}
    }
    if (cpu_cores == 0) cpu_cores = static_cast<std::uint32_t>(std::thread::hardware_concurrency());
    if (cpu_cores == 0) cpu_cores = 4;

    std::string gpu_name = extract_json_field(hw, "gpu_name");
    if (gpu_name.empty()) gpu_name = detect_gpu_name();
    if (gpu_name.empty()) gpu_name = extract_json_field(hw, "name");
    std::string vmem_str = extract_json_number(hw, "total_memory");
    if (vmem_str.empty()) vmem_str = extract_json_number(hw, "gpu_memory_bytes");
    std::uint64_t gpu_vmem = 0;
    if (!vmem_str.empty()) {
        try { gpu_vmem = std::stoull(vmem_str); } catch (...) {}
    }

    std::vector<DeviceProfile> devices;
    for (const auto& [backend, results_for_backend] : by_backend) {
        DeviceProfile dev;
        dev.device_id = backend_to_device_id_local(backend);
        if (is_gpu_backend(backend)) {
            dev.kind = DeviceKind::Gpu;
            dev.device_name = gpu_name.empty() ? backend : gpu_name;
            // 25 号计划 §3.4：真实 GPU 元数据（显存/SM/CC，经桥接查询）
            std::uint64_t g_total = 0, g_free = 0;
            int g_sm = 0, g_cc_maj = 0, g_cc_min = 0;
            cuda::bridge::ensure_bridge_loaded();
            auto& bapi = cuda::bridge::api();
            if (bapi.loaded() && bapi.device_memory && bapi.device_compute) {
                const char* berr = nullptr;
                const int dev_idx = static_cast<int>(dev.device_id) - 1;
                if (bapi.device_memory(dev_idx, &g_total, &g_free, &berr) == 0) {
                    dev.total_memory_bytes = static_cast<std::size_t>(g_total);
                    dev.available_memory_bytes = static_cast<std::size_t>(g_free);
                }
                if (bapi.device_compute(dev_idx, &g_sm, &g_cc_maj, &g_cc_min,
                                        &berr) == 0) {
                    dev.compute_units = static_cast<std::uint32_t>(g_sm);
                }
            }
            if (dev.total_memory_bytes == 0) {
                dev.total_memory_bytes = static_cast<std::size_t>(gpu_vmem);
            }
            dev.peak_bandwidth_gbps = 0.0;
        } else {
            dev.kind = DeviceKind::Cpu;
            dev.device_name = cpu_model.empty() ? "CPU" : cpu_model;
            // 25 号计划 §3.4：真实 CPU RAM（GlobalMemoryStatusEx）
            MEMORYSTATUSEX ms{};
            ms.dwLength = sizeof(ms);
            if (GlobalMemoryStatusEx(&ms)) {
                dev.total_memory_bytes = static_cast<std::size_t>(ms.ullTotalPhys);
                dev.available_memory_bytes = static_cast<std::size_t>(ms.ullAvailPhys);
            }
            dev.compute_units = cpu_cores;
            dev.peak_bandwidth_gbps = 0.0;
        }

        // 映射 benchmark 结果到能力曲线
        for (const auto* r : results_for_backend) {
            map_result_to_curves(dev, *r, kind);
        }

        // 填充默认固定开销
        fill_default_overheads(dev);

        devices.push_back(std::move(dev));
    }

    // 如果没有任何 benchmark 结果，至少创建一个 CPU device（fallback）
    if (devices.empty()) {
        DeviceProfile cpu;
        cpu.device_id = kHwCpuDeviceId;
        cpu.device_name = cpu_model.empty() ? "CPU" : cpu_model;
        cpu.kind = DeviceKind::Cpu;
        cpu.compute_units = cpu_cores;
        fill_default_overheads(cpu);
        devices.push_back(std::move(cpu));
    }

    // 按 device_id 排序（CPU 在前，GPU 在后）
    std::sort(devices.begin(), devices.end(),
              [](const DeviceProfile& a, const DeviceProfile& b) {
                  return a.device_id < b.device_id;
              });
    return devices;
}

void ProfileGenerator::map_result_to_curves(
    DeviceProfile& device, const KernelBenchmarkResult& r,
    ProfileKind kind) const {
    // 从聚合结果构造 CurvePoint
    // median_kernel_ns 为该 size 的中位耗时
    CurvePoint pt;
    pt.size = r.problem_size;
    pt.median = static_cast<double>(r.median_kernel_ns);
    // 估算 p95/mad（基于 stddev）
    pt.p95 = static_cast<double>(r.median_kernel_ns) +
             2.0 * r.stddev_kernel_ns;  // 粗略 p95 ≈ median + 2σ
    pt.mad = r.stddev_kernel_ns * 0.6745;  // σ → MAD 转换因子
    // 25 号计划 §3.3：样本数与置信度
    pt.sample_count = static_cast<std::uint32_t>(r.samples.size());
    pt.confidence = (r.median_kernel_ns > 0)
        ? std::max(0.0, 1.0 - r.stddev_kernel_ns /
                             static_cast<double>(r.median_kernel_ns))
        : 0.0;
    // 曲线来源与资格：仅 full 标定且每点样本数>=7 的 measured 曲线合格
    auto mark_measured = [kind](Curve& c, const CurvePoint& p) {
        c.source = "measured";
        const bool pt_ok = (kind == ProfileKind::Full) && (p.sample_count >= 7);
        c.qualified = pt_ok;
        for (const auto& old : c.points) {
            if (old.sample_count < 7) c.qualified = false;
        }
    };

    // 按 kernel_id 映射到能力曲线族
    // KernelId: Custom=0, Copy=1, Triad=2, AXPY=3, Dot=4, Transpose=5,
    //           Convolution2D=6, Histogram256=7, Scan=8, Gather=9, Scatter=10,
    //           Mandelbrot=11, Gemm=12, Fft=13
    HwPrecision prec = parse_hw_precision(r.precision);
    std::uint32_t kid = r.kernel_id;

    // 25 号计划 §4：内存曲线按 (level, residency, operation) 区分；
    // GPU 显存曲线不标 host MainMem
    const MemoryLevel mem_level =
        (device.kind == DeviceKind::Gpu) ? MemoryLevel::Vram : MemoryLevel::MainMem;
    const MemoryResidency mem_res =
        (device.kind == DeviceKind::Gpu) ? MemoryResidency::Device : MemoryResidency::Host;
    if (kid == 1) {  // Copy → memory[level:res:copy]
        auto& c = device.memory[{mem_level, mem_res, "copy"}];
        mark_measured(c, pt); c.points.push_back(pt);
    } else if (kid == 2) {  // Triad → memory[level:res:triad]
        auto& c = device.memory[{mem_level, mem_res, "triad"}];
        mark_measured(c, pt); c.points.push_back(pt);
    } else if (kid == 3) {  // AXPY → arithmetic[fp32:add:baseline]
        auto& c = device.arithmetic[{prec, "add:baseline"}];
        mark_measured(c, pt); c.points.push_back(pt);
    } else if (kid == 4) {  // Dot → reduction[sum:fp32] / reduction[dot:fp32]
        // 25 号计划 §4：sum 与 dot 是不同曲线；GPU Dot kernel 实为 sum(x)
        const std::string red_op =
            (r.variant == "dot" && device.kind != DeviceKind::Gpu) ? "dot" : "sum";
        auto& c = device.reduction[{red_op, prec}];
        mark_measured(c, pt); c.points.push_back(pt);
    } else if (kid == 5) {  // Transpose → transfer (CPU memcpy 带宽)
        // Commit E：用 Transpose kernel_id 标记 CPU 内存传输带宽
        auto& c = device.transfer[{TransferDirection::Bidir, MemoryType::HostPlain}];
        mark_measured(c, pt); c.points.push_back(pt);
    } else if (kid == 6) {  // Convolution2D → convolution[direct:3x3:fp32]
        auto& c = device.convolution["direct:3x3:" + std::string(r.precision)];
        mark_measured(c, pt); c.points.push_back(pt);
    } else if (kid == 7) {  // Histogram256 → irregular[histogram:uniform]
        auto& c = device.irregular["histogram:uniform"];
        mark_measured(c, pt); c.points.push_back(pt);
    } else if (kid == 9) {  // Gather → irregular[gather:random]
        auto& c = device.irregular["gather:random"];
        mark_measured(c, pt); c.points.push_back(pt);
    } else if (kid == 10) { // Scatter → irregular[scatter:random]
        auto& c = device.irregular["scatter:random"];
        mark_measured(c, pt); c.points.push_back(pt);
    } else if (kid == 11) { // Mandelbrot → branch[highly_variable]
        auto& c = device.branch["highly_variable"];
        mark_measured(c, pt); c.points.push_back(pt);
    } else if (kid == 12) { // Gemm → library[gemm]
        LibraryCapability cap;
        cap.available = true;
        cap.implementation = "self-benchmark";
        auto& c = cap.size_curves["default"];
        mark_measured(c, pt); c.points.push_back(pt);
        device.library["gemm"] = std::move(cap);
    } else if (kid == 13) { // Fft → library[fft]
        LibraryCapability cap;
        cap.available = true;
        cap.implementation = "self-benchmark";
        auto& c = cap.size_curves["default"];
        mark_measured(c, pt); c.points.push_back(pt);
        device.library["fft"] = std::move(cap);
    } else if (kid == 0) { // Custom → overhead[submit]（Commit E：测量真实 parallel_for 提交开销）
        FixedOverhead submit_oh;
        submit_oh.median_ns = pt.median;
        submit_oh.p95_ns = pt.p95;
        submit_oh.cold_start_ns = pt.p95;  // 首次调用可能冷启动
        submit_oh.warm_ns = pt.median;      // 后续调用温态
        device.overhead["submit"] = submit_oh;
    } else {
        // 兜底：其他 kernel → arithmetic[<precision>:add:baseline]
        auto& c = device.arithmetic[{prec, "add:baseline"}];
        mark_measured(c, pt); c.points.push_back(pt);
    }
}

void ProfileGenerator::fill_default_overheads(DeviceProfile& device) const {
    // 保守固定开销估算：仅填充未被 benchmark 测量的 overhead 项
    // Commit E：submit 现在由 benchmark_driver 测量（kid=0 Custom），
    //   其余 launch/event/alloc/merge 仍用保守估算待后续 benchmark 补充。
    bool is_gpu = (device.kind == DeviceKind::Gpu);

    // submit：仅当未被测量时才填默认值
    if (device.overhead.find("submit") == device.overhead.end()) {
        FixedOverhead submit_oh;
        submit_oh.median_ns = is_gpu ? 8500.0 : 1100.0;
        submit_oh.p95_ns = is_gpu ? 9200.0 : 1800.0;
        submit_oh.cold_start_ns = is_gpu ? 450000.0 : 95000.0;
        submit_oh.warm_ns = is_gpu ? 6500.0 : 600.0;
        device.overhead["submit"] = submit_oh;
    }

    FixedOverhead launch_oh;
    launch_oh.median_ns = is_gpu ? 7800.0 : 120.0;
    launch_oh.p95_ns = is_gpu ? 8500.0 : 200.0;
    launch_oh.cold_start_ns = is_gpu ? 95000.0 : 8000.0;
    launch_oh.warm_ns = is_gpu ? 6200.0 : 80.0;
    device.overhead["launch"] = launch_oh;

    FixedOverhead event_oh;
    event_oh.median_ns = is_gpu ? 1200.0 : 220.0;
    event_oh.p95_ns = is_gpu ? 1500.0 : 320.0;
    event_oh.cold_start_ns = is_gpu ? 8000.0 : 5000.0;
    event_oh.warm_ns = is_gpu ? 1000.0 : 180.0;
    device.overhead["event"] = event_oh;

    FixedOverhead alloc_oh;
    alloc_oh.median_ns = is_gpu ? 350000.0 : 580.0;
    alloc_oh.p95_ns = is_gpu ? 380000.0 : 950.0;
    alloc_oh.cold_start_ns = is_gpu ? 1200000.0 : 50000.0;
    alloc_oh.warm_ns = is_gpu ? 280000.0 : 350.0;
    device.overhead["alloc"] = alloc_oh;

    FixedOverhead merge_oh;
    merge_oh.median_ns = is_gpu ? 850.0 : 240.0;
    merge_oh.p95_ns = is_gpu ? 1100.0 : 380.0;
    merge_oh.cold_start_ns = is_gpu ? 5200.0 : 12000.0;
    merge_oh.warm_ns = is_gpu ? 720.0 : 200.0;
    device.overhead["merge"] = merge_oh;
}

HardwareProfile ProfileGenerator::generate_hardware_profile(
    const std::vector<KernelBenchmarkResult>& results, ProfileKind kind) const {
    // 先聚合原始样本（median/stddev），否则曲线 median 恒 0
    std::vector<KernelBenchmarkResult> aggregated = results;
    for (auto& r : aggregated) {
        aggregate(r);
    }

    HardwareProfile hp;
    hp.schema_version = "acr.hardware_profile.v1";
    hp.profile_kind = profile_kind_str(kind);
    // 25 号计划 §3.1：quick 标定仅作冒烟/诊断
    hp.diagnostic_only = (kind == ProfileKind::Quick);
    hp.state = HwProfileState::Valid;
    hp.stale = false;

    // 设备指纹
    DeviceFingerprint fp = build_fingerprint();
    hp.fingerprint_sha256 = fp.sha256;

    // 时间戳
    {
        std::time_t now = std::time(nullptr);
        std::tm* tm = std::gmtime(&now);
        char buf[32];
        if (tm) {
            std::strftime(buf, sizeof(buf), "%Y%m%dT%H%M%SZ", tm);
            hp.generated_at = buf;
        }
    }

    // 设备画像
    hp.devices = build_device_profiles(aggregated, kind);

    return hp;
}

std::string ProfileGenerator::serialize_hardware_profile(const HardwareProfile& hp) {
    std::ostringstream os;
    os << "{";
    os << "\"schema_version\":\"" << esc(hp.schema_version) << "\"";
    os << ",\"generated_at\":\"" << esc(hp.generated_at) << "\"";
    os << ",\"profile_kind\":\"" << esc(hp.profile_kind) << "\"";
    os << ",\"diagnostic_only\":" << (hp.diagnostic_only ? "true" : "false");
    os << ",\"fingerprint_sha256\":\"" << esc(hp.fingerprint_sha256) << "\"";
    os << ",\"stale\":" << (hp.stale ? "true" : "false");
    os << ",\"devices\":[";

    for (std::size_t i = 0; i < hp.devices.size(); ++i) {
        const auto& dev = hp.devices[i];
        if (i > 0) os << ",";
        os << "{";
        os << "\"device_id\":" << dev.device_id;
        os << ",\"device_name\":\"" << esc(dev.device_name) << "\"";
        os << ",\"kind\":\"" << device_kind_str(dev.kind) << "\"";
        os << ",\"total_memory_bytes\":" << dev.total_memory_bytes;
        os << ",\"available_memory_bytes\":" << dev.available_memory_bytes;
        os << ",\"compute_units\":" << dev.compute_units;
        os << ",\"peak_bandwidth_gbps\":" << dev.peak_bandwidth_gbps;

        // arithmetic 曲线
        if (!dev.arithmetic.empty()) {
            os << ",\"arithmetic\":{";
            std::size_t j = 0;
            for (const auto& [key, curve] : dev.arithmetic) {
                if (j > 0) os << ",";
                os << "\"" << hw_precision_str(key.first) << ":" << key.second << "\":";
                serialize_curve(os, curve);
                ++j;
            }
            os << "}";
        }

        // memory 曲线
        if (!dev.memory.empty()) {
            os << ",\"memory\":{";
            std::size_t j = 0;
            for (const auto& [key, curve] : dev.memory) {
                if (j > 0) os << ",";
                os << "\"" << memory_level_str(std::get<0>(key)) << ":"
                   << memory_residency_str(std::get<1>(key)) << ":"
                   << std::get<2>(key) << "\":";
                serialize_curve(os, curve);
                ++j;
            }
            os << "}";
        }

        // transfer 曲线
        if (!dev.transfer.empty()) {
            os << ",\"transfer\":{";
            std::size_t j = 0;
            for (const auto& [key, curve] : dev.transfer) {
                if (j > 0) os << ",";
                os << "\"" << transfer_direction_str(key.first) << ":"
                   << memory_type_str(key.second) << "\":";
                serialize_curve(os, curve);
                ++j;
            }
            os << "}";
        }

        // reduction 曲线
        if (!dev.reduction.empty()) {
            os << ",\"reduction\":{";
            std::size_t j = 0;
            for (const auto& [key, curve] : dev.reduction) {
                if (j > 0) os << ",";
                os << "\"" << key.first << ":" << hw_precision_str(key.second) << "\":";
                serialize_curve(os, curve);
                ++j;
            }
            os << "}";
        }

        // convolution 曲线
        if (!dev.convolution.empty()) {
            os << ",\"convolution\":{";
            std::size_t j = 0;
            for (const auto& [key, curve] : dev.convolution) {
                if (j > 0) os << ",";
                os << "\"" << esc(key) << "\":";
                serialize_curve(os, curve);
                ++j;
            }
            os << "}";
        }

        // irregular 曲线
        if (!dev.irregular.empty()) {
            os << ",\"irregular\":{";
            std::size_t j = 0;
            for (const auto& [key, curve] : dev.irregular) {
                if (j > 0) os << ",";
                os << "\"" << esc(key) << "\":";
                serialize_curve(os, curve);
                ++j;
            }
            os << "}";
        }

        // branch 曲线
        if (!dev.branch.empty()) {
            os << ",\"branch\":{";
            std::size_t j = 0;
            for (const auto& [key, curve] : dev.branch) {
                if (j > 0) os << ",";
                os << "\"" << esc(key) << "\":";
                serialize_curve(os, curve);
                ++j;
            }
            os << "}";
        }

        // overhead
        if (!dev.overhead.empty()) {
            os << ",\"overhead\":{";
            std::size_t j = 0;
            for (const auto& [name, oh] : dev.overhead) {
                if (j > 0) os << ",";
                serialize_overhead(os, name, oh);
                ++j;
            }
            os << "}";
        }

        // library
        if (!dev.library.empty()) {
            os << ",\"library\":{";
            std::size_t j = 0;
            for (const auto& [name, cap] : dev.library) {
                if (j > 0) os << ",";
                os << "\"" << esc(name) << "\":{";
                os << "\"available\":" << (cap.available ? "true" : "false");
                if (!cap.implementation.empty()) {
                    os << ",\"implementation\":\"" << esc(cap.implementation) << "\"";
                }
                if (!cap.version.empty()) {
                    os << ",\"version\":\"" << esc(cap.version) << "\"";
                }
                os << "}";
                ++j;
            }
            os << "}";
        }

        os << "}";  // end device
    }
    os << "]";  // end devices
    os << "}";  // end root
    return os.str();
}

bool ProfileGenerator::write_hardware_profile_to_file(
    const std::string& path, const HardwareProfile& hp) {
    std::ofstream f(path, std::ios::out | std::ios::trunc);
    if (!f.is_open()) return false;
    f << serialize_hardware_profile(hp);
    return f.good();
}

} // namespace astro::compute::qualification
