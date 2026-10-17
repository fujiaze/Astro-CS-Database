// Issue #92 —— star_matcher 真正消费的投影对象探针。
//
// 同一份驱动编译两次即可得到 A/B:
//   post: 当前 main 的 lib/algorithms/photometry/cpp/src/wcs_transform.cpp
//   pre : #56/#57/#61 修复前 (38eaeba8b^) 的同名文件
// 驱动只实例化生产类 pc::WcsTransform, 不复制投影数学。
//
// 三种构造:
//   real : (order, A, B, AP, BP)  —— 生产 pc_calibrate_*_v2_qf 的调用面
//   noap : (order, A, B, nullptr, nullptr) —— 同一 WcsTransform 的不动点分支
//   none : (0, nullptr, ...) —— 纯线性 (无 SIP)
//
// 用法: wcs_probe <gaia_csv|-> <out_json> [details_n]
//   gaia_csv 每行 "ra,dec,mag" (真实 Gaia 星); 空则用内置网格。
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "wcs_transform.h"
#include "params_issue92.inc"

namespace {

struct Row {
    double ra, dec, mag;
};

std::vector<Row> read_rows(const char* path) {
    std::vector<Row> rows;
    FILE* f = std::fopen(path, "r");
    if (!f) return rows;
    char line[512];
    while (std::fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        double a = 0, b = 0, c = 0;
        if (std::sscanf(line, "%lf,%lf,%lf", &a, &b, &c) >= 2) rows.push_back({a, b, c});
    }
    std::fclose(f);
    return rows;
}

struct Stat {
    int n = 0, n_nan = 0, n_in = 0;
    double xmin = 1e300, xmax = -1e300, ymin = 1e300, ymax = -1e300;
    std::vector<std::array<double, 4>> first;
};

Stat project(pc::WcsTransform& w, const std::vector<Row>& rows, int n_detail) {
    Stat s;
    s.n = static_cast<int>(rows.size());
    for (const Row& r : rows) {
        double x = 0, y = 0;
        w.skyToPixel(r.ra, r.dec, x, y);
        const bool nan = !std::isfinite(x) || !std::isfinite(y);
        if (nan) {
            ++s.n_nan;
        } else {
            if (x < s.xmin) s.xmin = x;
            if (x > s.xmax) s.xmax = x;
            if (y < s.ymin) s.ymin = y;
            if (y > s.ymax) s.ymax = y;
            if (x >= 0.0 && x < P_W && y >= 0.0 && y < P_H) ++s.n_in;
        }
        if (static_cast<int>(s.first.size()) < n_detail)
            s.first.push_back({r.ra, r.dec, x, y});
    }
    return s;
}

void dump(const char* tag, const Stat& s, bool finite_box) {
    std::printf("  \"%s\": {\"n\": %d, \"n_nan\": %d, \"n_in_frame\": %d", tag, s.n, s.n_nan, s.n_in);
    if (finite_box && s.n_in + s.n_nan < s.n)
        std::printf(", \"x_range\": [%.6f, %.6f], \"y_range\": [%.6f, %.6f]", s.xmin, s.xmax, s.ymin, s.ymax);
    std::printf("}");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <gaia_csv|-> <out_json> [details_n]\n", argv[0]);
        return 2;
    }
    const int n_detail = (argc > 3) ? std::atoi(argv[3]) : 20;
    std::vector<Row> rows;
    if (std::strcmp(argv[1], "-") == 0) {
        for (int i = 0; i < 17; ++i)
            for (int j = 0; j < 17; ++j)
                rows.push_back({P_CRVAL1 + (i - 8) * 0.05, P_CRVAL2 + (j - 8) * 0.05, 12.0});
    } else {
        rows = read_rows(argv[1]);
    }

