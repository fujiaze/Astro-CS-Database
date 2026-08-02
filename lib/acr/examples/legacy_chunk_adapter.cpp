// lib/acr/examples/legacy_chunk_adapter.cpp
// 演示用 parallel_chunks 适配旧 chunk 处理风格代码
//
// 旧代码常以 [begin, end) 区间为单位处理数据（如分块读取、分块变换）。
// parallel_chunks 按 chunk_size 把 [0, N) 拆分为多个连续 chunk，
// 每个 chunk 调用一次处理函数 (b, e)。无需重写旧逻辑即可获得并行加速。
//
// 适配要点：
//   - 旧函数签名 process_chunk(float* data, size_t b, size_t e) 不变
//   - 用 lambda 包裹调用 parallel_chunks
//   - chunk 间无依赖时安全并行；有依赖则需同步

#include <cstddef>
#include <cstdio>

#include "astro/compute/acr.hpp"

namespace {

// 模拟旧代码：处理 [b, e) 区间内的数据
// 此处做原地变换：data[i] = data[i] * 2.0f + 1.0f
void process_chunk(float* data, std::size_t b, std::size_t e) {
    for (std::size_t i = b; i < e; ++i) {
        data[i] = data[i] * 2.0f + 1.0f;
    }
}

}  // namespace

int main() {
    using namespace astro::compute;

    runtime_init();

    constexpr std::size_t N = 1u << 18;  // 256K 元素
    constexpr std::size_t chunk_size = 1024;

    Buffer<float> buf(N);
    // 初始化为 1.0f
    parallel_for(KernelId::Custom, Range1D{0, N},
        [&](std::size_t i) { buf[i] = 1.0f; });

    // 用 parallel_chunks 把 [0, N) 按 chunk_size=1024 拆分，
    // 每个 chunk 调用旧函数 process_chunk
    Event ev = parallel_chunks(
        KernelId::Custom, Range1D{0, N}, chunk_size,
        [&](std::size_t b, std::size_t e) {
            process_chunk(buf.data(), b, e);
        });
    ev.wait();

    // 验证：每个元素应为 1.0 * 2 + 1 = 3.0f
    bool ok = true;
    for (std::size_t i = 0; i < N; ++i) {
        if (buf[i] != 3.0f) { ok = false; break; }
    }

    std::printf("=== ACR legacy_chunk_adapter ===\n");
    std::printf("N          = %zu\n", N);
    std::printf("chunk_size = %zu\n", chunk_size);
    std::printf("chunks     = %zu\n", (N + chunk_size - 1) / chunk_size);
    std::printf("all elements == 3.0f: %s\n", ok ? "yes" : "no");

    runtime_shutdown();
    return ok ? 0 : 1;
}
