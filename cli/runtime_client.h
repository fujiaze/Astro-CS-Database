// RT-008 CLI Runtime client — CLI 通过公开 Runtime API 驱动 pipeline。
// CLI 不再 include session/CFITSIO/AIO/Drizzle 内部头；所有科学执行经此 client 进入唯一 Runtime。
// 只依赖 astrocs/core/runtime.h + module_adapters.h（公开合同）。
#pragma once

#include "astrocs/core/module_adapters.h"
#include "astrocs/core/runtime.h"

#include <atomic>
#include <map>
#include <string>
#include <vector>

namespace astrocs::cli {

// preset → PipelineIR（RT-008: run --phases 解析 preset→IR→Runtime；单 phase 命令用同一 IR 子图）
// phases: 1|2|3 组合（如 {1}, {2}, {3}, {2,3}, {1,2,3}）
// config_json: 运行配置（inputs/output_dir/phaseN 子对象等）
std::string build_pipeline_ir(const std::vector<int>& phases,
                              const std::string& config_json, std::string* err);

// 执行一次 pipeline（同步；取消经 Runtime::cancel）
// MON-002: cancel_ext 为可选外部协作取消源（first-10s gate 快速失败置位）——
// 与信号 cancel_flag 等价转发 rt->cancel()，但不影响 CLI 全局取消态（exit 语义仍由 gate 判定）。
// 返回: exit code（astrocs::OK=0；科学失败=4；IO=7；...）
int run_pipeline(const std::vector<int>& phases, const std::string& config_json,
                 uint32_t budget, std::string* fail_reason,
                 std::atomic<bool>* cancel_ext = nullptr);

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
void collect_node_trace(std::vector<astrocs::core::Runtime::NodeTrace>* out);

// RT-009: 最近一次 run 的 PipelineIR JSON 文本（静态图来源；供 graph 生成）。
const std::string& last_pipeline_ir_json();

// 注册 Phase1/2/3 模块（CLI 启动时调用一次）
astrocs::core::Result<void> register_cli_modules(astrocs::core::ModuleRegistry& reg);

}  // namespace astrocs::cli
