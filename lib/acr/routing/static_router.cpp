// lib/acr/routing/static_router.cpp — 静态路由解析器实现
// Phase E：手写极简 JSON 解析 + 三态处理 + lazy load。
#include "static_router.hpp"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>

#include "astro/compute/acr.hpp"
#include "astro/compute/topology.hpp"

namespace astro::compute::routing {

namespace {

// ===== 极简 JSON 解析（仅支持 routes.json 子集）=====
// 设计：手写下推自动机，状态机式扫描，避免引入 nlohmann/json 依赖。
// 支持的子集：object/array/string/number/bool/null，无浮点尾数精度保证（throughput 5 位足够）。
class JsonParser {
public:
    explicit JsonParser(const std::string& s) : s_(s), pos_(0) {}

    bool parse_object_start() {
        skip_ws();
        if (!consume('{')) return false;
        return true;
    }
    bool parse_object_end() {
        skip_ws();
        return consume('}');
    }
    bool parse_array_start() {
        skip_ws();
        return consume('[');
    }
    bool parse_array_end() {
        skip_ws();
        return consume(']');
    }
    bool parse_comma() {
        skip_ws();
        return consume(',');
    }
    bool parse_colon() {
        skip_ws();
        return consume(':');
    }
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
        ++pos_;  // skip closing "
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
        try {
            out = std::stod(s_.substr(start, pos_ - start));
        } catch (...) {
            return false;
        }
        return true;
    }
    bool parse_uint64(std::uint64_t& out) {
        double d;
        if (!parse_number(d)) return false;
        out = static_cast<std::uint64_t>(d);
        return true;
    }
    bool parse_uint32(std::uint32_t& out) {
        double d;
        if (!parse_number(d)) return false;
        out = static_cast<std::uint32_t>(d);
        return true;
    }
    bool peek_char(char c) {
        skip_ws();
        return pos_ < s_.size() && s_[pos_] == c;
    }
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
    void advance(std::size_t n = 1) { pos_ += n; }

private:
    const std::string& s_;
    std::size_t pos_;
};

// 解析 routes 数组
bool parse_routes_array(JsonParser& p, std::vector<RouteEntryView>& out) {
    if (!p.parse_array_start()) return false;
    p.skip_ws();
    if (p.peek_char(']')) { p.parse_array_end(); return true; }
    while (true) {
        if (!p.parse_object_start()) return false;
        RouteEntryView e;
        p.skip_ws();
        if (!p.peek_char('}')) {
            while (true) {
                std::string key;
                if (!p.parse_string(key)) return false;
                if (!p.parse_colon()) return false;
                if (key == "kernel_id") {
                    if (!p.parse_uint32(e.kernel_id)) return false;
                } else if (key == "kernel_name") {
                    if (!p.parse_string(e.kernel_name)) return false;
                } else if (key == "precision") {
                    if (!p.parse_string(e.precision)) return false;
                } else if (key == "preferred_backend") {
                    if (!p.parse_string(e.preferred_backend)) return false;
                } else if (key == "expected_throughput_gbps") {
                    double d;
                    if (!p.parse_number(d)) return false;
                    e.expected_throughput_gbps = d;
                } else if (key == "reason") {
                    if (!p.parse_string(e.reason)) return false;
                } else {
                    // 跳过未知字段（简单值：string/number）
                    std::string dummy_s;
                    double dummy_d;
                    if (!p.parse_string(dummy_s) && !p.parse_number(dummy_d)) return false;
                }
                if (p.peek_char(',')) { p.parse_comma(); continue; }
                break;
            }
        }
        if (!p.parse_object_end()) return false;
        out.push_back(std::move(e));
        if (p.peek_char(',')) { p.parse_comma(); continue; }
        break;
    }
    if (!p.parse_array_end()) return false;
    return true;
}

// 解析 fingerprint 对象
bool parse_fingerprint_object(JsonParser& p, DeviceFingerprintView& out) {
    if (!p.parse_object_start()) return false;
    p.skip_ws();
    if (p.peek_char('}')) { p.parse_object_end(); return true; }
    while (true) {
        std::string key;
        if (!p.parse_string(key)) return false;
        if (!p.parse_colon()) return false;
        if (key == "cpu_model") {
            if (!p.parse_string(out.cpu_model)) return false;
        } else if (key == "cpu_cores") {
            double d;
            if (!p.parse_number(d)) return false;
            out.cpu_cores = static_cast<std::uint32_t>(d);
        } else if (key == "isa_mask") {
            std::uint64_t u;
            if (!p.parse_uint64(u)) return false;
            out.isa_mask = u;
        } else if (key == "gpu_name") {
            if (!p.parse_string(out.gpu_name)) return false;
        } else if (key == "gpu_memory_bytes") {
            std::uint64_t u;
            if (!p.parse_uint64(u)) return false;
            out.gpu_memory_bytes = u;
        } else if (key == "gpu_driver_version") {
            if (!p.parse_string(out.gpu_driver_version)) return false;
        } else if (key == "sha256") {
            if (!p.parse_string(out.sha256)) return false;
        } else {
            std::string dummy_s;
            double dummy_d;
            if (!p.parse_string(dummy_s) && !p.parse_number(dummy_d)) return false;
        }
        if (p.peek_char(',')) { p.parse_comma(); continue; }
        break;
    }
    if (!p.parse_object_end()) return false;
    return true;
}

} // anonymous namespace

