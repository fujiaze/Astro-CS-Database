#ifndef HEALPIX_CORE_H
#define HEALPIX_CORE_H

// ============================================================================
// HEALpix 像素运算核心 (自实现, 不依赖 healpix-c++ 外部库)
//
// 参考: Gorski et al. 2005, "HEALPix: A Framework for High-Resolution
// Diffuse Analysis of Full-Sky Data"
//
// 支持两种像素排列:
// - NESTED: 用 z-order (Morton) 位编码, 适合树状层级 (LOD)
// - RING: 按纬度环排列, 适合球谐分析
//
// 约定:
// - theta: 极角 (0=北极, π=南极), 单位弧度
// - phi: 方位角 (0~2π), 单位弧度
// - RA: 赤经 (度), 0~360
// - Dec: 赤纬 (度), -90~+90
// - nside 必须是 2 的幂
// ============================================================================

#include <cstdint>
#include <vector>

namespace healpix {

class HealpixCore {
public:
    // 构造: nside 必须为 2 的幂; nested=true 用 NESTED, false 用 RING
    HealpixCore(int nside, bool nested = true);

    int     getNside() const;
    bool    isNested() const;
    int64_t getNpix() const;  // 12 * nside^2

    // 天球坐标(弧度) ↔ 像素号
    // theta: 0=北极, π=南极; phi: 0~2π
    int64_t ang2pix(double theta_rad, double phi_rad) const;
    void    pix2ang(int64_t ipix, double* theta_rad, double* phi_rad) const;

    // RA/Dec(度) ↔ 像素号
    int64_t radec2pix(double ra_deg, double dec_deg) const;
    void    pix2radec(int64_t ipix, double* ra_deg, double* dec_deg) const;

    // 像素分辨率(角秒) = sqrt(4π / (12*nside²)) * 206265
    double pixelResolutionArcsec() const;

    // 查询某像素的邻居 (返回 4 或 8 个, 边界处可能更少)
    std::vector<int64_t> neighbors(int64_t ipix) const;

    // 查询天区范围内的所有像素
    // ra_deg/dec_deg 中心, radius_arcsec 半径
    std::vector<int64_t> queryDisc(double ra_deg, double dec_deg,
                                   double radius_arcsec) const;

    // nside 转换 (用于 LOD): 当前 nside 的像素 → 粗 nside 的像素
    int64_t pixelToCoarse(int64_t ipix_fine, int nside_coarse) const;
    // 粗 nside 像素 → 当前 nside 的所有子像素
    std::vector<int64_t> pixelToFine(int64_t ipix_coarse, int nside_fine) const;

private:
    int  m_nside;
    bool m_nested;
    int  m_nsideBits;  // log2(nside)

    // ---- 内部 XY 方案 (bighp * nside² + x*nside + y) ----
    // ang → (bighp, x, y)
    void ang2xy(double theta, double phi,
                int* bighp, int* x, int* y) const;
    // (bighp, x, y) → ang
    void xy2ang(int bighp, int x, int y,
                double* theta, double* phi) const;

    // XY ↔ NESTED (位交织 / Morton 码)
    int64_t xy2nest(int bighp, int x, int y) const;
    void    nest2xy(int64_t ipix, int* bighp, int* x, int* y) const;

    // XY ↔ RING
    int64_t xy2ring(int bighp, int x, int y) const;
    void    ring2xy(int64_t ipix, int* bighp, int* x, int* y) const;

    // RING scheme 辅助 (对外接口要求)
    int64_t ring2pix(int ring, int phi_idx) const;
    void    pix2ring(int64_t ipix, int* ring, int* phi_idx) const;

    // NESTED ↔ RING 转换
    int64_t nest2ring(int64_t inest) const;
    int64_t ring2nest(int64_t iring) const;

    // 像素 → (bighp, x, y) (按当前 scheme)
    void pix2xy(int64_t ipix, int* bighp, int* x, int* y) const;
    // (bighp, x, y) → 像素 (按当前 scheme)
    int64_t xy2pix(int bighp, int x, int y) const;
};

} // namespace healpix

#endif // HEALPIX_CORE_H
