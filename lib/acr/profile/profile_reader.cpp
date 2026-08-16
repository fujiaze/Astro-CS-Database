// lib/acr/profile/profile_reader.cpp — HardwareProfileReader 实现
// Phase E2：手写极简 JSON 解析 + 三态处理 + lazy load + CPU fallback。
//
// 解析 hardware-profile.json 关键字段：
// schema_version, generated_at, profile_kind, fingerprint_sha256, devices[]
// device: device_id, device_name, kind, total_memory_bytes, available_memory_bytes,
// compute_units, peak_bandwidth_gbps, overhead{}
// 曲线字段（arithmetic/memory/transfer/reduction/convolution/irregular/branch/library）
// 在解析时跳过——CostEstimator 在无曲线时降级到 peak_bandwidth/overhead 估算。
// 完整曲线由 Phase E3 profile_generator 生成；运行时解析只读关键字段。
#include "profile_reader.hpp"
#include "crypto/sha256.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "astro/compute/topology.hpp"

namespace astro::compute::profile {

namespace {

// ===== 极简 JSON 解析器（与 routing/static_router.cpp 一致风格）=====
class JsonParser {
public:
    explicit JsonParser(const std::string& s) : s_(s), pos_(0) {}

    bool parse_object_start() { skip_ws(); return consume('{'); }
    bool parse_object_end()   { skip_ws(); return consume('}'); }
    bool parse_array_start()  { skip_ws(); return consume('['); }
    bool parse_array_end()    { skip_ws(); return consume(']'); }
    bool parse_comma()        { skip_ws(); return consume(','); }
    bool parse_colon()        { skip_ws(); return consume(':'); }

    bool parse_string(std::string& out) {
        skip_ws();
        if (pos_ >= s_.size() || s_[pos_] != '"') return false;
        ++pos_;
        out.clear();
        while (pos_ < s_.size() && s_[pos_] != '"') {
            if (s_[pos_] == '\\' && pos_ + 1 < s_.size()) {
                char esc = s_[pos_ + 1];
                switch (esc) {
                    case '"':  out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/':  out.push_back('/'); break;
                    case 'n':  out.push_back('\n'); break;
                    case 'r':  out.push_back('\r'); break;
                    case 't':  out.push_back('\t'); break;
                    default:   out.push_back(esc); break;
                }
                pos_ += 2;
            } else {
                out.push_back(s_[pos_]);
                ++pos_;
            }
        }
        if (pos_ >= s_.size()) return false;
        ++pos_;
        return true;
    }

    bool parse_number(double& out) {
        skip_ws();
        std::size_t start = pos_;
        if (pos_ < s_.size() && (s_[pos_] == '-' || s_[pos_] == '+')) ++pos_;
        bool seen_digit = false;
        while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) {
            ++pos_; seen_digit = true;
        }
        if (pos_ < s_.size() && s_[pos_] == '.') {
            ++pos_;
            while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) {
                ++pos_; seen_digit = true;
            }
        }
        if (pos_ < s_.size() && (s_[pos_] == 'e' || s_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < s_.size() && (s_[pos_] == '+' || s_[pos_] == '-')) ++pos_;
            while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) ++pos_;
        }
        if (!seen_digit) return false;
        try { out = std::stod(s_.substr(start, pos_ - start)); } catch (...) { return false; }
        return true;
    }

    bool parse_uint64(std::uint64_t& out) {
        double d;
        if (!parse_number(d)) return false;
        out = static_cast<std::uint64_t>(d);
        return true;
    }
    bool parse_int32(std::int32_t& out) {
        double d;
        if (!parse_number(d)) return false;
        out = static_cast<std::int32_t>(d);
        return true;
    }
    bool parse_size(std::size_t& out) {
        double d;
        if (!parse_number(d)) return false;
        out = static_cast<std::size_t>(d);
        return true;
    }

    bool peek_char(char c) const { return pos_ < s_.size() && s_[pos_] == c; }
    void skip_ws() {
        while (pos_ < s_.size()) {
            char c = s_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++pos_;
            else break;
        }
    }
    bool consume(char c) {
        if (pos_ < s_.size() && s_[pos_] == c) { ++pos_; return true; }
        return false;
    }
    std::size_t pos() const noexcept { return pos_; }
    bool eof() const noexcept { return pos_ >= s_.size(); }
    char peek() const noexcept { return pos_ < s_.size() ? s_[pos_] : '\0'; }
    void advance(std::size_t n = 1) { pos_ += n; }
    const std::string& source() const noexcept { return s_; }

    // 跳过任意 JSON 值（object/array/string/number/bool/null），用栈匹配嵌套
    bool skip_value() {
        skip_ws();
        if (eof()) return false;
        char c = peek();
        if (c == '"') { std::string s; return parse_string(s); }
        if (c == '{' || c == '[') {
            std::vector<char> stack;
            stack.push_back(c);
            advance();
            while (!stack.empty() && !eof()) {
                skip_ws();
                if (eof()) return false;
                char ch = peek();
                if (ch == '"') {
                    std::string s;
                    if (!parse_string(s)) return false;
                } else if (ch == '{' || ch == '[') {
                    stack.push_back(ch);
                    advance();
                } else if (ch == '}') {
                    if (stack.back() != '{') return false;
                    stack.pop_back();
                    advance();
                } else if (ch == ']') {
                    if (stack.back() != '[') return false;
                    stack.pop_back();
                    advance();
                } else {
                    double d;
                    if (!parse_number(d)) {
                        if (s_.compare(pos_, 4, "true") == 0) { advance(4); }
                        else if (s_.compare(pos_, 5, "false") == 0) { advance(5); }
                        else if (s_.compare(pos_, 4, "null") == 0) { advance(4); }
                        else { advance(); }
                    }
                }
            }
            return stack.empty();
        }
        double d;
        if (parse_number(d)) return true;
        if (s_.compare(pos_, 4, "true") == 0)  { advance(4); return true; }
        if (s_.compare(pos_, 5, "false") == 0) { advance(5); return true; }
        if (s_.compare(pos_, 4, "null") == 0)  { advance(4); return true; }
        return false;
    }

