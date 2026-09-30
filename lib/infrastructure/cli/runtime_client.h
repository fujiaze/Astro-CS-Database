// RT-008 CLI Runtime client — CLI 通过公开 Runtime API 驱动 pipeline。
// CLI 不再 include session/CFITSIO/AIO/Drizzle 内部头；所有科学执行经此 client 进入唯一 Runtime。
// 只依赖 acsd/core/runtime.h + module_adapters.h（公开合同）。
#pragma once

#include "acsd/core/module_adapters.h"
#include "acsd/core/runtime.h"

#include <atomic>
#include <map>
#include <string>
#include <vector>

namespace acsd::cli {

// P-158: ErrorDomain → CLI 退出码（docs/engineering/LOG_AND_ERROR_CONTRACT.md §5 唯一映射表；
// 码值语义唯一源 = lib/infrastructure/cli/exit_codes.h，本层不重定义数值）。
// 未列出的域一律 INTERNAL(70)（§5 末条）。**仅用于 load_pipeline 失败面**：
// run_pipeline 内 rt_ret 失败 switch 的既有域映射偏差（F-EXIT-MAP：
// CONFIG/BACKEND→70、RESOURCE→5）按台账保持不动，不在本任务改码。
int exit_code_for_error_domain(acsd::core::ErrorDomain domain);

// preset → PipelineIR（RT-008: run --phases 解析 preset→IR→Runtime；单 phase 命令用同一 IR 子图）
// phases: 1|2|3 组合（如 {1}, {2}, {3}, {2,3}, {1,2,3}）
// config_json: 运行配置（inputs/output_dir/phaseN 子对象等）
std::string build_pipeline_ir(const std::vector<int>& phases,
                              const std::string& config_json, std::string* err);

// 执行一次 pipeline（同步；取消经 Runtime::cancel）
// MON-002: cancel_ext 为可选外部协作取消源 —— 与信号 cancel_flag 等价转发 rt->cancel()，
// 但不影响 CLI 全局取消态。
// （docs/ACSD_DESIGN §4.5/§6.3）: 一般性资源超限门已取消 ⇒ CLI 不再因
// 资源判据（CPU/内存/线程）置位本取消源（原 first-10s gate 快速失败接线已退役）；
// 参数保留为通用外部取消面，调用点当前恒传 nullptr。
// 返回: exit code（acsd::OK=0；科学失败=4；IO=7；...）
// memory_limit_bytes / memory_source —— 内存静态预算（§8.3）。
//   来源 = 调用方按「配置/profile + 实测可用内存」解析（见 acsd/core/memory_budget.h）；
//   本函数只透传给 create_runtime(RuntimeResourceBudget)，**不发明数值**。
//   0 = 未提供 ⇒ 不启用内存回压（语义同旧行为，便于既有调用方零改动）。
// memory_available_bytes / memory_budget_percent —— 预算推导的输入回显（证据面；
//   只进观测，不参与判定）。压力分子来源（进程树 RSS）由本层注入 aio 探针：
//   aio 是文件级唯一 I/O 边界（docs/ACSD_DESIGN §10），core 不因此链接 acsd_aio。
//   压力事件台账落 <config.output_dir>/memory_pressure_ledger.jsonl（JSONL，只增不改）。
int run_pipeline(const std::vector<int>& phases, const std::string& config_json,
                 uint32_t budget, std::string* fail_reason,
                 std::atomic<bool>* cancel_ext = nullptr,
                 uint64_t memory_limit_bytes = 0,
                 const std::string& memory_source = std::string("none"),
                 uint64_t memory_available_bytes = 0,
                 uint32_t memory_budget_percent = 0);

// 执行后收集每个节点的 session manifest 摘要（node_id → JSON 文本）。
// 供 CLI 写 run manifest 时逐 artifact 验证（ArtifactStore 绑定语义）。
void collect_node_manifests(std::vector<std::pair<std::string, std::string>>* out);

// P1-5: 从节点 session manifest 集合收集 artifacts 工件路径（保持出现顺序、按路径去重）。
// 节点 id 是节点图 id（coverage/sample/upm_fit/upm_apply/reject/integrate/write 等链式
// id），任何节点 manifest 都可能带 artifacts 数组——按内容收集，不按硬编码节点名过滤
// （phase2 "res"/phase3 "hips" 旧过滤均指向已不存在的 node id → artifacts 恒空缺陷）。
std::vector<std::string> collect_node_artifact_paths(
    const std::vector<std::pair<std::string, std::string>>& manifests);

// RT-009: 执行后收集节点级 trace（node_id/status/时间/duration/workers/provider）。
// 供 CLI 生成 observed graph 与 sidecar（CHK-002 双向比较输入）。
void collect_node_trace(std::vector<acsd::core::Runtime::NodeTrace>* out);

// RT-009: 最近一次 run 的 PipelineIR JSON 文本（静态图来源；供 graph 生成）。
const std::string& last_pipeline_ir_json();

// 注册 Phase1/2/3 模块（CLI 启动时调用一次）
acsd::core::Result<void> register_cli_modules(acsd::core::ModuleRegistry& reg);

}  // namespace acsd::cli
