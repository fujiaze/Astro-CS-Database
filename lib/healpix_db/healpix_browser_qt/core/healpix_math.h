#ifndef HEALPIX_MATH_H
#define HEALPIX_MATH_H

#include <cstdint>
#include <vector>
#include <utility>

// HEALPix 球面坐标转换（纯静态方法，无状态）
// 支持 NESTED 排序，任意 nside（最大 8192，用 uint64_t 避免 npface 溢出）
class HealpixMath {
public:
    // NESTED 排序：ipix → (ra, dec)，单位度
    // nside: HEALPix 分辨率参数
    // ipix: 像素索引 [0, 12*nside²)
    // 输出 ra ∈ [0, 360), dec ∈ [-90, 90]
    static void pix2ang_nest(uint32_t nside, uint64_t ipix,
                             double& ra, double& dec);

    // NESTED 排序：(ra, dec) → ipix
    static uint64_t ang2pix_nest(uint32_t nside, double ra, double dec);

    // 球面圆盘查询：返回圆盘内所有 ipix
    // ra, dec: 圆盘中心（度）
    // radius_deg: 圆盘半径（度）
    static std::vector<uint64_t> query_disc(uint32_t nside,
                                            double ra, double dec,
                                            double radius_deg);

    // ud_grade 降采样结果
    struct GradeResult {
        uint32_t nside;
        std::vector<uint64_t> ipix;
        std::vector<float> pixel;
    };

    // NESTED 降采样：src_nside → target_nside（必须为 src_nside 的整数次幂分之一）
    // 相同 ipix_coarse 的 4^k 个 ipix_fine 像素求均值合并
    static GradeResult ud_grade(uint32_t src_nside,
                                const std::vector<uint64_t>& src_ipix,
                                const std::vector<float>& src_pixel,
                                uint32_t target_nside);

    // 大圆距离（度）
    static double angular_distance(double ra1, double dec1,
                                   double ra2, double dec2);
};

#endif // HEALPIX_MATH_H
