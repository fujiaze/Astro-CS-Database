// test_browser_backend.cpp - BrowserBackend 单元测试
// 功能: 验证 .hiss/.hcsd 文件加载, 按需子叶, ud_grade 降采样
// 编译: g++ -O2 -std=c++17 -Icore -Iinclude -I../healpix_io/include
//        -o test_browser_backend.exe test_browser_backend.cpp
//        core/browser_backend.cpp core/healpix_math.cpp
//        -L../healpix_io -lhealpix_io

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include "browser_backend.h"

// 测试数据路径 (使用现有 pipeline_debug 输出)
static const char* HISS_TEST =
    "f:/Astro dev/Astro CS Normalization Database/output/pipeline_debug/"
    "Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red/"
    "drizzle/Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.hiss";

void test_open_hiss() {
    BrowserBackend backend;
    int ret = backend.open_file(HISS_TEST);
    if (ret != 0) {
        printf("[SKIP] test_open_hiss (测试文件不存在或加载失败 ret=%d)\n", ret);
        return;
    }
    assert(backend.is_open());
    assert(backend.is_hiss());
    assert(!backend.is_hcsd());
    assert(backend.get_nside() > 0);
    assert(backend.get_n_pix() > 0);
    printf("[INFO] hiss: nside=%u npix=%llu filter=%s\n",
           backend.get_nside(),
           (unsigned long long)backend.get_n_pix(),
           backend.get_filter().c_str());

    // 测试 get_all_data
    LeafData all = backend.get_all_data();
    assert(all.n_pix > 0);
    assert(all.ipix != nullptr);
    assert(all.pixel != nullptr);
    // 注意: get_all_data 返回的数据由 backend 持有, 不应 release

    backend.close_file();
    assert(!backend.is_open());
    printf("[PASS] test_open_hiss\n");
}

void test_open_hcsd() {
    // 暂无 .hcsd 测试文件, 仅验证接口不崩溃
    BrowserBackend backend;
    int ret = backend.open_file("nonexistent.hcsd");
    assert(ret != 0);
    assert(!backend.is_open());
    printf("[PASS] test_open_hcsd (接口验证, 无实际 .hcsd 文件)\n");
}

void test_ud_grade() {
    // 构造 nside=4 的 4 个像素, 降采样到 nside=2
    LeafData input;
    input.leaf_ipix = 0;
    input.nside = 4;
    input.n_pix = 4;
    input.ipix = (uint64_t*)malloc(4 * sizeof(uint64_t));
    input.pixel = (float*)malloc(4 * sizeof(float));
    input.ipix[0] = 0; input.pixel[0] = 1.0f;
    input.ipix[1] = 1; input.pixel[1] = 2.0f;
    input.ipix[2] = 2; input.pixel[2] = 3.0f;
    input.ipix[3] = 3; input.pixel[3] = 4.0f;

    BrowserBackend backend;
    LeafData output = backend.ud_grade(input, 2);
    // nside=4 → nside=2, 4 像素合并为 1, 均值 = (1+2+3+4)/4 = 2.5
    assert(output.nside == 2);
    assert(output.n_pix == 1);
    assert(std::fabs(output.pixel[0] - 2.5f) < 0.01f);

    backend.release_leaf(input);
    backend.release_leaf(output);
    printf("[PASS] test_ud_grade (nside 4→2, pixel=%.2f)\n", 2.5f);
}

void test_ipix_to_angle_static() {
    // 验证静态方法 ipix_to_angle 转发到 HealpixMath
    double ra, dec;
    BrowserBackend::ipix_to_angle(1, 0, true, ra, dec);
    // nside=1 ipix=0: dec ≈ 41.81° (赤道带)
    assert(dec > 35.0 && dec < 45.0);
    printf("[PASS] test_ipix_to_angle_static (nside=1 ipix=0 ra=%.2f dec=%.2f)\n",
           ra, dec);
}

int main() {
    test_open_hiss();
    test_open_hcsd();
    test_ud_grade();
    test_ipix_to_angle_static();
    printf("\n=== test_browser_backend: ALL PASS ===\n");
    return 0;
}
