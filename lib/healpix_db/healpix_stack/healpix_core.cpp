#include "healpix_core.h"

#include <cstdio>
#include <cmath>
#include <algorithm>
#include <set>

namespace healpix {

// ============================================================================
// 常量
// ============================================================================
static const double PI      = 3.14159265358979323846;
static const double TWO_PI  = 2.0 * PI;
static const double HALF_PI = PI / 2.0;
static const double TWO_THIRDS = 2.0 / 3.0;
static const double ARCSEC_TO_RAD = PI / (180.0 * 3600.0);
static const double RAD_TO_ARCSEC = 180.0 * 3600.0 / PI;

// 平方
static inline double sq(double d) { return d * d; }

// clamp
static inline int clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// 判断 nside 是否为 2 的幂
static bool isPow2(int n) {
    return n > 0 && (n & (n - 1)) == 0;
}

// log2 (n 为 2 的幂)
static int log2i(int n) {
    int b = 0;
    while (n > 1) { n >>= 1; b++; }
    return b;
}

// ============================================================================
// 构造
// ============================================================================
HealpixCore::HealpixCore(int nside, bool nested)
    : m_nside(nside)
    , m_nested(nested)
    , m_nsideBits(0) {
    if (!isPow2(nside)) {
        fprintf(stderr, "[healpix][core] 警告: nside=%d 不是 2 的幂, 行为未定义\n", nside);
    }
    m_nsideBits = log2i(nside > 0 ? nside : 1);
}

int     HealpixCore::getNside() const  { return m_nside; }
bool    HealpixCore::isNested() const  { return m_nested; }
int64_t HealpixCore::getNpix() const   { return (int64_t)12 * m_nside * m_nside; }

double HealpixCore::pixelResolutionArcsec() const {
    // 每像素面积 = 4π / (12 * nside²) 球面度
    // 边长 ≈ sqrt(面积) (弧度) → 转角秒
    double area = 4.0 * PI / (12.0 * (double)m_nside * m_nside);
    return std::sqrt(area) * RAD_TO_ARCSEC;
}

// ============================================================================
// ang → (bighp, x, y)
// 改编自 astrometry.net xyztohp (Calabretta 共形投影的 HEALPix 实现)
// ============================================================================
void HealpixCore::ang2xy(double theta, double phi,
                         int* bighp, int* x, int* y) const {
    double z = std::cos(theta);
    int Ns = m_nside;

    // phi 归一化到 [0, 2π)
    phi = phi - TWO_PI * std::floor(phi / TWO_PI);
    if (phi < 0.0) phi += TWO_PI;
    if (phi >= TWO_PI) phi -= TWO_PI;

    double phi_t = std::fmod(phi, HALF_PI);  // [0, π/2)

    if (z >= TWO_THIRDS || z <= -TWO_THIRDS) {
        // 极冠区
        bool north = (z >= TWO_THIRDS);
        double zfactor = north ? 1.0 : -1.0;

        // 求解 eqn 20: k = Ns - xx (北半球)
        double root1 = (1.0 - z * zfactor) * 3.0 *
                       sq((double)Ns * (2.0 * phi_t - PI) / PI);
        double kx = (root1 <= 0.0) ? 0.0 : std::sqrt(root1);
        // 求解 eqn 19: k = Ns - yy
        double root2 = (1.0 - z * zfactor) * 3.0 *
                       sq((double)Ns * 2.0 * phi_t / PI);
        double ky = (root2 <= 0.0) ? 0.0 : std::sqrt(root2);

        double xx, yy;
        if (north) { xx = Ns - kx; yy = Ns - ky; }
        else       { xx = ky;      yy = kx; }

        *x = clampi((int)std::floor(xx), 0, Ns - 1);
        *y = clampi((int)std::floor(yy), 0, Ns - 1);

        double sector = (phi - phi_t) / HALF_PI;
        int offset = (int)std::round(sector);
        offset = ((offset % 4) + 4) % 4;
        *bighp = north ? offset : (8 + offset);
    } else {
        // 赤道带 (含可能的极冠/赤道过渡判定)
        double zunits  = (z + TWO_THIRDS) / (4.0 / 3.0);   // [0,1]
        double phiunits = phi_t / HALF_PI;                 // [0,1]
        // 转对角单位, 范围 [0,2]
        double u1 = zunits + phiunits;
        double u2 = zunits - phiunits + 1.0;
        double xx = u1 * Ns;
        double yy = u2 * Ns;

        double sector = (phi - phi_t) / HALF_PI;
        int offset = (int)std::round(sector);
        offset = ((offset % 4) + 4) % 4;

        if (xx >= Ns) {
            xx -= Ns;
            if (yy >= Ns) {
                yy -= Ns;
                *bighp = offset;                 // 北极
            } else {
                *bighp = ((offset + 1) % 4) + 4; // 右赤道
            }
        } else {
            if (yy >= Ns) {
                yy -= Ns;
                *bighp = offset + 4;             // 左赤道
            } else {
                *bighp = 8 + offset;             // 南极
            }
        }
        *x = clampi((int)std::floor(xx), 0, Ns - 1);
        *y = clampi((int)std::floor(yy), 0, Ns - 1);
    }
}

// ============================================================================
// (bighp, x, y) → ang
// 改编自 astrometry.net hp_to_xyz
// ============================================================================
void HealpixCore::xy2ang(int bighp, int x, int y,
                         double* theta, double* phi) const {
    int Ns = m_nside;
    int chp = bighp;
    // 像素中心: dx=dy=0.5
    double xf = x + 0.5;
    double yf = y + 0.5;

    bool equatorial = true;
    double zfactor = 1.0;

    // 北极 base 0..3: x+y > Ns 时为极冠部分
    if (chp <= 3) {
        if (xf + yf > Ns) { equatorial = false; zfactor = 1.0; }
    }
    // 南极 base 8..11: x+y < Ns 时为极冠部分
    if (chp >= 8) {
        if (xf + yf < Ns) { equatorial = false; zfactor = -1.0; }
    }

    double z, ph;
    if (equatorial) {
        double zoff = 0.0, phioff = 0.0;
        double xn = xf / Ns, yn = yf / Ns;
        if (chp <= 3) {
            phioff = 1.0;
        } else if (chp <= 7) {
            zoff = -1.0;
            chp -= 4;
        } else { // 8..11
            phioff = 1.0;
            zoff = -2.0;
            chp -= 8;
        }
        z  = (2.0 / 3.0) * (xn + yn + zoff);
        ph = (PI / 4.0) * (xn - yn + phioff + 2.0 * chp);
    } else {
        // 极冠: 解出 phi_t
        double xx = xf, yy = yf;
        if (zfactor == -1.0) {
            std::swap(xx, yy);
            xx = Ns - xx;
            yy = Ns - yy;
        }
        double phi_t;
        if (yy >= Ns && xx >= Ns) {
            phi_t = 0.0;
        } else {
            phi_t = PI * (Ns - yy) / (2.0 * ((Ns - xx) + (Ns - yy)));
        }
        if (phi_t < PI / 4.0) {
            z = 1.0 - sq(PI * (Ns - xx) / ((2.0 * phi_t - PI) * Ns)) / 3.0;
        } else {
            z = 1.0 - sq(PI * (Ns - yy) / (2.0 * phi_t * Ns)) / 3.0;
        }
        z *= zfactor;
        if (bighp >= 8) ph = HALF_PI * (bighp - 8) + phi_t;
        else            ph = HALF_PI * bighp + phi_t;
    }

    if (ph < 0.0) ph += TWO_PI;
    if (ph >= TWO_PI) ph -= TWO_PI;
    if (z > 1.0)  z = 1.0;
    if (z < -1.0) z = -1.0;
    *theta = std::acos(z);
    *phi   = ph;
}

// ============================================================================
// XY ↔ NESTED (Morton / z-order 位交织)
// x 的位填到结果的偶数位, y 的位填到奇数位
// ============================================================================
int64_t HealpixCore::xy2nest(int bighp, int x, int y) const {
    int64_t index = 0;
    int xb = x, yb = y;
    for (int i = 0; i < 32; i++) {
        index |= ((int64_t)(((yb & 1) << 1) | (xb & 1))) << (i * 2);
        xb >>= 1;
        yb >>= 1;
        if (!xb && !yb) break;
    }
    return index + (int64_t)bighp * m_nside * m_nside;
}

void HealpixCore::nest2xy(int64_t ipix, int* bighp, int* x, int* y) const {
    int64_t ns2 = (int64_t)m_nside * m_nside;
    *bighp = (int)(ipix / ns2);
    int64_t index = ipix % ns2;
    int xv = 0, yv = 0;
    for (int i = 0; i < 32; i++) {
        xv |= (int)((index & 0x1) << i);
        index >>= 1;
        yv |= (int)((index & 0x1) << i);
        index >>= 1;
        if (!index) break;
    }
    *x = xv;
    *y = yv;
}

// ============================================================================
// RING scheme: 按 ring 索引
// ring 1..Ns-1:    北极冠 (每环 4*ring 像素)
// ring Ns..3*Ns:   赤道带 (每环 4*Ns 像素)
// ring 3*Ns+1..4*Ns-1: 南极冠 (环 r 有 4*(4Ns-r) 像素)
// ============================================================================

// ring + longind(环内索引) → RING 像素号
int64_t HealpixCore::ring2pix(int ring, int longind) const {
    int Ns = m_nside;
    if (ring <= Ns) {
        // 北极冠
        return (int64_t)ring * (ring - 1) * 2 + longind;
    }
    if (ring < 3 * Ns) {
        // 赤道带
        return (int64_t)Ns * (Ns - 1) * 2 + (int64_t)Ns * 4 * (ring - Ns) + longind;
    }
    // 南极冠
    int ri = 4 * Ns - ring;
    return (int64_t)12 * Ns * Ns - 1 - ((int64_t)ri * (ri - 1) * 2 + (ri * 4 - 1 - longind));
}

// RING 像素号 → (ring, longind)
void HealpixCore::pix2ring(int64_t ipix, int* ring, int* phi_idx) const {
    int Ns = m_nside;
    int64_t hp = ipix;
    int64_t offset = 0;
    int r;
    for (r = 1; r <= Ns; r++) {
        if (offset + (int64_t)r * 4 > hp) {
            *ring = r; *phi_idx = (int)(hp - offset); return;
        }
        offset += (int64_t)r * 4;
    }
    for (; r < 3 * Ns; r++) {
        if (offset + (int64_t)Ns * 4 > hp) {
            *ring = r; *phi_idx = (int)(hp - offset); return;
        }
        offset += (int64_t)Ns * 4;
    }
    for (; r < 4 * Ns; r++) {
        int ri = 4 * Ns - r;
        if (offset + (int64_t)ri * 4 > hp) {
            *ring = r; *phi_idx = (int)(hp - offset); return;
        }
        offset += (int64_t)ri * 4;
    }
    *ring = -1; *phi_idx = -1;
}

// (bighp, x, y) → RING 像素号
// 改编自 astrometry.net healpix_xy_to_ring
int64_t HealpixCore::xy2ring(int bighp, int x, int y) const {
    int Ns = m_nside;
    int frow = bighp / 4;
    int F1 = frow + 2;
    int v = x + y;
    // ring 从北极=1 到南极=4Ns-1
    int ring = F1 * Ns - v - 1;
    if (ring < 1 || ring >= 4 * Ns) {
        fprintf(stderr, "[healpix][core] xy2ring: 无效 ring=%d (bighp=%d x=%d y=%d)\n",
                ring, bighp, x, y);
        return -1;
    }

    int64_t index;
    if (ring <= Ns) {
        // 北极冠
        index = (Ns - 1 - y);
        index += (int64_t)(bighp % 4) * ring;
        index += (int64_t)ring * (ring - 1) * 2;
    } else if (ring >= 3 * Ns) {
        // 南极冠
        int ri = 4 * Ns - ring;
        index = (ri - 1) - x;
        index += (int64_t)(3 - (bighp % 4)) * ri;
        index += (int64_t)ri * (ri - 1) * 2;
        index = (int64_t)12 * Ns * Ns - 1 - index;
    } else {
        // 赤道带
        int s = (ring - Ns) % 2;
        int F2 = 2 * (bighp % 4) - (frow % 2) + 1;
        int h = x - y;
        index = ((int64_t)F2 * Ns + h + s) / 2;
        index += (int64_t)Ns * (Ns - 1) * 2;                 // 北极冠总像素偏移
        index += (int64_t)Ns * 4 * (ring - Ns);              // 赤道带环偏移
        // healpix #4 的环绕修正
        if (bighp == 4 && y > x) {
            index += (4 * Ns - 1);
        }
    }
    return index;
}

// RING 像素号 → (bighp, x, y)
// 改编自 astrometry.net healpix_ring_to_xy (经 decompose_ring)
void HealpixCore::ring2xy(int64_t ipix, int* bighp, int* x, int* y) const {
    int Ns = m_nside;
    int ring, longind;
    pix2ring(ipix, &ring, &longind);

    if (ring <= Ns) {
        // 北极冠
        int ind = longind - (longind / ring) * ring;
        int bh  = longind / ring;
        int yy  = Ns - 1 - ind;
        int frow = bh / 4;
        int F1 = frow + 2;
        int vv = F1 * Ns - ring - 1;
        int xx = vv - yy;
        *bighp = bh; *x = xx; *y = yy;
    } else if (ring < 3 * Ns) {
        // 赤道带
        int panel = longind / Ns;
        int ind   = longind % Ns;
        bool bottomleft = ind < (ring - Ns + 1) / 2;
        bool topleft    = ind < (3 * Ns - ring + 1) / 2;
        int bh;
        int R = 0;
        if (!bottomleft && topleft) {
            bh = panel;
        } else if (bottomleft && !topleft) {
            bh = 8 + panel;
        } else if (bottomleft && topleft) {
            bh = 4 + panel;
        } else {
            bh = 4 + (panel + 1) % 4;
            if (bh == 4) {
                longind -= (4 * Ns - 1);
                R = 1;
            }
        }
        int frow = bh / 4;
        int F1 = frow + 2;
        int F2 = 2 * (bh % 4) - (frow % 2) + 1;
        int s = (ring - Ns) % 2;
        int vv = F1 * Ns - ring - 1;
        int hh = 2 * longind - s - F2 * Ns;
        if (R) hh--;
        int xx = (vv + hh) / 2;
        int yy = (vv - hh) / 2;
        // 一致性修正
        if (vv != (xx + yy) || hh != (xx - yy)) {
            hh++;
            xx = (vv + hh) / 2;
            yy = (vv - hh) / 2;
        }
        *bighp = bh; *x = xx; *y = yy;
    } else {
        // 南极冠
        int ri = 4 * Ns - ring;
        int bh = 8 + longind / ri;
        int ind = longind - (bh % 4) * ri;
        int yy = (ri - 1) - ind;
        int frow = bh / 4;
        int F1 = frow + 2;
        int vv = F1 * Ns - ring - 1;
        int xx = vv - yy;
        *bighp = bh; *x = xx; *y = yy;
    }
}

// NESTED ↔ RING 转换 (经 XY 中间表示)
int64_t HealpixCore::nest2ring(int64_t inest) const {
    int bh, x, y;
    nest2xy(inest, &bh, &x, &y);
    return xy2ring(bh, x, y);
}

int64_t HealpixCore::ring2nest(int64_t iring) const {
    int bh, x, y;
    ring2xy(iring, &bh, &x, &y);
    return xy2nest(bh, x, y);
}

// 按当前 scheme: 像素 → (bighp, x, y)
void HealpixCore::pix2xy(int64_t ipix, int* bighp, int* x, int* y) const {
    if (m_nested) nest2xy(ipix, bighp, x, y);
    else          ring2xy(ipix, bighp, x, y);
}

// 按当前 scheme: (bighp, x, y) → 像素
int64_t HealpixCore::xy2pix(int bighp, int x, int y) const {
    if (m_nested) return xy2nest(bighp, x, y);
    else          return xy2ring(bighp, x, y);
}

// ============================================================================
// 公共 API: ang ↔ pix
// ============================================================================
int64_t HealpixCore::ang2pix(double theta_rad, double phi_rad) const {
    int bh, x, y;
    ang2xy(theta_rad, phi_rad, &bh, &x, &y);
    return xy2pix(bh, x, y);
}

void HealpixCore::pix2ang(int64_t ipix, double* theta_rad, double* phi_rad) const {
    int bh, x, y;
    pix2xy(ipix, &bh, &x, &y);
    xy2ang(bh, x, y, theta_rad, phi_rad);
}

// RA/Dec(度) → (theta, phi): theta = π/2 - dec, phi = ra
int64_t HealpixCore::radec2pix(double ra_deg, double dec_deg) const {
    double theta = HALF_PI - dec_deg * PI / 180.0;
    double phi   = ra_deg * PI / 180.0;
    return ang2pix(theta, phi);
}

void HealpixCore::pix2radec(int64_t ipix, double* ra_deg, double* dec_deg) const {
    double theta, phi;
    pix2ang(ipix, &theta, &phi);
    *dec_deg = (HALF_PI - theta) * 180.0 / PI;
    *ra_deg  = phi * 180.0 / PI;
    if (*ra_deg < 0.0)  *ra_deg += 360.0;
    if (*ra_deg >= 360.0) *ra_deg -= 360.0;
}

// ============================================================================
// 邻居查询
// 改编自 astrometry.net get_neighbours / healpix_get_neighbour
// 返回 8 邻居 (边界/角点处可能少于 8 个, 用 -1 占位后过滤)
// ============================================================================

// 大像素(base healpix)邻居方向查询: 返回 -1 表示无邻居
static int baseNeighbour(int hp, int dx, int dy) {
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
        // 赤道带 4..7
        if (dx ==  1 && dy ==  0) return hp - 4;
        if (dx ==  0 && dy ==  1) return (hp + 3) % 4;
        if (dx == -1 && dy ==  0) return 8 + ((hp + 3) % 4);
        if (dx ==  0 && dy == -1) return hp + 4;
        if (dx ==  1 && dy == -1) return 4 + ((hp + 1) % 4);
        if (dx == -1 && dy ==  1) return 4 + ((hp - 1) % 4);
        return -1;
    }
}