private:
    const std::string& s_;
    std::size_t pos_;
};

// 解析 FixedOverhead 对象：{median_ns, p95_ns, cold_start_ns, warm_ns}
bool parse_overhead(JsonParser& p, FixedOverhead& out) {
    if (!p.parse_object_start()) return false;
    p.skip_ws();
    if (p.peek_char('}')) { p.parse_object_end(); return true; }
    while (true) {
        std::string key;
        if (!p.parse_string(key)) return false;
        if (!p.parse_colon()) return false;
        double d;
        if (key == "median_ns")         { if (!p.parse_number(d)) return false; out.median_ns = d; }
        else if (key == "p95_ns")       { if (!p.parse_number(d)) return false; out.p95_ns = d; }
        else if (key == "cold_start_ns"){ if (!p.parse_number(d)) return false; out.cold_start_ns = d; }
        else if (key == "warm_ns")      { if (!p.parse_number(d)) return false; out.warm_ns = d; }
        else { if (!p.skip_value()) return false; }
        if (p.peek_char(',')) { p.parse_comma(); continue; }
        break;
    }
    if (!p.parse_object_end()) return false;
    return true;
}

// 解析 LibraryCapability 对象（简化：只读 available/implementation/version，size_curves 跳过）
bool parse_library(JsonParser& p, LibraryCapability& out) {
    if (!p.parse_object_start()) return false;
    p.skip_ws();
    if (p.peek_char('}')) { p.parse_object_end(); return true; }
    while (true) {
        std::string key;
        if (!p.parse_string(key)) return false;
        if (!p.parse_colon()) return false;
        if (key == "available") {
            p.skip_ws();
            const std::string& s = p.source();
            if (s.compare(p.pos(), 4, "true") == 0) { out.available = true; p.advance(4); }
            else if (s.compare(p.pos(), 5, "false") == 0) { out.available = false; p.advance(5); }
            else { if (!p.skip_value()) return false; }
        } else if (key == "implementation") {
            if (!p.parse_string(out.implementation)) return false;
        } else if (key == "version") {
            if (!p.parse_string(out.version)) return false;
        } else {
            if (!p.skip_value()) return false;
        }
        if (p.peek_char(',')) { p.parse_comma(); continue; }
        break;
    }
    if (!p.parse_object_end()) return false;
    return true;
}

