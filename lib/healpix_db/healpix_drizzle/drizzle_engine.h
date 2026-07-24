#ifndef DRIZZLE_ENGINE_H
#define DRIZZLE_ENGINE_H

#include "fits_reader.h"
#include "wcs_sip.h"
#include "poly_clip.h"
#include "aio_healpix_io.h"   // HioSnrModel (稀疏 SNR 控制点模型, 向后兼容宏)
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>

// 前向声明 healpix 命名空间和 HealpixCore 类 (实现见 healpix_stack/healpix_core.h)
namespace healpix { class HealpixCore; }

namespace drizzle {

// Drizzle 配置
struct DrizzleConfig {
    int    nside = 32768;       // HEALPix nside
    bool   nested = true;       // NESTED 或 RING
    double pixfrac = 1.0;       // 像素收缩因子 (0.0~1.0, 默认1.0避免源像素固有缝隙)
};

// 单个 HEALPix 像素的累加器
struct PixelAccumulator {
    double sumFlux = 0.0;     // Σ value * weight
    double sumWeight = 0.0;   // Σ weight
    double sumSnrSq = 0.0;    // Σ SNR² * weight
};

// Drizzle 结果统计
struct DrizzleStats {
    int64_t nHealpixPixels = 0;    // 有效 HEALPix 像素数
    int64_t nSourcePixels = 0;     // 源图像像素数
    int    nside = 0;
    bool   nested = true;
    double elapsedSec = 0.0;
};

// Drizzle 元数据 (写入 .hiss JSON 头)
struct DrizzleMeta {
    std::string filter;                              // FILTER 滤光片名
    double      exposure_s = 0.0;                    // EXPTIME 曝光时间 (秒)
    std::string obs_time;                            // DATE-OBS 观测时间
    std::map<std::string, std::string> fits_meta;    // FITS 头 KV (OBJCTRA/OBJCTDEC/IMAGETYP/SITELAT/SITELONG 等)
};

// Drizzle 核心引擎
class DrizzleEngine {
public:
    DrizzleEngine();
    ~DrizzleEngine();

    // 执行 Drizzle: FITS 图像 → HEALPix 累加器
    // img: 输入 FITS 图像 (含 WCS)
    // config: Drizzle 配置
    // snrData: 可选 SNR 图 (W*H float, nullptr 则用 1.0)
    // weightData: 可选权重图 (W*H float, nullptr 则用 1.0)
    // stats: 输出统计信息
    // error_msg: 错误信息
    // 返回: 成功/失败
    bool drizzle(const FitsImage& img, const DrizzleConfig& config,
                 const float* snrData, const float* weightData,
                 std::unordered_map<uint64_t, PixelAccumulator>& accumulators,
                 DrizzleStats& stats, std::string& error_msg);

    // 将累加器归一化并写入 .hiss 文件
    // accumulators: Drizzle 输出的累加器
    // stats: Drizzle 统计
    // wcs: 原始 WCS 参数 (写入元数据)
    // config: Drizzle 配置 (写入元数据)
    // meta: Drizzle 元数据 (filter/exposure_s/obs_time/fits_meta, 写入 JSON 头)
    // fitsPath: 源 FITS 文件路径 (写入元数据)
    // outputPath: 输出 .hiss 文件路径
    // snr_model: 稀疏 SNR 控制点模型 (可为 nullptr, 不写 SNR 通道)
    // error_msg: 错误信息
    bool writeHis(const std::unordered_map<uint64_t, PixelAccumulator>& accumulators,
                  const DrizzleStats& stats, const WcsParams& wcs,
                  const DrizzleConfig& config, const DrizzleMeta& meta,
                  const std::string& fitsPath,
                  const std::string& outputPath,
                  const HioSnrModel* snr_model,
                  std::string& error_msg);

private:
    // 处理单个像素的 Drizzle (6步流水线)
    void processPixel(
        double px, double py,           // 像素中心 (0-based)
        float pixelValue,                // 像素值
        float snrValue,                  // SNR
        float weightValue,               // 权重
        const WcsSip& wcs,               // WCS 转换器
        const DrizzleConfig& config,     // 配置
        const healpix::HealpixCore& hp,  // HEALPix 核心
        std::unordered_map<uint64_t, PixelAccumulator>& accum  // 累加器
    ) const;

    // 获取 HEALPix 像素的四角球面坐标
    void getHealpixCorners(const healpix::HealpixCore& hp, int64_t ipix,
                           double ra0, double dec0,
                           std::vector<SkyCoord>& corners) const;
};

} // namespace drizzle

#endif // DRIZZLE_ENGINE_H
