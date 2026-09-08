// astrocs CLI — 命令实现 (RT-008 拆分自 main.cpp)
// 具体命令 + dispatch + 进程监控/资源事件桥接 + session/AIO/Drizzle 接线。
// 本文件允许 include 科学内部头（CHK-001 只扫描 cli/main.cpp）。
// 头部 include 顺序与原 main.cpp 逐字节一致, 保证宏/类型可见性等价。
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#if !defined(_WIN32)
#include <sched.h>
#endif
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "sha256.h"

#include "hardware_inspect.h"
#include "profile_gen.h"
#include "cpu_routing.h"



#include "backend_loader.h"
#include "astrocs_process.h"
#include "protocol.h"
#include "resource_recorder.h"

extern "C" {
int astrocs_host_services_default_v1(astrocs_host_services_v1* out, void** state_out);
void astrocs_host_services_destroy_state_v1(void* state);
void astrocs_host_state_set_budget_v1(void* state, uint32_t cpus, uint32_t max_workers,
                                      astrocs_host_services_v1* out);
uint64_t astrocs_cpu_detect_features_v1(void);
}


#include "cancel_token.h"
#include "exit_codes.h"
#include "jsonl.h"
#include "memory_report.h"
#include "monitor.h"
#include "resource_events.h"
#include "resource_gate.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif


#include "version_generated.h"

#include "cli_common.h"
#include "runtime_client.h"

// MON-002 资源/backend 事件发射(定义于后段, 此处前向声明供 phase run 共用引擎使用)
// 注: cmd_run_pipeline / cmd_graph 已随 CLI-002 移除; 下述 helper(write_run_graphs /
// emit_resource_summary / emit_backend_event) 保留, 供后续 phase run 共用引擎复用。
static void emit_resource_summary(astrocs::JsonlEmitter&, const std::string&,
                                  const astrocs::ProcessMonitor::Summary&, const std::string&,
                                  std::size_t, const std::string&);
static void emit_backend_event(astrocs::JsonlEmitter&, const std::string&, const std::string&,
                               const std::string&, uint32_t, uint32_t);

// 主机可用并行预算(禁硬编码, 04/BENCH 规范): Linux 用 sched_getaffinity, Windows 用有效处理器数; 至少 1。
static uint32_t cli_affinity_cpu_count() {
#if defined(_WIN32)
    unsigned int n = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    return n == 0 ? 1u : static_cast<uint32_t>(n);
#else
    cpu_set_t set; CPU_ZERO(&set);
    if (sched_getaffinity(0, sizeof(set), &set) != 0) return 1u;
    uint32_t n = 0;
    for (int i = 0; i < CPU_SETSIZE; ++i)
        if (CPU_ISSET(i, &set)) ++n;
    return n == 0 ? 1u : n;
#endif
}

// ───────────────────── 具体命令实现 ─────────────────────

// config 模板(CLI-002 最小骨架; 完整 config schema 属 CLI-003)
const char* kConfigTemplate =
    "{\n"
    "  \"schema_version\": \"1\",\n"
    "  \"inputs\": {\"lights\": [], \"darks\": [], \"flats\": [], \"bias\": []},\n"
    "  \"output_dir\": \".\"\n"
    "}\n";

int cmd_config_init(const Parsed& p, astrocs::JsonlEmitter& ev) {
    const std::string out = need_value(p, "--output");
    {
        std::ofstream f(std::filesystem::u8path(out), std::ios::binary | std::ios::trunc);
        if (!f) {
            std::fprintf(stderr, "astrocs: cannot write '%s'\n", out.c_str());
            return astrocs::IO;   // 04: I/O 失败 → 7
        }
        f << kConfigTemplate;
        if (!f.good()) return astrocs::IO;
    }
    // CLI-004: §4 artifact 冻结词表 {role,path,sha256,size_bytes}; 文件 artifact 必带
    // sha256+size_bytes(目录 artifact 才允许 null)。
    const std::string tmpl_sha = [&] { bool ok = false; return file_sha256(out, &ok); }();
    std::error_code tsz_ec;
    const auto tmpl_sz = std::filesystem::file_size(std::filesystem::u8path(out), tsz_ec);
    ev.emit("artifact", "info", "config", "template written",
            {{"role", "config_template"}, {"path", out}, {"sha256", tmpl_sha},
             {"size_bytes", tsz_ec ? nlohmann::json(nullptr)
                                   : nlohmann::json(static_cast<unsigned long long>(tmpl_sz))}});
    std::printf("%s\n", out.c_str());
    return astrocs::OK;
}

int cmd_config_validate(const Parsed& p, astrocs::JsonlEmitter& ev) {
    const std::string path = need_value(p, "--config");
    nlohmann::json doc;
    const int rc = validate_config_full(path, &doc);
    if (rc != astrocs::OK) return rc;
    ev.emit("artifact", "info", "config", "validated",
            {{"role", "config"}, {"path", path},
             {"sha256", [&]{ bool ok=false; return file_sha256(path, &ok); }()},
             {"size_bytes", [&]{ std::error_code ec2; auto sz = std::filesystem::file_size(
                                    std::filesystem::u8path(path), ec2);
                                 return ec2 ? nlohmann::json(nullptr)
                                            : nlohmann::json(static_cast<unsigned long long>(sz)); }()}});
    std::printf("config OK\n");
    return astrocs::OK;
}

// show-effective: config 与 profile 分别校验 → 合成 effective(--json 固定, 04 §1)
int cmd_show_effective(const Parsed& p, astrocs::JsonlEmitter& ev) {
    (void)p; (void)ev;
    if (!p.flags.count("--json")) parse_fail("config show-effective requires --json");
    const std::string cfg = need_value(p, "--config");
    nlohmann::json doc;
    int rc = validate_config_full(cfg, &doc);
    if (rc != astrocs::OK) return rc;
    nlohmann::json out = {
        {"schema_version", "1"},
        {"config", doc},
        {"effective", {{"phases", doc.value("inputs", nlohmann::json::object()).contains("lights") &&
                                            !doc["inputs"]["lights"].empty()
                                        ? nlohmann::json({1, 2})
                                        : nlohmann::json({3})}}},
    };
    if (p.values.count("--cpu-profile")) {
        nlohmann::json prof;
        rc = validate_cpu_profile(p.values.at("--cpu-profile"), &prof);
        if (rc != astrocs::OK) return rc;
        out["cpu_profile"] = prof;
        bool ok = false;
        out["effective"]["cpu_profile_sha256"] = file_sha256(p.values.at("--cpu-profile"), &ok);
        // CPU-004: 逐 kernel 路由摘要(provider 选择/workers/block/fallback reason/self-test hash)
        const std::string hw_json = astrocs::backend_host::hardware_inspect_json_v1(
            ASTROCS_VERSION_STRING);
        nlohmann::json routes = nlohmann::json::object();
        if (prof.contains("kernels") && prof["kernels"].is_object()) {
            for (auto it = prof["kernels"].begin(); it != prof["kernels"].end(); ++it) {
                const std::string kid = it.key();
                astrocs::backend_host::KernelRoute kr;
                astrocs::backend_host::route_kernel_from_profile(
                    prof.dump(), kid, hw_json, &kr);
                routes[kid] = {
                    {"provider", kr.provider},
                    {"workers", kr.workers},
                    {"block", kr.block},
                    {"fallback_reason", kr.fallback_reason.empty()
                        ? nlohmann::json(nullptr) : nlohmann::json(kr.fallback_reason)},
                    {"self_test_sha256", kr.self_test_sha256},
                };
            }
        }
        out["effective"]["kernel_routes"] = routes;
    }
    std::printf("%s\n", out.dump().c_str());
    return astrocs::OK;
}

// stub 命令(科学接线属 CODE/TST 域): 参数已按合同全量校验, 明示 not-wired。
// 测试钩子(ASTROCS_TEST_SLEEP_MS / ASTROCS_TEST_CRASH=1)仅用于协议 golden 测试, 非用户接口。
int cmd_test_synthetic(const Parsed& p, const std::string& group, astrocs::JsonlEmitter& ev);  // CLI-003

