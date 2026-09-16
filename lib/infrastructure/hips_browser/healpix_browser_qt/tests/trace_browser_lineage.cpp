// ============================================================================
// trace_browser_lineage.cpp - G4 Browser lineage (同一组 sky samples)
// 输入: <hips_root> <sky_samples.tsv> (ra dec 每行)
// 输出: browser_lineage.jsonl 每样本 {ra,dec,aio_signal,aio_support,
// browser_signal,browser_support,match}
// AIO direct 与 BrowserBackend 使用完全相同的 ra/dec 查询点。
// ============================================================================
#include "hips_browser_backend.h"
#include "aio_hips_reader.h"
#include "healpix/healpix_core.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: trace_browser_lineage <hips_root> <sky_samples.tsv>\n");
        return 2;
    }
    const std::string root = argv[1];
    std::ifstream in(argv[2]);
    if (!in.is_open()) { std::fprintf(stderr, "cannot open %s\n", argv[2]); return 2; }
    std::vector<std::pair<double,double>> samples;
    std::string line;
    while (std::getline(in, line)) {
        double ra = 0, dec = 0;
        if (std::sscanf(line.c_str(), "%lf %lf", &ra, &dec) == 2)
            samples.push_back({ra, dec});
    }
    std::fprintf(stderr, "samples=%zu root=%s\n", samples.size(), root.c_str());

    HipsBrowserBackend bk;
    if (bk.open_product(root) != 0) { std::fprintf(stderr, "open_product FAIL\n"); return 3; }
    AioHipsDataset* dsig = aio_hips_open(root.c_str(), AIO_HIPS_RD_SIGNAL);
    AioHipsDataset* dsup = aio_hips_open(root.c_str(), AIO_HIPS_RD_SUPPORT);
    if (!dsig || !dsup) { std::fprintf(stderr, "direct AIO open FAIL\n"); return 4; }
    const uint32_t nside = uint32_t(1) << (uint32_t)bk.get_leaf_order();
    const bool fp64 = bk.is_fp64();
    const int kDim = 512;
    const uint64_t kMask = (1ULL << 18) - 1;
    std::vector<float> tmp((size_t)kDim * kDim);
    std::vector<double> t64((size_t)kDim * kDim);
    long long mismatch = 0, queried = 0, outside = 0;

    FILE* out = std::fopen("browser_lineage.jsonl", "wb");
    if (!out) { std::fprintf(stderr, "cannot open output\n"); return 5; }
    for (const auto& s : samples) {
        const double ra = s.first, dec = s.second;
        const uint64_t leaf = astrocs::healpix::ang2pix_nest(nside, ra, dec);
        const uint64_t tile = leaf >> 18;
        const uint64_t z = leaf & kMask;
        // 共享标准映射 (AIO tile 为 standard HiPS row-major)
        const uint64_t idx = astrocs::healpix::nested_local_to_fits_index(z, 9u, 512u);
        double sig_d = NAN, sup_d = 0.0;
        int rc_sig = 0, rc_sup = 0;
        if (fp64) {
            rc_sig = aio_hips_read_tile_f64(dsig, tile, t64.data());
            if (rc_sig == 0) sig_d = t64[idx];
            rc_sup = aio_hips_read_tile_f64(dsup, tile, t64.data());
            if (rc_sup == 0) sup_d = t64[idx];
        } else {
            rc_sig = aio_hips_read_tile_f32(dsig, tile, tmp.data());
            if (rc_sig == 0) sig_d = (double)tmp[idx];
            rc_sup = aio_hips_read_tile_f32(dsup, tile, tmp.data());
            if (rc_sup == 0) sup_d = (double)tmp[idx];
        }
        double sig_b = NAN, sup_b = 0.0;
        int rc_b = bk.query_pixel(ra, dec, sig_b, sup_b);
        ++queried;
        if (rc_sig != 0 || rc_b != 0) {
            ++outside;
            std::fprintf(out,
                "{\"ra\":%.10f,\"dec\":%.10f,\"aio_signal\":null,\"aio_support\":null,"
                "\"browser_signal\":null,\"browser_support\":null,\"match\":false}\n",
                ra, dec);
            continue;
        }
        const bool b_ok = std::isfinite(sig_b);
        const bool d_ok = std::isfinite(sig_d);
        const bool match = (b_ok == d_ok) &&
            (!b_ok || std::fabs(sig_b - sig_d) <= 1e-6 * std::max(1.0, std::fabs(sig_d))) &&
            (std::fabs(sup_b - sup_d) <= 1e-9 * std::max(1.0, std::fabs(sup_d)));
        if (!match) ++mismatch;
        std::fprintf(out,
            "{\"ra\":%.10f,\"dec\":%.10f,\"aio_signal\":%.17g,\"aio_support\":%.17g,"
            "\"browser_signal\":%.17g,\"browser_support\":%.17g,\"match\":%s}\n",
            ra, dec, sig_d, sup_d, sig_b, sup_b, match ? "true" : "false");
    }
    std::fclose(out);
    std::fprintf(stderr, "RESULT: samples=%lld mismatch=%lld outside=%lld\n",
                 (long long)queried, (long long)mismatch, (long long)outside);
    aio_hips_close(dsig); aio_hips_close(dsup); bk.close();
    return (mismatch == 0 && queried > 0) ? 0 : 1;
}
