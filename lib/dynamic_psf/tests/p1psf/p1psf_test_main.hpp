// ============================================================================
// P1-PSF-TEST · 测试执行器框架 (模板 <prefix>-TEST 单测组 + 故障注入)
// ----------------------------------------------------------------------------
// 单跑: ./p1psf_tests units|properties|oracle|negative|boundary|all
// CTest: p1psf_units / p1psf_properties / p1psf_oracle / p1psf_negative /
//        p1psf_boundary
//
// 故障注入 (模板 <prefix>-TEST 验收: "故障注入能让测试失败"):
//   ASTROCS_P1PSF_FAULT=<regname>[,<regname>...] → 对应 CHECK 组确定性翻转
//   → rc=1 + stderr "FAULT-INJECT <name>"。注册表 (与 core/perf 实际
//   faultname 一致):
//     recovery, identity, worker_bitwise, abi_consistency,
//     negative_matrix, nan_semantics, oracle_recovery, perf
//   (p1psf_tests_selfcheck 以 fork+execve 注入跑注入相, 验证必败。)
// ============================================================================
#ifndef P1PSF_TEST_MAIN_HPP
#define P1PSF_TEST_MAIN_HPP

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace p1psf {

inline int g_p1psf_pass = 0;
inline int g_p1psf_fail = 0;

// ---------------------------------------------------------------------------
// 共享工具 (core/perf 双 TU 复用): 批量结果扁平化 + bitwise 等值
// ---------------------------------------------------------------------------
// DPSFFitResult* → [N,9] 行主序 (与 psf_params schema 布局一致)
inline std::vector<double> flatten9(const void* results_c, int n) {
    const auto* r = static_cast<const DPSFFitResult*>(results_c);
    std::vector<double> out((size_t)n * 9);
    for (int i = 0; i < n; ++i) {
        out[(size_t)i * 9 + 0] = r[i].B;
        out[(size_t)i * 9 + 1] = r[i].A;
        out[(size_t)i * 9 + 2] = r[i].cx;
        out[(size_t)i * 9 + 3] = r[i].cy;
        out[(size_t)i * 9 + 4] = r[i].sx;
        out[(size_t)i * 9 + 5] = r[i].sy;
        out[(size_t)i * 9 + 6] = r[i].theta;
        out[(size_t)i * 9 + 7] = r[i].fwhm_x;
        out[(size_t)i * 9 + 8] = r[i].fwhm_y;
    }
    return out;
}

inline bool vec_bitwise_eq(const std::vector<double>& a, const std::vector<double>& b) {
    return a.size() == b.size() &&
           std::memcmp(a.data(), b.data(), a.size() * sizeof(double)) == 0;
}

// 故障注入 CHECK: faultname 非空且被注入时, 该断言确定性翻转 (测试必败)
struct FaultRegistry {
    static FaultRegistry& instance() {
        static FaultRegistry r;
        return r;
    }
    std::vector<std::string> active;

    bool injected(const char* name) const {
        if (name == nullptr) return false;
        for (const auto& s : active)
            if (s == name) return true;
        return false;
    }
};

struct CheckState {
    int failures = 0;
    bool fault_reported = false;
    void note_injected(const char* name) {
        if (fault_reported) return;
        std::fprintf(stderr,
                     "FAULT-INJECT %s (deterministic failure injection)\n",
                     name);
        fault_reported = true;
    }
};

#define P1PSF_CHECK(cs, cond, faultname)                                  \
    do {                                                                  \
        if ((faultname) != nullptr &&                                     \
            p1psf::FaultRegistry::instance().injected(faultname)) {       \
            (cs).note_injected(faultname);                                \
            ++(cs).failures;                                              \
        } else if (!(cond)) {                                             \
            std::fprintf(stderr, "CHECK failed %s:%d: %s\n",              \
                         __FILE__, __LINE__, #cond);                      \
            ++(cs).failures;                                              \
        }                                                                 \
    } while (0)

// 等值 CHECK (整型/状态码断言: 实际值 vs 期望值, 计入组内 failures)
#define P1PSF_CHECK_EQ(cs, got, want)                                     \
    do {                                                                  \
        const long long _g = (long long)(got);                            \
        const long long _w = (long long)(want);                           \
        if (_g != _w) {                                                   \
            std::fprintf(stderr, "CHECK_EQ failed %s:%d: got=%lld want=%lld\n", \
                         __FILE__, __LINE__, _g, _w);                     \
            ++(cs).failures;                                              \
        }                                                                 \
    } while (0)

// 带消息 CHECK (科学断言: 阈值引用 ALG/README 段落 + 实测值)
#define P1PSF_CHECK_MSG(cs, cond, faultname, ...)                        \
    do {                                                                  \
        char _m[512];                                                     \
        std::snprintf(_m, sizeof(_m), __VA_ARGS__);                       \
        if ((faultname) != nullptr &&                                     \
            p1psf::FaultRegistry::instance().injected(faultname)) {       \
            (cs).note_injected(faultname);                                \
            ++(cs).failures;                                              \
        } else if (!(cond)) {                                             \
            std::fprintf(stderr, "CHECK failed %s:%d: %s | %s\n",         \
                         __FILE__, __LINE__, #cond, _m);                  \
            ++(cs).failures;                                              \
        }                                                                 \
    } while (0)

struct TestGroup {
    const char* name;
    int (*fn)(void);
};

inline int run_all_groups(const p1psf::TestGroup* groups, std::size_t n,
                          int argc, char** argv) {
    std::string group = "all";
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--group" && i + 1 < argc) group = argv[++i];
        else if (a.rfind("--", 0) != 0) group = a;
    }
    if (const char* f = std::getenv("ASTROCS_P1PSF_FAULT")) {
        // 同进程重入 (selfcheck 注入相) 防御: 先清空上次注册, 避免 active
        // 跨相累加 (execve 路径天然全新进程, 此行为纯防御)
        p1psf::FaultRegistry::instance().active.clear();
        std::string s = f;
        std::size_t pos = 0;
        while (pos < s.size()) {
            const std::size_t comma = s.find(',', pos);
            const std::string tok =
                s.substr(pos, (comma == std::string::npos ? s.size() : comma) - pos);
            if (!tok.empty())
                p1psf::FaultRegistry::instance().active.push_back(tok);
            if (comma == std::string::npos) break;
            pos = comma + 1;
        }
    }
    int total_fail = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (group != "all" && group != groups[i].name) continue;
        std::fprintf(stdout, "[p1psf] group %s ...\n", groups[i].name);
        std::fflush(stdout);
        const int rc = groups[i].fn();
        if (rc != 0) {
            std::fprintf(stderr, "[p1psf] group %s FAIL rc=%d\n",
                         groups[i].name, rc);
            total_fail += 1;
        } else {
            std::fprintf(stdout, "[p1psf] group %s PASS\n", groups[i].name);
        }
    }
    std::fprintf(stdout, "[p1psf] == P1-PSF-TEST: %d 通过, %d 失败 (组级) ==\n",
                 (int)n - total_fail, total_fail);
    return total_fail == 0 ? 0 : 1;
}

}  // namespace p1psf

#endif  // P1PSF_TEST_MAIN_HPP
