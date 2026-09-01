// lib/backend_host/hardware_inspect.cpp — 硬件画像采集实现 — BENCH-001
// Linux: /proc/cpuinfo + sched_getaffinity + /sys topology/cache/node + cgroup v2 + uname。
// Windows: Job Object/affinity 分支(随 WIN/FAT 域实测)。
#include "hardware_inspect.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "backend_loader.h"
#include "cpu_features.h"
#include "sha256.h"
#include "sha256.h"

#if defined(__x86_64__)
#include <cpuid.h>
#include <x86intrin.h>
#elif defined(_M_X64)
#include <intrin.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#else
#include <sched.h>
#include <sys/utsname.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace astrocs::backend_host {

namespace {

std::string read_file_trim(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::stringstream ss;
    ss << f.rdbuf();
    std::string s = ss.str();
    while (!s.empty() && (s.back() == '\n' || s.back() == ' ' || s.back() == '\r')) s.pop_back();
    return s;
}

#if !defined(_WIN32)
/* /proc/cpuinfo 首个处理器块的键值 */
std::string cpuinfo_field(const std::string& key) {
    std::ifstream f("/proc/cpuinfo");
    std::string line;
    while (std::getline(f, line)) {
        const auto pos = line.find(':');
        if (pos != std::string::npos && line.substr(0, pos).find(key) != std::string::npos) {
            std::string v = line.substr(pos + 1);
            while (!v.empty() && (v.front() == ' ' || v.front() == '\t')) v.erase(v.begin());
            if (!v.empty() && !v.empty()) return v;
        }
        if (line.rfind("\n", 0) == 0) break;
    }
    return {};
}
#endif

#if defined(__x86_64__) || defined(_M_X64)
/* OSXSAVE 探测后读取 XCR0(06 §2); 未启用返 0。GCC/Clang 用 __builtin_ia32_xgetbv(需 -mxsave),
   MSVC 用 _xgetbv。 */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((target("xsave")))
#endif
static unsigned long long read_xcr0_impl() {
#if defined(__x86_64__)
    return __builtin_ia32_xgetbv(0);
#else
    return _xgetbv(0);
#endif
}
#endif

uint64_t xcr0_cached() {
#if defined(__x86_64__) || defined(_M_X64)
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
#if defined(__x86_64__)
    if (!__get_cpuid_count(1, 0, &eax, &ebx, &ecx, &edx) || !(ecx & (1u << 27))) return 0;
#else
    int info[4];
    __cpuidex(info, 1, 0);
    ecx = static_cast<unsigned int>(info[2]);
    edx = static_cast<unsigned int>(info[3]);
    if (!(ecx & (1u << 27))) return 0;
#endif
    return read_xcr0_impl();
#else
    return 0;
#endif
}

#if defined(_M_X64)
/* Windows/MSVC CPUID 身份(06 §2): vendor(leaf0 EBX+EDX+ECX) / brand(0x80000002-4) / family-model-stepping(leaf1) */
static std::string cpu_vendor_msvc() {
    int r[4]; __cpuidex(r, 0, 0);
    char v[13] = {0};
    std::memcpy(v, &r[1], 4);      // EBX
    std::memcpy(v + 4, &r[3], 4);  // EDX
    std::memcpy(v + 8, &r[2], 4);  // ECX
    return std::string(v);
}
static std::string cpu_brand_msvc() {
    char b[49] = {0};
    for (unsigned int leaf = 0x80000002; leaf <= 0x80000004; ++leaf) {
        int r[4]; __cpuidex(r, static_cast<int>(leaf), 0);
        std::memcpy(b + (leaf - 0x80000002) * 16, r, 16);
    }
    std::string s(b);
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
}
static void cpu_family_model_msvc(int* fam, int* mod, int* step) {
    int r[4]; __cpuidex(r, 1, 0);
    const unsigned int eax = static_cast<unsigned int>(r[0]);
    const int base = (eax >> 8) & 0xF, ext = (eax >> 20) & 0xFF;
    const int bmod = (eax >> 4) & 0xF, emod = (eax >> 16) & 0xF;
    *fam = (base == 0xF) ? base + ext : base;
    *mod = (base == 0xF) ? ((emod << 4) | bmod) : bmod;
    *step = static_cast<int>(eax & 0xF);
}
#endif

uint64_t mem_total_bytes() {
#if defined(_WIN32)
    MEMORYSTATUSEX s{};
    s.dwLength = sizeof(s);
    GlobalMemoryStatusEx(&s);
    return s.ullTotalPhys;
#else
    std::ifstream f("/proc/meminfo");
    std::string line;
    while (std::getline(f, line))
        if (line.rfind("MemTotal:", 0) == 0) {
            long kb = 0;
            if (std::sscanf(line.c_str(), "MemTotal: %ld kB", &kb) == 1)
                return static_cast<uint64_t>(kb) * 1024ull;
        }
    return 0;
#endif
}

}  // namespace