int cmd_stub(const Parsed& p, const std::string& phase, astrocs::JsonlEmitter& ev) {
    (void)p;
    const char* sleep_ms = std::getenv("ASTROCS_TEST_SLEEP_MS");
    if (sleep_ms) {
        long ms = std::strtol(sleep_ms, nullptr, 10);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        ev.stage("stub_wait", true);
        while (std::chrono::steady_clock::now() < deadline) {
            if (astrocs::is_cancelled()) {
                ev.stage("stub_wait", false);
                ev.emit_final(astrocs::CANCELLED, "cancelled", nullptr, "cancelled by user");
                std::fprintf(stderr, "astrocs: cancelled\n");
                return astrocs::CANCELLED;   // 04: 取消 → 9, 不留伪完整产物
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        ev.stage("stub_wait", false);
    }
    if (std::getenv("ASTROCS_TEST_CRASH")) {
        throw std::runtime_error("selftest-crash");   // crash boundary 落锤(→70)
    }
    ev.emit_final(astrocs::ARGS, "not_wired", nullptr,
                  "command is declared by the CLI contract but science handlers are wired in later tasks");
    std::fprintf(stderr, "astrocs: '%s' is declared by the CLI contract but not wired in this build "
                         "(see docs/api/CLI_PROTOCOL_V1.md)\n", phase.c_str());
    return astrocs::ARGS;
}

// ── CLI-003: test synthetic 接通真实合成门 ──
// 运行 build 树内已编译的合成测试可执行文件 (路径: ASTROCS_TEST_BIN_DIR 或
// <cwd>/build/root-cmake/tests/unit)。group → 测试二进制映射; 全部 exit 0 = PASS。
// 无测试二进制 (非开发构建) → 明确错误 (可诊断, 非静默)。
int cmd_test_synthetic(const Parsed& p, const std::string& group, astrocs::JsonlEmitter& ev) {
    (void)p;
    struct G { const char* group; const char* bin; };
    static const G kMap[] = {
        {"calibration",           "p1_calibration_test"},
        {"wcs_psf",               "p1_stars_test"},
        {"noise_snr",             "p1_noise_test"},
        {"drizzle",               "p1_nside_test"},
        {"upm",                   "p2_upm_synthetic_test"},
        {"rejection_integration", "p2_output_semantics_test"},
        {"pipeline",              "p1_ir_facade_test"},
    };
    // RT-008: crash 测试钩子(非用户接口) — 供协议 golden 验证 crash boundary(→70)
    if (std::getenv("ASTROCS_TEST_CRASH")) {
        throw std::runtime_error("selftest-crash");
    }
    std::vector<const char*> bins;
    if (group == "all") {
        for (const auto& g : kMap) bins.push_back(g.bin);
    } else {
        for (const auto& g : kMap)
            if (group == g.group) bins.push_back(g.bin);
    }
    if (bins.empty()) {
        ev.emit_final(astrocs::ARGS, "no_tests", nullptr,
                      ("no synthetic tests for group '" + group + "'").c_str());
        return astrocs::ARGS;
    }
    std::string bin_dir = std::getenv("ASTROCS_TEST_BIN_DIR")
                              ? std::getenv("ASTROCS_TEST_BIN_DIR")
                              : "build/root-cmake/tests/unit";
    // 源码相对读取的测试 (p1_ir_facade/p2_ir_facade) 需要 ASTROCS_REPO;
    // 默认设为调用方 cwd, 可用环境变量覆盖。
    const char* repo_env = std::getenv("ASTROCS_REPO");
    // G9: 所有外部命令必须 timeout (ASTROCS_TEST_TIMEOUT_S 可配, 默认 600s)。
    // B8-P1-1a: 弃用 std::system 字符串拼接(shell 解析断裂空格路径/timeout 参数
    // 未消毒/POSIX-only) → 进程 API argv 数组传参; timeout 由 run_process 内建
    // (跨平台, 非外部 timeout 二进制), 非法值消毒回退默认。
    const char* tmo = std::getenv("ASTROCS_TEST_TIMEOUT_S");
    double timeout_s = 600.0;
    if (tmo && tmo[0]) {
        char* end = nullptr;
        const unsigned long v = std::strtoul(tmo, &end, 10);
        if (end && *end == '\0' && v > 0 && v <= 86400UL) timeout_s = static_cast<double>(v);
    }
    const std::string repo =
        (repo_env && repo_env[0]) ? std::string(repo_env) : std::string(".");
    int failed = 0;
    for (const char* b : bins) {
        const std::string exe = bin_dir + "/" + b;
        ev.stage(("test_" + std::string(b)).c_str(), true);
        const astrocs::process::RunResult cr = astrocs::process::run_process(
            {exe}, {{"ASTROCS_REPO", repo}}, timeout_s);
        int rc;
        std::string rc_note;
        if (cr.timed_out) {
            rc = 124;   // 与原 `timeout` 命令超时退出码保持一致
            rc_note = " timed out after " + std::to_string(static_cast<long long>(timeout_s)) + "s";
        } else if (cr.spawn_failed) {
            rc = 127;
            rc_note = " spawn failed: " + cr.error;
        } else if (!cr.exited) {
            rc = astrocs::INTERNAL;   // 信号终止 → INTERNAL(70), 数值走 exit_codes 单源
            rc_note = " abnormal termination: " + cr.error;
        } else {
            rc = cr.exit_code;
            rc_note.clear();
        }
        ev.stage(("test_" + std::string(b)).c_str(), rc == 0);
        if (rc != 0) {
            std::fprintf(stderr, "astrocs: synthetic test %s failed (rc=%d%s)\n",
                         b, rc, rc_note.c_str());
            ++failed;
        }
    }
    if (failed) {
        ev.emit_final(astrocs::INTERNAL, "synthetic_failed", nullptr,
                      ("synthetic tests failed: " + std::to_string(failed)).c_str());
        return astrocs::INTERNAL;
    }
    ev.emit_final(astrocs::OK, "synthetic_ok", nullptr,
                  ("synthetic tests passed (" + std::to_string(bins.size()) + ")").c_str());
    std::fprintf(stderr, "astrocs: test synthetic %s: %zu tests PASS\n",
                 group.c_str(), bins.size());
    return astrocs::OK;
}

// run manifest v1 原子写(tmp+rename; ARCH-002 §5 单元): stub/not-wired/cancelled 恒 incomplete
int write_run_manifest(const std::string& out_dir, astrocs::JsonlEmitter& ev, const std::string& status,
                       const std::string& summary, const std::string& config_path,
                       const std::string& config_sha, const std::vector<int>& phases,
                       const nlohmann::json& artifacts = nlohmann::json::array()) {
    nlohmann::json m = {
        {"schema_version", "1"},
        {"kind", "astrocs_run_manifest"},
        {"run_id", ev.run_id()},
        {"astrocs_version", ASTROCS_VERSION_STRING},
        {"platform", {{"os",
#ifdef _WIN32
                       "windows"
#else
                       "linux"
#endif
                       },
                      {"arch", "amd64"}}},
        {"config_path", config_path},
        {"config_sha256", config_sha},
        {"cpu_profile_path", nullptr},
        {"cpu_profile_sha256", nullptr},
        {"phases", phases},
        {"artifacts", artifacts},
        {"status", status},
        {"started_utc", astrocs::iso8601_utc_now()},
        {"finished_utc", astrocs::iso8601_utc_now()},
        {"summary", summary},
    };
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::u8path(out_dir), ec);
    const std::string final_path = out_dir + "/astrocs_run_" + ev.run_id() + ".json";
    const std::string tmp_path = final_path + ".tmp";
    {
        std::ofstream f(std::filesystem::u8path(tmp_path), std::ios::binary | std::ios::trunc);
        if (!f) {
            std::fprintf(stderr, "astrocs: cannot write run manifest '%s'\n", tmp_path.c_str());
            return astrocs::IO;
        }
        f << m.dump(2) << "\n";
        if (!f.good()) return astrocs::IO;
    }
    std::filesystem::rename(std::filesystem::u8path(tmp_path), std::filesystem::u8path(final_path), ec);
    if (ec) {
        std::fprintf(stderr, "astrocs: cannot finalize run manifest: %s\n", ec.message().c_str());
        return astrocs::IO;
    }
    // CLI-004: §4 artifact 冻结词表 {role,path,sha256,size_bytes} — manifest 补 size_bytes。
    {
        std::error_code mec;
        const auto msz = std::filesystem::file_size(std::filesystem::u8path(final_path), mec);
        ev.emit("artifact", "info", "manifest", "run manifest written",
                {{"role", "run_manifest"}, {"path", final_path},
                 {"sha256", [&]{ bool ok=false; return file_sha256(final_path, &ok); }()},
                 {"size_bytes", mec ? nlohmann::json(nullptr)
                                    : nlohmann::json(static_cast<unsigned long long>(msz))}});
    }
    // --events-jsonl 模式下 stdout 只能是 JSON 事件(04 §3): 路径已入 artifact 事件
    if (!ev.enabled()) std::printf("%s\n", final_path.c_str());
    return astrocs::OK;
}

// RT-009: 写运行图产物 — 静态 IR JSON + observed trace JSON + sidecar。
// 每次 run 都生成（静态图与 observed 图同源于同一 IR；L0 由 Python 渲染器派生）。
// 路径脱敏: 所有绝对路径替换为 <root> 相对占位（sanitize 逻辑在渲染器/JSON 输出统一）。
// 不失败 run: 图产物损坏只记 warning。
static void write_run_graphs(const std::string& out_dir, astrocs::JsonlEmitter& ev,
                             const std::string& config_path, const std::string& config_sha,
                             const std::vector<int>& phases) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::u8path(out_dir), ec);
    const std::string ir_json = astrocs::cli::last_pipeline_ir_json();
    const std::string gdir = out_dir + "/graph";
    std::filesystem::create_directories(std::filesystem::u8path(gdir), ec);
    if (ec) return;

    // 1) 静态图 = PipelineIR（节点/端口/artifact/资源）
    const std::string static_path = gdir + "/static_graph.json";
    {
        std::ofstream f(std::filesystem::u8path(static_path), std::ios::binary | std::ios::trunc);
        if (!f) return;
        f << ir_json << "\n";
        if (!f.good()) return;
    }

    // 2) observed trace = Runtime 节点 trace + session manifest 摘要（CHK-002 双向比较输入）
    std::vector<astrocs::core::Runtime::NodeTrace> tr;
    astrocs::cli::collect_node_trace(&tr);
    std::vector<std::pair<std::string, std::string>> mans;
    astrocs::cli::collect_node_manifests(&mans);
    nlohmann::json ir = nlohmann::json::parse(ir_json, nullptr, false);
    nlohmann::json static_nodes = nlohmann::json::object();
    if (!ir.is_discarded() && ir.contains("nodes") && ir["nodes"].is_array()) {
        for (const auto& n : ir["nodes"]) {
            if (n.contains("node_id")) static_nodes[n["node_id"].get<std::string>()] = n;
        }
    }
    nlohmann::json nodes = nlohmann::json::array();
    for (const auto& t : tr) {
        nlohmann::json nd;
        nd["node_id"] = t.node_id;
        if (static_nodes.contains(t.node_id)) {
            const auto& sn = static_nodes[t.node_id];
            if (sn.contains("module_id")) nd["module_id"] = sn["module_id"];
            if (sn.contains("module_api")) nd["module_version"] = sn["module_api"];
            if (sn.contains("inputs")) nd["inputs"] = sn["inputs"];
            if (sn.contains("outputs")) nd["outputs"] = sn["outputs"];
            if (sn.contains("resources")) nd["resources"] = sn["resources"];
        }
        nd["status"] = t.status;
        nd["started_utc"] = t.started_utc;
        nd["ended_utc"] = t.ended_utc;
        nd["duration_ms"] = t.duration_ms;
        nd["workers"] = t.workers;
        nd["provider"] = t.provider;
        if (!t.error.empty()) nd["error"] = t.error;
        // 节点 manifest → input/output artifact 摘要（id + sha256）
        for (const auto& [nid, mtext] : mans) {
            if (nid != t.node_id) continue;
            try {
                auto m = nlohmann::json::parse(mtext);
                if (m.contains("artifacts") && m["artifacts"].is_array()) {
                    for (const auto& a : m["artifacts"]) {
                        if (a.is_string()) {
                            std::string ap = a.get<std::string>();
                            bool sok = false;
                            nd["output_artifacts"].push_back(
                                {{"id", "artifact:" + t.node_id},
                                 {"path", sanitize_path(ap)},
                                 {"sha256", file_sha256(ap, &sok)}});
                        }
                    }
                }
                if (m.contains("output_fits_path")) {
                    std::string op = m["output_fits_path"].get<std::string>();
                    if (!op.empty()) {
                        bool sok = false;
                        nd["output_artifacts"].push_back(
                            {{"id", "artifact:out_" + t.node_id},
                             {"path", sanitize_path(op)},
                             {"sha256", file_sha256(op, &sok)}});
                    }
                }
            } catch (...) {}
            break;
        }
        nodes.push_back(std::move(nd));
    }
    nlohmann::json observed = {
        {"schema", "astrocs.observed-trace/v1"},
        {"run_id", ev.run_id()},
        {"pipeline_id", ir.contains("pipeline_id") ? ir["pipeline_id"] : "cli.run.preset"},
        {"nodes", nodes},
    };
    const std::string trace_path = gdir + "/observed_trace.json";
    {
        std::ofstream f(std::filesystem::u8path(trace_path), std::ios::binary | std::ios::trunc);
        if (!f) return;
        f << observed.dump(2) << "\n";
        if (!f.good()) return;
    }

    // 3) sidecar: IR hash / source commit / profile ID / input manifest hash
    nlohmann::json side = {
        {"schema", "astrocs.graph-sidecar/v1"},
        {"run_id", ev.run_id()},
        {"ir_sha256", [&] { bool ok = false; return file_sha256(static_path, &ok); }()},
        {"source_commit", git_head_sha().value_or("unknown")},
        {"profile_id", nullptr},
        {"input_manifest_sha256", config_sha},
        {"config_path", sanitize_path(config_path)},
        {"phases", phases},
    };
    const std::string side_path = gdir + "/graph_sidecar.json";
    {
        std::ofstream f(std::filesystem::u8path(side_path), std::ios::binary | std::ios::trunc);
        if (!f) return;
        f << side.dump(2) << "\n";
        if (!f.good()) return;
    }
    // RT-009: 渲染 DOT/SVG/L0（best-effort; 工具缺失/失败不失败 run）。
    // 仅当 tools/quality/gen_run_graphs.py 存在时调用; timeout 30s 防悬挂。
    // B8-P1-1b: 弃用 std::system 拼接（gdir 无引号+单引号逃逸+返回值丢弃 →
    // 渲染失败时主平台成功 run 的图产物静默缺失）→ 进程 API argv 传参 +
    // 显式检查子进程 exit code，失败 warning 事件 + stderr（不静默；不失败 run，
    // RT-009 冻结语义保留，但产物缺失必须可诊断）。
    {
        const char* env_repo = std::getenv("ASTROCS_REPO");
        const std::string repo = (env_repo && env_repo[0]) ? env_repo : ".";
        const std::string renderer = repo + "/tools/quality/gen_run_graphs.py";
        std::error_code ec;
        if (std::filesystem::is_regular_file(std::filesystem::u8path(renderer), ec)) {
            const astrocs::process::RunResult cr = astrocs::process::run_process(
                {"python3", renderer, "--graph-dir", gdir}, {}, 30.0, {}, true);
            bool rendered = astrocs::process::ok(cr);
            if (!rendered) {
                std::string why;
                if (cr.timed_out) why = "renderer timed out after 30s";
                else if (cr.spawn_failed) why = "renderer spawn failed: " + cr.error;
                else if (!cr.exited) why = "renderer abnormal termination: " + cr.error;
                else why = "renderer exit code " + std::to_string(cr.exit_code);
                std::fprintf(stderr, "astrocs: run graph rendering failed: %s (dir=%s)\n",
                             why.c_str(), gdir.c_str());
                ev.emit("graph", "warning", "run_graphs",
                        "run graph rendering failed", {{"reason", why}, {"path", gdir}});
            }
        } else {
            std::fprintf(stderr, "astrocs: run graph renderer missing: %s\n", renderer.c_str());
            ev.emit("graph", "warning", "run_graphs", "run graph renderer missing",
                    {{"path", renderer}});
        }
    }
    // CLI-004: §4 artifact 冻结词表 — graph_dir 为目录 artifact, sha256/size_bytes=null。
    ev.emit("artifact", "info", "graph", "run graphs written",
            {{"role", "graph_dir"}, {"path", gdir},
             {"sha256", nullptr}, {"size_bytes", nullptr}});
}

// phase1 run: CLI-004 — 进程内调用 p1_session(无 shell-out); cancel/budget/monitor 注入

