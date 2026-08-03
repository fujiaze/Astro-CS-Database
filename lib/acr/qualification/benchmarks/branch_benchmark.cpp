// lib/acr/qualification/benchmarks/branch_benchmark.cpp — 分支发散 Benchmark
//
// 设计（06_QUALIFICATION_BENCHMARK_SPEC.md + 17_CLASSIC_EXPERIMENT_SUITE.md）：
//   1. 分支发散测量（Mandelbrot 集合，uniform vs variable）
//   2. FP64 精度（Mandelbrot 需要双精度），单线程
//   3. 参数化尺寸：256x256 / 512x512 / 1024x1024
//   4. uniform：所有点迭代到最大次数（固定循环，无分支退出）
//   5. variable：标准 Mandelbrot（根据逃逸条件提前退出，分支发散）
//   6. 最大迭代 256 次，|z| > 2 时退出
//   7. 报告 MOp/s（ops = width * height * max_iterations * 4，每次迭代 4 ops：2 mul + 1 add + 1 compare）
//   8. 用于 CPU 硬件画像补全（CapabilityFamily::Branch）
#include "benchmark_common.hpp"

#include <benchmark/benchmark.h>

#include <string>
#include <vector>

namespace astro::compute::qualification::bench {

namespace {

constexpr int kMandelbrotMaxIter = 256;

// uniform：固定迭代，无分支退出
// 所有点都迭代 max_iter 次，不检查逃逸条件。
// 用于测量无分支发散时的吞吐基线。
void mandelbrot_uniform_fp64(double* output,
                             const double* cre,
                             const double* cim,
                             int width, int height,
                             int max_iter) {
    const int n = width * height;
    for (int i = 0; i < n; ++i) {
        double zr = 0.0;
        double zi = 0.0;
        const double cr = cre[i];
        const double ci = cim[i];
        for (int iter = 0; iter < max_iter; ++iter) {
            const double zr2 = zr * zr;
            const double zi2 = zi * zi;
            zi = 2.0 * zr * zi + ci;  // 用旧 zr
            zr = zr2 - zi2 + cr;       // 用旧 zr2/zi2
        }
        output[i] = zr;  // 写入结果防止优化
    }
}

// variable：标准 Mandelbrot，逃逸提前退出（分支发散）
// |z| > 2 时提前退出循环，导致分支发散。
void mandelbrot_variable_fp64(double* output,
                              const double* cre,
                              const double* cim,
                              int width, int height,
                              int max_iter) {
    const int n = width * height;
    for (int i = 0; i < n; ++i) {
        double zr = 0.0;
        double zi = 0.0;
        const double cr = cre[i];
        const double ci = cim[i];
        int iter = 0;
        for (; iter < max_iter; ++iter) {
            const double zr2 = zr * zr;
            const double zi2 = zi * zi;
            if (zr2 + zi2 > 4.0) break;  // |z|^2 > 4 即 |z| > 2，逃逸
            zi = 2.0 * zr * zi + ci;
            zr = zr2 - zi2 + cr;
        }
        output[i] = static_cast<double>(iter);  // 逃逸时间
    }
}

} // anonymous namespace

// ===== Fixture =====
class BranchFixture : public ::benchmark::Fixture {
public:
    std::vector<double> cre;
    std::vector<double> cim;
    std::vector<double> output;
    int width{0};
    int height{0};

    void SetUp(const ::benchmark::State& st) override {
        width = static_cast<int>(st.range(0));
        height = width;
        const std::size_t n = static_cast<std::size_t>(width) * height;
        cre.resize(n);
        cim.resize(n);
        output.resize(n);
        // fill_uniform 生成 [-1, 1) 范围
        fill_uniform(cre.data(), n, kBenchmarkSeed);
        fill_uniform(cim.data(), n, kBenchmarkSeed ^ 0xCAFE0002ULL);
        // 映射到 c 的实部范围 [-2, 1) 和虚部范围 [-1.5, 1.5)
        //   cre = -0.5 + 1.5 * v  (v ∈ [-1,1) → cre ∈ [-2, 1))
        //   cim = 1.5 * v          (v ∈ [-1,1) → cim ∈ [-1.5, 1.5))
        for (std::size_t i = 0; i < n; ++i) {
            cre[i] = -0.5 + 1.5 * cre[i];
            cim[i] = 1.5 * cim[i];
        }
    }