// 解析 DeviceProfile 对象
// 关键字段：device_id, device_name, kind, total/available_memory_bytes, compute_units,
// peak_bandwidth_gbps, overhead{}, library{}
// 曲线数组字段（arithmetic/memory/transfer/reduction/convolution/irregular/branch）跳过
bool parse_device(JsonParser& p, DeviceProfile& out) {
    if (!p.parse_object_start()) return false;
    p.skip_ws();
    if (p.peek_char('}')) { p.parse_object_end(); return true; }
    while (true) {
        std::string key;
        if (!p.parse_string(key)) return false;
        if (!p.parse_colon()) return false;
        if (key == "device_id") {
            if (!p.parse_int32(out.device_id)) return false;
        } else if (key == "device_name") {
            if (!p.parse_string(out.device_name)) return false;
        } else if (key == "kind") {
            std::string k;
            if (!p.parse_string(k)) return false;
            out.kind = (k == "gpu") ? DeviceKind::Gpu : DeviceKind::Cpu;
        } else if (key == "total_memory_bytes") {
            std::size_t v;
            if (!p.parse_size(v)) return false;
            out.total_memory_bytes = v;
        } else if (key == "available_memory_bytes") {
            std::size_t v;
            if (!p.parse_size(v)) return false;
            out.available_memory_bytes = v;
        } else if (key == "compute_units") {
            std::uint64_t v;
            if (!p.parse_uint64(v)) return false;
            out.compute_units = static_cast<std::uint32_t>(v);
        } else if (key == "peak_bandwidth_gbps") {
            double d;
            if (!p.parse_number(d)) return false;
            out.peak_bandwidth_gbps = d;
        } else if (key == "overhead") {
            // object: {"submit":{...}, "launch":{...}, ...}
            if (!p.parse_object_start()) return false;
            p.skip_ws();
            if (p.peek_char('}')) { p.parse_object_end(); }
            else {
                while (true) {
                    std::string k2;
                    if (!p.parse_string(k2)) return false;
                    if (!p.parse_colon()) return false;
                    FixedOverhead oh;
                    if (!parse_overhead(p, oh)) return false;
                    out.overhead[k2] = oh;
                    if (p.peek_char(',')) { p.parse_comma(); continue; }
                    break;
                }
                if (!p.parse_object_end()) return false;
            }
        } else if (key == "library") {
            if (!p.parse_object_start()) return false;
            p.skip_ws();
            if (p.peek_char('}')) { p.parse_object_end(); }
            else {
                while (true) {
                    std::string k2;
                    if (!p.parse_string(k2)) return false;
                    if (!p.parse_colon()) return false;
                    LibraryCapability cap;
                    if (!parse_library(p, cap)) return false;
                    out.library[k2] = std::move(cap);
                    if (p.peek_char(',')) { p.parse_comma(); continue; }
                    break;
                }
                if (!p.parse_object_end()) return false;
            }
        } else {
            // 曲线数组字段（arithmetic/memory/transfer/reduction/convolution/irregular/branch）
            // 及其他未知字段：跳过
            if (!p.skip_value()) return false;
        }
        if (p.peek_char(',')) { p.parse_comma(); continue; }
        break;
    }
    if (!p.parse_object_end()) return false;
    return true;
}

// ===== 从 hardware_report JSON 提取字段 =====
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

// ===== 公开 API =====

std::string compute_fingerprint_sha256() {
    std::string hw = generate_hardware_report();
    std::string cpu_model = extract_json_field(hw, "model");
    if (cpu_model.empty()) cpu_model = extract_json_field(hw, "cpu_model");
    std::string cores = extract_json_number(hw, "cores");
    if (cores.empty()) cores = extract_json_number(hw, "cpu_cores");
    std::string isa = extract_json_number(hw, "mask");
    if (isa.empty()) isa = extract_json_number(hw, "isa_mask");
    std::string gpu_name = extract_json_field(hw, "gpu_name");
    if (gpu_name.empty()) gpu_name = extract_json_field(hw, "name");
    std::string vmem = extract_json_number(hw, "total_memory");
    if (vmem.empty()) vmem = extract_json_number(hw, "gpu_memory_bytes");
    std::string driver = extract_json_field(hw, "driver_version");

    std::ostringstream fp_input;
    fp_input << cpu_model << "|" << cores << "|" << isa
             << "|" << gpu_name << "|" << vmem << "|" << driver;
    // SHA-256 归一化到 lib/common/crypto（单一实现）
    return astrocs::crypto::sha256_hex(fp_input.str().data(),
                                       fp_input.str().size());
}

