// lib/backend_host/profile_store.h — CPU-007 profile 存储生命周期
// 规格(V7.1 FINAL3 04_CPU_RESOURCE_TASKS.md CPU-007; 验收关键词 profile schema):
//   Windows 默认 %LOCALAPPDATA%/AstroCS/cpu_profile.json, Linux XDG data; 写临时→校验→
//   原子 rename; 绑定 schema/product build/CPU/OS/provider hash。
//   验收: 损坏、旧版本、机器变化、半写文件不被使用; 无 profile 运行清晰 warning;
//   profile 不进入源码/审核包原始数据。
// 补充(V8.1 03_P0_REMEDIATION_TASKS.md V8-CPU-002): 没有/失配 profile → 选择
//   generic(baseline) ISA 但仍动态多线程 —— 即装载失败一律回落, 不抛错不阻塞。
//
// 层次合同(不复制校验链): 本文件是 CPU-003 v2 profile(schema astrocs.cpu-profile/v2)
// 与 CPU-005 身份校验(check_profile_identity_v1)之上的**存储生命周期层**。合法性判定
// 全部复用既有唯一实现:
//   - 文本级: verify_profile_v2(schema/必填字段/版本/commit/指纹/kernels 结构)
//   - 身份级: check_profile_identity_v1(build/CPU/OS/provider hash 绑定; 机器变化→stale)
//   - 消费级: profile_kernel_benchmark_valid(oracle/fallback/median 完整性, 供消费方)
// 本层不解析 profile 业务字段、不改变任何选择逻辑; 与 CPU-006 bench_report
// (astrocs.benchmark-report/v1)结构性隔离: 本层只接受 cpu-profile/v2, report 文本
// 经 verify_profile_v2 拒绝 → 落库被拒, 两条 schema 互不渗透。
//
// 原子写协议(load_profile 唯一消费路径下半写文件结构性不可见):
//   1) 同目录临时文件 <target>.tmp-<pid>-<rand>(同 filesystem 保证 rename 原子性);
//   2) 写入全文 → fsync(fd) 落盘 → close;
//   3) 校验: verify_profile_v2(文本) + check_profile_identity_v1(文本, 当前机器);
//      校验失败 → 删除临时文件, 目标文件零接触(返回错误, 不产生半写目标);
//   4) rename(tmp, target)(POSIX 原子; Windows MoveFileEx REPLACE_EXISTING)。
//   崩溃/掉电窗口只可能残留 .tmp 孤儿文件(下次写前被清理), 目标文件要么是旧完整
//   版本要么是新完整版本 —— "半写文件不被使用"由此结构性保证(不依赖读侧容错)。
//
// 失效隔离(CPU-007 验收核心): 损坏/旧版本/机器变化/半写 不被使用 —— load 返回
//   LoadResult.valid=false + 精确 reason(机器可查); 隔离 = 重命名失效文件为
//   <target>.rejected-<utc>(不删除, 供审计/取证; 静默删除会丢失"为什么失效"证据),
//   下次 load 视为无 profile; 消费方按 V8-CPU-002 回落 generic + 动态多线程。
//
// 工程约束落实:
//   - profile 不进入源码/审核包: 默认路径在用户数据目录(XDG/LOCALAPPDATA), 不在
//     源码树; save 拒绝以源码树相对路径落盘(防误提交原始数据)。
//   - 无 profile 运行清晰 warning: load 未命中返回 status="missing" + warning 文本,
//     由消费方(CLI/runtime)原样输出; 本层不打印(库层无 stderr 约定, 同域一致)。
//   - 线程/ISA/block 由 benchmark 选择禁止硬编码: 本层只存取文本, 无任何选择逻辑。
//   - 重计算禁止单线程: 选择语义在 profile 内容本身(CPU-003/006 已冻结), 本层透传。
#ifndef ASTROCS_PROFILE_STORE_H
#define ASTROCS_PROFILE_STORE_H

#include <cstdint>
#include <string>
#include <vector>

