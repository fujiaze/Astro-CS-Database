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

// R11 (阶段7): Tile 局部累加器的叶像素累加单元 (模板双实例 Scalar=float/double)
//   FP32 模式: sumFlux/sumArea 为 IEEE binary32 (真 FP32 累计, 不共享 double)
//   FP64 模式: IEEE binary64 (与旧 PixelAccumulator 语义一致)
//   控制包 TILE_ACCUMULATOR_DESIGN: release 只保留 sumFlux/sumArea/nContrib;
//   sumWeight/sumSnrSq 为诊断字段, 已移除 (每 leaf 20B→12B, 完整帧 RSS -40%)
template <typename Scalar>
struct TileLeafAccumulatorT {
    Scalar sumFlux = Scalar(0);
    Scalar sumArea = Scalar(0);
    uint32_t nContrib = 0;
};
using TileLeafAccumulator = TileLeafAccumulatorT<double>;  // 兼容别名

// R11 (阶段6/7): Tile 局部累加器 (控制包 TILE_ACCUMULATOR_DESIGN, template<class Scalar>)
//   线程本地 map 以 parent_ipix 为 key; leaf 用 NESTED 位运算得到 local_ipix,
//   直接连续数组寻址 (禁止每-leaf unordered_map)
template <typename Scalar>
struct TileAccumulatorT {
    uint64_t parent_ipix = 0;
    std::vector<TileLeafAccumulatorT<Scalar>> pixels;  // 按 local_ipix 索引 (按需增长)
    std::vector<uint32_t> touched;            // 已触发的 local_ipix (合并/展开用)

    TileLeafAccumulatorT<Scalar>& leaf(uint32_t local) {
        if (local >= pixels.size()) pixels.resize((size_t)local + 1);
        TileLeafAccumulatorT<Scalar>& acc = pixels[local];
        if (acc.nContrib == 0) touched.push_back(local);  // 首触记录
        return acc;
    }
};
using TileAccumulator = TileAccumulatorT<double>;  // 兼容别名 (FP64/旧接口)

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
    // R11 (阶段6): 内部改为 Tile 级累加, 合并后展开为 leaf map 的兼容包装;
    // 正式写入路径请使用 drizzleTiled + writeHisTiles (取消全局 leaf map)
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

    // R11 (阶段6): Tile 级 Drizzle (正式路径, 取消全局 leaf map 与逐 key merge)
    //   输出: tiles 为按 parent_ipix 分组、叶像素连续数组寻址的累加结果
    //   (FP32 路径: 读 img.pixels float32, Scalar=float 真 FP32 累计; FP64 由 drizzleTiled_f64 提供)
    bool drizzleTiled(const FitsImage& img, const DrizzleConfig& config,
                      const float* snrData, const float* weightData,
                      std::vector<TileAccumulatorT<float>>& tiles,
                      DrizzleStats& stats, std::string& error_msg);

    // FP64 Tile 级 Drizzle (读 img.pixels_f64 double, 不降级到 float32)
    bool drizzleTiled_f64(const FitsImage& img, const DrizzleConfig& config,
                          const float* snrData, const float* weightData,
                          std::vector<TileAccumulatorT<double>>& tiles,
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

    // R11 (阶段6): 将 Tile 级累加结果直接写入 .hiss (流式, 不恢复全局 leaf map)
    //   与 writeHis 语义一致, 但输入为 TileAccumulator 列表 (FP32=float, FP64=double)
    template <typename Scalar>
    bool writeHisTilesT(const std::vector<TileAccumulatorT<Scalar>>& tiles,
                       const DrizzleStats& stats, const WcsParams& wcs,
                       const DrizzleConfig& config, const DrizzleMeta& meta,
                       const std::string& fitsPath,
                       const std::string& outputPath,
                       const HioSnrModel* snr_model,
                       std::string& error_msg);

    // 兼容包装 (double 实例, 旧调用方)
    bool writeHisTiles(const std::vector<TileAccumulator>& tiles,
                       const DrizzleStats& stats, const WcsParams& wcs,
                       const DrizzleConfig& config, const DrizzleMeta& meta,
                       const std::string& fitsPath,
                       const std::string& outputPath,
                       const HioSnrModel* snr_model,
                       std::string& error_msg);

private:
    // R11 (阶段6): 处理单个像素的 Drizzle (6步流水线) — 模板双实例 (Scalar=float/double)
    //   tileMap: 线程本地 Tile 累加 map (key=parent_ipix, Scalar=float/double)
    //   shift/mask: NESTED 位运算 (leaf_ipix >> shift = parent, leaf_ipix & mask = local)
    template <typename Scalar>
    void processPixelTiled(
        double px, double py,           // 像素中心 (0-based)
        Scalar pixelValue,               // 像素值 (float=FP32, double=FP64)
        float snrValue,                  // SNR
        float weightValue,               // 权重
        const WcsSip& wcs,               // WCS 转换器 (double 几何内核, 见 processPixelSharedTiled)
        const DrizzleConfig& config,     // 配置
        const healpix::HealpixCore& hp,  // HEALPix 核心
        uint32_t shift, uint64_t mask,   // NESTED tile 位运算
        std::unordered_map<uint64_t, TileAccumulatorT<Scalar>>& tileMap  // 线程本地 tile 累加
    ) const;

    // R11: 共享顶点路径 (pixfrac=1): 接收预计算的 4 角球面坐标, 跳过逐像素 WCS 角点变换
    template <typename Scalar>
    void processPixelSharedTiled(
        double px, double py,
        Scalar pixelValue, float snrValue, float weightValue,
        const double corners_ra[4], const double corners_dec[4],
        const WcsSip& wcs, const DrizzleConfig& config,
        const healpix::HealpixCore& hp,
        uint32_t shift, uint64_t mask,
        std::unordered_map<uint64_t, TileAccumulatorT<Scalar>>& tileMap) const;

    // R11 (阶段6): Tile 级 Drizzle 内部实现 (模板 Scalar=float/double)
    template <typename Scalar>
    bool drizzleTiledImpl(const FitsImage& img, const DrizzleConfig& config,
                          const float* snrData, const float* weightData,
                          const Scalar* pixels,
                          std::vector<TileAccumulatorT<Scalar>>& tiles,
                          DrizzleStats& stats, std::string& error_msg);

    // 获取 HEALPix 像素的四角球面坐标
    void getHealpixCorners(const healpix::HealpixCore& hp, int64_t ipix,
                           double ra0, double dec0,
                           std::vector<SkyCoord>& corners) const;
};

} // namespace drizzle

#endif // DRIZZLE_ENGINE_H
