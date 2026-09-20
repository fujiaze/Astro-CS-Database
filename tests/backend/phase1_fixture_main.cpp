// tests/backend/phase1_fixture_main.cpp — Phase1 合成 FITS fixture + 输出校验 (CLI-004)
// 用法:
//   phase1_fixture --make <dir>          写 bias/dark/flat/light_1/light_2 (64x64, 常量域)
//   phase1_fixture --make-bad-flat <dir> 写退化 master flat (全零/负中位数/非有限) 负例夹具
//   phase1_fixture --mean <fits>         读回并打印 "MEAN <value>"
// 已知值: bias=100, dark=150, flat=1.25, light=200 → 校准输出 = (200-100-1*(150-100))/1.25 = 40
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "astro_image_io.h"
#include "fitsio.h"

namespace {

constexpr int W = 64, H = 64;
constexpr float V_BIAS = 100.0f, V_DARK = 150.0f, V_FLAT = 1.25f, V_LIGHT = 200.0f;
constexpr double V_EXPTIME = 1.0;   // 全帧同曝光 → K = t_light/t_dark = 1

// BIAS-001 / B2-A13（cal 节点）: 只要 master_dark 在位，K=t_light/t_dark 就必须由
// FITS 头 EXPTIME 推导；真实相机帧恒带 EXPTIME，而 aio_write_fits 只写几何头
// （lib/infrastructure/aio/src/aio_fits.cpp 无任何 fits_write_key —— 元数据落盘缺口
// 已另行登记）。夹具必须与真实帧同构，故写盘后补写 EXPTIME。
bool add_exptime_key(const std::string& path) {
    fitsfile* f = nullptr;
    int st = 0;
    if (fits_open_file(&f, path.c_str(), READWRITE, &st) != 0) return false;
    double v = V_EXPTIME;
    fits_update_key(f, TDOUBLE, const_cast<char*>("EXPTIME"), &v,
                    const_cast<char*>("Exposure time (s)"), &st);
    fits_close_file(f, &st);
    return st == 0;
}

}  // namespace

#include "aio_fits.h"   // 完整 AIOImageData(fixture 与 CLI 同构)

