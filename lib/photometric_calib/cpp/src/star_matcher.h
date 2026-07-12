#ifndef PC_STAR_MATCHER_H
#define PC_STAR_MATCHER_H

#include <vector>
#include "wcs_transform.h"

namespace pc {

// 单颗星-图匹配结果
struct StarMatch {
    double x;         // 图像像素x (PSF质心)
    double y;         // 图像像素y (PSF质心)
    double f_instr;   // 仪器流量 (PSF flux)
    double f_syn;     // 合成流量 (Gaia)
};

// 星-图匹配器: Gaia星 <-> PSF拟合星
// 暴力最近邻搜索 + MAD离群清洗
class StarMatcher {
public:
    StarMatcher();

    // 匹配 + MAD清洗
    // 参数:
    //   wcs: WCS转换器
    //   gaia_ra/dec/mag/fsyn: Gaia星数组 [n_gaia]
    //   n_gaia: Gaia星数量
    //   psf_cx/cy/flux/status: PSF星数组 [n_psf]
    //   n_psf: PSF星数量
    //   match_radius_px: 匹配半径(像素), 最近邻距离须小于该值
    //   outlier_sigma: 离群阈值(倍sigma)
    // 返回: 清洗后的匹配列表
    std::vector<StarMatch> matchAndClean(
        const WcsTransform& wcs,
        const double* gaia_ra, const double* gaia_dec,
        const double* gaia_mag, const double* gaia_fsyn, int n_gaia,
        const double* psf_cx, const double* psf_cy,
        const double* psf_flux, const int* psf_status, int n_psf,
        double match_radius_px = 3.0,
        double outlier_sigma = 3.0);

private:
    // 暴力最近邻: 对每颗Gaia星找最近的PSF有效星
    // 返回匹配列表(未清洗)
    std::vector<StarMatch> matchBruteForce(
        const WcsTransform& wcs,
        const double* gaia_ra, const double* gaia_dec,
        const double* gaia_mag, const double* gaia_fsyn, int n_gaia,
        const double* psf_cx, const double* psf_cy,
        const double* psf_flux, const int* psf_status, int n_psf,
        double match_radius_px);

    // MAD离群清洗: r=log10(F_instr/F_syn), 剔除|r-median|>sigma*MAD/0.6745
    // 返回清洗后的匹配列表
    std::vector<StarMatch> cleanOutliers(
        const std::vector<StarMatch>& matches, double outlier_sigma);
};

} // namespace pc

#endif // PC_STAR_MATCHER_H