// ===== 公开 API =====
const char* profile_state_str(ProfileState s) noexcept {
    switch (s) {
        case ProfileState::Missing: return "missing";
        case ProfileState::Stale:   return "stale";
        case ProfileState::Corrupt: return "corrupt";
        case ProfileState::Valid:   return "valid";
    }
    return "unknown";
}

bool parse_route_profile(const std::string& json, RouteProfile& out) noexcept {
    try {
        JsonParser p(json);
        if (!p.parse_object_start()) return false;
        p.skip_ws();
        if (p.peek_char('}')) { p.parse_object_end(); return true; }
        while (true) {
            std::string key;
            if (!p.parse_string(key)) return false;
            if (!p.parse_colon()) return false;
            if (key == "schema_version") {
                if (!p.parse_string(out.schema_version)) return false;
            } else if (key == "generated_at") {
                if (!p.parse_string(out.generated_at)) return false;
            } else if (key == "profile_kind") {
                if (!p.parse_string(out.profile_kind)) return false;
            } else if (key == "fingerprint") {
                if (!parse_fingerprint_object(p, out.fingerprint)) return false;
            } else if (key == "routes") {
                if (!parse_routes_array(p, out.routes)) return false;
            } else {
                // 跳过未知字段（object/array 简单跳过不实现，假设 raw_results 等
                // 不影响解析）。对 string/number 直接消费。
                std::string dummy_s;
                double dummy_d;
                if (!p.parse_string(dummy_s) && !p.parse_number(dummy_d)) {
                    // 如果是 object 或 array，跳过到匹配的 } / ]
                    p.skip_ws();
                    if (p.peek_char('{') || p.peek_char('[')) {
                        // 简单跳过：用栈匹配
                        std::vector<char> stack;
                        char c = p.pos() < json.size() ? json[p.pos()] : '\0';
                        stack.push_back(c);
                        // advance past opening
                        // (consume the { or [)
                        if (c == '{') p.parse_object_start();
                        else p.parse_array_start();
                        while (!stack.empty() && !p.eof()) {
                            p.skip_ws();
                            if (p.eof()) return false;
                            char ch = json[p.pos()];
                            if (ch == '"') {
                                std::string s;
                                if (!p.parse_string(s)) return false;
                            } else if (ch == '{' || ch == '[') {
                                stack.push_back(ch);
                                if (ch == '{') p.parse_object_start();
                                else p.parse_array_start();
                            } else if (ch == '}') {
                                if (stack.back() != '{') return false;
                                stack.pop_back();
                                p.parse_object_end();
                            } else if (ch == ']') {
                                if (stack.back() != '[') return false;
                                stack.pop_back();
                                p.parse_array_end();
                            } else {
                                double d;
                                if (!p.parse_number(d)) {
                                    // 跳过单字符（true/false/null 等）
                                    p.advance();
                                }
                            }
                        }
                    } else {
                        return false;
                    }
                }
            }
            if (p.peek_char(',')) { p.parse_comma(); continue; }
            break;
        }
        if (!p.parse_object_end()) return false;
        return true;
    } catch (...) {
        return false;
    }
}

