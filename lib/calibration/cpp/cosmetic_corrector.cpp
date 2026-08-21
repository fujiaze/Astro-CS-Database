// ============================================================
// cosmetic_corrector.cpp - 坏点修复模块 C++ 实现（OpenMP并行）
// 功能: 中值滤波修复坏像素 + 从Dark/Bias检测热/冷像素
// 算法: 全局统计(median+MAD) + BFS连通区域过滤(排除星点)
// 依赖: 仅 STL + OpenMP，不依赖外部库
// ============================================================

#include "cosmetic_corrector.h"

#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <string>
#include <omp.h>

// ============================ 错误处理 ============================

static std::string g_last_error;

static void set_error(const char* msg) {
    g_last_error = msg;
}

CC_EXPORT const char* cc_last_error() {
    return g_last_error.c_str();
}

// ============================ 辅助函数 ============================

// 快速中值计算（使用 nth_element，会修改输入向量）
// 参数: data - float 向量（会被部分排序）
// 返回: 中值
static float quick_median(std::vector<float>& data) {
    if (data.empty()) return 0.0f;
    size_t n = data.size();
    if (n % 2 == 0) {
        // 偶数个: 取中间两个的平均
        std::nth_element(data.begin(), data.begin() + n / 2 - 1, data.end());
        float a = data[n / 2 - 1];
        std::nth_element(data.begin(), data.begin() + n / 2, data.end());
        float b = data[n / 2];
        return (a + b) * 0.5f;
    } else {
        // 奇数个: 取中间值
        std::nth_element(data.begin(), data.begin() + n / 2, data.end());
        return data[n / 2];
    }
}

// ============================ 连通区域过滤（星点保护） ============================

// BFS 标记8连通区域，移除大小 >= max_structure_size 的区域（星点）
// 参数: mask - 输入/输出掩码 [H*W]，1=候选坏像素，过滤后大区域被置0
// H, W - 图像尺寸
// max_structure_size - 最大结构大小，>=此值的区域被移除
static void filter_connected_components(
    uint8_t* mask, int H, int W, int max_structure_size
) {
    int N = H * W;
    std::vector<int> labels(N, 0);   // 区域标记，0=未访问
    std::vector<int> queue;           // BFS 队列（复用避免重复分配）
    queue.reserve(4096);
    int current_label = 0;

    // 8连通方向偏移
    static const int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    static const int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};

    for (int i = 0; i < N; i++) {
        if (mask[i] == 1 && labels[i] == 0) {
            current_label++;
            queue.clear();
            queue.push_back(i);
            labels[i] = current_label;

            // BFS 遍历连通区域
            size_t head = 0;
            while (head < queue.size()) {
                int idx = queue[head++];
                int y = idx / W;
                int x = idx % W;
                // 检查8个邻居
                for (int d = 0; d < 8; d++) {
                    int ny = y + dy[d];
                    int nx = x + dx[d];
                    if (ny < 0 || ny >= H || nx < 0 || nx >= W) continue;
                    int nidx = ny * W + nx;
                    if (mask[nidx] == 1 && labels[nidx] == 0) {
                        labels[nidx] = current_label;
                        queue.push_back(nidx);
                    }
                }
            }

            // 区域大小 >= max_structure_size 则移除（星点等大结构）
            int region_size = (int)queue.size();
            if (region_size >= max_structure_size) {
                for (int idx : queue) {
                    mask[idx] = 0;
                }
            }
        }
    }
}

// ============================ 中值滤波修复 ============================

CC_EXPORT long long cc_correct_median(
    float* data,
    const uint8_t* bad_mask,
    int H, int W,
    int window
) {
    // 参数校验
    if (!data || !bad_mask || H <= 0 || W <= 0) {
        set_error("cc_correct_median: 无效的输入参数（空指针或非正尺寸）");
        return -1;
    }
    if (window < 3 || window % 2 == 0) {
        set_error("cc_correct_median: window 必须为奇数且 >= 3");
        return -1;
    }
    if (window > 15) {
        set_error("cc_correct_median: window exceeds 15");
        return -1;
    }

    int N = H * W;

    // 统计坏像素数
    long long n_bad = 0;
    for (int i = 0; i < N; i++) {
        if (bad_mask[i] == 1) n_bad++;
    }
    if (n_bad == 0) {
        fprintf(stderr, "[cc_correct_median] 无坏像素需要修复\n");
        return 0;
    }

    fprintf(stderr, "[cc_correct_median] 开始修复: %dx%d, 坏像素=%lld, window=%d, 线程=%d\n",
            H, W, n_bad, window, omp_get_max_threads());

    // 复制原始数据，修复时从副本读邻域，避免串行污染
    std::vector<float> backup(data, data + N);

    int half = window / 2;
    long long count = 0;

    // OpenMP 按行并行，dynamic 调度均衡负载（坏像素分布不均）
    #pragma omp parallel for schedule(dynamic, 64) reduction(+:count)
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int idx = y * W + x;
            if (bad_mask[idx] != 1) continue;

            // 收集窗口内好像素（clamp 边缘处理）
            float neighbors[256];  // 栈上缓冲区，支持最大 15x15 窗口
            int n_count = 0;

            int y_start = y - half < 0 ? 0 : y - half;
            int y_end = y + half >= H ? H - 1 : y + half;
            int x_start = x - half < 0 ? 0 : x - half;
            int x_end = x + half >= W ? W - 1 : x + half;

            for (int ny = y_start; ny <= y_end; ny++) {
                for (int nx = x_start; nx <= x_end; nx++) {
                    int nidx = ny * W + nx;
                    if (bad_mask[nidx] == 0) {
                        neighbors[n_count++] = backup[nidx];
                    }
                }
            }

            if (n_count > 0) {
                // 用 nth_element 快速求中值
                float* begin = neighbors;
                float* end = neighbors + n_count;
                if (n_count % 2 == 0) {
                    std::nth_element(begin, begin + n_count / 2 - 1, end);
                    float a = neighbors[n_count / 2 - 1];
                    std::nth_element(begin, begin + n_count / 2, end);
                    float b = neighbors[n_count / 2];
                    data[idx] = (a + b) * 0.5f;
                } else {
                    std::nth_element(begin, begin + n_count / 2, end);
                    data[idx] = neighbors[n_count / 2];
                }
                count++;
            }
        }
    }

    fprintf(stderr, "[cc_correct_median] 修复完成: 实际修复 %lld / %lld 像素\n", count, n_bad);
    return count;
}

