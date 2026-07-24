#include "healpix_math.h"
#include "logger.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <algorithm>
#include <cmath>
#include <unordered_map>

// HEALPix 面索引查找表（HEALPix 标准）
// 用于 pix2ang_nest: jr = jrll[face]*nside - ix - iy - 1
//                    jp = (jpll[face]*nr + ix - iy + 1 + kshift) / 2
static const int JRLL[12] = {2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4};
static const int JPLL[12] = {1, 3, 5, 7, 0, 2, 4, 6, 1, 3, 5, 7};

// ============================================================================
// 内部辅助
// ============================================================================

static int ilog2_nside(uint32_t nside) {
    int k = 0;
    while ((1u << k) < nside) ++k;
    return k;
}

// NESTED 位交错解码: 从 ip_low 提取 (ix, iy)
static void nest_to_xy(uint64_t ip_low, int nside, int& ix, int& iy) {
    ix = 0;
    iy = 0;
    int bits = ilog2_nside((uint32_t)nside);
    for (int i = 0; i < bits; i++) {
        ix |= (int)(((ip_low >> (2 * i)) & 1ULL) << i);
        iy |= (int)(((ip_low >> (2 * i + 1)) & 1ULL) << i);
    }
}

// NESTED 位交错编码: (ix, iy) → ip_low
static uint64_t xy_to_nest(int ix, int iy, int nside) {
    uint64_t ip_low = 0;
    int bits = ilog2_nside((uint32_t)nside);
    for (int i = 0; i < bits; i++) {
        ip_low |= (uint64_t)(((ix >> i) & 1) << (2 * i));
        ip_low |= (uint64_t)(((iy >> i) & 1) << (2 * i + 1));
    }
    return ip_low;
}

// ============================================================================
// pix2ang_nest: ipix → (ra, dec)
// 参考 HEALPix 标准 Healpix_base::pix2ang_z_phi (NEST 分支)
// ============================================================================

void HealpixMath::pix2ang_nest(uint32_t nside, uint64_t ipix,
                                double& ra, double& dec) {
    if (nside == 0) {
        ra = 0.0;
        dec = 0.0;
        return;
    }

    const uint64_t npface = (uint64_t)nside * (uint64_t)nside;
    const uint64_t total = 12ULL * npface;

    if (ipix >= total) {
        ra = 0.0;
        dec = 0.0;
        return;
    }

    int face_num = (int)(ipix / npface);
    uint64_t ip_low = ipix % npface;

    int ix, iy;
    nest_to_xy(ip_low, (int)nside, ix, iy);

    int nside_i = (int)nside;
    int nl4 = 4 * nside_i;
    int jr = JRLL[face_num] * nside_i - ix - iy - 1;

    // HEALPix 官方系数: fact1_ = 2/(3*nside), fact2_ = 1/(3*nside²)
    // 注意: fact2_ = 1/(3*nside²), 不是 4/(3*nside²)
    // 验证: nside=8 ring=1 北极 z = 1 - 1*fact2 = 1 - 1/192 = 0.994792 (与 astropy 一致)
    // npface = nside², 所以 fact2_ = 1/(3*npface)
    double fact1 = 2.0 / (3.0 * (double)nside_i);
    double fact2 = 1.0 / (3.0 * (double)npface);

    double z;
    int nr, kshift;

    if (jr < nside_i) {
        // 北极帽区: z = 1 - ring² * fact2_
        nr = jr;
        z = 1.0 - (double)(nr * nr) * fact2;
        kshift = 0;
    } else if (jr > 3 * nside_i) {
        // 南极帽区: z = ring² * fact2_ - 1
        nr = nl4 - jr;
        z = (double)(nr * nr) * fact2 - 1.0;
        kshift = 0;
    } else {
        // 赤道带: z = (2*nside - jr) * fact1_
        nr = nside_i;
        z = (double)(2 * nside_i - jr) * fact1;
        kshift = (jr - nside_i) & 1;
    }

    int jp = (JPLL[face_num] * nr + ix - iy + 1 + kshift) / 2;
    if (jp > nl4) jp -= nl4;
    if (jp < 1) jp += nl4;

    double phi = ((double)jp - (kshift + 1) * 0.5) * (M_PI / 2.0) / (double)nr;

    // 归一化 phi 到 [0, 2π)
    while (phi < 0) phi += 2.0 * M_PI;
    while (phi >= 2.0 * M_PI) phi -= 2.0 * M_PI;

    if (z > 1.0) z = 1.0;
    if (z < -1.0) z = -1.0;

    dec = std::asin(z) * 180.0 / M_PI;
    ra = phi * 180.0 / M_PI;
}

// ============================================================================
// ang2pix_nest: (ra, dec) → ipix
// 参考 HEALPix 官方 Healpix_base::ang2pix_z_phi (NEST 分支)
// 来源: https://github.com/3dem/relion/blob/master/src/Healpix_2.15a/healpix_base.cc
// ============================================================================

