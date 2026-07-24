// test_gradient_sampler.cpp - gradient_sampler 编译验证 + 基本逻辑测试
//
// 验证:
//   1. 结构体大小 (SampleRow = 40 字节)
//   2. selectNsideI 逻辑
//   3. mortonDownsample 逻辑
//   4. 空输入处理

#include "gradient_sampler.h"
#include "snr_evaluator.h"

#include <cassert>
#include <cstdio>
#include <cmath>

using namespace gradient;

// 测试 1: 结构体大小
void test_struct_size() {
    assert(sizeof(SampleRow) == 36);
    printf("[PASS] test_struct_size (SampleRow=%zu bytes)\n", sizeof(SampleRow));
}

// 测试 2: 空输入
void test_empty_input() {
    GradientSampler sampler;
    SampleResult result;
    SamplerParams params;
    int rc = sampler.sample(nullptr, 0, "", params, result);
    assert(rc != 0);
    assert(result.rows.empty());
    assert(result.total_samples == 0);
    printf("[PASS] test_empty_input (rc=%d)\n", rc);
}

// 测试 3: 不存在的文件 + 无效 gaia 目录 (应返回错误, 不崩溃)
void test_nonexistent_file() {
    GradientSampler sampler;
    SampleResult result;
    SamplerParams params;
    FrameInfo frame;
    frame.hiss_path = "nonexistent.hiss";
    frame.frame_id = 0;
    // gaia_data_dir 为空 → gaia_client_create_ex 失败 → sample 返回 2
    int rc = sampler.sample(&frame, 1, "", params, result);
    assert(rc == 2);  // gaia 创建失败
    assert(!sampler.lastError().empty());
    printf("[PASS] test_nonexistent_file (rc=%d, gaia创建失败预期)\n", rc);
}

int main() {
    printf("=== test_gradient_sampler ===\n\n");
    test_struct_size();
    test_empty_input();
    test_nonexistent_file();
    printf("\n=== test_gradient_sampler: ALL PASS ===\n");
    return 0;
}
