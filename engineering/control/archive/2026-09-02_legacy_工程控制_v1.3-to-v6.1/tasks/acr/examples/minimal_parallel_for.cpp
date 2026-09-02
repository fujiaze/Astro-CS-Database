#include <astro/compute/acr.hpp>

void scale_image(float* out, const float* in, std::size_t n, float gain) {
    namespace acr = astro::compute;
    acr::TaskTraits traits{};
    traits.task_class = acr::TaskClass::elementwise;
    traits.access = acr::AccessPattern::contiguous;
    traits.intensity = acr::IntensityClass::memory_bound;
    traits.bytes_read_per_item = sizeof(float);
    traits.bytes_written_per_item = sizeof(float);

    auto event = acr::parallel_for(
        "example.scale.fp32",
        acr::Range1D{0, n},
        traits,
        [=](std::size_t i) { out[i] = in[i] * gain; });
    event.wait();
}