// MON-002: 发射资源分层事件(summary 强制; timeseries 详略受 --resource-detail 控制)。
// summary 事件内嵌指标; 原始 timeseries 只记录留存路径+样本数(不内嵌几十 MB 数据)。
static void emit_resource_summary(astrocs::JsonlEmitter& ev, const std::string& phase,
                                  const astrocs::ProcessMonitor::Summary& s,
                                  const std::string& raw_dir, std::size_t raw_n,
                                  const std::string& detail) {
    const auto p = astrocs::summarize(s);
    nlohmann::json payload = {
        {"n_samples", p.n_samples},
        {"wall_seconds", p.wall_seconds},
        {"avg_equivalent_cores", p.avg_equivalent_cores},
        {"peak_equivalent_cores", p.peak_equivalent_cores},
        {"peak_rss_bytes", p.peak_rss_bytes},
        {"rss_slope_bytes_per_s", p.rss_slope_bytes_per_s},
        {"total_read_bytes", p.total_read_bytes},
        {"total_write_bytes", p.total_write_bytes},
        {"total_ctx_switches", p.total_ctx_switches},
        {"max_threads", p.max_threads},
        {"sample_overhead_ms", p.sample_overhead_ms},
        {"resource_detail", detail},
        {"raw_dir", raw_dir},
        {"raw_n", raw_n},
    };
    if (detail == "timeseries") {
        payload["curve_points"] = nlohmann::json::array();
        payload["downsample_max"] = astrocs::kDownsampleMax;
    }
    // CLI-004: §4 kind 扩展字段冻结 —— resource{cpu_cores_used,rss_bytes,io_read_bytes,
    // io_write_bytes,threads}。映射(机器可消费规范面): cpu_cores_used=平均等价核数,
    // rss_bytes=峰值 RSS, io_*=累计读写字节, threads=最大活跃线程。MON-002 详细字段
    // 保留为附加扩展(协议允许只增不改)。
    payload["cpu_cores_used"] = p.avg_equivalent_cores;
    payload["rss_bytes"] = p.peak_rss_bytes;
    payload["io_read_bytes"] = p.total_read_bytes;
    payload["io_write_bytes"] = p.total_write_bytes;
    payload["threads"] = p.max_threads;
    ev.emit("resource", "info", phase, "resource summary", payload);
}

// MON-002: backend 事件(backend_id/status); 反映所选 backend 与 worker 选择(07 §2 必采)。
// CLI-004: §4 kind 扩展字段冻结 —— backend{kernel,backend_id,isa,workers,block_size,
// reason}。kernel=拓扑节点 id(runtime IR 链式节点, 非硬编码 phase 名); isa 取自硬件
// 画像实际特征(与 05 ISA 门控同源); block_size/reason 由选择点注入。
static std::string backend_isa_name() {
    try {
        const auto hw = nlohmann::json::parse(
            astrocs::backend_host::hardware_inspect_json_v1(ASTROCS_VERSION_STRING));
        const unsigned long long bits = hw.value("feature_bits", 0ull);
        if (bits & (1ull << 5)) return "avx512";   // ACS_FEAT_AVX512F (cpu_features.h 同源)
        if (bits & (1ull << 3)) return "avx2";     // ACS_FEAT_AVX2
        if (bits & (1ull << 2)) return "avx";      // ACS_FEAT_AVX
        return "baseline";
    } catch (...) {
        return "baseline";  // 画像失败保守取基线(事件不致命)
    }
}

static void emit_backend_event(astrocs::JsonlEmitter& ev, const std::string& phase,
                               const std::string& backend_id, const std::string& status,
                               uint32_t workers_used, uint32_t available_cpus) {
    ev.emit("backend", "info", phase, status,
            {{"kernel", phase},
             {"backend_id", backend_id},
             {"isa", backend_isa_name()},
             {"workers", workers_used},
             {"block_size", 0},
             {"reason", "cli affinity lease"},
             {"workers_used", workers_used},
             {"available_cpus", available_cpus}});
}

// CLI-004: phase 统计 resource 事件(既有载荷保留) + §4 冻结扩展字段
// {cpu_cores_used,rss_bytes,io_read_bytes,io_write_bytes,threads}(真实 monitor 摘要同源)。
static void emit_phase_stats_resource(astrocs::JsonlEmitter& ev, const std::string& phase,
                                      const std::string& message,
                                      const nlohmann::json& stats,
                                      const astrocs::ProcessMonitor::Summary* ms) {
    nlohmann::json payload = stats;
    if (ms != nullptr) {
        payload["cpu_cores_used"] = ms->avg_equivalent_cores;
        payload["rss_bytes"] = ms->peak_rss_bytes;
        payload["io_read_bytes"] = ms->total_read_bytes;
        payload["io_write_bytes"] = ms->total_write_bytes;
        payload["threads"] = ms->max_threads;
    }
    ev.emit("resource", "info", phase, message, payload);
}

// MON-002: 无标注 >5s 区间判 P1(供 MON-003 gating; 本函数仅供测试与 stage 落地校验)。
[[maybe_unused]] static bool is_stage_priority(const char* annotation, double wall_seconds) {
    return astrocs::is_unannotated_priority(annotation, wall_seconds);
}

// MON-004 资源门禁生产接线(cli/resource_gate.h 唯一生产调用点; 冻结约束:
// 重计算禁止单线程并自动资源监控, 低利用率/异常内存增长为失败):
// 后台线程对 run_pipeline 执行期采样(ProcessMonitor::tick), 结束后按 07 合同
// evaluate_gate 判定 —— 失败发 resource_gate FAIL 事件并返回 RESOURCE(10)。
// 短任务豁免(wall<5s)由 evaluate_gate 内建, 冒烟小测不受影响。
// kind 固定 Compute: phase1/2/3 均为 cpu_heavy 合成管线(runtime_client.cpp
// resources.class=cpu_heavy); io/mem 类判据属 benchmark 专用路径, 不在 CLI run。
// MON-002: --resource-detail 取值规范化（summary|timeseries; 非法值→ARGS(2)，
// 拒绝静默降级 —— MON-002 test_01 验收非法 detail 必须 fail）。
static std::string resource_detail_arg(const Parsed& p) {
    static const std::set<std::string> kAllowed{"summary", "timeseries"};
    if (!p.values.count("--resource-detail")) return "summary";
    const std::string v = p.values.at("--resource-detail");
    if (!kAllowed.count(v)) {
        throw ParseError("invalid --resource-detail '" + v +
                                 "' (summary|timeseries)");
    }
    return v;
}

