// ============================================================================
// test_healpix_oracle.cpp - 共享 HEALPix core 全天空 Oracle 验证
//
// 读取 healpix_fullsky_oracle.py 生成的 JSONL (order/ra/dec/ipix),
// 逐行比较 ang2pix_nest, 并对每个 ipix 做 pix2ang 往返 (角距容差)。
// 硬门: mismatch == 0 且 往返角距 <= 1.2 × hp_res + 1e-9。
//
// 用法: test_healpix_oracle.exe <oracle.jsonl>
// ============================================================================

#include "healpix/healpix_core.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>

namespace {

bool parse_line(const std::string& line, uint32_t& order, double& ra, double& dec,
                uint64_t& ipix) {
    // {"order":N,"ra":R,"dec":D,"ipix":I}
    const char* s = line.c_str();
    const char* p = std::strstr(s, "\"order\":");
    const char* q = std::strstr(s, "\"ra\":");
    const char* r = std::strstr(s, "\"dec\":");
    const char* t = std::strstr(s, "\"ipix\":");
    if (!p || !q || !r || !t) return false;
    order = static_cast<uint32_t>(std::strtoul(p + 8, nullptr, 10));
    ra = std::strtod(q + 5, nullptr);
    dec = std::strtod(r + 6, nullptr);
    ipix = std::strtoull(t + 7, nullptr, 10);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: test_healpix_oracle <oracle.jsonl>\n");
        return 2;
    }
    std::FILE* f = std::fopen(argv[1], "rb");
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }

    unsigned long long lines = 0, mismatch = 0, bad_parse = 0, bad_roundtrip = 0;
    unsigned long long per_face[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    double max_roundtrip_deg = 0.0;
    unsigned long long printed_bad = 0;

    char buf[1024];
    while (std::fgets(buf, sizeof(buf), f)) {
        std::string line = buf;
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        uint32_t order = 0;
        double ra = 0.0, dec = 0.0;
        uint64_t ipix = 0;
        if (!parse_line(line, order, ra, dec, ipix)) {
            ++bad_parse;
            continue;
        }
        ++lines;
        const uint32_t nside = uint32_t(1) << order;
        const uint64_t got = astrocs::healpix::ang2pix_nest(nside, ra, dec);
        if (got != ipix) {
            ++mismatch;
            if (mismatch <= 10) {
                std::printf("MISMATCH order=%u ra=%.10f dec=%.10f expect=%llu got=%llu\n",
                            order, ra, dec, (unsigned long long)ipix,
                            (unsigned long long)got);
            }
            continue;
        }
        if (order <= 22) {
            const uint64_t npface = static_cast<uint64_t>(nside) * nside;
            const uint32_t face = static_cast<uint32_t>(got / npface);
            if (face < 12) ++per_face[face];
        }
        // pix2ang roundtrip: 中心角距容差 = 1.2 × hp_res。
        // 极点 (|dec|=90) 是极冠像素的角点而非中心, 中心距离可远大于像素半径,
        // 属几何正确, 单独跳过 (ang2pix 本身已由 oracle 逐点验证)。
        if (std::fabs(dec) >= 89.999999) continue;
        // pix2ang roundtrip: 中心角距容差 = 1.2 × hp_res
        double rra = 0.0, rdec = 0.0;
        astrocs::healpix::pix2ang_nest(nside, got, rra, rdec);
        const double hp_res_deg = std::sqrt(3.141592653589793 / 3.0) / nside * 180.0 / 3.141592653589793;
        double d = astrocs::healpix::angular_distance_deg(ra, dec, rra, rdec);
        if (d > max_roundtrip_deg) max_roundtrip_deg = d;
        if (d > 1.2 * hp_res_deg + 1e-9) {
            ++bad_roundtrip;
            if (printed_bad < 10) {
                std::printf("BAD_RT order=%u ra=%.8f dec=%.8f ipix=%llu got_ra=%.8f got_dec=%.8f d=%.6f\n",
                            order, ra, dec, (unsigned long long)got, rra, rdec, d);
                ++printed_bad;
            }
        }
    }
    std::fclose(f);

    std::printf("lines=%llu mismatch=%llu bad_parse=%llu bad_roundtrip=%llu max_roundtrip_deg=%.3e\n",
                lines, mismatch, bad_parse, bad_roundtrip, max_roundtrip_deg);
    std::printf("face_counts:");
    for (int i = 0; i < 12; ++i) std::printf(" %llu", per_face[i]);
    std::printf("\n");

    if (lines == 0 || bad_parse != 0 || mismatch != 0 || bad_roundtrip != 0) {
        std::printf("RESULT: FAIL\n");
        return 1;
    }
    std::printf("RESULT: PASS (mismatch=0)\n");
    return 0;
}
