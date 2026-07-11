#include "lod_downsample.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace lod {

// ============================================================================
// fineToCoarse - NESTED scheme 子像素 → 父像素映射
//
// NESTED scheme 下, 像素号 = bighp * nside² + morton(x, y)
// 当 nside_fine / nside_coarse = 2^k 时:
//   ipix_coarse = ipix_fine >> (2 * k)
//
// 数学证明:
//   设 ratio = nside_fine / nside_coarse = 2^k
//   ipix_fine  = bighp * nside_fine²  + morton_fine(x, y)
//   ipix_coarse = bighp * nside_coarse² + morton_coarse(x/ratio, y/ratio)
//
//   morton_coarse = morton_fine >> (2*k)  (Morton 码的高位对应粗粒度)
//
//   由于 nside_fine² = ratio² * nside_coarse², 且 morton_fine < nside_fine²:
//   ipix_fine >> (2*k) = (bighp * nside_fine² + morton_fine) >> (2*k)
//                      = bighp * nside_coarse² + (morton_fine >> 2k)
//                      = ipix_coarse  ✓
// ============================================================================
uint64_t LodDownsampler::fineToCoarse(uint64_t ipixFine, int fineNside, int coarseNside) const {
    // 比值必须是 2 的幂
    int ratio = fineNside / coarseNside;
    if (ratio <= 1) return ipixFine;  // 同层级, 无需映射

    // 计算 log2(ratio)
    int shift = 0;
    int r = ratio;
    while (r > 1) {
        r >>= 1;
        shift++;
    }

    // 右移 2*shift 位 (因为 Morton 码每 2 位对应一级)
    return ipixFine >> (2 * shift);
}

// ============================================================================
// downsample - 批量降采样
//
// 算法:
//   1. 为每个细像素计算其所属粗像素号
//   2. 按粗像素号排序
//   3. 聚合相同粗像素号的细像素: 加权平均
//
// 复杂度: O(N log N) (排序为主)
// 内存: 额外 O(N) 用于临时数组
// ============================================================================
std::vector<CoarsePixel> LodDownsampler::downsample(
    const std::vector<FinePixel>& finePixels,
    int fineNside,
    int coarseNside
) {
    std::vector<CoarsePixel> result;

    if (finePixels.empty()) {
        return result;
    }

    // 参数校验
    if (fineNside <= 0 || coarseNside <= 0) {
        fprintf(stderr, "[lod][downsample] 错误: 无效 nside (fine=%d, coarse=%d)\n",
                fineNside, coarseNside);
        return result;
    }
    if (fineNside < coarseNside) {
        fprintf(stderr, "[lod][downsample] 错误: fineNside(%d) < coarseNside(%d)\n",
                fineNside, coarseNside);
        return result;
    }
    if (fineNside % coarseNside != 0) {
        fprintf(stderr, "[lod][downsample] 错误: fineNside(%d) 不是 coarseNside(%d) 的整数倍\n",
                fineNside, coarseNside);
        return result;
    }

    // 同层级无需降采样, 直接转换 (去重聚合)
    if (fineNside == coarseNside) {
        // 与下面的逻辑一致, fineToCoarse 返回原值
    }

    // 步骤 1: 计算每个细像素的粗像素号
    struct Entry {
        uint64_t coarseIpix;
        float    value;
        float    weight;
    };

    std::vector<Entry> entries;
    entries.reserve(finePixels.size());

    for (const auto& fp : finePixels) {
        Entry e;
        e.coarseIpix = fineToCoarse(fp.ipix, fineNside, coarseNside);
        e.value      = fp.value;
        e.weight     = fp.weight;
        entries.push_back(e);
    }

    // 步骤 2: 按粗像素号排序 (升序)
    std::sort(entries.begin(), entries.end(),
              [](const Entry& a, const Entry& b) {
                  return a.coarseIpix < b.coarseIpix;
              });

    // 步骤 3: 聚合相同粗像素号的细像素
    //   value  = Σ(value * weight) / Σ(weight)  (加权均值)
    //   weight = Σ(weight)                      (权重和)
    //   count  = 子像素数
    size_t i = 0;
    while (i < entries.size()) {
        uint64_t curIpix = entries[i].coarseIpix;

        // 用 double 累加, 避免浮点精度损失
        double sumVW = 0.0;   // Σ(value * weight)
        double sumW  = 0.0;   // Σ(weight)
        uint16_t count = 0;

        size_t j = i;
        while (j < entries.size() && entries[j].coarseIpix == curIpix) {
            // 跳过零权重像素 (无有效数据)
            if (entries[j].weight > 0.0f) {
                sumVW += (double)entries[j].value * (double)entries[j].weight;
                sumW  += (double)entries[j].weight;
            }
            count++;
            j++;
        }

        // count 可能超过 uint16_t 上限 (65535), 但实际不会 (每 tile 最多 4096 像素)
        CoarsePixel cp;
        cp.ipix   = curIpix;
        cp.value  = (sumW > 0.0) ? (float)(sumVW / sumW) : 0.0f;
        cp.weight = (float)sumW;
        cp.count  = count;
        result.push_back(cp);

        i = j;
    }

    fprintf(stderr, "[lod][downsample] 降采样完成: %zu 细像素 → %zu 粗像素 (nside %d→%d)\n",
            finePixels.size(), result.size(), fineNside, coarseNside);

    return result;
}

// ============================================================================
// downsampleIncremental - 增量降采样
//
// 调用方提供受影响粗像素的完整子像素集合, 函数执行与 downsample 相同的聚合。
// 区别在于语义: 调用方保证只传入受影响区域的像素, 输出仅为受影响的粗像素。
//
// 注意: 此函数不维护历史状态, 调用方需确保传入每个受影响粗像素的所有子像素。
// ============================================================================
std::vector<CoarsePixel> LodDownsampler::downsampleIncremental(
    const std::vector<FinePixel>& changedPixels,
    int fineNside,
    int coarseNside
) {
    // 增量降采样与全量降采样的聚合逻辑完全相同
    // 区别仅在调用方传入的像素范围 (增量 = 仅受影响区域)
    return downsample(changedPixels, fineNside, coarseNside);
}

} // namespace lod