// ============================ 热像素检测 ============================

CC_EXPORT long long cc_detect_hot(
    const float* dark_data,
    int H, int W,
    double sigma,
    int max_structure_size,
    uint8_t* out_mask
) {
    if (!dark_data || !out_mask || H <= 0 || W <= 0) {
        set_error("cc_detect_hot: 无效的输入参数（空指针或非正尺寸）");
        return -1;
    }

    int N = H * W;

    // 1. 计算全局 median
    std::vector<float> data_copy(dark_data, dark_data + N);
    float median = quick_median(data_copy);

    // 2. 计算 MAD = median(|v - median|)
    std::vector<float> abs_dev(N);
    for (int i = 0; i < N; i++) {
        abs_dev[i] = std::fabs(dark_data[i] - median);
    }
    float mad = quick_median(abs_dev);

    // sigma = 1.4826 * MAD，MAD=0 时回退到标准差
    double std_val = 1.4826 * (double)mad;
    if (mad <= 0) {
        double sum = 0.0, sum_sq = 0.0;
        for (int i = 0; i < N; i++) {
            sum += dark_data[i];
            sum_sq += (double)dark_data[i] * dark_data[i];
        }
        double mean = sum / N;
        std_val = std::sqrt(std::max(0.0, sum_sq / N - mean * mean));
    }

    double threshold = (double)median + sigma * std_val;

    fprintf(stderr, "[cc_detect_hot] median=%.4f, MAD=%.4f, std=%.4f, threshold=%.4f (%.1fσ)\n",
            median, mad, std_val, threshold, sigma);

    // 3. 标记候选热像素（高于阈值）
    long long n_candidates = 0;
    memset(out_mask, 0, (size_t)N);
    for (int i = 0; i < N; i++) {
        if ((double)dark_data[i] > threshold) {
            out_mask[i] = 1;
            n_candidates++;
        }
    }

    fprintf(stderr, "[cc_detect_hot] 候选热像素: %lld\n", n_candidates);

    // 4. 连通区域过滤（排除星点等大结构）
    filter_connected_components(out_mask, H, W, max_structure_size);

    // 5. 统计最终结果
    long long n_final = 0;
    for (int i = 0; i < N; i++) {
        if (out_mask[i] == 1) n_final++;
    }

    fprintf(stderr, "[cc_detect_hot] 过滤后热像素: %lld (max_structure_size=%d)\n",
            n_final, max_structure_size);
    return n_final;
}

// ============================ 冷像素检测 ============================

CC_EXPORT long long cc_detect_cold(
    const float* bias_data,
    int H, int W,
    double sigma,
    int max_structure_size,
    uint8_t* out_mask
) {
    if (!bias_data || !out_mask || H <= 0 || W <= 0) {
        set_error("cc_detect_cold: 无效的输入参数（空指针或非正尺寸）");
        return -1;
    }

    int N = H * W;

    // 1. 计算全局 median
    std::vector<float> data_copy(bias_data, bias_data + N);
    float median = quick_median(data_copy);

    // 2. 计算 MAD = median(|v - median|)
    std::vector<float> abs_dev(N);
    for (int i = 0; i < N; i++) {
        abs_dev[i] = std::fabs(bias_data[i] - median);
    }
    float mad = quick_median(abs_dev);

    // sigma = 1.4826 * MAD，MAD=0 时回退到标准差
    double std_val = 1.4826 * (double)mad;
    if (mad <= 0) {
        double sum = 0.0, sum_sq = 0.0;
        for (int i = 0; i < N; i++) {
            sum += bias_data[i];
            sum_sq += (double)bias_data[i] * bias_data[i];
        }
        double mean = sum / N;
        std_val = std::sqrt(std::max(0.0, sum_sq / N - mean * mean));
    }

    double threshold = (double)median - sigma * std_val;

    fprintf(stderr, "[cc_detect_cold] median=%.4f, MAD=%.4f, std=%.4f, threshold=%.4f (%.1fσ)\n",
            median, mad, std_val, threshold, sigma);

    // 3. 标记候选冷像素（低于阈值）
    long long n_candidates = 0;
    memset(out_mask, 0, (size_t)N);
    for (int i = 0; i < N; i++) {
        if ((double)bias_data[i] < threshold) {
            out_mask[i] = 1;
            n_candidates++;
        }
    }

    fprintf(stderr, "[cc_detect_cold] 候选冷像素: %lld\n", n_candidates);

    // 4. 连通区域过滤（排除星点等大结构）
    filter_connected_components(out_mask, H, W, max_structure_size);

    // 5. 统计最终结果
    long long n_final = 0;
    for (int i = 0; i < N; i++) {
        if (out_mask[i] == 1) n_final++;
    }

    fprintf(stderr, "[cc_detect_cold] 过滤后冷像素: %lld (max_structure_size=%d)\n",
            n_final, max_structure_size);
    return n_final;
}
