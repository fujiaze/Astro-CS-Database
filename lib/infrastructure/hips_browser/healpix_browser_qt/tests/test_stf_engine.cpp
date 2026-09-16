// test_stf_engine.cpp - STFEngine 单元测试
// 模块：healpix_browser_qt / tests
// 用途：验证 MTF 公式、预设、MAD 自动拉伸、GPU uniform 转换
// 编译：make tests/test_stf_engine.exe

#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
#include "stf_engine.h"

// 浮点近似比较
static bool approx(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) < eps;
}

// 测试 1：MTF 公式
// 验证三个不动点：MTF(0)=0, MTF(1)=1, MTF(m,m)=0.5
void test_mtf() {
    assert(approx(STFEngine::mtf(0.0f, 0.5f), 0.0f));
    assert(approx(STFEngine::mtf(1.0f, 0.5f), 1.0f));
    assert(approx(STFEngine::mtf(0.25f, 0.25f), 0.5f));
    assert(approx(STFEngine::mtf(0.5f, 0.5f), 0.5f));
    printf("[PASS] test_mtf\n");
}

// 测试 2：预设查询（新接口: 接收 data_min/data_max，返回原始像素值）
void test_presets() {
    const float dmin = 10.0f, dmax = 200.0f;
    auto linear = STFEngine::get_preset("linear", dmin, dmax);
    assert(approx(linear.shadows, dmin) && approx(linear.highlights, dmax));
    assert(approx(linear.midtones, 0.5f) && approx(linear.compression, 0.0f));
    auto sqrt_p = STFEngine::get_preset("sqrt", dmin, dmax);
    assert(approx(sqrt_p.shadows, dmin) && approx(sqrt_p.highlights, dmax));
    assert(approx(sqrt_p.midtones, 0.25f) && approx(sqrt_p.compression, 0.0f));
    auto asinh = STFEngine::get_preset("asinh", dmin, dmax);
    assert(approx(asinh.shadows, dmin) && approx(asinh.highlights, dmax));
    assert(approx(asinh.midtones, 0.25f) && approx(asinh.compression, 0.5f));
    auto log_p = STFEngine::get_preset("log", dmin, dmax);
    assert(approx(log_p.shadows, dmin) && approx(log_p.highlights, dmax));
    assert(approx(log_p.midtones, 0.15f) && approx(log_p.compression, 0.8f));
    printf("[PASS] test_presets\n");
}

// 测试 3：MAD 自动拉伸
// 构造均值约 50、有离散的合成数据，验证 shadows<median、highlights>median
void test_auto_stretch() {
    std::vector<float> data;
    for (int i = 0; i < 1000; ++i) {
        float x = 50.0f + 10.0f * (float)(i % 100 - 50) / 50.0f;
        data.push_back(x);
    }
    auto params = STFEngine::auto_stretch(data.data(), data.size(), 0.0f);
    assert(params.shadows < 50.0f);
    assert(params.highlights > 50.0f);
    assert(params.validate());
    printf("[PASS] test_auto_stretch (shadows=%.2f highlights=%.2f midtones=%.3f)\n",
           params.shadows, params.highlights, params.midtones);
}

// 测试 4：GPU uniform 转换
// 验证原始像素值 shadows/highlights 能正确归一化到 [0,1]
void test_to_uniforms() {
    STFParams params;
    params.shadows = 10.0f;
    params.highlights = 100.0f;
    params.midtones = 0.5f;
    params.compression = 0.0f;
    auto u = STFEngine::to_uniforms(params, 0.0f, 100.0f, 0.0f);
    assert(approx(u.shadows, 0.1f));
    assert(approx(u.highlights, 1.0f));
    printf("[PASS] test_to_uniforms\n");
}

int main() {
    test_mtf();
    test_presets();
    test_auto_stretch();
    test_to_uniforms();
    printf("\n=== test_stf_engine: ALL PASS ===\n");
    return 0;
}
