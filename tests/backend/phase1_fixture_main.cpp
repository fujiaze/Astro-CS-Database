// tests/backend/phase1_fixture_main.cpp — Phase1 合成 FITS fixture + 输出校验 (CLI-004)
// 用法:
//   phase1_fixture --make <dir>          写 bias/dark/flat/light_1/light_2 (64x64, 常量域)
//   phase1_fixture --mean <fits>         读回并打印 "MEAN <value>"
// 已知值: bias=100, dark=150, flat=1.25, light=200 → 校准输出 = (200-100-1*(150-100))/1.25 = 40
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "astro_image_io.h"

namespace {

constexpr int W = 64, H = 64;
constexpr float V_BIAS = 100.0f, V_DARK = 150.0f, V_FLAT = 1.25f, V_LIGHT = 200.0f;

}  // namespace

#include "aio_fits.h"   // 完整 AIOImageData(fixture 与 CLI 同构)

int main(int argc, char** argv) {
    // aio 不透明结构无直接分配导出 — 用 fitsio 裸头写入太重; 改为经 aio_read 循环?
    // 实际可行路径: aio_write_fits 要求完整结构, 而 AIOImageData 定义在 src/aio_fits.h。
    // fixture 与 CLI 同构: 本程序编进时 include src/aio_fits.h 直接构造完整结构。
if (argc < 3) { std::fprintf(stderr, "usage: --make <dir> | --mean <fits>\n"); return 2; }
    const std::string mode = argv[1], arg = argv[2];
    if (mode == "--make") {
        const std::string& dir = arg;
        struct { const char* name; float v; } items[] = {
            {"bias.fits", V_BIAS}, {"dark.fits", V_DARK},
            {"flat.fits", V_FLAT}, {"light_1.fits", V_LIGHT}, {"light_2.fits", V_LIGHT}};
        for (const auto& it : items) {
            AIOImageData im{};
            std::memset(&im, 0, sizeof(im));
            im.width = W;
            im.height = H;
            im.channels = 1;
            im.bits_per_sample = -32;   // FP32
            im.float_sample = 1;
            im.dtype = 0;   // FP32
            std::strncpy(im.source_format, "fits", sizeof(im.source_format) - 1);
            im.metadata.calibration.exptime = 1.0;
            im.metadata.calibration.frame_type[0] = 'L';
            im.data = static_cast<float*>(std::malloc(sizeof(float) * W * H));
            if (!im.data) return 3;
            for (int i = 0; i < W * H; ++i) im.data[i] = it.v;
            // FRAMETYPE 声明(校准元数据; io 侧只要求 geometry+data)
            const std::string p = dir + "/" + it.name;
            if (aio_write_fits(&im, p.c_str()) != 0) {
                std::fprintf(stderr, "write failed: %s\n", p.c_str());
                std::free(im.data);
                return 4;
            }
            std::free(im.data);
        }
        std::printf("FIXTURES_OK\n");
        return 0;
    }
    if (mode == "--mean") {
        AIOImageData* im = aio_read_fits(arg.c_str());
        if (!im) { std::fprintf(stderr, "read failed\n"); return 4; }
        float* px = aio_get_pixel_data(im);
        double acc = 0;
        const int n = W * H;   // fixture 恒 64x64
        for (int i = 0; i < n; ++i) acc += px[i];
        std::free(im);   // aio_free=pipeline TU; 释放合同=std::free
        std::printf("MEAN %.6f\n", acc / n);
        return 0;
    }
    return 2;
}
