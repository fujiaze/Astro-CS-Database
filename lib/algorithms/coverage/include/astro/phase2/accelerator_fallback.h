// lib/algorithms/coverage/include/astro/phase2/accelerator_fallback.h
//
// 「无可用加速器时回退到 CPU canonical 路径」的路由决策。
//
// 本单元是**产品行为**，不是异构计算实验的一部分，因此落在覆盖模块自己的
// 命名空间 `astro::phase2` 下、且只依赖 <string>：
//   - 它不引用任何异构计算运行时类型（registry / invocation / device executor /
//     cuda bridge），因此在异构计算子树完全缺席的构建里独立成立；
//   - 它只回答「请求了哪条路由、实际生效哪条、为什么回退」，不回答「怎么执行」。
//
// 上游：docs/ACSD_DESIGN.md §1.3（非目标：GPU 与 CPU/GPU 混合生产路由不实现）
// 与 §9（生产计算后端为纯 CPU 自适应后端）。
// 下游消费者：lib/algorithms/coverage/tools/stage2.cpp（mosaic 集成入口）。
#pragma once

#include <string>

namespace astro::phase2 {

// 无可用加速器时的**回退原因标记**。
//
// 这两个字符串是产品行为的一部分：mosaic 诊断面（diagnostics.json 的
// acr_fallback_reason 键）逐字记录它们并交给下游消费，属公开键面的一部分
// （docs/engineering/api/PUBLIC_API.md 的 diagnostics.json 键集）。
// 字符串内容不得改写、不得改名、不得合并——下游按字面比对。
inline constexpr const char* kFallbackReasonNoAcceleratorAuto =
    "linux_no_cuda_auto_fallback";
inline constexpr const char* kFallbackReasonAcceleratorUnavailable =
    "cuda_unavailable_fallback";

// 回退决策结果。fallback_reason 为空串表示本次未发生回退。
struct AcceleratorFallbackDecision {
    std::string effective_route;  // 实际生效的集成执行路由
    std::string fallback_reason;  // 回退原因标记；未回退时为空
};

// 按「请求的路由」与「本进程是否持有可用加速器」决定实际生效路由与回退原因。
//
// 生产侧恒传 accelerator_available=false：最高设计把 GPU 与 CPU/GPU 混合生产
// 路由列为非目标，生产计算后端是纯 CPU 自适应后端，故生产不存在加速器子系统
// 可用。参数保留而非写死常量，是为了让「回退」仍是一条**决策**（判据是加速器
// 可用性），而不是一段恒定的赋值。
AcceleratorFallbackDecision p2_resolve_accelerator_fallback(
    const std::string& requested_route, bool accelerator_available);

}  // namespace astro::phase2