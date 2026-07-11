#include "stack_engine.h"
#include "healpix_core.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace ahps {

// ============================================================================
// 辅助: 加权统计
// ============================================================================
StackEngine::StackEngine(const StackDbConfig& config)
    : m_config(config) {
}

// 加权 median: 按 value 排序, 累加 weight 直到 >= 总权重/2
float StackEngine::weightedMedian(const std::vector<float>& values,
                                  const std::vector<float>& weights) {
    if (values.empty()) return 0.0f;
    // 建立索引并按 value 排序
    std::vector<size_t> idx(values.size());
    for (size_t i = 0; i < idx.size(); i++) idx[i] = i;
    std::sort(idx.begin(), idx.end(), [&](size_t a, size_t b) {
        return values[a] < values[b];
    });

    double totalW = 0.0;
    for (float w : weights) totalW += w;
    if (totalW <= 0.0) return values[idx[idx.size() / 2]];

    double half = totalW * 0.5;
    double acc = 0.0;
    for (size_t i : idx) {
        acc += weights[i];
        if (acc >= half) return values[i];
    }
    return values[idx.back()];
}

// 加权 MAD = weighted median of |v - median|
float StackEngine::weightedMAD(const std::vector<float>& values,
                               const std::vector<float>& weights,
                               float median) {
    if (values.empty()) return 0.0f;
    std::vector<float> devs(values.size());
    for (size_t i = 0; i < values.size(); i++) {
        devs[i] = std::fabs(values[i] - median);
    }
    return weightedMedian(devs, weights);
}

// sigma-clip: 原地剔除离群值
// 返回是否剔除了任何值
bool StackEngine::sigmaClip(std::vector<float>& values,
                            std::vector<float>& weights,
                            float lowClip, float highClip) {
    if (values.size() < 3) return false;
    bool removed = false;

    for (int iter = 0; iter < 3; iter++) {
        if (values.size() < 3) break;
        float med = weightedMedian(values, weights);
        float mad = weightedMAD(values, weights, med);
        float sigma = 1.4826f * mad;
        if (sigma <= 0.0f) break;  // 无离散度, 停止

        float lo = med - lowClip * sigma;
        float hi = med + highClip * sigma;

        std::vector<float> nv, nw;
        nv.reserve(values.size());
        nw.reserve(weights.size());
        for (size_t i = 0; i < values.size(); i++) {
            if (values[i] >= lo && values[i] <= hi) {
                nv.push_back(values[i]);
                nw.push_back(weights[i]);
            }
        }
        if (nv.size() == values.size()) {
            // 本轮无剔除, 收敛
            break;
        }
        if (nv.size() < 2) {
            // 保留至少 2 个, 防止过度剔除
            break;
        }
        if (nv.size() < values.size()) removed = true;
        values = std::move(nv);
        weights = std::move(nw);
    }
    return removed;
}

// ============================================================================
// 单像素堆栈
// ============================================================================
void StackEngine::stackPixel(const std::vector<DrizzlePixel>& frames,
                             uint16_t* outCount, float* outSum,
                             float* outSumSq, float* outWeightSum,
                             bool* outLowConfidence) {
    *outCount = 0;
    *outSum = 0.0f;
    *outSumSq = 0.0f;
    *outWeightSum = 0.0f;
    *outLowConfidence = false;

    if (frames.empty()) return;

    // 收集 value + weight
    std::vector<float> values;
    std::vector<float> weights;
    values.reserve(frames.size());
    weights.reserve(frames.size());
    for (const auto& f : frames) {
        values.push_back(f.value);
        weights.push_back(f.weight);
    }

    // N < 3: 跳过 sigma-clip, 标记低置信度
    if (values.size() < 3) {
        *outLowConfidence = true;
    } else {
        sigmaClip(values, weights,
                  (float)m_config.sigmaClipLow,
                  (float)m_config.sigmaClipHigh);
    }

    // 累积统计量
    double sum = 0.0, sumSq = 0.0, wsum = 0.0;
    for (size_t i = 0; i < values.size(); i++) {
        float w = weights[i];
        float v = values[i];
        sum   += (double)v * w;
        sumSq += (double)v * v * w;
        wsum  += (double)w;
    }
    *outCount = (uint16_t)values.size();
    *outSum = (float)sum;
    *outSumSq = (float)sumSq;
    *outWeightSum = (float)wsum;
}

