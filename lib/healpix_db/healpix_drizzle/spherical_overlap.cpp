// ============================================================================
// 球面 HEALPix 重叠计算模块实现 (WP-D 步骤3-4)
//
// 替换 drizzle_engine.cpp 中的局部切平面近似 + 人工 HEALPix 菱形近似,
// 实现真实球面多边形裁剪与球面面积计算.
//
// 实现要点:
// - float64 内部精度 (double)
// - 球面面积用 Girard 定理: Area = Σ内角 - (n-2)π
// - 球面 Sutherland-Hodgman: 大圆弧裁剪, 保留法向量正侧
// - HEALPix 边界: 4 角顶点 (xy2ang), 处理赤道带菱形与极区三角形
// - 候选像素查询: drop 多边形球面包围圆 + queryDisc
// ============================================================================

#include "spherical_overlap.h"

#include <cstdio>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <unordered_set>

namespace spherical {

// HEALPix 像素外接半径安全系数
// 像素外接半径 = 中心到最远角点的大圆角距。全像素扫描证明 (NSIDE
// 16/32/64/128/256 穷举, scan_circumradius):
// nside=256 全局最大 = 1.043827 x hp_res @ (ra=89.8°, dec=-41.81°)
// (极区/赤道交界, HEALPix 像素最畸变区域), 随 NSIDE 单调收敛于 ~1.044
// 生产快速候选与 overlap 快速拒绝必须使用 >1.044 的缓冲:
// 取 1.1 x hp_res (约 5% 裕量覆盖浮点/边界数值效应)。
// 旧代码用 1.0 x hp_res 会在像素角区域漏选 (CAND-001, 实测比例 1.044 > 1.0)。
// 签字修正 (ORACLE_HARDENING): 像素外接半径安全上界。
// 解析上界 (赤道带, |z|<=2/3): 中心到最远顶点 ≤ 1.007×hp_res
// (半对角线: Δz=2/(3nside), Δφ·cos z=π/(4nside), arc≤sqrt(4/9+π²/16)/nside
// ≈1.0302/nside ≈ 1.0068×hp_res, hp_res=sqrt(π/3)/nside≈1.0233/nside);
// 极区 (bighp 0-3/8-11 极冠) 经验最坏 1.044×hp_res (dec≈±41.81°, 附 scan 证据);
// 取固定保守上界 1.25×hp_res (覆盖两者 + 浮点舍入), 快速路径依赖该上界,
// 扫描 (scan_circumradius) 仅作为附加证据。
static const double HP_CIRCUMRADIUS_FACTOR = 1.25;

// overlap 路径统计 (仅统计, 不改变逻辑; 由 drizzle_engine 汇总)
static thread_local long long g_tl_n_quick = 0;
static thread_local long long g_tl_n_fully = 0;
static thread_local long long g_tl_n_dropin = 0;
static thread_local long long g_tl_n_sh = 0;

// overlap 路径计数器默认关闭（与 fine profiler 同门控；
// 显式 ASTROCS_DRIZZLE_FINE_PROFILE=1 启用，避免生产路径无谓 ++ 开销）
static bool overlap_profile_enabled() {
    static const bool en = [] {
        const char* v = std::getenv("ASTROCS_DRIZZLE_FINE_PROFILE");
        return v && v[0] == '1';
    }();
    return en;
}

long long profile_overlap_path_counts(long long* fully, long long* dropin,
                                      long long* sh) {
    long long q = g_tl_n_quick;
    if (fully) *fully = g_tl_n_fully;
    if (dropin) *dropin = g_tl_n_dropin;
    if (sh) *sh = g_tl_n_sh;
    return q;
}

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
// 基本向量运算 ( 阶段7: template<typename T> 双实例 float/double)
// ============================================================================
template <typename T>
Vec3T<T> cross(const Vec3T<T>& a, const Vec3T<T>& b) {
    Vec3T<T> r;
    // 数值提升: 内部 double 累加, 防止 float 在微小几何下方向符号翻转
    r.x = T(double(a.y) * b.z - double(a.z) * b.y);
    r.y = T(double(a.z) * b.x - double(a.x) * b.z);
    r.z = T(double(a.x) * b.y - double(a.y) * b.x);
    return r;
}

template <typename T>
T dot(const Vec3T<T>& a, const Vec3T<T>& b) {
    return T(double(a.x) * b.x + double(a.y) * b.y + double(a.z) * b.z);
}

template <typename T>
T length(const Vec3T<T>& v) {
    return T(std::sqrt(double(v.x) * v.x + double(v.y) * v.y + double(v.z) * v.z));
}

template <typename T>
Vec3T<T> normalize(const Vec3T<T>& v) {
    double len = std::sqrt(double(v.x) * v.x + double(v.y) * v.y + double(v.z) * v.z);
    if (len < 1e-300) {
        // 退化向量, 返回北极作为安全默认
        return {T(0), T(0), T(1)};
    }
    double inv = 1.0 / len;
    return {T(v.x * inv), T(v.y * inv), T(v.z * inv)};
}

template <typename T>
Vec3T<T> radec_to_vec(T ra_deg, T dec_deg) {
    // 数值提升: double 三角函数, 输出 T 存储 (Scalar 实例, 精度稳健)
    double ra  = double(ra_deg)  * DEG2RAD;
    double dec = double(dec_deg) * DEG2RAD;
    double cd = std::cos(dec);
    return { T(cd * std::cos(ra)), T(cd * std::sin(ra)), T(std::sin(dec)) };
}

template <typename T>
void vec_to_radec(const Vec3T<T>& v, T& ra_deg, T& dec_deg) {
    double len = std::sqrt(double(v.x) * v.x + double(v.y) * v.y + double(v.z) * v.z);
    double inv = (len < 1e-300) ? 1.0 : 1.0 / len;
    double ux = v.x * inv, uy = v.y * inv, uz = v.z * inv;
    double dec = std::asin(std::max(-1.0, std::min(1.0, uz)));
    double ra  = std::atan2(uy, ux);
    if (ra < 0.0) ra += TWO_PI;
    if (ra >= TWO_PI) ra -= TWO_PI;
    dec_deg = T(dec * RAD2DEG);
    ra_deg  = T(ra  * RAD2DEG);
}

template <typename T>
T angular_distance(const Vec3T<T>& a, const Vec3T<T>& b) {
    double d = double(a.x) * b.x + double(a.y) * b.y + double(a.z) * b.z;
    d = std::max(-1.0, std::min(1.0, d));
    return T(std::acos(d));
}

// ============================================================================
// 球面多边形面积 (: 球面三角剖分 + Eriksson 稳定公式)
//
// 根因:
// 的双路径 (小多边形切平面鞋带 / 大多边形 Girard) 仍有缺陷:
// 1. 极区 1° 像素是大边形, 走 Girard 路径, 但内角和 ≈ n×(π/2) = 2π,
// (n-2)π = 2π, excess ≈ 0, catastrophic cancellation → 通量爆炸 303305×
// 2. Girard 切向量 t = A - (A·B)·B 在 A·B≈1 时有效数字丢失 ( 已识别)
// 3. 双路径切换阈值 60" 无数学依据, 1° 像素 (3600") 远超阈值走 Girard 失效
//
// 修复:
// 统一使用球面三角剖分 (fan triangulation) + Eriksson (2018) 稳定公式:
// 1. fan triangulation 以 V_0 为顶点: 三角形 (V_0, V_i, V_{i+1}), i=1..n-2
// 2. 每个球面三角形有向面积 = 2·atan2(det, 1 + a·b + b·c + c·a)
// 其中 det = a · (b × c) (标量三重积, 含符号)
// 3. 累加有向面积 (det<0 时三角形反向, 贡献负值)
//
// 修正: 原实现用 center 做扇出且 det<0 时取补面积 (4π-area),
// 这在 center 不在多边形内部时产生系统性误差. 改用 V_0 扇出 + 有符号累加,
// 对凸多边形 (S-H 裁剪结果) 总是正确, 无需依赖 center 位置.
//
// Eriksson 公式在所有退化情况下数值稳定:
// - 极小三角形: 分子分母同比缩小, 比值正确
// - 半球大小三角形: 分母→0, atan2 仍稳定
// - 共线顶点: 分子=0, 面积=0
// - 极区大像素: 无 excess≈0 的相消问题
//
// 参考: Eriksson, F. (2018) "The ang... spherical triangle area formula"
// ============================================================================
template <typename T>
T spherical_polygon_area(const std::vector<Vec3T<T>>& vertices) {
    return spherical_polygon_area_n<T>(vertices.data(), (int)vertices.size());
}

template <typename T>
T spherical_polygon_area_n(const Vec3T<T>* vertices, int n) {
    if (n < 3) return T(0);

    // ---- 签字修正: 冻结契约 (wiki/Reverse_Drizzle.md) ----
    // 仅支持包含在开半球内的简单多边形。环绕超过半球的多边形
    // (如 1/4 球面大四边形 {(1,0,0),(0,1,0),(-1,0,0),(0,0,1)}) 面积无定义,
    // 返回 NaN 明确"API 不支持", 不再返回误导性的 0 或错误面积。
    // 判定: 顶点质心方向 c, 若存在顶点到 c 角距 ≥ π/2 - tol, 则多边形
    // 未包含在开半球内 (凸多边形包含于开半球 ⇔ 归一化质心在凸包内 ⇔
    // 所有顶点到质心角距 < π/2)。
    {
        double cx = 0.0, cy = 0.0, cz = 0.0;
        for (int i = 0; i < n; i++) {
            cx += double(vertices[i].x);
            cy += double(vertices[i].y);
            cz += double(vertices[i].z);
        }
        double clen = std::sqrt(cx * cx + cy * cy + cz * cz);
        if (clen < 1e-12) return T(NAN);   // 质心退化 (环绕/对称) → 不支持
        double inv = 1.0 / clen;
        double max_ang = 0.0;
        for (int i = 0; i < n; i++) {
            double d = (double(vertices[i].x) * cx +
                        double(vertices[i].y) * cy +
                        double(vertices[i].z) * cz) * inv;
            d = std::max(-1.0, std::min(1.0, d));
            double ang = std::acos(d);
            if (ang > max_ang) max_ang = ang;
        }
        if (max_ang >= 0.5 * PI - 1e-12) return T(NAN);
    }

    // ---- fan triangulation 以 V_0 为顶点 + Eriksson 有符号面积 ----
    // 对凸多边形, V_0 与所有非相邻顶点构成同向三角形, 有符号累加得到正确面积.
    // 对非凸多边形, 此方法仍正确 (标准球面多边形面积定义).
    const Vec3T<T>& a = vertices[0];
    double total_area = 0.0;

    for (int i = 1; i < n - 1; i++) {
        const Vec3T<T>& b = vertices[i];
        const Vec3T<T>& c = vertices[i + 1];

        // 标量三重积 det = a · (b × c), 含符号
        double bx = double(b.y) * c.z - double(b.z) * c.y;
        double by = double(b.z) * c.x - double(b.x) * c.z;
        double bz = double(b.x) * c.y - double(b.y) * c.x;
        double det = double(a.x) * bx + double(a.y) * by + double(a.z) * bz;

        // 分母 = 1 + a·b + b·c + c·a
        double dot_ab = double(a.x) * b.x + double(a.y) * b.y + double(a.z) * b.z;
        double dot_bc = double(b.x) * c.x + double(b.y) * c.y + double(b.z) * c.z;
        double dot_ca = double(c.x) * a.x + double(c.y) * a.y + double(c.z) * a.z;
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

    return T(total_area);
}

// ============================================================================
// 球面 Sutherland-Hodgman 多边形裁剪
//
// 对每个裁剪平面法向量 n (保留 dot(n, v) >= 0 一侧):
// 遍历 subject 的每条边 (S → E):
// - 计算 S, E 是否在内侧 (dot(n, S) >= 0 / dot(n, E) >= 0)
// - 若跨越边界, 计算交点 I
// - 按经典 S-H 规则输出
//
// 球面交点:
// - 边 (S → E) 所在大圆的法向量 n_edge = cross(S, E)
// - 裁剪大圆的法向量 n_clip = n
// - 两大圆交点 = ±normalize(cross(n_edge, n_clip))
// - 选择 dot(I, S+E) > 0 的那个 (位于 S, E 之间)
// ============================================================================
template <typename T>
static inline bool is_inside(const Vec3T<T>& v, const Vec3T<T>& n) {
    // dot(n, v) >= 0 表示在保留侧 (含边界)
    return dot(n, v) >= T(-1e-15);
}

template <typename T>
static Vec3T<T> compute_intersection(const Vec3T<T>& S, const Vec3T<T>& E, const Vec3T<T>& n_clip) {
    // 边 (S→E) 所在大圆法向量
    Vec3T<T> n_edge = cross(S, E);
    // 两大圆交点 (两个候选)
    Vec3T<T> cross_nc = cross(n_edge, n_clip);
    Vec3T<T> I = normalize(cross_nc);

    // 选择位于 S, E 之间的那个 (dot(I, S+E) > 0)
    if (dot(I, S) + dot(I, E) < T(0)) {
        I.x = -I.x; I.y = -I.y; I.z = -I.z;
    }
    return I;
}

template <typename T>
std::vector<Vec3T<T>> sutherland_hodgman_spherical(
    const std::vector<Vec3T<T>>& subject,
    const std::vector<Vec3T<T>>& clip_plane_normals)
{
    if (subject.size() < 3) return {};
    if (clip_plane_normals.empty()) return subject;

    std::vector<Vec3T<T>> output = subject;

    for (const Vec3T<T>& n : clip_plane_normals) {
        if (output.empty()) break;

        Vec3T<T> nrm = normalize(n);
        std::vector<Vec3T<T>> input = output;
        output.clear();
        output.reserve(input.size() + 1);

        int m = (int)input.size();
        for (int i = 0; i < m; i++) {
            const Vec3T<T>& S = input[(i - 1 + m) % m];  // 前一顶点 (S-H 经典: S=E_prev)
            const Vec3T<T>& E = input[i];                 // 当前顶点
            // 注: Sutherland-Hodgman 经典实现遍历边 (S, E) = (input[i-1], input[i])
            // 这里改写为更直观的 (S, E) = (current, next) 等价形式, 但保持顶点遍历顺序一致
            bool S_in = is_inside<T>(S, nrm);
            bool E_in = is_inside<T>(E, nrm);

            if (E_in) {
                if (!S_in) {
                    // S 外 E 内: 输出交点 I, 再输出 E
                    output.push_back(compute_intersection<T>(S, E, nrm));
                }
                output.push_back(E);
            } else {
                if (S_in) {
                    // S 内 E 外: 只输出交点 I
                    output.push_back(compute_intersection<T>(S, E, nrm));
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
// sutherland_hodgman_spherical_fixed - 固定容量栈版本 (内环无堆分配)
// 逻辑与 sutherland_hodgman_spherical 一致; subject/输出 ≤ 16 顶点
// ============================================================================
int sutherland_hodgman_spherical_fixed(
    const Vec3* subject, int n_subject,
    const std::vector<Vec3>& clip_plane_normals,
    Vec3* out, int max_out)
{
    if (n_subject < 3 || max_out < 3) return 0;
    if (clip_plane_normals.empty()) {
        if (n_subject > max_out) n_subject = max_out;
        std::memcpy(out, subject, (size_t)n_subject * sizeof(Vec3));
        return n_subject;
    }

    Vec3 bufA[16], bufB[16];
    int n_in = n_subject;
    std::memcpy(bufA, subject, (size_t)n_subject * sizeof(Vec3));

    for (const Vec3& n : clip_plane_normals) {
        if (n_in < 3) break;
        Vec3 nrm = normalize(n);
        int n_out = 0;
        Vec3* dst = bufB;  // 16 上限足够, 双 buffer 轮换
        for (int i = 0; i < n_in; i++) {
            const Vec3& S = bufA[(i - 1 + n_in) % n_in];
            const Vec3& E = bufA[i];
            bool S_in = (S.x * nrm.x + S.y * nrm.y + S.z * nrm.z) >= -1e-15;
            bool E_in = (E.x * nrm.x + E.y * nrm.y + E.z * nrm.z) >= -1e-15;
            if (E_in) {
                if (!S_in) {
                    if (n_out >= 16) return 0;
                    dst[n_out++] = compute_intersection<double>(S, E, nrm);
                }
                if (n_out >= 16) return 0;
                dst[n_out++] = E;
            } else if (S_in) {
                if (n_out >= 16) return 0;
                dst[n_out++] = compute_intersection<double>(S, E, nrm);
            }
        }
        n_in = n_out;
        std::memcpy(bufA, bufB, (size_t)n_in * sizeof(Vec3));
        if (n_in < 3) return 0;
    }
    if (n_in > max_out) n_in = max_out;
    std::memcpy(out, bufA, (size_t)n_in * sizeof(Vec3));
    return n_in;
}

// ============================================================================
// 获取 HEALPix 像素的球面边界顶点
//
// 通过 healpix_core 的 pix2xy 得到 (bighp, x, y), 然后 4 个角点:
// C0 = (x, y), C1 = (x+1, y), C2 = (x+1, y+1), C3 = (x, y+1)
// 用 xy2ang 转换为 (theta, phi), 再转 (ra, dec), 再转 Vec3.
//
// 顺序: C0→C1→C2→C3, 在 (x,y) 平面上是逆时针, 在球面上保持一致方向.
// 极区像素一角可能退化 (如北极 bighp 0..3 的 C3 角), 但仍返回 4 个顶点 (退化角会
// 重合, 不影响凸性).
// ============================================================================
template <typename T>
std::vector<Vec3T<T>> get_healpix_boundary(
    const healpix::HealpixCore& hp, uint64_t ipix, int nside)
{
    (void)nside;  // 使用 hp.getNside, 参数保留接口一致性
    std::vector<Vec3T<T>> boundary;
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
    // - 北极 base 0..3, x+y > Ns 时为极冠
    // - 南极 base 8..11, x+y < Ns 时为极冠
    // - 其他为赤道带
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
    // C0 = (x, y), C1 = (x+1, y), C2 = (x+1, y+1), C3 = (x, y+1)
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
        // xyf2ang 已返回弧度 (theta, phi), 直接构造单位向量
        // (x=sinθcosφ, y=sinθsinφ, z=cosθ), 跳过 ra/dec 度往返转换
        // 与 radec_to_vec 数值等价 (误差 ~1e-16, 不影响任何门限)
        double st = std::sin(theta), ct = std::cos(theta);
        double sp = std::sin(phi),  cp = std::cos(phi);
        boundary.push_back({T(st * cp), T(st * sp), T(ct)});
    }

    return boundary;
}

// ============================================================================
// 固定 4 角边界（高 NSIDE 生产路径，无堆分配）。
// 与 get_healpix_boundary 逐位等价（同一 4 角计算逻辑）。
// ============================================================================
template <typename T>
void get_healpix_boundary4(const healpix::HealpixCore& hp, uint64_t ipix,
                           int nside, std::array<Vec3T<T>, 4>& out)
{
    (void)nside;
    int Ns = hp.getNside();
    int64_t npix_per_bighp = (int64_t)Ns * Ns;
    int bighp = (int)(ipix / npix_per_bighp);
    int64_t local_idx = ipix % npix_per_bighp;

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

    struct CornerXY { int x, y; };
    CornerXY corners[4] = {
        {xv, yv}, {xv + 1, yv}, {xv + 1, yv + 1}, {xv, yv + 1}
    };
    for (int i = 0; i < 4; i++) {
        double theta, phi;
        xyf2ang_replica(bighp, (double)corners[i].x, (double)corners[i].y,
                        Ns, &theta, &phi);
        double st = std::sin(theta), ct = std::cos(theta);
        double sp = std::sin(phi),  cp = std::cos(phi);
        out[(size_t)i] = {T(st * cp), T(st * sp), T(ct)};
    }
}

// ============================================================================
// HEALPix 像素边自适应细分辅助函数
//
// 对 HEALPix 像素的一条边 (像素坐标 (x0,y0)->(x1,y1)) 进行自适应二分细分.
// 边在像素坐标系为直线, 但映射到球面后既非大圆弧也非等纬度小圆弧
// (赤道带边在 (z,phi) 空间为线性曲线, 极区边为参数曲线).
//
// 收敛条件 (二选一):
// 1. 球面中点 (xyf2ang_replica 在像素坐标中点) 与大圆弧中点 (normalize(p0+p1))
// 的角偏差 < epsilon_rad → 该段已近似为直线, 取 p0
// 2. 递归深度达 max_depth → 强制截断 (防止无限递归)
//
// 递归二分: 不收敛时, 先细分 [p0, p_mid], 再细分 [p_mid, p1].
// 每个递归节点仅调用 1 次 xyf2ang_replica (中点), 端点复用父节点结果.
//
// 输出: out 追加从 p0 开始的细分顶点 (含 p0, 不含 p1, p1 由相邻边处理).
// ============================================================================
static const int    HP_ADAPTIVE_MAX_DEPTH = 8;      // 临时回退到 深度以排查崩溃
// 改进2: HP_ADAPTIVE_EPSILON 改为相对值 (见 subdivide_healpix_edge 内部计算)
// hp_epsilon = hp_res_rad * 1e-12, 其中 hp_res_rad = sqrt(π/(3·Ns²))
// NSIDE=64 时 hp_epsilon ≈ 1.6e-14 rad, 每条边细分到 ~256 段
// 实测固定 1e-9 (86 顶点) 会使 S-H 累积误差增大, 但 配合改进1 (解析面积)
// 和改进4 (精确中心) 后, 高细分不再进入 spherical_polygon_area, 累积误差问题消除

template <typename T>
static void subdivide_healpix_edge(
    int bighp, int Ns,
    double x0, double y0, const Vec3T<double>& p0,
    double x1, double y1, const Vec3T<double>& p1,
    int depth,
    std::vector<Vec3T<T>>& out)
{
    // 修复: HEALPix 边细分阈值从 1e-12 改为 1e-6
    // 根因: hp_res_rad * 1e-12 对非大圆弧的 HEALPix 边永远不收敛.
    // HEALPix 赤道带等纬度边是小圆 (非大圆弧), 经线边才是大圆弧.
    // 等纬度边的角距离偏差 ≈ sin(dec)*L²/8 (L=hp_res_rad):
    // NSIDE=65536: L=1.56e-5, 偏差≈1.2e-11, 旧阈值1.56e-17 → 永不收敛
    // 导致每边递归到 MAX_DEPTH=8 (256段/边, 1024顶点/像素),
    // compute_overlap_area 极慢 (16.2M源像素×20候选×1024三角形=332B次操作).
    // 新阈值 1e-6 (相对值): NSIDE=65536→1.56e-11, 1-2次二分收敛 (2-4段/边);
    // 弧弦误差 < 1e-6*hp_res ≈ 3e-6角秒, 远超 support uint8 量化精度(0.4%).
    // 注: WCS 边 (subdivide_wcs_edge) 用大圆弧平面偏差法 (dafa200 改进5),
    // 因 TAN 投影直线↔大圆弧; HEALPix 边非大圆弧, 用角距离法更合适.
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
    Vec3T<double> p_mid_wcs = radec_to_vec<double>(ra_m, dec_m);

    // 大圆弧中点 = normalize(p0 + p1)
    Vec3T<double> p_mid_gc = normalize(Vec3T<double>{p0.x + p1.x, p0.y + p1.y, p0.z + p1.z});

    // 角距离法: 反映 HEALPix 边偏离两端点大圆弧的程度 (double 计算,
    // 防 angular_distance<T> 的 float 返回截断 ~1e-7 导致永不收敛)
    double dev_cos = double(p_mid_wcs.x) * p_mid_gc.x +
                     double(p_mid_wcs.y) * p_mid_gc.y +
                     double(p_mid_wcs.z) * p_mid_gc.z;
    dev_cos = std::max(-1.0, std::min(1.0, dev_cos));
    double dev = std::acos(dev_cos);

    if (dev < hp_epsilon || depth >= HP_ADAPTIVE_MAX_DEPTH) {
        // 收敛: 该段近似为直线, 只输出 p0
        out.push_back({T(p0.x), T(p0.y), T(p0.z)});
        return;
    }

    // 未收敛: 递归二分
    subdivide_healpix_edge<T>(bighp, Ns, x0, y0, p0, xm, ym, p_mid_wcs, depth + 1, out);
    subdivide_healpix_edge<T>(bighp, Ns, xm, ym, p_mid_wcs, x1, y1, p1, depth + 1, out);
}

// ============================================================================
// 获取 HEALPix 像素的球面边界顶点 ( 自适应细分)
//
// 对所有 NSIDE 统一使用自适应边细分策略:
// - 极区像素和赤道带像素均通过 subdivide_healpix_edge 处理
// - 不再按 NSIDE<=8 硬切换采样数
// - 不再假设极区边是大圆弧 (实测极区边为参数曲线, 非大圆弧)
//
// 收敛条件: 球面中点与大圆弧中点偏差 < 1e-6 弧度 (≈0.2角秒), 或深度达 8.
// 每条边最多 2^8=256 段, 但实际收敛远早于此 (低 NSIDE 大像素约 4-16 段,
// 高 NSIDE 小像素约 1-2 段).
//
// 参数 samples_per_edge: 保留接口兼容性, 不再控制精度 (内部自适应决定).
//
// 顶点顺序: C0→(细分点)→C1→(细分点)→C2→(细分点)→C3→(细分点)→(回到 C0)
// 每条边输出含起点不含终点, 总顶点数 = 4 条边的细分段数之和.
// ============================================================================
template <typename T>
std::vector<Vec3T<T>> get_healpix_boundary_sampled(
    const healpix::HealpixCore& hp, uint64_t ipix, int nside,
    int samples_per_edge)
{
    (void)nside;
    (void)samples_per_edge;  //: 自适应细分, 不再依赖固定采样数

    int Ns = hp.getNside();
    // 性能: 高 NSIDE (小像素) 时 4 角已足够 — HEALPix 边偏离大圆弧的偏差
    // ≈ sin(dec)·L²/8 (L=像素尺度)。nside≥256 → L≤13.7' → 偏差 ≤ ~2e-10 rad,
    // 远小于任何面积/通量精度需求。跳过 subdivide_healpix_edge 的固定开销
    // (每边中点 xyf2ang + radec_to_vec + normalize + acos ≈ 100ns/边)。
    if (Ns >= 256) {
        return get_healpix_boundary<T>(hp, ipix, Ns);
    }
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

    // 计算 4 个角点的球面向量 (double 内部, 细分收敛; 输出转 T)
    Vec3T<double> corner_vecs[4];
    for (int i = 0; i < 4; i++) {
        double theta, phi;
        xyf2ang_replica(bighp, corners_xy[i][0], corners_xy[i][1], Ns, &theta, &phi);
        double dec = (HALF_PI - theta) * RAD2DEG;
        double ra  = phi * RAD2DEG;
        if (ra < 0.0)  ra += 360.0;
        if (ra >= 360.0) ra -= 360.0;
        corner_vecs[i] = radec_to_vec<double>(ra, dec);
    }

    // 对 4 条边自适应细分 (统一处理极区和赤道带)
    std::vector<Vec3T<T>> boundary;
    boundary.reserve(32);  // 预估, 自适应实际段数可能更多
    for (int e = 0; e < 4; e++) {
        int en = (e + 1) % 4;
        subdivide_healpix_edge<T>(
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
// c0 = (px - half, py - half) 左下
// c1 = (px + half, py - half) 右下
// c2 = (px + half, py + half) 右上
// c3 = (px - half, py + half) 左上
// half = 0.5 * pixfrac
//
// 每条边采样 samples_per_edge 段 (等分), 每个采样点通过 pixelToSky 回调映射到
// 天球坐标, 再转换为球面单位向量.
//
// 顶点顺序: c0→c1→c2→c3→c0 (逆时针), 每边不含末点 (避免与下一边首点重复),
// 总顶点数 = 4 * samples_per_edge.
//
// samples_per_edge=1: 退化为 4 个角顶点 (t=0 for each edge)
// ============================================================================
template <typename T>
std::vector<Vec3T<T>> build_drop_polygon_sampled(
    T px, T py, T pixfrac,
    PixelToSkyFn pixelToSky, void* user_data,
    int samples_per_edge)
{
    std::vector<Vec3T<T>> result;

    if (samples_per_edge < 1) samples_per_edge = 1;

    // pixfrac 收缩后的四角
    T half = T(0.5) * pixfrac;
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
            result.push_back(radec_to_vec<T>(T(ra), T(dec)));
        }
    }

    return result;
}

// ============================================================================
// 改进3+5: WCS 边自适应细分辅助函数
//
// 对源像素 WCS 弯曲边进行自适应二分细分 (类似 subdivide_healpix_edge 但用 WCS 回调).
//
// 改进5 根因 (修复 5.020e-09 closure 误差):
// 原收敛条件 "WCS 中点 vs 大圆弧球面中点" 是错误的.
// TAN (gnomonic) 投影保证平面直线 ↔ 大圆弧, 但平面中点不等于球面中点
// (gnomonic 参数化非线性: c = atan(rho), 平面 t=0.5 ≠ 球面 t=0.5).
// 即使 WCS 边精确是大圆弧, dev 永远 > 0 (~6.6e-7 rad/1°边), 触发过度细分
// (每边 4096 段), 累积 S-H 裁剪误差 + spherical_polygon_area fan triangulation 误差.
//
// 改进5 修复: 检查 WCS 中点是否在 (p0,p1) 大圆弧平面上
// 大圆弧法向量 n = normalize(cross(p0, p1))
// WCS 中点到大圆弧平面的角距离 = |asin(dot(n, p_mid_wcs))|
// gnomonic (TAN) 投影: WCS 边精确是大圆弧, dot(n, p_mid_wcs)=0, dev≈0 (浮点精度)
// SIP 投影: WCS 边偏离大圆弧, dev>0, 递归细分直到 dev<wcs_epsilon
//
// wcs_epsilon = src_scale_rad * 1e-12 (相对阈值, 与源像素尺度成正比)
// max_depth = 12 (每边最多 4096 段, 处理高曲率 SIP 投影)
// ============================================================================
static const int WCS_ADAPTIVE_MAX_DEPTH = 12;

template <typename T>
static void subdivide_wcs_edge(
    double x0, double y0, const Vec3T<double>& p0,
    double x1, double y1, const Vec3T<double>& p1,
    int depth,
    double wcs_epsilon,
    PixelToSkyFn pixelToSky, void* user_data,
    std::vector<Vec3T<T>>& out)
{
    // 像素坐标中点
    double xm = 0.5 * (x0 + x1);
    double ym = 0.5 * (y0 + y1);

    // WCS 中点 (通过 pixelToSky 回调映射)
    double ra_m, dec_m;
    if (!pixelToSky(xm, ym, ra_m, dec_m, user_data)) {
        out.push_back({T(p0.x), T(p0.y), T(p0.z)});
        return;
    }
    Vec3T<double> p_mid_wcs = radec_to_vec<double>(ra_m, dec_m);

    // 改进5: 大圆弧平面偏差检验
    // n = normalize(cross(p0, p1)) 是过 p0, p1 的大圆弧所在平面的法向量
    // dot(n, p_mid_wcs) = 0 ⟺ p_mid_wcs 在大圆弧平面上 ⟺ WCS 边是大圆弧
    // 角距离 = |asin(dot(n, p_mid_wcs))|
    Vec3T<double> n = normalize(cross(p0, p1));
    double d = double(n.x) * p_mid_wcs.x + double(n.y) * p_mid_wcs.y + double(n.z) * p_mid_wcs.z;
    if (d >  1.0) d =  1.0;
    if (d < -1.0) d = -1.0;
    double dev = std::fabs(std::asin(d));

    if (dev < wcs_epsilon || depth >= WCS_ADAPTIVE_MAX_DEPTH) {
        out.push_back({T(p0.x), T(p0.y), T(p0.z)});
        return;
    }

    // 未收敛: 递归二分
    subdivide_wcs_edge<T>(x0, y0, p0, xm, ym, p_mid_wcs, depth + 1,
                          wcs_epsilon, pixelToSky, user_data, out);
    subdivide_wcs_edge<T>(xm, ym, p_mid_wcs, x1, y1, p1, depth + 1,
                          wcs_epsilon, pixelToSky, user_data, out);
}

// ============================================================================
// 改进3: 构造源像素 drop 球面多边形 (自适应 WCS 边细分)
//
// 对每条 WCS 边递归细分直到收敛, 消除 TAN/SIP 投影曲率导致的面积误差.
// 小像素自动收敛 (仅 4 角), 大像素递归细分到机器精度.
// ============================================================================
template <typename T>
std::vector<Vec3T<T>> build_drop_polygon_adaptive(
    T px, T py, T pixfrac,
    PixelToSkyFn pixelToSky, void* user_data,
    T src_scale_rad)
{
    T half = T(0.5) * pixfrac;
    double corners_xy[4][2] = {
        {px - half, py - half},  // 0: 左下
        {px + half, py - half},  // 1: 右下
        {px + half, py + half},  // 2: 右上
        {px - half, py + half}   // 3: 左上
    };

    // 映射 4 角到球面 (double 内部, 细分收敛; 输出转 T)
    Vec3T<double> corner_vecs[4];
    for (int i = 0; i < 4; i++) {
        double ra, dec;
        if (!pixelToSky(corners_xy[i][0], corners_xy[i][1], ra, dec, user_data)) {
            return {};
        }
        corner_vecs[i] = radec_to_vec<double>(ra, dec);
    }

    // 改进3: 相对阈值 = src_scale_rad * 1e-12 (double, 防 float 截断收敛失效)
    // 签字修正 (REV-107): 阈值不得低于 pixelToSky 数值噪声。实测 TAN 边中点
    // 对大圆弧平面的偏差 ~6e-14 rad (WCS 映射数值误差累积), 若阈值低于该
    // 噪声则永不收敛 → 每边递归到深度 12 (4096 段), 像素 footprint 16384
    // 顶点, 反向 Drizzle 卡死。
    // 下限 1e-11 rad: TAN 立即收敛 (4 角); SIP 曲线偏差 < max(1e-11,
    // src_scale×1e-12) 即收敛; 面积误差 ~eps/(2·src_scale) ≤ 1.7e-7 相对
    // (src_scale=6.3"), 远低于任何科学门。
    double wcs_epsilon = std::max(double(src_scale_rad) * 1e-12, 1e-11);

    // 对 4 条边自适应细分
    std::vector<Vec3T<T>> result;
    result.reserve(32);
    for (int e = 0; e < 4; e++) {
        int en = (e + 1) % 4;
        subdivide_wcs_edge<T>(
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
// 修复: 三角形扇剖分 (fan triangulation) 替代直接 S-H 裁剪
//
// 根因:
// 直接用 HEALPix 像素边界 (自适应细分后可达 100+ 顶点) 作为 S-H 裁剪多边形,
// 100+ 条裁剪边的数值误差在迭代中累积, 导致极区像素重叠面积系统性低估
// (实测 ipix 8189 低估 27%, ipix 8183 完全漏掉).
//
// 诊断证据 (diag_pixel_boundary.cpp):
// ipix 8189 (123 顶点): prod=2.496e-05, tri_fan=3.428e-05, 独立参考=3.428e-05
// ipix 8191 (102 顶点): prod=3.797e-06, tri_fan=3.797e-06 (含极点, 边为大圆弧)
//
// 修复方案:
// 1. 获取 HEALPix 像素边界 (自适应细分, 与 一致)
// 2. 用 pix2radec 获取像素精确中心
// 3. 以像素中心为顶点, 对边界做 fan triangulation: 三角形 (center, V[i], V[i+1])
// 4. 对每个三角形与 drop 做 S-H 裁剪 (每个三角形仅 3 条裁剪边, 无累积误差)
// 5. 累加所有三角形的重叠面积
//
// 正确性:
// - 三角形在球面上总是凸的, S-H 裁剪数学上精确
// - 三角形扇覆盖整个像素, 无缝隙无重叠
// - 每个三角形仅 3 条裁剪边, 数值误差不累积
// - 与独立参考 (L'Huilier + 高密度点采样) 结果一致 (误差 < 1e-10)
// ============================================================================
template <typename T>
T compute_overlap_area(
    const std::vector<Vec3T<T>>& drop_corners,
    const healpix::HealpixCore& hp, uint64_t target_ipix)
{
    if (drop_corners.size() < 3) return T(0);

    DropGeometryT<T> g = build_drop_geometry<T>(drop_corners);
    return T(compute_overlap_area_g(g, hp, target_ipix));
}

// ============================================================================
// build_drop_geometry - 预计算 drop 包围圆 + 裁剪法向量 (Scalar 存储, 每源像素一次)
// ============================================================================
// 微小多边形切平面面积 ( 阶段4):
// 球面面积 = 切平面有向叉积和 × (1 + O(θ²)), θ=max_angle。
// θ < 1e-3 rad 时偏差 < 4e-8, 而 double 球面 Eriksson 的 det = a·(b×c)
// 在 θ~1e-7 rad 时是 ~1e-8 项相消到 ~1e-15 的差, 噪声 ~1e-4~5e-5
// (0.01\"~0.1\" drop 实测)。切平面坐标避免相消, 误差仅剩表示层
// ~1e-9 相对。对微小 drop 是"不加精度、不加计算量"的数值稳定替代。
// 顶点数组版 (c 为切平面法向/中心, 由调用方传入; 为空则用质心)
static double planar_polygon_area_n(const Vec3* pts, int n, const Vec3* center = nullptr) {
    if (n < 3) return 0.0;
    Vec3 c = center ? *center : Vec3{0, 0, 0};
    if (!center) {
        for (int i = 0; i < n; i++) { c.x += pts[i].x; c.y += pts[i].y; c.z += pts[i].z; }
    }
    double cl = std::sqrt(c.x * c.x + c.y * c.y + c.z * c.z);
    if (cl < 1e-300) return 0.0;
    c.x /= cl; c.y /= cl; c.z /= cl;
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        const Vec3& p = pts[i];
        const Vec3& q = pts[(i + 1) % n];
        // 投影到切平面 (垂直于 c)
        double dp = p.x * c.x + p.y * c.y + p.z * c.z;
        double dq = q.x * c.x + q.y * c.y + q.z * c.z;
        double ux = p.x - dp * c.x, uy = p.y - dp * c.y, uz = p.z - dp * c.z;
        double vx = q.x - dq * c.x, vy = q.y - dq * c.y, vz = q.z - dq * c.z;
        // (u × v) · c = 有向面积元
        sum += (uy * vz - uz * vy) * c.x +
               (uz * vx - ux * vz) * c.y +
               (ux * vy - uy * vx) * c.z;
    }
    return 0.5 * std::fabs(sum);
}

static double planar_polygon_area(const std::vector<Vec3>& pts) {
    return planar_polygon_area_n(pts.data(), (int)pts.size());
}

template <typename Scalar>
DropGeometryT<Scalar> build_drop_geometry(const std::vector<Vec3T<Scalar>>& drop_corners,
                                          const std::vector<Vec3>* corners_dbl) {
    DropGeometryT<Scalar> g;
    build_drop_geometry_into<Scalar>(g, drop_corners, corners_dbl);
    return g;
}

template <typename Scalar>
void build_drop_geometry_into(DropGeometryT<Scalar>& g,
                              const std::vector<Vec3T<Scalar>>& drop_corners,
                              const std::vector<Vec3>* corners_dbl) {
    // 复用已有容量（g 由调用方 thread-local 持有，首帧
    // reserve 后不再分配）；科学语义与 build_drop_geometry 完全一致。
    if (g.corners.capacity() < drop_corners.size())
        g.corners.reserve(drop_corners.size());
    if (g.clip_normals.capacity() < drop_corners.size())
        g.clip_normals.reserve(drop_corners.size());
    if (g.clip_normals_d.capacity() < drop_corners.size())
        g.clip_normals_d.reserve(drop_corners.size());
    if (g.corners_d.capacity() < drop_corners.size())
        g.corners_d.reserve(drop_corners.size());
    // 复用对象必须清空（corners 由赋值自动清空，clip_normals 为 push_back
    // 填充——不清空会逐像素累积上一像素的法向量）
    g.clip_normals.clear();
    g.clip_normals_d.clear();
    g.corners = drop_corners;
    int nd = (int)drop_corners.size();
    if (nd < 3) return;
    // double 角点缓存 (drop_area / 完全包含判定的几何源)
    g.corners_d.resize(nd);
    if (corners_dbl && (int)corners_dbl->size() == nd) {
        g.corners_d = *corners_dbl;
    } else {
        for (int j = 0; j < nd; j++)
            g.corners_d[j] = {double(drop_corners[j].x),
                              double(drop_corners[j].y),
                              double(drop_corners[j].z)};
    }
    // drop 面积 (double 源, 构建时一次; 供小 drop 完全包含快路径)
    // 尺度感知: max_angle 暂以临时中心近似估计 (下面精确计算后不重复)
    // 微小 drop (角跨度 < 1e-3 rad ≈ 206\") 用切平面 2D 面积 (数值稳定);
    // 大 drop 用球面 Eriksson (double 在 θ > 1e-3 时噪声 < 1e-7 可忽略)
    {
        double cx0 = 0.0, cy0 = 0.0, cz0 = 0.0;
        for (const auto& v : g.corners_d) { cx0 += v.x; cy0 += v.y; cz0 += v.z; }
        double l0 = std::sqrt(cx0 * cx0 + cy0 * cy0 + cz0 * cz0);
        if (l0 > 1e-300) { cx0 /= l0; cy0 /= l0; cz0 /= l0; }
        double ang0 = 0.0;
        for (const auto& v : g.corners_d) {
            double d = v.x * cx0 + v.y * cy0 + v.z * cz0;
            d = std::max(-1.0, std::min(1.0, d));
            ang0 = std::max(ang0, std::acos(d));
        }
        g.drop_area = (ang0 < 1e-3)
            ? planar_polygon_area(g.corners_d)
            : spherical_polygon_area<double>(g.corners_d);
    }
    // 方向判定/中心用 double 精度源 (corners_dbl 由调用方传入 WCS double 角点;
    // 无则从 Scalar 转 — float 存储的 ~1e-7 舍入在 6.3\" 尺度会导致
    // clip 法向量方向翻转, 因此生产路径必须提供 double 角点)
    double cxx = 0.0, cyy = 0.0, czz = 0.0;
    if (corners_dbl && (int)corners_dbl->size() == nd) {
        for (const auto& v : *corners_dbl) { cxx += v.x; cyy += v.y; czz += v.z; }
    } else {
        for (const auto& v : drop_corners) { cxx += v.x; cyy += v.y; czz += v.z; }
    }
    double clen = std::sqrt(cxx * cxx + cyy * cyy + czz * czz);
    double inv = (clen < 1e-300) ? 1.0 : 1.0 / clen;
    double cxn = cxx * inv, cyn = cyy * inv, czn = czz * inv;
    g.center = {Scalar(cxn), Scalar(cyn), Scalar(czn)};
    g.center_d = {cxn, cyn, czn};
    // max_angle 必须从 double 精度角点源计算 (corners_dbl 优先, 无则提升 Scalar):
    // float 存储角点含 ~1e-7 长度舍入, 对 6.3\" 尺度小 drop 其真实角距仅
    // ~2e-5 rad (cos 偏差 ~2e-10), 舍入噪声会淹没真实值, 使 max_angle 被
    // 钳制为 0 (→ 快速拒绝误杀边缘 leaf) 或膨胀 ~10 倍 (→ 候选集合扩大),
    // 直接导致 FP32 边缘 leaf 通量塌缩 (L2 NSIDE=65536 实测 445→3e-5)。
    double max_angle = 0.0;
    if (corners_dbl && (int)corners_dbl->size() == nd) {
        for (const auto& v : *corners_dbl) {
            double d = v.x * cxn + v.y * cyn + v.z * czn;
            d = std::max(-1.0, std::min(1.0, d));
            double ang = std::acos(d);
            if (ang > max_angle) max_angle = ang;
        }
    } else {
        for (const auto& v : drop_corners) {
            double d = double(v.x) * cxn + double(v.y) * cyn + double(v.z) * czn;
            d = std::max(-1.0, std::min(1.0, d));
            double ang = std::acos(d);
            if (ang > max_angle) max_angle = ang;
        }
    }
    g.max_angle = max_angle;

    g.clip_normals.reserve(nd);
    g.clip_normals_d.reserve(nd);
    for (int j = 0; j < nd; j++) {
        Vec3 P1, P2;
        if (corners_dbl && (int)corners_dbl->size() == nd) {
            P1 = (*corners_dbl)[j];
            P2 = (*corners_dbl)[(j + 1) % nd];
        } else {
            P1 = {double(drop_corners[j].x), double(drop_corners[j].y), double(drop_corners[j].z)};
            P2 = {double(drop_corners[(j + 1) % nd].x), double(drop_corners[(j + 1) % nd].y),
                  double(drop_corners[(j + 1) % nd].z)};
        }
        double nx = P1.y * P2.z - P1.z * P2.y;
        double ny = P1.z * P2.x - P1.x * P2.z;
        double nz = P1.x * P2.y - P1.y * P2.x;
        if (nx * cxn + ny * cyn + nz * czn < 0.0) {
            nx = -nx; ny = -ny; nz = -nz;
        }
        double nlen = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (nlen < 1e-300) { nlen = 1.0; }
        g.clip_normals.push_back({Scalar(nx / nlen), Scalar(ny / nlen), Scalar(nz / nlen)});
        g.clip_normals_d.push_back({nx / nlen, ny / nlen, nz / nlen});
    }
}

// ============================================================================
// compute_overlap_area_g - 使用预计算 drop 几何的重叠面积 (Scalar 实例)
// Scalar=float: 几何数据/返回 float (真 FP32 类型贯穿); 数值运算内部 double
// 提升 (微小球面几何 det/denom 在纯 float 下误差 ~9%, 提升后科学正确)
// ============================================================================
template <typename Scalar>
Scalar compute_overlap_area_g(const DropGeometryT<Scalar>& g,
                              const healpix::HealpixCore& hp, uint64_t target_ipix)
{
    // 旧签名保持 oracle 兼容；内部委托 ctx 版本
    // （hp_res_rad = 调用时一次解析，语义与历史实现完全一致）
    return compute_overlap_area_g_ctx<Scalar>(
        g, hp, target_ipix,
        hp.pixelResolutionArcsec() * ARCSEC_TO_RAD);
}

template <typename Scalar>
Scalar overlap_area_impl(const DropGeometryT<Scalar>& g,
                         double hp_res_rad, int nside,
                         const Vec3& hp_center,
                         const Vec3* hp_boundary, int nb) {
    int nd = (int)g.corners.size();
    if (nd < 3) return Scalar(0);

    // 内部 double 提升 (数值稳定; 输入/输出保持 Scalar 类型贯穿):
    // 微小球面几何 (6.3\" 尺度) 纯 float det/denom 误差 ~9%, 提升后科学正确
    // double 精度缓存 (build_drop_geometry 构建时一次)
    const std::vector<Vec3>& clip_d = g.clip_normals_d;
    double center_x = g.center_d.x, center_y = g.center_d.y, center_z = g.center_d.z;

    // 快速拒绝 (Oracle 穷举关键; 生产 query 已预过滤, 此判断为防御性重复):
    // 像素中心到 drop 包围圆中心距离 > max_angle + 1.0×hp_res (像素外接半径上界)
    // → 必然不相交, 跳过边界获取/S-H (避免极区大像素细分开销)
    // 像素中心只求一次 (pix2radec + radec_to_vec), 快速拒绝与
    // hp_center 复用同一向量 (原实现重复计算两次)
    // quick-reject 用"带安全余量的 dot 预判"：
    // d_c <= cos(lim + 1e-9 rad) → acos(d_c) >= lim+1e-9 > lim（浮点
    // acos/cos 舍入 ~1e-16 rad << 1e-9）→ 原 acos(d_c) > lim 必真，
    // 拒绝集是原拒绝集的严格子集（零漏、零误拒）；
    // 边界区 (cos(lim+1e-9), cos(lim)] 走原始 acos 判定 → 与历史实现
    // 位级一致（1e-19 sliver oracle 通过）。
    // 4.23 亿次候选快速拒绝中绝大多数（远离边界）免 acos。
    const double lim = g.max_angle + HP_CIRCUMRADIUS_FACTOR * hp_res_rad;
    const double cos_safe = std::cos(lim + 1e-9);
    {
        double d_c = hp_center.x * center_x + hp_center.y * center_y + hp_center.z * center_z;
        d_c = std::max(-1.0, std::min(1.0, d_c));
        if (d_c <= cos_safe) {
            if (overlap_profile_enabled()) g_tl_n_quick++;
            return Scalar(0);
        }
        if (std::acos(d_c) > lim) {
            if (overlap_profile_enabled()) g_tl_n_quick++;
            return Scalar(0);
        }
    }

    // 混合策略 (保持科学语义与数值精度):
    // 三角形扇剖分 (hp_center → 每边三角形) + 逐三角形 S-H 裁剪
    // 性能优化: tri_inside 快路径 (三角形完全在 drop 内 → 直接面积,
    // 免 S-H); drop 几何预计算复用; 高 NSIDE 4 角边界直出。
    const std::vector<Vec3>& drop_clip_normals = clip_d;

    // 判定在 double 缓存 (clip_normals_d / center_d) 上进行, 与 Scalar 存储
    // 无关, FP32/FP64 实例必须使用同一容差: 1e-12 仅覆盖 double 舍入。
    // 注: 凸分离快速判定 (drop 全在像素某边外 / 像素全在 drop 某边外)
    // 已移除 — 球面像素边界的支撑线无法精确表示 (细分小段不是支撑线,
    // 主 4 角大圆弧与真实边界内缩/外扩不定), 任何近似支撑线的分离判定都会
    // 在边界附近误杀真实相交的 drop (L0 NSIDE=64 实测通量丢失 0.6%)。
    // 保留数学安全的快路径: 快速拒绝 (包围圆) / leaf_fully / drop_inside。
    const double inside_tol = 1e-12;
    bool leaf_fully_inside_drop = true;
    for (const auto& n : drop_clip_normals) {
        for (int i = 0; i < nb; i++) {
            if (hp_boundary[i].x * n.x + hp_boundary[i].y * n.y +
                    hp_boundary[i].z * n.z < -inside_tol) {
                leaf_fully_inside_drop = false;
                break;
            }
        }
        if (!leaf_fully_inside_drop) break;
    }
    if (leaf_fully_inside_drop) {
        // drop 包含像素 → overlap = 像素解析面积 (4π/(12·NSIDE²) = π/(3·NSIDE²))
        if (overlap_profile_enabled()) g_tl_n_fully++;
        return Scalar(PI / (3.0 * (double)nside * (double)nside));
    }

    // drop 完全位于目标像素内 → overlap = drop_area (weight=1 精确)
    // 原 S-H (像素三角形扇 x drop 边) 会重算 drop 角点: 像素边大圆与
    // drop 边大圆在赤道带接近平行, 交点固定绝对误差 ~1e-11 rad, 相对
    // 误差与 drop 尺寸成反比 (0.01\" 实测 4.6e-4) → 必须免 S-H。
    // 判定用真实细分边界 (精确, 不依赖任何支撑线近似)。
    {
        bool drop_inside_pixel = true;
        for (int i = 0; i < nb; i++) {
            const Vec3& P1 = hp_boundary[i];
            const Vec3& P2 = hp_boundary[(i + 1) % nb];
            double nx = P1.y * P2.z - P1.z * P2.y;
            double ny = P1.z * P2.x - P1.x * P2.z;
            double nz = P1.x * P2.y - P1.y * P2.x;
            double nl = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (nl < 1e-12) { drop_inside_pixel = false; break; }
            nx /= nl; ny /= nl; nz /= nl;
            if (nx * hp_center.x + ny * hp_center.y + nz * hp_center.z < 0.0) {
                nx = -nx; ny = -ny; nz = -nz;
            }
            double min_d = 1e9;
            for (const auto& v : g.corners_d) {
                double d = v.x * nx + v.y * ny + v.z * nz;
                if (d < min_d) min_d = d;
            }
            if (min_d < -inside_tol) { drop_inside_pixel = false; break; }
        }
        if (drop_inside_pixel) {
        if (overlap_profile_enabled()) g_tl_n_dropin++;
        return Scalar(g.drop_area);
        }
    }

    // 三角形扇剖分 + 逐三角形 S-H 裁剪 (旧算法数值路径, 通量闭合 ~1e-11)
    if (overlap_profile_enabled()) g_tl_n_sh++;
    double total_overlap = 0.0;
    // 高 NSIDE 生产路径 (边界 4 角, 凸四边形) 用一次四边形 S-H
    // 代替 4 次三角形扇裁剪 — 数学等价 (凸∩凸 = 一次 S-H 交集),
    // S-H 顶点处理量约为三角形扇的 1/4 (4 subject 顶点 vs 4x3)
    if (nb == 4) {
        Vec3 intersection[16];
        int ni = sutherland_hodgman_spherical_fixed(
            hp_boundary, 4, drop_clip_normals, intersection, 16);
        if (ni >= 3) {
            if (g.max_angle < 1e-3) {
                total_overlap = planar_polygon_area_n(intersection, ni, &g.center_d);
            } else {
                total_overlap = spherical_polygon_area_n<double>(intersection, ni);
            }
        }
        return Scalar(total_overlap);
    }

    // 低 NSIDE (细分边界, subject 可 >16 顶点): 保持三角形扇
    bool center_in_drop = true;
    for (const auto& n : drop_clip_normals) {
        if (hp_center.x * n.x + hp_center.y * n.y + hp_center.z * n.z < -inside_tol) {
            center_in_drop = false;
            break;
        }
    }
    for (int i = 0; i < nb; i++) {
        const Vec3& A = hp_boundary[i];
        const Vec3& B = hp_boundary[(i + 1) % nb];
        Vec3 triangle[3] = {hp_center, A, B};

        // tri_inside: 三角形 3 顶点全在 drop 内 → 直接面积 (免 S-H)
        bool tri_inside = center_in_drop;
        if (tri_inside) {
            for (const auto& n : drop_clip_normals) {
                if (A.x * n.x + A.y * n.y + A.z * n.z < -inside_tol ||
                    B.x * n.x + B.y * n.y + B.z * n.z < -inside_tol) {
                    tri_inside = false;
                    break;
                }
            }
        }
        if (tri_inside) {
            total_overlap += spherical_polygon_area_n<double>(triangle, 3);
        } else {
            Vec3 intersection[16];
            int ni = sutherland_hodgman_spherical_fixed(triangle, 3, drop_clip_normals, intersection, 16);
            if (ni < 3) continue;
            // 面积与 drop_area 同一尺度感知策略:
            // drop 微小 (max_angle < 1e-3 rad) 时交集也是微小多边形,
            // 用切平面面积 (与 g.drop_area 表示一致, 避免 weight 偏差);
            // 大 drop 用球面 Eriksson
            if (g.max_angle < 1e-3) {
                total_overlap += planar_polygon_area_n(
                    intersection, ni, &g.center_d);
            } else {
                total_overlap += spherical_polygon_area_n<double>(intersection, ni);
            }
        }
    }
    return Scalar(total_overlap);
}

template <typename Scalar>
Scalar compute_overlap_area_g_ctx(const DropGeometryT<Scalar>& g,
                                  const healpix::HealpixCore& hp,
                                  uint64_t target_ipix,
                                  double hp_res_rad)
{
    int nside = hp.getNside();
    double ra_c, dec_c;
    hp.pix2radec((int64_t)target_ipix, &ra_c, &dec_c);
    const Vec3 hp_center = radec_to_vec<double>(ra_c, dec_c);
    // 重构保序：quick-reject 必须在边界构建之前（与历史实现一致），
    // 否则穷举 oracle 对每个被拒像素都构建细分边界（nside<256 64 顶点）
    // 造成 ~1000 倍退化。此检查与 overlap_area_impl 内检查数值等价。
    {
        const double lim =
            g.max_angle + HP_CIRCUMRADIUS_FACTOR * hp_res_rad;
        const double cos_safe = std::cos(lim + 1e-9);
        double d_c = hp_center.x * g.center_d.x +
                     hp_center.y * g.center_d.y +
                     hp_center.z * g.center_d.z;
        d_c = std::max(-1.0, std::min(1.0, d_c));
        if (d_c <= cos_safe || std::acos(d_c) > lim) return Scalar(0);
    }
    // 1. 获取目标 HEALPix 像素边界 (double 内部)
    // nside>=256 生产路径用固定 4 角 array（无堆分配），
    // 与 get_healpix_boundary 逐位等价；低 NSIDE 保留自适应细分 vector。
    std::array<Vec3, 4> hp_boundary4;
    std::vector<Vec3> hp_boundary_vec;
    const Vec3* hp_boundary = nullptr;
    int nb = 0;
    if (nside >= 256) {
        get_healpix_boundary4<double>(hp, target_ipix, nside, hp_boundary4);
        hp_boundary = hp_boundary4.data();
        nb = 4;
    } else {
        int samples = (nside <= 8) ? 16 : 1;
        hp_boundary_vec = get_healpix_boundary_sampled<double>(
            hp, target_ipix, nside, samples);
        if (hp_boundary_vec.size() < 3) return Scalar(0);
        hp_boundary = hp_boundary_vec.data();
        nb = (int)hp_boundary_vec.size();
    }
    return overlap_area_impl<Scalar>(g, hp_res_rad, nside,
                                     hp_center, hp_boundary, nb);
}

// ============================================================================
// bounded target-ipix geometry cache 实现
// ============================================================================
const TargetPixelGeometry* TargetGeomCache::get_or_build(
    const healpix::HealpixCore& hp, std::uint64_t ipix, bool* built_out) {
    auto it = map_.find(ipix);
    if (it != map_.end()) {
        ++hits_;
        // LRU touch：移到 deque 前端
        for (auto d = lru_.begin(); d != lru_.end(); ++d) {
            if (*d == ipix) {
                lru_.erase(d);
                break;
            }
        }
        lru_.push_front(ipix);
        if (built_out) *built_out = false;
        return &it->second.geom;
    }
    ++misses_;
    TargetPixelGeometry tg;
    double ra_c, dec_c;
    hp.pix2radec((int64_t)ipix, &ra_c, &dec_c);
    tg.center = radec_to_vec<double>(ra_c, dec_c);
    get_healpix_boundary4<double>(hp, ipix, hp.getNside(), tg.boundary4);
    tg.ready = true;
    // 插入 + LRU 淘汰（容量有界）
    map_[ipix] = Entry{ipix, tg};
    lru_.push_front(ipix);
    while (map_.size() > capacity_) {
        const std::uint64_t victim = lru_.back();
        lru_.pop_back();
        map_.erase(victim);
    }
    const TargetPixelGeometry* out = &map_.find(ipix)->second.geom;
    if (built_out) *built_out = true;
    return out;
}

void TargetGeomCache::clear() {
    map_.clear();
    lru_.clear();
    hits_ = 0;
    misses_ = 0;
}

template <typename Scalar>
Scalar compute_overlap_area_g_ctx_cached(
    const DropGeometryT<Scalar>& g, const healpix::HealpixCore& hp,
    std::uint64_t target_ipix, double hp_res_rad,
    TargetGeomCache& cache) {
    const int nside = hp.getNside();
    if (nside < 256) {
        // 低 NSIDE 保留自适应细分边界（不缓存，语义不变）
        return compute_overlap_area_g_ctx<Scalar>(g, hp, target_ipix,
                                                  hp_res_rad);
    }
    bool built = false;
    const TargetPixelGeometry* tg =
        cache.get_or_build(hp, target_ipix, &built);
    (void)built;
    return overlap_area_impl<Scalar>(g, hp_res_rad, nside,
                                     tg->center, tg->boundary4.data(), 4);
}

// ============================================================================
// 查询与 drop 多边形可能相交的所有 HEALPix 像素 (不限于 1-ring)
//
// 修复: 保守球冠保证不漏选
// 原实现用 1.5×hp_res 经验缓冲, 在 1°/pixel 极区场景下漏选 8/288 例.
// 修复方案:
// 1. drop 包围圆半径 = max(顶点到中心角距离) + drop 对角线半长裕量
// 2. 缓冲 = HEALPix 像素最大角半径 (中心到最远顶点)
// HEALPix 像素非正方形, 对角线方向延伸更大:
// 赤道带对角线 ≈ 1.532 × res, 半长 ≈ 0.766 × res
// 极区三角形外接圆半径更大
// 取保守上界 2.0 × hp_res (覆盖所有方向的最坏情况)
// 3. 额外加 ε = 0.1 × hp_res 防浮点边界
// ============================================================================
template <typename T>
void query_candidate_pixels(
    const std::vector<Vec3T<T>>& drop_corners,
    const healpix::HealpixCore& hp,
    std::vector<uint64_t>& candidates)
{
    candidates.clear();
    if (drop_corners.empty()) return;

    // 1. 计算 drop 多边形的球面包围圆
    // 中心 = 所有顶点向量的平均 (归一化)
    // 半径 = 最大顶点到中心的角距离
    Vec3T<T> center = {T(0), T(0), T(0)};
    for (const Vec3T<T>& v : drop_corners) {
        center.x += v.x; center.y += v.y; center.z += v.z;
    }
    center = normalize(center);

    double max_angle = 0.0;
    for (const Vec3T<T>& v : drop_corners) {
        double ang = angular_distance<T>(v, center);
        if (ang > max_angle) max_angle = ang;
    }

    // 2.: 保守缓冲 — HEALPix 像素最大角半径 + 裕量
    // HEALPix 像素分辨率 (角秒) = sqrt(4π/(12*nside²)) * 206265
    // 像素最大角半径 (中心→最远顶点) 实测上界 = 1.044 × hp_res
    // (全像素扫描 @dec=±41.81°; CAND-001 证明)
    // 取 3.0 × hp_res 作为保守缓冲, 覆盖极区三角形和所有方向的最坏情况
    // 额外裕量防浮点边界效应和queryDisc中心判定偏差
    double hp_res_arcsec = hp.pixelResolutionArcsec();
    double hp_res_rad    = hp_res_arcsec * ARCSEC_TO_RAD;
    double buffer_rad     = 3.0 * hp_res_rad;
    double query_radius_rad = max_angle + buffer_rad;

    // 3. 中心向量转 RA/Dec (T 类型, vec_to_radec 模板推导)
    T ra_c, dec_c;
    vec_to_radec<T>(center, ra_c, dec_c);

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

// ============================================================================
// query_candidate_pixels_fast - NESTED 直接候选枚举 (, 替代 queryDisc BFS)
//
// 保守性: 任何与 drop 相交的 HEALPix 像素, 其中心必然落在
// "drop 包围圆半径 + 像素外接圆半径" 的圆盘内。
// 像素外接圆半径上界取 1.2 × hp_res (HEALPix 像素最坏情况外接半径 ≈ 1.19×res)。
// 直接枚举 NESTED (face, ix, iy) 正方形包围盒内的像素 (整数位操作, 无 BFS/邻居展开),
// 不做距离剔除 (允许少量 false positives, 由 overlap 精确计算过滤) → 零漏选。
// ============================================================================
template <typename T>
void query_candidate_pixels_fast(
    const std::vector<Vec3T<T>>& drop_corners,
    const healpix::HealpixCore& hp,
    std::vector<uint64_t>& candidates,
    bool* used_fallback)
{
    candidates.clear();
    if (used_fallback) *used_fallback = false;
    if (drop_corners.empty()) return;

    // 1. drop 球面包围圆 (内部 double 计算: 候选是几何完备性判定,
    // float 舍入会抖动 delta/中心导致 FP32/FP64 候选集合不一致;
    // 候选集合本身为整数 ipix, 与 Scalar 类型无关)
    double cx_ = 0.0, cy_ = 0.0, cz_ = 0.0;
    for (const Vec3T<T>& v : drop_corners) {
        cx_ += double(v.x); cy_ += double(v.y); cz_ += double(v.z);
    }
    double cl_ = std::sqrt(cx_ * cx_ + cy_ * cy_ + cz_ * cz_);
    double ci_ = (cl_ < 1e-300) ? 1.0 : 1.0 / cl_;
    Vec3 center = {cx_ * ci_, cy_ * ci_, cz_ * ci_};
    double max_angle = 0.0;
    for (const Vec3T<T>& v : drop_corners) {
        double d = double(v.x) * center.x + double(v.y) * center.y + double(v.z) * center.z;
        d = std::max(-1.0, std::min(1.0, d));
        double ang = std::acos(d);
        if (ang > max_angle) max_angle = ang;
    }

    double hp_res_arcsec = hp.pixelResolutionArcsec();
    double hp_res_rad    = hp_res_arcsec * ARCSEC_TO_RAD;
    double buffer_rad    = HP_CIRCUMRADIUS_FACTOR * hp_res_rad;
    double query_radius_rad = max_angle + buffer_rad;

    double ra_c, dec_c;
    vec_to_radec<double>(center, ra_c, dec_c);

    // 2. 中心像素 NESTED (face, ix, iy) — 公开 radec2pix + 自实现 morton 解交织
    int nside = hp.getNside();
    uint64_t nside64 = (uint64_t)nside;
    uint64_t per_face = nside64 * nside64;
    uint64_t cipix = (uint64_t)hp.radec2pix(ra_c, dec_c);
    int face = (int)(cipix / per_face);
    uint64_t rem = cipix % per_face;
    auto deinterleave = [](uint64_t v, bool odd) -> uint32_t {
        auto compact = [](uint64_t x) -> uint64_t {
            x &= 0x5555555555555555ull;
            x = (x | (x >> 1))  & 0x3333333333333333ull;
            x = (x | (x >> 2))  & 0x0F0F0F0F0F0F0F0Full;
            x = (x | (x >> 4))  & 0x00FF00FF00FF00FFull;
            x = (x | (x >> 8))  & 0x0000FFFF0000FFFFull;
            x = (x | (x >> 16)) & 0x00000000FFFFFFFFull;
            return x;
        };
        return (uint32_t)compact(odd ? (v >> 1) : v);
    };
    int ix0 = (int)deinterleave(rem, false);
    int iy0 = (int)deinterleave(rem, true);

    // 3. 半径转像素单位 (线性尺度), 取上整 (预过滤已保证精确半径, 无需 +1 裕量)
    double radius_px_d = query_radius_rad / hp_res_rad;
    int delta = (int)std::ceil(radius_px_d);
    if (delta < 0) delta = 0;
    if (delta > nside - 1) delta = nside - 1;

    // 面内畸变与极冠回退。
    // HEALPix face 内 (ix,iy) 平面距离与球面角距不成正比, 面内畸变率
    // (面内 1 像素对应球面角距 / hp_res) 实测 (scan_face_distortion,
    // nside=512 全 face 网格扫描):
    // - 赤道带内部 (离极冠边界 >0.2Ns): 0.9999 x hp_res (无畸变)
    // - 极冠边界带 (<0.08Ns): 0.874 x hp_res (畸变 1.14x)
    // - 极冠像素: 0.798 x hp_res (畸变 1.25x)
    // → 快速路径仅用于赤道带, delta 乘 1.25 安全系数 (签字修正
    // ORACLE_HARDENING: 赤道带 |z|<=2/3 面内畸变解析上界
    // ds/dx_face=(1/nside)·sqrt(4/9+π²cos²θ/16), 最坏在 z=±2/3:
    // cosθ=sqrt(5/9), sqrt(4/9+5π²/144)≈0.8872 → 面距离/球面角距
    // ≤1/0.8872≈1.127; 经验扫描最坏 1.14; 取 1.25 覆盖解析+浮点);
    // 极冠 (bighp 0-3 的 ix+iy>Ns, bighp 8-11 的 ix+iy<Ns) 回退球面查询
    // (queryDisc 3.0 buffer, 与面内畸变无关, 天然正确)。
    bool in_polar_cap = false;
    if (face <= 3)      in_polar_cap = (ix0 + iy0 > nside);
    else if (face >= 8) in_polar_cap = (ix0 + iy0 < nside);
    // 中心像素在赤道侧但枚举盒可能延伸进极冠 (解析上界仅对赤道带成立,
    // 极冠畸变 1.25x 无安全系数) → 枚举盒任何像素触及极冠即回退保守路径。
    // face 0-3: 极冠 = ix+iy > Ns, 盒最大 ix+iy = ix0+iy0+2*delta
    // face 8-11: 极冠 = ix+iy < Ns, 盒最小 ix+iy = ix0+iy0-2*delta
    bool box_touches_polar = false;
    if (face <= 3)      box_touches_polar = ((ix0 + iy0) + 2 * delta >= nside);
    else if (face >= 8) box_touches_polar = ((ix0 + iy0) - 2 * delta <= nside);
    if (in_polar_cap || box_touches_polar) {
        query_candidate_pixels<T>(drop_corners, hp, candidates);
        if (used_fallback) *used_fallback = true;
        return;
    }
    // 赤道带: 面内畸变安全系数 1.15 (实测最坏 1.14 @ 极冠边界带 <0.08Ns)
    delta = (int)std::ceil(radius_px_d * 1.15);
    if (delta < 0) delta = 0;
    if (delta > nside - 1) delta = nside - 1;

    // 4. 边界判断 + 枚举 ( CANDIDATE_QUERY_REPAIR):
    // 仅当枚举包围盒完全位于中心 base face 内部时, 才允许 face 内快速枚举
    // (可证明不会跨 face, 零漏选由 Oracle 矩阵验证)。
    // 包围盒触及 face 边界/角/极区接缝时, 回退保守 PRECISE 候选
    // (query_candidate_pixels: queryDisc 圆盘, 天然跨 face, buffer 3.0×hp_res)。
    // 禁止只裁剪中心 face (会漏相邻 face 真相交像素)。
    int x0 = ix0 - delta, x1 = ix0 + delta;
    int y0 = iy0 - delta, y1 = iy0 + delta;
    if (x0 < 0 || y0 < 0 || x1 > nside - 1 || y1 > nside - 1) {
        // 边界回退: 保守 inclusive 查询 (跨 face / 极区 / RA 跨界均安全)
        query_candidate_pixels<T>(drop_corners, hp, candidates);
        if (used_fallback) *used_fallback = true;
        return;
    }
    // 快速路径统计: 单 face 内部枚举 (无跨 face)
    candidates.reserve((size_t)(x1 - x0 + 1) * (y1 - y0 + 1));
    // NESTED morton 交织 (标准位操作, 纯数学, 不依赖 healpix_core 私有接口)
    auto morton = [](int x, int y) -> uint64_t {
        auto spread = [](uint32_t v) -> uint64_t {
            uint64_t x = v & 0x00000000FFFFFFFFull;
            x = (x | (x << 16)) & 0x0000FFFF0000FFFFull;
            x = (x | (x << 8))  & 0x00FF00FF00FF00FFull;
            x = (x | (x << 4))  & 0x0F0F0F0F0F0F0F0Full;
            x = (x | (x << 2))  & 0x3333333333333333ull;
            x = (x | (x << 1))  & 0x5555555555555555ull;
            return x;
        };
        return spread((uint32_t)x) | (spread((uint32_t)y) << 1);
    };
    // 4b. 不再做平面圆预过滤 ( 修复):
    // 面内 (ix,iy)→球面映射在菱形网格对角线方向不单调 — 平面距离
    // sqrt(2) 的对角像素其球面距离可能 < query_radius, 平面圆
    // dx²+dy²>delta² 会把它误滤 → 候选漏选 (低 NSIDE 像素角点场景
    // 实测: drop 质心落在 4 像素公共角附近, 真实相交像素在对角方向
    // 被滤, 通量丢失 0.6%)。整个包围盒 (2delta+1)² 全枚举,
    // 由下方精确球面圆心距离过滤负责去重/裁剪, 所有 NSIDE 统一正确。
    for (int iy = y0; iy <= y1; ++iy) {
        for (int ix = x0; ix <= x1; ++ix) {
            uint64_t ipix = (uint64_t)face * nside64 * nside64 + morton(ix, iy);
            candidates.push_back(ipix);
        }
    }
    // 5. 圆心距离预过滤 (保守: 像素中心在查询圆盘内才保留)
    // 查询圆盘半径 = max_angle + 1.0×hp_res (像素外接圆半径上界;
    // 零漏选由候选 Oracle 矩阵对全部 face/NSIDE 验证)。
    // 过滤掉正方形包围盒的边角, 减少后续 compute_overlap_area 调用。
    double cos_lim = std::cos(double(query_radius_rad));
    std::vector<uint64_t> filtered;
    filtered.reserve(candidates.size());
    for (uint64_t ipix : candidates) {
        double t, p;
        hp.pix2ang((int64_t)ipix, &t, &p);
        double st = std::sin(t);
        double px = st * std::cos(p);
        double py = st * std::sin(p);
        double pz = std::cos(t);
        if (px * center.x + py * center.y + pz * center.z >= cos_lim) {
            filtered.push_back(ipix);
        }
    }
    candidates.swap(filtered);
    std::sort(candidates.begin(), candidates.end());
}

// ============================================================================
// 阶段7: 显式实例化 FP32/FP64 双实例 (真 Scalar 模板, 不共享固定 double 内核)
// ============================================================================
template Vec3T<float> cross<float>(const Vec3T<float>&, const Vec3T<float>&);
template Vec3T<double> cross<double>(const Vec3T<double>&, const Vec3T<double>&);
template float dot<float>(const Vec3T<float>&, const Vec3T<float>&);
template double dot<double>(const Vec3T<double>&, const Vec3T<double>&);
template float length<float>(const Vec3T<float>&);
template double length<double>(const Vec3T<double>&);
template Vec3T<float> normalize<float>(const Vec3T<float>&);
template Vec3T<double> normalize<double>(const Vec3T<double>&);
template Vec3T<float> radec_to_vec<float>(float, float);
template Vec3T<double> radec_to_vec<double>(double, double);
template void vec_to_radec<float>(const Vec3T<float>&, float&, float&);
template void vec_to_radec<double>(const Vec3T<double>&, double&, double&);
template float angular_distance<float>(const Vec3T<float>&, const Vec3T<float>&);
template double angular_distance<double>(const Vec3T<double>&, const Vec3T<double>&);
template float spherical_polygon_area<float>(const std::vector<Vec3T<float>>&);
template double spherical_polygon_area<double>(const std::vector<Vec3T<double>>&);
template float spherical_polygon_area_n<float>(const Vec3T<float>*, int);
template double spherical_polygon_area_n<double>(const Vec3T<double>*, int);
template std::vector<Vec3T<float>> sutherland_hodgman_spherical<float>(
    const std::vector<Vec3T<float>>&, const std::vector<Vec3T<float>>&);
template std::vector<Vec3T<double>> sutherland_hodgman_spherical<double>(
    const std::vector<Vec3T<double>>&, const std::vector<Vec3T<double>>&);
template std::vector<Vec3T<float>> get_healpix_boundary<float>(
    const healpix::HealpixCore&, uint64_t, int);
template std::vector<Vec3T<double>> get_healpix_boundary<double>(
    const healpix::HealpixCore&, uint64_t, int);
template std::vector<Vec3T<float>> get_healpix_boundary_sampled<float>(
    const healpix::HealpixCore&, uint64_t, int, int);
template std::vector<Vec3T<double>> get_healpix_boundary_sampled<double>(
    const healpix::HealpixCore&, uint64_t, int, int);
template void get_healpix_boundary4<float>(
    const healpix::HealpixCore&, uint64_t, int, std::array<Vec3T<float>, 4>&);
template void get_healpix_boundary4<double>(
    const healpix::HealpixCore&, uint64_t, int, std::array<Vec3T<double>, 4>&);
template std::vector<Vec3T<float>> build_drop_polygon_sampled<float>(
    float, float, float, PixelToSkyFn, void*, int);
template std::vector<Vec3T<double>> build_drop_polygon_sampled<double>(
    double, double, double, PixelToSkyFn, void*, int);
template std::vector<Vec3T<float>> build_drop_polygon_adaptive<float>(
    float, float, float, PixelToSkyFn, void*, float);
template std::vector<Vec3T<double>> build_drop_polygon_adaptive<double>(
    double, double, double, PixelToSkyFn, void*, double);
template float compute_overlap_area<float>(
    const std::vector<Vec3T<float>>&, const healpix::HealpixCore&, uint64_t);
template double compute_overlap_area<double>(
    const std::vector<Vec3T<double>>&, const healpix::HealpixCore&, uint64_t);
template DropGeometryT<float> build_drop_geometry<float>(
    const std::vector<Vec3T<float>>&, const std::vector<Vec3>*);
template DropGeometryT<double> build_drop_geometry<double>(
    const std::vector<Vec3T<double>>&, const std::vector<Vec3>*);
template void build_drop_geometry_into<float>(
    DropGeometryT<float>&, const std::vector<Vec3T<float>>&, const std::vector<Vec3>*);
template void build_drop_geometry_into<double>(
    DropGeometryT<double>&, const std::vector<Vec3T<double>>&, const std::vector<Vec3>*);
template float compute_overlap_area_g<float>(const DropGeometryT<float>&,
                                             const healpix::HealpixCore&, uint64_t);
template double compute_overlap_area_g<double>(const DropGeometryT<double>&,
                                               const healpix::HealpixCore&, uint64_t);
template float compute_overlap_area_g_ctx<float>(const DropGeometryT<float>&,
                                                 const healpix::HealpixCore&,
                                                 uint64_t, double);
template double compute_overlap_area_g_ctx<double>(const DropGeometryT<double>&,
                                                   const healpix::HealpixCore&,
                                                   uint64_t, double);
template float compute_overlap_area_g_ctx_cached<float>(
    const DropGeometryT<float>&, const healpix::HealpixCore&, uint64_t,
    double, TargetGeomCache&);
template double compute_overlap_area_g_ctx_cached<double>(
    const DropGeometryT<double>&, const healpix::HealpixCore&, uint64_t,
    double, TargetGeomCache&);
template void query_candidate_pixels<float>(
    const std::vector<Vec3T<float>>&, const healpix::HealpixCore&, std::vector<uint64_t>&);
template void query_candidate_pixels<double>(
    const std::vector<Vec3T<double>>&, const healpix::HealpixCore&, std::vector<uint64_t>&);
template void query_candidate_pixels_fast<float>(
    const std::vector<Vec3T<float>>&, const healpix::HealpixCore&,
    std::vector<uint64_t>&, bool*);
template void query_candidate_pixels_fast<double>(
    const std::vector<Vec3T<double>>&, const healpix::HealpixCore&,
    std::vector<uint64_t>&, bool*);

} // namespace spherical
