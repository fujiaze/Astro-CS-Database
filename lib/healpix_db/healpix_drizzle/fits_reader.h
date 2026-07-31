#ifndef FITS_READER_H
#define FITS_READER_H

#include <cstdint>
#include <string>
#include <vector>

namespace drizzle {

// SIP 系数 (最多 4 阶, 36 个系数, A[i*6+j] 对应 dx^i*dy^j)
struct SipCoeffs {
    double a[36] = {0};   // 前向 A
    double b[36] = {0};   // 前向 B
    double ap[36] = {0};  // 逆向 AP
    double bp[36] = {0};  // 逆向 BP
    int    order = 0;     // 前向阶数
    int    ap_order = 0;  // 逆向阶数
};

// WCS 参数
struct WcsParams {
    double cd[4] = {0};       // CD 矩阵 [cd1_1, cd1_2, cd2_1, cd2_2]
    double crval[2] = {0};    // 中心赤经赤纬 (度)
    double crpix[2] = {0};    // 参考像素 (1-based)
    char   ctype1[16] = {0};  // 如 "RA---TAN-SIP"
    char   ctype2[16] = {0};  // 如 "DEC--TAN-SIP"
    SipCoeffs sip;
    bool   has_wcs = false;
};

// FITS 图像数据
struct FitsImage {
    std::vector<float> pixels;   // 像素数据 (float32, HWC 排列)
    int    width = 0;
    int    height = 0;
    int    channels = 1;         // 1=单通道, 3=RGB
    WcsParams wcs;
    double bzero = 0.0;          // BITPIX 缩放
    double bscale = 1.0;
    // B5 修复: 测光校准元数据 (由 PHOTOMETRIC 阶段写入 FITS header)
    double photscal = 0.0;       // PHOTSCAL 关键字 (0=未设置)
    int    photappl = 0;         // PHOTAPPL 关键字 (0=未应用, 1=已应用)
};

// 读取 FITS 文件
// 成功返回 true, 失败返回 false (error_msg 填充错误信息)
bool readFits(const std::string& path, FitsImage& img, std::string& error_msg);

} // namespace drizzle

#endif // FITS_READER_H