std::vector<int64_t> HealpixCore::neighbors(int64_t ipix) const {
    std::vector<int64_t> result;
    int Ns = m_nside;
    int base, x, y;
    pix2xy(ipix, &base, &x, &y);

    bool nPol = (base <= 3);
    bool sPol = (base >= 8);

    // 8 个方向: (+0), (++), (0+), (-+), (-0), (--), (0-), (+-)
    struct Dir { int dx, dy; };
    Dir dirs[8] = {
        { 1, 0}, { 1, 1}, { 0, 1}, {-1, 1},
        {-1, 0}, {-1,-1}, { 0,-1}, { 1,-1}
    };

    for (int d = 0; d < 8; d++) {
        int dx = dirs[d].dx, dy = dirs[d].dy;
        int nx = x + dx, ny = y + dy;
        int nbase = base;

        if (nx >= 0 && nx < Ns && ny >= 0 && ny < Ns) {
            // 同一 base 内
        } else {
            // 跨越 base 边界
            bool atRight  = (x == Ns - 1);
            bool atLeft   = (x == 0);
            bool atTop    = (y == Ns - 1);
            bool atBottom = (y == 0);

            bool corner = ((dx != 0) && (dy != 0) &&
                           ((atRight && atTop) || (atRight && atBottom) ||
                            (atLeft  && atTop) || (atLeft  && atBottom)));
            bool edgeX  = (dx != 0 && ((atRight && dx > 0) || (atLeft && dx < 0)));
            bool edgeY  = (dy != 0 && ((atTop  && dy > 0) || (atBottom && dy < 0)));

            // 对角跨越只在两极 base 有效, 赤道带对角无邻居
            if (corner) {
                if (nPol || sPol) {
                    nbase = baseNeighbour(base, dx, dy);
                } else {
                    continue;  // 赤道带对角无邻居
                }
            } else if (edgeX && edgeY) {
                continue;
            } else if (edgeX) {
                nbase = baseNeighbour(base, dx, 0);
            } else if (edgeY) {
                nbase = baseNeighbour(base, 0, dy);
            } else {
                continue;
            }
            if (nbase < 0) continue;

            // 重新计算 nx, ny (考虑极区坐标翻转)
            nx = (x + dx + Ns) % Ns;
            ny = (y + dy + Ns) % Ns;
            // 极区边缘跨越时坐标需修正
            if (nPol) {
                if (atRight && dx > 0) { nx = Ns - 1; std::swap(nx, ny); }
                else if (atTop && dy > 0) { ny = Ns - 1; std::swap(nx, ny); }
            } else if (sPol) {
                if (atLeft && dx < 0) { nx = 0; std::swap(nx, ny); }
                else if (atBottom && dy < 0) { ny = 0; std::swap(nx, ny); }
            }
            // 非极区 edgeX/edgeY 已正确取模
        }
        nx = clampi(nx, 0, Ns - 1);
        ny = clampi(ny, 0, Ns - 1);
        result.push_back(xy2pix(nbase, nx, ny));
    }
    return result;
}

