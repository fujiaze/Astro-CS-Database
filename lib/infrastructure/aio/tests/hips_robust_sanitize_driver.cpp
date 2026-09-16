// hips_robust_sanitize_driver.cpp - ASan/UBSan/LSan 健壮性驱动
//
// 覆盖: HiPS writer(FP32/FP64) + VOTable metadata.xml 写入完整性,
// corrupt TSV rows / corrupt properties / corrupt Moc.fits 输入,
// 读回 catalog 与 tile 的异常路径。所有失败均须优雅返回而非崩溃。
//
// 用法: <exe> <out_dir> [dtype: 0=float32 1=float64]
#include "aio_hips.h"
#include "aio_hips_reader.h"

#include <sys/stat.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int corrupt_snr_tsv(const std::string& out) {
    // 在全部 SNR TSV tile 中注入损坏行 + 替换首行为畸形行
    const std::string snr = out + "/snr";
    int files = 0, bad_lines = 0;
    for (const auto& e : fs::recursive_directory_iterator(snr)) {
        if (!e.is_regular_file()) continue;
        const std::string p = e.path().string();
        if (p.size() < 4 || p.substr(p.size() - 4) != ".tsv") continue;
        if (std::getenv("ASTROCS_KEEP_ORIG")) {
            std::ifstream cp(p, std::ios::binary);
            std::ofstream op(p + ".orig", std::ios::binary | std::ios::trunc);
            op << cp.rdbuf();
        }
        std::ifstream in(p, std::ios::binary);
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(in, line)) lines.push_back(line);
        in.close();
        std::ofstream f(p, std::ios::binary | std::ios::trunc);
        if (!f) { std::fprintf(stderr, "corrupt open fail: %s\n", p.c_str()); return -1; }
        for (size_t i = 0; i < lines.size(); ++i) {
            if (i == 0) {
                // 首行(数据行)替换为多种畸形: 缺列/非数字/超大指数/超长字段
                f << "1.0 2.0 3.0\n";
                f << "abc def ghi jkl mno pqr\n";
                f << "1001 0.0 90.0 notanumber 999999999 4294967295\n";
                f << std::string(1 << 14, 'X') << "\n";
                bad_lines += 4;
            } else {
                f << lines[i] << "\n";
            }
        }
        f.close();
        ++files;
    }
    std::printf("corrupt_snr_tsv files=%d injected_bad_lines=%d\n", files, bad_lines);
    return files > 0 ? 0 : -1;
}

static int corrupt_properties(const std::string& out, const std::string& sub) {
    // 删除 hips_version 使 reader 判定属性损坏; 同时写空行/垃圾键
    const std::string p = out + "/" + sub + "/properties";
    std::ifstream in(p);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        if (line.find("hips_version") == std::string::npos) lines.push_back(line);
    }
    in.close();
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    for (const auto& l : lines) f << l << "\n";
    f << "=broken-key\n\n";
    f.close();
    return 0;
}

