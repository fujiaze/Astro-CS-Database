// P1-COS-TEST · 独立 oracle
//
// 合同锚: docs/algorithms/COSMETIC_ALGORITHMS.md §9 TEST-COS-DESIGN-001
// (P1-COS-DOC 冻结, 2026-09-07, wave W1); 容差标度 SCI-CAL-001 §11。
//
// 独立性规则 (模板 <prefix>-TEST §3): oracle 不调用被测函数、不复制同一实现。
// 推导路径与被测实现 (lib/calibration/src/cosmetic_corrector.cpp) 的差异:
//   - 中位数: 复制+std::sort 全排序 (被测: std::nth_element 选择路径);
//     偶数取双中位均值 (0.5*(v[k-1]+v[k])) 与被测 (hi+lo)*0.5 语义一致,
//     数值路径不同源; 统计全程 double 域 (被测: float 域)。
//   - 连通域: std::deque 队列 BFS (被测: std::queue + 单遍标记), size 统计
//     在 BFS 内直接累计 (被测: 二次全图扫描)。
//   - 插值: double 域逐方向 1/dist 直接复算 + 镜像坐标独立推导 (对称取负
//     递归式), 与被测 float 域镜像 clamp 分支不同路径。
//   - 期望值绝不调用被测函数 (模板 <prefix>-TEST §3 / 验收"无生产函数生成
//     期望值")。
// 容差: FIX-COS-B 冻结 rtol=1e-6, atol=1e-7 (SCI-CAL-001 §11 预冻结标度);
// FIX-COS-E/F bitwise 由 2 的幂 fixture 值保证 (零舍入)。
#ifndef P1COS_ORACLE_HPP
#define P1COS_ORACLE_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <deque>
#include <limits>
#include <vector>

namespace p1cos {

// 冻结容差 (ALG §9 FIX-COS-B; 不得放宽, P1-COS-TEST 无权改)
inline constexpr double kCosRtol = 1e-6;
inline constexpr double kCosAtol = 1e-7;

// 复制+sort 中位数 (double 域; 偶数取双中位均值)
inline double median_oracle(std::vector<double> v) {
    if (v.empty()) return std::numeric_limits<double>::quiet_NaN();
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    if (n % 2 == 1) return v[n / 2];
    return 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

// 全局统计 oracle (ALG-COS-001): med / mad / sigma=1.4826*mad, double 域
struct StatsOracle {
    double med, mad, sigma;
};

inline StatsOracle stats_oracle(const std::vector<float>& src) {
    std::vector<double> dv(src.size());
    for (std::size_t i = 0; i < src.size(); ++i) dv[i] = src[i];
    StatsOracle s{};
    s.med = median_oracle(dv);
    std::vector<double> absdev(src.size());
    for (std::size_t i = 0; i < src.size(); ++i) absdev[i] = std::fabs(dv[i] - s.med);
    s.mad = median_oracle(absdev);
    s.sigma = 1.4826 * s.mad;
    return s;
}

// ALG-COS-001 判定 oracle (掩码极性 1=坏点, SCI-CAL-001 §9a; 严格不等式)
// 结构过滤由调用方显式调用 filter_structure_oracle (保持两步可断言)。
inline std::vector<char> detect_hot_oracle(const std::vector<float>& dark, double hot_sigma) {
    const StatsOracle s = stats_oracle(dark);
    const double thr = s.med + hot_sigma * s.sigma;
    std::vector<char> mask(dark.size(), 0);
    for (std::size_t i = 0; i < dark.size(); ++i) mask[i] = (dark[i] > thr) ? 1 : 0;
    return mask;
}

inline std::vector<char> detect_cold_oracle(const std::vector<float>& bias, double cold_sigma) {
    const StatsOracle s = stats_oracle(bias);
    const double thr = s.med - cold_sigma * s.sigma;
    std::vector<char> mask(bias.size(), 0);
    for (std::size_t i = 0; i < bias.size(); ++i) mask[i] = (bias[i] < thr) ? 1 : 0;
    return mask;
}

// ALG-COS-002 结构过滤 oracle: 8 连通域 size >= max_size 清除 (deque BFS,
// BFS 内累计 size — 与被测二次扫描路径不同源)
inline void filter_structure_oracle(std::vector<char>* mask, std::size_t w, std::size_t h,
                                    int max_size) {
    const std::size_t npix = w * h;
    if (npix == 0) return;
    std::vector<int> label(npix, 0);
    int num = 0;
    const int wi = static_cast<int>(w);
    const int hi = static_cast<int>(h);
    for (std::size_t s = 0; s < npix; ++s) {
        if (!(*mask)[s] || label[s] != 0) continue;
        const int lab = ++num;
        std::deque<std::size_t> q;
        q.push_back(s);
        label[s] = lab;
        int size = 0;
        while (!q.empty()) {
            const std::size_t idx = q.front();
            q.pop_front();
            ++size;
            const int x = static_cast<int>(idx % w);
            const int y = static_cast<int>(idx / w);
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    const int nx = x + dx, ny = y + dy;
                    if (nx < 0 || nx >= wi || ny < 0 || ny >= hi) continue;
                    const std::size_t nidx = static_cast<std::size_t>(ny) * w + static_cast<std::size_t>(nx);
                    if ((*mask)[nidx] && label[nidx] == 0) {
                        label[nidx] = lab;
                        q.push_back(nidx);
                    }
                }
            }
        }
        if (size >= max_size) {
            // 重新遍历该域清除 (独立二次遍历: 从 label 表直接收集)
            for (std::size_t i = 0; i < npix; ++i)
                if (label[i] == lab) (*mask)[i] = 0;
        }
    }
}

