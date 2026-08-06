// lib/acr/examples/weighted_integration/weighted_integration_kernels.hpp
#pragma once

#include "weighted_integration_common.hpp"

namespace astro::compute::weighted_integration {

// SerialReference（quick 小 case 数值参考）
void weighted_integration_serial(const WeightedIntegrationView& v,
                                 float* output);

// OpenMPBaseline（独立性能基线；与 ACR CPU 共享同一逐像素核心）
void weighted_integration_openmp(const WeightedIntegrationView& v,
                                 float* output,
                                 int threads);

// KernelRegistry 注册 CPU + CUDA launcher（幂等）
void register_weighted_integration_kernels();

} // namespace astro::compute::weighted_integration
