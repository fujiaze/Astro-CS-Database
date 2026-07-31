// ============================================================================
// 球面 HEALPix 重叠计算模块实现 (WP-D 步骤3-4)
//
// 替换 drizzle_engine.cpp 中的局部切平面近似 + 人工 HEALPix 菱形近似,
// 实现真实球面多边形裁剪与球面面积计算.
//
// 实现要点:
//   - float64 内部精度 (double)
//   - 球面面积用 Girard 定理: Area = Σ内角 - (n-2)π
//   - 球面 Sutherland-Hodgman: 大圆弧裁剪, 保留法向量正侧
//   - HEALPix 边界: 4 角顶点 (xy2ang), 处理赤道带菱形与极区三角形
//   - 候选像素查询: drop 多边形球面包围圆 + queryDisc
// ============================================================================

#include "spherical_overlap.h"

#include <cstdio>
#include <cmath>
#include <algorithm>
#include <unordered_set>

namespace spherical {

// ============================================================================
// 常量
// ============================================================================
static const double PI      = 3.14159265358979323846;
static const double TWO_PI  = 2.0 * PI;
static const double HALF_PI = PI / 2.0;
static const double DEG2RAD = PI / 180.0;
static const double RAD2DEG = 180.0 / PI;
static const double ARCSEC_TO_RAD = PI / (180.0 * 3600.0);
static const double RAD_TO_ARCSEC = 180.0 * 3600.0 / PI;

// ============================================================================
// 前向声明: xyf2ang 副本 (避开 healpix_core.h private 方法限制)
// 复制自 healpix_core.cpp 的 xy2ang 实现, 接受浮点像素坐标 (xf, yf)
// 像素中心: xf=x+0.5, yf=y+0.5; 像素角点: xf=x, yf=y (整数)
// ============================================================================
static void xyf2ang_replica(int bighp, double xf, double yf, int Ns,
                            double* theta, double* phi);

// ============================================================================
// 基本向量运算
// ============================================================================
Vec3 cross(const Vec3& a, const Vec3& b) {
    Vec3 r;
    r.x = a.y * b.z - a.z * b.y;
    r.y = a.z * b.x - a.x * b.z;
    r.z = a.x * b.y - a.y * b.x;
    return r;
}

double dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

double length(const Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

Vec3 normalize(const Vec3& v) {
    double len = length(v);
    if (len < 1e-300) {
        // 退化向量, 返回北极作为安全默认
        return {0.0, 0.0, 1.0};
    }
    double inv = 1.0 / len;
    return {v.x * inv, v.y * inv, v.z * inv};
}

Vec3 radec_to_vec(double ra_deg, double dec_deg) {
    double ra  = ra_deg  * DEG2RAD;
    double dec = dec_deg * DEG2RAD;
    double cd = std::cos(dec);
    return { cd * std::cos(ra), cd * std::sin(ra), std::sin(dec) };
}

void vec_to_radec(const Vec3& v, double& ra_deg, double& dec_deg) {
    Vec3 u = normalize(v);
    double dec = std::asin(std::max(-1.0, std::min(1.0, u.z)));
    double ra  = std::atan2(u.y, u.x);
    if (ra < 0.0) ra += TWO_PI;
    if (ra >= TWO_PI) ra -= TWO_PI;
    dec_deg = dec * RAD2DEG;
    ra_deg  = ra  * RAD2DEG;
}

double angular_distance(const Vec3& a, const Vec3& b) {
    double d = dot(a, b);
    d = std::max(-1.0, std::min(1.0, d));
    return std::acos(d);
}

// ============================================================================
// 球面多边形面积 (Girard 定理)
//
// 球面 excess = Σ内角 - (n-2)π
// 内角在顶点 B 处, 相邻顶点 A, C:
//   - na = normalize(cross(B, A))   // 大圆 BA 的极向量 (法向量)
//   - nb = normalize(cross(B, C))   // 大圆 BC 的极向量
//   - 内角 = acos(dot(na, nb))      // ∈ [0, π], 无符号二面角
//
// 注: 对凸球面多边形 (HEALPix 像素, drop 多边形, 两者交集), 内角 ∈ [0, π],
//     二面角 = 内角. 用 acos 避免顶点顺序导致的符号问题.
//     最终面积 = |excess|, 对 excess ∈ [0, 2π] 的凸多边形直接正确.
// ============================================================================
double spherical_polygon_area(const std::vector<Vec3>& vertices) {
    int n = (int)vertices.size();
    if (n < 3) return 0.0;

    double angle_sum = 0.0;
    for (int i = 0; i < n; i++) {
        // 当前顶点 B, 前一顶点 A, 后一顶点 C
        const Vec3& B = vertices[i];
        const Vec3& A = vertices[(i - 1 + n) % n];
        const Vec3& C = vertices[(i + 1)     % n];

        // 大圆 BA / BC 的极向量 (法向量)
        Vec3 na = normalize(cross(B, A));
        Vec3 nb = normalize(cross(B, C));

        // 球面内角 (无符号, ∈ [0, π])
        double cos_angle = dot(na, nb);
        cos_angle = std::max(-1.0, std::min(1.0, cos_angle));
        double angle = std::acos(cos_angle);
        angle_sum += angle;
    }

    // 球面 excess = Σ内角 - (n-2)π
    double excess = angle_sum - (n - 2) * PI;
    return std::fabs(excess);
}

// ============================================================================
// 球面 Sutherland-Hodgman 多边形裁剪
//
// 对每个裁剪平面法向量 n (保留 dot(n, v) >= 0 一侧):
//   遍历 subject 的每条边 (S → E):
//     - 计算 S, E 是否在内侧 (dot(n, S) >= 0 / dot(n, E) >= 0)
//     - 若跨越边界, 计算交点 I
//     - 按经典 S-H 规则输出
//
// 球面交点:
//   - 边 (S → E) 所在大圆的法向量 n_edge = cross(S, E)
//   - 裁剪大圆的法向量 n_clip = n
//   - 两大圆交点 = ±normalize(cross(n_edge, n_clip))
//   - 选择 dot(I, S+E) > 0 的那个 (位于 S, E 之间)
// ============================================================================
static inline bool is_inside(const Vec3& v, const Vec3& n) {
    // dot(n, v) >= 0 表示在保留侧 (含边界)
    return dot(n, v) >= -1e-15;
}

static Vec3 compute_intersection(const Vec3& S, const Vec3& E, const Vec3& n_clip) {
    // 边 (S→E) 所在大圆法向量
    Vec3 n_edge = cross(S, E);
    // 两大圆交点 (两个候选)
    Vec3 cross_nc = cross(n_edge, n_clip);
    Vec3 I = normalize(cross_nc);

    // 选择位于 S, E 之间的那个 (dot(I, S+E) > 0)
    if (dot(I, S) + dot(I, E) < 0.0) {
        I.x = -I.x; I.y = -I.y; I.z = -I.z;
    }
    return I;
}

std::vector<Vec3> sutherland_hodgman_spherical(
    const std::vector<Vec3>& subject,
    const std::vector<Vec3>& clip_plane_normals)
{
    if (subject.size() < 3) return {};
    if (clip_plane_normals.empty()) return subject;

    std::vector<Vec3> output = subject;

    for (const Vec3& n : clip_plane_normals) {
        if (output.empty()) break;

        Vec3 nrm = normalize(n);
        std::vector<Vec3> input = output;
        output.clear();
        output.reserve(input.size() + 1);

        int m = (int)input.size();
        for (int i = 0; i < m; i++) {
            const Vec3& S = input[(i - 1 + m) % m];  // 前一顶点 (S-H 经典: S=E_prev)
            const Vec3& E = input[i];                 // 当前顶点
            // 注: Sutherland-Hodgman 经典实现遍历边 (S, E) = (input[i-1], input[i])
            // 这里改写为更直观的 (S, E) = (current, next) 等价形式, 但保持顶点遍历顺序一致
            bool S_in = is_inside(S, nrm);
            bool E_in = is_inside(E, nrm);

            if (E_in) {
                if (!S_in) {
                    // S 外 E 内: 输出交点 I, 再输出 E
                    output.push_back(compute_intersection(S, E, nrm));
                }
                output.push_back(E);
            } else {
                if (S_in) {
                    // S 内 E 外: 只输出交点 I
                    output.push_back(compute_intersection(S, E, nrm));
                }
                // S 外 E 外: 不输出
            }
        }

        // 退化检查: 顶点数 < 3 表示无有效交集
        if (output.size() < 3) {
            output.clear();
            break;
        }
    }

    return output;
}

// ============================================================================
// 获取 HEALPix 像素的球面边界顶点
//
// 通过 healpix_core 的 pix2xy 得到 (bighp, x, y), 然后 4 个角点:
//   C0 = (x,   y),   C1 = (x+1, y),   C2 = (x+1, y+1),   C3 = (x,   y+1)
// 用 xy2ang 转换为 (theta, phi), 再转 (ra, dec), 再转 Vec3.
//
// 顺序: C0→C1→C2→C3, 在 (x,y) 平面上是逆时针, 在球面上保持一致方向.
// 极区像素一角可能退化 (如北极 bighp 0..3 的 C3 角), 但仍返回 4 个顶点 (退化角会
// 重合, 不影响凸性).
// ============================================================================
std::vector<Vec3> get_healpix_boundary(
    const healpix::HealpixCore& hp, uint64_t ipix, int nside)
{
    (void)nside;  // 使用 hp.getNside(), 参数保留接口一致性
    std::vector<Vec3> boundary;
    boundary.reserve(4);

    // 通过 pix2xy 获取 (bighp, x, y)
    // healpix_core.h 的 pix2xy 是 private, 但暴露了 pix2ang
    // 用 xy2ang 公开版本需要 bighp/x/y, 这里直接使用公开 API:
    // 通过 pix2ang 获取 4 个角点. 但 pix2ang 只返回像素中心.
    //
    // 替代方案: 用 nest2xy + xy2ang. 但 nest2xy 是 private.
    // 公开 API 中只有 pix2ang (中心). 需要自行构造 4 角.
    //
    // 解决方案: 借助 hp.neighbors + 中心位置反推 4 角, 或直接用
    // 公开 API 计算 4 角的近似 (中心 +/- 半分辨率).
    //
    // 精确方案: 用 pix2radec 得到中心 (ra_c, dec_c), 然后用
    // 4 个相邻像素的中心位置构造角点. 但这会引入额外误差.
    //
    // 最稳妥方案: 直接调用 healpix_core 的内部 xy2ang. 由于该方法是 private,
    // 这里通过友元或在 spherical_overlap.cpp 中重新实现 xy2ang.
    // 为避免修改 healpix_core.h, 这里重新实现 HEALPix 像素角的计算.
    //
    // 参考 healpix_core.cpp 的 xy2ang 实现 (Gorski 2005):
    //   - 北极 base 0..3, x+y > Ns 时为极冠
    //   - 南极 base 8..11, x+y < Ns 时为极冠
    //   - 其他为赤道带
    // 重写 pix2xyCorner(bighp, x, y, nside) → (theta, phi)
    //
    // 但 pix2xy 是 private, 不能直接调用. 这里通过 neighbors + 中心反推.
    //
    // 实际方案: 重新实现 nest2xy (NESTED 解码) + xy2ang (角点计算).
    // 这是 healpix_core.cpp 中已有算法的副本, 仅用于本模块.

    int Ns = hp.getNside();
    int64_t npix_per_bighp = (int64_t)Ns * Ns;
    int bighp = (int)(ipix / npix_per_bighp);
    int64_t local_idx = ipix % npix_per_bighp;

    // NESTED 解码: x 位填偶数位, y 位填奇数位
    int xv = 0, yv = 0;
    {
        int64_t idx = local_idx;
        for (int i = 0; i < 32; i++) {
            xv |= (int)((idx & 0x1) << i);
            idx >>= 1;
            yv |= (int)((idx & 0x1) << i);
            idx >>= 1;
            if (!idx) break;
        }
    }

    // 4 个角点 (像素角, 不是像素中心)
    // C0 = (x,   y),   C1 = (x+1, y),   C2 = (x+1, y+1),   C3 = (x,   y+1)
    struct CornerXY { int x, y; };
    CornerXY corners[4] = {
        {xv,     yv    },
        {xv + 1, yv    },
        {xv + 1, yv + 1},
        {xv,     yv + 1}
    };

    for (int i = 0; i < 4; i++) {
        double theta, phi;
        // 调用 xyf2ang_replica 计算角点 (整数坐标直接作为浮点 xf/yf, 不加 0.5)
        xyf2ang_replica(bighp, (double)corners[i].x, (double)corners[i].y, Ns, &theta, &phi);
        double dec = (HALF_PI - theta) * RAD2DEG;
        double ra  = phi * RAD2DEG;
        if (ra < 0.0)  ra += 360.0;
        if (ra >= 360.0) ra -= 360.0;
        boundary.push_back(radec_to_vec(ra, dec));
    }

    return boundary;
}

// ============================================================================
// xyf2ang 副本 (避开 healpix_core.h private 方法限制)
//
// 复制自 healpix_core.cpp 的 xy2ang 实现, 接受浮点像素坐标 (xf, yf).
// 像素中心: xf=x+0.5, yf=y+0.5; 像素角点: xf=x, yf=y (整数).
// 仅用于 get_healpix_boundary 内部计算像素角点 (像素中心版已在 hp.pix2ang 提供).
// ============================================================================
static void xyf2ang_replica(int bighp, double xf, double yf, int Ns,
                            double* theta, double* phi)
{
    int chp = bighp;
    // xf, yf 直接作为浮点像素坐标 (调用方已决定是中心还是角点)

    bool equatorial = true;
    double zfactor = 1.0;

    if (chp <= 3) {
        if (xf + yf > Ns) { equatorial = false; zfactor = 1.0; }
    }
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
        } else {
            phioff = 1.0;
            zoff = -2.0;
            chp -= 8;
        }
        z  = (2.0 / 3.0) * (xn + yn + zoff);
        ph = (PI / 4.0) * (xn - yn + phioff + 2.0 * chp);
    } else {
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
            z = 1.0 - (PI * (Ns - xx) / ((2.0 * phi_t - PI) * Ns))
                     * (PI * (Ns - xx) / ((2.0 * phi_t - PI) * Ns)) / 3.0;
        } else {
            z = 1.0 - (PI * (Ns - yy) / (2.0 * phi_t * Ns))
                     * (PI * (Ns - yy) / (2.0 * phi_t * Ns)) / 3.0;
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
// 计算源像素 drop 与目标 HEALPix 像素的球面重叠面积
// ============================================================================
double compute_overlap_area(
    const std::vector<Vec3>& drop_corners,
    const healpix::HealpixCore& hp, uint64_t target_ipix)
{
    if (drop_corners.size() < 3) return 0.0;

    // 1. 获取目标 HEALPix 像素边界 (4 角, 单位向量)
    std::vector<Vec3> hp_boundary = get_healpix_boundary(hp, target_ipix, hp.getNside());
    if (hp_boundary.size() < 3) return 0.0;

    // 2. 构造裁剪平面法向量 (每条边一个大圆, 指向像素内部)
    //    对于逆时针顺序的顶点 v0→v1→v2→v3:
    //      边 (v_i → v_{i+1}) 的大圆法向量 n = cross(v_i, v_{i+1})
    //      内部点 P 满足 dot(n, P) > 0 (因为内部在边的左侧)
    //    HEALPix 像素中心必然在所有裁剪平面的正侧, 用此验证方向
    Vec3 hp_center = {0.0, 0.0, 0.0};
    for (const Vec3& v : hp_boundary) {
        hp_center.x += v.x; hp_center.y += v.y; hp_center.z += v.z;
    }
    hp_center = normalize(hp_center);

    std::vector<Vec3> clip_normals;
    clip_normals.reserve(hp_boundary.size());
    int nb = (int)hp_boundary.size();
    for (int i = 0; i < nb; i++) {
        const Vec3& A = hp_boundary[i];
        const Vec3& B = hp_boundary[(i + 1) % nb];
        Vec3 n = cross(A, B);
        // 确保法向量指向像素内部 (与中心同侧)
        if (dot(n, hp_center) < 0.0) {
            n.x = -n.x; n.y = -n.y; n.z = -n.z;
        }
        // 法向量无需单位化 (is_inside 用符号判断), 但单位化更稳定
        n = normalize(n);
        clip_normals.push_back(n);
    }

    // 3. 球面 Sutherland-Hodgman 裁剪
    std::vector<Vec3> intersection = sutherland_hodgman_spherical(drop_corners, clip_normals);
    if (intersection.size() < 3) return 0.0;

    // 4. 用 Girard 定理计算交集面积
    return spherical_polygon_area(intersection);
}

// ============================================================================
// 查询与 drop 多边形可能相交的所有 HEALPix 像素 (不限于 1-ring)
// ============================================================================
void query_candidate_pixels(
    const std::vector<Vec3>& drop_corners,
    const healpix::HealpixCore& hp,
    std::vector<uint64_t>& candidates)
{
    candidates.clear();
    if (drop_corners.empty()) return;

    // 1. 计算 drop 多边形的球面包围圆
    //    中心 = 所有顶点向量的平均 (归一化)
    //    半径 = 最大顶点到中心的角距离
    Vec3 center = {0.0, 0.0, 0.0};
    for (const Vec3& v : drop_corners) {
        center.x += v.x; center.y += v.y; center.z += v.z;
    }
    center = normalize(center);

    double max_angle = 0.0;
    for (const Vec3& v : drop_corners) {
        double ang = angular_distance(v, center);
        if (ang > max_angle) max_angle = ang;
    }

    // 2. 加缓冲: 1.5 倍 HEALPix 像素角分辨率 (确保不漏选相邻像素)
    //    HEALPix 像素分辨率 (角秒) = sqrt(4π/(12*nside²)) * 206265
    //    缓冲取 1.5 倍像素对角线半长, 安全覆盖像素边界
    double hp_res_arcsec = hp.pixelResolutionArcsec();
    double hp_res_rad    = hp_res_arcsec * ARCSEC_TO_RAD;
    double buffer_rad     = 1.5 * hp_res_rad;
    double query_radius_rad = max_angle + buffer_rad;

    // 3. 中心向量转 RA/Dec
    double ra_c, dec_c;
    vec_to_radec(center, ra_c, dec_c);

    // 4. 用 hp.queryDisc 查询圆盘内所有像素
    double query_radius_arcsec = query_radius_rad * RAD_TO_ARCSEC;
    std::vector<int64_t> disc_pixels = hp.queryDisc(ra_c, dec_c, query_radius_arcsec);

    // 5. 输出候选列表 (uint64_t, 去重已由 queryDisc 内部保证)
    std::unordered_set<uint64_t> seen;
    seen.reserve(disc_pixels.size());
    for (int64_t ipix : disc_pixels) {
        if (ipix < 0) continue;
        uint64_t upix = (uint64_t)ipix;
        if (seen.insert(upix).second) {
            candidates.push_back(upix);
        }
    }
    std::sort(candidates.begin(), candidates.end());
}

} // namespace spherical