// 镜像坐标 oracle (ALG-COS-003 median 路径边界语义): 独立推导为
// "对称取负直到入界再 clamp 上界"的等价闭式, 与被测顺序分支路径不同源。
inline std::size_t mirror_coord_oracle(std::ptrdiff_t v, std::ptrdiff_t n) {
    if (n <= 1) return 0;
    std::ptrdiff_t m = v;
    while (m < 0 || m >= n) {
        if (m < 0) m = -m;
        if (m >= n) m = 2 * n - m - 2;
    }
    if (m < 0) m = 0;
    if (m >= n) m = n - 1;
    return static_cast<std::size_t>(m);
}

// ALG-COS-003 median 修复 oracle (method=AC_METHOD_MEDIAN): 5×5 镜像窗内
// 非坏像素 double 域中值; 空邻域回退原值 (I6)。
inline float median_repair_oracle(const std::vector<float>& data, const std::vector<char>& bad,
                                  std::size_t w, std::size_t h, std::size_t idx) {
    const std::size_t x = idx % w, y = idx / w;
    std::vector<double> vals;
    vals.reserve(25);
    for (std::ptrdiff_t dy = -2; dy <= 2; ++dy) {
        for (std::ptrdiff_t dx = -2; dx <= 2; ++dx) {
            const std::size_t nx = mirror_coord_oracle(static_cast<std::ptrdiff_t>(x) + dx,
                                                       static_cast<std::ptrdiff_t>(w));
            const std::size_t ny = mirror_coord_oracle(static_cast<std::ptrdiff_t>(y) + dy,
                                                       static_cast<std::ptrdiff_t>(h));
            const std::size_t nidx = ny * w + nx;
            if (!bad[nidx]) vals.push_back(data[nidx]);
        }
    }
    if (vals.empty()) return data[idx];
    return static_cast<float>(median_oracle(vals));
}

