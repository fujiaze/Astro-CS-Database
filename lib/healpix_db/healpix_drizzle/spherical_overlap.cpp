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
// 球面多边形面积 (R06-B05: 球面三角剖分 + Eriksson 稳定公式)
//
// R06-B01/B05 根因:
//   R05 的双路径 (小多边形切平面鞋带 / 大多边形 Girard) 仍有缺陷:
//   1. 极区 1° 像素是大边形, 走 Girard 路径, 但内角和 ≈ n×(π/2) = 2π,
//      (n-2)π = 2π, excess ≈ 0, catastrophic cancellation → 通量爆炸 303305×
//   2. Girard 切向量 t = A - (A·B)·B 在 A·B≈1 时有效数字丢失 (R05-B01 已识别)
//   3. 双路径切换阈值 60" 无数学依据, 1° 像素 (3600") 远超阈值走 Girard 失效
//
// 修复 (R06-B05):
//   统一使用球面三角剖分 (fan triangulation) + Eriksson (2018) 稳定公式:
//   1. fan triangulation 以 V_0 为顶点: 三角形 (V_0, V_i, V_{i+1}), i=1..n-2
//   2. 每个球面三角形有向面积 = 2·atan2(det, 1 + a·b + b·c + c·a)
//      其中 det = a · (b × c) (标量三重积, 含符号)
//   3. 累加有向面积 (det<0 时三角形反向, 贡献负值)
//
//   R06-B05 修正: 原实现用 center 做扇出且 det<0 时取补面积 (4π-area),
//   这在 center 不在多边形内部时产生系统性误差. 改用 V_0 扇出 + 有符号累加,
//   对凸多边形 (S-H 裁剪结果) 总是正确, 无需依赖 center 位置.
//
//   Eriksson 公式在所有退化情况下数值稳定:
//   - 极小三角形: 分子分母同比缩小, 比值正确
//   - 半球大小三角形: 分母→0, atan2 仍稳定
//   - 共线顶点: 分子=0, 面积=0
//   - 极区大像素: 无 excess≈0 的相消问题
//
// 参考: Eriksson, F. (2018) "The ang... spherical triangle area formula"
// ============================================================================
double spherical_polygon_area(const std::vector<Vec3>& vertices) {
    int n = (int)vertices.size();
    if (n < 3) return 0.0;

    // ---- fan triangulation 以 V_0 为顶点 + Eriksson 有符号面积 ----
    // 对凸多边形, V_0 与所有非相邻顶点构成同向三角形, 有符号累加得到正确面积.
    // 对非凸多边形, 此方法仍正确 (标准球面多边形面积定义).
    const Vec3& a = vertices[0];
    double total_area = 0.0;

    for (int i = 1; i < n - 1; i++) {
        const Vec3& b = vertices[i];
        const Vec3& c = vertices[i + 1];

        // 标量三重积 det = a · (b × c), 含符号
        Vec3 b_cross_c = cross(b, c);
        double det = dot(a, b_cross_c);

        // 分母 = 1 + a·b + b·c + c·a
        double dot_ab = dot(a, b);
        double dot_bc = dot(b, c);
        double dot_ca = dot(c, a);
        double denom = 1.0 + dot_ab + dot_bc + dot_ca;

        // 有符号三角形面积 = 2·atan2(det, denom)
        // det>0: 逆时针三角形, 面积为正
        // det<0: 顺时针三角形, 面积为负 (从总面积中减去)
        // denom<0: 三角形 > 半球, atan2 自动处理
        double tri_area = 2.0 * std::atan2(det, denom);

        total_area += tri_area;
    }

    // 总面积取绝对值 (顶点绕序可能为顺时针或逆时针)
    total_area = std::fabs(total_area);

    // 防御性: 若 > 2π, 多边形覆盖 > 半球, 取补
    if (total_area > 2.0 * PI) {
        total_area = 4.0 * PI - total_area;
    }

    // 防御性: 极小负值归零 (浮点误差)
    if (total_area < 0.0) total_area = 0.0;

    return total_area;
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
// R06-B02: HEALPix 像素边自适应细分辅助函数
//
// 对 HEALPix 像素的一条边 (像素坐标 (x0,y0)->(x1,y1)) 进行自适应二分细分.
// 边在像素坐标系为直线, 但映射到球面后既非大圆弧也非等纬度小圆弧
// (赤道带边在 (z,phi) 空间为线性曲线, 极区边为参数曲线).
//
// 收敛条件 (二选一):
//   1. 球面中点 (xyf2ang_replica 在像素坐标中点) 与大圆弧中点 (normalize(p0+p1))
//      的角偏差 < epsilon_rad → 该段已近似为直线, 取 p0
//   2. 递归深度达 max_depth → 强制截断 (防止无限递归)
//
// 递归二分: 不收敛时, 先细分 [p0, p_mid], 再细分 [p_mid, p1].
// 每个递归节点仅调用 1 次 xyf2ang_replica (中点), 端点复用父节点结果.
//
// 输出: out 追加从 p0 开始的细分顶点 (含 p0, 不含 p1, p1 由相邻边处理).
// ============================================================================
static const int    HP_ADAPTIVE_MAX_DEPTH = 8;      // 临时回退到 R06 深度以排查崩溃
// R08 改进2: HP_ADAPTIVE_EPSILON 改为相对值 (见 subdivide_healpix_edge 内部计算)
//   hp_epsilon = hp_res_rad * 1e-12, 其中 hp_res_rad = sqrt(π/(3·Ns²))
//   NSIDE=64 时 hp_epsilon ≈ 1.6e-14 rad, 每条边细分到 ~256 段
//   R06 实测固定 1e-9 (86 顶点) 会使 S-H 累积误差增大, 但 R08 配合改进1 (解析面积)
//   和改进4 (精确中心) 后, 高细分不再进入 spherical_polygon_area, 累积误差问题消除

static void subdivide_healpix_edge(
    int bighp, int Ns,
    double x0, double y0, const Vec3& p0,
    double x1, double y1, const Vec3& p1,
    int depth,
    std::vector<Vec3>& out)
{
    // R10 修复: HEALPix 边细分阈值从 1e-12 改为 1e-6
    //   根因: hp_res_rad * 1e-12 对非大圆弧的 HEALPix 边永远不收敛.
    //   HEALPix 赤道带等纬度边是小圆 (非大圆弧), 经线边才是大圆弧.
    //   等纬度边的角距离偏差 ≈ sin(dec)*L²/8 (L=hp_res_rad):
    //     NSIDE=65536: L=1.56e-5, 偏差≈1.2e-11, 旧阈值1.56e-17 → 永不收敛
    //     导致每边递归到 MAX_DEPTH=8 (256段/边, 1024顶点/像素),
    //     compute_overlap_area 极慢 (16.2M源像素×20候选×1024三角形=332B次操作).
    //   新阈值 1e-6 (相对值): NSIDE=65536→1.56e-11, 1-2次二分收敛 (2-4段/边);
    //   弧弦误差 < 1e-6*hp_res ≈ 3e-6角秒, 远超 support uint8 量化精度(0.4%).
    //   注: WCS 边 (subdivide_wcs_edge) 用大圆弧平面偏差法 (dafa200 改进5),
    //       因 TAN 投影直线↔大圆弧; HEALPix 边非大圆弧, 用角距离法更合适.
    double hp_res_rad = std::sqrt(PI / (3.0 * (double)Ns * (double)Ns));
    double hp_epsilon = hp_res_rad * 1e-6;

    // 像素坐标中点 → 球面 WCS 中点
    double xm = 0.5 * (x0 + x1);
    double ym = 0.5 * (y0 + y1);
    double thm, phm;
    xyf2ang_replica(bighp, xm, ym, Ns, &thm, &phm);
    double dec_m = (HALF_PI - thm) * RAD2DEG;
    double ra_m  = phm * RAD2DEG;
    if (ra_m < 0.0)  ra_m += 360.0;
    if (ra_m >= 360.0) ra_m -= 360.0;
    Vec3 p_mid_wcs = radec_to_vec(ra_m, dec_m);

    // 大圆弧中点 = normalize(p0 + p1)
    Vec3 p_mid_gc = normalize(Vec3{p0.x + p1.x, p0.y + p1.y, p0.z + p1.z});

    // 角距离法: 反映 HEALPix 边偏离两端点大圆弧的程度
    //   经线边 (大圆弧) → dev≈0, 0次二分; 等纬度边 (小圆) → dev>0, 递归细分
    double dev = angular_distance(p_mid_wcs, p_mid_gc);

    if (dev < hp_epsilon || depth >= HP_ADAPTIVE_MAX_DEPTH) {
        // 收敛: 该段近似为直线, 只输出 p0
        out.push_back(p0);
        return;
    }

    // 未收敛: 递归二分
    subdivide_healpix_edge(bighp, Ns, x0, y0, p0, xm, ym, p_mid_wcs, depth + 1, out);
    subdivide_healpix_edge(bighp, Ns, xm, ym, p_mid_wcs, x1, y1, p1, depth + 1, out);
}

// ============================================================================
// 获取 HEALPix 像素的球面边界顶点 (R06-B02 自适应细分)
//
// 对所有 NSIDE 统一使用自适应边细分策略:
//   - 极区像素和赤道带像素均通过 subdivide_healpix_edge 处理
//   - 不再按 NSIDE<=8 硬切换采样数
//   - 不再假设极区边是大圆弧 (实测极区边为参数曲线, 非大圆弧)
//
// 收敛条件: 球面中点与大圆弧中点偏差 < 1e-6 弧度 (≈0.2角秒), 或深度达 8.
// 每条边最多 2^8=256 段, 但实际收敛远早于此 (低 NSIDE 大像素约 4-16 段,
// 高 NSIDE 小像素约 1-2 段).
//
// 参数 samples_per_edge: 保留接口兼容性, 不再控制精度 (内部自适应决定).
//
// 顶点顺序: C0→(细分点)→C1→(细分点)→C2→(细分点)→C3→(细分点)→(回到 C0)
//   每条边输出含起点不含终点, 总顶点数 = 4 条边的细分段数之和.
// ============================================================================
std::vector<Vec3> get_healpix_boundary_sampled(
    const healpix::HealpixCore& hp, uint64_t ipix, int nside,
    int samples_per_edge)
{
    (void)nside;
    (void)samples_per_edge;  // R06-B02: 自适应细分, 不再依赖固定采样数

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

    // 像素四角 (像素角, 整数坐标)
    // C0=(x,y), C1=(x+1,y), C2=(x+1,y+1), C3=(x,y+1)
    double corners_xy[4][2] = {
        {(double)xv,     (double)yv    },
        {(double)(xv+1), (double)yv    },
        {(double)(xv+1), (double)(yv+1)},
        {(double)xv,     (double)(yv+1)}
    };

    // 辅助 lambda: 像素坐标 (xf,yf) → 球面单位向量
    auto xyf2vec = [&](double xf, double yf) -> Vec3 {
        double theta, phi;
        xyf2ang_replica(bighp, xf, yf, Ns, &theta, &phi);
        double dec = (HALF_PI - theta) * RAD2DEG;
        double ra  = phi * RAD2DEG;
        if (ra < 0.0)  ra += 360.0;
        if (ra >= 360.0) ra -= 360.0;
        return radec_to_vec(ra, dec);
    };

    // 计算 4 个角点的球面向量
    Vec3 corner_vecs[4];
    for (int i = 0; i < 4; i++) {
        corner_vecs[i] = xyf2vec(corners_xy[i][0], corners_xy[i][1]);
    }

    // 对 4 条边自适应细分 (统一处理极区和赤道带)
    std::vector<Vec3> boundary;
    boundary.reserve(32);  // 预估, 自适应实际段数可能更多
    for (int e = 0; e < 4; e++) {
        int en = (e + 1) % 4;
        subdivide_healpix_edge(
            bighp, Ns,
            corners_xy[e][0], corners_xy[e][1], corner_vecs[e],
            corners_xy[en][0], corners_xy[en][1], corner_vecs[en],
            0, boundary);
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
// 构造源像素 drop 球面多边形 (带边采样)
//
// 源像素四角 (pixfrac 收缩后):
//   c0 = (px - half, py - half)  左下
//   c1 = (px + half, py - half)  右下
//   c2 = (px + half, py + half)  右上
//   c3 = (px - half, py + half)  左上
//   half = 0.5 * pixfrac
//
// 每条边采样 samples_per_edge 段 (等分), 每个采样点通过 pixelToSky 回调映射到
// 天球坐标, 再转换为球面单位向量.
//
// 顶点顺序: c0→c1→c2→c3→c0 (逆时针), 每边不含末点 (避免与下一边首点重复),
// 总顶点数 = 4 * samples_per_edge.
//
// samples_per_edge=1: 退化为 4 个角顶点 (t=0 for each edge)
// ============================================================================
std::vector<Vec3> build_drop_polygon_sampled(
    double px, double py, double pixfrac,
    PixelToSkyFn pixelToSky, void* user_data,
    int samples_per_edge)
{
    std::vector<Vec3> result;

    if (samples_per_edge < 1) samples_per_edge = 1;

    // pixfrac 收缩后的四角
    double half = 0.5 * pixfrac;
    double corners[4][2] = {
        {px - half, py - half},  // 0: 左下
        {px + half, py - half},  // 1: 右下
        {px + half, py + half},  // 2: 右上
        {px - half, py + half}   // 3: 左上
    };

    result.reserve((size_t)4 * samples_per_edge);

    // 遍历 4 条边, 每边采样 samples_per_edge 段
    for (int edge = 0; edge < 4; edge++) {
        double x0 = corners[edge][0];
        double y0 = corners[edge][1];
        double x1 = corners[(edge + 1) % 4][0];
        double y1 = corners[(edge + 1) % 4][1];

        for (int s = 0; s < samples_per_edge; s++) {
            double t = (double)s / (double)samples_per_edge;
            double x = x0 + t * (x1 - x0);
            double y = y0 + t * (y1 - y0);

            double ra, dec;
            if (!pixelToSky(x, y, ra, dec, user_data)) {
                return {};  // 投影失败, 返回空向量
            }
            result.push_back(radec_to_vec(ra, dec));
        }
    }

    return result;
}

// ============================================================================
// R08 改进3+5: WCS 边自适应细分辅助函数
//
// 对源像素 WCS 弯曲边进行自适应二分细分 (类似 subdivide_healpix_edge 但用 WCS 回调).
//
// R08 改进5 根因 (修复 5.020e-09 closure 误差):
//   原收敛条件 "WCS 中点 vs 大圆弧球面中点" 是错误的.
//   TAN (gnomonic) 投影保证平面直线 ↔ 大圆弧, 但平面中点不等于球面中点
//   (gnomonic 参数化非线性: c = atan(rho), 平面 t=0.5 ≠ 球面 t=0.5).
//   即使 WCS 边精确是大圆弧, dev 永远 > 0 (~6.6e-7 rad/1°边), 触发过度细分
//   (每边 4096 段), 累积 S-H 裁剪误差 + spherical_polygon_area fan triangulation 误差.
//
// R08 改进5 修复: 检查 WCS 中点是否在 (p0,p1) 大圆弧平面上
//   大圆弧法向量 n = normalize(cross(p0, p1))
//   WCS 中点到大圆弧平面的角距离 = |asin(dot(n, p_mid_wcs))|
//   gnomonic (TAN) 投影: WCS 边精确是大圆弧, dot(n, p_mid_wcs)=0, dev≈0 (浮点精度)
//   SIP 投影: WCS 边偏离大圆弧, dev>0, 递归细分直到 dev<wcs_epsilon
//
// wcs_epsilon = src_scale_rad * 1e-12 (相对阈值, 与源像素尺度成正比)
// max_depth = 12 (每边最多 4096 段, 处理高曲率 SIP 投影)
// ============================================================================
static const int WCS_ADAPTIVE_MAX_DEPTH = 12;

static void subdivide_wcs_edge(
    double x0, double y0, const Vec3& p0,
    double x1, double y1, const Vec3& p1,
    int depth,
    double wcs_epsilon,
    PixelToSkyFn pixelToSky, void* user_data,
    std::vector<Vec3>& out)
{
    // 像素坐标中点
    double xm = 0.5 * (x0 + x1);
    double ym = 0.5 * (y0 + y1);

    // WCS 中点 (通过 pixelToSky 回调映射)
    double ra_m, dec_m;
    if (!pixelToSky(xm, ym, ra_m, dec_m, user_data)) {
        out.push_back(p0);
        return;
    }
    Vec3 p_mid_wcs = radec_to_vec(ra_m, dec_m);

    // R08 改进5: 大圆弧平面偏差检验
    //   n = normalize(cross(p0, p1)) 是过 p0, p1 的大圆弧所在平面的法向量
    //   dot(n, p_mid_wcs) = 0 ⟺ p_mid_wcs 在大圆弧平面上 ⟺ WCS 边是大圆弧
    //   角距离 = |asin(dot(n, p_mid_wcs))|
    Vec3 n = normalize(cross(p0, p1));
    double d = dot(n, p_mid_wcs);
    if (d >  1.0) d =  1.0;
    if (d < -1.0) d = -1.0;
    double dev = std::fabs(std::asin(d));

    if (dev < wcs_epsilon || depth >= WCS_ADAPTIVE_MAX_DEPTH) {
        out.push_back(p0);
        return;
    }

    // 未收敛: 递归二分
    subdivide_wcs_edge(x0, y0, p0, xm, ym, p_mid_wcs, depth + 1,
                       wcs_epsilon, pixelToSky, user_data, out);
    subdivide_wcs_edge(xm, ym, p_mid_wcs, x1, y1, p1, depth + 1,
                       wcs_epsilon, pixelToSky, user_data, out);
}

// ============================================================================
// R08 改进3: 构造源像素 drop 球面多边形 (自适应 WCS 边细分)
//
// 对每条 WCS 边递归细分直到收敛, 消除 TAN/SIP 投影曲率导致的面积误差.
// 小像素自动收敛 (仅 4 角), 大像素递归细分到机器精度.
// ============================================================================
std::vector<Vec3> build_drop_polygon_adaptive(
    double px, double py, double pixfrac,
    PixelToSkyFn pixelToSky, void* user_data,
    double src_scale_rad)
{
    double half = 0.5 * pixfrac;
    double corners_xy[4][2] = {
        {px - half, py - half},  // 0: 左下
        {px + half, py - half},  // 1: 右下
        {px + half, py + half},  // 2: 右上
        {px - half, py + half}   // 3: 左上
    };

    // 映射 4 角到球面
    Vec3 corner_vecs[4];
    for (int i = 0; i < 4; i++) {
        double ra, dec;
        if (!pixelToSky(corners_xy[i][0], corners_xy[i][1], ra, dec, user_data)) {
            return {};
        }
        corner_vecs[i] = radec_to_vec(ra, dec);
    }

    // R08 改进3: 相对阈值 = src_scale_rad * 1e-12
    double wcs_epsilon = src_scale_rad * 1e-12;

    // 对 4 条边自适应细分
    std::vector<Vec3> result;
    result.reserve(32);
    for (int e = 0; e < 4; e++) {
        int en = (e + 1) % 4;
        subdivide_wcs_edge(
            corners_xy[e][0], corners_xy[e][1], corner_vecs[e],
            corners_xy[en][0], corners_xy[en][1], corner_vecs[en],
            0, wcs_epsilon, pixelToSky, user_data,
            result);
    }

    return result;
}

// ============================================================================
// 计算源像素 drop 与目标 HEALPix 像素的球面重叠面积
//
// R07 修复: 三角形扇剖分 (fan triangulation) 替代直接 S-H 裁剪
//
// R06 根因:
//   直接用 HEALPix 像素边界 (自适应细分后可达 100+ 顶点) 作为 S-H 裁剪多边形,
//   100+ 条裁剪边的数值误差在迭代中累积, 导致极区像素重叠面积系统性低估
//   (实测 ipix 8189 低估 27%, ipix 8183 完全漏掉).
//
//   诊断证据 (diag_pixel_boundary.cpp):
//     ipix 8189 (123 顶点): prod=2.496e-05, tri_fan=3.428e-05, 独立参考=3.428e-05
//     ipix 8191 (102 顶点): prod=3.797e-06, tri_fan=3.797e-06 (含极点, 边为大圆弧)
//
// 修复方案:
//   1. 获取 HEALPix 像素边界 (自适应细分, 与 R06 一致)
//   2. 用 pix2radec 获取像素精确中心
//   3. 以像素中心为顶点, 对边界做 fan triangulation: 三角形 (center, V[i], V[i+1])
//   4. 对每个三角形与 drop 做 S-H 裁剪 (每个三角形仅 3 条裁剪边, 无累积误差)
//   5. 累加所有三角形的重叠面积
//
// 正确性:
//   - 三角形在球面上总是凸的, S-H 裁剪数学上精确
//   - 三角形扇覆盖整个像素, 无缝隙无重叠
//   - 每个三角形仅 3 条裁剪边, 数值误差不累积
//   - 与独立参考 (L'Huilier + 高密度点采样) 结果一致 (误差 < 1e-10)
// ============================================================================
double compute_overlap_area(
    const std::vector<Vec3>& drop_corners,
    const healpix::HealpixCore& hp, uint64_t target_ipix)
{
    if (drop_corners.size() < 3) return 0.0;

    // 1. 获取目标 HEALPix 像素边界 (自适应细分)
    int nside = hp.getNside();
    int samples = (nside <= 8) ? 16 : 1;
    std::vector<Vec3> hp_boundary = get_healpix_boundary_sampled(hp, target_ipix, nside, samples);
    if (hp_boundary.size() < 3) return 0.0;

    // R08 改进4: 用 hp.pix2radec 获取像素精确中心 (替代边界顶点质心)
    //   边界顶点质心在大像素/极区场景下偏离真实中心, 引入 fan triangulation 系统误差
    //   pix2radec 返回 HEALPix 数学定义的像素中心, 精确到机器精度
    double ra_c, dec_c;
    hp.pix2radec((int64_t)target_ipix, &ra_c, &dec_c);
    Vec3 hp_center = radec_to_vec(ra_c, dec_c);

    int nb = (int)hp_boundary.size();

    // R07 混合策略:
    //   三角形扇剖分用于所有边界 (避免 S-H 高顶点数累积误差)
    //   优化: 若所有三角形完全在 drop 内 (drop 包含整个像素),
    //         直接返回 spherical_polygon_area(hp_boundary) 消除 Eriksson 面积差异
    int nd = (int)drop_corners.size();
    std::vector<Vec3> drop_clip_normals;
    drop_clip_normals.reserve(nd);
    Vec3 drop_centroid = {0.0, 0.0, 0.0};
    for (const auto& v : drop_corners) {
        drop_centroid.x += v.x; drop_centroid.y += v.y; drop_centroid.z += v.z;
    }
    drop_centroid = normalize(drop_centroid);
    for (int j = 0; j < nd; j++) {
        const Vec3& P1 = drop_corners[j];
        const Vec3& P2 = drop_corners[(j + 1) % nd];
        Vec3 n = cross(P1, P2);
        if (dot(n, drop_centroid) < 0.0) {
            n.x = -n.x; n.y = -n.y; n.z = -n.z;
        }
        drop_clip_normals.push_back(normalize(n));
    }

    // 三角形扇剖分 + 逐三角形 S-H 裁剪
    double total_overlap = 0.0;
    bool all_fully_inside = true;  // 所有三角形完全在 drop 内 → drop 包含像素

    // 辅助: 检查点是否在 drop 内 (所有 clip 法向量 dot >= 0)
    auto point_in_drop = [&](const Vec3& v) -> bool {
        for (const auto& n : drop_clip_normals) {
            if (dot(v, n) < -1e-12) return false;
        }
        return true;
    };

    for (int i = 0; i < nb; i++) {
        const Vec3& A = hp_boundary[i];
        const Vec3& B = hp_boundary[(i + 1) % nb];

        // 构造三角形 (hp_center, A, B)
        std::vector<Vec3> triangle = {hp_center, A, B};

        // 检查三角形是否完全在 drop 内 (3 个顶点均在 drop 内)
        bool tri_inside = point_in_drop(hp_center) && point_in_drop(A) && point_in_drop(B);
        if (!tri_inside) all_fully_inside = false;

        // S-H 裁剪: triangle (subject) against drop (clip)
        std::vector<Vec3> intersection = sutherland_hodgman_spherical(triangle, drop_clip_normals);
        if (intersection.size() < 3) continue;

        total_overlap += spherical_polygon_area(intersection);
    }

    // R08 改进1: 若 drop 完全包含像素, 直接用 HEALPix 解析面积 (机器精度)
    // HEALPix 所有像素等面积: 4π/(12·NSIDE²) = π/(3·NSIDE²)
    // 消除 spherical_polygon_area 的 fan triangulation 数值误差 (R07 残留 ~1e-5)
    if (all_fully_inside && total_overlap > 0.0) {
        double analytic_area = PI / (3.0 * (double)nside * (double)nside);
        return analytic_area;
    }

    return total_overlap;
}

// ============================================================================
// 查询与 drop 多边形可能相交的所有 HEALPix 像素 (不限于 1-ring)
//
// R06-B04 修复: 保守球冠保证不漏选
//   原实现用 1.5×hp_res 经验缓冲, 在 1°/pixel 极区场景下漏选 8/288 例.
//   修复方案:
//   1. drop 包围圆半径 = max(顶点到中心角距离) + drop 对角线半长裕量
//   2. 缓冲 = HEALPix 像素最大角半径 (中心到最远顶点)
//      HEALPix 像素非正方形, 对角线方向延伸更大:
//        赤道带对角线 ≈ 1.532 × res, 半长 ≈ 0.766 × res
//        极区三角形外接圆半径更大
//      取保守上界 2.0 × hp_res (覆盖所有方向的最坏情况)
//   3. 额外加 ε = 0.1 × hp_res 防浮点边界
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

    // 2. R06-B04: 保守缓冲 — HEALPix 像素最大角半径 + 裕量
    //    HEALPix 像素分辨率 (角秒) = sqrt(4π/(12*nside²)) * 206265
    //    像素最大角半径 (中心→最远顶点) 上界 ≈ 1.0 × hp_res (对角线半长)
    //    取 3.0 × hp_res 作为保守缓冲, 覆盖极区三角形和所有方向的最坏情况
    //    额外裕量防浮点边界效应和queryDisc中心判定偏差
    double hp_res_arcsec = hp.pixelResolutionArcsec();
    double hp_res_rad    = hp_res_arcsec * ARCSEC_TO_RAD;
    double buffer_rad     = 3.0 * hp_res_rad;
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
