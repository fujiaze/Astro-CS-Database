// lib/acr/tests/unit/test_cuda_bridge_chunk.cpp — CUDA 分块卷积与缓冲容量测试
//
// 25 §2：
// - 分块 3×3 卷积使用全局输出索引读图、chunk-local 写输出；
// - 强制 2/3/17 个 GPU 块、行中间/行末边界、非整除 tail；
// - 32×32、257×263、2048×2048 随机输入与 CPU reference 全像素对比；
// - 独立容量记账（small→large→medium 扩缩容交错，无越界）。
// 无 GPU/桥接时 GTEST_SKIP。
#include <gtest/gtest.h>

#include "../backends/cuda/bridge/cuda_bridge_api.hpp"
#include "../backends/classic/classic_kernels.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using astro::compute::cuda::bridge::api;
using astro::compute::cuda::bridge::ensure_bridge_loaded;

namespace {

// CPU 3x3 clamp 卷积 reference
void conv3x3_cpu(const std::vector<float>& img,
                 std::size_t w, std::size_t h,
                 const float* k9,
                 std::vector<float>& out) {
    const std::size_t n = w * h;
    out.assign(n, 0.0f);
    for (std::size_t p = 0; p < n; ++p) {
        const int px = static_cast<int>(p % w);
        const int py = static_cast<int>(p / w);
        float acc = 0.0f;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const int nx = px + dx;
                const int ny = py + dy;
                if (nx < 0 || ny < 0 || nx >= static_cast<int>(w) ||
                    ny >= static_cast<int>(h)) {
                    continue;  // clamp-to-zero 边界（与 CUDA kernel 一致）
                }
                acc += img[static_cast<std::size_t>(ny) * w + nx] *
                       k9[static_cast<std::size_t>(dy + 1) * 3 + (dx + 1)];
            }
        }
        out[p] = acc;
    }
}

// 分块调用桥接卷积并验证与 CPU reference 全像素一致
void run_chunked_conv(void* handle,
                      std::size_t w, std::size_t h,
                      std::size_t chunk_size,
                      const std::vector<float>& img,
                      const float* k9) {
    const std::size_t n = w * h;
    std::vector<float> y(n, 0.0f);
    std::size_t begin = 0;
    while (begin < n) {
        const std::size_t len = std::min(chunk_size, n - begin);
        std::uint64_t elapsed = 0;
        const char* err = nullptr;
        const int rc = api().submit_conv3x3(
            handle, begin, begin + len, y.data(), img.data(),
            w, h, k9, &elapsed, &err);
        ASSERT_EQ(rc, 0) << (err ? err : "conv failed");
        begin += len;
    }

    std::vector<float> ref;
    conv3x3_cpu(img, w, h, k9, ref);
    for (std::size_t p = 0; p < n; ++p) {
        EXPECT_NEAR(y[p], ref[p], 1e-4) << "pixel " << p << " (w=" << w << ")";
    }
}

} // anonymous namespace

TEST(CudaBridgeChunk, ConvolutionMultiBlockMatchesCpu) {
    ensure_bridge_loaded();
    if (!api().loaded()) {
        GTEST_SKIP() << "no CUDA bridge/device available";
    }
    const char* err = nullptr;
    if (api().init(&err) <= 0) {
        GTEST_SKIP() << "no CUDA device";
    }
    void* handle = api().executor_create(0, 65536, 256, &err);
    ASSERT_NE(handle, nullptr);

    float k9[9] = {1, 0, -1, 2, 0, -2, 1, 0, -1};

    struct Case { std::size_t w, h; std::size_t chunk; };
    const Case cases[] = {
        {32, 32, 512},    // 2 块
        {32, 32, 342},    // 3 块（非整除）
        {32, 32, 61},     // 17 块
        {257, 263, 33795},  // 2 块（行中间边界）
        {257, 263, 22531},  // 3 块
        {257, 263, 3976},   // 17 块
        {2048, 2048, 2097152},  // 2 块
        {2048, 2048, 1398101},  // 3 块
        {2048, 2048, 246723},   // 17 块
        {100, 10, 300},         // 非整除 tail（最后一块 100）
    };

    for (const auto& c : cases) {
        const std::size_t n = c.w * c.h;
        std::vector<float> img(n);
        unsigned seed = static_cast<unsigned>(c.w * 2654435761u + c.h);
        for (auto& v : img) {
            seed = seed * 1103515245u + 12345u;
            v = static_cast<float>((seed >> 8) % 1000) / 1000.0f;
        }
        SCOPED_TRACE("case w=" + std::to_string(c.w) +
                     " h=" + std::to_string(c.h) +
                     " chunk=" + std::to_string(c.chunk));
        run_chunked_conv(handle, c.w, c.h, c.chunk, img, k9);
    }
    api().executor_destroy(handle);
}

TEST(CudaBridgeChunk, CapacityIndependentResizeCycle) {
    ensure_bridge_loaded();
    if (!api().loaded()) {
        GTEST_SKIP() << "no CUDA bridge/device available";
    }
    const char* err = nullptr;
    if (api().init(&err) <= 0) {
        GTEST_SKIP() << "no CUDA device";
    }
    void* handle = api().executor_create(0, 65536, 256, &err);
    ASSERT_NE(handle, nullptr);

    // small → large → medium → large 交错（触发 d_x/d_y 独立扩容，防越界）
    const std::size_t sizes[] = {1000, 1u << 21, 5000, 1u << 20, 1u << 22};
    for (std::size_t sz : sizes) {
        std::vector<float> x(sz, 1.0f);
        std::vector<float> y(sz, 2.0f);
        std::uint64_t elapsed = 0;
        const char* err2 = nullptr;
        const int rc = api().submit_axpy(handle, 0, sz, y.data(), x.data(),
                                         2.0f, &elapsed, &err2);
        ASSERT_EQ(rc, 0) << (err2 ? err2 : "axpy failed");
        for (std::size_t i = 0; i < sz; ++i) {
            ASSERT_FLOAT_EQ(y[i], 4.0f);
        }
    }

    // partials 与 kernel 独立容量（交替增长；span = classic::kReduceBlocks）
    {
        constexpr std::size_t kSpan = astro::compute::classic::kReduceBlocks;
        std::vector<float> x(1u << 18, 1.0f);  // 262144 → blocks=1024 ≤ span
        std::vector<double> partials(2 * kSpan, 0.0);
        std::uint64_t elapsed = 0;
        const char* err2 = nullptr;
        ASSERT_EQ(api().submit_reduce(handle, 0, x.size(), x.data(),
                                      partials.data(), kSpan, 0,
                                      &elapsed, &err2), 0);
        std::vector<float> img(64 * 64, 1.0f);
        std::vector<float> y(64 * 64, 0.0f);
        float k9[9] = {1, 0, -1, 2, 0, -2, 1, 0, -1};
        ASSERT_EQ(api().submit_conv3x3(handle, 0, 64 * 64, y.data(), img.data(),
                                       64, 64, k9, &elapsed, &err2), 0);
        // 再次 reduce（partials 复用已扩容缓冲）
        ASSERT_EQ(api().submit_reduce(handle, 0, x.size(), x.data(),
                                      partials.data(), kSpan, 1,
                                      &elapsed, &err2), 0);
        double total = 0.0;
        for (double v : partials) total += v;
        EXPECT_NEAR(total, static_cast<double>(x.size()) * 2.0, 1e-2);
    }

    api().executor_destroy(handle);
}
