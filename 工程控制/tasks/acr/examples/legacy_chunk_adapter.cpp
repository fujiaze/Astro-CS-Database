#include <astro/compute/acr.hpp>

// 未来接入时，仅当[begin,end)间没有依赖和共享写冲突，才可包装旧的串行范围函数。
struct SerialRangeKernel {
    ACR_KERNEL void operator()(std::size_t i,
                               astro::compute::BufferView<float> out,
                               astro::compute::BufferView<const float> in) const noexcept {
        out[i] = in[i] * in[i];
    }
};

void submit_range(std::size_t n,
                  astro::compute::BufferView<float> out,
                  astro::compute::BufferView<const float> in) {
    using namespace astro::compute;
    TaskTraits traits{};
    traits.task_class = TaskClass::elementwise;
    traits.access = AccessPattern::contiguous;
    parallel_for("example.square.fp32", {0, n}, traits,
                 SerialRangeKernel{}, out, in).wait();
}
