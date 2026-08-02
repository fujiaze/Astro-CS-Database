#include <astro/compute/acr.hpp>

struct AxpyItem {
    ACR_KERNEL void operator()(std::size_t i,
                               astro::compute::BufferView<float> out,
                               astro::compute::BufferView<const float> x,
                               astro::compute::BufferView<const float> y,
                               float alpha) const noexcept {
        out[i] = alpha * x[i] + y[i];
    }
};

void submit_axpy(std::size_t n,
                 astro::compute::BufferView<float> out,
                 astro::compute::BufferView<const float> x,
                 astro::compute::BufferView<const float> y,
                 float alpha) {
    using namespace astro::compute;
    TaskTraits traits{};
    traits.task_class = TaskClass::elementwise;
    traits.access = AccessPattern::contiguous;
    traits.intensity = IntensityClass::memory_bound;

    parallel_for("classic.axpy.fp32", {0, n}, traits,
                 AxpyItem{}, out, x, y, alpha).wait();
}
