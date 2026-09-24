// cli_bench_verdict_test.cpp — B8-P1-2: benchmark_profile_verdict 推导共址单测。
// 语义: 任一 kernel correctness_test != "oracle:pass" → FAIL(含缺失/空/非对象);
// 全部 oracle:pass → PASS。fixture 与 eng/tests/cli/test_bench_cli.py 集成断言互补:
// 真 CLI 全 fail 路径的 exit≠0(→SCIENCE=4)由 dispatch 调用本函数实现。
#include "cli_common.h"
#include "protocol.h"   // FIX-405 G3-10: §4 kind 注册表硬闸（ValidateEventV1）

#include <cstdio>
#include <string>
#include <vector>

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


    // ── 7. FIX-405 G3-10: §4 kind 注册表硬闸（10 类全登记；未登记 kind 拒发） ──
    // 读侧独立重实现扩展字段名册（与 lib/infrastructure/cli/protocol.h 互为对偶，
    // 防同源盲区）；实现正本 = protocol.h registered_event_kinds_v1()。
    {
        using astrocs::ValidateEventV1;
        using astrocs::is_registered_event_kind_v1;
        struct KindExt { const char* kind; std::vector<std::string> ext; };
        const std::vector<KindExt> kRegistry = {
            {"progress", {"completed", "total", "unit", "rate", "eta_seconds"}},
            {"resource", {"cpu_cores_used", "rss_bytes", "io_read_bytes", "io_write_bytes",
                          "threads"}},
            {"artifact", {"role", "path", "sha256", "size_bytes"}},
            {"backend", {"kernel", "backend_id", "isa", "workers", "block_size", "reason"}},
            {"final", {"exit_code", "status", "run_manifest", "summary"}},
            {"stage_start", {}},
            {"stage_end", {}},
            {"graph", {"path"}},
            {"resource_gate", {"diag", "enforcement", "strict", "enforced",
                               "work_core_seconds", "workload_floor_core_seconds",
                               "workload_floor_reached",
                               "so05_signoff_id", "so05_signoff_status",
                               "auto_adjudication_allowed"}},
            {"v6_mode_route", {"route_kind", "token", "surface", "source", "reason",
                               "implicit_phase_chain", "budget_source_owner",
                               "budget_allocated_cores", "one_budget_source_rule"}},
        };
        CHECK(kRegistry.size() == 10, "§4 kind 注册表必须是 10 类");
        for (const auto& ke : kRegistry)
            CHECK(is_registered_event_kind_v1(ke.kind),
                  (std::string("实现正本缺 kind: ") + ke.kind).c_str());
        CHECK(!is_registered_event_kind_v1("bogus_kind"),
              "未登记 kind 不得被判为已登记");

        auto base_event = [](const std::string& kind) {
            json ev = {{"schema_version", "1"},
                       {"event_id", "evt-0123456789ab-0"},
                       {"run_id", "0123456789ab"},
                       {"timestamp_utc", "2026-01-01T00:00:00Z"},
                       {"sequence", 0},
                       {"kind", kind},
                       {"severity", "info"},
                       {"phase", "normalize"},
                       {"stage", "n/a"},
                       {"message", "m"}};
            return ev;
        };
        for (const auto& ke : kRegistry) {
            json ev = base_event(ke.kind);
            for (const auto& f : ke.ext) {
                if (f == "exit_code") ev[f] = 0;
                else if (f == "run_manifest" || f == "sha256" || f == "size_bytes" ||
                         f == "rate" || f == "eta_seconds") ev[f] = nullptr;
                else if (f == "strict" || f == "enforced" ||
                         f == "workload_floor_reached" ||
                         f == "implicit_phase_chain") ev[f] = false;
                else if (f == "workers" || f == "block_size" || f == "completed" ||
                         f == "total" || f == "cpu_cores_used" || f == "rss_bytes" ||
                         f == "io_read_bytes" || f == "io_write_bytes" ||
                         f == "threads" || f == "work_core_seconds" ||
                         f == "workload_floor_core_seconds" ||
                         f == "budget_allocated_cores") ev[f] = 0;
                else ev[f] = "x";
            }
            CHECK(ValidateEventV1(ev, 0),
                  (std::string("登记 kind 的合规事件必须放行: ") + ke.kind).c_str());
            // 负例：去掉一个冻结扩展字段 ⇒ 必须拒发（能红）
            if (!ke.ext.empty()) {
                json bad = ev;
                bad.erase(ke.ext.front());
                CHECK(!ValidateEventV1(bad, 0),
                      (std::string("缺冻结扩展字段必须拒发: ") + ke.kind + "/" +
                       ke.ext.front()).c_str());
            }
        }
        // 负例：未登记 kind（10 必含字段齐全）⇒ 硬闸拒发
        {
            json ev = base_event("bogus_kind");
            CHECK(!ValidateEventV1(ev, 0), "未登记 kind 必须拒发（fail-closed）");
        }
    }

    if (fail_count) {
        std::fprintf(stderr, "cli_bench_verdict_test: %d FAIL\n", fail_count);
        return 1;
    }
    std::fprintf(stderr, "cli_bench_verdict_test: all PASS\n");
    return 0;
}