// ============================================================================
// queryDisc: 查询天区圆盘内的所有像素
// 算法: 从中心像素 BFS 扩展邻居, 用大圆距离判定
// ============================================================================
std::vector<int64_t> HealpixCore::queryDisc(double ra_deg, double dec_deg,
                                            double radius_arcsec) const {
    std::vector<int64_t> result;
    double radius_rad = radius_arcsec * ARCSEC_TO_RAD;

    // 中心 (x,y,z) 单位向量
    double decR = dec_deg * PI / 180.0;
    double raR  = ra_deg  * PI / 180.0;
    double cdec = std::cos(decR);
    double cx = cdec * std::cos(raR);
    double cy = cdec * std::sin(raR);
    double cz = std::sin(decR);

    int64_t center = radec2pix(ra_deg, dec_deg);

    // BFS
    std::set<int64_t> visited;
    std::vector<int64_t> queue;
    queue.push_back(center);
    visited.insert(center);

    while (!queue.empty()) {
        std::vector<int64_t> next;
        for (int64_t ipix : queue) {
            // 计算像素中心的大圆距离
            double t, p;
            pix2ang(ipix, &t, &p);
            double st = std::sin(t);
            double px = st * std::cos(p);
            double py = st * std::sin(p);
            double pz = std::cos(t);
            double cosd = px * cx + py * cy + pz * cz;
            if (cosd > 1.0) cosd = 1.0;
            if (cosd < -1.0) cosd = -1.0;
            double dist = std::acos(cosd);

            if (dist <= radius_rad) {
                result.push_back(ipix);
                // 扩展邻居
                auto nbrs = neighbors(ipix);
                for (int64_t nb : nbrs) {
                    if (visited.find(nb) == visited.end()) {
                        visited.insert(nb);
                        next.push_back(nb);
                    }
                }
            }
            // dist > radius 的像素不扩展 (剪枝)
        }
        queue.swap(next);
    }

    // 排序输出
    std::sort(result.begin(), result.end());
    return result;
}

