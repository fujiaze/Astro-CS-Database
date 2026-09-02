#include <astro/compute/acr.hpp>
#include <functional>

float sum_pixels(const float* input, std::size_t n) {
    namespace acr = astro::compute;
    acr::TaskTraits traits{};
    traits.task_class = acr::TaskClass::reduction;
    traits.access = acr::AccessPattern::contiguous;
    traits.numeric.compute = acr::NumericPolicy::Compute::fp32;
    traits.numeric.accumulator = acr::NumericPolicy::Accumulator::fp64;

    return acr::parallel_reduce<float>(
        "example.sum.fp32_acc64",
        acr::Range1D{0, n},
        traits,
        0.0f,
        [=](std::size_t i) { return input[i]; },
        std::plus<float>{});
}