HardwareProfile make_cpu_fallback_profile() {
    HardwareProfile p;
    p.schema_version = "acr.hardware_profile.v1";
    p.fingerprint_sha256 = compute_fingerprint_sha256();
    p.profile_kind = "fallback";
    p.state = HwProfileState::Missing;

    DeviceProfile cpu;
    cpu.device_id = kHwCpuDeviceId;
    cpu.device_name = "CPU (fallback, no profile)";
    cpu.kind = DeviceKind::Cpu;
    cpu.compute_units = static_cast<std::uint32_t>(std::thread::hardware_concurrency());
    if (cpu.compute_units == 0) cpu.compute_units = 4;
    cpu.total_memory_bytes = 0;
    cpu.available_memory_bytes = 0;
    cpu.peak_bandwidth_gbps = 0.0;

    // 保守的固定开销估算（无画像时用）
    FixedOverhead submit_oh;
    submit_oh.median_ns = 1000.0;      // ~1us TBB 提交
    submit_oh.warm_ns = 500.0;
    submit_oh.cold_start_ns = 100000.0;
    cpu.overhead["submit"] = submit_oh;

    FixedOverhead launch_oh;
    launch_oh.median_ns = 100.0;
    launch_oh.warm_ns = 50.0;
    cpu.overhead["launch"] = launch_oh;

    FixedOverhead event_oh;
    event_oh.median_ns = 200.0;
    cpu.overhead["event"] = event_oh;

    FixedOverhead alloc_oh;
    alloc_oh.median_ns = 500.0;
    cpu.overhead["alloc"] = alloc_oh;

    FixedOverhead merge_oh;
    merge_oh.median_ns = 200.0;
    cpu.overhead["merge"] = merge_oh;

    p.devices.push_back(std::move(cpu));
    return p;
}

// ===== HardwareProfileReader::Impl =====
struct HardwareProfileReader::Impl {
    std::mutex mtx;
    std::string profile_path{"./hardware-profile.json"};
    std::atomic<bool> loaded{false};
    std::atomic<bool> load_attempted{false};
    HardwareProfile profile;
    HardwareProfile fallback_profile;
    HwProfileState state{HwProfileState::Missing};
    bool stale{false};

    void load_locked() {
        if (load_attempted.load(std::memory_order_acquire)) return;
        load_attempted.store(true, std::memory_order_release);

        if (fallback_profile.devices.empty()) {
            fallback_profile = make_cpu_fallback_profile();
        }

        std::ifstream f(profile_path, std::ios::in);
        if (!f.is_open()) {
            state = HwProfileState::Missing;
            profile = fallback_profile;
            profile.state = HwProfileState::Missing;
            return;
        }
        std::ostringstream oss;
        oss << f.rdbuf();
        std::string json = oss.str();

        HardwareProfile parsed;
        JsonParser p(json);
        if (!p.parse_object_start()) {
            state = HwProfileState::Corrupt;
            profile = fallback_profile;
            profile.state = HwProfileState::Corrupt;
            return;
        }
        p.skip_ws();
        if (!p.peek_char('}')) {
            while (true) {
                std::string key;
                if (!p.parse_string(key)) { goto corrupt; }
                if (!p.parse_colon()) { goto corrupt; }
                if (key == "schema_version") {
                    if (!p.parse_string(parsed.schema_version)) { /* tolerate */ }
                } else if (key == "generated_at") {
                    if (!p.parse_string(parsed.generated_at)) { /* tolerate */ }
                } else if (key == "profile_kind") {
                    if (!p.parse_string(parsed.profile_kind)) { /* tolerate */ }
                } else if (key == "fingerprint_sha256") {
                    if (!p.parse_string(parsed.fingerprint_sha256)) { /* tolerate */ }
                } else if (key == "devices") {
                    if (!p.parse_array_start()) { goto corrupt; }
                    p.skip_ws();
                    if (p.peek_char(']')) { p.parse_array_end(); }
                    else {
                        while (true) {
                            DeviceProfile dev;
                            if (!parse_device(p, dev)) { goto corrupt; }
                            parsed.devices.push_back(std::move(dev));
                            if (p.peek_char(',')) { p.parse_comma(); continue; }
                            break;
                        }
                        if (!p.parse_array_end()) { goto corrupt; }
                    }
                } else {
                    if (!p.skip_value()) { goto corrupt; }
                }
                if (p.peek_char(',')) { p.parse_comma(); continue; }
                break;
            }
        }
        if (!p.parse_object_end()) { goto corrupt; }

        // schema 校验
        if (parsed.schema_version.empty() ||
            parsed.schema_version != "acr.hardware_profile.v1") {
            goto corrupt;
        }

        // 指纹比较
        {
            std::string current_fp = compute_fingerprint_sha256();
            bool fp_match = !parsed.fingerprint_sha256.empty() &&
                            parsed.fingerprint_sha256 == current_fp;
            profile = std::move(parsed);
            if (!fp_match) {
                stale = true;
                state = HwProfileState::Stale;
                profile.state = HwProfileState::Stale;
            } else {
                stale = false;
                state = HwProfileState::Valid;
                profile.state = HwProfileState::Valid;
            }
            loaded.store(true, std::memory_order_release);
        }
        return;

    corrupt:
        state = HwProfileState::Corrupt;
        profile = fallback_profile;
        profile.state = HwProfileState::Corrupt;
        return;
    }
};

