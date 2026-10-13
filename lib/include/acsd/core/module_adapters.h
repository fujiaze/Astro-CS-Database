// ACSD Core — RT-005 可执行模块适配器
// 将真实 Phase1/2/3 session (C ABI) 包装为 IModule 工厂并注册到 ModuleRegistry。
// 工厂经 acsd_host_services_default_v1 创建 host services（唯一宿主）。
#pragma once

#include "acsd/core/module.h"

#include <memory>

namespace acsd::core {

// 注册三个真实生产模块（Phase1 calibration / Phase2 resample / Phase3 hips）：
// - acsd.phase1.calibration
// - acsd.phase2.resample
// - acsd.phase3.resample
// 每个模块先 register_module(descriptor) 再 register_factory(真实工厂)。
// 注册失败 → 返回失败（任一阶段模块不可执行则整体失败）。
Result<void> register_phase_modules(ModuleRegistry& registry);

// B2-A10（宪章 §4.3）: 运行上下文 run_context.json 的**唯一生成路径**。
// 由 CLI run（phase1/2/3 会话启动前）与 node 级测试夹具共同复用，避免
// schema 漂移；原子写(tmp+rename)，失败返回失败（绝不半写）。
// 字段: schema_version/kind/run_id/software_version/source_sha（+ 可选 work_dir）。
// 注: input_manifest_hash 不在此文件——它由 phase 节点从**真实输入产品字节**
// （Phase3 writer: signal/properties + signal/Moc.fits）派生，禁 CLI 侧占位。
// work_dir（可选，加性）：本运行的**中间产物工作区实际路径**（阶段一）。消费者
// 读它、**不自行推导命名规则**（位置可被配置覆盖 ⇒ 推导会错）。缺省 nullptr/空串
// ⇒ 不写该键（既有调用方与既有产物逐字节不变）。
Result<void> write_run_context(const std::string& out_dir, const std::string& run_id,
                               const std::string& software_version,
                               const std::string& source_sha,
                               const char* work_dir = nullptr);

// 中间产物工作区路径的**唯一实现**（阶段一）：work_dir_override 非空 ⇒ 原样返回；
// 否则 = <output_dir 基名>.p1-work（**同级兄弟目录**、同文件系统 ⇒ 保持「同目录
// 临时文件 → rename」原子替换语义）。配置面见 CONFIG.md（work_dir 键）。
std::string resolve_work_dir(const std::string& output_dir,
                             const std::string& work_dir_override);

// PSF-FAST-001 (依据 ENGINEERING_SPEC.md §2「保留则注释」): 精确 PSF 节点路径**保留但 inactive**
// —— 生产路径不调用（当前 DATA-P1-PSF.psf_params 零消费者、psf 端口为死边,
// 测光正式口径 = 孔径测光）。本声明只暴露**直调测试钩子**, 供测试证明精确
// 实现仍可编译且能跑出结果, 防止 inactive 代码被当作死代码清理;
// 生产注册表 (register_phase_modules) 不注册本路径。
// 等价于节点内部 p1_op_star_psf_impl(..., n_fit_limit=0): 全量星 Moffat4 拟合。
Result<void> p1_op_star_psf_precise_json(const std::string& config_json,
                                         std::string* manifest_json);

}  // namespace acsd::core