// ============================================================================
// 批量堆栈
// ============================================================================
void StackEngine::stackBatch(const std::map<uint64_t, std::vector<DrizzlePixel>>& pixelFrames,
                             StackResult& outResult) {
    outResult.pixels.clear();
    outResult.counts.clear();
    outResult.sums.clear();
    outResult.sumSqs.clear();
    outResult.weightSums.clear();
    outResult.lowConfidence.clear();

    size_t n = pixelFrames.size();
    outResult.pixels.reserve(n);
    outResult.counts.reserve(n);
    outResult.sums.reserve(n);
    outResult.sumSqs.reserve(n);
    outResult.weightSums.reserve(n);
    outResult.lowConfidence.reserve(n);

    for (const auto& kv : pixelFrames) {
        uint64_t pix = kv.first;
        const auto& frames = kv.second;

        uint16_t cnt;
        float sum, sumSq, wsum;
        bool lowConf;
        stackPixel(frames, &cnt, &sum, &sumSq, &wsum, &lowConf);

        outResult.pixels.push_back(pix);
        outResult.counts.push_back(cnt);
        outResult.sums.push_back(sum);
        outResult.sumSqs.push_back(sumSq);
        outResult.weightSums.push_back(wsum);
        outResult.lowConfidence.push_back(lowConf ? 1 : 0);
    }

    // 按像素号排序 (同步排序所有数组)
    // 建立索引
    std::vector<size_t> order(outResult.pixels.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = i;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return outResult.pixels[a] < outResult.pixels[b];
    });
    // 按序重排
    StackResult sorted;
    sorted.pixels.reserve(order.size());
    sorted.counts.reserve(order.size());
    sorted.sums.reserve(order.size());
    sorted.sumSqs.reserve(order.size());
    sorted.weightSums.reserve(order.size());
    sorted.lowConfidence.reserve(order.size());
    for (size_t i : order) {
        sorted.pixels.push_back(outResult.pixels[i]);
        sorted.counts.push_back(outResult.counts[i]);
        sorted.sums.push_back(outResult.sums[i]);
        sorted.sumSqs.push_back(outResult.sumSqs[i]);
        sorted.weightSums.push_back(outResult.weightSums[i]);
        sorted.lowConfidence.push_back(outResult.lowConfidence[i]);
    }
    outResult = std::move(sorted);

    fprintf(stderr, "[ahps][engine] stackBatch: 堆栈 %zu 个像素\n", outResult.pixels.size());
}

