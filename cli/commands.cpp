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



#include "backend_loader.h"

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
#include "monitor.h"
#include "resource_events.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif


#include "version_generated.h"

#include "cli_common.h"
#include "runtime_client.h"

// CLI→host 的取消/日志桥接(定义于后段, 此处前向声明供 cmd_run_pipeline 使用)
static int cli_cancel_probe(void*);
static void cli_session_log(void*, int, const char*, const char*);
// MON-002 资源/backend 事件发射(定义于后段, 前向声明供 cmd_run_pipeline 使用)
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
    ev.emit("artifact", "info", "config", "template written",
            {{"role", "config_template"}, {"path", out}});
    std::printf("%s\n", out.c_str());
    return astrocs::OK;
}

int cmd_config_validate(const Parsed& p, astrocs::JsonlEmitter& ev) {
    const std::string path = need_value(p, "--config");
    nlohmann::json doc;
    const int rc = validate_config_full(path, &doc);
    if (rc != astrocs::OK) return rc;
    ev.emit("artifact", "info", "config", "validated", {{"role", "config"}, {"path", path}});
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
    int failed = 0;
    for (const char* b : bins) {
        const std::string exe = bin_dir + "/" + b;
        ev.stage(("test_" + std::string(b)).c_str(), true);
        // 源码相对读取的测试 (p1_ir_facade/p2_ir_facade) 需要 ASTROCS_REPO;
        // 默认设为调用方 cwd, 可用环境变量覆盖。
        const char* repo_env = std::getenv("ASTROCS_REPO");
        // G9: 所有外部命令必须 timeout (ASTROCS_TEST_TIMEOUT_S 可配, 默认 600s)
        const char* tmo = std::getenv("ASTROCS_TEST_TIMEOUT_S");
        const std::string tmo_s = tmo ? tmo : "600";
        const std::string cmd = std::string("ASTROCS_REPO=") +
                                (repo_env ? repo_env : ".") + " timeout " + tmo_s + "s " + exe;
        const int rc = std::system(cmd.c_str());
        ev.stage(("test_" + std::string(b)).c_str(), rc == 0);
        if (rc != 0) {
            std::fprintf(stderr, "astrocs: synthetic test %s failed (rc=%d)\n", b, rc);
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
    ev.emit("artifact", "info", "manifest", "run manifest written",
            {{"role", "run_manifest"}, {"path", final_path},
             {"sha256", [&]{ bool ok=false; return file_sha256(final_path, &ok); }() }});
    // --events-jsonl 模式下 stdout 只能是 JSON 事件(04 §3): 路径已入 artifact 事件
    if (!ev.enabled()) std::printf("%s\n", final_path.c_str());
    return astrocs::OK;
}

int cmd_run_pipeline(const Parsed& p, astrocs::JsonlEmitter& ev) {
    const std::string cfg = need_value(p, "--config");
    nlohmann::json doc;
    int rc = validate_config_full(cfg, &doc);
    if (rc != astrocs::OK) return rc;
    bool ok = false;
    const std::string cfg_sha = file_sha256(cfg, &ok);
    if (!ok) return astrocs::INPUT;
    std::ifstream cf(std::filesystem::u8path(cfg), std::ios::binary);
    std::stringstream cb; cb << cf.rdbuf();
    const std::string cfg_text = cb.str();  // RT-008: Runtime 需要完整 config 文本
    // MON-002: --resource-detail summary|timeseries(07 §1); summary 默认强制。
    std::string resource_detail = p.values.count("--resource-detail")
                                      ? p.values.at("--resource-detail") : std::string("summary");
    if (resource_detail != "summary" && resource_detail != "timeseries")
        parse_fail("invalid --resource-detail (summary|timeseries)");
    // MON-002: 进程监控(07 §1 强制): run 阶段采样, 摘要内嵌于 resource 事件。
    astrocs::ProcessMonitor proc_mon(0.25);
    // --phases: 1|2|3 的非空升序无重复逗号子集(04 示例: 1,2,3) — 先于任何写操作
    std::vector<int> phases;
    int last = 0;
    for (char c : p.values.at("--phases")) {
        if (c == ',') continue;
        if (c < '1' || c > '3') parse_fail("invalid --phases");
        const int v = c - '0';
        if (v <= last) parse_fail("invalid --phases (must be ascending, unique)");
        last = v;
        phases.push_back(v);
    }
    if (phases.empty() || phases.size() > 3) parse_fail("invalid --phases");
    // 取消检查点(真实 sleep 钩子沿用 cmd_stub 语义; 内核取消点在 CODE 域接线)
    const char* sleep_ms = std::getenv("ASTROCS_TEST_SLEEP_MS");
    if (sleep_ms) {
        const long ms = std::strtol(sleep_ms, nullptr, 10);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        ev.stage("run_wait", true);
        while (std::chrono::steady_clock::now() < deadline) {
            if (astrocs::is_cancelled()) {
                ev.stage("run_wait", false);
                rc = write_run_manifest(doc.value("output_dir", "."), ev, "incomplete",
                                        "cancelled by user", cfg, cfg_sha, phases);
                if (rc != astrocs::OK) return rc;
                ev.emit_final(astrocs::CANCELLED, "cancelled", nullptr, "cancelled by user");
                std::fprintf(stderr, "astrocs: cancelled\n");
                return astrocs::CANCELLED;               // 04: 取消 → 9, manifest=incomplete
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        ev.stage("run_wait", false);
    }
    if (std::getenv("ASTROCS_TEST_CRASH")) throw std::runtime_error("selftest-crash");
    // ── RT-008: real orchestrator via Runtime (唯一执行路径) ──
    // run --phases 解析 preset→IR→Runtime: P1/P2/P3 节点一次调度, 经 ArtifactStore 连续。
    // 单 phase 命令(phase1/2/3 run)走同一 IR 子图(见 runtime_client), 不是第二条路径。
    const std::string out_dir = doc.value("output_dir", std::string("."));
    nlohmann::json all_artifacts = nlohmann::json::array();
    std::string fail_reason;
    int fail_phase = 0;

    // resume / hash-mismatch: 校验 output_dir 中所有 prior astrocs_run_*.json 记录的 artifact 哈希链;
    // 任一 prior artifact 磁盘 sha 与其记录不符 → 8(绝不静默跳过验证)→不进入新一轮运行。
    {
        bool mismatch = false;
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::u8path(out_dir), ec)) {
            const std::string fn = entry.path().filename().u8string();
            if (!entry.is_regular_file() || fn.rfind("astrocs_run_", 0) != 0 || fn.size() <= 14 ||
                fn.substr(fn.size() - 5) != ".json")
                continue;
            try {
                std::ifstream pf(entry.path(), std::ios::binary);
                nlohmann::json pm = nlohmann::json::parse(
                    std::string(std::istreambuf_iterator<char>(pf), {}));
                if (pm.value("kind", std::string()) != "astrocs_run_manifest") continue;
                for (const auto& a : pm.value("artifacts", nlohmann::json::array())) {
                    const std::string ap = a.value("path", std::string());
                    if (ap.empty()) continue;
                    if (!std::filesystem::exists(std::filesystem::u8path(ap), ec)) { mismatch = true; break; }
                    bool ok = false; const std::string sha = file_sha256(ap, &ok);
                    if (!ok || sha != a.value("sha256", std::string())) { mismatch = true; break; }
                }
            } catch (...) { mismatch = true; }
            if (ec) { mismatch = false; ec.clear(); }   // 目录遍历错误不算哈希不匹配
            if (mismatch) break;
        }
        if (mismatch) {
            rc = write_run_manifest(out_dir, ev, "incomplete", "resume hash mismatch",
                                    cfg, cfg_sha, phases);
            if (rc != astrocs::OK) return rc;
            ev.emit_final(astrocs::INTEGRITY, "resume_hash_mismatch", nullptr,
                          "prior artifact hash mismatch");
            std::fprintf(stderr, "astrocs: resume hash mismatch\n");
            return astrocs::INTEGRITY;   // 04/07: 输出完整性/验证失败 → 8
        }
    }

    // MON-002: 背景采样线程(进程 CPU/RSS 在 run 期间累计), 000 后 join; 禁硬编码。
    std::atomic<bool> mon_stop{false};
    std::thread mon_thread([&proc_mon, &mon_stop]() {
        while (!mon_stop.load()) {
            proc_mon.tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    });

    // RT-008: 通过 Runtime 执行全部 phase(单次调度; IR 由 runtime_client 构建)。
    for (int phase : phases) ev.stage(("run_phase" + std::to_string(phase)).c_str(), true);
    {
        const uint32_t budget = cli_affinity_cpu_count();
        const int rrc = astrocs::cli::run_pipeline(phases, cfg_text, budget, &fail_reason);
        if (rrc != astrocs::OK && fail_reason.empty()) {
            fail_reason = "runtime pipeline failed (exit " + std::to_string(rrc) + ")";
        }
        // 失败时按首个失败 phase 定位(manifest 不变量与旧行为一致)
        if (!fail_reason.empty()) {
            for (int ph : phases) {
                // Runtime 节点失败信息在 fail_reason 中; 定位不到具体 phase 时用最小 phase
                if (fail_phase == 0) fail_phase = ph;
            }
        }
        // 收集节点 manifest(cal=1, res=2, hips=3) → artifact 哈希链
        std::vector<std::pair<std::string, std::string>> mans;
        astrocs::cli::collect_node_manifests(&mans);
        for (const auto& [nid, mtext] : mans) {
            int phase = (nid == "cal") ? 1 : (nid == "res") ? 2 : 3;
            nlohmann::json m;
            try { m = nlohmann::json::parse(mtext); } catch (...) { continue; }
            const std::string role = (phase == 1) ? "phase1_output"
                                     : (phase == 2) ? "phase2_output" : "phase3_output";
            for (const auto& a : m.value("artifacts", nlohmann::json::array())) {
                const std::string ap = a.get<std::string>();
                bool sok = false; const std::string sha = file_sha256(ap, &sok);
                std::error_code ec2;
                const auto sz = std::filesystem::file_size(std::filesystem::u8path(ap), ec2);
                all_artifacts.push_back({{"role", role}, {"path", ap},
                                         {"sha256", sok ? sha : ""},
                                         {"size_bytes", ec2 ? 0ULL : static_cast<unsigned long long>(sz)}});
            }
            // phase3 附加 output_fits_path 产物
            if (phase == 3) {
                const std::string op = m.value("output_fits_path", std::string());
                if (!op.empty()) {
                    bool sok = false; const std::string sha = file_sha256(op, &sok);
                    std::error_code ec2;
                    const auto sz = std::filesystem::file_size(std::filesystem::u8path(op), ec2);
                    all_artifacts.push_back({{"role", "phase3_output"}, {"path", op},
                                             {"sha256", sok ? sha : ""},
                                             {"size_bytes", ec2 ? 0ULL : static_cast<unsigned long long>(sz)}});
                }
            }
        }
    }
    for (int phase : phases) ev.stage(("run_phase" + std::to_string(phase)).c_str(), false);
    mon_stop.store(true);
    mon_thread.join();

    // 取消检查(任一阶段取消)
    if (astrocs::is_cancelled()) {
        rc = write_run_manifest(out_dir, ev, "incomplete", "cancelled by user",
                                cfg, cfg_sha, phases, all_artifacts);
        if (rc != astrocs::OK) return rc;
        ev.emit_final(astrocs::CANCELLED, "cancelled", nullptr, "cancelled by user");
        return astrocs::CANCELLED;
    }
    if (!fail_reason.empty()) {
        rc = write_run_manifest(out_dir, ev, "incomplete", "phase" + std::to_string(fail_phase) +
                                " failed: " + fail_reason, cfg, cfg_sha, phases, all_artifacts);
        if (rc != astrocs::OK) return rc;
        ev.emit_final(astrocs::SCIENCE, "run_failed", nullptr, fail_reason);
        std::fprintf(stderr, "astrocs: run failed: %s\n", sanitize(fail_reason).c_str());
        return astrocs::SCIENCE;
    }

    // (resume/hash-mismatch 校验移到 phase 循环之前执行)
    rc = write_run_manifest(out_dir, ev, "complete", "run ok", cfg, cfg_sha, phases, all_artifacts);
    if (rc != astrocs::OK) return rc;
    // MON-002: 结束监控, 发射资源分层事件(07 §1 强制 summary) + backend 事件(07 §2 必采)。
    const auto proc_summary = proc_mon.summary();
    emit_resource_summary(ev, "pipeline", proc_summary, out_dir, proc_summary.n_samples,
                          resource_detail);
    // backend 选择: 从 config 读取(无则 baseline); workers 取 affinity(有效核)。禁硬编码。
    emit_backend_event(ev, "pipeline", doc.value("backend", "baseline"), "ok",
                       std::max(1u, proc_mon.n_cores_hint()), proc_mon.n_cores_hint());
    ev.emit("resource", "info", "pipeline", "run summary",
            {{"n_phases", phases.size()}, {"n_artifacts", all_artifacts.size()}});
    ev.emit_final(astrocs::OK, "ok", nullptr, "run complete");
    return astrocs::OK;
}

// phase1 run: CLI-004 — 进程内调用 p1_session(无 shell-out); cancel/budget/monitor 注入
static int cli_cancel_probe(void*) { return astrocs::is_cancelled() ? 1 : 0; }
static void cli_session_log(void*, int level, const char* component, const char* msg) {
    static const char* kLv[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    std::fprintf(stderr, "[astrocs:%s][%s] %s\n",
                 kLv[level & 3], component ? component : "phase1", msg ? msg : "");
}

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
    ev.emit("resource", "info", phase, "resource summary", payload);
}

// MON-002: backend 事件(backend_id/status); 反映所选 backend 与 worker 选择(07 §2 必采)。
static void emit_backend_event(astrocs::JsonlEmitter& ev, const std::string& phase,
                               const std::string& backend_id, const std::string& status,
                               uint32_t workers_used, uint32_t available_cpus) {
    ev.emit("backend", "info", phase, status,
            {{"backend_id", backend_id}, {"workers_used", workers_used},
             {"available_cpus", available_cpus}});
}

// MON-002: 无标注 >5s 区间判 P1(供 MON-003 gating; 本函数仅供测试与 stage 落地校验)。
[[maybe_unused]] static bool is_stage_priority(const char* annotation, double wall_seconds) {
    return astrocs::is_unannotated_priority(annotation, wall_seconds);
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
    const int rrc = astrocs::cli::run_pipeline({2}, cfg_text, budget, &fail_reason);
    ev.stage("phase2_session", false);

    nlohmann::json artifacts = nlohmann::json::array();
    std::vector<std::pair<std::string, std::string>> mans;
    astrocs::cli::collect_node_manifests(&mans);
    for (const auto& [nid, mtext] : mans) {
        if (nid != "res") continue;
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
    // RT-008: 从节点 manifest 读真实科学值（session inspect 摘要）
    uint64_t n_inputs = 0, n_obs = 0;
    for (const auto& [nid, mtext] : mans) {
        if (nid != "res") continue;
        try {
            auto m = nlohmann::json::parse(mtext);
            n_inputs = m.value("n_inputs", 0ull);
            n_obs = m.value("n_obs", 0ull);
        } catch (...) {}
    }
    ev.emit("resource", "info", "phase2", "session summary",
            {{"n_inputs", n_inputs}, {"n_obs", n_obs}});
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
    const int rrc = astrocs::cli::run_pipeline({3}, cfg_text, budget, &fail_reason);
    ev.stage("phase3_session", false);

    nlohmann::json artifacts = nlohmann::json::array();
    std::vector<std::pair<std::string, std::string>> mans;
    astrocs::cli::collect_node_manifests(&mans);
    for (const auto& [nid, mtext] : mans) {
        if (nid != "hips") continue;
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
        const std::string op = m.value("output_fits_path", std::string());
        if (!op.empty()) {
            bool ok2 = false;
            const std::string sha = file_sha256(op, &ok2);
            std::error_code ec;
            const auto size = std::filesystem::file_size(std::filesystem::u8path(op), ec);
            artifacts.push_back({{"path", op}, {"sha256", ok2 ? sha : ""},
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
    ev.emit("resource", "info", "phase3", "session summary",
            {{"outputs", artifacts.size()}});
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
    const int rrc = astrocs::cli::run_pipeline({1}, cfg_text, budget, &fail_reason);
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
    ev.emit("resource", "info", "phase1", "frames processed",
            {{"frames", artifacts.size()}});
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



int cmd_drizzle(const Parsed& p, astrocs::JsonlEmitter& ev) {
    (void)p;
    ev.emit_final(astrocs::ARGS, "test_preset_only", nullptr,
                  "drizzle 命令仅支持测试 preset：请用 'astrocs test synthetic --group drizzle'");
    std::fprintf(stderr, "astrocs: drizzle 命令仅支持测试 preset "
                         "(test synthetic --group drizzle)；生产 HiPS 投影请用 phase2 run\n");
    return astrocs::ARGS;
}

// dispatch: 外部可见（cli_common.h 声明；main.cpp 调用）。
int dispatch(const Parsed& p) {
    const std::string joined = p.join();
    const bool events = p.flags.count("--events-jsonl") > 0;
    const std::string phase_name =
        (joined == "run") ? "pipeline" : (joined.rfind("phase", 0) == 0 ? joined.substr(0, 6) : joined);
    astrocs::JsonlEmitter ev(events, astrocs::make_run_id(), phase_name);

    if (joined == "--version") {
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
    if (joined == "benchmark cpu") {
        const bool quick = p.flags.count("--quick") > 0;
        const bool full = p.flags.count("--full") > 0;
        if (quick == full) parse_fail("benchmark cpu requires exactly one of --quick|--full");
        const std::string out_path = p.values.count("--output") ? p.values.at("--output")
                                                                : "cpu_profile.json";
        const std::string mode = quick ? "quick" : "full";
        const std::string commit = ASTROCS_COMMIT_SHA;
        const std::string json = astrocs::backend_host::generate_profile_json(
            mode, ASTROCS_VERSION_STRING, commit, ASTROCS_COMMIT_SHA);
        {
            std::ofstream f(std::filesystem::u8path(out_path), std::ios::binary | std::ios::trunc);
            if (!f) {
                std::fprintf(stderr, "astrocs: cannot write profile '%s'\n", out_path.c_str());
                return astrocs::IO;
            }
            f << json;
        }
        // 机器可读结果(stdout 简洁结果): 输出路径+verdict
        try {
            auto doc = nlohmann::json::parse(json);
            std::printf("%s %s\n", out_path.c_str(),
                        doc.value("verdict", "FAIL").c_str());
        } catch (...) {
            std::printf("%s\n", out_path.c_str());
        }
        ev.emit("artifact", "info", "benchmark", "cpu profile written",
                {{"role", "cpu_profile"}, {"path", out_path}});
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
    if (joined == "run")                   return cmd_run_pipeline(p, ev);
    if (joined == "drizzle")               return cmd_drizzle(p, ev);
    if (joined == "test synthetic") {
        const std::string g = need_value(p, "--group");
        if (!kGroups.count(g)) parse_fail("invalid --group '" + g + "'");
        return cmd_test_synthetic(p, g, ev);
    }
    return cmd_stub(p, joined, ev);
}