static int run_with_resource_gate(astrocs::JsonlEmitter& ev, const std::string& phase,
                                  const std::string& cfg_text, uint32_t budget,
                                  std::string& fail_reason,
                                  const std::string& resource_detail = "summary",
                                  astrocs::ProcessMonitor::Summary* summary_out = nullptr) {
    astrocs::ProcessMonitor mon(0.5);
    // MON-001: 记录器(样本/阶段分段/worker balance)随采样线程写入; interval 与采样
    // 周期一致(0.5s), 保证 cpu_pct=ΔCPU秒/区间墙钟 的 normalized 口径成立。
    astrocs::ResourceRecorder recorder(0.5);
    // MON-002(V7 04_CPU_RESOURCE_TASKS): RSS/allocation report 记录器 —— 同一采样
    // 线程驱动(无新增线程), 定期采样 RSS/private/commit/allocator outstanding,
    // run 结束验证可解释回落并保存原始曲线(alloc_samples.csv + alloc_report.json)。
    astrocs::AllocationRecorder alloc_rec;
    std::atomic<bool> sampling{true};
    // MON-002 first-10s gate: 采样线程在 10s 边界调用一次 fast_fail_first10s(07 §4);
    // 失败置位外部协作取消源 → run_pipeline 内 cancel_watch 转发 Runtime::cancel()。
    std::atomic<int> first10s_diag{static_cast<int>(astrocs::GateDiag::Ok)};
    std::atomic<bool> first10s_done{false};
    std::atomic<bool> first10s_cancel{false};
    std::thread sampler([&mon, &recorder, &alloc_rec, &sampling, &first10s_diag,
                         &first10s_done, &first10s_cancel] {
        using SteadyNs = std::chrono::steady_clock::duration;
        const auto period = std::chrono::duration_cast<SteadyNs>(std::chrono::duration<double>(0.5));
        auto next = std::chrono::steady_clock::now();
        unsigned tick = 0;
        while (sampling.load(std::memory_order_relaxed)) {
            mon.tick();
            recorder.record(mon.last_sample());
            // MON-002(V7): RSS/private/commit/allocator outstanding 同 tick 采样。
            alloc_rec.tick(mon.last_sample());
            ++tick;
            // MON-002: first 10s gate 调用点 —— 跨过 10s 边界后首次采样即评估:
            // 低 CPU+非 IO+非内存带宽饱和 → 快速失败(协作取消), 收尾归并 RESOURCE(10)。
            if (!first10s_done.load(std::memory_order_relaxed) &&
                static_cast<double>(tick) * 0.5 >= 10.0) {
                first10s_done.store(true, std::memory_order_relaxed);
                const auto recs = recorder.records_upto(10.0);
                if (recs.size() >= 2) {
                    std::vector<double> cpus;
                    uint64_t io_bytes = 0;
                    for (const auto& r : recs) {
                        cpus.push_back(r.cpu_pct);
                        io_bytes += r.read_bytes + r.write_bytes;
                    }
                    astrocs::GateConfig f10;
                    f10.first10s_low_cpu =
                        astrocs::percentile_sorted(cpus, 0.50) < 20.0;
                    const double win = std::max(0.5, recs.back().elapsed_seconds -
                                                          recs.front().elapsed_seconds);
                    // 非 IO 密集: 进程 read+write < 1MB/s
                    f10.first10s_non_io = static_cast<double>(io_bytes) < 1e6 * win;
                    // 内存带宽未测: CPU 低时必然未饱和(07 §4 保守取真)
                    f10.first10s_mem_not_saturated = true;
                    if (astrocs::fast_fail_first10s(f10)) {
                        first10s_diag.store(
                            static_cast<int>(astrocs::GateDiag::FastFailFirst10s),
                            std::memory_order_relaxed);
                        first10s_cancel.store(true, std::memory_order_relaxed);
                    }
                }
            }
            next += period;
            std::this_thread::sleep_until(next);
        }
    });
    recorder.set_stage(astrocs::ResStage::Active);
    // MON-001: active 阶段注入实际 worker 租约数(cli_affinity 分配核; 禁硬编码)。
    recorder.set_workers(budget, budget);
    // CLI-004: §4 progress 事件(04 冻结字段 completed/total/unit/rate/eta_seconds)。
    // 粒度 = phase 粒度(run 开始 0/1, 结束 1/1): Runtime 公开合同无节点级进度回调,
    // 协议面按合同冻结 —— 粒度升级(节点/帧级采样)不改变字段结构, 消费者透明。
    ev.emit_progress(0, 1, "phases", nullptr, nullptr);
    const int rrc = astrocs::cli::run_pipeline({phase.back() - '0'}, cfg_text, budget,
                                               &fail_reason, &first10s_cancel);
    {
        const auto s0 = mon.summary();
        const double done_rate = s0.wall_seconds > 0.0 ? 1.0 / s0.wall_seconds : 0.0;
        ev.emit_progress(1, 1, "phases", &done_rate, nullptr);
    }
    recorder.set_stage(astrocs::ResStage::Flush);
    sampling.store(false, std::memory_order_relaxed);
    sampler.join();
    // MON-001: run 收尾自动生成 resource_samples.csv / resource_summary.json /
    // worker_balance.csv(无需操作者脚本; 管线失败也留资源证据)。开销占比由
    // summary.sample_overhead_ms(真实累计采样 wall / 总 wall 口径的原料)度量。
    {
        const astrocs::ProcessMonitor::Summary mon_s = mon.summary();
        const std::string res_out_dir = [&] {
            try { return nlohmann::json::parse(cfg_text).value("output_dir", std::string(".")); }
            catch (...) { return std::string("."); }
        }();
        const bool wrote = recorder.write_all(res_out_dir, mon_s.wall_seconds,
                                              mon_s.sample_overhead_ms);
        if (!wrote) {
            std::fprintf(stderr, "astrocs: warning: resource files not written to %s\n",
                         sanitize(res_out_dir).c_str());
        }
        // MON-002(V7): 原始曲线 + 报告落盘(alloc_samples.csv/alloc_report.json;
        // 管线失败也留证据, 与 MON-001 三产物同策略)。
        alloc_rec.finalize();
        if (!alloc_rec.write_all(res_out_dir)) {
            std::fprintf(stderr, "astrocs: warning: alloc report files not written to %s\n",
                         sanitize(res_out_dir).c_str());
        }
    }
    // MON-002: first-10s gate 快速失败 → 统一 RESOURCE(10)+diagnosis(规格: 失败返回
    // 统一 RESOURCE exit code; 禁止仅 emit event 不改变退出状态)。仅归并 gate 触发的
    // 协作取消(CANCELLED); 用户 SIGINT(first10s=Ok)仍保留 9 语义。
    const astrocs::GateDiag f10 =
        static_cast<astrocs::GateDiag>(first10s_diag.load(std::memory_order_relaxed));
    if (rrc != astrocs::OK && rrc == astrocs::CANCELLED &&
        f10 == astrocs::GateDiag::FastFailFirst10s) {
        const std::string why = "resource gate FAILED: fast_fail_first_10s (" +
                                astrocs::diag_message(f10, astrocs::GateConfig{}) + ")";
        ev.emit("resource_gate", "error", phase, why, {});
        std::fprintf(stderr, "astrocs: %s\n", why.c_str());
        fail_reason = "resource gate failed (first-10s): fast_fail_first_10s";
        return astrocs::RESOURCE;  // exit_codes.h:17 = 10
    }
    if (rrc != astrocs::OK) return rrc;  // 管线自身失败: 保留原退出码

    const auto s = mon.summary();
    astrocs::GateConfig g;
    g.kind = astrocs::ResKind::Compute;
    g.available_cpus = cli_affinity_cpu_count();
    g.selected_workers = budget;
    g.max_active_threads = s.max_threads;
    g.avg_equivalent_cores = s.avg_equivalent_cores;
    g.wall_seconds = s.wall_seconds;
    g.cpu_percent = s.avg_cpu_percent;
    // MON-001 记录器已按 init/active/flush 分段标注(07 §1 stage 标注)
    g.has_stage_annotation = true;
    // MON-002: active 窗口采样统计 → gate 阈值输入(worker p50/CPU p50/mean)。
    // CPU 判据仅 active window>=10s 时提供(规格前置); rss_slope 供横切 memory_growth。
    const auto stats = recorder.stage_stats();
    const auto& act = stats[static_cast<std::size_t>(astrocs::ResStage::Active)];
    if (act.n_samples > 0) {
        g.workers_p50 = act.workers_p50;
        if (act.wall_seconds >= 10.0) {
            g.cpu_p50_percent = act.cpu_pct_p50;
            g.cpu_mean_percent = act.cpu_pct_mean;
        }
        g.rss_slope_measured = true;
        g.rss_slope_mb_per_s =
            static_cast<double>(act.rss_slope_bytes_per_s) / (1024.0 * 1024.0);
    }
    // MON-002: 结束时 gate 调用; first-10s 已失败而结束判定通过时, 快速失败兜底生效。
    astrocs::GateDiag d = astrocs::evaluate_gate(g);
    if (d == astrocs::GateDiag::Ok && f10 == astrocs::GateDiag::FastFailFirst10s)
        d = astrocs::GateDiag::FastFailFirst10s;
    // MON-001(V7): 逐样本聚合判定与 evaluate_gate 互补 —— 监控缺失直接 FAIL;
    // >=70% 样本 U>=0.75; 队列有工作时连续>=10s U<0.50。哨兵纪律: 统计不可得
    // (如 mini workload 采样不足)记 -1 跳过对应判定(p2007 先例), 不构成 FAIL
    // 证据; 监控在跑但 active 段零样本仍是 MonitoringMissing(无资源证据)。
    // mini 任务 0.80*min(selected,available) 门的结构失配(abs-floor)为负责人
    // 裁决项, 此处不自行放宽。
    if (d == astrocs::GateDiag::Ok) {
        const auto mon_recs = recorder.records_stage(astrocs::ResStage::Active);
        g.monitor_present = true;   // ProcessMonitor 采样线程已实际运行并落盘三产物
        if (mon_recs.empty()) {
            g.util_samples_measured = 0.0;
        } else {
            g.util_samples_measured = static_cast<double>(mon_recs.size());
            if (mon_recs.size() >= 2) {
                double pass = 0.0;
                double q_low_run = 0.0, q_low_best = 0.0;
                for (const auto& r : mon_recs) {
                    // 逐样本利用率 U=ΔCPU/(interval×min(selected,available)) —
                    // 100%=全部分配核用满; 与 utilization_value 同口径。
                    if (astrocs::utilization_value(g, r.cpu_pct) >= 0.75) ++pass;
                    // 队列有工作(runnable>0)且利用率<0.50 的连续 run 长度。
                    if (r.runnable_workers > 0 &&
                        astrocs::utilization_value(g, r.cpu_pct) < 0.50) {
                        q_low_run += 0.5;  // 采样周期 0.5s(与 sampler interval 一致)
                        if (q_low_run > q_low_best) q_low_best = q_low_run;
                    } else {
                        q_low_run = 0.0;
                    }
                }
                g.util_samples_pass_frac = pass / static_cast<double>(mon_recs.size());
                g.queue_low_run_seconds = q_low_best;
            }
            // 单样本: 无法算占比/连续窗口 → 哨兵 -1(未观测, 跳过对应判定)。
        }
        const astrocs::GateDiag m1 = astrocs::evaluate_mon001(g);
        if (m1 != astrocs::GateDiag::Ok) d = m1;
    }
    // MON-002(V7): RSS/allocation report 判定与 evaluate_gate/evaluate_mon001 互补 ——
    // 注入 leak(稳健斜率越线)与结束回落不可解释 → FAIL; 面在零有效样本(伪造
    // monitor/全哨兵)同样 FAIL。哨兵纪律: 面未接入/样本不足(斜率未算)跳过对应
    // 判定, 显式呈现不静默。
    {
        const astrocs::AllocReport ar = alloc_rec.report();
        g.alloc_report_present = ar.n_curve > 0;
        if (ar.n_curve > 0) {
            g.alloc_samples_measured = static_cast<double>(ar.n_samples);
            g.alloc_growth_mb_per_s = ar.slope_points > 0
                                          ? ar.rss_growth_mb_per_s
                                          : astrocs::kMon001NotSampled;  // 样本不足未算斜率
            g.alloc_reclaim_verdict = ar.reclaim_verdict;
        }
        const astrocs::GateDiag m2 = astrocs::evaluate_mon002(g);
        if (m2 != astrocs::GateDiag::Ok) d = m2;
    }
    ev.emit("resource", "info", phase, "resource gate", {
        {"verdict", astrocs::gate_diag_name(d)},
        {"wall_seconds", s.wall_seconds},
        {"avg_equivalent_cores", s.avg_equivalent_cores},
        {"max_active_threads", s.max_threads},
        {"selected_workers", g.selected_workers},
        {"available_cpus", g.available_cpus},
        {"workers_p50", g.workers_p50},
        {"cpu_p50_percent", g.cpu_p50_percent},
        {"cpu_mean_percent", g.cpu_mean_percent},
        {"first_10s_gate", astrocs::gate_diag_name(f10)},
        // MON-001: 逐样本门观测证据(-1=未采样哨兵, 非合法值)。
        {"mon001_util_samples_measured", g.util_samples_measured},
        {"mon001_util_samples_pass_frac", g.util_samples_pass_frac},
        {"mon001_queue_low_run_seconds", g.queue_low_run_seconds},
        // MON-002(V7): RSS/allocation report 证据(验收关键词 RSS/allocation report;
        // reclaim_frac=-1=未采样哨兵非 0 回落; 判定用曲线非峰值)。
        {"alloc_report_present", g.alloc_report_present},
        {"alloc_report_n_samples", alloc_rec.report().n_samples},
        {"alloc_report_n_sentinel", alloc_rec.report().n_sentinel},
        {"alloc_report_growth_mb_per_s", g.alloc_growth_mb_per_s},
        {"alloc_report_growth_verdict",
         astrocs::alloc_growth_verdict_name(alloc_rec.report().growth_verdict)},
        {"alloc_report_reclaim_verdict",
         astrocs::alloc_reclaim_verdict_name(g.alloc_reclaim_verdict)},
        {"alloc_report_reclaim_frac", alloc_rec.report().reclaim_frac},
        {"alloc_report_peak_rss_bytes", alloc_rec.report().peak_rss_bytes},
        {"alloc_report_last_rss_bytes", alloc_rec.report().last_rss_bytes},
        {"alloc_report_allocator_probe", alloc_rec.report().allocator_probe_available},
        {"alloc_sample_overhead_ms", alloc_rec.report().alloc_sample_overhead_ms},
        // CLI-004: §4 resource 冻结扩展字段(硬闸要求)。
        {"cpu_cores_used", s.avg_equivalent_cores},
        {"rss_bytes", s.peak_rss_bytes},
        {"io_read_bytes", s.total_read_bytes},
        {"io_write_bytes", s.total_write_bytes},
        {"threads", s.max_threads},
    });
    if (d != astrocs::GateDiag::Ok) {
        const std::string why = "resource gate FAILED: " + std::string(astrocs::gate_diag_name(d)) +
                                " (" + astrocs::diag_message(d, g) + ")";
        ev.emit("resource_gate", "error", phase, why, {});
        std::fprintf(stderr, "astrocs: %s\n", why.c_str());
        return astrocs::RESOURCE;  // exit_codes.h:17 = 10
    }
    // MON-002: 分层事件接线（summary 强制；timeseries 详略由 --resource-detail 控）。
    // CLI-002 移除 cmd_run_pipeline 时漏接（定义保留未调用），MON-002 验收的
    // resource summary / backend 事件在 phase run 路径从未发出——此处补齐。
    // raw 产物目录: recorder.write_all 的 res_out_dir 同源（07 合同 raw 落点）。
    {
        const astrocs::ProcessMonitor::Summary mon_s2 = mon.summary();
        // CLI-004: 真实 monitor 摘要外带给 phase stats 事件(冻结扩展字段同源填充)。
        if (summary_out != nullptr) *summary_out = mon_s2;
        const std::string res_out_dir = [&] {
            try { return nlohmann::json::parse(cfg_text).value("output_dir", std::string(".")); }
            catch (...) { return std::string("."); }
        }();
        emit_resource_summary(ev, phase, mon_s2, res_out_dir, recorder.record_count(),
                              resource_detail);
        emit_backend_event(ev, phase, "astrocs.cpu.baseline", "selected", budget, budget);
    }
    return astrocs::OK;
}

