// P1-WCS-TEST · 测试执行器框架 (单执行器 + 单测组注册 + 故障注入)
//
// 控制包任务: P1-WCS-TEST (lock-P1-WCS; 依赖 P1-WCS-DOC 闭环)。
// 合同锚: docs/algorithms/PLATESOLVE.md §11.4 TEST-WCS-DESIGN-001
// (P1-WCS-DOC 冻结, 2026-09-07, wave W1); 矩阵行 P1-WCS
// (MOD astrocs.phase1.wcs-platesolve, TEST-P1-WCS-001)。
//
// 单跑方式 (每个测试组 == 独立 ctest 名, 二进制内按位置参数单跑):
//   ./p1wcs_tests units|properties|oracle|negative
//   ./p1wcs_tests all
//
// 故障注入 (模板 <prefix>-TEST 验收: "故障注入能让测试失败"):
//   双注入点 (对齐任务规格 "fork/execve 双注入点必败自检, argv 子进程模式"):
//   A) env:  ASTROCS_P1WCS_FAULT=<regname>[,<regname>...]
//   B) argv: ./p1wcs_tests <group> --fault=<regname>[,<regname>...]
//   每个注册 fault 使对应 CHECK 确定性翻转 → rc=1, 输出 "FAULT-INJECT <name>"。
//   可注入 registry 见 kP1wcsFaultNames[] (各 CHECK 第三参与此表对齐);
//   注入名示例: ASTROCS_P1WCS_FAULT=u1_f1_cd_relative ./p1wcs_tests units
// 模式对齐先例: lib/astro_image_io/tests/p1hips/p1hips_test_main.hpp
// (c19b4a59, FaultRegistry + 组 runner + note_injected 一次性报告)。
#ifndef P1WCS_TEST_MAIN_HPP
#define P1WCS_TEST_MAIN_HPP

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace p1wcs {

// ---------------------------------------------------------------------------
// 集中 fault 注册表: 全部注入名 (各测试 TU 的 P1WCS_CHECK/P1WCS_CHECK_NEAR
// 第三参必须取自本表, 新增注入点须同步)。selfcheck 双注入点取
// kP1wcsFaultNames[0] (units) 与 oracle 组注入名。
// ---------------------------------------------------------------------------
static const char* const kP1wcsFaultNames[] = {
    // units 组
    "u1_f1_cd_relative",      // units: F1 CD 相对误差断言
    "u1_f1_rms",              // units: F1 rms_arcsec 断言
    "u1_f1_crval",            // units: F1 |ΔCRVAL| 断言
    "u1_f3_crpix_exact",      // units: F3 CRPIX 精确断言
    "u1_f3_ydown_sign",       // units: F3 Y-down 第 2 列符号断言
    "u1_f6_roundtrip",        // units: F6 WcsTan roundtrip 断言
    "u1_f6_abs_cross",        // units: F6c WcsTan 绝对前向交叉锚 (B2-A1)
    "u1_boundary_n3",         // units: n=3 恰好边界断言
    // properties 组
    "p1_det_bitwise",         // properties: 同输入 3 次 bitwise 断言
    "p1_thread_bitwise",      // properties: 线程 1/2/4 bitwise 断言
    "p1_order2_linear",       // properties: order=2 对线性场二阶项≈0
    "p1_translate_invariant", // properties: triangle 平移不变性
    "p1_fixture_deterministic", // properties: fixture 同 seed 确定性
    // oracle 组
    "o1_gnomonic_roundtrip",  // oracle: gnomonic 正逆往返
    "o1_linear6_recover",     // oracle: 线性 6 参数恢复
    "o1_sip_expect",          // oracle: SIP 期望全链自洽
    "o1_wcs_forward_reverse", // oracle: WCS 前向/逆向往返
    "o1_f2_sip_vs_oracle",    // oracle: F2 被测 SIP vs oracle 期望
    "o1_f2_apbp_roundtrip",   // oracle: F2 AP/BP 逆向 roundtrip
    "o1_gnomonic_cross",      // oracle: gnomonic 闭式/向量双路径交叉 (WCS-001)
    "o1_apbp_reverse_oracle", // oracle: oracle-6 AP/BP 逆向独立实现断言 (WCS-001)
    // apbp 组 (WCS-003: AP/BP 布局扩展 + 消费方迭代式反演冻结门)
    "f2x_center90_freeze",      // apbp: 中心 90% 区域冻结门 <1e-4 px
    "f2x_boundary_freeze",      // apbp: 图像边界与角点冻结门 <1e-4 px
    "f2x_random_freeze",        // apbp: ≥1000 确定性随机点冻结门 <1e-4 px
    "f2x_truth_anchor",         // apbp: 真值锚全链冻结门 <1e-4 px
    "f2x_reject_deterministic", // apbp: 奇点/非有限/超限确定性拒绝
    "f2x_parity_workers",       // apbp: 1 worker / N worker bitwise
    "f2x_layout_expand",        // apbp: 布局扩展登记 + 一步防退化观察线
    "f2x_cross_export",         // apbp: 交叉验证输入导出
    // negative 组
    "n1_few_stars",           // negative: <3 星 success=0
    "n1_collinear_success",   // negative: 共线退化 success=0 (DISP-WCS-001)
    "n1_unmatched_offset",    // negative: 无对应场 success=0
    "n1_nan_input",           // negative: NaN/Inf 输入不崩
    "n1_zero_pairs_anchor",   // negative: 空对确定性路径锚 (WCS-002 翻锚:
                              //   三空对组合直接断言零 UB + 确定 success=0)
    "n1_wcs_tan_degenerate",  // negative: WcsTan det 退化坍缩行为锚
    "n1_wcs_tan_unit_anchor", // negative: WcsTan ξ/η 单位**绝对正确性锚**
                              //   (B2-A1: 原缺陷行为锚翻转为 ≤1e-9 deg)
    "n1_wcs_tan_bridge_declared", // negative: WcsTan +1 桥接声明用法 (B2-A1)
    "n1_wcs_tan_bridge_removed",  // negative: 去桥接注入必被检出 (B2-A1)
    "n1_wcs_tan_bridge_doubled",  // negative: 双桥接注入必被检出 (B2-A1)
    "n1_extract_zero_trans",  // negative: extract_wcs_sip 全零 trans 行为锚
    // perf 组
    "perf_baseline",          // perf: 基线时长为正
    "perf_parity_2t",         // perf: parity T(2T)/T(1T) 上界
    "perf_trend_4t",          // perf: trend T(4T)/T(1T) 上下界
};

struct FaultRegistry {
    static FaultRegistry& instance() {
        static FaultRegistry r;
        return r;
    }
    // 由 init_fault_registry_from_env / argv 解析填充
    std::vector<std::string> active;

    bool injected(const char* name) const {
        if (name == nullptr) return false;
        for (const auto& s : active)
            if (s == name) return true;
        return false;
    }
};

// 从 env (ASTROCS_P1WCS_FAULT, 逗号分隔) 初始化注入名单。
// selfcheck execve 重入路径必须显式调用 (先例 c19b4a59 "registry 初始化" 教训)。
inline void init_fault_registry_from_env() {
    if (const char* f = std::getenv("ASTROCS_P1WCS_FAULT")) {
        std::string s = f;
        std::size_t pos = 0;
        while (pos < s.size()) {
            const std::size_t comma = s.find(',', pos);
            const std::string tok =
                s.substr(pos, (comma == std::string::npos ? s.size() : comma) - pos);
            if (!tok.empty()) FaultRegistry::instance().active.push_back(tok);
            if (comma == std::string::npos) break;
            pos = comma + 1;
        }
    }
}

// 从 "--fault=a,b" 形式 argv token 追加注入名单 (注入点 B)。
inline void add_faults_from_arg(const std::string& arg) {
    const std::string prefix = "--fault=";
    if (arg.rfind(prefix, 0) != 0) return;
    std::string s = arg.substr(prefix.size());
    std::size_t pos = 0;
    while (pos < s.size()) {
        const std::size_t comma = s.find(',', pos);
        const std::string tok =
            s.substr(pos, (comma == std::string::npos ? s.size() : comma) - pos);
        if (!tok.empty()) FaultRegistry::instance().active.push_back(tok);
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
}

struct CheckState {
    int failures = 0;
    bool fault_reported = false;
    // 故障注入报告: 每个 fault 名只在首个 CHECK 触发一次
    void note_injected(const char* name) {
        if (fault_reported) return;
        std::fprintf(stderr, "FAULT-INJECT %s (deterministic failure injection)\n", name);
        fault_reported = true;
    }
};

#define P1WCS_CHECK(cs, cond, faultname)                                   \
    do {                                                                   \
        if ((cs).fault_reported && (faultname) != nullptr) {               \
            /* 已注入本组: 后续 CHECK 全部计为失败 (测试必败) */            \
            ++(cs).failures;                                               \
        } else if (faultname != nullptr &&                                 \
                   p1wcs::FaultRegistry::instance().injected(faultname)) { \
            (cs).note_injected(faultname);                                 \
            ++(cs).failures;                                               \
        } else if (!(cond)) {                                              \
            std::fprintf(stderr, "CHECK failed %s:%d: %s\n",               \
                         __FILE__, __LINE__, #cond);                       \
            ++(cs).failures;                                               \
        }                                                                  \
    } while (0)

#define P1WCS_CHECK_NEAR(cs, got, want, tol, faultname)                    \
    do {                                                                   \
        const double g_ = (got), w_ = (want), t_ = (tol);                  \
        if (p1wcs::FaultRegistry::instance().injected(faultname)) {        \
            (cs).note_injected(faultname);                                 \
            ++(cs).failures;                                               \
        } else if (!((g_ - w_) <= t_ && (w_ - g_) <= t_)) {                \
            std::fprintf(stderr,                                        \
                         "CHECK failed %s:%d: %s got=%.12g want=%.12g tol=%.3g\n", \
                         __FILE__, __LINE__, #got, g_, w_, t_);            \
            ++(cs).failures;                                               \
        }                                                                  \
    } while (0)

struct TestGroup {
    const char* name;
    int (*fn)(void);
};

// 全链 main 框架: 解析组名/--fault → 跑组 → rc
inline int run_all_groups(const p1wcs::TestGroup* groups, std::size_t n, int argc, char** argv) {
    p1wcs::init_fault_registry_from_env();
    std::string group = "all";
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--group" && i + 1 < argc) group = argv[++i];
        else if (a.rfind("--fault=", 0) == 0) p1wcs::add_faults_from_arg(a);
        else if (a.rfind("--", 0) != 0) group = a;
    }
    int total_fail = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (group != "all" && group != groups[i].name) continue;
        std::fprintf(stdout, "[p1wcs] group %s ...\n", groups[i].name);
        std::fflush(stdout);
        const int rc = groups[i].fn();
        if (rc != 0) {
            std::fprintf(stderr, "[p1wcs] group %s FAIL rc=%d\n", groups[i].name, rc);
            total_fail += 1;
        } else {
            std::fprintf(stdout, "[p1wcs] group %s PASS\n", groups[i].name);
        }
        std::fflush(stdout);
    }
    if (total_fail == 0) {
        std::fprintf(stdout, "P1WCS TESTS PASS (group=%s)\n", group.c_str());
        return 0;
    }
    std::fprintf(stderr, "P1WCS TESTS FAIL (%d group(s) failed, group=%s)\n", total_fail, group.c_str());
    return 1;
}

}  // namespace p1wcs

#endif  // P1WCS_TEST_MAIN_HPP
