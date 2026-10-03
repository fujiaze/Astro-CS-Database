// lib/algorithms/coverage/src/accelerator_fallback.cpp
//
// 「无可用加速器时回退到 CPU canonical 路径」的路由决策实现。
//
// 本单元刻意**不** include 任何异构计算运行时头：它是纯决策，不触执行面。
// 这样即使异构计算实验子树整体退场，回退路径依然自洽可用 —— 它只需要
// 「请求了哪条路由」和「加速器是否可用」两个输入。
#include "astro/phase2/accelerator_fallback.h"

namespace astro::phase2 {

AcceleratorFallbackDecision p2_resolve_accelerator_fallback(
    const std::string& requested_route, bool accelerator_available) {
    AcceleratorFallbackDecision d;
    if (accelerator_available) {
        // 有可用加速器：请求被满足，不发生回退。
        d.effective_route = "cuda";
        d.fallback_reason.clear();
        return d;
    }
    // 无可用加速器：回退到 CPU canonical 路径（同一科学语义，CPU reference 权威）。
    d.effective_route = "cpu";
    // 回退原因按**请求的路由**区分，两个标记逐字保留（见头文件说明）。
    // requested_route == "cpu" 时本就请求 CPU ⇒ 未发生回退 ⇒ 原因串留空。
    if (requested_route == "auto") {
        d.fallback_reason = kFallbackReasonNoAcceleratorAuto;
    } else if (requested_route == "cuda") {
        d.fallback_reason = kFallbackReasonAcceleratorUnavailable;
    }
    return d;
}

}  // namespace astro::phase2