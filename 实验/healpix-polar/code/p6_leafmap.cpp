// ============================================================================
// p6_leafmap.cpp — EXP-07-POLAR 实验 6：极冠叶的边界表示误差空间图
//
//  T18 单叶"弦折线面积"相对解析叶面积 pi/(3 nside^2) 的亏损（按 1/d^2 衰减）
//  T19 单叶边界真实曲线对 4 角大圆弧弦的最大偏差 / hp_res
//
// 用法: p6 t18 | p6 t19
// ============================================================================
#include "polar_common.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
using namespace pp;

static double leaf_area_chord(int f, uint32_t Ns, uint32_t i, uint32_t j) {
    double uv[4][2]; leaf_corner_uv(Ns, i, j, uv); std::vector<V3> q;
    for (int e = 0; e < 4; ++e) q.push_back(chart_uv_to_xyz(f, uv[e][0], uv[e][1], 1));
    return vos_area_rotated(q);
}
static double leaf_area_curve(int f, uint32_t Ns, uint32_t i, uint32_t j, int K) {
    std::vector<V3> q; double uv[4][2]; leaf_corner_uv(Ns, i, j, uv);
    for (int e = 0; e < 4; ++e) {
        int e2 = (e + 1) % 4;
        for (int k = 0; k < K; ++k) {
            double t = (double)k / (double)K;
            q.push_back(chart_uv_to_xyz(f, uv[e][0] + t*(uv[e2][0]-uv[e][0]),
                                           uv[e][1] + t*(uv[e2][1]-uv[e][1]), 1));
        }
    }
    return vos_area_rotated(q);
}
static double leaf_chord_dev(int f, uint32_t Ns, uint32_t i, uint32_t j) {
    double uv[4][2]; leaf_corner_uv(Ns, i, j, uv); double worst = 0.0;
    for (int e = 0; e < 4; ++e) {
        int e2 = (e + 1) % 4;
        V3 A = chart_uv_to_xyz(f, uv[e][0], uv[e][1], 1);
        V3 B = chart_uv_to_xyz(f, uv[e2][0], uv[e2][1], 1);
        V3 n = normalize3(cross3(A, B));
        for (int k = 1; k < 24; ++k) {
            double t = k / 24.0;
            V3 M = chart_uv_to_xyz(f, uv[e][0] + t*(uv[e2][0]-uv[e][0]),
                                      uv[e][1] + t*(uv[e2][1]-uv[e][1]), 1);
            worst = std::max(worst, std::fabs(std::asin(std::max(-1.0, std::min(1.0, dot3(n, M))))));
        }
    }
    return worst;
}

static const int DS[] = {1,2,4,8,16,32,64,128,256,512,1024,4096,16384,65536,262144,1048576};
static const int NDS = 16;

static void t18(uint32_t Ns) {
    const double exact = kPi / (3.0 * (double)Ns * (double)Ns);
    printf("## T18 单叶弦折线面积相对解析叶面积 pi/(3 nside^2) 的亏损 (nside=%u)\n", Ns);
    printf("## A. face0 北极冠, 沿 v=Ns-1 扫 u\n");
    for (int b = 0; b < NDS; ++b) {
        uint32_t i = Ns - (uint32_t)DS[b], j = Ns - 1;
        double a1 = leaf_area_chord(0, Ns, i, j), a2 = leaf_area_curve(0, Ns, i, j, 512);
        printf("  du=%-9d 亏损(chord-curve)/exact=%+.4e   chord/exact-1=%+.4e\n",
               DS[b], (a1 - a2) / exact, a1 / exact - 1.0);
    }
    printf("## B. face0 北极冠, 沿对角 du=dv\n");
    for (int b = 0; b < NDS; ++b) {
        uint32_t i = Ns - (uint32_t)DS[b], j = Ns - (uint32_t)DS[b];
        double a1 = leaf_area_chord(0, Ns, i, j), a2 = leaf_area_curve(0, Ns, i, j, 512);
        printf("  d=%-9d 亏损(chord-curve)/exact=%+.4e   chord/exact-1=%+.4e\n",
               DS[b], (a1 - a2) / exact, a1 / exact - 1.0);
    }
    printf("## C. 极冠外对照 (face4 赤道面, 应在数值地板)\n");
    for (int b = 0; b < 6; ++b) {
        uint32_t i = Ns/2 + (uint32_t)DS[b], j = Ns/2;
        double a1 = leaf_area_chord(4, Ns, i, j), a2 = leaf_area_curve(4, Ns, i, j, 512);
        printf("  du=%-9d 亏损=%+.4e\n", DS[b], (a1 - a2) / exact);
    }
}

static void t19(uint32_t Ns) {
    const double hp = std::sqrt(kPi / 3.0) / (double)Ns;
    printf("## T19 叶边界真实曲线对 4 角大圆弧弦的最大偏差 / hp_res (nside=%u)\n", Ns);
    printf("## A. face0 北极冠二维图 (行 = v 索引距 1 的偏移 dv, 列 = u 索引距 1 的偏移 du)\n     du= ");
    const int DU[] = {1,2,4,8,16,32,64,128,256,1024,16384,262144,1048576};
    for (int k = 0; k < 13; ++k) printf("%10d", DU[k]);
    printf("\n");
    for (int a = 0; a < 13; ++a) {
        printf("dv=%-6d", DU[a]);
        for (int b = 0; b < 13; ++b) {
            uint32_t i = Ns - (uint32_t)DU[b], j = Ns - (uint32_t)DU[a];
            if (i >= Ns || j >= Ns) { printf("%10s", "-"); continue; }
            printf("%10.2e", leaf_chord_dev(0, Ns, i, j) / hp);
        }
        printf("\n");
    }
    printf("## B. 其他关键位置\n");
    struct L { const char* n; int f; double u, v; };
    const L ls[] = {
        {"face0 极冠/赤道交界 (0.5,0.5)", 0, 0.5, 0.5},
        {"face0 三角点角 (0,0)",          0, 0.0, 0.0},
        {"face0 三角点角 (1,0)",          0, 1.0, 0.0},
        {"face0 赤道三角内部 (0.2,0.2)",  0, 0.2, 0.2},
        {"face4 赤道面中心 (0.5,0.5)",    4, 0.5, 0.5},
        {"face4 z=0 角 (1,0)",            4, 1.0, 0.0},
    };
    for (const auto& x : ls) {
        uint32_t i = (uint32_t)(x.u * Ns), j = (uint32_t)(x.v * Ns);
        if (i >= Ns) i = Ns - 1; if (j >= Ns) j = Ns - 1;
        printf("  %-32s 弦偏差=%.6e hp_res\n", x.n, leaf_chord_dev(x.f, Ns, i, j) / hp);
    }
}

int main(int argc, char** argv) {
    const char* which = (argc > 1) ? argv[1] : "all";
    uint32_t Ns = 2097152u;
    if (argc > 2) Ns = (uint32_t)strtoul(argv[2], nullptr, 10);
    if (std::strcmp(which, "t18") == 0 || std::strcmp(which, "all") == 0) t18(Ns);
    if (std::strcmp(which, "t19") == 0 || std::strcmp(which, "all") == 0) t19(Ns);
    return 0;
}
