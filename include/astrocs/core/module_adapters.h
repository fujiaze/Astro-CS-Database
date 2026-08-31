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

}  // namespace astrocs::core