int main(int argc, char** argv) {
    // aio 不透明结构无直接分配导出 — 用 fitsio 裸头写入太重; 改为经 aio_read 循环?
    // 实际可行路径: aio_write_fits 要求完整结构, 而 AIOImageData 定义在 src/aio_fits.h。
    // fixture 与 CLI 同构: 本程序编进时 include src/aio_fits.h 直接构造完整结构。
    if (argc < 3) {
        std::fprintf(stderr,
                     "usage: --make <dir> | --make-noisy <dir> | --make-bad-flat <dir> |"
                     " --mean <fits>\n");
        return 2;
    }
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
            if (!add_exptime_key(p)) {
                std::fprintf(stderr, "EXPTIME key write failed: %s\n", p.c_str());
                std::free(im.data);
                return 5;
            }
            std::free(im.data);
        }
        std::printf("FIXTURES_OK\n");
        return 0;
    }
    // ── --make-noisy: 带确定性噪声的 light 帧（bias/dark/flat 仍为常量域）────────
    // 为什么需要（ASTROCS_DESIGN §2.1：HiPS 里**存**帧级 SNR；DATA-UNC-001 §30.1：
    // 缺逐帧 ivar 时**禁止静默回退等权**）：mosaic 的默认（唯一）生产权重 = 逐帧
    // 逆方差，其 ivar 子产品来自 Phase1 drizzle 的方差传播，而方差传播只在噪声模型
    // 有合格 patch（σ>0）时成立。--make 的常量域帧 σ=0 ⇒ 噪声模型整帧退化
    // （n_qualified_patches=0，科学上正确）⇒ Phase1 产品无 variance/ivar、无帧级
    // SNR ⇒ Phase2 默认链按 §30.1 fail-closed（rc=2）。故端到端正例需要非退化噪声面。
    // 数值约定：light = V_LIGHT + n，n ~ U[-4,4] ADU（确定性哈希，无 RNG 状态）；
    // 校准后 (light−bias−(dark−bias))/flat = 40 + n/1.25，均值仍为 40。
    // --make 逐字节不变（其校准数值 oracle = 40 由其它用例断言）。
    if (mode == "--make-noisy") {
        const std::string& dir = arg;
        struct { const char* name; float v; unsigned seed; } items[] = {
            {"bias.fits", V_BIAS, 0u}, {"dark.fits", V_DARK, 0u},
            {"flat.fits", V_FLAT, 0u}, {"light_1.fits", V_LIGHT, 1u},
            {"light_2.fits", V_LIGHT, 2u}};
        for (const auto& it : items) {
            AIOImageData im{};
            std::memset(&im, 0, sizeof(im));
            im.width = W;
            im.height = H;
            im.channels = 1;
            im.bits_per_sample = -32;
            im.float_sample = 1;
            im.dtype = 0;
            std::strncpy(im.source_format, "fits", sizeof(im.source_format) - 1);
            im.metadata.calibration.exptime = 1.0;
            im.metadata.calibration.frame_type[0] = it.seed ? 'L' : 'B';
            im.data = static_cast<float*>(std::malloc(sizeof(float) * W * H));
            if (!im.data) return 3;
            for (int i = 0; i < W * H; ++i) {
                float n = 0.0f;
                if (it.seed) {
                    unsigned h = static_cast<unsigned>(i) * 2654435761u + 12345u +
                                 it.seed * 7919u;
                    h ^= h >> 13; h *= 2246822519u; h ^= h >> 17;
                    n = (static_cast<float>(h % 4001u) - 2000.0f) / 500.0f;  // U[-4,4] ADU
                }
                im.data[i] = it.v + n;
            }
            const std::string p = dir + "/" + it.name;
            if (aio_write_fits(&im, p.c_str()) != 0) {
                std::fprintf(stderr, "write failed: %s\n", p.c_str());
                std::free(im.data);
                return 4;
            }
            if (!add_exptime_key(p)) {
                std::fprintf(stderr, "EXPTIME key write failed: %s\n", p.c_str());
                std::free(im.data);
                return 5;
            }
            std::free(im.data);
        }
        std::printf("FIXTURES_OK\n");
        return 0;
    }
    // B2-A6: 退化 master flat 负例夹具 —— 消费边界 fail-closed 的输入
    // (全零 / 负中位数 / 非有限), 均不得被 calibrate 静默按 0.1 除。
    if (mode == "--make-bad-flat") {
        const std::string& dir = arg;
        struct { const char* name; float v; int nan_idx; } items[] = {
            {"flat_zero.fits", 0.0f, -1},
            {"flat_neg.fits", -1.0f, -1},
            {"flat_nan.fits", 1.25f, 7}};
        for (const auto& it : items) {
            AIOImageData im{};
            std::memset(&im, 0, sizeof(im));
            im.width = W;
            im.height = H;
            im.channels = 1;
            im.bits_per_sample = -32;
            im.float_sample = 1;
            im.dtype = 0;
            std::strncpy(im.source_format, "fits", sizeof(im.source_format) - 1);
            im.metadata.calibration.exptime = 1.0;
            im.metadata.calibration.frame_type[0] = 'F';
            im.data = static_cast<float*>(std::malloc(sizeof(float) * W * H));
            if (!im.data) return 3;
            for (int i = 0; i < W * H; ++i) im.data[i] = it.v;
            if (it.nan_idx >= 0) im.data[it.nan_idx] = NAN;
            const std::string p = dir + "/" + it.name;
            if (aio_write_fits(&im, p.c_str()) != 0) {
                std::fprintf(stderr, "write failed: %s\n", p.c_str());
                std::free(im.data);
                return 4;
            }
            std::free(im.data);
        }
        std::printf("BAD_FLATS_OK\n");
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