std::string hardware_inspect_json_v1(const std::string& build_id) {
    nlohmann::json j;
    // ── affinity/逻辑 CPU(06 §2: 有效 affinity, 禁 nproc 单独决定) ──
#if defined(_WIN32)
    DWORD_PTR proc = 0, sys = 0;
    uint64_t affinity_mask = 0;
    if (GetProcessAffinityMask(GetCurrentProcess(), &proc, &sys)) affinity_mask = proc;
    std::vector<int> aff;
    for (int i = 0; i < 64; ++i)
        if (affinity_mask & (1ull << i)) aff.push_back(i);
#else
    cpu_set_t set;
    CPU_ZERO(&set);
    sched_getaffinity(0, sizeof(set), &set);
    std::vector<int> aff;
    for (int i = 0; i < CPU_SETSIZE; ++i)
        if (CPU_ISSET(i, &set)) aff.push_back(i);
#endif
    const uint32_t aff_count = static_cast<uint32_t>(aff.size());
    // cgroup v2 cpu.max(quota/period)→ 有效上限; 空文件=max
    uint32_t cgroup_limit = 0;
    const std::string cmax = read_file_trim("/sys/fs/cgroup/cpu.max");
    if (!cmax.empty() && cmax.rfind("max", 0) != 0) {
        double quota = 0, period = 0;
        if (std::sscanf(cmax.c_str(), "%lf %lf", &quota, &period) == 2 && period > 0)
            cgroup_limit = static_cast<uint32_t>(quota / period + 0.999);
    }
    // cgroup v1 回退: cpu.cfs_quota_us / cpu.cfs_period_us（v2 无 cpu.max 时探测）
    if (cgroup_limit == 0) {
        const std::string cq = read_file_trim("/sys/fs/cgroup/cpu/cpu.cfs_quota_us");
        const std::string cp = read_file_trim("/sys/fs/cgroup/cpu/cpu.cfs_period_us");
        if (!cq.empty() && cq != "-1" && !cp.empty()) {
            double quota = 0, period = 0;
            if (std::sscanf(cq.c_str(), "%lf", &quota) == 1 &&
                std::sscanf(cp.c_str(), "%lf", &period) == 1 && period > 0 && quota > 0)
                cgroup_limit = static_cast<uint32_t>(quota / period + 0.999);
        }
    }
    uint32_t avail = aff_count;
    if (cgroup_limit > 0 && cgroup_limit < avail) avail = cgroup_limit;   // ∩ 约束

    // ── CPU 身份 ──
#if !defined(_WIN32)
    const std::string vendor = cpuinfo_field("vendor_id");
    const std::string model_name = cpuinfo_field("model name");
    const std::string microcode = cpuinfo_field("microcode");
    int family = 0, model = 0, stepping = 0;
    std::sscanf(cpuinfo_field("cpu family").c_str(), "%d", &family);
    std::sscanf(cpuinfo_field("model").c_str(), "%d", &model);
    std::sscanf(cpuinfo_field("stepping").c_str(), "%d", &stepping);
#else
#if defined(_M_X64)
    const std::string vendor = cpu_vendor_msvc();
    const std::string model_name = cpu_brand_msvc();
    const std::string microcode;   // MSVC 侧无易得途径 → 空
    int family = 0, model = 0, stepping = 0;
    cpu_family_model_msvc(&family, &model, &stepping);
#else
    const std::string vendor, model_name, microcode;
    int family = 0, model = 0, stepping = 0;
#endif
#endif

    const uint64_t feats = astrocs_cpu_detect_features_v1();
    static const struct { uint64_t bit; const char* name; } kFeat[] = {
        {ACS_FEAT_SSE2, "sse2"}, {ACS_FEAT_SSE4_1, "sse4_1"}, {ACS_FEAT_AVX, "avx"},
        {ACS_FEAT_AVX2, "avx2"}, {ACS_FEAT_FMA, "fma"}, {ACS_FEAT_AVX512F, "avx512f"}};
    nlohmann::json feat_names = nlohmann::json::array();
    for (const auto& f : kFeat)
        if (feats & f.bit) feat_names.push_back(f.name);

    // ── cache 层级(cpu0 可读时) ──
    nlohmann::json caches = nlohmann::json::array();
#if !defined(_WIN32)
    for (int idx = 0; idx < 4; ++idx) {
        const std::string base =
            "/sys/devices/system/cpu/cpu0/cache/index" + std::to_string(idx);
        const std::string size = read_file_trim(base + "/size");
        if (size.empty()) break;
        nlohmann::json c;
        c["level"] = read_file_trim(base + "/level");
        c["type"] = read_file_trim(base + "/type");
        c["size"] = size;
        caches.push_back(c);
    }
#endif

    // ── NUMA/SMT(可读时) ──
    uint32_t numa_nodes = 0;
#if !defined(_WIN32)
    for (int n = 0; n < 64; ++n) {
        std::error_code ec;
        if (!fs::exists(fs::u8path("/sys/devices/system/node/node" + std::to_string(n)), ec))
            break;
        ++numa_nodes;
    }
#endif
    const std::string sib = read_file_trim(
        "/sys/devices/system/cpu/cpu0/cache/index0/shared_cpu_list");  // 可读性探测
    const bool smt_known = !sib.empty();
    const bool smt = smt_known && sib.find(',') != std::string::npos;

    // ── OS/构建/hash ──
    nlohmann::json os = {{"name",
#if defined(_WIN32)
                          "windows"
#else
                          "linux"
#endif
                         }};
#if !defined(_WIN32)
    struct utsname u;
    if (uname(&u) == 0) {
        os["kernel"] = u.release;
        os["machine"] = u.machine;
    }
#endif
    std::string cli_hash;
#if !defined(_WIN32)
    char exe[4096] = {0};
    const ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n > 0) {
        exe[n] = 0;
        bool ok = false;
        cli_hash = file_sha256_hex(exe);
        (void)ok;
    }