HardwareProfileReader::HardwareProfileReader()
    : impl_(std::make_unique<Impl>()) {}
HardwareProfileReader::~HardwareProfileReader() = default;

void HardwareProfileReader::set_profile_path(const std::string& path) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (impl_->load_attempted.load(std::memory_order_acquire)) return;
    impl_->profile_path = path;
}

void HardwareProfileReader::invalidate_cache() {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->loaded.store(false, std::memory_order_release);
    impl_->load_attempted.store(false, std::memory_order_release);
    impl_->state = HwProfileState::Missing;
    impl_->stale = false;
    impl_->profile = HardwareProfile{};
}

const HardwareProfile* HardwareProfileReader::get_profile() {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->load_locked();
    if (impl_->state == HwProfileState::Missing || impl_->state == HwProfileState::Corrupt) {
        return nullptr;
    }
    return &impl_->profile;
}

HwProfileState HardwareProfileReader::profile_state() const noexcept {
    return impl_->state;
}

bool HardwareProfileReader::loaded() const noexcept {
    return impl_->loaded.load(std::memory_order_acquire);
}

const std::string& HardwareProfileReader::profile_path() const noexcept {
    return impl_->profile_path;
}

const HardwareProfile& HardwareProfileReader::get_profile_or_cpu_fallback() {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->load_locked();
    if (impl_->state == HwProfileState::Missing) {
        std::fprintf(stderr,
            "[ACR] 警告：硬件画像未标定（missing），使用 CPU fallback（请运行 acr-benchmark 生成 hardware-profile.json）\n");
        return impl_->fallback_profile;
    }
    if (impl_->state == HwProfileState::Corrupt) {
        std::fprintf(stderr,
            "[ACR] 警告：硬件画像损坏（corrupt），使用 CPU fallback（hardware-profile.json 解析失败）\n");
        return impl_->fallback_profile;
    }
    if (impl_->state == HwProfileState::Stale) {
        std::fprintf(stderr,
            "[ACR] 警告：硬件画像过期（指纹不匹配），继续运行（不强制重新 benchmark）\n");
    }
    return impl_->profile;
}

std::string HardwareProfileReader::status_json() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    std::ostringstream os;
    os << "{";
    os << "\"profile_state\":\"" << hw_profile_state_str(impl_->state) << "\"";
    os << ",\"profile_path\":\"" << impl_->profile_path << "\"";
    os << ",\"loaded\":" << (impl_->loaded.load(std::memory_order_acquire) ? "true" : "false");
    os << ",\"stale\":" << (impl_->stale ? "true" : "false");
    if (impl_->loaded.load(std::memory_order_acquire)) {
        os << ",\"schema_version\":\"" << impl_->profile.schema_version << "\"";
        os << ",\"generated_at\":\"" << impl_->profile.generated_at << "\"";
        os << ",\"profile_kind\":\"" << impl_->profile.profile_kind << "\"";
        os << ",\"fingerprint_sha256\":\"" << impl_->profile.fingerprint_sha256 << "\"";
        os << ",\"devices_count\":" << impl_->profile.devices.size();
        os << ",\"has_gpu\":" << (impl_->profile.has_gpu() ? "true" : "false");
    }
    os << "}";
    return os.str();
}

// ===== 全局单例 =====
HardwareProfileReader& global_profile_reader() {
    static HardwareProfileReader inst;
    return inst;
}

} // namespace astro::compute::profile
