#pragma once

#include <cstddef>
#include <string_view>
#include <utility>

#ifndef ACR_KERNEL
#define ACR_KERNEL
#endif

namespace astro::compute {

using OperationId = std::string_view;

enum class TaskClass {
    elementwise,
    reduction,
    stencil_2d,
    convolution_direct,
    convolution_separable,
    resampling_gather,
    histogram_atomic,
    sparse_gather,
    sparse_scatter,
    branch_heavy,
    batch_independent,
    fft_library,
    gemm_library,
    custom
};

enum class AccessPattern { contiguous, strided, local_neighborhood, random, scatter };
enum class WorkUniformity { uniform, mildly_variable, highly_variable };
enum class IntensityClass { memory_bound, balanced, compute_bound };

struct NumericPolicy {
    enum class Compute { fp32, fp64 } compute{Compute::fp32};
    enum class Accumulator { same, fp64 } accumulator{Accumulator::same};
    bool deterministic_merge{false};
    bool allow_fast_math{false};
};

struct TaskTraits {
    TaskClass task_class{TaskClass::elementwise};
    AccessPattern access{AccessPattern::contiguous};
    WorkUniformity uniformity{WorkUniformity::uniform};
    IntensityClass intensity{IntensityClass::memory_bound};
    NumericPolicy numeric{};
    bool splittable{true};
    bool mixed_device_safe{true};
    bool requires_atomic{false};
    double active_fraction_hint{1.0};
    std::size_t halo_x{};
    std::size_t halo_y{};
};

struct Range1D { std::size_t begin{}, end{}; };
struct Extent2D { std::size_t width{}, height{}; };
struct TileShape { std::size_t width{}, height{}; };

class Event {
public:
    void wait() const;
    [[nodiscard]] bool ready() const noexcept;
    void cancel();
};

template<class T>
class BufferView {
public:
    ACR_KERNEL T& operator[](std::size_t i) const noexcept;
    [[nodiscard]] ACR_KERNEL std::size_t size() const noexcept;
};

template<class Kernel, class... Args>
Event parallel_for(OperationId id, Range1D range, TaskTraits traits,
                   Kernel kernel, Args&&... args);

template<class Kernel, class... Args>
Event parallel_tiles(OperationId id, Extent2D extent, TileShape preferred,
                     TaskTraits traits, Kernel kernel, Args&&... args);

template<class T, class MapKernel, class ReduceOp, class... Args>
T parallel_reduce(OperationId id, Range1D range, TaskTraits traits,
                  T identity, MapKernel map, ReduceOp reduce, Args&&... args);

template<class Kernel, class... Args>
Event parallel_batch(OperationId id, std::size_t item_count, TaskTraits traits,
                     Kernel kernel, Args&&... args);

} // namespace astro::compute
