// cli_bench_verdict_test.cpp — B8-P1-2: benchmark_profile_verdict 推导共址单测。
// 语义: 任一 kernel correctness_test != "oracle:pass" → FAIL(含缺失/空/非对象);
// 全部 oracle:pass → PASS。fixture 与 tests/cli/test_bench_cli.py 集成断言互补:
// 真 CLI 全 fail 路径的 exit≠0(→SCIENCE=4)由 dispatch 调用本函数实现。
#include "cli_common.h"

#include <cstdio>
#include <string>

static int fail_count = 0;
#define CHECK(cond, msg)                                                 \
    do {                                                                 \
        if (!(cond)) {                                                   \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, \
                         (msg));                                         \
            ++fail_count;                                                \
        }                                                                \
    } while (0)

int main() {
    using nlohmann::json;
    using astrocs::benchmark_profile_verdict;

    // ── 1. 全 pass fixture → PASS ──
    {
        const json d = json::parse(R"({
            "schema": "astrocs.cpu-profile/v2",
            "kernels": {
                "calibration-pixel-transform": {"correctness_test": "oracle:pass"},
                "upm-spmv": {"correctness_test": "oracle:pass"}
            }
        })");
        CHECK(benchmark_profile_verdict(d) == "PASS", "全部 oracle:pass → PASS");
    }
    // ── 2. 全 fail fixture → FAIL ──
    {
        const json d = json::parse(R"({
            "schema": "astrocs.cpu-profile/v2",
            "kernels": {
                "calibration-pixel-transform": {"correctness_test": "oracle:fail",
                                                 "fallback_reason": "no passing provider"}
            }
        })");
        CHECK(benchmark_profile_verdict(d) == "FAIL", "全部 oracle:fail → FAIL");
    }
    // ── 3. 单 kernel fail(混合) → FAIL(正确性筛选优先, 不静默放行) ──
    {
        const json d = json::parse(R"({
            "kernels": {
                "a": {"correctness_test": "oracle:pass"},
                "b": {"correctness_test": "oracle:fail"}
            }
        })");
        CHECK(benchmark_profile_verdict(d) == "FAIL", "任一 oracle:fail → FAIL");
    }
    // ── 4. correctness_test 缺失 → FAIL(无正确性证据不判成功) ──
    {
        const json d = json::parse(R"({"kernels": {"a": {}}})");
        CHECK(benchmark_profile_verdict(d) == "FAIL", "correctness_test 缺失 → FAIL");
    }
    // ── 5. kernels 空/缺失/非对象 → FAIL ──
    {
        CHECK(benchmark_profile_verdict(json::parse(R"({"kernels": {}})")) == "FAIL",
              "kernels 空 → FAIL");
        CHECK(benchmark_profile_verdict(json::parse(R"({"schema": "x"})")) == "FAIL",
              "kernels 缺失 → FAIL");
        CHECK(benchmark_profile_verdict(json::parse(R"({"kernels": []})")) == "FAIL",
              "kernels 非对象 → FAIL");
        CHECK(benchmark_profile_verdict(json::array()) == "FAIL",
              "profile 非对象 → FAIL");
    }
    // ── 6. verdict 字段可回写(输出 JSON 顶层登记语义) ──
    {
        json d = json::parse(R"({"kernels": {"a": {"correctness_test": "oracle:pass"}}})");
        const std::string v = benchmark_profile_verdict(d);
        d["verdict"] = v;
        CHECK(d.value("verdict", "") == "PASS", "顶层 verdict 字段可登记");
    }

    if (fail_count) {
        std::fprintf(stderr, "cli_bench_verdict_test: %d FAIL\n", fail_count);
        return 1;
    }
    std::fprintf(stderr, "cli_bench_verdict_test: all PASS\n");
    return 0;
}