// ============================================================================
// 处理一组帧: 按像素分组 → 按 tile 分组 → 堆栈 → 写入/合并 tile
// ============================================================================
int StackEngine::processFrames(StackDatabase* db,
                               const std::vector<std::vector<DrizzlePixel>>& frameData) {
    if (!db) return 0;
    const auto& cfg = db->getConfig();

    // 1. 按像素分组所有帧数据
    std::map<uint64_t, std::vector<DrizzlePixel>> byPixel;
    int64_t totalPixels = 0;
    for (const auto& frame : frameData) {
        for (const auto& dp : frame) {
            byPixel[dp.healpixPix].push_back(dp);
            totalPixels++;
        }
    }
    fprintf(stderr, "[ahps][engine] processFrames: %zu 帧, %zu 唯一像素, %lld 像素-帧\n",
            frameData.size(), byPixel.size(), (long long)totalPixels);

    if (byPixel.empty()) return 0;

    // 2. 堆栈每个像素
    StackResult result;
    stackBatch(byPixel, result);

    // 3. 按 tile 分组像素 (用 pixelToCoarse 计算 tile 像素号)
    healpix::HealpixCore core(cfg.nsideData, cfg.nested);
    // tileIpix → {pixel index in result}
    std::map<int64_t, std::vector<size_t>> byTile;
    for (size_t i = 0; i < result.pixels.size(); i++) {
        int64_t tileIpix = core.pixelToCoarse(result.pixels[i], cfg.tileNside);
        byTile[tileIpix].push_back(i);
    }

    fprintf(stderr, "[ahps][engine] 涉及 %zu 个 tile\n", byTile.size());

    // 4. 每个 tile: 合并到已有数据或创建新文件
    int updatedTiles = 0;
    for (const auto& kv : byTile) {
        int64_t tileIpix = kv.first;
        const auto& pixIdx = kv.second;  // result 中的下标

        // 读取已有 tile (若存在)
        std::vector<uint64_t> existPixels;
        // 各波段: bandStats[band][pixelIndex]
        std::vector<std::vector<PixelStats>> existStats(cfg.bands.size());

        AhpsReader* reader = db->openTileReader(tileIpix);
        if (reader) {
            existPixels = reader->readPixelIndices();
            for (int b = 0; b < (int)cfg.bands.size() && b < reader->getBandCount(); b++) {
                existStats[b] = reader->readBandStats(b);
            }
            delete reader;
        }

        // 合并: 像素号 → 全局统计 map
        // 用 map 合并 (像素号 → 各 band 的 stats)
        // 因为单次 updateGlobal 的 result 只有 1 个波段组? 
        // 注意: result 是单波段堆栈. 但 DrizzlePixel 没有 band 信息.
        // 约定: updateGlobal 处理单波段数据, bandIndex 由调用方在外部确定.
        // 这里将 result 写入 band 0 (调用方可多次调用 updateGlobal 分别处理各波段)
        // 更通用做法: 每个 DrizzlePixel 带 band, 但当前结构未含 band.
        // 简化: 将本次 result 视为 band 0 的增量, 合并到 tile.

        // 构建 pixel → stats (本次, band 0)
        std::map<uint64_t, PixelStats> newStats;
        for (size_t i : pixIdx) {
            PixelStats ps;
            ps.count = result.counts[i];
            ps.sum = result.sums[i];
            ps.sumSq = result.sumSqs[i];
            ps.weightSum = result.weightSums[i];
            newStats[result.pixels[i]] = ps;
        }

        // 合并到 existStats[0]
        int bandCount = (int)cfg.bands.size();
        if ((int)existStats.size() < bandCount) existStats.resize(bandCount);
        // 建立 existPixels 的索引 map
        std::map<uint64_t, size_t> existIdx;
        for (size_t i = 0; i < existPixels.size(); i++) {
            existIdx[existPixels[i]] = i;
        }
        // 确保所有 band 数组长度一致
        for (int b = 0; b < bandCount; b++) {
            if (existStats[b].size() < existPixels.size()) {
                existStats[b].resize(existPixels.size());
            }
        }

        // 合并本次的 band 0 数据
        for (const auto& ns : newStats) {
            uint64_t pix = ns.first;
            auto it = existIdx.find(pix);
            if (it == existIdx.end()) {
                // 新像素: 追加
                existPixels.push_back(pix);
                size_t newIdx = existPixels.size() - 1;
                existIdx[pix] = newIdx;
                for (int b = 0; b < bandCount; b++) {
                    PixelStats ps;
                    ps.count = 0; ps.sum = 0.0f; ps.sumSq = 0.0f; ps.weightSum = 0.0f;
                    existStats[b].push_back(ps);
                }
                // band 0 设为本次
                existStats[0][newIdx] = ns.second;
            } else {
                // 已有像素: 累加 band 0
                size_t idx = it->second;
                PixelStats& dst = existStats[0][idx];
                dst.count += ns.second.count;
                dst.sum += ns.second.sum;
                dst.sumSq += ns.second.sumSq;
                dst.weightSum += ns.second.weightSum;
            }
        }

        // 排序像素 (合并后需重新排序)
        std::vector<size_t> order(existPixels.size());
        for (size_t i = 0; i < order.size(); i++) order[i] = i;
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
            return existPixels[a] < existPixels[b];
        });
        std::vector<uint64_t> sortedPixels(existPixels.size());
        std::vector<std::vector<PixelStats>> sortedStats(bandCount);
        for (int b = 0; b < bandCount; b++) sortedStats[b].resize(existPixels.size());
        for (size_t i = 0; i < order.size(); i++) {
            size_t src = order[i];
            sortedPixels[i] = existPixels[src];
            for (int b = 0; b < bandCount; b++) {
                sortedStats[b][i] = existStats[b][src];
            }
        }

        // 写入 tile
        AhpsWriter* writer = db->getOrCreateTileWriter(tileIpix);
        if (!writer) {
            fprintf(stderr, "[ahps][engine] 无法创建 tile writer: ipix=%lld\n",
                    (long long)tileIpix);
            continue;
        }
        writer->setPixelIndices(sortedPixels);
        for (int b = 0; b < bandCount; b++) {
            writer->setBandStats(b, sortedStats[b]);
        }
        std::string tilePath = db->findTile(cfg.nsideData, tileIpix);
        if (tilePath.empty()) {
            tilePath = db->getPath() + "/tiles/nside_" + std::to_string(cfg.nsideData) +
                       "/tile_" + std::to_string(tileIpix) + ".ahps";
        }
        if (writer->write(tilePath, 5)) {
            updatedTiles++;
            fprintf(stderr, "[ahps][engine] tile %lld 已更新 (%zu 像素)\n",
                    (long long)tileIpix, sortedPixels.size());
        } else {
            fprintf(stderr, "[ahps][engine] tile %lld 写入失败\n", (long long)tileIpix);
        }
        delete writer;
    }

    fprintf(stderr, "[ahps][engine] processFrames 完成: 更新 %d 个 tile\n", updatedTiles);
    return (int)result.pixels.size();
}

int StackEngine::updateGlobal(StackDatabase* db,
                              const std::vector<std::vector<DrizzlePixel>>& frameData) {
    return processFrames(db, frameData);
}

int StackEngine::updateRange(StackDatabase* db,
                             const std::map<std::string, std::vector<DrizzlePixel>>& fileRange) {
    // 将 fileRange 展开为 frameData 向量
    std::vector<std::vector<DrizzlePixel>> frameData;
    for (const auto& kv : fileRange) {
        frameData.push_back(kv.second);
    }
    return processFrames(db, frameData);
}

} // namespace ahps
