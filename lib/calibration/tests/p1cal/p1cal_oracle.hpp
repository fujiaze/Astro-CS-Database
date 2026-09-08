// P1-CAL-TEST · 独立 oracle
//
// 合同锚: docs/algorithms/CALIBRATION_ALGORITHMS.md §9.1 (TEST-CAL-DESIGN-001)。
//
// 独立性规则 (模板 <prefix>-TEST §3): oracle 不调用被测函数、不复制同一实现。
// 推导路径:
//   - 像素级校准算术: 以解析/代数化简式在 double 域重算 (ALG-CAL-003 代数等价),
//     避免 FP32 逐算子的位数级仿真 → 比较 rtol=1e-6 / atol=1e-7 (§9 冻结容差)。
//   - 排序类统计 (median/MAD): 使用复制+sort 路径, 与被测实现的
//     nth_element 选择路径不同源 (ALG-CAL §3 中位数定义一致, 实现不同)。
//   - 结构过滤/修复: 以明确定义逐域 BFS (deque 队列, 非被测 std::queue 实现路径)
//     与 4 方向 1/dist 加权直接复算。
#ifndef P1CAL_ORACLE_HPP
#define P1CAL_ORACLE_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <limits>
#include <vector>

namespace p1cal {

// 复制+sort 中位数 (偶数取双中位均值; 与实现语义一致, 实现路径不同源)
inline double median_oracle(std::vector<double> v) {
    if (v.empty()) return std::numeric_limits<double>::quiet_NaN();
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    if (n % 2 == 1) return v[n / 2];
    return 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

// ---------------------------------------------------------------------------
// ALG-CAL-003 oracle: 单帧校准, double 域整式复算
//   标准分支:   out = (light − dark·[dark!=NULL]) / max(flat, 0.1)·[flat!=NULL]
//   dark_opt:   out = (light − bias − K·(dark − bias)) / max(flat, 0.1)·[flat!=NULL]
// (分支选择语义: dark_opt==1 && bias && dark 才走 dark_opt, 否则标准分支 k=1)
// ---------------------------------------------------------------------------
inline double calibrate_oracle(std::size_t /*i*/,
                               double light, double dark, bool has_dark,
                               double bias, bool has_bias,
                               double flat, bool has_flat,
                               bool dark_opt, double k) {
    if (dark_opt && has_bias && has_dark) {
        double v = light - bias - k * (dark - bias);
        if (has_flat) v /= std::max(flat, 0.1);
        return v;
    }
    double v = light;
    if (has_dark) v -= dark;
    if (has_flat) v /= std::max(flat, 0.1);
    return v;
}

// dark_opt 生效时的 actual_k 恒等映射 oracle (ALG §9.1: dark_opt=1 → actual_k==k_init)
inline double actual_k_oracle(bool dark_opt, bool has_bias, bool has_dark, double k_init) {
    return (dark_opt && has_bias && has_dark) ? k_init : 1.0;
}

// ---------------------------------------------------------------------------
// ALG-CAL-001 oracle: 迭代 sigma-clip 合并, double 域独立复算 (ALG §3 F1):
//   迭代: med = median(非NaN), MAD = median(|v-med|), sigma = 1.4826*MAD;
//         sigma<=0 终止; 拒绝 dev < -sigma_low*sigma 或 dev > sigma_high*sigma;
//         本轮零拒绝 → 收敛; max_iter 轮上限。
//   合并: mean = 双域均值 (实现为 FP32 累加, 容差覆盖); median 同规则。
// 独立性: sort 统计 + double 域, 与实现 (nth_element + FP32) 数值路径不同。
// ---------------------------------------------------------------------------
template <typename T>
inline double master_gen_oracle(const T* stack, std::size_t npix, std::size_t idx,
                                int n_frames, double sigma_low, double sigma_high,
                                int max_iter, bool combine_median) {
    std::vector<double> vals(n_frames);
    for (int n = 0; n < n_frames; ++n)
        vals[static_cast<std::size_t>(n)] =
            static_cast<double>(stack[static_cast<std::size_t>(n) * npix + idx]);

    for (int iter = 0; iter < max_iter; ++iter) {
        std::vector<double> work;
        for (double v : vals)
            if (!std::isnan(v)) work.push_back(v);
        if (work.empty()) break;
        const double med = median_oracle(work);
        std::vector<double> devs;
        for (double v : vals)
            if (!std::isnan(v)) devs.push_back(std::fabs(v - med));
        const double sigma = 1.4826 * median_oracle(devs);
        if (sigma <= 0.0) break;
        int rejected = 0;
        for (double& v : vals) {
            if (std::isnan(v)) continue;
            const double dev = v - med;
            if (dev < -sigma_low * sigma || dev > sigma_high * sigma) {
                v = std::numeric_limits<double>::quiet_NaN();
                ++rejected;
            }
        }
        if (rejected == 0) break;
    }

    std::vector<double> keep;
    for (double v : vals)
        if (!std::isnan(v)) keep.push_back(v);
    if (keep.empty()) return std::numeric_limits<double>::quiet_NaN();
    if (combine_median) return median_oracle(keep);
    double sum = 0.0;
    for (double v : keep) sum += v;
    return sum / static_cast<double>(keep.size());
}

// ---------------------------------------------------------------------------
// ALG-CAL-002 oracle: master flat 全链 (step1 逐帧中位数归一 + floor 0.1 →
// step2 迭代 sigma-clip + mean 合并 → step3 final 中位数归一 + floor 0.1),
// double 域复算。
// ---------------------------------------------------------------------------
struct FlatOracle {
    bool ok;
    std::vector<double> out;   // 最终期望 (step3 后)
};

inline FlatOracle master_flat_oracle(const std::vector<float>& stack,
                                     std::size_t n_frames, std::size_t w, std::size_t h,
                                     const std::vector<float>* bias,
                                     double sigma_low = 3.0, double sigma_high = 3.0,
                                     int max_iter = 5) {
    const std::size_t npix = w * h;
    FlatOracle res;
    res.ok = false;
    if (n_frames == 0 || npix == 0) return res;

    // step1: 逐帧减 bias + 中位数归一 + floor 0.1
    std::vector<double> norm(n_frames * npix);
    for (std::size_t n = 0; n < n_frames; ++n) {
        std::vector<double> dst(npix);
        for (std::size_t i = 0; i < npix; ++i) {
            dst[i] = static_cast<double>(stack[n * npix + i]);
            if (bias) dst[i] -= static_cast<double>((*bias)[i]);
        }
        const double frame_med = median_oracle(dst);
        if (!(frame_med > 0.0)) return res;  // 与实现一致: frame_med<=0/NaN 拒绝
        for (std::size_t i = 0; i < npix; ++i) {
            double v = dst[i] / frame_med;
            if (v < 0.1) v = 0.1;
            norm[n * npix + i] = v;
        }
    }

    // step2: 迭代 sigma-clip + mean 合并 (复用 ALG-CAL-001 oracle, double 域)。
    for (std::size_t i = 0; i < npix; ++i) {
        res.out.push_back(master_gen_oracle(norm.data(), npix, i,
                                            static_cast<int>(n_frames),
                                            sigma_low, sigma_high, max_iter, false));
    }

    // step3: final 中位数归一 + floor 0.1
    const double final_med = median_oracle(res.out);
    if (!(final_med > 0.0)) { res.ok = false; return res; }
    for (auto& v : res.out) {
        v = std::max(v / final_med, 0.1);
    }
    res.ok = true;
    return res;
}

// ---------------------------------------------------------------------------
// ALG-CAL-004 oracle:
//   热像素阈值:   dark > med + hot_sigma·1.4826·MAD(dark)
//   冷像素阈值:   bias < med − cold_sigma·1.4826·MAD(bias)
// (阈值统计与被测实现同定义; MAD 复算走 median_oracle sort 路径)
// ---------------------------------------------------------------------------
inline std::vector<char> detect_hot_oracle(const std::vector<float>& dark,
                                           double hot_sigma) {
    std::vector<char> mask(dark.size(), 0);
    std::vector<double> dv(dark.size());
    for (std::size_t i = 0; i < dark.size(); ++i) dv[i] = dark[i];
    const double med = median_oracle(dv);
    std::vector<double> abs_dev(dark.size());
    for (std::size_t i = 0; i < dark.size(); ++i) abs_dev[i] = std::fabs(dv[i] - med);
    const double sigma = 1.4826 * median_oracle(abs_dev);
    const double thr = med + hot_sigma * sigma;
    for (std::size_t i = 0; i < dark.size(); ++i) mask[i] = (dv[i] > thr) ? 1 : 0;
    return mask;
}

inline std::vector<char> detect_cold_oracle(const std::vector<float>& bias,
                                            double cold_sigma) {
    std::vector<char> mask(bias.size(), 0);
    std::vector<double> bv(bias.size());
    for (std::size_t i = 0; i < bias.size(); ++i) bv[i] = bias[i];
    const double med = median_oracle(bv);
    std::vector<double> abs_dev(bias.size());
    for (std::size_t i = 0; i < bias.size(); ++i) abs_dev[i] = std::fabs(bv[i] - med);
    const double sigma = 1.4826 * median_oracle(abs_dev);
    const double thr = med - cold_sigma * sigma;
    for (std::size_t i = 0; i < bias.size(); ++i) mask[i] = (bv[i] < thr) ? 1 : 0;
    return mask;
}

// 8 连通域 size>=max_size 的清除 (背景 label 0 不参与)
inline void filter_structure_oracle(std::vector<char>* mask, std::size_t w, std::size_t h,
                                    std::size_t max_size) {
    const std::size_t npix = w * h;
    std::vector<int> label(npix, 0);
    int num = 0;
    for (std::size_t s = 0; s < npix; ++s) {
        if (!(*mask)[s] || label[s] != 0) continue;
        const int id = ++num;
        std::deque<std::size_t> q{s};
        label[s] = id;
        while (!q.empty()) {
            const std::size_t cur = q.front(); q.pop_front();
            const std::size_t x = cur % w, y = cur / w;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    const std::ptrdiff_t nx = static_cast<std::ptrdiff_t>(x) + dx;
                    const std::ptrdiff_t ny = static_cast<std::ptrdiff_t>(y) + dy;
                    if (nx < 0 || nx >= static_cast<std::ptrdiff_t>(w) ||
                        ny < 0 || ny >= static_cast<std::ptrdiff_t>(h)) continue;
                    const std::size_t ni = static_cast<std::size_t>(ny) * w + static_cast<std::size_t>(nx);
                    if ((*mask)[ni] && label[ni] == 0) {
                        label[ni] = id;
                        q.push_back(ni);
                    }
                }
            }
        }
    }
    if (num == 0) return;
    std::vector<int> sizes(num + 1, 0);
    for (std::size_t i = 0; i < npix; ++i)
        if (label[i] > 0) ++sizes[label[i]];
    for (std::size_t i = 0; i < npix; ++i)
        if (label[i] > 0 && static_cast<std::size_t>(sizes[label[i]]) >= max_size)
            (*mask)[i] = 0;
}

