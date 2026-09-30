// AIO-001 · 唯一 AIO C ABI v1 与内容哈希复核 · 测试执行器 (单执行器 + 组注册 + 故障注入)
//
// 控制包任务: AIO-001 (ACSD-CONSTITUTION-ALIGNMENT-V1)。
// 合同锚: lib/include/acsd/io/aio_abi_v1.h + eng/contracts/data/aio_abi_contract_v1.json。
// 模式对齐先例: lib/infrastructure/aio/tests/p1hips/p1hips_test_main.hpp
// (FaultRegistry + 组 runner + note_injected 一次性报告)。
//
// 单跑: ./aio_abi_tests units|negative|selfcheck|all
//
// 故障注入 (验收: "故障注入能让测试失败" + 无恒 PASS 占位):
//   ACSD_AIO_FAULT=<regname>[,<regname>...]
//   n1_hash_value_flip            -> units: sha256 重算输出翻转 → 已知向量必败
//   n2_verify_mismatch_shortcut   -> negative: verify_buffer 恒 OK → 篡改必败
//   n3_file_size_skip             -> negative: verify_file 跳过 size 核对 → 截断必败
#ifndef AIO_ABI_TEST_MAIN_HPP
#define AIO_ABI_TEST_MAIN_HPP

#include "acsd/io/aio_abi_v1.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace aio_abi_test {

// ───────── 故障注册表 (进程级; main 启动时由 ACSD_AIO_FAULT 初始化) ─────────
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

inline void init_fault_registry_from_env() {
    const char* env = std::getenv("ACSD_AIO_FAULT");
    if (!env || !*env) return;
    std::string s(env);
    for (std::size_t pos = 0; pos < s.size();) {
        std::size_t comma = s.find(',', pos);
        if (comma == std::string::npos) comma = s.size();
        if (comma > pos) FaultRegistry::instance().active.push_back(s.substr(pos, comma - pos));
        pos = comma + 1;
    }
}

struct CheckState {
    int failures = 0;
    bool fault_reported = false;
    void note_injected(const char* name) {
        if (fault_reported) return;
        std::fprintf(stderr, "FAULT-INJECT %s (deterministic failure injection)\n", name);
        fault_reported = true;
    }
};

// 故障名的可注入性判定（**运行期**）。
//
// 旧写法是 (faultname) != nullptr: 本仓全部调用点都传字符串字面量, 字面量地址
// 永不为 null ⇒ 该比较编译期恒真, 「故障名不可用」这条失败路径**从未被执行过**
//（恒真门没有证据资格, AGENTS.md §5）。改为对**故障名内容**的运行期判定, 并把
// 注入与否交给注册表实际查询（不是对字面量地址的常量比较）:
//   * nullptr = 明确的「本断言不属于任何注入点」约定（注入分支不生效, 断言照常求值）;
//   * 空串名 = 注入注册错误（永远匹配不到任何注入名）⇒ **判失败**; 该路径可被触发,
//     证据见本套件 selfcheck 的 guard-path 相。
inline bool fault_name_usable(const char* name) {
    return name != nullptr && name[0] != '\0';
}

#define AIO_CHECK(cs, cond, faultname)                                          \
    do {                                                                        \
        const char* const fn_ = (faultname);                                   \
        if (fn_ != nullptr && !::aio_abi_test::fault_name_usable(fn_)) {       \
            std::fprintf(stderr, "CHECK failed %s:%d: empty fault name\n",     \
                         __FILE__, __LINE__);                                  \
            ++(cs).failures;                                                   \
        } else if (::aio_abi_test::FaultRegistry::instance().injected(fn_)) {  \
            (cs).note_injected(fn_);                                           \
            ++(cs).failures;                                                    \
        } else if ((cs).fault_reported && ::aio_abi_test::fault_name_usable(fn_)) { \
            /* 已注入本组: 后续具名 CHECK 全部计为失败 (测试必败) */                              \
            ++(cs).failures;                                                   \
        } else if (!(cond)) {                                                   \
            std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++(cs).failures;                                                    \
        }                                                                       \
    } while (0)

#define AIO_CHECK_MSG(cs, cond, faultname, ...)                                 \
    do {                                                                        \
        const char* const fn_ = (faultname);                                   \
        if (fn_ != nullptr && !::aio_abi_test::fault_name_usable(fn_)) {       \
            std::fprintf(stderr, "CHECK failed %s:%d: empty fault name\n",     \
                         __FILE__, __LINE__);                                  \
            ++(cs).failures;                                                   \
        } else if (::aio_abi_test::FaultRegistry::instance().injected(fn_)) {  \
            (cs).note_injected(fn_);                                           \
            ++(cs).failures;                                                    \
        } else if ((cs).fault_reported && ::aio_abi_test::fault_name_usable(fn_)) { \
            /* 已注入本组: 后续具名 CHECK 全部计为失败 (测试必败) */                              \
            ++(cs).failures;                                                   \
        } else if (!(cond)) {                                                   \
            std::fprintf(stderr, "CHECK failed %s:%d: %s — ", __FILE__, __LINE__, #cond); \
            std::fprintf(stderr, __VA_ARGS__);                                  \
            std::fprintf(stderr, "\n");                                         \
            ++(cs).failures;                                                    \
        }                                                                       \
    } while (0)

// ───────── 测试组 (实现于 aio_abi_tests.cpp) ─────────
int test_units(CheckState& cs);
int test_negative(CheckState& cs);

// 声明格式校验 (64 字符小写 [0-9a-f]; negative 复用)
int aio_abi_test_decl_valid(const char* s);

inline int run_group(const char* group) {
    CheckState cs;
    int rc = 0;
    if (std::strcmp(group, "units") == 0) {
        rc = test_units(cs);
    } else if (std::strcmp(group, "negative") == 0) {
        rc = test_negative(cs);
    } else {
        std::fprintf(stderr, "unknown group '%s'\n", group);
        return 127;
    }
    if (rc != 0) cs.failures += 1;
    if (cs.failures != 0) {
        std::fprintf(stderr, "AIO-ABI %s FAIL (%d failure(s))\n", group, cs.failures);
        return 1;
    }
    std::fprintf(stdout, "AIO-ABI %s PASS\n", group);
    return 0;
}

}  // namespace aio_abi_test

// main 入口 (全局; AIO_ABI_TEST_NO_MAIN 时不定义 main, selfcheck TU 提供自己的)
#ifndef AIO_ABI_TEST_NO_MAIN
int main(int argc, char** argv);
#endif
int aio_abi_tests_main(int argc, char** argv);

#endif /* AIO_ABI_TEST_MAIN_HPP */
