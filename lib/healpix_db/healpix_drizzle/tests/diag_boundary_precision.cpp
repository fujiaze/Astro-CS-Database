// ============================================================================
// R06 诊断: HEALPix 边界自适应细分精度验证
//
// 验证 nside=256 在 dec=30° 区域的像素边界自适应细分是否工作
// ============================================================================
#include <cstdio>
#include <cmath>
#include <vector>
#include <cstring>

#include "spherical_overlap.h"
#include "healpix_core.h"

static const double PI      = 3.14159265358979323846;
static const double HALF_PI = PI / 2.0;
static const double DEG2RAD = PI / 180.0;
static const double RAD2DEG = 180.0 / PI;
static const double RAD_TO_ARCSEC = 180.0 * 3600.0 / PI;

int main() {
    printf("================================================================\n");
    printf("R06 诊断: HEALPix 边界自适应细分精度验证\n");
    printf("================================================================\n\n");

    int nside = 256;
    healpix::HealpixCore hp(nside, true);
    printf("nside=%d, hp_res=%.4f arcsec = %.6f deg\n",
           nside, hp.pixelResolutionArcsec(), hp.pixelResolutionArcsec()/3600.0);

    // 找一个靠近 (60, 30) 的 HEALPix 像素
    int64_t ipix_test = hp.radec2pix(60.0, 30.0);
    double ra_c, dec_c;
    hp.pix2radec(ipix_test, &ra_c, &dec_c);
    printf("\n测试像素: ipix=%lld center=(%.6f, %.6f)\n",
           (long long)ipix_test, ra_c, dec_c);

    // 获取 4 顶点边界 (samples=1)
    std::vector<spherical::Vec3> b4 = spherical::get_healpix_boundary_sampled(hp, ipix_test, nside, 1);
    printf("\n4 顶点边界 (samples=1, 但自适应细分): 顶点数=%zu\n", b4.size());

    // 计算面积
    double area4 = spherical::spherical_polygon_area(b4);
    double theory_area = 4.0 * PI / (12.0 * nside * nside);
    printf("  面积 = %.10e steradian\n", area4);
    printf("  理论 = %.10e steradian\n", theory_area);
    printf("  相对误差 = %.6e\n", std::fabs(area4 - theory_area) / theory_area);

    // 打印每个顶点
    for (size_t i = 0; i < b4.size(); i++) {
        double r, d;
        spherical::vec_to_radec(b4[i], r, d);
        printf("  [%zu] ra=%.6f dec=%.6f\n", i, r, d);
    }

    // 手动计算每条边的中点偏差
    printf("\n==== 边中点偏差分析 ====\n");
    int nb = (int)b4.size();
    for (int i = 0; i < nb; i++) {
        const auto& p0 = b4[i];
        const auto& p1 = b4[(i + 1) % nb];

        // 大圆弧中点
        spherical::Vec3 p_mid_gc = spherical::normalize(
            spherical::Vec3{p0.x + p1.x, p0.y + p1.y, p0.z + p1.z});

        // 计算偏差
        double dev = spherical::angular_distance(p0, p_mid_gc);
        double dev2 = spherical::angular_distance(p1, p_mid_gc);
        double edge_len = spherical::angular_distance(p0, p1);

        printf("  边 %d->%d: 长度=%.6e rad (%.4f arcsec), GC中点偏p0=%.6e, 偏p1=%.6e\n",
               i, (i + 1) % nb, edge_len, edge_len * RAD_TO_ARCSEC,
               dev, dev2);
    }

    // 测试多个像素的面积精度
    printf("\n==== 多像素面积精度 (dec=30° 附近) ====\n");
    double max_err = 0.0;
    double sum_err = 0.0;
    int n_tested = 0;
    for (double ra = 59.5; ra <= 60.5; ra += 0.1) {
        for (double dec = 29.5; dec <= 30.5; dec += 0.1) {
            int64_t ipix = hp.radec2pix(ra, dec);
            std::vector<spherical::Vec3> b = spherical::get_healpix_boundary_sampled(hp, ipix, nside, 1);
            if (b.size() < 3) continue;
            double area = spherical::spherical_polygon_area(b);
            double rel = std::fabs(area - theory_area) / theory_area;
            if (rel > max_err) max_err = rel;
            sum_err += rel;
            n_tested++;
        }
    }
    printf("测试像素数: %d\n", n_tested);
    printf("最大面积相对误差: %.6e\n", max_err);
    printf("平均面积相对误差: %.6e\n", sum_err / n_tested);

    // 对比: 不同 samples 的效果
    printf("\n==== 不同 samples 对比 ====\n");
    for (int s : {1, 4, 8, 16, 32}) {
        std::vector<spherical::Vec3> b = spherical::get_healpix_boundary_sampled(hp, ipix_test, nside, s);
        double area = spherical::spherical_polygon_area(b);
        double rel = std::fabs(area - theory_area) / theory_area;
        printf("  samples=%2d: 顶点数=%zu, 面积rel_err=%.6e\n", s, b.size(), rel);
    }

    return 0;
}
