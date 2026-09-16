// tests/backend/p3_resample_probe_main.cpp — P3-003 重采样探针
// 用法:
//   probe order <max_order> <scale_deg_px>          → "OK <order>" | "FAIL <code>"
//   probe mode <input_mode>                          → "OK" | "FAIL <code>"
//   probe open <signal_dir>                          → "OK" | "FAIL"
//   probe nearest <dir> <ra> <dec>                   → "OK <value> <coverage>" | "FAIL"
//   probe bilinear <dir> <ra> <dec>                  → 同上
//   probe pix2ang <dir> <ipix>                       → "OK <ra> <dec>"(Oracle 用中心坐标)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "aio_hips_reader.h"
#include "healpix_core.h"
#include "p3_resample.h"

using namespace astrocs::phase3;

int main(int argc, char** argv) {
    if (argc < 3) return 2;
    const std::string mode = argv[1];
    if (mode == "order") {
        int o = -1;
        const P3ResampleStatus st = p3_order_select(atoi(argv[2]), atof(argv[3]), &o);
        if (st != P3_RS_OK) { std::printf("FAIL %d\n", (int)st); return 1; }
        std::printf("OK %d\n", o);
        return 0;
    }
    if (mode == "mode") {
        const P3ResampleStatus st = p3_resample_check_mode(argv[2]);
        if (st != P3_RS_OK) { std::printf("FAIL %d\n", (int)st); return 1; }
        std::printf("OK\n");
        return 0;
    }
    if (mode == "pix2ang") {
        // tile ipix(order 0) 中心: nside=1 不被 healpix_core 支持 → 用 nside=4 的
        // 首子像素中心(ipix<<4, 偏移 < 基元尺度 1/16, 处于本 tile 内部 → Oracle 成立)
        const uint64_t sub = (strtoull(argv[2], nullptr, 10) << 4);
        double ra = 0, dec = 0;
        astrocs::healpix::pix2ang_nest(4, sub, ra, dec);
        std::printf("OK %.12f %.12f\n", ra, dec);
        return 0;
    }
    if (mode == "open" || mode == "nearest" || mode == "bilinear") {
        P3Sampler s{};
        std::string err;
        const P3ResampleStatus st = p3_sampler_open(argv[2], &s, &err);
        if (st != P3_RS_OK) { std::printf("FAIL %d %s\n", (int)st, err.c_str()); return 1; }
        if (mode == "open") { std::printf("OK\n"); return 0; }
        const bool bil = (mode == "bilinear");
        float v = 0; int cov = 0;
        const P3ResampleStatus r = bil ? p3_sample_bilinear(&s, atof(argv[3]), atof(argv[4]), &v, &cov)
                                       : p3_sample_nearest(&s, atof(argv[3]), atof(argv[4]), &v, &cov);
        if (r != P3_RS_OK) { std::printf("FAIL %d\n", (int)r); return 1; }
                if (cov == 0) {
            // 诊断: 打印样本 leaf/tip 与 reader 错误
            const uint32_t ns = 512;
            const uint64_t leaf = astrocs::healpix::ang2pix_nest(ns, atof(argv[3]), atof(argv[4]));
            std::printf("dbg leaf=%llu tip=%llu rderr=%s\n",
                        (unsigned long long)leaf, (unsigned long long)(leaf >> 18),
                        aio_hips_reader_last_error());
        }  // sampler 内部错误见 impl
        std::printf("OK %.8f %d\n", v, cov);
        return 0;
    }
    return 2;
}
