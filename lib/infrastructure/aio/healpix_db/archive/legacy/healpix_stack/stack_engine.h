#ifndef STACK_ENGINE_H
#define STACK_ENGINE_H

#include "ahps_format.h"
#include "stack_db.h"

#include <cstdint>
#include <vector>
#include <map>
#include <string>

namespace ahps {

// ============================================================================
// 堆栈引擎数据结构
// ============================================================================

// 单帧某像素的 drizzle 输出
struct DrizzlePixel {
    uint64_t healpixPix;   // HEALpix 像素号
    float    value;        // 像素值
    float    snr;          // 信噪比
    float    weight;       // 权重 (已含 SNR)
};

// 堆栈结果 (多像素)
struct StackResult {
    std::vector<uint64_t> pixels;        // 像素号 (排序)
    std::vector<uint16_t> counts;        // 帧计数
    std::vector<float>    sums;          // 加权和
    std::vector<float>    sumSqs;        // 加权平方和
    std::vector<float>    weightSums;    // 权重累加
    std::vector<uint8_t>  lowConfidence; // 低置信度标记 (N<3)
};

// ============================================================================
// StackEngine - sigma-clip + SNR 加权堆栈引擎
//
// 算法:
//   1. 收集某像素的所有帧数据 (value, weight)
//   2. 若 N < 3: 跳过 sigma-clip, 直接加权平均, 标记低置信度
//   3. 否则: 计算 weighted median + MAD, sigma = 1.4826 * MAD
//      剔除 [median - lowClip*sigma, median + highClip*sigma] 之外的值
//      迭代 2-3 次
//   4. 累积保留帧的 sum/sumSq/weightSum/count
// ============================================================================

class StackEngine {
public:
    StackEngine(const StackDbConfig& config);

    // 堆栈一组帧的某像素数据 → 该像素的统计量
    void stackPixel(const std::vector<DrizzlePixel>& frames,
                    uint16_t* outCount, float* outSum,
                    float* outSumSq, float* outWeightSum,
                    bool* outLowConfidence);

    // 批量堆栈 (多像素)
    void stackBatch(const std::map<uint64_t, std::vector<DrizzlePixel>>& pixelFrames,
                    StackResult& outResult);

    // 全局更新: 处理一组帧, 更新数据库
    // frameData: 每帧的 drizzle 输出 (帧内多像素)
    // 返回处理的像素数
    int updateGlobal(StackDatabase* db,
                     const std::vector<std::vector<DrizzlePixel>>& frameData);

    // 局部更新: 只更新指定文件范围
    // fileRange: {文件路径 → 该文件的 drizzle 数据}
    int updateRange(StackDatabase* db,
                    const std::map<std::string, std::vector<DrizzlePixel>>& fileRange);

private:
    StackDbConfig m_config;

    // sigma-clip: 原地剔除离群值, 返回是否剔除了任何值
    bool sigmaClip(std::vector<float>& values,
                   std::vector<float>& weights,
                   float lowClip, float highClip);

    // 加权 median
    float weightedMedian(const std::vector<float>& values,
                         const std::vector<float>& weights);

    // 加权 MAD (Median Absolute Deviation)
    float weightedMAD(const std::vector<float>& values,
                      const std::vector<float>& weights,
                      float median);

    // 按像素分组帧数据, 然后堆栈并写入对应 tile
    int processFrames(StackDatabase* db,
                      const std::vector<std::vector<DrizzlePixel>>& frameData);
};

} // namespace ahps

#endif // STACK_ENGINE_H
