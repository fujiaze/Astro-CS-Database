// astrocs CLI — 命令实现 (RT-008 拆分自 main.cpp)
// 具体命令 + dispatch + 进程监控/资源事件桥接 + session/AIO/Drizzle 接线。
// 本文件允许 include 科学内部头（CHK-001 只扫描 lib/infrastructure/cli/main.cpp）。
// 头部 include 顺序与原 main.cpp 逐字节一致, 保证宏/类型可见性等价。
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#if defined(__GLIBC__)
#include <malloc.h>   // MON-002 reclaim: malloc_trim 归还线程 arena 空闲块
#endif
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

// DET-001: 规范产品哈希（canonical product hash）—— run manifest 的 artifact
// 行同时登记「完整性」与「可复现」两个哈希, 命名区分（禁一名两义）。
#include "astrocs/core/canonical_hash.h"

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
#include "disk_gate.h"        // §9.74 裁决 10: 磁盘门 = 唯一资源判据（内存/CPU/线程不设门）
#include "resource_events.h"
#include "resource_gate.h"
#include "astrocs/core/context.h"  // B2-A18: 租约授予观测
#include "astrocs/core/memory_budget.h"  // 内存静态预算来源解析（§8.3）
#include "aio_sysinfo.h"                 // 可用内存唯一探测实现（aio 边界）
#include "aio_file_io.h"                 // 文件读取唯一机制原语（aio 边界）
#include "v6_runtime_contract.h"   // RUNTIME-CI-001: 统一预算/模式路由/SO-05 策略单一来源

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif


#include "version_generated.h"
// RUN-PROVENANCE-01: 构建期指纹（run manifest provenance 的"同一代码"判据）。
#include "astrocs/core/build_stamp.h"

#include "astrocs/core/module_adapters.h"  // B2-A10: write_run_context 唯一路径

#include "cli_common.h"
#include "runtime_client.h"
#include "v6_mode_gate.h"   // RUNTIME-CI-001: V6 显式模式路由门

// CLI-001: 三个平级用户命令（normalize/mosaic/export）的入口实现在
// lib/infrastructure/cli/**；本文件是它们背后的会话层（执行/校验/计划/检视）。
// 相对路径: 根 CMakeLists.txt 的 astrocs target include 目录尚未登记本模块
// （登记属 INT-001 域），此处用工作区相对包含保证根图与 lib/infrastructure/cli/ 独立图都能编译。
#include "command_tree.h"
#include "session_commands.h"
#include "subcommand.h"
#include "normalize/normalize.h"
#include "mosaic/mosaic.h"
#include "export/export.h"

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


// ── 运行期内存静态预算解析（§8.3「静态预算」的**来源**面） ───────────────────
// 依据（逐条）：
//   * 负责人裁决 2026-09-22（逐字）：「我是不设置上限，有多少资源吃多少资源。（内存
//     最高吃掉空闲的 95% 避免卡死，且**这个参数配置在 config 里面可调，默认 95**）」
//   * ASTROCS_DESIGN.md §8.3（静态预算 + 内存占用永不越界）、§9（一个进程一个资源
//     调度器与线程预算源）、§3.5（资源门只管磁盘 ⇒ 本预算是调度准入输入，不是门禁）
//   * docs/contracts/SCHEDULER_CONTRACT.md §3（内存上限由配置/资源门决定，禁止硬编码）
// 两个输入：
//   ① 可用内存 = aio_system_available_memory_bytes()（aio 是文件级唯一 I/O 边界；
//      Linux 口径 = MemAvailable（含可回收 page cache）∩ cgroup 内存余量；
//      Windows = ullAvailPhys。返回 0 = 不可判定）。
//   ② 比例 = 机器绑定 profile（--cpu-profile 指向的 astrocs.cpu-profile/v2 的
//      host.memory_budget_percent）优先；未声明/非法 ⇒ 用 config 默认值（95，唯一数值源
//      = eng/packaging/config/runtime_resources.json）。
// 两者都不含硬编码内存上限：比例默认值来自生成头，可用内存来自实测探测。
static uint32_t cli_memory_budget_percent(const std::string& cpu_profile_path) {
    if (cpu_profile_path.empty()) return 0;   // 0 = 未配置 ⇒ resolve 取默认
    try {
        // 文件读取经 aio 唯一机制原语（aio_file_io.h；aio 边界内唯一 fopen/fread 实现），
        // 本 TU 不复制第二份打开/读取通道 —— 与 core/canonical_hash.cpp 同款 PRIVATE include
        // 面（CHK-AIO-IO-BOUNDARY 的棘轮台账因此零增长）。
        std::string text;
        if (!aio_file::read_all(cpu_profile_path.c_str(), &text)) return 0;
        const nlohmann::json doc = nlohmann::json::parse(text);
        if (!doc.is_object()) return 0;
        // profile 失配（非 v2）⇒ 按 V8-CPU-002 回落，不阻塞、不报错。
        if (doc.value("schema", std::string()) != "astrocs.cpu-profile/v2") return 0;
        if (!doc.contains("host") || !doc["host"].is_object()) return 0;
        const nlohmann::json& h = doc["host"];
        if (!h.contains("memory_budget_percent")) return 0;
        const nlohmann::json& v = h["memory_budget_percent"];
        if (!v.is_number_integer() && !v.is_number_unsigned()) return 0;
        const long long p = v.get<long long>();
        if (p < 1 || p > 100) return 0;  // 非法值 ⇒ 回落默认（由 resolve 记录）
        return static_cast<uint32_t>(p);
    } catch (...) {
        return 0;   // profile 不可读/不可解析 ⇒ 回落默认，不阻断运行
    }
}

// 解析出 (上限, 来源) 并落一条可核对的 stderr 事实行（与既有 "session run: budget workers="
// 同款；不进协议事件字段面，避免改冻结的事件 schema）。
static void cli_resolve_memory_budget(const std::string& cpu_profile_path,
                                      uint64_t* limit_out, std::string* source_out) {
    const uint64_t avail = aio_system_available_memory_bytes();
    const uint32_t pct = cli_memory_budget_percent(cpu_profile_path);
    const astrocs::core::MemoryBudget mb = astrocs::core::resolve_memory_budget(avail, pct);
    if (limit_out) *limit_out = mb.limit_bytes;
    if (source_out) *source_out = astrocs::core::memory_budget_source_name(mb.source);
    std::fprintf(stderr,
                 "session run: memory budget=%llu B (available=%llu B, percent=%u, source=%s)\n",
                 static_cast<unsigned long long>(mb.limit_bytes),
                 static_cast<unsigned long long>(mb.available_bytes),
                 static_cast<unsigned>(mb.percent),
                 astrocs::core::memory_budget_source_name(mb.source));
    std::fflush(stderr);
}

// ───────────────────── 具体命令实现 ─────────────────────

// config 模板(CLI-002 最小骨架; 完整 config schema 属 CLI-003)
const char* kConfigTemplate =
    "{\n"
    "  \"schema_version\": \"1\",\n"
    "  \"inputs\": {\"lights\": [], \"darks\": [], \"flats\": [], \"bias\": []},\n"
    "  \"output_dir\": \".\"\n"
    "}\n";

// DET-001: run manifest 的 artifact 行补充规范化哈希字段。
//   sha256            —— CLI-004 冻结词表字段（= 整文件 sha256, 保持不变）
//   integrity_sha256  —— 同值, 但名字显式声明其口径（完整性/防改动）
//   canonical_sha256  —— 规范产品哈希（像素数据 + 科学元数据）=> 可复现性判据
//   canonical_hash_spec / canonical_format —— 口径版本与判定到的格式（审计面）
// 规范化失败不静默: 记 canonical_sha256=null + canonical_error。
// PERF-P2 S2: 复用已算出的 CanonicalHashResult（同一 path 只读一遍）。
// integrity_sha256 取 ch.integrity_sha256 —— canonical_product_hash_file 在读入
// 同一文件字节时同步算出整文件 sha256（raw 分支为单遍流式）, 与 file_sha256(path)
// 同值; 读失败时两者同为空。因此本重载与下方两参版本输出逐字节一致, 仅省掉一次
// 重复整文件读 + 哈希。
nlohmann::json with_canonical_hash(const nlohmann::json& row,
                                   const astrocs::core::CanonicalHashResult& ch) {
    nlohmann::json out = row;
    out["integrity_sha256"] = ch.integrity_sha256;
    if (ch.ok) {
        out["canonical_sha256"] = ch.canonical_sha256;
        out["canonical_hash_spec"] = astrocs::core::kCanonicalProductHashSpec;
        out["canonical_format"] = ch.format;
    } else {
        out["canonical_sha256"] = nullptr;
        out["canonical_error"] = ch.error;
    }
    return out;
}

nlohmann::json with_canonical_hash(const nlohmann::json& row, const std::string& path) {
    return with_canonical_hash(row, astrocs::core::canonical_product_hash_file(path));
}

// CLI-001: 合成测试门与 stub 用户命令已删除（不在 §6.2 唯一命令树内）——
// 它们是开发期工具，不是产品命令面；旧入口现在解析失败 → exit 2。
// 合成测试直接跑 build 树内测试二进制（eng/tests/unit/**、eng/tests/system/**）。
// B2-A10（宪章 §4.3/§4.2）: 运行上下文（run_id/source SHA/软件版本）在会话启动
// 前写入 output_dir，供各 phase 的 provenance 消费端读取（禁节点级占位串）。
// 生成逻辑唯一实现 = astrocs::core::write_run_context（node 级测试夹具同源复用）；
// 本处仅做 Result→CLI exit code 映射，与 run manifest 同序，绝不半写。
int write_run_context(const std::string& out_dir, const std::string& run_id) {
    auto rc = astrocs::core::write_run_context(out_dir, run_id, ASTROCS_VERSION_STRING,
                                               ASTROCS_COMMIT_SHA);
    if (rc.failed()) {
        std::fprintf(stderr, "astrocs: cannot write run context: %s\n",
                     rc.error().message().c_str());
        return astrocs::IO;
    }
    return astrocs::OK;
}

// B2-A10（宪章 §4.3）: run manifest provenance 子对象。
// 字段来源（不造占位）:
//   source_sha/source_version  = version_generated.h（**configure 期**采样；只表示
//                                configure 时刻的 HEAD，不是构建指纹）;
//   build_head_sha/build_dirty/build_source_digest/configure_head_sha
//                              = 构建期指纹（RUN-PROVENANCE-01，build_stamp.h）;
//                                **判"同一代码/同一二进制"只看 build_source_digest**;
//   algorithm_ids/module_build_ids/provider = 各真实节点 manifest（节点自报）;
//   units/coordinate_frame     = 节点 manifest 实际 BUNIT/frame（缺则省略）;
//   input_product_hashes       = 节点自报的 input_manifest_hash（如 P3 writer）;
//   output_product_hashes      = artifacts[] 中带 sha256 的产物（role/path/sha）。
nlohmann::json build_run_provenance(
    const std::vector<std::pair<std::string, std::string>>& mans,
    const nlohmann::json& artifacts) {
    nlohmann::json p = nlohmann::json::object();
    p["source_sha"] = ASTROCS_COMMIT_SHA;
    p["source_version"] = ASTROCS_VERSION_STRING;
    // RUN-PROVENANCE-01: 构建期指纹（additive；CLI-003 §2.1 的 provenance 是加性
    // 扩展，v1 校验器忽略未知子键）。没有它，两份 provenance 的 source_sha 相等
    // **不蕴含**同一二进制 —— 见 run/RUN-PROVENANCE-01/REPORT.md §A。
    const astrocs::core::BuildStamp& bstamp = astrocs::core::build_stamp();
    p["build_head_sha"] = bstamp.head_sha;
    p["build_dirty"] = bstamp.dirty;
    p["build_source_digest"] = bstamp.source_digest;
    p["configure_head_sha"] = bstamp.configure_head_sha;
    std::set<std::string> algs, builds, providers, units, frames;
    nlohmann::json in_hashes = nlohmann::json::array();
    for (const auto& [nid, mtext] : mans) {
        nlohmann::json m;
        try { m = nlohmann::json::parse(mtext); } catch (...) { continue; }
        if (!m.is_object()) continue;
        if (m.contains("algorithm_id") && m["algorithm_id"].is_string())
            algs.insert(m["algorithm_id"].get<std::string>());
        if (m.contains("module_build_id") && m["module_build_id"].is_string())
            builds.insert(m["module_build_id"].get<std::string>());
        if (m.contains("provider") && m["provider"].is_string())
            providers.insert(m["provider"].get<std::string>());
        if (m.contains("bunit") && m["bunit"].is_string())
            units.insert(m["bunit"].get<std::string>());
        if (m.contains("coordinate_frame") && m["coordinate_frame"].is_string())
            frames.insert(m["coordinate_frame"].get<std::string>());
        if (m.contains("input_manifest_hash") && m["input_manifest_hash"].is_string())
            in_hashes.push_back({{"node", nid},
                                 {"sha256", m["input_manifest_hash"].get<std::string>()}});
    }
    p["algorithm_ids"] = nlohmann::json(algs);
    p["module_build_ids"] = nlohmann::json(builds);
    p["providers"] = nlohmann::json(providers);
    p["units"] = nlohmann::json(units);
    p["coordinate_frames"] = nlohmann::json(frames);
    p["input_product_hashes"] = in_hashes;
    nlohmann::json out_hashes = nlohmann::json::array();
    if (artifacts.is_array()) {
        for (const auto& a : artifacts) {
            if (!a.is_object()) continue;
            const std::string ap = a.value("path", std::string());
            const std::string h = a.value("sha256", std::string());
            if (ap.empty() || h.empty()) continue;
            nlohmann::json row = {{"path", ap}, {"sha256", h}};
            if (a.contains("role")) row["role"] = a["role"];
            out_hashes.push_back(std::move(row));
        }
    }
    p["output_product_hashes"] = out_hashes;
    return p;
}