    void TearDown(const ::benchmark::State& st) override {
        if (!output.empty()) {
            do_not_optimize_array(output.data(), output.size());
        }
        (void)st;
    }
};

// uniform benchmark body
static void run_mandelbrot_uniform(BranchFixture* self, ::benchmark::State& state) {
    if (self->width <= 0) {
        state.SkipWithError("invalid mandelbrot parameters");
        return;
    }
    double* out = self->output.data();
    const double* re = self->cre.data();
    const double* im = self->cim.data();
    const int w = self->width;
    const int h = self->height;

    // 预热
    mandelbrot_uniform_fp64(out, re, im, w, h, kMandelbrotMaxIter);

    for (auto _ : state) {
        mandelbrot_uniform_fp64(out, re, im, w, h, kMandelbrotMaxIter);
        ACR_BENCH_ASM_MEMORY_BARRIER();
    }

    // 防止编译器消除
    do_not_optimize(out[0]);
    do_not_optimize_array(out, static_cast<std::size_t>(w) * h);

    // ops = width * height * max_iter * 4（每次迭代 4 ops：2 mul + 1 add + 1 compare）
    const int64_t ops_per_iter = static_cast<int64_t>(w) * h * kMandelbrotMaxIter * 4;
    state.SetItemsProcessed(ops_per_iter * state.iterations());
    state.counters["MOp/s"] = ::benchmark::Counter(
        static_cast<double>(ops_per_iter) * 1e-6,
        ::benchmark::Counter::kIsIterationInvariantRate);
}

// variable benchmark body
static void run_mandelbrot_variable(BranchFixture* self, ::benchmark::State& state) {
    if (self->width <= 0) {
        state.SkipWithError("invalid mandelbrot parameters");
        return;
    }
    double* out = self->output.data();
    const double* re = self->cre.data();
    const double* im = self->cim.data();
    const int w = self->width;
    const int h = self->height;

    // 预热
    mandelbrot_variable_fp64(out, re, im, w, h, kMandelbrotMaxIter);

    for (auto _ : state) {
        mandelbrot_variable_fp64(out, re, im, w, h, kMandelbrotMaxIter);
        ACR_BENCH_ASM_MEMORY_BARRIER();
    }

    // 防止编译器消除
    do_not_optimize(out[0]);
    do_not_optimize_array(out, static_cast<std::size_t>(w) * h);

    // ops = width * height * max_iter * 4
    // 注意：variable 实际迭代次数因像素而异，此处用 max_iter 作为 ops 估计，
    // 反映分支发散导致的吞吐损失（variable MOp/s 会低于 uniform）。
    const int64_t ops_per_iter = static_cast<int64_t>(w) * h * kMandelbrotMaxIter * 4;
    state.SetItemsProcessed(ops_per_iter * state.iterations());
    state.counters["MOp/s"] = ::benchmark::Counter(
        static_cast<double>(ops_per_iter) * 1e-6,
        ::benchmark::Counter::kIsIterationInvariantRate);
}

// ===== 注册 benchmark =====
BENCHMARK_DEFINE_F(BranchFixture, mandelbrot_uniform_fp64)(::benchmark::State& st) {
    run_mandelbrot_uniform(this, st);
}
BENCHMARK_REGISTER_F(BranchFixture, mandelbrot_uniform_fp64)
    ->Arg(256)->Arg(512)->Arg(1024)
    ->Unit(::benchmark::kMillisecond);

BENCHMARK_DEFINE_F(BranchFixture, mandelbrot_variable_fp64)(::benchmark::State& st) {
    run_mandelbrot_variable(this, st);
}
BENCHMARK_REGISTER_F(BranchFixture, mandelbrot_variable_fp64)
    ->Arg(256)->Arg(512)->Arg(1024)
    ->Unit(::benchmark::kMillisecond);

} // namespace astro::compute::qualification::bench
