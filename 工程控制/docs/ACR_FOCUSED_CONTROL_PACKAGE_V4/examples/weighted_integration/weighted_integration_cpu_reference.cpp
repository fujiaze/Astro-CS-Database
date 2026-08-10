#include "weighted_integration_contract.hpp"
#include <cstddef>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace acr_example {

void weighted_integration_serial(const WeightedIntegrationView& v,
                                 float* output) {
    integrate_range(v, 0, v.pixel_count, output);
}

void weighted_integration_openmp(const WeightedIntegrationView& v,
                                 float* output,
                                 int threads) {
#ifdef _OPENMP
    omp_set_dynamic(0);
    if (threads > 0) omp_set_num_threads(threads);
#pragma omp parallel for schedule(static)
    for (long long p = 0; p < static_cast<long long>(v.pixel_count); ++p) {
        output[static_cast<std::size_t>(p)] =
            integrate_one_pixel(v, static_cast<std::size_t>(p));
    }
#else
    (void)threads;
    weighted_integration_serial(v, output);
#endif
}

// Agent落地时：把本函数注册为KernelRegistry CPU launcher。
// inv.domain就是ACR领取的独占输出像素范围；函数内部禁止再开OpenMP。
void weighted_integration_cpu_range(const WeightedIntegrationView& v,
                                    std::size_t begin,
                                    std::size_t end,
                                    float* output) {
    integrate_range(v, begin, end, output);
}

} // namespace acr_example
