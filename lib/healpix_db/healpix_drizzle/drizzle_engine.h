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
    int    nside = 32768;       // HEALPix nside (nside=0 触发自动 NSIDE 计算)
    bool   nested = true;       // NESTED 或 RING (HISS 内部统一 NESTED)
    double pixfrac = 1.0;       // 像素收缩因子 (0 < pixfrac <= 1, 标准 drop 语义)
    // 测光校准语义 (B5 修复): PHOTOMETRIC 阶段 (pc_calibrate_simple) 已把 photscal
    // 乘入像素值, drizzle 不再重复应用。这两个字段仅用于元数据记录。
    bool   apply_photometry = false;  // 测光已应用到像素 (元数据标记, drizzle 不再应用)
    double photscal = 1.0;     // 测光校准比例 (实际应用值, 元数据记录用)
    bool   photometry_applied_upstream = false;  // PHOTOMETRIC 阶段已应用测光校准
    // R10: 精度模式 (0=FP32 binary32 默认, 1=FP64 binary64)
    // FP64 模式: signal 子块输出 float64, metadata 记录 precision_mode=1, signal_dtype=1
    uint8_t precision_mode = 0;
    // R11: 线程数 (0=自动 omp_get_max_threads; 禁止硬编码 16)
    int threads = 0;
};

// 单个 HEALPix 像素的累加器 (float64 内部精度, 02_FROZEN §8/§10)
struct PixelAccumulator {
    double sumFlux = 0.0;     // Σ L_j * a_jp / A_j_drop (通量守恒累加)
    double sumWeight = 0.0;   // Σ weight (兼容旧代码, 通量守恒模式下 = sumArea)
    double sumSnrSq = 0.0;    // Σ SNR² * weight
    double sumArea = 0.0;    // Σ a_jp (球面重叠面积, 用于 support = Σ a_jp / A_p)
    uint32_t nContrib = 0;    // 贡献源像素数 (诊断用)
};

// 自动 NSIDE 计算 (02_FROZEN §5)
// 依据 WCS/SIP 局部 Jacobian 找到最细输入像素尺度, 选择最小 2 次幂 NSIDE
// 使 HEALPix 线性像素尺度不粗于该最细尺度 (1~2 倍线性过采样)
// wcs: WCS/SIP 参数
// img_w, img_h: 图像尺寸
// 返回: 推荐 NSIDE (2 的幂), 0 表示失败
int compute_auto_nside(const WcsParams& wcs, int img_w, int img_h);

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

    // 执行 Drizzle: FITS 图像 → HEALPix 累加器 (FP32 路径, 读 img.pixels)
    // img: 输入 FITS 图像 (含 WCS, 像素在 img.pixels float32)
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

    // 执行 Drizzle: FITS 图像 → HEALPix 累加器 (FP64 路径, 读 img.pixels_f64)
    // 双精度 ABI: FP64 模式下从 img.pixels_f64 (double) 读取像素值, 不降级到 float32
    // 其余逻辑 (WCS / 球面几何 / 累加器) 与 drizzle 完全一致, 仅像素值类型不同
    // img: 输入 FITS 图像 (含 WCS, 像素在 img.pixels_f64 float64)
    // config: Drizzle 配置 (precision_mode 应为 1=FP64)
    // snrData / weightData: 仍为 float (SNR 重建与权重掩膜, 不需要 FP64)
    bool drizzle_f64(const FitsImage& img, const DrizzleConfig& config,
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
    // 处理单个像素的 Drizzle (6步流水线) — FP32 路径
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

    // 处理单个像素的 Drizzle (6步流水线) — FP64 路径
    // 双精度 ABI: pixelValue 为 double, 直接累加到 PixelAccumulator.sumFlux (double)
    // snrValue / weightValue 仍为 float (SNR 重建与权重掩膜不需要 FP64)
    void processPixel_f64(
        double px, double py,
        double pixelValue,
        float snrValue,
        float weightValue,
        const WcsSip& wcs,
        const DrizzleConfig& config,
        const healpix::HealpixCore& hp,
        std::unordered_map<uint64_t, PixelAccumulator>& accum
    ) const;

    // 获取 HEALPix 像素的四角球面坐标
    void getHealpixCorners(const healpix::HealpixCore& hp, int64_t ipix,
                           double ra0, double dec0,
                           std::vector<SkyCoord>& corners) const;
};

} // namespace drizzle

#endif // DRIZZLE_ENGINE_H