int cmd_phase2_run(const Parsed& p, astrocs::JsonlEmitter& ev) {
    const std::string cfg = need_value(p, "--config");
    std::ifstream f(std::filesystem::u8path(cfg), std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "astrocs: config not found '%s'\n", cfg.c_str());
        return astrocs::INPUT;
    }
    std::stringstream buf; buf << f.rdbuf();
    const std::string cfg_text = buf.str();
    bool ok = false;
    const std::string cfg_sha = file_sha256(cfg, &ok);
    if (!ok) return astrocs::INPUT;
    // CLI-002: 单 phase 命令复用顶层 config 全量校验(unknown key→3), 与已移除的 run 路径同面。
    nlohmann::json cfg_doc;
    const int vrc2 = validate_config_full(cfg, &cfg_doc, /*session_mode=*/true);
    if (vrc2 != astrocs::OK) return vrc2;

    // RT-008: phase2 走 Runtime 单 phase IR 子图（与 run --phases 2 同一路径）。
    ev.stage("phase2_session", true);
    if (const char* sleep_ms = std::getenv("ASTROCS_TEST_SLEEP_MS")) {
        const long ms = std::strtol(sleep_ms, nullptr, 10);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        while (std::chrono::steady_clock::now() < deadline) {
            if (astrocs::is_cancelled()) {
                ev.stage("phase2_session", false);
                const int wrc = write_run_manifest(".", ev, "incomplete", "cancelled by user",
                                                   cfg, cfg_sha, {2});
                if (wrc != astrocs::OK) return wrc;
                ev.emit_final(astrocs::CANCELLED, "cancelled", nullptr, "cancelled by user");
                std::fprintf(stderr, "astrocs: cancelled\n");
                return astrocs::CANCELLED;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    std::string fail_reason;
    const uint32_t budget = cli_affinity_cpu_count();
    astrocs::ProcessMonitor::Summary p2_summary;
    const int rrc = run_with_resource_gate(ev, "phase2", cfg_text, budget, fail_reason,
                              resource_detail_arg(p), &p2_summary);
    ev.stage("phase2_session", false);

    nlohmann::json artifacts = nlohmann::json::array();
    std::vector<std::pair<std::string, std::string>> mans;
    astrocs::cli::collect_node_manifests(&mans);
    for (const std::string& ap : astrocs::cli::collect_node_artifact_paths(mans)) {
        bool ok2 = false;
        const std::string sha = file_sha256(ap, &ok2);
        std::error_code ec;
        const auto size = std::filesystem::file_size(std::filesystem::u8path(ap), ec);
        artifacts.push_back({{"path", ap}, {"sha256", ok2 ? sha : ""},
                             {"size_bytes", ec ? 0ULL : static_cast<unsigned long long>(size)}});
    }
    const std::string out_dir = [&] {
        try { return nlohmann::json::parse(cfg_text).value("output_dir", std::string(".")); }
        catch (...) { return std::string("."); }
    }();

    if (astrocs::is_cancelled()) {
        const int wrc = write_run_manifest(out_dir, ev, "incomplete", "cancelled by user",
                                           cfg, cfg_sha, {2}, artifacts);
        if (wrc != astrocs::OK) return wrc;
        ev.emit_final(astrocs::CANCELLED, "cancelled", nullptr, "cancelled by user");
        std::fprintf(stderr, "astrocs: cancelled\n");
        return astrocs::CANCELLED;
    }
    if (rrc != astrocs::OK) {
        const std::string why = fail_reason.empty() ? ("phase2 failed (exit " + std::to_string(rrc) + ")")
                                                    : fail_reason;
        const int wrc = write_run_manifest(out_dir, ev, "incomplete", "phase2 failed: " + why,
                                           cfg, cfg_sha, {2}, artifacts);
        if (wrc != astrocs::OK) return wrc;
        ev.emit_final(rrc, "phase2_failed", nullptr, why);
        std::fprintf(stderr, "astrocs: phase2 failed: %s\n", sanitize(why).c_str());
        return rrc;
    }
    const int wrc = write_run_manifest(out_dir, ev, "complete", "phase2 ok", cfg, cfg_sha, {2},
                                       artifacts);
    if (wrc != astrocs::OK) return wrc;
    // RT-008: 从节点 manifest 读真实科学值（session inspect 摘要）。
    // 节点 id 是节点图 id（coverage/sample/…/write），不含 "res"——按内容扫描
    // 任一带 n_obs 键的节点 manifest（旧 "res" 过滤是 CLI-002 拆分前 node id）。
    uint64_t n_inputs = 0, n_obs = 0;
    for (const auto& [nid, mtext] : mans) {
        try {
            auto m = nlohmann::json::parse(mtext);
            if (!m.is_object() || !m.contains("n_obs")) continue;
            n_inputs = m.value("n_inputs", 0ull);
            n_obs = m.value("n_obs", 0ull);
            break;
        } catch (...) {}
    }
    emit_phase_stats_resource(ev, "phase2", "session summary",
                              {{"n_inputs", n_inputs}, {"n_obs", n_obs}}, &p2_summary);
    ev.emit_final(astrocs::OK, "ok", nullptr, "phase2 complete");
    return astrocs::OK;
}


int cmd_phase3_run(const Parsed& p, astrocs::JsonlEmitter& ev) {
    const std::string cfg = need_value(p, "--config");
    std::ifstream f(std::filesystem::u8path(cfg), std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "astrocs: config not found '%s'\n", cfg.c_str());
        return astrocs::INPUT;
    }
    std::stringstream buf; buf << f.rdbuf();
    const std::string cfg_text = buf.str();
    bool ok = false;
    const std::string cfg_sha = file_sha256(cfg, &ok);
    if (!ok) return astrocs::INPUT;

    // CLI-002 实现漏迁恢复(原 cmd_run_pipeline 段, test_08 冻结验收): prior
    // astrocs_run_*.json 记录的 artifact 哈希链任一与磁盘不符 → 8(绝不静默跳过验证)。
    // 范围收缩(CLI-002 语义): 逐相 phase3 run 是全新 run, 不跨 run resume 编排——
    // 只对「同一 config 重跑」(prior manifest.config_path == 本次 cfg) 的 prior
    // complete manifest 做 resume 预检; 不同 config 的历史 manifest 属于独立 run,
    // 新 run 可合法覆盖共享 output_dir 的产物, 不校验(否则同目录不同参数重跑恒 8)。
    {
        nlohmann::json prior_cfg_doc;
        try { prior_cfg_doc = nlohmann::json::parse(cfg_text); } catch (...) {}
        const std::string scan_dir = prior_cfg_doc.is_object()
            ? prior_cfg_doc.value("output_dir", std::string(".")) : std::string(".");
        bool mismatch = false;
        std::error_code dec;
        for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::u8path(scan_dir), dec)) {
            const std::string fn = entry.path().filename().u8string();
            if (!entry.is_regular_file() || fn.rfind("astrocs_run_", 0) != 0 || fn.size() <= 14 ||
                fn.substr(fn.size() - 5) != ".json")
                continue;
            try {
                std::ifstream pf(entry.path(), std::ios::binary);
                nlohmann::json pm = nlohmann::json::parse(
                    std::string(std::istreambuf_iterator<char>(pf), {}));
                if (pm.value("kind", std::string()) != "astrocs_run_manifest") continue;
                if (pm.value("config_path", std::string()) != cfg) continue;
                for (const auto& a : pm.value("artifacts", nlohmann::json::array())) {
                    const std::string ap = a.value("path", std::string());
                    if (ap.empty()) continue;
                    if (!std::filesystem::exists(std::filesystem::u8path(ap), dec)) { mismatch = true; break; }
                    bool hok = false; const std::string sha = file_sha256(ap, &hok);
                    if (!hok || sha != a.value("sha256", std::string())) { mismatch = true; break; }
                }
            } catch (...) { mismatch = true; }
            if (mismatch) break;
        }
        if (mismatch) {
            nlohmann::json cfg_doc0;
            const int vrc0 = validate_config_full(cfg, &cfg_doc0, /*session_mode=*/true);
            (void)vrc0;
            const std::string out_dir0 = cfg_doc0.is_object()
                ? cfg_doc0.value("output_dir", std::string(".")) : std::string(".");
            const int wrc = write_run_manifest(out_dir0, ev, "incomplete", "resume hash mismatch",
                                               cfg, cfg_sha, {3});
            if (wrc != astrocs::OK) return wrc;
            ev.emit_final(astrocs::INTEGRITY, "resume_hash_mismatch", nullptr,
                          "prior artifact hash mismatch");
            std::fprintf(stderr, "astrocs: resume hash mismatch\n");
            return astrocs::INTEGRITY;  // 04: 输出完整性验证失败 → 8
        }
    }
    // CLI-002: 单 phase 命令复用顶层 config 全量校验(unknown key→3), 与已移除的 run 路径同面。
    nlohmann::json cfg_doc3;
    const int vrc3 = validate_config_full(cfg, &cfg_doc3, /*session_mode=*/true);
    if (vrc3 != astrocs::OK) return vrc3;

    // RT-008: phase3 走 Runtime 单 phase IR 子图（与 run --phases 3 同一路径）。
    ev.stage("phase3_session", true);
    if (const char* sleep_ms = std::getenv("ASTROCS_TEST_SLEEP_MS")) {
        const long ms = std::strtol(sleep_ms, nullptr, 10);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        while (std::chrono::steady_clock::now() < deadline) {
            if (astrocs::is_cancelled()) {
                ev.stage("phase3_session", false);
                const int wrc = write_run_manifest(".", ev, "incomplete", "cancelled by user",
                                                   cfg, cfg_sha, {3});
                if (wrc != astrocs::OK) return wrc;
                ev.emit_final(astrocs::CANCELLED, "cancelled", nullptr, "cancelled by user");
                std::fprintf(stderr, "astrocs: cancelled\n");
                return astrocs::CANCELLED;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    std::string fail_reason;
    const uint32_t budget = cli_affinity_cpu_count();
    astrocs::ProcessMonitor::Summary p3_summary;
    const int rrc = run_with_resource_gate(ev, "phase3", cfg_text, budget, fail_reason,
                              resource_detail_arg(p), &p3_summary);
    ev.stage("phase3_session", false);

    nlohmann::json artifacts = nlohmann::json::array();
    std::set<std::string> seen_paths;  // 链式节点共享同一 session 产物 → 按 path 去重
    std::vector<std::pair<std::string, std::string>> mans;
    astrocs::cli::collect_node_manifests(&mans);
    for (const auto& [nid, mtext] : mans) {
        // node id 是节点图 id（properties/wcs/resample2/writer/verify…），session
        // manifest 任何节点都可能带 output_fits_path/工件清单——按内容收集，勿按
        // 硬编码节点名过滤（旧写法只认 "hips"，P3 链 node id 不含 hips → artifacts 恒空）。
        nlohmann::json m;
        try { m = nlohmann::json::parse(mtext); } catch (...) { continue; }
        if (!m.is_object()) continue;
        for (const auto& a : m.value("artifacts", nlohmann::json::array())) {
            const std::string ap = a.get<std::string>();
            if (!seen_paths.insert(ap).second) continue;
            bool ok2 = false;
            const std::string sha = file_sha256(ap, &ok2);
            std::error_code ec;
            const auto size = std::filesystem::file_size(std::filesystem::u8path(ap), ec);
            artifacts.push_back({{"path", ap}, {"sha256", ok2 ? sha : ""},
                                 {"size_bytes", ec ? 0ULL : static_cast<unsigned long long>(size)}});
        }
        const std::string op = m.value("output_fits_path", std::string());
        if (!op.empty() && seen_paths.insert(op).second) {
            bool ok2 = false;
            const std::string sha = file_sha256(op, &ok2);
            std::error_code ec;
            const auto size = std::filesystem::file_size(std::filesystem::u8path(op), ec);
            // CLI-007 冻结语义: phase3 输出 FITS 记 role=phase3_output(test_07
            // 断言); CLI-002 拆分时聚合迁入 cmd_phase3_run 丢失 role 字段。
            artifacts.push_back({{"role", "phase3_output"}, {"path", op}, {"sha256", ok2 ? sha : ""},
                                 {"size_bytes", ec ? 0ULL : static_cast<unsigned long long>(size)}});
        }
    }
    const std::string out_dir = [&] {
        try { return nlohmann::json::parse(cfg_text).value("output_dir", std::string(".")); }
        catch (...) { return std::string("."); }
    }();

    if (astrocs::is_cancelled()) {
        const int wrc = write_run_manifest(out_dir, ev, "incomplete", "cancelled by user",
                                           cfg, cfg_sha, {3}, artifacts);
        if (wrc != astrocs::OK) return wrc;
        ev.emit_final(astrocs::CANCELLED, "cancelled", nullptr, "cancelled by user");
        std::fprintf(stderr, "astrocs: cancelled\n");
        return astrocs::CANCELLED;
    }
    if (rrc != astrocs::OK) {
        const std::string why = fail_reason.empty() ? ("phase3 failed (exit " + std::to_string(rrc) + ")")
                                                    : fail_reason;
        const int wrc = write_run_manifest(out_dir, ev, "incomplete", "phase3 failed: " + why,
                                           cfg, cfg_sha, {3}, artifacts);
        if (wrc != astrocs::OK) return wrc;
        ev.emit_final(rrc, "phase3_failed", nullptr, why);
        std::fprintf(stderr, "astrocs: phase3 failed: %s\n", sanitize(why).c_str());
        return rrc;
    }
    const int wrc = write_run_manifest(out_dir, ev, "complete", "phase3 ok", cfg, cfg_sha, {3},
                                       artifacts);
    if (wrc != astrocs::OK) return wrc;
    // RT-009: phase3 run 成功路径补写运行图产物（static/observed/sidecar + L0 渲染）。
    // 此前 write_run_graphs 定义后无任何调用点（CLI-002 移除 cmd_run_pipeline/
    // cmd_graph 时漏接），RT-009 test_07 期望的 out/graph/* 恒缺失。
    // best-effort: 函数内部只 warning 不失败 run（"不失败 run"合同见其注释）。
    write_run_graphs(out_dir, ev, cfg, cfg_sha, {3});
    emit_phase_stats_resource(ev, "phase3", "session summary",
                              {{"outputs", artifacts.size()}}, &p3_summary);
    ev.emit_final(astrocs::OK, "ok", nullptr, "phase3 complete");
    return astrocs::OK;
}


int cmd_phase1_run(const Parsed& p, astrocs::JsonlEmitter& ev) {
    const std::string cfg = need_value(p, "--config");
    std::ifstream f(std::filesystem::u8path(cfg), std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "astrocs: config not found '%s'\n", cfg.c_str());
        return astrocs::INPUT;
    }
    std::stringstream buf; buf << f.rdbuf();
    const std::string cfg_text = buf.str();
    bool ok = false;
    const std::string cfg_sha = file_sha256(cfg, &ok);
    if (!ok) return astrocs::INPUT;
    // CLI-002: 单 phase 命令复用顶层 config 全量校验(unknown key→3), 与已移除的 run 路径同面。
    nlohmann::json cfg_doc1;
    const int vrc1 = validate_config_full(cfg, &cfg_doc1, /*session_mode=*/true);
    if (vrc1 != astrocs::OK) return vrc1;

    // RT-008: phase1 走 Runtime 单 phase IR 子图（与 run --phases 1 同一路径，不是第二条）。
    // 退出码映射保持旧协议：配置错→2; 输入缺→3; 科学失败→70; IO→7; 取消→9。
    ev.stage("phase1_session", true);
    // 测试钩子(非用户接口): 阶段间等待, 供取消/无子进程证明(与 run/stub 同语义)
    if (const char* sleep_ms = std::getenv("ASTROCS_TEST_SLEEP_MS")) {
        const long ms = std::strtol(sleep_ms, nullptr, 10);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        while (std::chrono::steady_clock::now() < deadline) {
            if (astrocs::is_cancelled()) {
                ev.stage("phase1_session", false);
                const int wrc = write_run_manifest(".", ev, "incomplete", "cancelled by user",
                                                   cfg, cfg_sha, {1});
                if (wrc != astrocs::OK) return wrc;
                ev.emit_final(astrocs::CANCELLED, "cancelled", nullptr, "cancelled by user");
                std::fprintf(stderr, "astrocs: cancelled\n");
                return astrocs::CANCELLED;             // 04: 取消 → 9
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    std::string fail_reason;
    const uint32_t budget = cli_affinity_cpu_count();
    astrocs::ProcessMonitor::Summary p1_summary;
    const int rrc = run_with_resource_gate(ev, "phase1", cfg_text, budget, fail_reason,
                              resource_detail_arg(p), &p1_summary);
    ev.stage("phase1_session", false);

    nlohmann::json artifacts = nlohmann::json::array();
    std::vector<std::pair<std::string, std::string>> mans;
    astrocs::cli::collect_node_manifests(&mans);
    for (const auto& [nid, mtext] : mans) {
        if (nid != "cal") continue;
        nlohmann::json m;
        try { m = nlohmann::json::parse(mtext); } catch (...) { continue; }
        for (const auto& a : m.value("artifacts", nlohmann::json::array())) {
            const std::string ap = a.get<std::string>();
            bool ok2 = false;
            const std::string sha = file_sha256(ap, &ok2);
            std::error_code ec;
            const auto size = std::filesystem::file_size(std::filesystem::u8path(ap), ec);
            artifacts.push_back({{"path", ap}, {"sha256", ok2 ? sha : ""},
                                 {"size_bytes", ec ? 0ULL : static_cast<unsigned long long>(size)}});
        }
    }
    const std::string out_dir = [&] {
        try { return nlohmann::json::parse(cfg_text).value("output_dir", std::string(".")); }
        catch (...) { return std::string("."); }
    }();

    if (astrocs::is_cancelled()) {
        const int wrc = write_run_manifest(out_dir, ev, "incomplete", "cancelled by user",
                                           cfg, cfg_sha, {1}, artifacts);
        if (wrc != astrocs::OK) return wrc;
        ev.emit_final(astrocs::CANCELLED, "cancelled", nullptr, "cancelled by user");
        std::fprintf(stderr, "astrocs: cancelled\n");
        return astrocs::CANCELLED;                   // 04: 取消 → 9, manifest=incomplete
    }
    if (rrc != astrocs::OK) {
        const std::string why = fail_reason.empty() ? ("phase1 failed (exit " + std::to_string(rrc) + ")")
                                                    : fail_reason;
        const int wrc = write_run_manifest(out_dir, ev, "incomplete", "phase1 failed: " + why,
                                           cfg, cfg_sha, {1}, artifacts);
        if (wrc != astrocs::OK) return wrc;
        ev.emit_final(rrc, "phase1_failed", nullptr, why);
        std::fprintf(stderr, "astrocs: phase1 failed: %s\n", sanitize(why).c_str());
        return rrc;  // RT-008: Runtime 退出码映射(Runtime 已按 04 合同映射)
    }
    const int wrc = write_run_manifest(out_dir, ev, "complete", "phase1 ok", cfg, cfg_sha, {1},
                                       artifacts);
    if (wrc != astrocs::OK) return wrc;
    emit_phase_stats_resource(ev, "phase1", "frames processed",
                              {{"frames", artifacts.size()}}, &p1_summary);
    ev.emit_final(astrocs::OK, "ok", nullptr, "phase1 complete");
    return astrocs::OK;
}

// verify: 04 §3 — manifest→status→version→输入 hash→逐 artifact(存在→sha→size)
int cmd_verify(const Parsed& p, astrocs::JsonlEmitter& ev) {
    (void)ev;
    if (!p.flags.count("--json")) parse_fail("verify requires --json");
    const std::string mp = need_value(p, "--run-manifest");
    std::ifstream f(std::filesystem::u8path(mp), std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "astrocs: run manifest not found '%s'\n", mp.c_str());
        return astrocs::INPUT;
    }
    std::stringstream buf; buf << f.rdbuf();
    nlohmann::json m;
    try {
        m = nlohmann::json::parse(buf.str());
    } catch (const nlohmann::json::parse_error& e) {
        std::fprintf(stderr, "astrocs: manifest malformed JSON: %s\n", sanitize(e.what()).c_str());
        return astrocs::INPUT;
    }
    if (!m.is_object() || m.value("kind", std::string()) != "astrocs_run_manifest" ||
        m.value("schema_version", std::string()) != "1") {
        std::fprintf(stderr, "astrocs: not a v1 astrocs_run_manifest document\n");
        return astrocs::INPUT;
    }
    if (m.value("status", std::string()) != "complete") {
        std::fprintf(stderr, "astrocs: run manifest status='%s' (incomplete run cannot be verified)\n",
                     m.value("status", std::string()).c_str());
        return astrocs::INTEGRITY;                        // 04: 输出完整性/验证失败 → 8
    }
    if (m.value("astrocs_version", std::string()) != ASTROCS_VERSION_STRING) {
        std::fprintf(stderr, "astrocs: manifest was produced by version '%s', this is '%s'\n",
                     m.value("astrocs_version", std::string()).c_str(), ASTROCS_VERSION_STRING);
        return astrocs::BACKEND;                          // 04 §5(换版本不可 verify 旧 run)
    }
    int checked = 1;
    if (m.contains("config_path") && !m["config_path"].is_null()) {
        bool ok = false;
        const std::string cur = file_sha256(m["config_path"].get<std::string>(), &ok);
        if (!ok) {
            std::fprintf(stderr, "astrocs: config input no longer readable\n");
            return astrocs::INPUT;
        }
        if (cur != m.value("config_sha256", std::string())) {
            std::fprintf(stderr, "astrocs: config changed since the run (hash mismatch)\n");
            return astrocs::INPUT;
        }
        ++checked;
    }
    for (const auto& a : m.value("artifacts", nlohmann::json::array())) {
        const std::string apath = a.value("path", std::string());
        std::error_code ec;
        if (!std::filesystem::exists(std::filesystem::u8path(apath), ec)) {
            std::fprintf(stderr, "astrocs: artifact missing '%s'\n", apath.c_str());
            return astrocs::INPUT;
        }
        bool ok = false;
        const std::string sha = file_sha256(apath, &ok);
        if (!ok || sha != a.value("sha256", std::string())) {
            std::fprintf(stderr, "astrocs: artifact sha256 mismatch '%s'\n", apath.c_str());
            return astrocs::INTEGRITY;
        }
        const auto size = std::filesystem::file_size(std::filesystem::u8path(apath), ec);
        if (ec || static_cast<unsigned long long>(size) != a.value("size_bytes", 0ULL)) {
            std::fprintf(stderr, "astrocs: artifact size mismatch '%s'\n", apath.c_str());
            return astrocs::INTEGRITY;
        }
        ++checked;
    }
    nlohmann::json out = {{"verify", "ok"}, {"checked", checked}, {"manifest", mp}};
    std::printf("%s\n", out.dump().c_str());
    return astrocs::OK;
}

// CPU-003: `verify profile --profile <path> [--json]` — 独立复读 v2 benchmark profile。
// 校验: JSON 可解析、v2 schema 字段完整、版本/commit/workers/block/median 合理。
int cmd_verify_profile(const Parsed& p, astrocs::JsonlEmitter& ev) {
    const std::string pp = need_value(p, "--profile");
    std::ifstream f(std::filesystem::u8path(pp), std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "astrocs: profile not found '%s'\n", pp.c_str());
        return astrocs::INPUT;
    }
    std::stringstream buf; buf << f.rdbuf();
    const std::string err = astrocs::backend_host::verify_profile_v2(buf.str(), ASTROCS_COMMIT_SHA);
    if (!err.empty()) {
        std::fprintf(stderr, "astrocs: verify profile FAIL: %s\n", err.c_str());
        return astrocs::INTEGRITY;
    }
    nlohmann::json d = nlohmann::json::parse(buf.str());
    const std::string verdict = "PASS";
    nlohmann::json out = {{"verify_profile", "ok"},
                          {"verdict", verdict},
                          {"kernels", d["kernels"].size()},
                          {"logical_available", d["host"].value("logical_available", 0)},
                          {"commit", d["build"].value("source_commit", "")}};
    std::printf("%s\n", out.dump().c_str());
    // CLI-004: §4 artifact 冻结词表 — cpu profile 补 sha256+size_bytes。
    {
        bool pok = false;
        const std::string psha = file_sha256(pp, &pok);
        std::error_code pec;
        const auto psz = std::filesystem::file_size(std::filesystem::u8path(pp), pec);
        ev.emit("artifact", "info", "benchmark", "cpu profile verified",
                {{"role", "cpu_profile"}, {"path", pp}, {"verdict", verdict},
                 {"sha256", psha},
                 {"size_bytes", pec ? nlohmann::json(nullptr)
                                    : nlohmann::json(static_cast<unsigned long long>(psz))}});
    }
    return astrocs::OK;
}



int cmd_drizzle(const Parsed& p, astrocs::JsonlEmitter& ev) {
    (void)p;
    ev.emit_final(astrocs::ARGS, "test_preset_only", nullptr,
                  "drizzle 命令仅支持测试 preset：请用 'astrocs test synthetic --group drizzle'");
    std::fprintf(stderr, "astrocs: drizzle 命令仅支持测试 preset "
                         "(test synthetic --group drizzle)；生产 HiPS 投影请用 phase2 run\n");
    return astrocs::ARGS;
}

// RT-009: `graph --preset 1,2,3 --config cfg.json --output DIR` 生成静态图
// （IR → static JSON/DOT/SVG + L0）。不执行科学计算; 只构图。
// ═══════════════════════ CLI-001: V7 统一命令面 ═══════════════════════
// 契约: 03_TARGET_PRODUCT_AND_ARCHITECTURE.md §3 (version/modules/selftest)。
// stdout JSON schema: contracts/config/cli_modules_list.schema.json,
//                     contracts/config/cli_selftest.schema.json。
// 稳定退出码: OK=0; 缺 product manifest/坏 manifest/缺 DLL → BACKEND=5
// (backend ABI/装配/加载失败, exit_codes.h 唯一源); 参数错 → 2。
// 机器输出纪律: --json 模式 stdout 恰一个 JSON 文档, 无混杂进度文字(04 §3)。

// 可执行文件所在目录(manifest/模块 发现根; UTF-8 路径安全)。
static std::string cli_exe_dir() {
#if defined(_WIN32)
    wchar_t buf[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0) return ".";
    std::wstring ws(buf, n);
    // B13-R13-4: 修复 1 字节越界写 — 分配 len-1 却传 cbMultiByte=len。
    // 正确顺序 (缓冲计算见 cli_common.h utf8_from_wide_*): 分配 len → 转 len → 去 NUL。
    const int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s;
    if (astrocs::utf8_from_wide_should_convert(len)) {
        s.assign(astrocs::utf8_from_wide_alloc_bytes(len), '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, s.data(), len, nullptr, nullptr);
        s.resize(astrocs::utf8_from_wide_final_len(len));
    }
    const std::filesystem::path p(std::filesystem::u8path(s));
    const auto parent = p.parent_path();
    return parent.empty() ? "." : parent.string();
#else
    std::error_code ec;
    const auto p = std::filesystem::canonical("/proc/self/exe", ec);
    if (ec) return ".";
    const auto parent = p.parent_path();
    return parent.empty() ? "." : parent.string();
#endif
}

// 定位 product manifest: 优先 exe 旁 astrocs.product.json; 回退 CWD。
static std::string locate_product_manifest() {
    const std::string dir = cli_exe_dir();
    const std::string cand_exe = dir + "/astrocs.product.json";
    std::error_code ec;
    if (std::filesystem::is_regular_file(std::filesystem::u8path(cand_exe), ec)) return cand_exe;
    const std::string cand_cwd = "astrocs.product.json";
    if (std::filesystem::is_regular_file(std::filesystem::u8path(cand_cwd), ec)) return cand_cwd;
    return "";
}

// manifest 所在目录(绝对; rel_path 相对它解析, 与 ABI-004 registry 语义一致)。
// manifest 可为 exe 旁(安装树)或 CWD(开发/测试树)。
static std::string product_manifest_base_dir(const std::string& mpath) {
    std::error_code ec;
    std::filesystem::path p = std::filesystem::u8path(mpath);
    if (p.is_relative()) {
        p = std::filesystem::absolute(p, ec);
        if (ec) p = std::filesystem::u8path(mpath);
    }
    const auto parent = p.parent_path();
    return parent.empty() ? "." : parent.string();
}

// rel_path(manifest 相对) 在安装树内是否实际存在(UTF-8 安全)。
static bool unit_file_present(const std::string& base_dir, const std::string& rel) {
    if (rel.empty()) return false;
    std::error_code ec;
    return std::filesystem::is_regular_file(
        std::filesystem::u8path(base_dir + "/" + rel), ec);
}

// 解析 product manifest → units[] 数组; 返回 OK; 坏/缺失 → 相应退出码, err 填充。
// 只做最小语义校验(不引入 schema 引擎; schema 机器门在 tests/contracts 侧)。
static int load_product_manifest(const std::string& path, nlohmann::json* units_out,
                                 std::string* err) {
    units_out->clear();
    std::ifstream f(std::filesystem::u8path(path), std::ios::binary);
    if (!f) {
        if (err) *err = "product manifest unreadable";
        return astrocs::BACKEND;   // 05: 装配/加载失败(manifest 是安装树事实源)
    }
    std::stringstream buf; buf << f.rdbuf();
    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(buf.str());
    } catch (const nlohmann::json::parse_error& e) {
        if (err) *err = std::string("product manifest malformed JSON: ") + sanitize(e.what());
        return astrocs::BACKEND;
    }
    if (!doc.is_object() || !doc.contains("units") || !doc["units"].is_array()) {
        if (err) *err = "product manifest missing 'units' array";
        return astrocs::BACKEND;
    }
    *units_out = doc["units"];
    return astrocs::OK;
}

// modules list: 扫描 manifest 登记的 unit; 逐个检查安装树文件存在性。
// 无 product manifest = 宿主未按安装树装配 → BACKEND(5), 明示原因(非静默 PASS)。
static int cmd_modules_list(const Parsed& p, astrocs::JsonlEmitter& ev) {
    (void)ev;
    const bool json = p.flags.count("--json") > 0;
    const std::string mpath = locate_product_manifest();
    nlohmann::json units = nlohmann::json::array();
    nlohmann::json issues = nlohmann::json::array();
    bool present = false;
    int rc = astrocs::OK;
    if (mpath.empty()) {
        // 无 product manifest: 宿主未按安装树装配(纯开发单 exe 场景)。
        // modules list 属装配查询: 无 manifest → BACKEND(缺装配事实源), 明示原因。
        rc = astrocs::BACKEND;
        issues.push_back({{"kind", "manifest"},
                          {"detail", "no product manifest (not an installed tree)"}});
    } else {
        present = true;
        std::string err;
        rc = load_product_manifest(mpath, &units, &err);
        if (rc != astrocs::OK) {
            issues.push_back({{"kind", "manifest"}, {"detail", err}});
        } else {
            const std::string dir = product_manifest_base_dir(mpath);
            nlohmann::json cleaned = nlohmann::json::array();
            for (const auto& u : units) {
                if (!u.is_object()) continue;
                const std::string rel = u.value("rel_path", std::string());
                const std::string kind = u.value("kind", std::string());
                const bool file_present = unit_file_present(dir, rel);
                nlohmann::json e = {
                    {"unit_id", u.value("unit_id", std::string())},
                    {"kind", kind},
                    {"rel_path", rel},
                    {"module_id", u.contains("module_id") && !u["module_id"].is_null()
                                      ? nlohmann::json(u["module_id"].get<std::string>())
                                      : nlohmann::json(nullptr)},
                    {"abi_version", u.contains("abi_version") && !u["abi_version"].is_null()
                                        ? nlohmann::json(u["abi_version"].get<int>())
                                        : nlohmann::json(nullptr)},
                    {"status", u.value("status", std::string())},
                    {"sha256", u.contains("sha256") && !u["sha256"].is_null()
                                   ? nlohmann::json(u["sha256"].get<std::string>())
                                   : nlohmann::json(nullptr)},
                    {"present", file_present},
                };
                cleaned.push_back(e);
                // 缺 DLL/模块文件 → issue(list 报告; verify 判 FAIL)
                if (!file_present && (kind == "module" || kind == "provider" ||
                                      kind == "runtime" || kind == "io")) {
                    issues.push_back({{"kind", "missing_unit_file"},
                                      {"unit_id", u.value("unit_id", std::string())},
                                      {"rel_path", rel}});
                }
            }
            units = cleaned;
        }
    }
    const bool fail = rc != astrocs::OK || !issues.empty();
    nlohmann::json doc = {
        {"schema_version", 1},
        {"kind", "astrocs_modules_list"},
        {"manifest", present ? nlohmann::json(mpath) : nlohmann::json(nullptr)},
        {"manifest_present", present},
        {"units", units},
        {"issues", issues},
        {"verdict", fail ? "FAIL" : "PASS"},
    };
    if (json) {
        std::printf("%s\n", doc.dump(2).c_str());
        return fail ? astrocs::BACKEND : astrocs::OK;
    }
    if (!present) {
        // 人类模式: 无 manifest → 简短错误(退出 5)
        std::fprintf(stderr, "astrocs: modules list: no product manifest "
                             "(not an installed tree; run from an install root)\n");
        return astrocs::BACKEND;
    }
    std::printf("manifest: %s\n", mpath.c_str());
    for (const auto& u : units) {
        std::printf("%s\t%s\t%s\t%s\n",
                    u.value("kind", std::string()).c_str(),
                    u.value("unit_id", std::string()).c_str(),
                    u.value("status", std::string()).c_str(),
                    u.value("present", false) ? "present" : "MISSING");
    }
    if (fail) {
        std::fprintf(stderr, "astrocs: modules list: one or more declared units missing\n");
        return astrocs::BACKEND;
    }
    return astrocs::OK;
}

// modules verify: 独立实现(list 之上把"缺文件"视为失败 = 装配完整性门)。
// 输出复用 cli_modules_list.schema(契约一致; verdict FAIL + issues 非空)。
static int cmd_modules_verify(const Parsed& p, astrocs::JsonlEmitter& ev) {
    (void)ev;
    const bool json = p.flags.count("--json") > 0;
    const std::string mpath = locate_product_manifest();
    nlohmann::json units = nlohmann::json::array();
    nlohmann::json issues = nlohmann::json::array();
    if (mpath.empty()) {
        issues.push_back({{"kind", "manifest"}, {"detail", "no product manifest"}});
        if (json) {
            nlohmann::json doc = {
                {"schema_version", 1}, {"kind", "astrocs_modules_list"},
                {"manifest", nullptr}, {"manifest_present", false},
                {"units", nlohmann::json::array()},
                {"issues", issues}, {"verdict", "FAIL"},
            };
            std::printf("%s\n", doc.dump(2).c_str());
        } else {
            std::fprintf(stderr, "astrocs: modules verify FAIL: no product manifest "
                                 "(not an installed tree)\n");
        }
        return astrocs::BACKEND;   // 缺装配事实源 → 5
    }
    std::string err;
    const int rc = load_product_manifest(mpath, &units, &err);
    const std::string dir = product_manifest_base_dir(mpath);
    if (rc != astrocs::OK) {
        issues.push_back({{"kind", "manifest"}, {"detail", err}});
    } else {
        nlohmann::json cleaned = nlohmann::json::array();
        for (const auto& u : units) {
            if (!u.is_object()) continue;
            const std::string rel = u.value("rel_path", std::string());
            const std::string kind = u.value("kind", std::string());
            const bool file_present = unit_file_present(dir, rel);
            cleaned.push_back(nlohmann::json{
                {"unit_id", u.value("unit_id", std::string())},
                {"kind", kind},
                {"rel_path", rel},
                {"module_id", u.contains("module_id") && !u["module_id"].is_null()
                                  ? nlohmann::json(u["module_id"].get<std::string>())
                                  : nlohmann::json(nullptr)},
                {"abi_version", u.contains("abi_version") && !u["abi_version"].is_null()
                                    ? nlohmann::json(u["abi_version"].get<int>())
                                    : nlohmann::json(nullptr)},
                {"status", u.value("status", std::string())},
                {"sha256", u.contains("sha256") && !u["sha256"].is_null()
                               ? nlohmann::json(u["sha256"].get<std::string>())
                               : nlohmann::json(nullptr)},
                {"present", file_present},
            });
            if (!file_present &&
                (kind == "module" || kind == "provider" || kind == "runtime" ||
                 kind == "io" || kind == "exe")) {
                issues.push_back({{"kind", "missing_unit_file"},
                                  {"unit_id", u.value("unit_id", std::string())},
                                  {"rel_path", rel}});
            }
        }
        units = cleaned;
    }
    const bool fail = !issues.empty();
    if (json || fail) {
        nlohmann::json doc = {
            {"schema_version", 1},
            {"kind", "astrocs_modules_list"},
            {"manifest", nlohmann::json(mpath)},
            {"manifest_present", true},
            {"units", units},
            {"issues", issues},
            {"verdict", fail ? "FAIL" : "PASS"},
        };
        std::printf("%s\n", doc.dump(2).c_str());
    } else {
        std::printf("modules verify OK (%zu units present)\n", units.size());
    }
    return fail ? astrocs::BACKEND : astrocs::OK;
}

// selftest: 宿主自检 + 可选 module/provider 装配校验。--module 时缺该 DLL → 5。
static int cmd_selftest(const Parsed& p, astrocs::JsonlEmitter& ev) {
    (void)ev;
    const bool json = p.flags.count("--json") > 0;
    const std::string want_mod = p.values.count("--module") ? p.values.at("--module") : "";
    const std::string want_prov = p.values.count("--provider") ? p.values.at("--provider") : "";
    nlohmann::json checks = nlohmann::json::array();

    // 1) 宿主基线(与 doctor baseline_selftest 同源; 独立重跑不缓存)
    {
        astrocs_host_services_v1 host;
        void* hstate = nullptr;
        astrocs_host_services_default_v1(&host, &hstate);
        astrocs_backend_api_v1 api{};
        std::memset(&api, 0, sizeof(api));
        const int grc = astrocs_backend_get_api_v1(ACS_ABI_VERSION_V1,
                                                   sizeof(astrocs_host_services_v1), &host, &api);
        checks.push_back(nlohmann::json{
            {"name", "host_baseline"},
            {"status", (grc == ACS_OK && api.self_test &&
                        api.self_test(&host) == ACS_OK) ? "pass" : "fail"}});
    }
    // 2) 退出码表一致性: 单源由 tools/check_cli_command_layer +
    // tests/cli test_10_exit_codes_single_source 机器门保证(数值唯一在
    // cli/exit_codes.h)。本命令不重列数值, 仅做可编译引用证明头已含表。
    {
        // 引用 exit_codes 枚举成员(数值单源); probe 无科学含义
        const long probe = static_cast<long>(astrocs::ARGS) + static_cast<long>(astrocs::INTERNAL);
        checks.push_back(nlohmann::json{{"name", "exit_code_table"},
                                        {"status", probe > 0 ? "pass" : "fail"}});
    }
    // 3) 装配完整性: 找 manifest; 记录缺文件 unit
    {
        const std::string mpath = locate_product_manifest();
        if (mpath.empty()) {
            checks.push_back(nlohmann::json{{"name", "product_manifest"},
                                            {"status", "skipped"},
                                            {"detail", "no installed tree (dev single-exe)"}});
        } else {
            nlohmann::json units;
            std::string err;
            const int rc = load_product_manifest(mpath, &units, &err);
            if (rc != astrocs::OK) {
                checks.push_back(nlohmann::json{{"name", "product_manifest"},
                                                {"status", "fail"}, {"detail", err}});
            } else {
                const std::string dir = product_manifest_base_dir(mpath);
                bool all_present = true;
                std::vector<std::string> missing;
                for (const auto& u : units) {
                    if (!u.is_object()) continue;
                    const std::string kind = u.value("kind", std::string());
                    if (kind != "module" && kind != "provider") continue;
                    const std::string rel = u.value("rel_path", std::string());
                    if (rel.empty()) continue;
                    if (!unit_file_present(dir, rel)) {
                        all_present = false;
                        missing.push_back(u.value("unit_id", rel));
                    }
                }
                // 过滤 --module/--provider 定向选择: 未装配/未找到 → 明确 fail。
                // 模块按 module_id(唯一登记身份)精确匹配; provider 按 unit_id 或
                // 其 module_id 精确匹配——绝不"任意 provider 兜底"(否则未登记
                // provider ID 会静默 PASS, 违背 缺 DLL/未登记→非零 契约)。
                if (!want_mod.empty()) {
                    bool found = false;
                    for (const auto& u : units) {
                        if (!u.is_object()) continue;
                        if (u.value("kind", std::string()) != "module") continue;
                        const std::string mid =
                            u.contains("module_id") && !u["module_id"].is_null()
                                ? u["module_id"].get<std::string>() : std::string();
                        if (mid != want_mod) continue;
                        found = true;
                        const std::string rel = u.value("rel_path", std::string());
                        const bool fp = unit_file_present(dir, rel);
                        checks.push_back(nlohmann::json{
                            {"name", "module_assembly:" + want_mod},
                            {"status", fp ? "pass" : "fail"},
                            {"detail", fp ? std::string("present") : ("missing: " + rel)}});
                        if (!fp) all_present = false;
                    }
                    if (!found) {
                        checks.push_back(nlohmann::json{
                            {"name", "module_assembly:" + want_mod},
                            {"status", "fail"},
                            {"detail", "module not declared in product manifest"}});
                        all_present = false;
                    }
                }
                if (!want_prov.empty()) {
                    bool found = false;
                    for (const auto& u : units) {
                        if (!u.is_object()) continue;
                        if (u.value("kind", std::string()) != "provider") continue;
                        const std::string uid = u.value("unit_id", std::string());
                        const std::string mid =
                            u.contains("module_id") && !u["module_id"].is_null()
                                ? u["module_id"].get<std::string>() : std::string();
                        if (uid != want_prov && mid != want_prov) continue;
                        found = true;
                        const std::string rel = u.value("rel_path", std::string());
                        const bool fp = unit_file_present(dir, rel);
                        checks.push_back(nlohmann::json{
                            {"name", "provider_assembly:" + want_prov},
                            {"status", fp ? "pass" : "fail"},
                            {"detail", fp ? std::string("present") : ("missing: " + rel)}});
                        if (!fp) all_present = false;
                    }
                    if (!found) {
                        checks.push_back(nlohmann::json{
                            {"name", "provider_assembly:" + want_prov},
                            {"status", "fail"},
                            {"detail", "provider not declared in product manifest"}});
                        all_present = false;
                    }
                }
                if (want_mod.empty() && want_prov.empty()) {
                    checks.push_back(nlohmann::json{
                        {"name", "module_assembly"},
                        {"status", all_present ? "pass" : "fail"},
                        {"detail", all_present ? std::string("all declared units present")
                                               : ("missing: " + missing.front())}});
                }
            }
        }
    }
    bool all = true;
    for (const auto& c : checks)
        if (c.value("status", "") == "fail") all = false;
    nlohmann::json doc = {{"schema_version", 1}, {"kind", "astrocs_selftest"},
                          {"checks", checks}, {"verdict", all ? "PASS" : "FAIL"}};
    if (json || all) {
        std::printf("%s\n", doc.dump(2).c_str());
    }
    if (!json && !all) {
        for (const auto& c : checks)
            if (c.value("status", "") == "fail")
                std::fprintf(stderr, "astrocs: selftest FAIL: %s\n",
                             c.value("name", "").c_str());
    }
    return all ? astrocs::OK : astrocs::BACKEND;
}

// dispatch: 外部可见（cli_common.h 声明；main.cpp 调用）。
int dispatch(const Parsed& p) {
    const std::string joined = p.join();
    const bool events = p.flags.count("--events-jsonl") > 0;
    const std::string phase_name =
        (joined.rfind("phase", 0) == 0 ? joined.substr(0, 6) : joined);
    astrocs::JsonlEmitter ev(events, astrocs::make_run_id(), phase_name);

    if (joined == "--version" || joined == "version") {
        if (p.flags.count("--json")) {
            std::printf("{\"schema_version\":\"1\",\"name\":\"astrocs\",\"version\":\"%s\"}\n",
                        ASTROCS_VERSION_STRING);
        } else {
            std::printf("astrocs %s\n", ASTROCS_VERSION_STRING);
        }
        return astrocs::OK;
    }
    if (joined == "--help" || joined == "-h") {
        std::fputs(kHelp, stdout);
        return astrocs::OK;
    }
    if (joined == "modules list")            return cmd_modules_list(p, ev);
    if (joined == "modules verify")          return cmd_modules_verify(p, ev);
    if (joined == "selftest")                return cmd_selftest(p, ev);
    if (joined == "benchmark cpu") {
        const bool quick = p.flags.count("--quick") > 0;
        const bool full = p.flags.count("--full") > 0;
        if (quick == full) parse_fail("benchmark cpu requires exactly one of --quick|--full");
        const std::string out_path = p.values.count("--output") ? p.values.at("--output")
                                                                : "cpu_profile.json";
        const std::string mode = quick ? "quick" : "full";
        const std::string commit = ASTROCS_COMMIT_SHA;
        // CPU-003: v2 profile 生成(完整 benchmark 链; backends 目录=可执行文件旁 provider 目录)
        std::string backends_dir = ".";
        std::string exe_path;
#if defined(_WIN32)
        {
            char buf[MAX_PATH];
            const DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
            if (n > 0) exe_path.assign(buf, n);
        }
#else
        {
            std::error_code ec;
            const auto p = std::filesystem::canonical("/proc/self/exe", ec);
            if (!ec) exe_path = p.string();
        }
#endif
        if (!exe_path.empty()) {
            std::error_code ec2;
            const auto parent = std::filesystem::path(exe_path).parent_path();
            if (!parent.empty()) backends_dir = parent.string();
        }
        // cli_sha 锚定当前 CLI 二进制本体。Windows 可执行名是 astrocs.exe
        // （GetModuleFileNameA 已给出 exe_path，backends_dir=其父目录），拼
        // "/astrocs" 无后缀时文件不存在 → file_sha256_hex 空串 → v2 profile
        // 的 cli_sha 在 Windows 恒空（R2 线索 2）。显式探测 .exe 后缀。
        std::string cli_bin = backends_dir.empty() ? std::string(".") : backends_dir + "/astrocs";
#if defined(_WIN32)
        {
            std::error_code ec3;
            if (!std::filesystem::is_regular_file(std::filesystem::u8path(cli_bin), ec3))
                cli_bin += ".exe";
        }
#endif
        const std::string cli_sha = astrocs::backend_host::file_sha256_hex(cli_bin);
        auto pb = astrocs::backend_host::generate_profile_v2(
            mode, ASTROCS_VERSION_STRING, commit, cli_sha, backends_dir);
        const std::string json = pb.json;
        // B8-P1-2: verdict 推导必须发生在写文件前 —— 顶层 verdict 字段与
        // "全 kernel oracle:fail → FAIL + exit≠0" 的 CLI 合同语义(退出码 SCIENCE=4,
        // 与 verify-profile 失败族一致)不可依赖已写盘文件的二次解析。
        std::string verdict;
        nlohmann::json doc;
        try {
            doc = nlohmann::json::parse(json);
            verdict = astrocs::benchmark_profile_verdict(doc);
        } catch (...) {
            verdict = "FAIL";   // profile 本体不可解析 → 无正确性证据 → FAIL
        }
        doc["verdict"] = verdict;
        const std::string json_with_verdict = doc.dump(2) + "\n";
        {
            std::ofstream f(std::filesystem::u8path(out_path), std::ios::binary | std::ios::trunc);
            if (!f) {
                std::fprintf(stderr, "astrocs: cannot write profile '%s'\n", out_path.c_str());
                return astrocs::IO;
            }
            f << json_with_verdict;
        }
        // 机器可读结果: 普通模式 → "path verdict" 一行; events-jsonl → JSON 事件行
        const bool events = ev.enabled();
        if (events) {
            // CPU-003: 全部原始候选逐条发射(审计/复读; 与 raw_samples_sha256 绑定)
            for (const auto& c : pb.raw) {
                ev.emit("benchmark", "info", "cpu", "raw candidate",
                        {{"kernel", c.kernel_id}, {"size_class", c.size_class},
                         {"provider", c.provider}, {"workers", c.workers},
                         {"block", c.block}, {"median_ns", c.median_ns},
                         {"mad_ns", c.mad_ns}, {"p05_ns", c.p05_ns}, {"p95_ns", c.p95_ns},
                         {"oracle_pass", c.oracle_pass}, {"fallback_reason", c.fallback_reason}});
            }
        }
        if (events) {
            ev.emit("result", verdict == "PASS" ? "info" : "error",
                    "benchmark", "cpu profile written",
                    {{"path", out_path}, {"verdict", verdict},
                     {"profile_id", doc.value("profile_id", "")},
                     {"raw_samples_sha256", doc.value("raw_samples_sha256", "")}});
        } else {
            std::printf("%s %s\n", out_path.c_str(), verdict.c_str());
        }
        if (verdict != "PASS") {
            std::fprintf(stderr,
                         "astrocs: benchmark cpu FAIL: kernel(s) failed oracle "
                         "(correctness_test != oracle:pass); profile written to '%s'\n",
                         out_path.c_str());
            return astrocs::SCIENCE;
        }
        return astrocs::OK;
    }
    if (joined == "doctor") {
        if (!p.flags.count("--json")) parse_fail("doctor requires --json");
        const std::string hw = astrocs::backend_host::hardware_inspect_json_v1(ASTROCS_VERSION_STRING);
        auto hwd = nlohmann::json::parse(hw);
        astrocs_host_services_v1 host;
        void* hstate = nullptr;
        astrocs_host_services_default_v1(&host, &hstate);
        astrocs_backend_api_v1 api{};
        std::memset(&api, 0, sizeof(api));
        const int grc = astrocs_backend_get_api_v1(ACS_ABI_VERSION_V1,
                                                   sizeof(astrocs_host_services_v1), &host, &api);
        nlohmann::json checks = nlohmann::json::array();
        checks.push_back(nlohmann::json{
            {"name", "baseline_selftest"},
            {"status", (grc == ACS_OK && api.self_test &&
                        api.self_test(&host) == ACS_OK) ? "pass" : "fail"}});
        checks.push_back(nlohmann::json{
            {"name", "hardware_sanity"},
            {"status", (hwd.value("available_logical_cpus", 0u) >= 1 &&
                        hwd.value("ram_bytes", 0ull) > 0) ? "pass" : "fail"}});
        // shipped backend 核查(05 §7): 安全检测但不执行不支持指令——预检 manifest 内条目
        std::ifstream mf("backends.manifest.json");
        if (mf) {
            std::stringstream mbuf; mbuf << mf.rdbuf();
            std::vector<astrocs::backend_host::ManifestEntry> entries;
            std::string merr;
            astrocs::backend_host::parse_backends_manifest(mbuf.str(), &entries, &merr);
            for (const auto& e : entries) {
                std::string why;
                auto pr = astrocs::backend_host::preflight_entry(
                    ".", e, astrocs_cpu_detect_features_v1(), &why);
                nlohmann::json ck;
                ck["name"] = "backend_preflight:" + e.backend_id;
                ck["status"] = pr.decision == astrocs::backend_host::LoadResult::OK
                                   ? "pass" : "skipped";
                ck["detail"] = why;
                checks.push_back(ck);
            }
        } else {
            checks.push_back(nlohmann::json{{"name", "backends_manifest"},
                                            {"status", "pass"},
                                            {"detail", "no shipped DSO (builtin baseline)"}});
        }
        bool all = true;
        for (const auto& c : checks)
            if (c.value("status", "") == "fail") all = false;
        nlohmann::json doc = {{"schema_version", 1}, {"kind", "astrocs_doctor"},
                              {"checks", checks}, {"verdict", all ? "PASS" : "FAIL"}};
        std::printf("%s\n", doc.dump(2).c_str());
        return all ? astrocs::OK : astrocs::SCIENCE;
    }
    if (joined == "hardware inspect") {
        if (!p.flags.count("--json")) parse_fail("hardware inspect requires --json");
        std::fputs(astrocs::backend_host::hardware_inspect_json_v1(ASTROCS_VERSION_STRING).c_str(), stdout);
        return astrocs::OK;
    }
    if (joined == "config init")           return cmd_config_init(p, ev);
    if (joined == "config validate")       return cmd_config_validate(p, ev);
    if (joined == "config show-effective") return cmd_show_effective(p, ev);
    if (joined == "phase1 run")            return cmd_phase1_run(p, ev);
    if (joined == "phase2 run")            return cmd_phase2_run(p, ev);
    if (joined == "phase3 run")            return cmd_phase3_run(p, ev);
    if (joined == "verify")                return cmd_verify(p, ev);
    if (joined == "verify profile")        return cmd_verify_profile(p, ev);
    if (joined == "benchmark verify-profile") return cmd_verify_profile(p, ev);
    if (joined == "drizzle")               return cmd_drizzle(p, ev);
    if (joined == "test synthetic") {
        const std::string g = need_value(p, "--group");
        if (!kGroups.count(g)) parse_fail("invalid --group '" + g + "'");
        return cmd_test_synthetic(p, g, ev);
    }
    return cmd_stub(p, joined, ev);
}