// ALG-COS-003 IDW 修复 oracle (method!=0 现状分支, DISP-COS-003): 4 方向
// (左,右,上,下 — 与被测方向表顺序一致, 保证同序 float 断言可行) 最近好
// 像素 1/dist 加权, double 域复算; sum_w==0 回退原值 (I6)。
inline float idw_repair_oracle(const std::vector<float>& data, const std::vector<char>& bad,
                               std::size_t w, std::size_t h, std::size_t idx) {
    const std::size_t x = idx % w, y = idx / w;
    const int dirs[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    double sum_wv = 0.0, sum_w = 0.0;
    for (int d = 0; d < 4; ++d) {
        const int dx = dirs[d][0], dy = dirs[d][1];
        std::ptrdiff_t nx = static_cast<std::ptrdiff_t>(x) + dx;
        std::ptrdiff_t ny = static_cast<std::ptrdiff_t>(y) + dy;
        int dist = 1;
        while (nx >= 0 && nx < static_cast<std::ptrdiff_t>(w) &&
               ny >= 0 && ny < static_cast<std::ptrdiff_t>(h) &&
               bad[static_cast<std::size_t>(ny) * w + static_cast<std::size_t>(nx)]) {
            nx += dx;
            ny += dy;
            ++dist;
        }
        if (nx >= 0 && nx < static_cast<std::ptrdiff_t>(w) &&
            ny >= 0 && ny < static_cast<std::ptrdiff_t>(h)) {
            const double weight = 1.0 / static_cast<double>(dist);
            sum_wv += data[static_cast<std::size_t>(ny) * w + static_cast<std::size_t>(nx)] * weight;
            sum_w += weight;
        }
    }
    if (sum_w > 0.0) return static_cast<float>(sum_wv / sum_w);
    return data[idx];
}

// ALG-COS-001..004 组合 oracle (double 域全链): 期望帧 + 期望计数。
// detect → 结构过滤 → 合并 → 插值, 与 ac::correct_frame 语义一致、数值路径
// 不同源 (sort 统计 / deque BFS / double 插值)。
struct CorrectOracle {
    std::vector<float> out;
    int n_hot = 0, n_cold = 0;
};

inline CorrectOracle correct_oracle(const std::vector<float>& data, std::size_t w, std::size_t h,
                                    const std::vector<float>* dark, const std::vector<float>* bias,
                                    double hot_sigma, double cold_sigma,
                                    int method, int max_size) {
    const std::size_t npix = w * h;
    std::vector<char> hot(npix, 0), cold(npix, 0);
    if (dark && hot_sigma > 0.0) {
        hot = detect_hot_oracle(*dark, hot_sigma);
        filter_structure_oracle(&hot, w, h, max_size);
    }
    if (bias && cold_sigma > 0.0) {
        cold = detect_cold_oracle(*bias, cold_sigma);
        filter_structure_oracle(&cold, w, h, max_size);
    }
    CorrectOracle r;
    r.out.resize(npix);
    std::vector<char> all_bad(npix, 0);
    for (std::size_t i = 0; i < npix; ++i) {
        if (hot[i]) ++r.n_hot;
        if (cold[i]) ++r.n_cold;
        all_bad[i] = (hot[i] || cold[i]) ? 1 : 0;
    }
    for (std::size_t i = 0; i < npix; ++i) {
        if (!all_bad[i]) {
            r.out[i] = data[i];
        } else if (method == 0) {  // AC_METHOD_MEDIAN
            r.out[i] = median_repair_oracle(data, all_bad, w, h, i);
        } else {
            r.out[i] = idw_repair_oracle(data, all_bad, w, h, i);
        }
    }
    return r;
}

// 冻结容差比较 (rtol=1e-6, atol=1e-7): |got-want| <= atol + rtol*|want|
inline bool close_enough(double got, double want) {
    return std::fabs(got - want) <= kCosAtol + kCosRtol * std::fabs(want);
}

// NaN/Inf 感知位型比较 (float32): NaN 槽位要求位型一致 (透传路径无算术)
inline bool bit_equal(float a, float b) {
    std::uint32_t ua, ub;
    static_assert(sizeof(ua) == sizeof(a), "float32 required");
    std::memcpy(&ua, &a, sizeof(ua));
    std::memcpy(&ub, &b, sizeof(ub));
    return ua == ub;
}

}  // namespace p1cos

#endif  // P1COS_ORACLE_HPP
