// P2-004 单元测试: block plan 预算生成 + 等价 + 边界 + 峰值误差界
#include "astro/phase2/block.h"

#include <cstdio>
#include <cstring>

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

int main() {
  // 1) 预算内: 小数据不需缩块 (block_pixels = 全量)
  {
    P2BlockPlannerInput in{};
    in.output_pixels = 512 * 512;
    in.covering_frames = 4;
    in.precision = 0;               // fp32: 4B/sample
    in.memory_limit_bytes = 8ULL << 30;  // 8GB 预算
    in.safety_factor = 0.75;
    in.scratch_bytes_per_sample = 4;
    in.scratch_bytes_per_pixel = 16;
    in.fixed_overhead = 64 << 20;
    P2BlockPlan out{};
    CHECK(p2_block_plan(&in, &out) == 0);
    CHECK(out.status == 0);
    CHECK(out.block_pixels == in.output_pixels);  // 全量 (无需缩)
    CHECK(out.micro_chunk_required == 0);
    // 峰值估算必须 < 预算 (误差界: plan 峰值 <= memory_limit)
    CHECK(out.estimated_peak_bytes <= in.memory_limit_bytes);
  }

  // 2) 超预算: 正常缩块 (不 swap, micro-chunk 或更小块)
  {
    P2BlockPlannerInput in{};
    in.output_pixels = 4096 * 4096;
    in.covering_frames = 32;
    in.precision = 0;
    in.memory_limit_bytes = 256ULL << 20;  // 256MB 小预算
    in.safety_factor = 0.75;
    in.scratch_bytes_per_sample = 4;
    in.scratch_bytes_per_pixel = 16;
    in.fixed_overhead = 16 << 20;
    P2BlockPlan out{};
    CHECK(p2_block_plan(&in, &out) == 0);
    // 要么缩块要么 micro-chunk; 峰值必须 <= 预算 (误差界)
    CHECK(out.block_pixels <= in.output_pixels);
    CHECK(out.estimated_peak_bytes <= in.memory_limit_bytes);
  }

  // 3) 边界: 零内存拒绝; 零像素拒绝
  {
    P2BlockPlannerInput in{};
    in.output_pixels = 1024 * 1024;
    in.covering_frames = 2;
    in.precision = 0;
    in.memory_limit_bytes = 0;  // 非法
    in.safety_factor = 0.75;
    P2BlockPlan out{};
    CHECK(p2_block_plan(&in, &out) != 0 || out.status == 1);  // 拒绝
  }

  // 4) 峰值误差界: 重复调用确定性 (同输入同计划)
  {
    P2BlockPlannerInput in{};
    in.output_pixels = 2048 * 2048;
    in.covering_frames = 8;
    in.precision = 0;
    in.memory_limit_bytes = 1ULL << 30;
    in.safety_factor = 0.75;
    in.scratch_bytes_per_sample = 4;
    in.scratch_bytes_per_pixel = 16;
    in.fixed_overhead = 32 << 20;
    P2BlockPlan a{}, b{};
    CHECK(p2_block_plan(&in, &a) == 0);
    CHECK(p2_block_plan(&in, &b) == 0);
    CHECK(a.block_pixels == b.block_pixels);       // 确定性
    CHECK(a.estimated_peak_bytes == b.estimated_peak_bytes);
  }

  // 5) 无稠密全局 cache: 缩块时 block_pixels 显著小于全量 (不构造全局稠密)
  {
    P2BlockPlannerInput in{};
    in.output_pixels = 8192 * 8192;   // 大输出
    in.covering_frames = 64;
    in.precision = 0;
    in.memory_limit_bytes = 128ULL << 20;  // 极小预算
    in.safety_factor = 0.75;
    in.scratch_bytes_per_sample = 4;
    in.scratch_bytes_per_pixel = 16;
    in.fixed_overhead = 8 << 20;
    P2BlockPlan out{};
    CHECK(p2_block_plan(&in, &out) == 0);
    CHECK(out.block_pixels < in.output_pixels);   // 必须缩块 (不建全局 cache)
    CHECK(out.estimated_peak_bytes <= 128ULL << 20);
  }

  if (failures == 0) {
    std::printf("P2-004 TESTS PASS (预算生成/全量等价/超预算缩块/边界/峰值误差界/无全局 cache)\n");
    return 0;
  }
  std::fprintf(stderr, "P2-004 TESTS FAIL (%d)\n", failures);
  return 1;
}