static int corrupt_moc(const std::string& out, const std::string& sub) {
    const std::string p = out + "/" + sub + "/Moc.fits";
    std::FILE* f = std::fopen(p.c_str(), "r+b");
    if (!f) return -1;
    const char junk[128] = {0};
    std::fseek(f, 0, SEEK_SET);
    std::fwrite(junk, 1, 100, f);   // 破坏 FITS 头
    std::fclose(f);
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    const std::string out = argv[1];
    const int dtype = (argc > 2) ? std::atoi(argv[2]) : 0;
    const uint32_t nside = 2048;
    const uint32_t leaf_order = 11;
    const double pi = std::acos(-1.0);
    const double a_cell = 4.0 * pi / (12.0 * (double)nside * nside);
    const size_t n = 512 * 512;

    AioHipsProductSet* ps = aio_hips_product_begin(
        out.c_str(), nside, 512, dtype, AIO_HIPS_PRODUCT_ALL,
        "ivo://astrocs/sanitize", "Sanitizer HiPS", "L", 300.0, "2026-08-09", 0);
    if (!ps) { std::fprintf(stderr, "begin fail: %s\n", aio_hips_last_error()); return 3; }
    std::vector<float>  fluxF(n, 0.0f),  areaF(n, 0.0f);
    std::vector<double> fluxD(n, 0.0),  areaD(n, 0.0);
    for (size_t i = 0; i < n; ++i) {
        size_t x = i % 512, y = i / 512;
        if (x > 100 && x < 400 && y > 100 && y < 400) {
            fluxF[i] = 2.0f;  fluxD[i] = 2.0;
            areaF[i] = (float)(0.5 * a_cell); areaD[i] = 0.5 * a_cell;
        }
    }
    const void* fluxp = (dtype == AIO_HIPS_FLOAT32) ? (const void*)fluxF.data() : (const void*)fluxD.data();
    const void* areap = (dtype == AIO_HIPS_FLOAT32) ? (const void*)areaF.data() : (const void*)areaD.data();
    for (uint64_t tile_ipix = 0; tile_ipix < 3; ++tile_ipix) {
        AstroSphereTileView view;
        std::memset(&view, 0, sizeof(view));
        view.parent_ipix = tile_ipix;
        view.leaf_order = leaf_order;
        view.width = 512;
        view.data_type = dtype;
        view.flux_sum = fluxp;
        view.covered_area = areap;
        if (aio_hips_write_signal_support_tile(ps, &view) != 0) {
            std::fprintf(stderr, "tile fail: %s\n", aio_hips_last_error());
            aio_hips_abort(ps);
            return 4;
        }
    }
    AioHipsSnrPoint pts[2];
    pts[0] = {10.0, 89.5, 12.3, 1001, 1u, 1u};
    pts[1] = {20.0, -30.0, 5.5, 1002, 4u, 0u};
    aio_hips_write_snr_points(ps, pts, 2);
    if (aio_hips_finalize(ps) != 0) {
        std::fprintf(stderr, "finalize fail: %s\n", aio_hips_last_error());
        return 5;
    }
    // 1) metadata.xml 必须为标准 VOTable 根元素
    {
        std::ifstream f(out + "/snr/metadata.xml", std::ios::binary);
        std::string head((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (head.find("<VOTABLE") == std::string::npos) {
            std::fprintf(stderr, "metadata.xml missing VOTABLE root\n");
            return 6;
        }
        std::printf("metadata_votable_ok bytes=%zu\n", head.size());
    }
    // 1b) 损坏前 catalog 读回必须完整 (2 行, id=1001/1002)
    {
        AioHipsDataset* pre = aio_hips_open(out.c_str(), AIO_HIPS_RD_SNR);
        if (!pre) { std::fprintf(stderr, "pre-corrupt snr open fail: %s\n", aio_hips_reader_last_error()); return 7; }
        double ra[8] = {0}, dec[8] = {0}, snr[8] = {0};
        int64_t sid[8] = {0};
        uint32_t qf[8] = {0}, ps[8] = {0};
        int got = aio_hips_read_snr_catalog(pre, ra, dec, snr, sid, qf, ps, 8);
        std::printf("pre_corrupt_catalog got=%d rows:", got);
        for (int i = 0; i < got; ++i) std::printf(" [%lld %.4f %.4f snr=%.3f qf=%u ps=%u]",
                                                  (long long)sid[i], ra[i], dec[i], snr[i], qf[i], ps[i]);
        std::printf("\n");
        aio_hips_close(pre);
        if (got != 2) { std::fprintf(stderr, "pre-corrupt catalog size unexpected\n"); return 7; }
        bool have1 = false, have2 = false;
        for (int i = 0; i < got; ++i) { if (sid[i] == 1001) have1 = true; if (sid[i] == 1002) have2 = true; }
        if (!have1 || !have2) { std::fprintf(stderr, "pre-corrupt ids unexpected\n"); return 7; }
    }
    // 2) 损坏 TSV / properties / Moc 后仍须优雅失败
    if (corrupt_snr_tsv(out) != 0) { std::fprintf(stderr, "corrupt tsv setup fail\n"); return 7; }
    if (corrupt_moc(out, "signal") != 0) { std::fprintf(stderr, "corrupt moc setup fail\n"); return 8; }

    AioHipsDataset* ds = aio_hips_open(out.c_str(), AIO_HIPS_RD_SNR);
    if (!ds) { std::fprintf(stderr, "snr open fail after corrupt: %s\n", aio_hips_reader_last_error()); return 9; }
    {
        double ra[8] = {0}, dec[8] = {0}, snr[8] = {0};
        int64_t sid[8] = {0};
        uint32_t qf[8] = {0}, ps[8] = {0};
        int got = aio_hips_read_snr_catalog(ds, ra, dec, snr, sid, qf, ps, 8);
        // 损坏行被跳过, 2 个有效行仍应可读 (顺序按 MOC ipix 升序, 不假设 id 顺序)
        if (got != 2) {
            std::fprintf(stderr, "snr catalog after corrupt unexpected: got=%d\n", got);
            aio_hips_close(ds);
            return 10;
        }
        bool have1 = false, have2 = false;
        for (int i = 0; i < got; ++i) { if (sid[i] == 1001) have1 = true; if (sid[i] == 1002) have2 = true; }
        if (!have1 || !have2) {
            std::fprintf(stderr, "snr catalog after corrupt ids: got=%d sid0=%lld sid1=%lld\n",
                         got, (long long)sid[0], (long long)sid[1]);
            aio_hips_close(ds);
            return 10;
        }
        std::printf("snr_catalog_after_corrupt got=%d ids_ok\n", got);
    }
    aio_hips_close(ds);

    // corrupt properties: signal 打开应失败且不崩溃
    if (corrupt_properties(out, "signal") != 0) { std::fprintf(stderr, "corrupt props setup fail\n"); return 11; }
    {
        AioHipsDataset* d2 = aio_hips_open(out.c_str(), AIO_HIPS_RD_SIGNAL);
        if (d2) { std::fprintf(stderr, "signal open should fail after properties corrupt\n"); aio_hips_close(d2); return 12; }
        std::printf("signal_open_after_props_corrupt gracefully_null\n");
    }
    // corrupt Moc: signal 打开成功但 0 tiles (优雅降级)
    {
        AioHipsDataset* d3 = aio_hips_open(out.c_str(), AIO_HIPS_RD_SIGNAL);
        if (!d3) {
            std::printf("signal_open_after_moc_corrupt graceful_open_fail\n");
        } else {
            int tc = aio_hips_tile_count(d3);
            std::printf("signal_open_after_moc_corrupt tiles=%d\n", tc);
            aio_hips_close(d3);
        }
    }
    std::printf("HIPS_ROBUST_SANITIZE_OK dtype=%d\n", dtype);
    return 0;
}
