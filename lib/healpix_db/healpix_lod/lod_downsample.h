#ifndef LOD_DOWNSAMPLE_H
#define LOD_DOWNSAMPLE_H

// ============================================================================
// LOD 降采样引擎
//
// 在 NESTED scheme 下, HEALpix 像素号采用 Morton (z-order) 位编码,
// 父子层级关系可通过位运算直接表达:
//   ipix_coarse = ipix_fine >> (2 * log2(nside_fine / nside_coarse))
//
// 降采样: 收集同一父像素的所有子像素, 加权平均
//   value_coarse  = Σ(value_fine * weight_fine) / Σ(weight_fine)
//   weight_coarse = Σ(weight_fine)
//   count_coarse  = 子像素数
// ============================================================================

#include <cstdint>
#include <vector>

namespace lod {

// --------------------------------------------------------------------------
// 细像素 (降采样输入)
// --------------------------------------------------------------------------
struct FinePixel {
    uint64_t ipix;   // 子像素号 (NESTED scheme, 全局像素号)
    float    value;  // 像素值
    float    weight; // 像素权重 (通常 = weightSum)
};

// --------------------------------------------------------------------------
// 粗像素 (降采样输出)
// --------------------------------------------------------------------------
struct CoarsePixel {
    uint64_t ipix;   // 父像素号 (NESTED scheme, 全局像素号)
    float    value;  // 加权均值
    float    weight; // 权重和
    uint16_t count;  // 贡献的子像素数
};

// --------------------------------------------------------------------------
// LodDownsampler - 降采样引擎
// --------------------------------------------------------------------------
class LodDownsampler {
public:
    // 批量降采样: 将一批细像素聚合为粗像素
    // fineNside → coarseNside (coarseNside 必须是 fineNside 的约数, 且比值是 2 的幂)
    // 自动按 NESTED 层级聚合, 返回按 ipix 升序排列的粗像素
    std::vector<CoarsePixel> downsample(
        const std::vector<FinePixel>& finePixels,
        int fineNside,
        int coarseNside
    );

    // 增量降采样: 只处理变化的子像素
    // changedPixels: 变化的子像素列表 (应包含所有受影响粗像素的全部子像素)
    // 返回受影响的父像素及新值
    // 注意: 调用方需提供每个受影响父像素的完整子像素集合, 而非仅变化的部分
    std::vector<CoarsePixel> downsampleIncremental(
        const std::vector<FinePixel>& changedPixels,
        int fineNside,
        int coarseNside
    );

private:
    // NESTED scheme 下, 子像素 → 父像素映射 (位运算)
    // ipix_coarse = ipix_fine >> (2 * log2(fineNside / coarseNside))
    uint64_t fineToCoarse(uint64_t ipixFine, int fineNside, int coarseNside) const;
};

} // namespace lod

#endif // LOD_DOWNSAMPLE_H