bool fingerprint_matches(const DeviceFingerprintView& a,
                         const DeviceFingerprintView& b) noexcept {
    // 优先比较 sha256（profile_generator 写入的是真正 SHA-256 hex）
    if (!a.sha256.empty() && !b.sha256.empty()) {
        return a.sha256 == b.sha256;
    }
    // 退化：直接比较关键字段（用于 routing 独立场景）
    return a.cpu_model == b.cpu_model &&
           a.cpu_cores == b.cpu_cores &&
           a.isa_mask == b.isa_mask &&
           a.gpu_name == b.gpu_name &&
           a.gpu_memory_bytes == b.gpu_memory_bytes &&
           a.gpu_driver_version == b.gpu_driver_version;
}

DeviceFingerprintView query_current_fingerprint() {
    DeviceFingerprintView fp;
    std::string hw = generate_hardware_report();
    // 简单抽取（与 profile_generator.cpp 中的 extract_json_field 一致逻辑）
    auto extract_str = [&](const std::string& key) -> std::string {
        std::string pat = "\"" + key + "\":\"";
        auto pos = hw.find(pat);
        if (pos == std::string::npos) return "";
        pos += pat.size();
        auto end = hw.find('"', pos);
        if (end == std::string::npos) return "";
        return hw.substr(pos, end - pos);
    };
    auto extract_num = [&](const std::string& key) -> std::string {
        std::string pat = "\"" + key + "\":";
        auto pos = hw.find(pat);
        if (pos == std::string::npos) return "";
        pos += pat.size();
        while (pos < hw.size() && (hw[pos] == ' ' || hw[pos] == '\t')) ++pos;
        auto end = pos;
        while (end < hw.size() &&
               (std::isdigit(static_cast<unsigned char>(hw[end])) ||
                hw[end] == '.' || hw[end] == '-' || hw[end] == '+')) {
            ++end;
        }
        return hw.substr(pos, end - pos);
    };
    fp.cpu_model = extract_str("model");
    if (fp.cpu_model.empty()) fp.cpu_model = extract_str("cpu_model");
    std::string cores = extract_num("cores");
    if (cores.empty()) cores = extract_num("cpu_cores");
    if (!cores.empty()) {
        try { fp.cpu_cores = static_cast<std::uint32_t>(std::stoul(cores)); } catch (...) {}
    }
    std::string isa = extract_num("mask");
    if (isa.empty()) isa = extract_num("isa_mask");
    if (!isa.empty()) {
        try { fp.isa_mask = std::stoull(isa); } catch (...) {}
    }
    fp.gpu_name = extract_str("gpu_name");
    if (fp.gpu_name.empty()) fp.gpu_name = extract_str("name");
    std::string vmem = extract_num("total_memory");
    if (vmem.empty()) vmem = extract_num("gpu_memory_bytes");
    if (!vmem.empty()) {
        try { fp.gpu_memory_bytes = std::stoull(vmem); } catch (...) {}
    }
    fp.gpu_driver_version = extract_str("driver_version");
    // routing 模块不依赖 qualification（避免循环依赖），不计算 SHA-256。
    // fingerprint_matches 会走关键字段比较分支（sha256 为空时）。
    return fp;
}

// ===== StaticRouteResolver::Impl =====
struct StaticRouteResolver::Impl {
    std::mutex mtx;
    std::string profile_path{"./routes.json"};
    std::atomic<bool> loaded{false};
    std::atomic<bool> load_attempted{false};
    RouteProfile profile;
    ProfileState state{ProfileState::Missing};
    bool stale{false};
    bool corrupt{false};
    bool missing{false};

    void load_locked() {
        if (load_attempted.load(std::memory_order_acquire)) return;
        load_attempted.store(true, std::memory_order_release);
        // 尝试读取文件
        std::ifstream f(profile_path, std::ios::in);
        if (!f.is_open()) {
            missing = true;
            state = ProfileState::Missing;
            return;
        }
        std::ostringstream oss;
        oss << f.rdbuf();
        std::string json = oss.str();
        RouteProfile parsed;
        if (!parse_route_profile(json, parsed)) {
            corrupt = true;
            state = ProfileState::Corrupt;
            return;
        }
        if (parsed.schema_version.empty() ||
            parsed.schema_version != "acr.route_profile.v1") {
            corrupt = true;
            state = ProfileState::Corrupt;
            return;
        }
        // 指纹比较
        DeviceFingerprintView current = query_current_fingerprint();
        bool fp_match = fingerprint_matches(parsed.fingerprint, current);
        profile = std::move(parsed);
        if (!fp_match) {
            stale = true;
            state = ProfileState::Stale;
        } else {
            stale = false;
            state = ProfileState::Valid;
        }
        loaded.store(true, std::memory_order_release);
    }
};