// 5x5 中值修复 oracle (镜像反射索引, 仅收非 bad 邻居, 空 → 原值)
inline double median_fix_oracle(const std::vector<float>& data, const std::vector<char>& bad,
                                std::size_t w, std::size_t h, std::size_t x, std::size_t y) {
    std::vector<double> vals;
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            std::ptrdiff_t nx = static_cast<std::ptrdiff_t>(x) + dx;
            std::ptrdiff_t ny = static_cast<std::ptrdiff_t>(y) + dy;
            if (nx < 0) nx = -nx;
            if (ny < 0) ny = -ny;
            if (nx >= static_cast<std::ptrdiff_t>(w)) nx = 2 * static_cast<std::ptrdiff_t>(w) - nx - 2;
            if (ny >= static_cast<std::ptrdiff_t>(h)) ny = 2 * static_cast<std::ptrdiff_t>(h) - ny - 2;
            if (nx < 0) nx = 0;
            if (ny < 0) ny = 0;
            if (nx >= static_cast<std::ptrdiff_t>(w)) nx = static_cast<std::ptrdiff_t>(w) - 1;
            if (ny >= static_cast<std::ptrdiff_t>(h)) ny = static_cast<std::ptrdiff_t>(h) - 1;
            const std::size_t ni = static_cast<std::size_t>(ny) * w + static_cast<std::size_t>(nx);
            if (!bad[ni]) vals.push_back(data[ni]);
        }
    }
    if (vals.empty()) return data[y * w + x];
    return median_oracle(vals);
}

