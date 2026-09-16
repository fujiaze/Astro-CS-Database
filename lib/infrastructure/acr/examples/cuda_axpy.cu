// lib/acr/examples/cuda_axpy.cu — Phase D CUDA AXPY 示例
// 在 RTX 3060 Ti 上跑 AXPY，打印设备信息、结果校验、耗时。
//
// 编译：ACR_BUILD_CUDA=ON 时由 examples/CMakeLists.txt 编译。
#ifdef ACR_BUILD_CUDA

#include <cstddef>
#include <cstdio>
#include <vector>

#include "cuda_backend.hpp"
#include "cuda_buffer.hpp"

int main() {
    using namespace astro::compute::cuda;
    namespace acr = astro::compute;

    auto& backend = CudaBackend::instance();
    acr::StatusCode s = backend.initialize();
    if (s != acr::StatusCode::Ok || !backend.available()) {
        std::printf("No CUDA device available (status=%d). Fallback to CPU.\n",
                    static_cast<int>(s));
        return 0;
    }

    const auto& info = backend.device_info();
    std::printf("=== ACR CUDA AXPY Example ===\n");
    std::printf("Device:        %s\n", info.name.c_str());
    std::printf("UUID:          %s\n", info.uuid.c_str());
    std::printf("Compute:       %d.%d\n", info.compute_major, info.compute_minor);
    std::printf("SM count:      %d\n", info.sm_count);
    std::printf("Total memory:  %zu MB\n", info.total_memory / (1024 * 1024));
    std::printf("Free memory:   %zu MB\n", info.free_memory / (1024 * 1024));
    std::printf("Driver:        %d.%d\n", info.driver_major, info.driver_minor);
    std::printf("\n");

    constexpr std::size_t kN = 1 << 22;  // 4M elements
    constexpr float kA = 2.0f;
    std::printf("AXPY: N=%zu, a=%.1f\n", kN, kA);

    std::vector<float> x(kN, 1.5f), y(kN, 2.0f);
    CudaBuffer<float> dx(kN), dy(kN);
    if (!dx.valid() || !dy.valid()) {
        std::printf("ERROR: cudaMalloc failed\n");
        return 1;
    }

    // H2D
    cuda_event h2d_start, h2d_end;
    h2d_start.record(backend.stream());
    dx.copy_h2d(x.data(), kN, backend.stream());
    dy.copy_h2d(y.data(), kN, backend.stream());
    h2d_end.record(backend.stream());
    h2d_end.sync();
    std::printf("H2D time:      %.3f ms\n", h2d_end.elapsed_since(h2d_start));

    // Kernel
    cuda_event kern_start, kern_end;
    kern_start.record(backend.stream());
    axpy(dy.data(), dx.data(), kA, kN, backend.stream());
    kern_end.record(backend.stream());
    kern_end.sync();
    std::printf("Kernel time:   %.3f ms\n", kern_end.elapsed_since(kern_start));

    // D2H
    std::vector<float> y_out(kN, 0.0f);
    cuda_event d2h_start, d2h_end;
    d2h_start.record(backend.stream());
    dy.copy_d2h(y_out.data(), kN, backend.stream());
    d2h_end.record(backend.stream());
    d2h_end.sync();
    std::printf("D2H time:      %.3f ms\n", d2h_end.elapsed_since(d2h_start));

    // 校验
    const float expected = kA * 1.5f + 2.0f;  // = 5.0
    bool ok = true;
    for (std::size_t i = 0; i < kN; ++i) {
        if (std::fabs(y_out[i] - expected) > 1e-4f) {
            ok = false;
            std::printf("MISMATCH at i=%zu: got %f, expect %f\n",
                        i, y_out[i], expected);
            break;
        }
    }
    std::printf("Result:        %s (y[0]=%f, expect %f)\n",
                ok ? "PASS" : "FAIL", y_out[0], expected);
    return ok ? 0 : 1;
}

#else
// CPU-only 构建不应编译此文件（CMake 用 if(ACR_BUILD_CUDA) 保护）
int main() { return 0; }
#endif
