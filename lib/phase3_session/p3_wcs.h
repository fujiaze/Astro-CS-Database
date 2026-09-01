// lib/phase3_session/p3_wcs.h — FITS-WCS 输出描述符 + TAN 投影正反变换 (ALG-P3-002) — P3-002
// 覆盖: CRPIX(pixel-center)/CD 关键字正确性、east_left|east_right、旋转 PA、
// RA wrap、TAN 半球守卫(输出四角同半球)、abs(dec)<=85° 极点守卫(单一条件)。
#ifndef ASTROCS_P3_WCS_H
#define ASTROCS_P3_WCS_H

#include <string>

namespace astrocs::phase3 {

struct P3WcsDescriptor {
    double crval_ra_deg = 0;      // ICRS 中心
    double crval_dec_deg = 0;
    double crpix_x = 0;           // FITS 1-based 参考像素(pixel-center)
    double crpix_y = 0;
    double cd[2][2] = {{0, 0}, {0, 0}};   // deg/px, FITS 顺序 CD[i][j]
    int width_px = 0;
    int height_px = 0;
    const char* projection = "TAN";
};

enum P3WcsStatus {
    P3_WCS_OK = 0,
    P3_WCS_PARAM = 1,          // abs(dec)>85°/W,H 越界/极点守卫
    P3_WCS_UNSUPPORTED = 2,    // projection≠TAN
    P3_WCS_HEMISPHERE = 3      // 输出跨 TAN 半球
};

/* 构造描述符: scale 为 deg/px; parity: "east_left"(CD1_1<0, 默认)|"east_right"(CD1_1>0);
 * rotation_pa_deg: 天北方向相对 +y 的位置角(可选, 0=北朝上)。逐项守卫(ALG-P3-003)。 */
P3WcsStatus p3_wcs_make(double centre_ra_deg, double centre_dec_deg,
                        double scale_deg_per_px, int width_px, int height_px,
                        const char* parity, double rotation_pa_deg,
                        P3WcsDescriptor* out);

/* pixel-center world 变换: (x,y) 为 0-based 像素坐标(FITS=+1);
 * 失败(半球外)返回非 0。 */
P3WcsStatus p3_wcs_pix2world(const P3WcsDescriptor* d, double x, double y,
                             double* ra_deg, double* dec_deg);

/* 反变换(world→pixel); abs(dec)<=85° 守卫内部隐含(切平面远离极点)。 */
P3WcsStatus p3_wcs_world2pix(const P3WcsDescriptor* d, double ra_deg, double dec_deg,
                             double* x, double* y);

/* FITS 关键字文本(CTYPE/CRPIX/CRVAL/CD/CUNIT; 含 END 前格式); 每行 80 字节内。 */
std::string p3_wcs_fits_keywords(const P3WcsDescriptor* d);

}  // namespace astrocs::phase3

#endif  // ASTROCS_P3_WCS_H
