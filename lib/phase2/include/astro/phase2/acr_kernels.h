// lib/phase2/include/astro/phase2/acr_kernels.h
#pragma once

namespace astro::compute::phase2 {

extern const char* kOpMosaicReject;

// 注册 Phase2 合成 Operation（幂等）：CPU legacy launcher 走 phase2
// rejection+integration 语义；ACR 只加速不改变科学语义。
void register_phase2_acr_kernels();

} // namespace astro::compute::phase2