#else
    char exe[MAX_PATH] = {0};
    const DWORD n = GetModuleFileNameA(NULL, exe, (DWORD)sizeof(exe));
    if (n > 0 && n < sizeof(exe)) cli_hash = file_sha256_hex(exe);
#endif

    long page = 4096;
#if defined(_WIN32)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    page = si.dwPageSize;
#else
    page = sysconf(_SC_PAGESIZE);
#endif

    j["schema_version"] = 1;
    j["kind"] = "astrocs_hardware_inspect";
    j["architecture"] = "amd64";
    j["vendor"] = vendor;
    j["brand"] = model_name;
    j["family"] = family;
    j["model"] = model;
    j["stepping"] = stepping;
    j["microcode"] = microcode;
    j["feature_bits"] = feats;
    j["feature_names"] = feat_names;
    j["xcr0"] = xcr0_cached();
    j["logical_cpus_configured"] =
#if defined(_WIN32)
        0;
#else
        static_cast<uint32_t>(sysconf(_SC_NPROCESSORS_CONF));
#endif
    j["affinity"] = aff;
    j["available_logical_cpus"] = avail;          // affinity∩cgroup(06 §2 硬性)
    j["affinity_count"] = aff_count;
    j["cgroup_cpu_limit"] = cgroup_limit;         // 0=无显式限制
    j["job_object_limit"] = nullptr;              // Windows Job Object(WIN/FAT 域)
    // CPU-002: quota signature = 有效配额状态的指纹(affinity∩cgroup/job; 禁硬编码)。
    {
        astrocs::crypto::Sha256 qh;
        const std::string qsrc =
            std::to_string(aff_count) + "|" + std::to_string(cgroup_limit) +
            "|" + std::to_string(avail) + "|" + std::to_string(feats) + "|" +
            std::to_string(xcr0_cached());
        qh.update(qsrc.data(), qsrc.size());
        j["quota_signature"] = qh.final_hex();
    }
    j["physical_packages"] = nullptr;   // 拓扑受限容器不可读; 可得时填充
    j["smt"] = {{"known", smt_known}, {"enabled", smt}};
    j["numa_nodes"] = numa_nodes;
    j["ram_bytes"] = mem_total_bytes();
    j["page_size"] = page;
    j["cache"] = caches;
    j["os"] = os;
    j["compiler"] = {
        {"id",
#if defined(__clang__)
         "clang"
#elif defined(_MSC_VER)
         "msvc"
#else
         "gcc"
#endif
        },
        {"version",
#if defined(__clang__)
         __clang_version__
#elif defined(__GNUC__)
         "gcc-" __VERSION__
#elif defined(_MSC_VER)
         "msvc"
#endif
        }};
    j["astrocs_build"] = build_id;
    j["cli_sha256"] = cli_hash;
    j["backend_hashes"] = nlohmann::json::array();   // 与 backends.manifest.json 联动(ABI-002)
    return j.dump(2) + "\n";
}

}  // namespace astrocs::backend_host