StaticRouteResolver::StaticRouteResolver()
    : impl_(std::make_unique<Impl>()) {}
StaticRouteResolver::~StaticRouteResolver() = default;

void StaticRouteResolver::set_profile_path(const std::string& path) {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    if (impl_->load_attempted.load(std::memory_order_acquire)) return;
    impl_->profile_path = path;
}

void StaticRouteResolver::invalidate_cache() {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->loaded.store(false, std::memory_order_release);
    impl_->load_attempted.store(false, std::memory_order_release);
    impl_->state = ProfileState::Missing;
    impl_->stale = false;
    impl_->corrupt = false;
    impl_->missing = false;
    impl_->profile = RouteProfile{};
}

RouteResolution StaticRouteResolver::resolve(KernelId kid, const std::string& precision) {
    RouteResolution r;
    std::lock_guard<std::mutex> lk(impl_->mtx);
    impl_->load_locked();

    const std::uint32_t id = static_cast<std::uint32_t>(kid);
    r.profile_state = impl_->state;
    r.stale = impl_->stale;
    r.corrupt = impl_->corrupt;
    r.missing = impl_->missing;

    if (impl_->missing) {
        r.backend = "cpu";
        r.reason = "missing-profile";
        std::fprintf(stderr,
            "[ACR] 警告：未标定，使用 CPU baseline（请运行 acr-benchmark 生成 routes.json）\n");
        return r;
    }
    if (impl_->corrupt) {
        r.backend = "cpu";
        r.reason = "corrupt";
        std::fprintf(stderr,
            "[ACR] 警告：profile 损坏，使用 CPU baseline（routes.json 解析失败）\n");
        return r;
    }
    if (impl_->stale) {
        std::fprintf(stderr,
            "[ACR] 警告：profile 过期（设备指纹不匹配），继续运行（不强制重新 benchmark）\n");
        // 继续用 profile 路由（不回退 CPU）
    }

    // 在 routes 中查找
    for (const auto& e : impl_->profile.routes) {
        if (e.kernel_id == id && (precision.empty() || e.precision == precision)) {
            r.backend = e.preferred_backend;
            r.reason = impl_->stale ? "stale" : "profile";
            return r;
        }
    }
    // 未找到路由：回退 CPU
    r.backend = "cpu";
    r.reason = "fallback";
    return r;
}

ProfileState StaticRouteResolver::current_state() const noexcept {
    return impl_->state;
}

const RouteProfile* StaticRouteResolver::loaded_profile() const noexcept {
    if (!impl_->loaded.load(std::memory_order_acquire)) return nullptr;
    return &impl_->profile;
}

std::string StaticRouteResolver::status_json() const {
    std::lock_guard<std::mutex> lk(impl_->mtx);
    std::ostringstream os;
    os << "{";
    os << "\"profile_state\":\"" << profile_state_str(impl_->state) << "\"";
    os << ",\"profile_path\":\"" << impl_->profile_path << "\"";
    os << ",\"loaded\":" << (impl_->loaded.load(std::memory_order_acquire) ? "true" : "false");
    os << ",\"stale\":" << (impl_->stale ? "true" : "false");
    os << ",\"missing\":" << (impl_->missing ? "true" : "false");
    os << ",\"corrupt\":" << (impl_->corrupt ? "true" : "false");
    if (impl_->loaded.load(std::memory_order_acquire)) {
        os << ",\"routes_count\":" << impl_->profile.routes.size();
        os << ",\"schema_version\":\"" << impl_->profile.schema_version << "\"";
        os << ",\"generated_at\":\"" << impl_->profile.generated_at << "\"";
        os << ",\"profile_kind\":\"" << impl_->profile.profile_kind << "\"";
    }
    os << "}";
    return os.str();
}

} // namespace astro::compute::routing
