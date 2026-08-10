// lib/acr/qualification/benchmarks/convolution_benchmark.cpp — 2D 直接卷积吞吐 Benchmark
//
// 设计（06_QUALIFICATION_BENCHMARK_SPEC.md + 17_CLASSIC_EXPERIMENT_SUITE.md）：
//   1. 2D 直接卷积吞吐测量（direct convolution, no FFT）
//   2. 卷积核尺寸：3x3 / 5x5 / 7x7（naive 实现，无 FFT、无 im2col）
//   3. FP32 精度，单线程
//   4. 输入图像尺寸参数化：64x64 / 128x128 / 256x256 / 512x512
//   5. 边界处理：零填充（out-of-bounds 视为 0）
//   6. 报告 MOp/s（ops = width * height * kernel_w * kernel_h * 2，乘加算 2 ops）
//   7. 用于 CPU 硬件画像补全（CapabilityFamily::Convolution）
#include "benchmark_common.hpp"

#include <benchmark/benchmark.h>

#include <string>
#include <vector>

namespace astro::compute::qualification::bench {

namespace {

// 2D 直接卷积 kernel（naive，零填充边界）
// output[y * width + x] = sum_{ky,kx} input[(y+ky-half)*width + (x+kx-half)] * kernel[ky*ksize+kx]
// 越界输入视为 0（零填充）。
void conv2d_direct_fp32(float* output,
                        const float* input,
                        const float* kernel,
                        int width, int height,
                        int ksize) {
    const int half = ksize / 2;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float sum = 0.0f;
            for (int ky = 0; ky < ksize; ++ky) {
                const int iy = y + ky - half;
                if (iy < 0 || iy >= height) continue;  // 零填充
                for (int kx = 0; kx < ksize; ++kx) {
                    const int ix = x + kx - half;
                    if (ix < 0 || ix >= width) continue;  // 零填充
                    sum += input[iy * width + ix] * kernel[ky * ksize + kx];
                }
            }
            output[y * width + x] = sum;
        }
    }
}

} // anonymous namespace

// ===== Fixture =====
class ConvolutionFixture : public ::benchmark::Fixture {
public:
    std::vector<float> input;
    std::vector<float> output;
    std::vector<float> kernel;
    int width{0};
    int height{0};
    int ksize{0};

    void SetUp(const ::benchmark::State& st) override {
        width = static_cast<int>(st.range(0));
        height = width;
        // 从 benchmark name 解析 kernel size
        const std::string name = st.name();
        if (name.find("3x3") != std::string::npos) ksize = 3;
        else if (name.find("5x5") != std::string::npos) ksize = 5;
        else if (name.find("7x7") != std::string::npos) ksize = 7;
        else ksize = 3;  // 默认 3x3

        const std::size_t img_n = static_cast<std::size_t>(width) * height;
        const std::size_t ker_n = static_cast<std::size_t>(ksize) * ksize;
        input.resize(img_n);
        output.resize(img_n);
        kernel.resize(ker_n);
        fill_uniform(input.data(), img_n, kBenchmarkSeed);
        fill_uniform(kernel.data(), ker_n, kBenchmarkSeed ^ 0xCAFE0001ULL);
    }

    void TearDown(const ::benchmark::State& st) override {
        if (!output.empty()) {
            do_not_optimize_array(output.data(), output.size());
        }
        (void)st;
    }
};

// 通用 benchmark body
static void run_conv_bench(ConvolutionFixture* self, ::benchmark::State& state) {
    if (self->ksize <= 0 || self->width <= 0) {
        state.SkipWithError("invalid convolution parameters");
        return;
    }
    float* out = self->output.data();
    const float* in = self->input.data();
    const float* ker = self->kernel.data();
    const int w = self->width;
    const int h = self->height;
    const int k = self->ksize;

    // 预热一次
    conv2d_direct_fp32(out, in, ker, w, h, k);

    for (auto _ : state) {
        conv2d_direct_fp32(out, in, ker, w, h, k);
        ACR_BENCH_ASM_MEMORY_BARRIER();
    }

    // 防止编译器消除输出
    do_not_optimize_array(out, static_cast<std::size_t>(w) * h);

    // ops = width * height * ksize * ksize * 2（每个像素的乘加算 2 ops）
    const int64_t ops_per_iter = static_cast<int64_t>(w) * h * k * k * 2;
    state.SetItemsProcessed(ops_per_iter * state.iterations());
    // 报告 MOp/s
    state.counters["MOp/s"] = ::benchmark::Counter(
        static_cast<double>(ops_per_iter) * 1e-6,
        ::benchmark::Counter::kIsIterationInvariantRate);
}

// ===== 注册 benchmark =====
BENCHMARK_DEFINE_F(ConvolutionFixture, conv2d_direct_3x3_fp32)(::benchmark::State& st) {
    run_conv_bench(this, st);
}
BENCHMARK_REGISTER_F(ConvolutionFixture, conv2d_direct_3x3_fp32)
    ->Arg(64)->Arg(128)->Arg(256)->Arg(512)
    ->Unit(::benchmark::kMillisecond);

BENCHMARK_DEFINE_F(ConvolutionFixture, conv2d_direct_5x5_fp32)(::benchmark::State& st) {
    run_conv_bench(this, st);
}
BENCHMARK_REGISTER_F(ConvolutionFixture, conv2d_direct_5x5_fp32)
    ->Arg(64)->Arg(128)->Arg(256)->Arg(512)
    ->Unit(::benchmark::kMillisecond);

BENCHMARK_DEFINE_F(ConvolutionFixture, conv2d_direct_7x7_fp32)(::benchmark::State& st) {
    run_conv_bench(this, st);
}
BENCHMARK_REGISTER_F(ConvolutionFixture, conv2d_direct_7x7_fp32)
    ->Arg(64)->Arg(128)->Arg(256)->Arg(512)
    ->Unit(::benchmark::kMillisecond);

} // namespace astro::compute::qualification::bench