uint64_t HealpixMath::ang2pix_nest(uint32_t nside, double ra, double dec) {
    if (nside == 0) return 0;

    const double two_pi = 2.0 * M_PI;
    const double halfpi = M_PI / 2.0;
    const double twothird = 2.0 / 3.0;

    // dec → z (theta=colatitude 的 cos; dec=90° → z=1 北极)
    double z = std::sin(dec * M_PI / 180.0);
    double phi = ra * M_PI / 180.0;

    // 归一化 phi 到 [0, 2π)
    phi = phi - std::floor(phi / two_pi) * two_pi;
    if (z > 1.0) z = 1.0;
    if (z < -1.0) z = -1.0;

    double za = std::fabs(z);
    double tt = phi / halfpi;  // [0, 4)

    int n = (int)nside;
    int order = ilog2_nside(nside);

    int face_num, ix, iy;

    if (za <= twothird) {
        // 赤道带 - 使用 (jp, jm) 双线索引, face 反查用 ifp/ifm 移位
        // 这是 HEALPix 官方算法, 与 jr/jp 方案不同
        double temp1 = (double)n * (0.5 + tt);
        double temp2 = (double)n * (z * 0.75);
        int jp = (int)(temp1 - temp2);  // ascending edge line
        int jm = (int)(temp1 + temp2);  // descending edge line
        int ifp = jp >> order;          // [0, 4]
        int ifm = jm >> order;          // [0, 4]

        if (ifp == ifm) {
            // faces 4 to 7 (赤道 4 面)
            face_num = (ifp == 4) ? 4 : (ifp + 4);
        } else if (ifp < ifm) {
            // (half-)faces 0 to 3 (北极 4 面的赤道部分)
            face_num = ifp;
        } else {
            // (half-)faces 8 to 11 (南极 4 面的赤道部分)
            face_num = ifm + 8;
        }
        ix = jm & (n - 1);
        iy = n - (jp & (n - 1)) - 1;
    } else {
        // 极区 (|z| > 2/3)
        int ntt = (int)tt;
        if (ntt >= 4) ntt = 3;
        double tp = tt - (double)ntt;
        double tmp = (double)n * std::sqrt(3.0 * (1.0 - za));
        int jp = (int)(tp * tmp);           // increasing edge line index
        int jm = (int)((1.0 - tp) * tmp);   // decreasing edge line index
        // 边界保护
        if (jp >= n) jp = n - 1;
        if (jm >= n) jm = n - 1;

        if (z >= 0) {
            // 北极 (face 0-3)
            face_num = ntt;
            ix = n - jm - 1;
            iy = n - jp - 1;
        } else {
            // 南极 (face 8-11)
            face_num = ntt + 8;
            ix = jp;
            iy = jm;
        }
    }

    // xyf2nest: face_num * npface + xy_to_nest(ix, iy)
    uint64_t npface = (uint64_t)n * (uint64_t)n;
    uint64_t ip_low = xy_to_nest(ix, iy, n);
    return (uint64_t)face_num * npface + ip_low;
}

// ============================================================================
// query_disc: 圆盘查询
// ============================================================================

std::vector<uint64_t> HealpixMath::query_disc(uint32_t nside,
                                               double ra, double dec,
                                               double radius_deg) {
    std::vector<uint64_t> result;
    if (nside == 0) return result;

    uint64_t total = 12ULL * (uint64_t)nside * (uint64_t)nside;

    result.reserve(1024);
    for (uint64_t ipix = 0; ipix < total; ipix++) {
        double p_ra, p_dec;
        pix2ang_nest(nside, ipix, p_ra, p_dec);
        double dist = angular_distance(ra, dec, p_ra, p_dec);
        if (dist <= radius_deg) {
            result.push_back(ipix);
        }
    }
    return result;
}

// ============================================================================
// ud_grade: NESTED 降采样
// ============================================================================

HealpixMath::GradeResult HealpixMath::ud_grade(uint32_t src_nside,
                                                const std::vector<uint64_t>& src_ipix,
                                                const std::vector<float>& src_pixel,
                                                uint32_t target_nside) {
    GradeResult result;
    result.nside = target_nside;

    if (src_nside == 0 || target_nside == 0 || target_nside > src_nside) {
        return result;
    }

    uint32_t ratio = src_nside / target_nside;
    if (ratio == 0) return result;

    if ((ratio & (ratio - 1)) != 0) {
        LOG_WARN("ud_grade: src_nside/target_nside=%u/%u 不是 2 的幂", src_nside, target_nside);
        return result;
    }

    int shift = 0;
    uint32_t r = ratio;
    while (r > 1) { shift++; r >>= 1; }
    int bit_shift = 2 * shift;

    std::unordered_map<uint64_t, std::pair<double, uint64_t>> agg;
    agg.reserve(src_ipix.size());

    size_t n = std::min(src_ipix.size(), src_pixel.size());
    for (size_t i = 0; i < n; i++) {
        uint64_t ipix_coarse = src_ipix[i] >> bit_shift;
        auto& entry = agg[ipix_coarse];
        entry.first += src_pixel[i];
        entry.second += 1;
    }

    result.ipix.reserve(agg.size());
    result.pixel.reserve(agg.size());
    for (const auto& kv : agg) {
        result.ipix.push_back(kv.first);
        result.pixel.push_back((float)(kv.second.first / (double)kv.second.second));
    }

    return result;
}

// ============================================================================
// angular_distance: 大圆距离
// ============================================================================

double HealpixMath::angular_distance(double ra1, double dec1,
                                     double ra2, double dec2) {
    const double DEG2RAD = M_PI / 180.0;
    double dec1_r = dec1 * DEG2RAD;
    double dec2_r = dec2 * DEG2RAD;
    double d_ra = (ra2 - ra1) * DEG2RAD;

    double cos_d = std::sin(dec1_r) * std::sin(dec2_r) +
                   std::cos(dec1_r) * std::cos(dec2_r) * std::cos(d_ra);

    if (cos_d > 1.0) cos_d = 1.0;
    if (cos_d < -1.0) cos_d = -1.0;

    return std::acos(cos_d) * 180.0 / M_PI;
}