    pc::WcsTransform real(P_CRVAL1, P_CRVAL2, P_CRPIX1, P_CRPIX2,
                          P_CD11, P_CD12, P_CD21, P_CD22,
                          P_ORDER, P_A, P_B, P_AP, P_BP);
    pc::WcsTransform noap(P_CRVAL1, P_CRVAL2, P_CRPIX1, P_CRPIX2,
                          P_CD11, P_CD12, P_CD21, P_CD22,
                          P_ORDER, P_A, P_B, nullptr, nullptr);
    pc::WcsTransform none(P_CRVAL1, P_CRVAL2, P_CRPIX1, P_CRPIX2,
                          P_CD11, P_CD12, P_CD21, P_CD22,
                          0, nullptr, nullptr, nullptr, nullptr);

    // 参考点 + 角点: skyToPixel 是否给出预期 0-based 像素
    struct Pt { double x, y; };
    const Pt pts[] = {{0, 0}, {2047.5, 2047.5}, {4095, 4095}, {1000, 3000}, {3000, 1000}};
    std::printf("{\n");
    std::printf("  \"source\": \"%s\",\n", P_SRC);
    std::printf("  \"order\": %d, \"ap_order\": %d, \"sip_ap0\": %.17g, \"sip_bp0\": %.17g,\n",
                P_ORDER, P_AP_ORDER, P_AP[0], P_BP[0]);
    std::printf("  \"n_pairs\": %d, \"rms_arcsec\": %.6f, \"max_roundtrip_px\": %.6f,"
                " \"max_sip_effect_deg\": %.10g, \"max_forward_cross_deg\": %.6g,\n",
                P_NPAIRS, P_RMS_ARCSEC, P_MAX_ROUNDTRIP_PX, P_MAX_SIP_EFFECT_DEG,
                P_MAX_FORWARD_CROSS_DEG);
    std::printf("  \"roundtrip_pix_to_sky_to_pix\": [\n");
    for (size_t i = 0; i < sizeof(pts) / sizeof(pts[0]); ++i) {
        double ra = 0, dec = 0, xr = 0, yr = 0, xn = 0, yn = 0;
        real.pixelToSky(pts[i].x, pts[i].y, ra, dec);
        real.skyToPixel(ra, dec, xr, yr);
        noap.skyToPixel(ra, dec, xn, yn);
        std::printf("    {\"x\": %.1f, \"y\": %.1f, \"ra\": %.10f, \"dec\": %.10f,"
                    " \"real\": [%s, %s], \"noap\": [%.9f, %.9f]}%s\n",
                    pts[i].x, pts[i].y, ra, dec,
                    std::isfinite(xr) ? std::to_string(xr).c_str() : "\"NaN\"",
                    std::isfinite(yr) ? std::to_string(yr).c_str() : "\"NaN\"",
                    xn, yn, (i + 1 == sizeof(pts) / sizeof(pts[0])) ? "" : ",");
    }
    std::printf("  ],\n");
    const Stat sr = project(real, rows, n_detail);
    const Stat sn = project(noap, rows, n_detail);
    const Stat sf = project(none, rows, n_detail);
    std::printf("  \"projection\": {\n");
    dump("real_apbp", sr, true);
    std::printf(",\n");
    dump("noap_iterate", sn, true);
    std::printf(",\n");
    dump("linear_only", sf, true);
    std::printf("\n  },\n");
    std::printf("  \"first_rows_real\": [");
    for (size_t i = 0; i < sr.first.size(); ++i) {
        const auto& r = sr.first[i];
        std::printf("[%.8f, %.8f, %s, %s]%s", r[0], r[1],
                    std::isfinite(r[2]) ? std::to_string(r[2]).c_str() : "\"NaN\"",
                    std::isfinite(r[3]) ? std::to_string(r[3]).c_str() : "\"NaN\"",
                    (i + 1 == sr.first.size()) ? "" : ", ");
    }
    std::printf("],\n");
    std::printf("  \"first_rows_noap\": [");
    for (size_t i = 0; i < sn.first.size(); ++i) {
        const auto& r = sn.first[i];
        std::printf("[%.8f, %.8f, %.6f, %.6f]%s", r[0], r[1], r[2], r[3],
                    (i + 1 == sn.first.size()) ? "" : ", ");
    }
    std::printf("]\n}\n");
    (void)argv[2];  // out_json 由驱动脚本从 stdout 落盘
    return 0;
}
