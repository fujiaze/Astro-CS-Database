#include "poly_clip.h"

#include <cstdio>
#include <cmath>
#include <cfloat>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace drizzle {

// ============================================================================
// 内部辅助: 角度转弧度 / 弧度转角度
// ============================================================================
static inline double deg2rad(double deg) { return deg * M_PI / 180.0; }
static inline double rad2deg(double rad) { return rad * 180.0 / M_PI; }

// 日志宏: 默认关闭，仅在定义 POLY_CLIP_DEBUG 时输出
#ifdef POLY_CLIP_DEBUG
#define POLY_LOG(fmt, ...) \
    fprintf(stderr, "[poly_clip] " fmt "\n", ##__VA_ARGS__)
#else
#define POLY_LOG(fmt, ...) do {} while(0)
#endif

// ============================================================================
// 1. Gnomonic 正向投影 (球面 → 切平面)
//
// 公式 (Calabretta & Greisen 2002, TAN 投影):
// cos(c) = sin(dec0)*sin(dec) + cos(dec0)*cos(dec)*cos(ra-ra0)
// x = cos(dec)*sin(ra-ra0) / cos(c)
// y = (cos(dec0)*sin(dec) - sin(dec0)*cos(dec)*cos(ra-ra0)) / cos(c)
//
// 输入: ra, dec, ra0, dec0 (度)
// 输出: 切平面坐标 (x, y), 单位弧度
// ============================================================================
Point2D PolyClip::gnomonicForward(double ra, double dec,
                                  double ra0, double dec0) {
    Point2D out;
    // 转弧度
    const double ra_rad  = deg2rad(ra);
    const double dec_rad = deg2rad(dec);
    const double ra0_rad  = deg2rad(ra0);
    const double dec0_rad = deg2rad(dec0);

    const double sdec0 = std::sin(dec0_rad);
    const double cdec0 = std::cos(dec0_rad);
    const double sdec  = std::sin(dec_rad);
    const double cdec  = std::cos(dec_rad);
    const double dra   = ra_rad - ra0_rad;
    const double cdra  = std::cos(dra);
    const double sdra  = std::sin(dra);

    // cos(c) = 点积 (单位向量) = 到切点中心的角距离余弦
    const double cosc = sdec0 * sdec + cdec0 * cdec * cdra;

    // 保护: cos(c) 接近 0 (天极对面, 即距离切点 ~90°) 时投影发散
    // 取 1e-12 作为阈值, 小于此值返回极大值, 标记为发散
    if (std::fabs(cosc) < 1e-12) {
        POLY_LOG("警告: gnomonicForward 投影发散 (cos(c)=%.3e, 距切点近90°)", cosc);
        // 返回有符号的极大值, 方向由分子决定
        const double BIG = 1e6;
        const double num_x = cdec * sdra;
        const double num_y = cdec0 * sdec - sdec0 * cdec * cdra;
        const double sign_x = (num_x >= 0.0) ? 1.0 : -1.0;
        const double sign_y = (num_y >= 0.0) ? 1.0 : -1.0;
        out.x = sign_x * BIG;
        out.y = sign_y * BIG;
        return out;
    }

    out.x = cdec * sdra / cosc;
    out.y = (cdec0 * sdec - sdec0 * cdec * cdra) / cosc;
    return out;
}

// ============================================================================
// 2. Gnomonic 逆向投影 (切平面 → 球面)
//
// 公式:
// rho = sqrt(x*x + y*y)
// c = atan(rho)
// dec = asin(cos(c)*sin(dec0) + y*sin(c)*cos(dec0)/rho)
// ra = ra0 + atan2(x*sin(c), rho*cos(dec0)*cos(c) - y*sin(dec0)*sin(c))
//
// 输入: x, y (弧度), ra0, dec0 (度)
// 输出: SkyCoord (ra, dec, 度)
// ============================================================================
SkyCoord PolyClip::gnomonicReverse(double x, double y,
                                   double ra0, double dec0) {
    SkyCoord out;
    out.ra  = ra0;
    out.dec = dec0;

    // 保护: rho 接近 0 (切点本身) 时直接返回 (ra0, dec0)
    const double rho = std::sqrt(x * x + y * y);
    if (rho < 1e-12) {
        // 切点本身, 无需计算
        return out;
    }

    const double dec0_rad = deg2rad(dec0);
    const double sdec0 = std::sin(dec0_rad);
    const double cdec0 = std::cos(dec0_rad);

    const double c    = std::atan(rho);
    const double sinc = std::sin(c);
    const double cosc = std::cos(c);

    // dec = asin(cos(c)*sin(dec0) + y*sin(c)*cos(dec0)/rho)
    const double sin_dec = cosc * sdec0 + y * sinc * cdec0 / rho;
    // 数值保护: asin 参数可能因浮点误差略微超出 [-1, 1]
    double sin_dec_clamped = sin_dec;
    if (sin_dec_clamped >  1.0) sin_dec_clamped =  1.0;
    if (sin_dec_clamped < -1.0) sin_dec_clamped = -1.0;
    const double dec_rad = std::asin(sin_dec_clamped);

    // ra = ra0 + atan2(x*sin(c), rho*cos(dec0)*cos(c) - y*sin(dec0)*sin(c))
    const double dra = std::atan2(x * sinc,
                                  rho * cdec0 * cosc - y * sdec0 * sinc);
    double ra_rad = deg2rad(ra0) + dra;
    // 归一化 ra 到 [0, 2π)
    while (ra_rad < 0.0)        ra_rad += 2.0 * M_PI;
    while (ra_rad >= 2.0 * M_PI) ra_rad -= 2.0 * M_PI;

    out.ra  = rad2deg(ra_rad);
    out.dec = rad2deg(dec_rad);
    return out;
}

// ============================================================================
// 3. Sutherland-Hodgman 多边形裁剪
//
// 标准算法:
// - 对裁剪窗口 clip 的每条边, 依次裁剪 subject 多边形
// - 每条边由两个点 (e1, e2) 定义, 逆时针窗口的左侧为内侧
// - 判断点 p 在内侧: 叉积 (e2-e1) × (p-e1) >= 0
// - 边的交点计算: 线性插值
//
// 边界情况:
// - subject 为空 → 返回空
// - clip 顶点 < 3 → 返回 subject (无法形成裁剪窗口)
// ============================================================================
namespace {

// 计算叉积 (e2-e1) × (p-e1)
// > 0: p 在边 (e1→e2) 的左侧 (内侧, 逆时针窗口)
// < 0: p 在右侧 (外侧)
// = 0: p 在边上
inline double crossProduct(const Point2D& e1, const Point2D& e2,
                           const Point2D& p) {
    const double ex = e2.x - e1.x;
    const double ey = e2.y - e1.y;
    const double px = p.x  - e1.x;
    const double py = p.y  - e1.y;
    return ex * py - ey * px;
}

// 计算线段 (s→e) 与裁剪边 (e1→e2) 的交点
// 使用参数 t = cross(e1, e2, s) / cross(e1, e2, s-e)
// 即 s + t*(e-s) 落在边 (e1→e2) 上
inline Point2D intersect(const Point2D& s, const Point2D& e,
                         const Point2D& e1, const Point2D& e2) {
    // 使用叉积判断
    const double dc = crossProduct(e1, e2, e);  // e 相对边的位置
    const double ds = crossProduct(e1, e2, s);  // s 相对边的位置
    // 参数 t: s→e 上, 与裁剪边的交点位置
    // 若 s 在外侧 (ds<0), e 在内侧 (dc>=0), t = ds / (ds - dc)
    const double denom = ds - dc;
    double t;
    if (std::fabs(denom) < DBL_EPSILON) {
        // 平行或重合, 取中点
        t = 0.5;
    } else {
        t = ds / denom;
    }
    Point2D out;
    out.x = s.x + t * (e.x - s.x);
    out.y = s.y + t * (e.y - s.y);
    return out;
}

} // anonymous namespace

std::vector<Point2D> PolyClip::clipPolygon(const std::vector<Point2D>& subject,
                                           const std::vector<Point2D>& clip) {
    // 边界: subject 为空 → 返回空
    if (subject.empty()) {
        return std::vector<Point2D>();
    }
    // 边界: clip 顶点 < 3 → 返回 subject (无法形成裁剪窗口)
    if (clip.size() < 3) {
        return subject;
    }

    // 双 buffer 交替优化: 避免每条裁剪边的 vector 拷贝
    // buf_a 初始为 subject, buf_b 为空
    // 每条裁剪边: buf_a → buf_b, 然后 swap
    // 最终结果在 buf_a (最后一次 swap 后)
    std::vector<Point2D> buf_a(subject.begin(), subject.end());
    std::vector<Point2D> buf_b;
    // 预留空间: 裁剪后顶点数 ≤ subject.size() + clip.size()
    buf_a.reserve(subject.size() + clip.size());
    buf_b.reserve(subject.size() + clip.size());

    const size_t nclip = clip.size();
    for (size_t i = 0; i < nclip; ++i) {
        const Point2D& e1 = clip[i];
        const Point2D& e2 = clip[(i + 1) % nclip];

        // 若当前输入已空, 提前终止
        if (buf_a.empty()) {
            break;
        }

        // buf_a 为输入, buf_b 为输出
        std::vector<Point2D>& input = buf_a;
        std::vector<Point2D>& output = buf_b;
        output.clear();

        const size_t ninput = input.size();
        for (size_t j = 0; j < ninput; ++j) {
            const Point2D& s = input[j];                   // 当前点
            const Point2D& e = input[(j + 1) % ninput];    // 下一点

            const double cross_s = crossProduct(e1, e2, s);  // s 在边内侧?
            const double cross_e = crossProduct(e1, e2, e);  // e 在边内侧?

            const bool s_inside = (cross_s >= 0.0);
            const bool e_inside = (cross_e >= 0.0);

            if (s_inside) {
                if (e_inside) {
                    // s 内, e 内: 只输出 e
                    output.push_back(e);
                } else {
                    // s 内, e 外: 输出交点 (s→e 与裁剪边)
                    output.push_back(intersect(s, e, e1, e2));
                }
            } else {
                if (e_inside) {
                    // s 外, e 内: 输出交点 + e
                    output.push_back(intersect(s, e, e1, e2));
                    output.push_back(e);
                } else {
                    // s 外, e 外: 不输出
                }
            }
        }

        // 交换 buffer: 下一轮 buf_b(有数据) 为输入, buf_a(已清空) 为输出
        std::swap(buf_a, buf_b);
    }

    // 最终结果在 buf_a (最后一次 swap 后, 有数据的 buffer)
    return buf_a;
}

// ============================================================================
// 4. Shoelace 公式计算多边形面积
//
// area = 0.5 * |Σ (x_i * y_{i+1} - x_{i+1} * y_i)|
//
// 逆时针方向时 Σ 为正, 顺时针为负, 取绝对值保证非负。
// 顶点数 < 3 时返回 0; 自动闭合 (最后一个点连接到第一个点)。
// ============================================================================
double PolyClip::polygonArea(const std::vector<Point2D>& poly) {
    const size_t n = poly.size();
    if (n < 3) {
        // 顶点数 < 3 无法形成多边形
        return 0.0;
    }

    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const Point2D& a = poly[i];
        const Point2D& b = poly[(i + 1) % n];  // 自动闭合
        sum += a.x * b.y - b.x * a.y;
    }

    const double area = 0.5 * std::fabs(sum);
    POLY_LOG("polygonArea: 顶点数=%zu, 面积=%.6e", n, area);
    return area;
}

} // namespace drizzle
