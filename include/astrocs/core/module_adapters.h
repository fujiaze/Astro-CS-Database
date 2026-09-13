// AstroCS Core — RT-005 可执行模块适配器
// 将真实 Phase1/2/3 session (C ABI) 包装为 IModule 工厂并注册到 ModuleRegistry。
// 工厂经 astrocs_host_services_default_v1 创建 host services（唯一宿主）。
#pragma once

#include "astrocs/core/module.h"

#include <memory>

namespace astrocs::core {

// 注册三个真实生产模块（Phase1 calibration / Phase2 resample / Phase3 hips）：
// - astrocs.phase1.calibration
// - astrocs.phase2.resample
// - astrocs.phase3.resample
// 每个模块先 register_module(descriptor) 再 register_factory(真实工厂)。
// 注册失败 → 返回失败（任一阶段模块不可执行则整体失败）。
Result<void> register_phase_modules(ModuleRegistry& registry);

// B2-A10（宪章 §4.3）: 运行上下文 run_context.json 的**唯一生成路径**。
// 由 CLI run（phase1/2/3 会话启动前）与 node 级测试夹具共同复用，避免
// schema 漂移；原子写(tmp+rename)，失败返回失败（绝不半写）。
// 字段: schema_version/kind/run_id/software_version/source_sha。
// 注: input_manifest_hash 不在此文件——它由 phase 节点从**真实输入产品字节**
// （Phase3 writer: signal/properties + signal/Moc.fits）派生，禁 CLI 侧占位。
Result<void> write_run_context(const std::string& out_dir, const std::string& run_id,
                               const std::string& software_version,
                               const std::string& source_sha);

}  // namespace astrocs::core