namespace astrocs::backend_host {

// ── 默认存储路径(V7.1 规格) ──
// Windows: %LOCALAPPDATA%/AstroCS/cpu_profile.json(LOCALAPPDATA 缺失→回退 USERPROFILE
// → 仍缺失→空串+ok=false); Linux/macOS: XDG_DATA_HOME(缺省 ~/.local/share)/AstroCS/
// cpu_profile.json(HOME 缺失→空串+ok=false)。
// 本函数只做路径合成, 不创建目录、不读盘。
struct PathResult {
    std::string path;   // 绝对路径; 失败为空串
    bool ok = false;
    std::string reason; // ok=false 时的原因(诊断)
};
PathResult default_profile_path_v1();

// ── 原子保存: 写临时 → 校验 → rename ──
// json_text: 候选 profile 全文(schema astrocs.cpu-profile/v2)。
// hw_json: 当前机器硬件画像(hardware_inspect JSON; 空串=跳过机器身份校验, 仅文本级)。
// current_commit: 期望 build.source_commit(空串=跳过 build 绑定校验)。
// 校验失败 → 返回 ok=false + reason, 目标文件与目录零改动(临时文件已清理)。
// 目录不存在 → 自动创建(parent 目录逐级 mkdir; 创建失败→ok=false)。
struct SaveResult {
    bool ok = false;
    std::string path;   // 实际落盘目标路径
    std::string reason; // ok=false 时的原因
};
SaveResult save_profile_atomic_v1(const std::string& json_text,
                                  const std::string& hw_json,
                                  const std::string& current_commit,
                                  const std::string& target_path);

// ── 装载与失效隔离 ──
// 读取 target_path → verify_profile_v2(文本) → check_profile_identity_v1(文本, hw_json,
// current_commit) → 全过才 valid=true。任一环节失败:
//   valid=false + status="rejected" + 精确 reason(损坏/旧版本/机器变化/半写 归类) +
//   rejected_path 非空: 失效文件已改名 <target>.rejected-<utc>(隔离, 不再被消费)。
// target_path 不存在: valid=false + status="missing"(无 profile 运行; warning_text
//   给出清晰提示文本, 消费方按 V8-CPU-002 回落 generic + 动态多线程)。
// hw_json/current_commit 传空串则相应校验跳过(仅文本级 + 消费级)。
// check_consumer: 消费级校验开关(CPU-005 profile_kernel_benchmark_valid); 对
//   kernel_ids 中每个 kernel 校验 oracle/median 完整性, 任一失败 → rejected。空表=跳过。
struct LoadResult {
    bool valid = false;
    std::string status;          // "ok" | "missing" | "rejected"
    std::string reason;          // rejected 时的精确原因(机器可查)
    std::string warning_text;    // missing 时的清晰 warning(消费方原样输出)
    std::string json_text;       // valid=true 时为 profile 全文(否则空)
    std::string rejected_path;   // rejected 时失效文件隔离路径(空=未改名/missing)
};
LoadResult load_profile_checked_v1(const std::string& target_path,
                                   const std::string& hw_json,
                                   const std::string& current_commit,
                                   const std::vector<std::string>& check_consumer);

// ── 失效归类(负向样例共用同一实现; 纯函数) ──
// 输入 verify_profile_v2 与 check_profile_identity_v1 的错误文本, 归类为规格验收类别:
//   "corrupted"   损坏/半写(JSON 解析失败、必填字段缺失、结构非法)
//   "old_schema"  旧版本(schema 非 astrocs.cpu-profile/v2, 含 v1/空/外域 schema)
//   "stale_machine" 机器变化(vendor/family/model/stepping/xcr0/os_abi/features/
//                 benchmark_binary_sha256 任一变化)
//   "stale_build" build 变化(source_commit 不符)
//   "consumer_invalid" 消费级失败(kernel oracle/median 完整性)
// 未知文本 → "corrupted"(保守: 一切不明失效按损坏隔离)。
std::string classify_profile_rejection_v1(const std::string& verify_error,
                                          const std::string& identity_reason);

}  // namespace astrocs::backend_host

#endif  // ASTROCS_PROFILE_STORE_H