// 4 方向最近非 bad 像素 1/dist 加权 (IDW; 全方向无 → 原值)
inline double idw_fix_oracle(const std::vector<float>& data, const std::vector<char>& bad,
                             std::size_t w, std::size_t h, std::size_t x, std::size_t y) {
    double sum_wv = 0.0, sum_w = 0.0;
    const int dirs[4][2] = {{-1,0},{1,0},{0,-1},{0,1}};
    for (int d = 0; d < 4; ++d) {
        const int dx = dirs[d][0], dy = dirs[d][1];
        std::ptrdiff_t nx = static_cast<std::ptrdiff_t>(x) + dx;
        std::ptrdiff_t ny = static_cast<std::ptrdiff_t>(y) + dy;
        double dist = 1.0;
        while (nx >= 0 && nx < static_cast<std::ptrdiff_t>(w) &&
               ny >= 0 && ny < static_cast<std::ptrdiff_t>(h) &&
               bad[static_cast<std::size_t>(ny) * w + static_cast<std::size_t>(nx)]) {
            nx += dx; ny += dy; dist += 1.0;
        }
        if (nx >= 0 && nx < static_cast<std::ptrdiff_t>(w) &&
            ny >= 0 && ny < static_cast<std::ptrdiff_t>(h)) {
            const double weight = 1.0 / dist;
            sum_wv += data[static_cast<std::size_t>(ny) * w + static_cast<std::size_t>(nx)] * weight;
            sum_w += weight;
        }
    }
    if (sum_w > 0.0) return sum_wv / sum_w;
    return data[y * w + x];
}

}  // namespace p1cal

#endif  // P1CAL_ORACLE_HPP
