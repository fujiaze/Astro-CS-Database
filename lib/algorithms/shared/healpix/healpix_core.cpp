// ============================================================================
// healpix_core.cpp - AstroCS 共享 HEALPix 核心实现 (NESTED)
//
// ang2pix/pix2ang 算法来源: astrometry.net healpix.c
// (BSD-3-Clause, 与 astropy-healpix 内置 C 核心同源, 见
// https:// github.com/astrometry/astrometry.net healpix.c 文件头许可)
// 逐文件审核后迁移, 仅保留 NESTED 所需路径 (ring 排序未迁移)。
// 由 astropy-healpix 1,000,000 点全天 Oracle 验证, mismatch=0。
// 详细归属与许可见本目录 THIRD_PARTY_NOTICE.md。
// ============================================================================

#include "healpix/healpix_core.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace astrocs {
namespace healpix {

namespace {

constexpr double kPi = 3.14159265358979323846264338327950288;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kHalfPi = 0.5 * kPi;
constexpr double kTwoThird = 2.0 / 3.0;
constexpr double kRoot3 = 1.73205080756887729352744634150587237;

// ---- base face 区域判断 ----
inline bool is_north_polar(int base) { return base >= 0 && base <= 3; }
inline bool is_south_polar(int base) { return base >= 8 && base <= 11; }

// ---- 位交错 (NESTED) ----
// compose: (x,y) 位交错 -> ip_low (ix 在偶数位, iy 在奇数位)
uint64_t xy_to_nest(uint32_t ix, uint32_t iy, uint32_t bits) {
    uint64_t ip = 0;
    for (uint32_t i = 0; i < bits; ++i) {
        ip |= (static_cast<uint64_t>((ix >> i) & 1u)) << (2u * i);
        ip |= (static_cast<uint64_t>((iy >> i) & 1u)) << (2u * i + 1u);
    }
    return ip;
}

// decompose: ip_low -> (ix, iy)
void nest_to_xy(uint64_t ip_low, uint32_t bits, uint32_t& ix, uint32_t& iy) {
    ix = 0;
    iy = 0;
    for (uint32_t i = 0; i < bits; ++i) {
        ix |= static_cast<uint32_t>((ip_low >> (2u * i)) & 1ULL) << i;
        iy |= static_cast<uint32_t>((ip_low >> (2u * i + 1u)) & 1ULL) << i;
    }
}

// 单位方向向量 (ra/dec 弧度) -> (x, y, z)
void radec_to_xyz(double ra_rad, double dec_rad, double& vx, double& vy, double& vz) {
    vx = std::cos(dec_rad) * std::cos(ra_rad);
    vy = std::cos(dec_rad) * std::sin(ra_rad);
    vz = std::sin(dec_rad);
}

// 单位方向向量 -> (ra, dec) 弧度, ra ∈ [0, 2π)
void xyz_to_radec(double vx, double vy, double vz, double& ra_rad, double& dec_rad) {
    ra_rad = std::atan2(vy, vx);
    if (ra_rad < 0.0) ra_rad += kTwoPi;
    double z = vz;
    if (z > 1.0) z = 1.0;
    if (z < -1.0) z = -1.0;
    dec_rad = std::asin(z);
}

// ============================================================================
// xyz -> (basehp, x, y) (迁移自 astrometry.net xyztohp, BSD-3)
// ============================================================================
void xyz_to_hp(double vx, double vy, double vz, uint32_t nside,
               uint32_t& basehp, uint32_t& x, uint32_t& y) {
    double phi = std::atan2(vy, vx);
    if (phi < 0.0) phi += kTwoPi;
    const double phi_t = std::fmod(phi, kHalfPi);

    const int ns = static_cast<int>(nside);
    int base = 0;
    int ix = 0, iy = 0;

    if (vz >= kTwoThird || vz <= -kTwoThird) {
        // 极冠区
        const bool north = (vz >= kTwoThird);
        double zz = north ? vz : -vz;
        const double coz = std::sqrt(vx * vx + vy * vy);
        const double kx = (coz / std::sqrt(1.0 + zz)) * kRoot3 *
                          std::fabs(ns * (2.0 * phi_t - kPi) / kPi);
        const double ky = (coz / std::sqrt(1.0 + zz)) * kRoot3 * ns * 2.0 * phi_t / kPi;
        double xx, yy;
        if (north) {
            xx = ns - kx;
            yy = ns - ky;
        } else {
            xx = ky;
            yy = kx;
        }
        ix = static_cast<int>(std::min<double>(ns - 1, std::floor(xx)));
        iy = static_cast<int>(std::min<double>(ns - 1, std::floor(yy)));
        if (ix < 0) ix = 0;
        if (iy < 0) iy = 0;
        const double sector = (phi - phi_t) / kHalfPi;
        int offset = static_cast<int>(std::round(sector));
        offset = ((offset % 4) + 4) % 4;
        base = north ? offset : (8 + offset);
    } else {
        // 赤道带 (含极区面的赤道部分)
        const double zunits = (vz + kTwoThird) / (4.0 / 3.0);
        const double phiunits = phi_t / kHalfPi;
        const double u1 = zunits + phiunits;        // [0,2]
        const double u2 = zunits - phiunits + 1.0;  // [0,2]
        double xx = u1 * ns;
        double yy = u2 * ns;
        const double sector = (phi - phi_t) / kHalfPi;
        int offset = static_cast<int>(std::round(sector));
        offset = ((offset % 4) + 4) % 4;
        if (xx >= ns) {
            xx -= ns;
            if (yy >= ns) {
                yy -= ns;
                base = offset;               // 北极端面
            } else {
                base = ((offset + 1) % 4) + 4;  // 右侧赤道面
            }
        } else {
            if (yy >= ns) {
                yy -= ns;
                base = offset + 4;           // 左侧赤道面
            } else {
                base = 8 + offset;           // 南极端面
            }
        }
        ix = static_cast<int>(std::floor(xx));
        iy = static_cast<int>(std::floor(yy));
        if (ix < 0) ix = 0;
        if (iy < 0) iy = 0;
        if (ix >= ns) ix = ns - 1;
        if (iy >= ns) iy = ns - 1;
    }

    basehp = static_cast<uint32_t>(base);
    x = static_cast<uint32_t>(ix);
    y = static_cast<uint32_t>(iy);
}

// ============================================================================
// (basehp, x, y) -> xyz (迁移自 astrometry.net hp_to_xyz, BSD-3)
// ============================================================================
// dx/dy 为像素内分数位置 (0.5 = 像素中心, 与 astropy-healpix 约定一致)
void hp_to_xyz(uint32_t basehp, uint32_t px, uint32_t py, double dx, double dy,
               uint32_t nside, double& rx, double& ry, double& rz) {
    const int ns = static_cast<int>(nside);
    const int chp0 = static_cast<int>(basehp);
    double x = static_cast<double>(px) + dx;
    double y = static_cast<double>(py) + dy;
    bool equatorial = true;
    double zfactor = 1.0;

    if (is_north_polar(chp0) && (x + y) > ns) {
        equatorial = false;
        zfactor = 1.0;
    }
    if (is_south_polar(chp0) && (x + y) < ns) {
        equatorial = false;
        zfactor = -1.0;
    }

    double z, phi;
    if (equatorial) {
        int chp = chp0;
        double zoff = 0.0, phioff = 0.0;
        x /= static_cast<double>(ns);
        y /= static_cast<double>(ns);
        if (chp <= 3) {
            phioff = 1.0;
        } else if (chp <= 7) {
            zoff = -1.0;
            chp -= 4;
        } else {
            phioff = 1.0;
            zoff = -2.0;
            chp -= 8;
        }
        z = kTwoThird * (x + y + zoff);
        phi = kPi / 4.0 * (x - y + phioff + 2.0 * chp);
        const double rad = std::sqrt(1.0 - z * z);
        rx = rad * std::cos(phi);
        ry = rad * std::sin(phi);
        rz = z;
    } else {
        double phi_t;
        if (zfactor == -1.0) {
            std::swap(x, y);
            x = ns - x;
            y = ns - y;
        }
        if (y == ns && x == ns) {
            phi_t = 0.0;
        } else {
            phi_t = kPi * (ns - y) / (2.0 * ((ns - x) + (ns - y)));
        }
        double vv;
        if (phi_t < kPi / 4.0) {
            vv = std::fabs(kPi * (ns - x) / ((2.0 * phi_t - kPi) * ns) / kRoot3);
        } else {
            vv = std::fabs(kPi * (ns - y) / (2.0 * phi_t * ns) / kRoot3);
        }
        z = (1.0 - vv) * (1.0 + vv);
        const double rad = std::sqrt(1.0 + z) * vv;
        z *= zfactor;
        if (is_south_polar(chp0)) {
            phi = kHalfPi * (chp0 - 8) + phi_t;
        } else {
            phi = kHalfPi * chp0 + phi_t;
        }
        if (phi < 0.0) phi += kTwoPi;
        rx = rad * std::cos(phi);
        ry = rad * std::sin(phi);
        rz = z;
    }
}

} // namespace

// ============================================================================
// ang2pix_nest - (ra, dec) -> NESTED ipix @ nside
// ============================================================================
uint64_t ang2pix_nest(uint32_t nside, double ra_deg, double dec_deg) {
    require_valid_nside(nside);   // R9-B: 非 2 次幂 nside 禁止静默向上取整
    const uint32_t order = nside_to_order(nside);
    const uint32_t ns = uint32_t(1) << order;
    double vx, vy, vz;
    radec_to_xyz(ra_deg * kPi / 180.0, dec_deg * kPi / 180.0, vx, vy, vz);
    uint32_t basehp = 0, x = 0, y = 0;
    xyz_to_hp(vx, vy, vz, ns, basehp, x, y);
    const uint64_t npface = static_cast<uint64_t>(ns) * ns;
    return static_cast<uint64_t>(basehp) * npface + xy_to_nest(x, y, order);
}

// ============================================================================
// pix2ang_nest - NESTED ipix @ nside -> (ra, dec)
// ============================================================================
void pix2ang_nest(uint32_t nside, uint64_t ipix, double& ra_deg, double& dec_deg) {
    ra_deg = 0.0;
    dec_deg = 0.0;
    require_valid_nside(nside);   // R9-B: 非 2 次幂 nside 禁止静默取整
    const uint32_t order = nside_to_order(nside);
    const uint32_t ns = uint32_t(1) << order;
    const uint64_t npface = static_cast<uint64_t>(ns) * ns;
    if (ipix >= 12ULL * npface) return;

    const uint32_t basehp = static_cast<uint32_t>(ipix / npface);
    uint32_t x = 0, y = 0;
    nest_to_xy(ipix % npface, order, x, y);

    double rx, ry, rz;
    hp_to_xyz(basehp, x, y, 0.5, 0.5, ns, rx, ry, rz);
    double ra_rad = 0.0, dec_rad = 0.0;
    xyz_to_radec(rx, ry, rz, ra_rad, dec_rad);
    ra_deg = ra_rad * 180.0 / kPi;
    dec_deg = dec_rad * 180.0 / kPi;
}


// ============================================================================
// nested_local_to_xy / xy_to_nested_local - NESTED 局部索引 <-> 二维 xy
// shift = 每轴位数 (tile 512×512 → 9); local 占用 2*shift 位 (x 偶数位, y 奇数位)
// ============================================================================
void nested_local_to_xy(uint64_t local, uint32_t shift, uint32_t& x, uint32_t& y) {
    if (shift >= 32) shift = 31;
    nest_to_xy(local, shift, x, y);
}

uint64_t xy_to_nested_local(uint32_t x, uint32_t y, uint32_t shift) {
    if (shift >= 32) shift = 31;
    return xy_to_nest(x, y, shift);
}

// ============================================================================
// 标准 HiPS Image tile 排列 (IVOA HiPS 1.0, CDS Hipsgen MAPTILES Oracle 冻结):
// FITS 列 = y, FITS 行 = tile_width-1-x, 行主序 = (tile_width-1-x)*tile_width + y
// ============================================================================
uint64_t nested_local_to_fits_index(uint64_t local, uint32_t shift, uint32_t tile_width) {
    uint32_t x = 0, y = 0;
    nested_local_to_xy(local, shift, x, y);
    const uint32_t maxv = (tile_width > 0) ? (tile_width - 1u) : 0u;
    return (uint64_t)((x <= maxv ? maxv - x : 0u)) * (uint64_t)tile_width + (uint64_t)y;
}

uint64_t fits_index_to_nested_local(uint64_t fits_index, uint32_t shift, uint32_t tile_width) {
    if (tile_width == 0) return 0;
    const uint32_t row = (uint32_t)(fits_index / (uint64_t)tile_width);
    const uint32_t col = (uint32_t)(fits_index % (uint64_t)tile_width);
    const uint32_t maxv = tile_width - 1u;
    const uint32_t x = (row <= maxv) ? (maxv - row) : 0u;
    const uint32_t y = col;
    return xy_to_nested_local(x, y, shift);
}
double angular_distance_deg(double ra1, double dec1, double ra2, double dec2) {
    const double d1 = dec1 * kPi / 180.0;
    const double d2 = dec2 * kPi / 180.0;
    const double dra = (ra2 - ra1) * kPi / 180.0;
    double c = std::sin(d1) * std::sin(d2) + std::cos(d1) * std::cos(d2) * std::cos(dra);
    if (c > 1.0) c = 1.0;
    if (c < -1.0) c = -1.0;
    return std::acos(c) * 180.0 / kPi;
}

uint64_t parent_nest(uint64_t ipix, uint32_t shift) {
    return ipix >> (2u * shift);
}

uint64_t child_nest(uint64_t ipix, uint32_t shift) {
    const uint32_t bits = 2u * shift;
    if (bits == 0) return ipix;
    if (bits >= 64 || (ipix >> (64u - bits)) != 0) {
        throw std::overflow_error("healpix: child_nest shift overflow");
    }
    return ipix << bits;
}

double pixel_resolution_arcsec(uint32_t nside) {
    if (nside == 0) return 0.0;
    double area = 4.0 * kPi / (12.0 * double(nside) * double(nside));
    return std::sqrt(area) * (180.0 * 3600.0 / kPi);
}

uint64_t npix(uint32_t nside) {
    return 12ULL * uint64_t(nside) * uint64_t(nside);
}

// ---- neighbors / query_disc helpers (B4-01 精选迁移, 仅 NESTED) ----
// R9-B 重写: 邻居算法移植自官方 HEALPix C++ (Healpix_3.83 healpix_base.cc /
// healpix_tables.cc, GPL-2+ 参考), 8 方向槽位序 (-0,-+,0+,++ ,+0,+- ,0- ,--)
// + nbnum 面映射/折回表; 由纯 python 双参照 oracle (astrometry get_neighbours
// 逐槽算法 + Healpix paper Gorski 2005 钉值) 全像素交叉验证。
namespace {

// 邻居方向偏移 (官方 Healpix_Tables::nb_xoffset/nb_yoffset)
constexpr int kNbXOffset[8] = { -1, -1,  0, 1, 1, 1,  0, -1 };
constexpr int kNbYOffset[8] = {  0,  1,  1, 1, 0, -1, -1, -1 };
// nbnum (x越界1/-1, y越界3/-3 组合) -> 12 base face 的邻居面, -1 = 不存在
constexpr int kNbFaceArray[9][12] = {
    {  8,  9, 10, 11, -1, -1, -1, -1, 10, 11,  8,  9 },   // S  (nbnum=0)
    {  5,  6,  7,  4,  8,  9, 10, 11,  9, 10, 11,  8 },   // SE (nbnum=1)
    { -1, -1, -1, -1,  5,  6,  7,  4, -1, -1, -1, -1 },   // E  (nbnum=2)
    {  4,  5,  6,  7, 11,  8,  9, 10, 11,  8,  9, 10 },   // SW (nbnum=3)
    {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11 },   // center (nbnum=4)
    {  1,  2,  3,  0,  0,  1,  2,  3,  5,  6,  7,  4 },   // NE (nbnum=5)
    { -1, -1, -1, -1,  7,  4,  5,  6, -1, -1, -1, -1 },   // W  (nbnum=6)
    {  3,  0,  1,  2,  3,  0,  1,  2,  4,  5,  6,  7 },   // NW (nbnum=7)
    {  2,  3,  0,  1, -1, -1, -1, -1,  0,  1,  2,  3 },   // N  (nbnum=8)
};
// nbnum -> 坐标折回位 (bit0: x 镜像, bit1: y 镜像, bit2: x/y 交换), 按 face>>2 取
constexpr int kNbSwapArray[9][3] = {
    { 0, 0, 3 },   // S
    { 0, 0, 6 },   // SE
    { 0, 0, 0 },   // E
    { 0, 0, 5 },   // SW
    { 0, 0, 0 },   // center
    { 5, 0, 0 },   // NE
    { 0, 0, 0 },   // W
    { 6, 0, 0 },   // NW
    { 3, 0, 0 },   // N
};

} // namespace

std::vector<uint64_t> neighbors(uint32_t nside, uint64_t ipix) {
    std::vector<uint64_t> result;
    if (nside == 0) return result;
    const uint32_t order = nside_to_order(nside);
    const uint32_t ns = uint32_t(1) << order;
    const uint64_t npface = uint64_t(ns) * ns;
    if (ipix >= 12ULL * npface) return result;

    const uint32_t face = uint32_t(ipix / npface);
    uint32_t x = 0, y = 0;
    nest_to_xy(ipix % npface, order, x, y);

    result.reserve(8);
    const int Ns = (int)ns;
    const int nsm1 = Ns - 1;
    const int ix = (int)x, iy = (int)y;
    // 输出槽序采用 astrometry get_neighbours 序 (+0,++,0+,-+,-0,--,0-,+-),
    // 即官方 offset 表索引的置换; 共址测试与 python oracle 逐槽比对以此为锚。
    constexpr int kSlotMap[8] = { 4, 3, 2, 1, 0, 7, 6, 5 };
    for (int i = 0; i < 8; ++i) {
        const int s = kSlotMap[i];
        int xx = ix + kNbXOffset[s];
        int yy = iy + kNbYOffset[s];
        int nbnum = 4;
        if (xx < 0)      { xx += Ns; nbnum -= 1; }
        else if (xx > nsm1) { xx -= Ns; nbnum += 1; }
        if (yy < 0)      { yy += Ns; nbnum -= 3; }
        else if (yy > nsm1) { yy -= Ns; nbnum += 3; }

        const int f = kNbFaceArray[nbnum][face];
        if (f < 0) continue;                     // 真角: 该方向无邻居 (赤道面)
        const int bits = kNbSwapArray[nbnum][face >> 2];
        if (bits & 1) xx = nsm1 - xx;
        if (bits & 2) yy = nsm1 - yy;
        if (bits & 4) { int t = xx; xx = yy; yy = t; }
        result.push_back(uint64_t(f) * npface +
                         xy_to_nest((uint32_t)xx, (uint32_t)yy, order));
    }
    // 官方语义: 8 槽位可能含重复 (极点角槽), 保留槽序不去重 (与官方 neighbors 一致);
    // 唯一邻居集合 = result 去重 (消费方如需去重自行处理)。
    return result;
}

// ---- query_disc (R9-B 重写: 官方 HEALPix C++ NEST scheme 四叉树下钻算法) ----
// 移植自 Healpix_3.83 healpix_base.cc query_disc_internal (NEST 分支, fct=0):
// 12 基面入栈 -> 逐像素以 cos 角距分区 (zone 0=盘外/1=安全环/2=中心入盘/3=整像素
// 入盘), 粗于目标阶时 zone 3 整子树直出、zone 1/2 下钻; 目标阶 zone>=2 出结果。
// 安全余量 max_pixrad 取自官方实现 (z=2/3,φ=π/4nside 与 (1-(1-1/nside)^2)/3 处
// 像素角的最大角距)。语义 = 像素中心落在盘内 (与官方 query_disc 一致)。
namespace {

// 官方 max_pixrad (healpix_base.cc L1314)
inline double max_pixrad_order(uint32_t order) {
    const double nside = double(uint32_t(1) << order);
    const double za = kTwoThird;
    const double phi_a = kPi / (4.0 * nside);
    const double t1 = 1.0 - 1.0 / nside;
    const double zb = 1.0 - t1 * t1 / 3.0;
    const double dot = za * zb + std::sqrt((1.0 - za * za) * (1.0 - zb * zb)) * std::cos(phi_a);
    return std::acos(std::min(1.0, std::max(-1.0, dot)));
}

} // namespace

std::vector<uint64_t> query_disc(uint32_t nside, double ra_deg, double dec_deg,
                                 double radius_arcsec) {
    std::vector<uint64_t> result;
    require_valid_nside(nside);   // R9-B: 非 2 次幂 nside 禁止静默取整
    const uint32_t order = nside_to_order(nside);
    double radius_rad = radius_arcsec * kPi / (180.0 * 3600.0);
    double decR = dec_deg * kPi / 180.0;
    const double raR = ra_deg * kPi / 180.0;
    const double cz = std::sin(decR);
    if (radius_rad <= 0.0) {
        // 零/负半径: 仅中心像素 (与既有调用约定一致)
        result.push_back(ang2pix_nest(nside, ra_deg, dec_deg));
        return result;
    }
    if (radius_rad >= kPi) {
        const uint64_t ntotal = 12ULL * uint64_t(uint32_t(1) << order) * (uint32_t(1) << order);
        result.resize(ntotal);
        for (uint64_t i = 0; i < ntotal; ++i) result[i] = i;
        return result;
    }

    const double cosrad = std::cos(radius_rad);
    // 逐阶安全余量 (官方 NEST 分支: 各阶以该阶 max_pixrad 扩盘定候选环)
    std::vector<double> crpdr(order + 1), crmdr(order + 1);
    for (uint32_t o = 0; o <= order; ++o) {
        const double dr = max_pixrad_order(o);
        crpdr[o] = (radius_rad + dr > kPi) ? -1.0 : std::cos(radius_rad + dr);
        crmdr[o] = (radius_rad - dr < 0.0) ? 1.0 : std::cos(radius_rad - dr);
    }

    struct SI { uint64_t pix; uint32_t o; };
    std::vector<SI> stk;
    stk.reserve(64);
    for (int i = 0; i < 12; ++i) stk.push_back({uint64_t(11 - i), 0});   // 官方逆序入栈

    while (!stk.empty()) {
        const SI cur = stk.back();
        stk.pop_back();
        const uint32_t on = uint32_t(1) << cur.o;
        double ra_c = 0.0, dec_c = 0.0;
        pix2ang_nest(on, cur.pix, ra_c, dec_c);
        const double z = std::sin(dec_c * kPi / 180.0);
        const double phi = ra_c * kPi / 180.0;
        // cosdist_zphi (官方): 中心角距余弦
        double cangdist = z * cz +
            std::sqrt(std::max(0.0, (1.0 - z * z) * (1.0 - cz * cz))) * std::cos(phi - raR);
        if (cangdist > 1.0) cangdist = 1.0;
        if (cangdist < -1.0) cangdist = -1.0;
        if (cangdist <= crpdr[cur.o]) continue;   // 盘外 (含安全余量)
        const int zone = (cangdist < cosrad)
            ? 1
            : ((cangdist <= crmdr[cur.o]) ? 2 : 3);
        if (cur.o < order) {
            if (zone >= 3) {
                // 整像素在盘内: 子树全量直出 (官方 pixset.append(pix<<sdist, (pix+1)<<sdist))
                const uint32_t sdist = 2u * (order - cur.o);
                const uint64_t first = cur.pix << sdist;
                const uint64_t last = (cur.pix + 1ull) << sdist;
                for (uint64_t s = first; s < last; ++s) result.push_back(s);
            } else {
                for (int i = 0; i < 4; ++i)   // 下钻 (官方逆序, 输出端已排序故不影响)
                    stk.push_back({4ull * cur.pix + uint64_t(3 - i), cur.o + 1});
            }
        } else {   // cur.o == order (非 inclusive 模式: zone==1 不输出)
            if (zone >= 2) result.push_back(cur.pix);
        }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

} // namespace healpix
} // namespace astrocs

