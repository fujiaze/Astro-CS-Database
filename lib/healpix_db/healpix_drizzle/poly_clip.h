#ifndef POLY_CLIP_H
#define POLY_CLIP_H

// ============================================================================
// 多边形裁剪模块 - 局部切平面 Sutherland-Hodgman 算法
//
// 用途:
// 在 HEALPix Drizzle 流水线中, 计算球面像素四边形与 HEALPix 像素四边形的
// 重叠面积。为避免球面几何复杂度, 先将两者投影到以某点为中心的局部切平面,
// 再用 Sutherland-Hodgman 多边形裁剪算法求交集, 最后用 Shoelace 公式算面积。
//
// 参考:
// - Calabretta & Greisen 2002 (TAN 投影 / Gnomonic)
// - Sutherland & Hodgman 1974 (多边形裁剪)
// ============================================================================

#include <cstdint>
#include <vector>

namespace drizzle {

// 2D 点 (切平面坐标, 弧度)
struct Point2D {
    double x, y;
};

// 球面坐标 (RA/Dec, 度)
struct SkyCoord {
    double ra, dec;
};

// 多边形裁剪器 - 局部切平面 Sutherland-Hodgman 算法
class PolyClip {
public:
    // 将球面坐标投影到以 (ra0, dec0) 为中心的切平面 (gnomonic 正向投影)
    // 输入: ra, dec, ra0, dec0 (单位: 度)
    // 输出: 切平面坐标 (x, y), 单位: 弧度 (近似角秒/206265)
    // 保护: cos(c) 接近 0 时 (天极对面) 返回极大值
    static Point2D gnomonicForward(double ra, double dec,
                                   double ra0, double dec0);

    // 切平面坐标反投影到球面坐标 (gnomonic 逆向投影)
    // 输入: x, y (弧度), ra0, dec0 (度)
    // 输出: SkyCoord (ra, dec, 度)
    // 保护: rho 接近 0 时直接返回 (ra0, dec0)
    static SkyCoord gnomonicReverse(double x, double y,
                                    double ra0, double dec0);

    // Sutherland-Hodgman 多边形裁剪
    // subject: 被裁剪多边形顶点 (任意方向, 可凹可凸)
    // clip: 裁剪窗口多边形顶点 (必须是凸多边形, 逆时针)
    // 返回: 裁剪后多边形顶点 (可能为空)
    // 边界情况:
    // - subject 为空时返回空
    // - clip 顶点数 < 3 时返回 subject (无法形成裁剪窗口)
    static std::vector<Point2D> clipPolygon(const std::vector<Point2D>& subject,
                                            const std::vector<Point2D>& clip);

    // Shoelace 公式计算多边形面积 (逆时针为正)
    // 顶点数 < 3 时返回 0
    // 自动闭合 (最后一个点连接到第一个点)
    static double polygonArea(const std::vector<Point2D>& poly);
};

} // namespace drizzle

#endif // POLY_CLIP_H
