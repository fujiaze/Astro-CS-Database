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
    if (nside == 0) return 0;
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
    if (nside == 0) return;
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
    return ipix << (2u * shift);
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
namespace {
inline int base_neighbour(int hp, int dx, int dy) {
    bool north = (hp <= 3);
    bool south = (hp >= 8);
    if (north) {
        if (dx ==  1 && dy ==  0) return (hp + 1) % 4;
        if (dx ==  0 && dy ==  1) return (hp + 3) % 4;
        if (dx ==  1 && dy ==  1) return (hp + 2) % 4;
        if (dx == -1 && dy ==  0) return hp + 4;
        if (dx ==  0 && dy == -1) return 4 + ((hp + 1) % 4);
        if (dx == -1 && dy == -1) return hp + 8;
        return -1;
    } else if (south) {
        if (dx ==  1 && dy ==  0) return 4 + ((hp + 1) % 4);
        if (dx ==  0 && dy ==  1) return hp - 4;
        if (dx == -1 && dy ==  0) return 8 + ((hp + 3) % 4);
        if (dx ==  0 && dy == -1) return 8 + ((hp + 1) % 4);
        if (dx == -1 && dy == -1) return 8 + ((hp + 2) % 4);
        if (dx ==  1 && dy ==  1) return hp - 8;
        return -1;
    } else {
        if (dx ==  1 && dy ==  0) return hp - 4;
        if (dx ==  0 && dy ==  1) return (hp + 3) % 4;
        if (dx == -1 && dy ==  0) return 8 + ((hp + 3) % 4);
        if (dx ==  0 && dy == -1) return hp + 4;
        if (dx ==  1 && dy == -1) return 4 + ((hp + 1) % 4);
        if (dx == -1 && dy ==  1) return 4 + ((hp - 1) % 4);
        return -1;
    }
}
} // namespace

std::vector<uint64_t> neighbors(uint32_t nside, uint64_t ipix) {
    std::vector<uint64_t> result;
    if (nside == 0) return result;
    uint32_t order = nside_to_order(nside);
    uint32_t ns = uint32_t(1) << order;
    uint64_t npface = uint64_t(ns) * ns;
    if (ipix >= 12ULL * npface) return result;
    uint32_t bighp = uint32_t(ipix / npface);
    uint32_t x = 0, y = 0;
    nest_to_xy(ipix % npface, order, x, y);
    int Ns = (int)ns;
    int base = (int)bighp;
    bool nPol = (base <= 3);
    bool sPol = (base >= 8);
    struct Dir { int dx, dy; };
    Dir dirs[8] = {{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1},{0,-1},{1,-1}};
    auto clampi = [](int v, int lo, int hi){ return v<lo?lo:(v>hi?hi:v); };
    for (int d = 0; d < 8; ++d) {
        int dx = dirs[d].dx, dy = dirs[d].dy;
        int nx = (int)x + dx, ny = (int)y + dy;
        int nbase = base;
        if (nx >= 0 && nx < Ns && ny >= 0 && ny < Ns) {
        } else {
            bool atRight  = (x == (uint32_t)Ns - 1);
            bool atLeft   = (x == 0);
            bool atTop    = (y == (uint32_t)Ns - 1);
            bool atBottom = (y == 0);
            bool corner = ((dx != 0) && (dy != 0) &&
                           ((atRight && atTop) || (atRight && atBottom) ||
                            (atLeft  && atTop) || (atLeft  && atBottom)));
            bool edgeX  = (dx != 0 && ((atRight && dx > 0) || (atLeft && dx < 0)));
            bool edgeY  = (dy != 0 && ((atTop  && dy > 0) || (atBottom && dy < 0)));
            if (corner) {
                if (nPol || sPol) nbase = base_neighbour(base, dx, dy);
                else continue;
            } else if (edgeX && edgeY) {
                continue;
            } else if (edgeX) {
                nbase = base_neighbour(base, dx, 0);
            } else if (edgeY) {
                nbase = base_neighbour(base, 0, dy);
            } else continue;
            if (nbase < 0) continue;
            nx = ((int)x + dx + Ns) % Ns;
            ny = ((int)y + dy + Ns) % Ns;
            if (nPol) {
                if (atRight && dx > 0) { nx = Ns - 1; std::swap(nx, ny); }
                else if (atTop && dy > 0) { ny = Ns - 1; std::swap(nx, ny); }
            } else if (sPol) {
                if (atLeft && dx < 0) { nx = 0; std::swap(nx, ny); }
                else if (atBottom && dy < 0) { ny = 0; std::swap(nx, ny); }
            }
        }
        nx = clampi(nx, 0, Ns - 1);
        ny = clampi(ny, 0, Ns - 1);
        uint64_t out = uint64_t(nbase) * npface + xy_to_nest((uint32_t)nx, (uint32_t)ny, order);
        result.push_back(out);
    }
    return result;
}

std::vector<uint64_t> query_disc(uint32_t nside, double ra_deg, double dec_deg,
                                 double radius_arcsec) {
    std::vector<uint64_t> result;
    if (nside == 0) return result;
    double radius_rad = radius_arcsec * kPi / (180.0 * 3600.0);
    double decR = dec_deg * kPi / 180.0;
    double raR  = ra_deg  * kPi / 180.0;
    double cdec = std::cos(decR);
    double cx = cdec * std::cos(raR);
    double cy = cdec * std::sin(raR);
    double cz = std::sin(decR);
    uint64_t center = ang2pix_nest(nside, ra_deg, dec_deg);
    // BFS over neighbors, angular distance check via dot product
    std::unordered_map<uint64_t, bool> visited;
    visited.reserve(64);
    std::vector<uint64_t> queue;
    queue.push_back(center);
    visited[center] = true;
    while (!queue.empty()) {
        std::vector<uint64_t> next;
        for (uint64_t ip : queue) {
            double ra_c, dec_c;
            pix2ang_nest(nside, ip, ra_c, dec_c);
            double dec2 = dec_c * kPi / 180.0;
            double ra2  = ra_c  * kPi / 180.0;
            double c2 = std::cos(dec2);
            double px = c2 * std::cos(ra2);
            double py = c2 * std::sin(ra2);
            double pz = std::sin(dec2);
            double cosd = px*cx + py*cy + pz*cz;
            if (cosd > 1.0) cosd = 1.0;
            if (cosd < -1.0) cosd = -1.0;
            double dist = std::acos(cosd);
            if (dist <= radius_rad) {
                result.push_back(ip);
                auto nbrs = neighbors(nside, ip);
                for (uint64_t nb : nbrs) {
                    if (!visited.count(nb)) {
                        visited[nb] = true;
                        next.push_back(nb);
                    }
                }
            }
        }
        queue.swap(next);
    }
    std::sort(result.begin(), result.end());
    return result;
}

} // namespace healpix
} // namespace astrocs