// ============================================================================
// nside 转换 (LOD)
// ============================================================================

// 当前 nside 像素 → 粗 nside 像素
// 用 (bighp, x, y) 按比例缩放
int64_t HealpixCore::pixelToCoarse(int64_t ipix_fine, int nside_coarse) const {
    int bh, fx, fy;
    pix2xy(ipix_fine, &bh, &fx, &fy);
    // 像素中心归一化坐标
    double fxn = (fx + 0.5) / m_nside;
    double fyn = (fy + 0.5) / m_nside;
    int cx = clampi((int)std::floor(fxn * nside_coarse), 0, nside_coarse - 1);
    int cy = clampi((int)std::floor(fyn * nside_coarse), 0, nside_coarse - 1);
    // 粗像素 scheme 与当前一致 (用 NESTED 表达, 保证 bighp 不变)
    HealpixCore coarse(nside_coarse, m_nested);
    return coarse.xy2pix(bh, cx, cy);
}

// 粗 nside 像素 → 当前 nside 的所有子像素
std::vector<int64_t> HealpixCore::pixelToFine(int64_t ipix_coarse, int nside_fine) const {
    std::vector<int64_t> result;
    int bh, cx, cy;
    // ipix_coarse 是在 nside_coarse (=当前 m_nside) 下的像素
    pix2xy(ipix_coarse, &bh, &cx, &cy);
    HealpixCore fine(nside_fine, m_nested);
    int ratio = nside_fine / m_nside;
    if (ratio < 1) ratio = 1;
    for (int dx = 0; dx < ratio; dx++) {
        for (int dy = 0; dy < ratio; dy++) {
            int fx = cx * ratio + dx;
            int fy = cy * ratio + dy;
            if (fx < nside_fine && fy < nside_fine) {
                result.push_back(fine.xy2pix(bh, fx, fy));
            }
        }
    }
    return result;
}

} // namespace healpix
