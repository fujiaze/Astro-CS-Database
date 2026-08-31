// RT-008 CLI Runtime client — CLI 通过公开 Runtime API 驱动 pipeline。
// CLI 不再 include session/CFITSIO/AIO/Drizzle 内部头；所有科学执行经此 client 进入唯一 Runtime。
// 只依赖 astrocs/core/runtime.h + module_adapters.h（公开合同）。
#pragma once

#include "astrocs/core/module_adapters.h"
#include "astrocs/core/runtime.h"

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
// 返回: exit code（astrocs::OK=0；科学失败=4；IO=7；...）
int run_pipeline(const std::vector<int>& phases, const std::string& config_json,
                 uint32_t budget, std::string* fail_reason);

// 执行后收集每个节点的 session manifest 摘要（node_id → JSON 文本）。
// 供 CLI 写 run manifest 时逐 artifact 验证（ArtifactStore 绑定语义）。
void collect_node_manifests(std::vector<std::pair<std::string, std::string>>* out);

// 注册 Phase1/2/3 模块（CLI 启动时调用一次）
astrocs::core::Result<void> register_cli_modules(astrocs::core::ModuleRegistry& reg);

}  // namespace astrocs::cli