// ── 磁盘门运行期臂（§9.74 裁决 10；ASTROCS_DESIGN §3.5「运行中写盘失败/磁盘满 ⇒ 报错
// （fail-closed）」+ §6.3「exit 10 = 磁盘写满 / 写盘失败」）──
// 判定唯一实现 = lib/infrastructure/cli/disk_gate.h（classify_write_failure / probe_writable）；
// 本函数只做「落退出码 + 发 error 事件」，不重复实现判据，也不引入任何内存/CPU/线程门。
// 返回：磁盘写满/写盘失败 ⇒ RESOURCE(10)；其它写失败 ⇒ IO(7)（维持既有 I/O 失败语义）。
// what = **标签**（如 run_manifest / resource_timeseries.csv / output_dir），不是路径 ——
// 自由文本不得携带绝对路径（脱敏纪律）；真实落点走结构化字段 output_dir（机器通道）。
static int disk_write_failure_exit(astrocs::JsonlEmitter& ev, const std::string& phase,
                                   const std::string& out_dir, const std::string& what,
                                   const astrocs::WriteProbe& pr,
                                   const std::string& detail = std::string()) {
    const std::string kind = astrocs::write_failure_kind_name(pr.kind);
    nlohmann::json payload = {
        {"diag", "disk_write_failure:" + kind},
        {"failure_kind", kind},
        {"errno", pr.err},
        {"write_target", what},
        {"output_dir", out_dir},
        // §4 resource 冻结扩展字段（硬闸要求）: 失败路径取 0 哨兵, 不伪造测量值。
        {"cpu_cores_used", 0.0},
        {"rss_bytes", 0},
        {"io_read_bytes", 0},
        {"io_write_bytes", 0},
        {"threads", 0},
    };
    if (!detail.empty()) payload["detail"] = detail;
    ev.emit("resource", "error", phase,
            "disk write failed (" + kind + "): " + what, payload);
    std::fprintf(stderr, "astrocs: disk write failed (%s): %s\n", kind.c_str(),
                 sanitize(what).c_str());
    return astrocs::write_failure_is_resource_exit(pr.kind) ? astrocs::RESOURCE : astrocs::IO;
}

// run manifest v1 原子写(tmp+rename; ARCH-002 §5 单元): stub/not-wired/cancelled 恒 incomplete
int write_run_manifest(const std::string& out_dir, astrocs::JsonlEmitter& ev, const std::string& status,
                       const std::string& summary, const std::string& config_path,
                       const std::string& config_sha, const std::vector<int>& phases,
                       const nlohmann::json& artifacts = nlohmann::json::array(),
                       const nlohmann::json& extra = nlohmann::json()) {
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
    // SMOKE-001 D7: 失败/取消的 manifest 必须自带失败原因字段，供机器消费者
    // 在没有进程 rc 的情况下判定（此前只有 status:"incomplete"，原因仅在 stderr）。
    if (status != "complete") m["error"] = {{"message", summary}};
    // FIX-E2E B1-A5/A10: 节点级科学事实（如 uncertainty_available）并入 run manifest，
    // 单一来源 = 节点 manifest，不在 CLI 另造。键缺失即不写（禁占位）。
    if (extra.is_object()) {
        for (auto it = extra.begin(); it != extra.end(); ++it) m[it.key()] = it.value();
    }
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::u8path(out_dir), ec);
    const std::string final_path = out_dir + "/astrocs_run_" + ev.run_id() + ".json";
    const std::string tmp_path = final_path + ".tmp";
    std::string body;
    {
        std::ofstream f(std::filesystem::u8path(tmp_path), std::ios::binary | std::ios::trunc);
        if (!f) {
            std::fprintf(stderr, "astrocs: cannot write run manifest '%s'\n", tmp_path.c_str());
            // §9.74 裁决 10: 写盘失败/磁盘满 ⇒ fail-closed（磁盘满 → 10；其它 → 7）。
            return disk_write_failure_exit(ev, "manifest", out_dir, "run_manifest",
                                           astrocs::probe_writable(out_dir),
                                           "run manifest open failed");
        }
        body = m.dump(2) + "\n";
        f << body;
        // §9.74 裁决 10: ofstream 的缓冲失败可能延迟到析构 flush —— 必须显式 flush 并
        // 用落盘字节数核对（旧实现只看 f.good() ⇒ 磁盘满时静默产出 0 字节 manifest
        // 且照发 "run manifest written" 事件 = fail-open）。
        f.flush();
        if (!f.good()) {
            return disk_write_failure_exit(ev, "manifest", out_dir, "run_manifest",
                                           astrocs::probe_writable(out_dir),
                                           "run manifest write failed");
        }
    }
    {
        std::error_code sec;
        const auto tsz = std::filesystem::file_size(std::filesystem::u8path(tmp_path), sec);
        if (sec || static_cast<std::size_t>(tsz) != body.size()) {
            return disk_write_failure_exit(ev, "manifest", out_dir, "run_manifest",
                                           astrocs::probe_writable(out_dir),
                                           "run manifest truncated (size mismatch)");
        }
    }
    std::filesystem::rename(std::filesystem::u8path(tmp_path), std::filesystem::u8path(final_path), ec);
    if (ec) {
        std::fprintf(stderr, "astrocs: cannot finalize run manifest: %s\n", ec.message().c_str());
        return disk_write_failure_exit(ev, "manifest", out_dir, "run_manifest",
                                       astrocs::probe_writable(out_dir),
                                       "run manifest finalize failed: " + ec.message());
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
    // §4 final.run_manifest 回填（SMOKE-001 D8）：登记本次 manifest 路径。
    ev.set_run_manifest(final_path);
    // §9.74 裁决 7-a 定案 1（ASTROCS_DESIGN §6.3）：事件流 = **默认输出** ⇒ stdout 只承载
    // 机器 JSONL（无日志污染），人可读摘要由 JsonlEmitter 同源写到 stderr。旧「人类模式把
    // manifest 路径打到 stdout」的分支随之退役（manifest 路径 = artifact 事件的 path 字段，
    // final 事件回填 run_manifest）——不得再往 stdout 打非 JSON 文本。
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
    // 仅当 eng/eng/tools/quality/gen_run_graphs.py 存在时调用; timeout 30s 防悬挂。
    // B8-P1-1b: 弃用 std::system 拼接（gdir 无引号+单引号逃逸+返回值丢弃 →
    // 渲染失败时主平台成功 run 的图产物静默缺失）→ 进程 API argv 传参 +
    // 显式检查子进程 exit code，失败 warning 事件 + stderr（不静默；不失败 run，
    // RT-009 冻结语义保留，但产物缺失必须可诊断）。
    {
        const char* env_repo = std::getenv("ASTROCS_REPO");
        const std::string repo = (env_repo && env_repo[0]) ? env_repo : ".";
        const std::string renderer = repo + "/eng/tools/quality/gen_run_graphs.py";
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

// MON-002: 发射资源 summary 事件。
// summary 事件内嵌指标; 原始时序只记录留存路径+样本数(不内嵌几十 MB 数据)。
// GATE-FIX-RES(R-4 D-14/D-15): 分层档 detail 参数与 curve_points 恒空数组已删除;
// 曲线唯一载体 = raw_dir/resource_timeseries.csv(字段 resource_curve_artifact 声明)。
static void emit_resource_summary(astrocs::JsonlEmitter& ev, const std::string& phase,
                                  const astrocs::ProcessMonitor::Summary& s,
                                  const std::string& raw_dir, std::size_t raw_n) {
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
        // 曲线载体显式声明(不再有恒空 curve_points 数组误导下游)。
        {"resource_curve_artifact", "resource_timeseries.csv"},
        {"raw_dir", raw_dir},
        {"raw_n", raw_n},
    };
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

// MON-004 资源观测生产接线(lib/infrastructure/cli/resource_gate.h 唯一生产调用点):
// 后台线程对 run_pipeline 执行期采样(ProcessMonitor::tick), 结束后按 07 合同
// evaluate_gate 判定 —— 判定结果**只记录**(resource/resource_gate 事件 + 资源三产物)。
// §9.74 裁决 10（负责人逐字「不应该有资源超限（除非存储不足）……只考虑磁盘写满这一个问题」）:
// **一般性资源超限门已取消** ⇒ CPU/内存/线程判据**不产生任何退出码**（rc=10 路径已删），
// 唯一资源门 = 磁盘门（disk_gate.h；跑前 warn / 运行中写盘失败 error + exit 10）。
// 短任务判定域由 evaluate_gate 内建(NotApplicable), 冒烟小测不受影响。
// kind 固定 Compute: phase1/2/3 均为 cpu_heavy 合成管线(runtime_client.cpp
// resources.class=cpu_heavy); io/mem 类判据属 benchmark 专用路径, 不在 CLI run。
// GATE-FIX-RES(R-4 D-14): resource_detail_arg() 已删除 —— 该旗标从未进入命令树
// 白名单（lib/infrastructure/cli/command_tree.h 未登记, 真 CLI 报
// "unknown flag '--resource-detail'" rc=2）, 属**死代码**; 唯一曲线载体为磁盘工件
// resource_timeseries.csv（summary 事件 raw_dir/raw_n + resource_curve_artifact）。

// 资源门「记录/裁决分离」开关解析（**历史复现开关，已退役**）。
//   §9.74 裁决 10: 一般性资源超限门已取消 ⇒ 资源判据**恒为 record-only**（无 rc=10 路径）；
//   本开关保留接受并在事件里如实登记（strict_flag_requested），但**不再**改变裁决。
//   --on-resource-gate accept|record: 与默认等价的显式写法(端到端脚本兼容)。
// 非法取值 → ARGS(2), 拒绝静默降级（登记现状，不赋予合同承诺）。
static bool strict_resource_gate_arg(const Parsed& p) {
    if (p.flags.count("--strict-resource-gate")) return true;
    if (!p.values.count("--on-resource-gate")) return false;
    const std::string v = p.values.at("--on-resource-gate");
    if (v == "strict" || v == "enforce") return true;
    if (v == "accept" || v == "record" || v == "record-only") return false;
    throw ParseError("invalid --on-resource-gate '" + v + "' (accept|strict)");
}

static int run_with_resource_gate(astrocs::JsonlEmitter& ev, const std::string& phase,
                                  const std::string& cfg_text, uint32_t budget,
                                  std::string& fail_reason,
                                  astrocs::ProcessMonitor::Summary* summary_out = nullptr,
                                  bool strict_flag_requested = false,
                                  const std::string& cpu_profile_path = std::string()) {
    // 输出落点（磁盘门探测 + 资源产物落点同源；块级 output_dir 由调用方展开进 cfg_text）。
    const std::string res_out_dir = [&] {
        try { return nlohmann::json::parse(cfg_text).value("output_dir", std::string(".")); }
        catch (...) { return std::string("."); }
    }();
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
    // RESCUE-FD-08(短 run 观测链): active 阶段标注与起始 worker 容量必须在采样
    // 线程首次 record 之前就绪, 且主线程要等到第一个 active 样本落盘再启动
    // run_pipeline —— 否则 wall < 采样周期(0.5s)的 run 与线程调度竞争, active
    // 段可能零样本: workers_p50 保持未采样哨兵(-1), gate 回退 max_active_threads
    // (无样本时同样 0=哨兵), 把"未观测"误判为"只有一个活跃计算线程"
    // (single_threaded, exit 10)。worker 值取有效配置容量 min(budget,可用核),
    // 与循环内 B2-A18 未观测回退同一口径; 阈值与判据表达式不动。
    recorder.set_stage(astrocs::ResStage::Active);
    const uint32_t planned_start = std::min(budget, cli_affinity_cpu_count());
    recorder.set_workers(planned_start, planned_start);
    std::atomic<bool> first_sample_done{false};
    std::thread sampler([&mon, &recorder, &alloc_rec, &sampling, &first10s_diag,
                         &first10s_done, &budget, &first_sample_done] {
        using SteadyNs = std::chrono::steady_clock::duration;
        const auto period = std::chrono::duration_cast<SteadyNs>(std::chrono::duration<double>(0.5));
        auto next = std::chrono::steady_clock::now();
        unsigned tick = 0;
        while (sampling.load(std::memory_order_relaxed)) {
            mon.tick();
            // B2-A18: worker 数优先写真实租约观测 (峰值并发授予); 未观测
            // (哨兵 0) 时回退到有效配置容量 min(budget, 可用核) —— 与
            // utilization_value 同一哨兵纪律, 不以观测名义回填配置。
            {
                const uint32_t obs_workers = astrocs::core::granted_worker_observation()
                    .peak_active.load(std::memory_order_relaxed);
                const uint32_t planned =
                    std::min(budget, cli_affinity_cpu_count());
                const uint32_t eff = obs_workers > 0 ? obs_workers : planned;
                recorder.set_workers(eff, eff);
            }
            recorder.record(mon.last_sample());
            first_sample_done.store(true, std::memory_order_relaxed);
            // MON-002(V7): RSS/private/commit/allocator outstanding 同 tick 采样。
            alloc_rec.tick(mon.last_sample());
            ++tick;
            // MON-002: first 10s gate 调用点 —— 跨过 10s 边界后首次采样即评估:
            // 低 CPU+非 IO+非内存带宽饱和 → **只登记诊断事实**（§9.74 裁决 10: 资源判据
            // 不设门、不接入协作取消、不改退出码；原 rc=10 归并路径已删除）。
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
                    // §9.74 裁决 10: 资源判据不再接入协作取消（一般性资源超限门已取消）
                    // ⇒ 只登记诊断事实，不置位任何取消源；本判定仍由收尾事件如实呈现。
                    if (astrocs::fast_fail_first10s(f10)) {
                        first10s_diag.store(
                            static_cast<int>(astrocs::GateDiag::FastFailFirst10s),
                            std::memory_order_relaxed);
                    }
                }
            }
            next += period;
            std::this_thread::sleep_until(next);
        }
    });
    // 等第一个 active 样本真正记录后再启动 run_pipeline(消除短 run 竞态; 首个
    // 样本是真实 /proc 观测, 不额外造样本, 不改变活跃均值口径)。
    while (!first_sample_done.load(std::memory_order_relaxed))
        std::this_thread::yield();
    // B2-A18: 其后的 active 阶段 worker 数由采样循环持续写入真实租约观测值。
    // CLI-004: §4 progress 事件(04 冻结字段 completed/total/unit/rate/eta_seconds)。
    // 粒度 = phase 粒度(run 开始 0/1, 结束 1/1): Runtime 公开合同无节点级进度回调,
    // 协议面按合同冻结 —— 粒度升级(节点/帧级采样)不改变字段结构, 消费者透明。
    ev.emit_progress(0, 1, "phases", nullptr, nullptr);
    // §9.74 裁决 10: 资源判据（CPU/内存/线程）不设门 ⇒ 恒不接入协作取消
    // （取消源恒 nullptr）；计算不再因利用率被判据提前打断（记录与裁决分离）。
    // 内存静态预算（§8.3）—— CPU 与内存同源解析：CPU 取亲和性核数
    // （cli_affinity_cpu_count，上方 budget），内存取「实测可用内存 × 可配置比例
    // （默认 95%）」。上限只作调度准入（回压/排队），不产生退出码（§3.5 内存不设门）。
    uint64_t mem_limit = 0;
    std::string mem_source = "none";
    cli_resolve_memory_budget(cpu_profile_path, &mem_limit, &mem_source);
    const int rrc = astrocs::cli::run_pipeline({phase.back() - '0'}, cfg_text, budget,
                                               &fail_reason, nullptr, mem_limit, mem_source);
    // MON-002 reclaim: 多线程重计算节点释放的大块缓冲会滞留在线程 glibc arena
    // 中（真实 T4 运行 live heap(alloc_outstanding) 仅 ~0.2GB 而 RSS 残留 ~2.4GB,
    // 被 reclaim 门判为"不可解释残留"）。run 结束后显式将各 arena 空闲块归还
    // 内核, 使收尾 RSS 反映真实工作集; 不改任何门/阈值/科学公式。
#if defined(__GLIBC__)
    malloc_trim(0);
#endif
    {
        const auto s0 = mon.summary();
        const double done_rate = s0.wall_seconds > 0.0 ? 1.0 / s0.wall_seconds : 0.0;
        ev.emit_progress(1, 1, "phases", &done_rate, nullptr);
    }
    recorder.set_stage(astrocs::ResStage::Flush);
    sampling.store(false, std::memory_order_relaxed);
    sampler.join();
    // MON-001: run 收尾自动生成 resource_timeseries.csv / resource_summary.json /
    // worker_balance.csv(无需操作者脚本; 管线失败也留资源证据)。开销占比由
    // summary.sample_overhead_ms(真实累计采样 wall / 总 wall 口径的原料)度量。
    {
        const astrocs::ProcessMonitor::Summary mon_s = mon.summary();
        const bool wrote = recorder.write_all(res_out_dir, mon_s.wall_seconds,
                                              mon_s.sample_overhead_ms);
        if (!wrote) {
            // §9.74 裁决 10 运行期臂: 写盘失败/磁盘满 ⇒ error + fail-closed（磁盘 → 10）。
            const astrocs::WriteProbe pr = astrocs::probe_writable(res_out_dir);
            if (astrocs::write_failure_is_resource_exit(pr.kind))
                return disk_write_failure_exit(ev, phase, res_out_dir, "resource_timeseries.csv",
                                               pr, "resource recorder write_all failed");
            std::fprintf(stderr, "astrocs: warning: resource files not written to %s\n",
                         sanitize(res_out_dir).c_str());
        }
        // MON-002(V7): 原始曲线 + 报告落盘(alloc_samples.csv/alloc_report.json;
        // 管线失败也留证据, 与 MON-001 三产物同策略)。
        alloc_rec.finalize();
        if (!alloc_rec.write_all(res_out_dir)) {
            const astrocs::WriteProbe pr = astrocs::probe_writable(res_out_dir);
            if (astrocs::write_failure_is_resource_exit(pr.kind))
                return disk_write_failure_exit(ev, phase, res_out_dir, "alloc_report.json",
                                               pr, "allocation report write_all failed");
            std::fprintf(stderr, "astrocs: warning: alloc report files not written to %s\n",
                         sanitize(res_out_dir).c_str());
        }
    }
    // MON-002: first-10s 判定只作**记录事实**（§9.74 裁决 10: 资源判据不设门、不改退出码；
    // 原 fast_fail_first_10s → rc=10 归并路径已随一般性资源超限门取消）。
    const astrocs::GateDiag f10 =
        static_cast<astrocs::GateDiag>(first10s_diag.load(std::memory_order_relaxed));
    if (rrc != astrocs::OK) {
        // 主判据（先于探针）: 管线退出码已是 10 ⇒ 失败**本身**已被分类为磁盘满
        // （aio 在清理临时产物之前判定 ENOSPC/EDQUOT → 失败节点 manifest
        // error_kind="disk_full" → runtime_client.cpp::pipeline_exit_code_from_error
        // 映射 10）。此时发与探针兜底**同一形状**的 resource error 事件
        // （failure_kind=disk_full），使事件流消费者拿到同口径判定字段。
        if (rrc == astrocs::RESOURCE) {
            astrocs::WriteProbe pr_cls;
            pr_cls.kind = astrocs::WriteFailureKind::DiskFull;
            return disk_write_failure_exit(
                ev, phase, res_out_dir, "output_dir", pr_cls,
                "pipeline failed (rc=10): disk full classified at the failure site "
                "(node manifest error_kind=disk_full), before temp-artifact cleanup");
        }
        // §9.74 裁决 10 运行期臂（兜底）: 管线失败且**磁盘写不进去**（写满/写失败）⇒ fail-closed
        // 归并为 exit 10（真实探测写判定，不猜 errno）；否则保留管线原退出码。
        //
        // 定位: 本探针是**兜底**，不是判据。它问的是"现在还能不能写"，而
        // ASTROCS_DESIGN §10 要求失败/取消路径**清理临时产物** —— 清理会释放磁盘满，
        // 探针随后必然成功（fail-open，实测 rc=7 而非 10）。磁盘满的**判据**改为在
        // 失败发生处（aio，清理之前）分类，经失败节点 manifest 的
        // error_kind="disk_full" 由 runtime_client.cpp::pipeline_exit_code_from_error
        // 映射为 10；此处仅兜住"未走该通道且确实仍写不进去"的残余情形。
        const astrocs::WriteProbe pr = astrocs::probe_writable(res_out_dir);
        if (astrocs::write_failure_is_resource_exit(pr.kind)) {
            fail_reason = "disk write failed (" +
                          std::string(astrocs::write_failure_kind_name(pr.kind)) +
                          "); pipeline rc=" + std::to_string(rrc);
            return disk_write_failure_exit(ev, phase, res_out_dir, "output_dir", pr,
                                           "pipeline failed (rc=" + std::to_string(rrc) +
                                               ") and output_dir is not writable");
        }
        return rrc;  // 管线自身失败: 保留原退出码
    }

    const auto s = mon.summary();
    astrocs::GateConfig g;
    g.kind = astrocs::ResKind::Compute;
    g.available_cpus = cli_affinity_cpu_count();
    g.selected_workers = budget;   // 配置基准 (阈值用)
    // B2-A18: U 分母 = 真实观测的租约宽度 (0 = 未观测哨兵)。
    g.granted_workers = astrocs::core::granted_worker_observation()
                            .peak_active.load(std::memory_order_relaxed);
    g.max_active_threads = s.max_threads;
    g.avg_equivalent_cores = s.avg_equivalent_cores;   // 单位=等效核(见 monitor.h)
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
        g.active_window_seconds = act.wall_seconds;   // B1-A6 判定域前置
        if (act.wall_seconds >= 10.0) {
            // M5a-G-002: 采集端 act.cpu_pct_* 单位 = 100×等效核
            // (percent_of_one_core, resource_recorder.h), 必须先按「已分配容量」
            // (allocated_capacity_cores)归一为百分比, 再交给 evaluate_gate 的
            // 90%/85% 判据; 否则 85%/90% 退化为 0.85/0.90 核绝对下限。
            g.cpu_p50_percent =
                astrocs::cpu_percent_of_allocated_capacity(g, act.cpu_pct_p50);
            g.cpu_mean_percent =
                astrocs::cpu_percent_of_allocated_capacity(g, act.cpu_pct_mean);
        }
        g.rss_slope_measured = true;
        g.rss_slope_mb_per_s =
            static_cast<double>(act.rss_slope_bytes_per_s) / (1024.0 * 1024.0);
    }
    // P26(负责人 2.A): 运行工作量(线程秒 = 等效核·秒)。有 active 段样本时用 active
    // 判定窗(与统计判据同域); 采样不足则回退整段 wall(并如实反映为工作量的上界)。
    // 该字段只做事实标记/报告, 不改任何 §18.2 冻结阈值与判定式。
    const double work_win = g.active_window_seconds > 0.0 ? g.active_window_seconds
                                                          : s.wall_seconds;
    g.work_core_seconds = s.avg_equivalent_cores * work_win;
    // MON-002: 结束时 gate 调用; first-10s 已失败而结束判定通过时, 快速失败兜底生效。
    astrocs::GateDiag d = astrocs::evaluate_gate(g);
    if (!astrocs::gate_diag_is_violation(d) &&
        f10 == astrocs::GateDiag::FastFailFirst10s)
        d = astrocs::GateDiag::FastFailFirst10s;
    // MON-001(V7): 逐样本聚合判定与 evaluate_gate 互补 —— 监控缺失直接 FAIL;
    // >=70% 样本 U>=0.75; 队列有工作时连续>=10s U<0.50。哨兵纪律: 统计不可得
    // (如 mini workload 采样不足)记 -1 跳过对应判定(p2007 先例), 不构成 FAIL
    // 证据; 监控在跑但 active 段零样本仍是 MonitoringMissing(无资源证据)。
    // mini 任务 0.85*min(selected,available) 门的结构失配(abs-floor)为负责人
    // 裁决项, 此处不自行放宽(§18.2 冻结值 85%/60%)。
    // NotApplicable(判定域不成立)与 Ok 一样继续走 mon001/mon002 证据面:
    // "未判"不等于"没有监控证据", MonitoringMissing 必须照常抓。
    if (!astrocs::gate_diag_is_violation(d)) {
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
                    // 逐样本利用率 U=ΔCPU/(interval×allocated_capacity) —
                    // 100%=已分配容量用满; 与 utilization_value 同口径。
                    // §18.2 冻结值: 单样本 85%、队列窗口 60%(常量集中在
                    // resource_gate.h, 不散落硬编码)。
                    if (astrocs::utilization_value(g, r.cpu_pct) >=
                        astrocs::kMon001UtilSampleMinPercent / 100.0) ++pass;
                    // 队列有工作(runnable>0)且利用率<60% 的连续 run 长度。
                    if (r.runnable_workers > 0 &&
                        astrocs::utilization_value(g, r.cpu_pct) <
                            astrocs::kMon001QueueUtilMinPercent / 100.0) {
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
    // RUNTIME-CI-001 (§10.5 work units): 记录本 run 的 typed DAG 节点数（真实 IR，
    // 非配置占位）。IR 构建失败 → 0（哨兵：不臆造工作量）。
    const uint64_t measured_work_units = [&]() -> uint64_t {
        std::string ir_err;
        const std::string irj =
            astrocs::cli::build_pipeline_ir({phase.back() - '0'}, cfg_text, &ir_err);
        if (irj.empty()) return 0;
        try {
            return static_cast<uint64_t>(nlohmann::json::parse(irj)["nodes"].size());
        } catch (...) {
            return 0;
        }
    }();
    ev.emit("resource", "info", phase, "resource gate", {
        {"verdict", astrocs::gate_diag_name(d)},
        {"wall_seconds", s.wall_seconds},
        {"avg_equivalent_cores", s.avg_equivalent_cores},
        {"max_active_threads", s.max_threads},
        {"selected_workers", g.selected_workers},
        {"granted_workers_observed", g.granted_workers},
        {"available_cpus", g.available_cpus},
        {"workers_p50", g.workers_p50},
        {"cpu_p50_percent", g.cpu_p50_percent},
        {"cpu_mean_percent", g.cpu_mean_percent},
        // P26(负责人 T2/2.A): 工作量下限事实 + 记录/裁决分离处置(见 resource_gate.h)。
        {"work_core_seconds", g.work_core_seconds},
        {"workload_floor_core_seconds", astrocs::kMon003MinCoreSeconds},
        {"workload_floor_reached", astrocs::gate_workload_above_floor(g)},
        {"resource_gate_mode", "record_only"},   // §9.74 裁决 10: 恒 record-only（无 enforce 路径）
        {"strict_flag_requested", strict_flag_requested},
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
        // RUNTIME-CI-001 (§10.5 每线程 CPU / I/O wait / 队列深度 / worker 均衡):
        // 每线程 CPU 与 active compute threads 来自 /proc/self/task 真实观测
        // (lib/infrastructure/cli/monitor.h read_thread_cpu → lib/infrastructure/cli/resource_recorder.h ResRecord)。
        {"per_thread_cpu_max_pct", act.per_thread_cpu_max_pct_peak},
        {"per_thread_cpu_sum_pct", act.per_thread_cpu_sum_pct_mean},
        {"active_compute_threads_peak", act.active_compute_threads_peak},
        {"io_wait_pct", act.io_wait_pct_mean},
        {"queue_depth_observed", g.queue_low_run_seconds >= 0.0 ? 1 : 0},
        {"worker_balance_active_over_runnable", g.workers_p50},
        {"work_units", measured_work_units},
        // SO-05 记录/裁决分离（04_OPEN_ITEMS_AND_SIGNOFF SO-05；宪章 §10.5/§17.6）:
        // 未签字前资源判据恒为 record_only + 显式 pending，绝不自动升级为硬失败。
        {"measurement_policy", astrocs::v6runtime::kRecordOnlyPolicy},
        {"so05_signoff_id", astrocs::v6runtime::kSo05Id},
        {"so05_signoff_status", astrocs::v6runtime::kSo05Status},
        {"auto_adjudication_allowed", false},
        {"auto_adjudication_policy", astrocs::v6runtime::kNoAutoAdjudication},
        {"one_budget_source_rule", astrocs::v6runtime::kOneBudgetSourceRule},
        {"determinism_contract", astrocs::v6runtime::kDeterminismContractId},
    });
    // §9.74 裁决 10（ASTROCS_DESIGN §3.5/§6.3）: 一般性资源超限门已取消 ⇒ 资源判据
    // **恒为 record-only**（完整记录，不改变退出码，无 rc=10 路径）。阈值/判定式一字未改;
    // 工作量下限(负责人 2.A)作为事实字段一并记录。--strict-resource-gate/--on-resource-gate
    // 保留接受（登记现状）: strict_flag_requested 如实入事件，但不再改变裁决。
    if (astrocs::gate_diag_is_violation(d)) {
        const astrocs::GateEnforcement enf = astrocs::gate_enforcement(strict_flag_requested, d);
        const bool enforced = enf == astrocs::GateEnforcement::Enforced;   // 恒 false
        const std::string why = std::string("resource gate recorded (not enforced): ") +
                                astrocs::gate_diag_name(d) +
                                " (" + astrocs::diag_message(d, g) + ")";
        ev.emit("resource_gate", "warning", phase, why,
                {{"diag", astrocs::gate_diag_name(d)},
                 {"enforcement", astrocs::gate_enforcement_name(enf)},
                 // strict 是 resource_gate 的**冻结必含扩展字段**（五面同面：protocol.h kExt /
                 // schema allOf.then.required / schema x-astrocs-event-kind-registry /
                 // 读侧 CLI-004 / 读侧 FIX208）。原先与之同实参重复的 strict_flag_requested
                 // 已删——一次 emit 不写两个同义键（B4）。
                 {"strict", strict_flag_requested},
                 {"enforced", enforced},
                 {"work_core_seconds", g.work_core_seconds},
                 {"workload_floor_core_seconds", astrocs::kMon003MinCoreSeconds},
                 {"workload_floor_reached", astrocs::gate_workload_above_floor(g)},
                 // SO-05 记录/裁决分离（§9.74 裁决 10；docs/plugins/infrastructure/
                 // 21_observability.md §8.3/§8.4）：资源判据**恒 record-only**，CLI 面不存在
                 // enforce 路径（resource_gate.h::gate_enforcement 恒 RecordOnly）。原先写的
                 // would_fail_if_so05_signed=true 已删：它对 §8.3 的 record_and_justify 判据
                 // ④⑤⑥（平均利用率 / 利用率 p50 / 逐样本利用率）是错的——§8.3 明定它们的
                 // 违约后果是「记录 + 超标登记（不改退出码）」，即使签字也不产生 exit 10；
                 // 该键是**事件级常量**，无法表达逐判据的 enforce/record 分类，属「不该存在」。
                 // 以下三键是 SO-05 证据面的**必含扩展字段**（B4 登记，五面同面；语义见
                 // 21_observability.md §8.4）。
                 {"so05_signoff_id", astrocs::v6runtime::kSo05Id},
                 {"so05_signoff_status", astrocs::v6runtime::kSo05Status},
                 {"auto_adjudication_allowed", false}});
        std::fprintf(stderr, "astrocs: WARNING (recorded, not enforced): %s\n", why.c_str());
    }
    // MON-002: 资源 summary 事件接线（曲线唯一载体 = resource_timeseries.csv）。
    // CLI-002 移除 cmd_run_pipeline 时漏接（定义保留未调用），MON-002 验收的
    // resource summary / backend 事件在 phase run 路径从未发出——此处补齐。
    // raw 产物目录: recorder.write_all 的 res_out_dir 同源（07 合同 raw 落点）。
    {
        const astrocs::ProcessMonitor::Summary mon_s2 = mon.summary();
        // CLI-004: 真实 monitor 摘要外带给 phase stats 事件(冻结扩展字段同源填充)。
        if (summary_out != nullptr) *summary_out = mon_s2;
        emit_resource_summary(ev, phase, mon_s2, res_out_dir, recorder.record_count());
        emit_backend_event(ev, phase, "astrocs.cpu.baseline", "selected", budget, budget);
    }
    return astrocs::OK;
}

// ── CLI-MULTIBLOCK（GAP_AUDIT §9.68 / §9.71 裁决 2）：逐块会话执行的共用返回面 ──
// 单块简写调用一次（block_count==1）；多块形态逐块调用 —— 每块独立 output_dir、
// 独立 run manifest（不得把多块混成一个 manifest）；块名写入日志与 manifest。
// cfg/cfg_sha = 用户配置文件（manifest provenance 面：多块时恒为原文件，使 verify 的
// config 哈希仍锚在用户实际给的配置上）；cfg_text = 本块实际生效配置 JSON
// （单块简写本身，或 blocks[i] 展开）。final 事件由调用方统一发一次（一次运行恰一个）。
struct BlockOutcome {
    int rc = astrocs::OK;
    std::string kind = "ok";
    std::string why = "complete";
};

// ── 三阶段「写盘阶段」取消窗（测试钩子，非用户接口）───────────────────────
// 取消点覆盖（三命令同构，缺一不可）:
//   ① 入口窗     ASTROCS_TEST_SLEEP_MS           (subcommand.h run(), 读配置前)
//   ② 计算窗     ASTROCS_TEST_PIPELINE_SLEEP_MS  (runtime_client.cpp run_pipeline,
//                Runtime 已加载; 置位经 cancel_watch → rt->cancel() 送达调度器)
//   ③ 写盘窗     本函数（产物收集/哈希完成 → run manifest/运行图落盘之前）
// 语义: 轮询 astrocs::is_cancelled()（与信号处理器同一原子标志）; 返回 true 时
// 调用方按 ASTROCS_DESIGN §7.2「取消 → 写 incomplete manifest → exit 9」收尾。
// 不设环境变量时零影响（单次 getenv，不 sleep、不改判定）。
// 权威: ASTROCS_DESIGN.md §7.2（退出码 9 / 取消路径）、§10（原子产品：没有完成
// 清单就不算成功对象）；GAP_AUDIT G3-15（SIGTERM 全阶段 exit 9 路径未覆盖）。
bool write_stage_cancel_window() {
    const char* ms_env = std::getenv("ASTROCS_TEST_WRITE_SLEEP_MS");
    if (ms_env == nullptr) return false;   // 生产路径: 无钩子
    const long ms = std::strtol(ms_env, nullptr, 10);
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(ms > 0 ? ms : 0);
    while (std::chrono::steady_clock::now() < deadline) {
        if (astrocs::is_cancelled()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return astrocs::is_cancelled();
}

// CLI-MULTIBLOCK（§9.71 裁决 2）：单块 phase2 会话执行（与 phase1 同构）。
BlockOutcome run_phase2_block(const Parsed& p, astrocs::JsonlEmitter& ev,
                              const std::string& cfg, const std::string& cfg_sha,
                              const std::string& cfg_text, const std::string& block_name,
                              int block_index, int block_count) {
    BlockOutcome out;
    const bool multi = block_count > 1;
    const std::string tag =
        multi ? (" (block " + std::to_string(block_index + 1) + "/" +
                 std::to_string(block_count) +
                 (block_name.empty() ? "" : " '" + block_name + "'") + ")")
              : std::string();
    // B1-A8: 取消/失败路径也用显式 output_dir（禁 CWD "." 残留）；多块形态的
    // output_dir 在块级 ⇒ 从本块生效配置取（不是原文件的顶层）。
    const std::string cfg_out_dir = [&] {
        try { return nlohmann::json::parse(cfg_text).value("output_dir", std::string(".")); }
        catch (...) { return std::string("."); }
    }();
    if (multi)
        std::fprintf(stderr, "astrocs: mosaic block %d/%d%s → %s\n", block_index + 1,
                     block_count, block_name.empty() ? "" : (" '" + block_name + "'").c_str(),
                     cfg_out_dir.c_str());

    // RT-008: phase2 走 Runtime 单 phase IR 子图（与 run --phases 2 同一路径）。
    ev.stage("phase2_session", true);
    if (const char* sleep_ms = std::getenv("ASTROCS_TEST_SLEEP_MS")) {
        const long ms = std::strtol(sleep_ms, nullptr, 10);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        while (std::chrono::steady_clock::now() < deadline) {
            if (astrocs::is_cancelled()) {
                ev.stage("phase2_session", false);
                const int wrc = write_run_manifest(cfg_out_dir, ev, "incomplete", "cancelled by user",
                                                   cfg, cfg_sha, {2});
                if (wrc != astrocs::OK) { out.rc = wrc; out.kind = "phase2_failed"; out.why = "manifest write failed"; return out; }
                std::fprintf(stderr, "astrocs: cancelled%s\n", tag.c_str());
                out.rc = astrocs::CANCELLED; out.kind = "cancelled"; out.why = "cancelled by user";
                return out;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    // B2-A10: 同 phase3 —— 会话前落 run_context（§4.3 provenance 单一来源）。
    {
        const int ctxrc = write_run_context(cfg_out_dir, ev.run_id());
        if (ctxrc != astrocs::OK) { out.rc = ctxrc; out.kind = "phase2_failed"; out.why = "run context write failed"; return out; }
    }
    std::string fail_reason;
    const uint32_t budget = cli_affinity_cpu_count();
    // RESCUE-FD-08(观测链): 预算注入链正向证据(session 层 host budget = 真机
    // affinity 分配核)。CLI-002 入口迁移后 p2_session 旧通道不在生产调用面上,
    // 该实测证据仅剩 Runtime 注入面; 此处如实复述同一预算源(不造占位值, 不新增
    // 配置面)。阈值/门禁判据与 Runtime 实际注入的 budget 完全同源。
    std::fprintf(stderr, "session run: budget workers=%u (cpus=%u)\n",
                 (unsigned)budget, (unsigned)cli_affinity_cpu_count());
    std::fflush(stderr);
    astrocs::ProcessMonitor::Summary p2_summary;
    const int rrc = run_with_resource_gate(ev, "phase2", cfg_text, budget, fail_reason,
                              &p2_summary, strict_resource_gate_arg(p),
                              p.values.count("--cpu-profile") ? p.values.at("--cpu-profile") : std::string());
    ev.stage("phase2_session", false);

    nlohmann::json artifacts = nlohmann::json::array();
    std::vector<std::pair<std::string, std::string>> mans;
    astrocs::cli::collect_node_manifests(&mans);
    // PERF-P2 S2: 逐 artifact **并行**哈希（N = Runtime 预算核数 budget, 非硬编码;
    // 无硬编码线程数/ISA）。每个 artifact 独立读文件、结果写入按 path 下标固定的
    // 槽位; 组装顺序 = apaths 序 ⇒ manifest 字段与串行逐字节同值, 只是更快。
    // 每 artifact 只读一遍: canonical_product_hash_file 同时给出整文件
    // integrity_sha256 与 canonical_sha256（raw/.bin 为单遍流式, 不再整文件入
    // 内存）, 取代原先「file_sha256 + with_canonical_hash 内再 file_sha256 +
    // canonical_product_hash_file 整读」的 2–3 遍冗余。
    const std::vector<std::string> apaths =
        astrocs::cli::collect_node_artifact_paths(mans);
    std::vector<astrocs::core::CanonicalHashResult> chs(apaths.size());
    std::vector<std::uintmax_t> asizes(apaths.size(), 0);
    std::vector<unsigned char> asize_ok(apaths.size(), 0);
    const uint32_t hash_workers =
        std::max(1u, std::min<uint32_t>(budget, static_cast<uint32_t>(apaths.size())));
    auto hash_one = [&](std::size_t i) {
        chs[i] = astrocs::core::canonical_product_hash_file(apaths[i]);
        std::error_code ec;
        asizes[i] = std::filesystem::file_size(std::filesystem::u8path(apaths[i]), ec);
        asize_ok[i] = ec ? 0 : 1;
    };
    if (hash_workers <= 1 || apaths.size() <= 1) {
        for (std::size_t i = 0; i < apaths.size(); ++i) hash_one(i);
    } else {
        std::atomic<std::size_t> next_hash{0};
        std::vector<std::thread> hash_pool;
        hash_pool.reserve(hash_workers);
        for (uint32_t w = 0; w < hash_workers; ++w) {
            hash_pool.emplace_back([&]() {
                for (;;) {
                    const std::size_t i = next_hash.fetch_add(1);
                    if (i >= apaths.size()) break;
                    hash_one(i);
                }
            });
        }
        for (auto& th : hash_pool) th.join();
    }
    for (std::size_t i = 0; i < apaths.size(); ++i) {
        artifacts.push_back(with_canonical_hash(
            {{"path", apaths[i]}, {"sha256", chs[i].integrity_sha256},
             {"size_bytes", asize_ok[i]
                                ? static_cast<unsigned long long>(asizes[i])
                                : 0ULL}},
            chs[i]));
    }
    // B1-A5: uncertainty_available 由 integrate/write 节点 manifest 提供（mode=1 →
    // false; mode=2+ivar → true），CLI 只透传不判定。
    nlohmann::json extra = nlohmann::json::object();
    {
        bool ua_found = false; bool ua = false;
        for (const auto& [nid, mtext] : mans) {
            (void)nid;
            try {
                auto m = nlohmann::json::parse(mtext);
                if (m.is_object() && m.contains("uncertainty_available")) {
                    ua = m.value("uncertainty_available", false);
                    ua_found = true;
                }
            } catch (...) {}
        }
        if (ua_found) extra["uncertainty_available"] = ua;
    }
    // B2-A10（宪章 §4.3）: 真实 provenance 子对象（source SHA/单位/frame/算法 ID/
    // module build ID/provider/输入输出产品 hash），字段全部来自节点自报与
    // 已核验 artifacts，不在 CLI 侧造占位。
    extra["provenance"] = build_run_provenance(mans, artifacts);
    const std::string out_dir = [&] {
        try { return nlohmann::json::parse(cfg_text).value("output_dir", std::string(".")); }
        catch (...) { return std::string("."); }
    }();

    // CLI-MULTIBLOCK（§9.71 裁决 2）: 块归属写进本块 manifest（单块简写不写，保持旧形态）。
    if (multi)
        extra["block"] = {{"name", block_name}, {"index", block_index}, {"count", block_count}};

    // 写盘阶段取消窗（测试钩子；未设 ASTROCS_TEST_WRITE_SLEEP_MS = 零影响）
    write_stage_cancel_window();
    if (astrocs::is_cancelled()) {
        const int wrc = write_run_manifest(out_dir, ev, "incomplete", "cancelled by user",
                                           cfg, cfg_sha, {2}, artifacts, extra);
        if (wrc != astrocs::OK) { out.rc = wrc; out.kind = "phase2_failed"; out.why = "manifest write failed"; return out; }
        std::fprintf(stderr, "astrocs: cancelled%s\n", tag.c_str());
        out.rc = astrocs::CANCELLED; out.kind = "cancelled"; out.why = "cancelled by user";
        return out;
    }
    if (rrc != astrocs::OK) {
        const std::string why = fail_reason.empty() ? ("phase2 failed (exit " + std::to_string(rrc) + ")")
                                                    : fail_reason;
        const int wrc = write_run_manifest(out_dir, ev, "incomplete", "phase2 failed: " + why,
                                           cfg, cfg_sha, {2}, artifacts, extra);
        if (wrc != astrocs::OK) { out.rc = wrc; out.kind = "phase2_failed"; out.why = "manifest write failed"; return out; }
        std::fprintf(stderr, "astrocs: phase2 failed%s: %s\n", tag.c_str(), sanitize(why).c_str());
        out.rc = rrc; out.kind = "phase2_failed"; out.why = why;
        return out;
    }
    const int wrc = write_run_manifest(out_dir, ev, "complete", "phase2 ok", cfg, cfg_sha, {2},
                                       artifacts, extra);
    if (wrc != astrocs::OK) { out.rc = wrc; out.kind = "phase2_failed"; out.why = "manifest write failed"; return out; }
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
    out.rc = astrocs::OK;
    out.kind = "ok";
    out.why = multi ? ("phase2 complete" + tag) : std::string("phase2 complete");
    return out;
}

// CLI-MULTIBLOCK（§9.71 裁决 2）：mosaic 运行入口（形态判定 + 逐块派发；final 恰一个）。
int cmd_session2_run(const Parsed& p, astrocs::JsonlEmitter& ev) {
    // RUNTIME-CI-001: 显式模式路由门（--mode；§9.73 A44 后 legacy 整数 weight_mode
    // 与 config 的 weight_mode 键均已删除，出现即具名拒绝）；reject → ARGS(2)。
    {
        const int mrc = astrocs::v6cli::mode_gate(p, 2, ev);
        if (mrc != astrocs::OK) return mrc;
    }
    const std::string cfg = need_value(p, "--json");
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
    nlohmann::json doc;
    try { doc = nlohmann::json::parse(cfg_text); } catch (...) { doc = nlohmann::json(); }
    if (!(doc.is_object() && doc.contains("blocks"))) {
        // 平铺单块简写（原路径，向后兼容）
        // CLI-002: 单 phase 命令复用顶层 config 全量校验(unknown key→3), 与已移除的 run 路径同面。
        nlohmann::json validated;
        const int vrc2 = validate_config_full(cfg, &validated, /*session_mode=*/true, "mosaic");
        if (vrc2 != astrocs::OK) return vrc2;
        BlockOutcome o = run_phase2_block(p, ev, cfg, cfg_sha, cfg_text, std::string(), 0, 1);
        ev.emit_final(o.rc, o.kind, nullptr, o.why);
        return o.rc;
    }
    // 多块形态：结构校验（唯一实现 = parser.cpp session_blocks_errors，键集 = session_keys() ∪ block_keys()）
    int bcode = astrocs::ARGS;
    const std::vector<std::string> berrs = session_blocks_errors("mosaic", doc, &bcode);
    if (!berrs.empty()) {
        for (const auto& e : berrs) std::fprintf(stderr, "astrocs: %s\n", e.c_str());
        ev.emit_final(bcode, "phase2_failed", nullptr, berrs.front());
        return bcode;
    }
    const int nblocks = static_cast<int>(doc["blocks"].size());
    BlockOutcome agg;
    bool have_fail = false;
    for (int i = 0; i < nblocks; ++i) {
        nlohmann::json bdoc = doc["blocks"][i];
        const std::string bname =
            (bdoc.contains("name") && bdoc["name"].is_string()) ? bdoc["name"].get<std::string>()
                                                                : std::string();
        bdoc.erase("name");
        if (!bdoc.contains("schema_version")) bdoc["schema_version"] = "1";
        BlockOutcome o = run_phase2_block(p, ev, cfg, cfg_sha, bdoc.dump(), bname, i, nblocks);
        if (o.rc != astrocs::OK && !have_fail) { agg = o; have_fail = true; }
        if (o.rc == astrocs::CANCELLED) break;   // 用户已要求停：后续块不再起
    }
    if (!have_fail) agg.why = "phase2 complete (blocks=" + std::to_string(nblocks) + ")";
    ev.emit_final(agg.rc, agg.kind, nullptr, agg.why);
    return agg.rc;
}


// CLI-MULTIBLOCK（§9.71 裁决 2）：单块 phase3 会话执行（与 phase1/2 同构）。
BlockOutcome run_phase3_block(const Parsed& p, astrocs::JsonlEmitter& ev,
                              const std::string& cfg, const std::string& cfg_sha,
                              const std::string& cfg_text, const std::string& block_name,
                              int block_index, int block_count) {
    BlockOutcome out;
    const bool multi = block_count > 1;
    const std::string tag =
        multi ? (" (block " + std::to_string(block_index + 1) + "/" +
                 std::to_string(block_count) +
                 (block_name.empty() ? "" : " '" + block_name + "'") + ")")
              : std::string();
    // B1-A8: 取消/失败路径也用显式 output_dir（禁 CWD "." 残留）；多块形态的
    // output_dir 在块级 ⇒ 从本块生效配置取（不是原文件的顶层）。
    const std::string cfg_out_dir = [&] {
        try { return nlohmann::json::parse(cfg_text).value("output_dir", std::string(".")); }
        catch (...) { return std::string("."); }
    }();
    if (multi)
        std::fprintf(stderr, "astrocs: export block %d/%d%s → %s\n", block_index + 1,
                     block_count, block_name.empty() ? "" : (" '" + block_name + "'").c_str(),
                     cfg_out_dir.c_str());

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
        // RESCUE-FD-08: resume 预检只核「同一 config 的最新一次 complete prior run」。
        // 同一 output_dir 反复重跑同一 config 时, 每次 run 都会合法覆盖共享产物
        // (FITS 的 RUNID/provenance 等必为本次新值), 历史 manifest 记录的旧
        // artifact sha 必然与磁盘不再一致; 遍历全部历史 manifest 会把"已被后续
        // run 合法取代"误报成篡改(rc=8, p3006 第 3 次同 config 重跑实证)。
        // fail-closed 不减弱: 最新 complete prior manifest 的任一 artifact 缺失或
        // sha 不符即拒绝; 同一 config 无 complete manifest 时无从 resume, 直接放行。
        bool mismatch = false;
        std::error_code dec;
        std::string prior_path;
        std::filesystem::file_time_type prior_mtime{};
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
                if (pm.value("status", std::string()) != "complete") continue;
                if (pm.value("config_path", std::string()) != cfg) continue;
            } catch (...) { mismatch = true; break; }
            std::error_code tec;
            const auto mt = entry.last_write_time(tec);
            if (prior_path.empty() || (!tec && mt > prior_mtime)) {
                prior_path = entry.path().u8string();
                prior_mtime = mt;
            }
        }
        if (!mismatch && !prior_path.empty()) {
            try {
                std::ifstream pf(std::filesystem::u8path(prior_path), std::ios::binary);
                nlohmann::json pm = nlohmann::json::parse(
                    std::string(std::istreambuf_iterator<char>(pf), {}));
                for (const auto& a : pm.value("artifacts", nlohmann::json::array())) {
                    const std::string ap = a.value("path", std::string());
                    if (ap.empty()) continue;
                    if (!std::filesystem::exists(std::filesystem::u8path(ap), dec)) { mismatch = true; break; }
                    bool hok = false; const std::string sha = file_sha256(ap, &hok);
                    if (!hok || sha != a.value("sha256", std::string())) { mismatch = true; break; }
                }
            } catch (...) { mismatch = true; }
        }
        if (mismatch) {
            // 多块形态: 落点取**本块** output_dir（顶层 output_dir 在块形态下不存在,
            // 用 "." 会把 incomplete manifest 写到 CWD —— B1-A8 禁止）。
            const int wrc = write_run_manifest(cfg_out_dir, ev, "incomplete", "resume hash mismatch",
                                               cfg, cfg_sha, {3});
            if (wrc != astrocs::OK) { out.rc = wrc; out.kind = "phase3_failed"; out.why = "manifest write failed"; return out; }
            std::fprintf(stderr, "astrocs: resume hash mismatch%s\n", tag.c_str());
            out.rc = astrocs::INTEGRITY;
            out.kind = "resume_hash_mismatch";
            out.why = "prior artifact hash mismatch";
            return out;  // 04: 输出完整性验证失败 → 8
        }
    }

    // RT-008: phase3 走 Runtime 单 phase IR 子图（与 run --phases 3 同一路径）。
    ev.stage("phase3_session", true);
    if (const char* sleep_ms = std::getenv("ASTROCS_TEST_SLEEP_MS")) {
        const long ms = std::strtol(sleep_ms, nullptr, 10);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        while (std::chrono::steady_clock::now() < deadline) {
            if (astrocs::is_cancelled()) {
                ev.stage("phase3_session", false);
                const int wrc = write_run_manifest(cfg_out_dir, ev, "incomplete", "cancelled by user",
                                                   cfg, cfg_sha, {3});
                if (wrc != astrocs::OK) { out.rc = wrc; out.kind = "phase3_failed"; out.why = "manifest write failed"; return out; }
                std::fprintf(stderr, "astrocs: cancelled%s\n", tag.c_str());
                out.rc = astrocs::CANCELLED; out.kind = "cancelled"; out.why = "cancelled by user";
                return out;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    // B2-A10: 会话启动前落 run_context（节点 provenance 消费的真实 run_id/
    // 软件版本/源码 SHA 单一来源；缺上下文 = 节点 DATA fail-closed）。
    {
        const int ctxrc = write_run_context(cfg_out_dir, ev.run_id());
        if (ctxrc != astrocs::OK) { out.rc = ctxrc; out.kind = "phase3_failed"; out.why = "run context write failed"; return out; }
    }
    std::string fail_reason;
    const uint32_t budget = cli_affinity_cpu_count();
    astrocs::ProcessMonitor::Summary p3_summary;
    const int rrc = run_with_resource_gate(ev, "phase3", cfg_text, budget, fail_reason,
                              &p3_summary, strict_resource_gate_arg(p),
                              p.values.count("--cpu-profile") ? p.values.at("--cpu-profile") : std::string());
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
        const std::string op = m.value("output_fits_path", std::string());
        for (const auto& a : m.value("artifacts", nlohmann::json::array())) {
            if (!a.is_string()) continue;
            const std::string ap = a.get<std::string>();
            bool ok2 = false;
            const std::string sha = file_sha256(ap, &ok2);
            std::error_code ec;
            const auto size = std::filesystem::file_size(std::filesystem::u8path(ap), ec);
            if (!seen_paths.insert(ap).second) continue;
            // CLI-007 冻结语义: phase3 输出 FITS 记 role=phase3_output(test_07 断言)。
            // B1-A2: 该 FITS 也在节点 manifest 的 artifacts 清单里，必须在去重前
            // 判角色，否则先入的无 role 条目会把带 role 的条目去重掉。
            if (!op.empty() && ap == op)
                artifacts.push_back(with_canonical_hash(
                    {{"role", "phase3_output"}, {"path", ap},
                     {"sha256", ok2 ? sha : ""},
                     {"size_bytes", ec ? 0ULL : static_cast<unsigned long long>(size)}}, ap));
            else
                artifacts.push_back(with_canonical_hash(
                    {{"path", ap}, {"sha256", ok2 ? sha : ""},
                     {"size_bytes", ec ? 0ULL : static_cast<unsigned long long>(size)}}, ap));
        }
        // CLI-007: 节点 manifest 只声明 output_fits_path（未入 artifacts 数组）时也登记。
        if (!op.empty() && seen_paths.insert(op).second) {
            bool ok2 = false;
            const std::string sha = file_sha256(op, &ok2);
            std::error_code ec;
            const auto size = std::filesystem::file_size(std::filesystem::u8path(op), ec);
            artifacts.push_back(with_canonical_hash(
                {{"role", "phase3_output"}, {"path", op}, {"sha256", ok2 ? sha : ""},
                 {"size_bytes", ec ? 0ULL : static_cast<unsigned long long>(size)}}, op));
        }
    }
    // B1-A2/A10: phase3 侧同源透传 uncertainty_available（resample/writer 节点 manifest）。
    nlohmann::json extra = nlohmann::json::object();
    {
        bool ua_found = false; bool ua = false;
        for (const auto& [nid, mtext] : mans) {
            (void)nid;
            try {
                auto m = nlohmann::json::parse(mtext);
                if (m.is_object() && m.contains("uncertainty_available")) {
                    ua = m.value("uncertainty_available", false);
                    ua_found = true;
                }
            } catch (...) {}
        }
        if (ua_found) extra["uncertainty_available"] = ua;
    }
    // B2-A10（宪章 §4.3）: 真实 provenance 子对象（source SHA/单位/frame/算法 ID/
    // module build ID/provider/输入输出产品 hash），字段全部来自节点自报与
    // 已核验 artifacts，不在 CLI 侧造占位。
    extra["provenance"] = build_run_provenance(mans, artifacts);
    const std::string out_dir = [&] {
        try { return nlohmann::json::parse(cfg_text).value("output_dir", std::string(".")); }
        catch (...) { return std::string("."); }
    }();

    // CLI-MULTIBLOCK（§9.71 裁决 2）: 块归属写进本块 manifest（单块简写不写，保持旧形态）。
    if (multi)
        extra["block"] = {{"name", block_name}, {"index", block_index}, {"count", block_count}};

    // 写盘阶段取消窗（测试钩子；未设 ASTROCS_TEST_WRITE_SLEEP_MS = 零影响）
    write_stage_cancel_window();
    if (astrocs::is_cancelled()) {
        const int wrc = write_run_manifest(out_dir, ev, "incomplete", "cancelled by user",
                                           cfg, cfg_sha, {3}, artifacts, extra);
        if (wrc != astrocs::OK) { out.rc = wrc; out.kind = "phase3_failed"; out.why = "manifest write failed"; return out; }
        std::fprintf(stderr, "astrocs: cancelled%s\n", tag.c_str());
        out.rc = astrocs::CANCELLED; out.kind = "cancelled"; out.why = "cancelled by user";
        return out;
    }
    if (rrc != astrocs::OK) {
        const std::string why = fail_reason.empty() ? ("phase3 failed (exit " + std::to_string(rrc) + ")")
                                                    : fail_reason;
        const int wrc = write_run_manifest(out_dir, ev, "incomplete", "phase3 failed: " + why,
                                           cfg, cfg_sha, {3}, artifacts, extra);
        if (wrc != astrocs::OK) { out.rc = wrc; out.kind = "phase3_failed"; out.why = "manifest write failed"; return out; }
        std::fprintf(stderr, "astrocs: phase3 failed%s: %s\n", tag.c_str(), sanitize(why).c_str());
        out.rc = rrc; out.kind = "phase3_failed"; out.why = why;
        return out;
    }
    const int wrc = write_run_manifest(out_dir, ev, "complete", "phase3 ok", cfg, cfg_sha, {3},
                                       artifacts, extra);
    if (wrc != astrocs::OK) { out.rc = wrc; out.kind = "phase3_failed"; out.why = "manifest write failed"; return out; }
    // RT-009: phase3 run 成功路径补写运行图产物（static/observed/sidecar + L0 渲染）。
    // 此前 write_run_graphs 定义后无任何调用点（CLI-002 移除 cmd_run_pipeline/
    // cmd_graph 时漏接），RT-009 test_07 期望的 out/graph/* 恒缺失。
    // best-effort: 函数内部只 warning 不失败 run（"不失败 run"合同见其注释）。
    write_run_graphs(out_dir, ev, cfg, cfg_sha, {3});
    emit_phase_stats_resource(ev, "phase3", "session summary",
                              {{"outputs", artifacts.size()}}, &p3_summary);
    out.rc = astrocs::OK;
    out.kind = "ok";
    out.why = multi ? ("phase3 complete" + tag) : std::string("phase3 complete");
    return out;
}

// CLI-MULTIBLOCK（§9.71 裁决 2）：export 运行入口（形态判定 + 逐块派发；final 恰一个）。
int cmd_session3_run(const Parsed& p, astrocs::JsonlEmitter& ev) {
    // RUNTIME-CI-001: 显式输出模式路由门（--export-mode）；reject → ARGS(2)。
    {
        const int mrc = astrocs::v6cli::mode_gate(p, 3, ev);
        if (mrc != astrocs::OK) return mrc;
    }
    const std::string cfg = need_value(p, "--json");
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
    nlohmann::json doc;
    try { doc = nlohmann::json::parse(cfg_text); } catch (...) { doc = nlohmann::json(); }
    if (!(doc.is_object() && doc.contains("blocks"))) {
        // 平铺单块简写（原路径，向后兼容）
        nlohmann::json validated;
        const int vrc3 = validate_config_full(cfg, &validated, /*session_mode=*/true, "export");
        if (vrc3 != astrocs::OK) return vrc3;
        BlockOutcome o = run_phase3_block(p, ev, cfg, cfg_sha, cfg_text, std::string(), 0, 1);
        ev.emit_final(o.rc, o.kind, nullptr, o.why);
        return o.rc;
    }
    int bcode = astrocs::ARGS;
    const std::vector<std::string> berrs = session_blocks_errors("export", doc, &bcode);
    if (!berrs.empty()) {
        for (const auto& e : berrs) std::fprintf(stderr, "astrocs: %s\n", e.c_str());
        ev.emit_final(bcode, "phase3_failed", nullptr, berrs.front());
        return bcode;
    }
    const int nblocks = static_cast<int>(doc["blocks"].size());
    BlockOutcome agg;
    bool have_fail = false;
    for (int i = 0; i < nblocks; ++i) {
        nlohmann::json bdoc = doc["blocks"][i];
        const std::string bname =
            (bdoc.contains("name") && bdoc["name"].is_string()) ? bdoc["name"].get<std::string>()
                                                                : std::string();
        bdoc.erase("name");
        if (!bdoc.contains("schema_version")) bdoc["schema_version"] = "1";
        BlockOutcome o = run_phase3_block(p, ev, cfg, cfg_sha, bdoc.dump(), bname, i, nblocks);
        if (o.rc != astrocs::OK && !have_fail) { agg = o; have_fail = true; }
        if (o.rc == astrocs::CANCELLED) break;   // 用户已要求停：后续块不再起
    }
    if (!have_fail) agg.why = "phase3 complete (blocks=" + std::to_string(nblocks) + ")";
    ev.emit_final(agg.rc, agg.kind, nullptr, agg.why);
    return agg.rc;
}


// ── CLI-001(宪章对齐): phaseN validate|plan|inspect(宪章 §8.1 薄命令面补齐) ──
// 语义(冻结于 eng/tests/cli/test_cli001_vpi.py, 与 test_cli003_semantics 的 config validate
// 浅面相区分):
//   validate = session_mode 全量 config 校验 + PipelineIR 静态构建; 零 Runtime 实例化、
//              零科学执行、零 I/O 产物; 深层拒绝与 run 同面(IR 构建失败 → 2)。
//   plan     = validate 前置 + 确定性 plan 文档(typed DAG 节点/work units/预算/IO 引用;
//              无 run_id/时间戳 → 同 config 两次运行逐字节一致); --output 落盘同文档。
//   inspect  = 只读 output_dir 下 astrocs_run_*.json(kind=astrocs_run_manifest, phases
//              含 N)逐 run 呈现 + products 去重; malformed manifest 呈现不中断; 零写入。
namespace {

// 共用前置: 校验 config(session_mode) → build_pipeline_ir 静态构建。
// 成功返回 OK 并填充 cfg_sha/ir; 失败返回对应退出码(2/3)并写 stderr 诊断。
int phase_ir_prereq(const Parsed& p, int phase, std::string* cfg_sha_out,
                    std::string* ir_out) {
    const std::string cfg_path = need_value(p, "--config");
    nlohmann::json doc;
    const int rc = validate_config_full(cfg_path, &doc, /*session_mode=*/true, "normalize");
    if (rc != astrocs::OK) return rc;
    bool ok = false;
    const std::string sha = file_sha256(cfg_path, &ok);
    if (!ok) {
        std::fprintf(stderr, "astrocs: cannot hash config '%s'\n", cfg_path.c_str());
        return astrocs::INPUT;
    }
    std::string err;
    const std::string ir = astrocs::cli::build_pipeline_ir({phase}, doc.dump(), &err);
    if (ir.empty()) {
        std::fprintf(stderr, "astrocs: phase%d rejected: %s\n", phase,
                     sanitize(err).c_str());
        return astrocs::ARGS;   // 与 run 的 IR 构建失败映射一致(runtime_client → 2)
    }
    *cfg_sha_out = sha;
    *ir_out = ir;
    return astrocs::OK;
}

}  // namespace

// phaseN validate: 相级深层校验(不执行科学重算)
int cmd_phase_validate(const Parsed& p, int phase, astrocs::JsonlEmitter& ev) {
    // RUNTIME-CI-001: 显式模式路由门在 validate 面同样 fail-closed。
    {
        const int mrc = astrocs::v6cli::mode_gate(p, phase, ev);
        if (mrc != astrocs::OK) return mrc;
    }
    std::string cfg_sha, ir;
    const int rc = phase_ir_prereq(p, phase, &cfg_sha, &ir);
    if (rc != astrocs::OK) return rc;
    const auto irj = nlohmann::json::parse(ir);
    const std::size_t n = irj["nodes"].size();
    if (p.flags.count("--json")) {
        const nlohmann::json out = {
            {"schema_version", "1"},
            {"kind", "astrocs_phase_validate"},
            {"phase", phase},
            {"config", {{"path", p.values.at("--config")}, {"sha256", cfg_sha}}},
            {"node_count", n},
            {"status", "ok"},
        };
        std::printf("%s\n", out.dump().c_str());
    } else {
        std::printf("phase%d validate OK (%zu nodes)\n", phase, n);
    }
    return astrocs::OK;
}

// phaseN plan: typed DAG/work units/内存-IO/并行计划(确定性文档, 不执行)
int cmd_phase_plan(const Parsed& p, int phase, astrocs::JsonlEmitter& ev) {
    // RUNTIME-CI-001: 显式模式路由门在 plan 面同样 fail-closed。
    {
        const int mrc = astrocs::v6cli::mode_gate(p, phase, ev);
        if (mrc != astrocs::OK) return mrc;
    }
    std::string cfg_sha, ir;
    const int rc = phase_ir_prereq(p, phase, &cfg_sha, &ir);
    if (rc != astrocs::OK) return rc;
    const auto irj = nlohmann::json::parse(ir);
    nlohmann::json nodes = nlohmann::json::array();
    std::size_t parallel = 0, io = 0;
    std::set<std::string> seen_refs;
    nlohmann::json artifact_refs = nlohmann::json::array();
    for (const auto& nd : irj["nodes"]) {
        nodes.push_back({{"node_id", nd["node_id"]},
                         {"module_id", nd["module_id"]},
                         {"inputs", nd["inputs"]},
                         {"outputs", nd["outputs"]},
                         {"resources", nd["resources"]}});
        if (nd["resources"].value("parallel", false)) ++parallel; else ++io;
        for (auto it = nd["outputs"].begin(); it != nd["outputs"].end(); ++it) {
            const std::string ref = it.value().get<std::string>();
            if (seen_refs.insert(ref).second) artifact_refs.push_back(ref);
        }
    }
    const nlohmann::json out = {
        {"schema_version", "1"},
        {"kind", "astrocs_plan"},
        {"phase", phase},
        {"config", {{"path", p.values.at("--config")}, {"sha256", cfg_sha}}},
        {"budget", {{"cpu_cores", cli_affinity_cpu_count()}}},
        {"pipeline", {{"schema", "astrocs.pipeline/v1"},
                      {"nodes", nodes},
                      {"outputs", irj["outputs"]},
                      {"artifact_refs", artifact_refs}}},
        {"work_units", {{"total", nodes.size()},
                        {"parallel", parallel},
                        {"io", io}}},
    };
    const std::string text = out.dump();
    if (p.flags.count("--json")) {
        std::printf("%s\n", text.c_str());
    }
    if (p.values.count("--output")) {
        const std::string op = p.values.at("--output");
        {
            std::ofstream f(std::filesystem::u8path(op), std::ios::binary | std::ios::trunc);
            if (!f) {
                std::fprintf(stderr, "astrocs: cannot write plan '%s'\n", op.c_str());
                return astrocs::IO;
            }
            f << text << "\n";
            if (!f.good()) return astrocs::IO;
        }
        if (!p.flags.count("--json")) std::printf("%s\n", op.c_str());
    }
    if (!p.flags.count("--json") && !p.values.count("--output")) {
        std::printf("phase%d plan OK: %zu nodes (%zu parallel, %zu io), budget=%u cores\n",
                    phase, nodes.size(), parallel, io, cli_affinity_cpu_count());
    }
    return astrocs::OK;
}

// phaseN inspect: 只读已有运行和产品(零写入)
int cmd_phase_inspect(const Parsed& p, int phase, astrocs::JsonlEmitter& ev) {
    (void)ev;
    const std::string cfg_path = need_value(p, "--config");
    nlohmann::json doc;
    // inspect 只要求 config 合法(session_mode)以取得 output_dir; 不要求 IR 可构建
    // (错相 config 也允许检视运行历史)。
    const int rc = validate_config_full(cfg_path, &doc, /*session_mode=*/true, "normalize");
    if (rc != astrocs::OK) return rc;
    const std::string out_dir = doc.value("output_dir", std::string("."));
    std::error_code ec;
    if (!std::filesystem::exists(std::filesystem::u8path(out_dir), ec)) {
        std::fprintf(stderr, "astrocs: output_dir not found '%s'\n", out_dir.c_str());
        return astrocs::INPUT;
    }
    // 仅顶层 astrocs_run_*.json(文件名字典序, journal 语义即 run 顺序)
    std::vector<std::string> files;
    for (std::filesystem::directory_iterator it(std::filesystem::u8path(out_dir), ec), end;
         it != end; it.increment(ec)) {
        if (ec) break;
        const std::string fn = it->path().filename().string();
        if (fn.rfind("astrocs_run_", 0) == 0 && fn.size() > 5 &&
            fn.compare(fn.size() - 5, 5, ".json") == 0)
            files.push_back(fn);
    }
    std::sort(files.begin(), files.end());
    nlohmann::json runs = nlohmann::json::array();
    nlohmann::json products = nlohmann::json::array();
    std::set<std::string> seen_paths;
    for (const std::string& fn : files) {
        const std::string fp = out_dir + "/" + fn;
        std::ifstream f(std::filesystem::u8path(fp), std::ios::binary);
        if (!f) continue;
        std::stringstream buf; buf << f.rdbuf();
        nlohmann::json m = nlohmann::json::parse(buf.str(), nullptr, false);
        if (m.is_discarded() || !m.is_object() ||
            m.value("kind", std::string()) != "astrocs_run_manifest") {
            // 文档结构一致性: 所有 runs 行恒含 run_id 键(malformed 时 null)
            runs.push_back({{"run_id", nullptr}, {"path", fp}, {"status", "malformed"}});
            continue;
        }
        bool phase_match = false;
        if (m.contains("phases") && m["phases"].is_array()) {
            for (const auto& ph : m["phases"])
                if (ph.is_number_integer() && ph.get<int>() == phase) phase_match = true;
        }
        if (!phase_match) continue;
        nlohmann::json row = {
            {"run_id", m.value("run_id", std::string())},
            {"status", m.value("status", std::string())},
            {"phases", m.value("phases", nlohmann::json::array())},
            {"summary", m.value("summary", std::string())},
            {"artifacts_count", 0},
            {"path", fp},
            {"started_utc", m.value("started_utc", std::string())},
            {"finished_utc", m.value("finished_utc", std::string())},
            {"config_sha256", m.contains("config_sha256") && !m["config_sha256"].is_null()
                                  ? nlohmann::json(m["config_sha256"]) : nlohmann::json(nullptr)},
        };
        if (m.contains("artifacts") && m["artifacts"].is_array()) {
            row["artifacts_count"] = m["artifacts"].size();
            for (const auto& a : m["artifacts"]) {
                if (!a.is_object() || !a.contains("path") || !a["path"].is_string()) continue;
                const std::string ap = a["path"].get<std::string>();
                if (!seen_paths.insert(ap).second) continue;
                nlohmann::json pr = {{"path", ap},
                                     {"exists", std::filesystem::exists(
                                          std::filesystem::u8path(ap), ec)}};
                if (a.contains("role")) pr["role"] = a["role"];
                if (a.contains("sha256")) pr["sha256"] = a["sha256"];
                if (a.contains("size_bytes")) pr["size_bytes"] = a["size_bytes"];
                products.push_back(std::move(pr));
            }
        }
        runs.push_back(std::move(row));
    }
    if (p.flags.count("--json")) {
        const nlohmann::json out = {
            {"schema_version", "1"},
            {"kind", "astrocs_phase_inspect"},
            {"phase", phase},
            {"output_dir", out_dir},
            {"total_runs", runs.size()},
            {"runs", runs},
            {"products", products},
        };
        std::printf("%s\n", out.dump().c_str());
    } else {
        for (const auto& r : runs) {
            if (r.value("status", std::string()) == "malformed") {
                std::printf("%s: malformed (not a v1 astrocs_run_manifest)\n",
                            r.value("path", std::string()).c_str());
            } else {
                std::string phases_str;
                for (const auto& ph : r["phases"]) {
                    if (!phases_str.empty()) phases_str += ",";
                    phases_str += std::to_string(ph.get<int>());
                }
                std::printf("%s %s phases=[%s] %s\n",
                            r.value("run_id", std::string()).c_str(),
                            r.value("status", std::string()).c_str(),
                            phases_str.c_str(),
                            r.value("summary", std::string()).c_str());
            }
        }
        std::printf("%zu run(s), %zu product(s)\n", runs.size(), products.size());
    }
    return astrocs::OK;
}


// CLI-MULTIBLOCK（§9.68/§9.71）：单块 phase1 会话执行（结构体定义见文件上方）。

BlockOutcome run_phase1_block(const Parsed& p, astrocs::JsonlEmitter& ev,
                                    const std::string& cfg_path, const std::string& cfg_sha,
                                    const std::string& block_text,
                                    const std::string& block_name,
                                    int block_index, int block_count) {
    BlockOutcome out;
    const bool multi = block_count > 1;
    const std::string tag =
        multi ? (" (block " + std::to_string(block_index + 1) + "/" +
                 std::to_string(block_count) +
                 (block_name.empty() ? "" : " '" + block_name + "'") + ")")
              : std::string();
    // CLI-002: 单 phase 命令复用顶层 config 全量校验(unknown key→3), 与已移除的 run 路径同面。
    // 多块形态校验的是**整个文件**（validate_config_full 逐块判）⇒ 每块调用同一路径。
    nlohmann::json cfg_doc1;
    const int vrc1 = validate_config_full(cfg_path, &cfg_doc1, /*session_mode=*/true, "normalize");
    if (vrc1 != astrocs::OK) {
        out.rc = vrc1; out.kind = "phase1_failed"; out.why = "config rejected";
        return out;
    }
    // B1-A8: 取消路径也用显式 output_dir（禁 CWD "." 残留）。
    // 多块形态的 output_dir 在块级 ⇒ 从本块生效配置取（不是原文件的顶层）。
    const std::string cfg_out_dir = [&] {
        try { return nlohmann::json::parse(block_text).value("output_dir", std::string(".")); }
        catch (...) { return std::string("."); }
    }();
    if (multi)
        std::fprintf(stderr, "astrocs: normalize block %d/%d%s → %s\n", block_index + 1,
                     block_count, block_name.empty() ? "" : (" '" + block_name + "'").c_str(),
                     cfg_out_dir.c_str());

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
                const int wrc = write_run_manifest(cfg_out_dir, ev, "incomplete",
                                                   "cancelled by user", cfg_path, cfg_sha, {1});
                if (wrc != astrocs::OK) { out.rc = wrc; out.kind = "phase1_failed"; out.why = "manifest write failed"; return out; }
                std::fprintf(stderr, "astrocs: cancelled%s\n", tag.c_str());
                out.rc = astrocs::CANCELLED; out.kind = "cancelled"; out.why = "cancelled by user";
                return out;                            // 04: 取消 → 9
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    // B2-A10: 同 phase3 —— 会话前落 run_context（§4.3 provenance 单一来源）。
    {
        const int ctxrc = write_run_context(cfg_out_dir, ev.run_id());
        if (ctxrc != astrocs::OK) { out.rc = ctxrc; out.kind = "phase1_failed"; out.why = "run context write failed"; return out; }
    }
    std::string fail_reason;
    const uint32_t budget = cli_affinity_cpu_count();
    astrocs::ProcessMonitor::Summary p1_summary;
    const int rrc = run_with_resource_gate(ev, "phase1", block_text, budget, fail_reason,
                              &p1_summary, strict_resource_gate_arg(p),
                              p.values.count("--cpu-profile") ? p.values.at("--cpu-profile") : std::string());
    ev.stage("phase1_session", false);

    // FIX-E2E B1-A1: 全链 8 节点产物收集(旧写法 `if (nid != "cal") continue;` 只认
    // cal, HiPS/manifest 科学产物永不入 run manifest)。按 path 去重(链共享 output_dir)。
    nlohmann::json artifacts = nlohmann::json::array();
    std::set<std::string> seen_paths;
    std::vector<std::pair<std::string, std::string>> mans;
    astrocs::cli::collect_node_manifests(&mans);
    for (const auto& [nid, mtext] : mans) {
        (void)nid;
        nlohmann::json m;
        try { m = nlohmann::json::parse(mtext); } catch (...) { continue; }
        if (!m.is_object()) continue;
        for (const auto& a : m.value("artifacts", nlohmann::json::array())) {
            if (!a.is_string()) continue;
            const std::string ap = a.get<std::string>();
            if (!seen_paths.insert(ap).second) continue;
            bool ok2 = false;
            const std::string sha = file_sha256(ap, &ok2);
            std::error_code ec;
            const auto size = std::filesystem::file_size(std::filesystem::u8path(ap), ec);
            artifacts.push_back(with_canonical_hash(
                {{"path", ap}, {"sha256", ok2 ? sha : ""},
                 {"size_bytes", ec ? 0ULL : static_cast<unsigned long long>(size)}}, ap));
        }
    }
    // B2-A10（宪章 §4.3）: 同 phase2/3 的真实 provenance 子对象。
    nlohmann::json p1_extra = nlohmann::json::object();
    p1_extra["provenance"] = build_run_provenance(mans, artifacts);
    // CLI-MULTIBLOCK（§9.68）: 块归属写进本块 manifest（块级 name/index/count），
    // 便于多块各自归属；单块简写不写本子对象（保持旧 manifest 逐字节形态）。
    if (multi)
        p1_extra["block"] = {{"name", block_name}, {"index", block_index}, {"count", block_count}};
    const std::string out_dir = cfg_out_dir;

    // 写盘阶段取消窗（测试钩子；未设 ASTROCS_TEST_WRITE_SLEEP_MS = 零影响）
    write_stage_cancel_window();
    if (astrocs::is_cancelled()) {
        const int wrc = write_run_manifest(out_dir, ev, "incomplete", "cancelled by user",
                                           cfg_path, cfg_sha, {1}, artifacts, p1_extra);
        if (wrc != astrocs::OK) { out.rc = wrc; out.kind = "phase1_failed"; out.why = "manifest write failed"; return out; }
        std::fprintf(stderr, "astrocs: cancelled%s\n", tag.c_str());
        out.rc = astrocs::CANCELLED; out.kind = "cancelled"; out.why = "cancelled by user";
        return out;                                  // 04: 取消 → 9, manifest=incomplete
    }
    if (rrc != astrocs::OK) {
        const std::string why = fail_reason.empty() ? ("phase1 failed (exit " + std::to_string(rrc) + ")")
                                                    : fail_reason;
        const int wrc = write_run_manifest(out_dir, ev, "incomplete", "phase1 failed: " + why,
                                           cfg_path, cfg_sha, {1}, artifacts, p1_extra);
        if (wrc != astrocs::OK) { out.rc = wrc; out.kind = "phase1_failed"; out.why = "manifest write failed"; return out; }
        std::fprintf(stderr, "astrocs: phase1 failed%s: %s\n", tag.c_str(), sanitize(why).c_str());
        out.rc = rrc; out.kind = "phase1_failed"; out.why = why;
        return out;  // RT-008: Runtime 退出码映射(Runtime 已按 04 合同映射)
    }
    const int wrc = write_run_manifest(out_dir, ev, "complete", "phase1 ok", cfg_path, cfg_sha, {1},
                                       artifacts, p1_extra);
    if (wrc != astrocs::OK) { out.rc = wrc; out.kind = "phase1_failed"; out.why = "manifest write failed"; return out; }
    // RT-009/P1-001: phase1 成功路径补写运行图产物（static/observed/sidecar）。
    // 真实节点化后 phase1 trace 含每节点观测; best-effort: 函数内部只 warning
    // 不失败 run（"不失败 run"合同见其注释）。
    write_run_graphs(out_dir, ev, cfg_path, cfg_sha, {1});
    emit_phase_stats_resource(ev, "phase1", "frames processed",
                              {{"frames", artifacts.size()}}, &p1_summary);
    out.rc = astrocs::OK;
    out.kind = "ok";
    out.why = multi ? ("phase1 complete" + tag) : std::string("phase1 complete");
    return out;
}

// CLI-MULTIBLOCK（GAP_AUDIT §9.68）：normalize 运行入口。
// 形态判定：顶层 blocks ⇒ 多块（逐块按序各自成一次运行：独立 output_dir、独立
// run manifest、独立 run_context/graph；块级 name 写入日志与 manifest）；
// 否则走平铺单块简写（原路径，向后兼容）。
// 失败处置：逐块继续（块之间独立，后续块仍产出自己的 manifest），聚合返回**首个**
// 非零 rc；取消（rc=9）立即停止后续块（用户已要求停）。final 事件恰一个。
int cmd_session1_run(const Parsed& p, astrocs::JsonlEmitter& ev) {
    const std::string cfg = need_value(p, "--json");
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
    nlohmann::json doc;
    try { doc = nlohmann::json::parse(cfg_text); } catch (...) { doc = nlohmann::json(); }
    if (!(doc.is_object() && doc.contains("blocks"))) {
        BlockOutcome o = run_phase1_block(p, ev, cfg, cfg_sha, cfg_text, std::string(), 0, 1);
        ev.emit_final(o.rc, o.kind, nullptr, o.why);
        return o.rc;
    }
    // 多块形态：结构校验（唯一实现 = parser.cpp；含形态互斥/块级 output_dir/未知键）
    int bcode = astrocs::ARGS;
    const std::vector<std::string> berrs = session_blocks_errors("normalize", doc, &bcode);
    if (!berrs.empty()) {
        for (const auto& e : berrs) std::fprintf(stderr, "astrocs: %s\n", e.c_str());
        ev.emit_final(bcode, "phase1_failed", nullptr, berrs.front());
        return bcode;
    }
    const int nblocks = static_cast<int>(doc["blocks"].size());
    BlockOutcome agg;
    bool have_fail = false;
    for (int i = 0; i < nblocks; ++i) {
        const auto& b = doc["blocks"][i];
        const std::string bname =
            (b.contains("name") && b["name"].is_string()) ? b["name"].get<std::string>()
                                                          : std::string();
        nlohmann::json bdoc = b;
        bdoc.erase("name");                       // name 是块归属标识，不是运行参数
        if (!bdoc.contains("schema_version")) bdoc["schema_version"] = "1";
        const std::string btext = bdoc.dump();
        BlockOutcome o = run_phase1_block(p, ev, cfg, cfg_sha, btext, bname, i, nblocks);
        if (o.rc != astrocs::OK && !have_fail) { agg = o; have_fail = true; }
        if (o.rc == astrocs::CANCELLED) break;    // 用户已要求停：后续块不再起
    }
    if (!have_fail) agg.why = "phase1 complete (blocks=" + std::to_string(nblocks) + ")";
    ev.emit_final(agg.rc, agg.kind, nullptr, agg.why);
    return agg.rc;
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
    // FIX-E2E B1-A2: 必需产物角色核验 —— 声明了 Phase 却无该 Phase 的必需产物角色
    // → 8（旧行为: artifacts 恒空仍 PASS, 空过）。role 由生产节点 manifest 定义,
    // CLI 不做兼容别名（单点键名）。
    {
        const auto arts = m.value("artifacts", nlohmann::json::array());
        for (const auto& ph : m.value("phases", nlohmann::json::array())) {
            if (!ph.is_number_integer()) continue;
            const int phase = ph.get<int>();
            if (phase == 3) {
                bool has_out = false;
                for (const auto& a : arts)
                    if (a.is_object() && a.value("role", std::string()) == "phase3_output")
                        has_out = true;
                if (!has_out) {
                    std::fprintf(stderr, "astrocs: phase3 manifest declares no phase3_output artifact\n");
                    return astrocs::INTEGRITY;   // 8
                }
            } else if (phase == 1 || phase == 2) {
                if (!arts.is_array() || arts.empty()) {
                    std::fprintf(stderr, "astrocs: phase%d manifest declares no artifacts\n", phase);
                    return astrocs::INTEGRITY;   // 8
                }
            }
        }
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

// config init / config validate / config show-effective 与 verify / verify profile 同属
// CLI-001 已删命令面（docs/api/CLI_PROTOCOL_V1.md §1 已删除别名 → rc=2；ASTROCS_DESIGN
// §6.2 唯一命令树无 config */verify*）。全仓零调用点，按 ENGINEERING_SPEC §8 显式退役；
// 现行校验入口 = 会话命令预检（subcommand.h precheck_config）+ doctor --json +
// export 的 resume/manifest 校验（cmd_session3_run）。

// cmd_drizzle / RT-009 graph / CLI-001 modules list·verify·selftest 三个命令面（连同只
// 服务它们的 cli_exe_dir() 清单/模块发现根）同为已删能力（docs/api/CLI_PROTOCOL_V1.md §1
// 已删除别名 → rc=2）。零调用点死代码按 ENGINEERING_SPEC §8 显式退役，能力去向见下条注释。
// CLI-001 已删除 modules list/verify/selftest 用户命令（docs/api/CLI_PROTOCOL_V1.md §1
// 明列 modules */selftest 为已删除别名 → rc=2；ASTROCS_DESIGN §6.2 命令树只有
// normalize/mosaic/export/help/--version/doctor/benchmark）。原实现
// （cmd_modules_list / cmd_modules_verify / cmd_selftest 及 locate_product_manifest /
// product_manifest_base_dir / unit_file_present / load_product_manifest）为零调用点
// 死代码，按 ENGINEERING_SPEC §8「锚存活」显式退役；能力去向 = 安装树产品 manifest
// astrocs.product.json + eng/packaging/verify_install_tree.py + 装载器合同探针
// （eng/tests/abi/mod001_install_load_check.py S7/S8 逐条验证 units 计数 10 / 逐 unit 在位 /
// 装配三校验 / 未登记必败 / 缺 DLL 必败）。

// dispatch: 外部可见（cli_common.h 声明；main.cpp 调用）。
// CLI-001: 用户可见命令面 = §6.2 唯一命令树（normalize/mosaic/export/help/
// --version/doctor/benchmark）。旧命令树（hardware inspect / config * /
// modules * / selftest / test synthetic / verify* / drizzle / benchmark cpu /
// phase1|2|3 *）不在表内，解析阶段即 unknown command → exit 2。
int dispatch(const Parsed& p) {
    const std::string joined = p.join();
    // §9.74 裁决 7-a 定案 1（ASTROCS_DESIGN §6.3）：运行事件流 = **默认输出**，不需要旗标
    // 开启（GUI 用其它语言直接捕获 CLI 输出）。--events-jsonl 保留接受（等价默认行为，
    // 不再是开启开关）；stdout 恒为纯 JSONL/单 JSON 文档，人可读摘要走 stderr。
    astrocs::JsonlEmitter ev(astrocs::make_run_id(), joined);

    if (joined == "--version" || joined == "version") {
        if (p.flags.count("--json")) {
            std::printf("{\"schema_version\":\"1\",\"name\":\"astrocs\",\"version\":\"%s\"}\n",
                        ASTROCS_VERSION_STRING);
        } else {
            std::printf("astrocs %s\n", ASTROCS_VERSION_STRING);
        }
        return astrocs::OK;
    }
    if (joined == "help" || joined == "--help" || joined == "-h") {
        std::fputs(kHelp, stdout);
        return astrocs::OK;
    }
    // 三个平级子命令（§1.2：互不串接，各自独立进程/独立恢复/独立验收）。
    for (const auto& s : astrocs::cli::cmd::session_commands()) {
        if (joined != s.name) continue;
        const astrocs::cli::cmd::Subcommand sub{s.name, s.session};
        return sub.dispatch(p, ev);
    }
    if (joined == "doctor") {
        if (!p.flags.count("--json")) parse_fail("doctor requires --json");
        // G3-11（GAP_AUDIT G3-11）：verify 能力纳入命令树。
        // 落位 = doctor 的机器旗标 --run-manifest <manifest.json>（ASTROCS_DESIGN
        // §7.1 唯一命令树只有 normalize/mosaic/export/help/--version/doctor/
        // benchmark，无独立 verify；verify* 是已删别名 → rc=2，见
        // docs/api/CLI_PROTOCOL_V1.md §1 + eng/tests/cli/test_cli_protocol.py
        // test_03 的负例 ("verify", "--run-manifest", ...) → 2）。
        // 语义 = 04 §3 manifest→status→version→输入 hash→逐 artifact
        // （存在→sha256→size_bytes）；退出码：参数 2 / 输入 3 / 版本 5 /
        // 完整性 8。stdout 恰一个 JSON 文档（--json 纪律）。
        if (p.values.count("--run-manifest")) return cmd_verify(p, ev);
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
    if (joined == "benchmark") {
        // §6.2: benchmark 生成/更新 cpu_profile（机器绑定配置，运行时自动读取）。
        // 数值/并行决策唯一来源 = lib/backend_host；本层不写死任何 ISA/线程数。
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
            const auto cp = std::filesystem::canonical("/proc/self/exe", ec);
            if (!ec) exe_path = cp.string();
        }
#endif
        std::string install_dir = ".";
        if (!exe_path.empty()) {
            std::error_code ec2;
            const auto parent = std::filesystem::path(exe_path).parent_path();
            if (!parent.empty()) install_dir = parent.string();
        }
        std::string cli_bin = install_dir.empty() ? std::string(".") : install_dir + "/astrocs";
#if defined(_WIN32)
        {
            std::error_code ec3;
            if (!std::filesystem::is_regular_file(std::filesystem::u8path(cli_bin), ec3))
                cli_bin += ".exe";
        }
#endif
        const std::string cli_sha = astrocs::backend_host::file_sha256_hex(cli_bin);
        auto pb = astrocs::backend_host::generate_profile_v2(
            "full", ASTROCS_VERSION_STRING, ASTROCS_COMMIT_SHA, cli_sha, install_dir);
        std::string verdict;
        nlohmann::json doc;
        try {
            doc = nlohmann::json::parse(pb.json);
            verdict = astrocs::benchmark_profile_verdict(doc);
        } catch (...) {
            verdict = "FAIL";
        }
        doc["verdict"] = verdict;
        const std::string out_path = install_dir + "/cpu_profile.json";
        {
            std::ofstream f(std::filesystem::u8path(out_path), std::ios::binary | std::ios::trunc);
            if (!f) {
                std::fprintf(stderr, "astrocs: cannot write cpu_profile '%s'\n", out_path.c_str());
                return astrocs::IO;
            }
            f << doc.dump(2) << "\n";
        }
        std::printf("%s %s\n", out_path.c_str(), verdict.c_str());
        if (verdict != "PASS") return astrocs::SCIENCE;
        return astrocs::OK;
    }
    parse_fail("unknown command '" + joined + "'");
}

// ── 会话层分派（命令层 ↔ 会话层唯一契约面，声明见 lib/infrastructure/cli/cli_common.h）──
// 三个会话各自独立执行、独立恢复，互不共享进程状态（ASTROCS_DESIGN §1.2）。
int session_dispatch(int session, SessionOp op, const Parsed& p, astrocs::JsonlEmitter& ev) {
    switch (op) {
        case SessionOp::Run:
            if (session == 1) return cmd_session1_run(p, ev);
            if (session == 2) return cmd_session2_run(p, ev);
            if (session == 3) return cmd_session3_run(p, ev);
            break;
        case SessionOp::Validate: return cmd_phase_validate(p, session, ev);
        case SessionOp::Plan:     return cmd_phase_plan(p, session, ev);
        case SessionOp::Inspect:  return cmd_phase_inspect(p, session, ev);
    }
    parse_fail("unknown session");
}

// 命令层共享的会话信息读取器（lib/infrastructure/cli/subcommand.h 只依赖这三个符号）。
std::string session_config_template(int session) {
    return astrocs::cli::cmd::config_template(static_cast<astrocs::cli::cmd::SessionId>(session));
}
std::string session_run_message(int session) {
    switch (static_cast<astrocs::cli::cmd::SessionId>(session)) {
        case astrocs::cli::cmd::SESSION_NORMALIZE: return "normalize";
        case astrocs::cli::cmd::SESSION_MOSAIC:    return "mosaic";
        case astrocs::cli::cmd::SESSION_EXPORT:    return "export";
        default: return "session";
    }
}
bool session_has_flag(const Parsed& p, const char* flag) {
    return p.flags.count(flag) > 0 || p.values.count(flag) > 0;
}
