// lib/phase2/src/rejection.cpp — Phase2 Rejection Framework CPU reference
//
// W7（控制包 34A532A2...B2EB308 + wiki Phase2_Rejection）：
//   - 输入：UPM-calibrated 样本栈 values[]/valid[]/support[]/weights[]/quality[]；
//   - 首版实现：None、Sigma、WinsorizedSigma（确定性，Oracle 对照）；
//   - AveragedSigma/LinearFit/ESD/RCR 接口冻结，后续子任务按论文/Oracle 独立实现；
//   - 输出 accepted mask + low/high 计数 + 迭代数 + status。
#include "astro/phase2/rejection.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

inline double median(std::vector<double> v) {
    if (v.empty()) return 0.0;
    const std::size_t n = v.size();
    const std::size_t mid = n / 2;
    std::nth_element(v.begin(), v.begin() + mid, v.end());
    if (n % 2 == 1) return v[mid];
    const double a = v[mid];
    // nth_element 后 [begin, mid) 全部 ≤ v[mid]；第 mid 小（即排序后
    // v[mid-1]）是前 mid 个元素的最大值。
    const double b = *std::max_element(v.begin(), v.begin() + mid);
    return 0.5 * (a + b);
}

inline double mad(std::vector<double> v, double med) {
    for (auto& x : v) x = std::fabs(x - med);
    return 1.4826 * median(std::move(v));
}

// 正则化不完全 beta I_x(a,b)（Lentz 连分数，Numerical Recipes betai/betacf 算法）
double ibeta_cf(double a, double b, double x) {
    const double fpmin = 1e-300;
    const double qab = a + b;
    const double qap = a + 1.0;
    const double qam = a - 1.0;
    double c = 1.0;
    double d = 1.0 - qab * x / qap;
    if (std::fabs(d) < fpmin) d = fpmin;
    d = 1.0 / d;
    double h = d;
    for (int m = 1; m <= 300; ++m) {
        const int m2 = 2 * m;
        double aa = (double)m * (b - (double)m) * x /
                    ((qam + m2) * (a + m2));
        d = 1.0 + aa * d;
        if (std::fabs(d) < fpmin) d = fpmin;
        c = 1.0 + aa / c;
        if (std::fabs(c) < fpmin) c = fpmin;
        d = 1.0 / d;
        h *= d * c;
        aa = -(a + (double)m) * (qab + (double)m) * x /
             ((a + m2) * (qap + m2));
        d = 1.0 + aa * d;
        if (std::fabs(d) < fpmin) d = fpmin;
        c = 1.0 + aa / c;
        if (std::fabs(c) < fpmin) c = fpmin;
        d = 1.0 / d;
        const double del = d * c;
        h *= del;
        if (std::fabs(del - 1.0) < 1e-12) break;
    }
    return h;
}

double ibeta(double a, double b, double x) {
    if (x <= 0.0) return 0.0;
    if (x >= 1.0) return 1.0;
    const double ln = std::lgamma(a + b) - std::lgamma(a) -
                      std::lgamma(b) + a * std::log(x) +
                      b * std::log(1.0 - x);
    const double bt = std::exp(ln);
    if (x < (a + 1.0) / (a + b + 2.0))
        return bt * ibeta_cf(a, b, x) / a;
    return 1.0 - bt * ibeta_cf(b, a, 1.0 - x) / b;
}

// Student-t CDF（双侧对称）
double t_cdf(double t, double nu) {
    const double x = nu / (nu + t * t);
    // F(t) = 1 − ½·I_x(ν/2, 1/2)（t ≥ 0），x = ν/(ν+t²)
    if (t >= 0.0) return 1.0 - 0.5 * ibeta(nu / 2.0, 0.5, x);
    return 0.5 * ibeta(nu / 2.0, 0.5, x);
}

// Student-t 分位数（二分求逆，p 为单侧下尾概率）
double t_quantile(double p, double nu) {
    if (p <= 0.0) return 0.0;
    if (p >= 1.0) return 40.0;
    double lo = 0.0, hi = 40.0;
    for (int it = 0; it < 80; ++it) {
        const double mid = 0.5 * (lo + hi);
        const double cdf = t_cdf(mid, nu);
        if (cdf < p) lo = mid; else hi = mid;
    }
    return 0.5 * (lo + hi);
}

// 最小二乘直线 y = a*x + b（x 为样本序号）
void ls_fit_line(const std::vector<double>& xv, const std::vector<double>& yv,
                 double* a, double* b) {
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    const std::size_t n = xv.size();
    for (std::size_t i = 0; i < n; ++i) {
        sx += xv[i];
        sy += yv[i];
        sxx += xv[i] * xv[i];
        sxy += xv[i] * yv[i];
    }
    const double den = (double)n * sxx - sx * sx;
    if (std::fabs(den) < 1e-12) {
        *a = 0.0;
        *b = n ? sy / (double)n : 0.0;
        return;
    }
    *a = ((double)n * sxy - sx * sy) / den;
    *b = (sy - *a * sx) / (double)n;
}

} // namespace

extern "C" {

int p2_reject_stack(const P2SampleStackView* in, P2RejectionResult* out) {
    if (in == nullptr || out == nullptr) return 1;
    std::uint8_t* accepted_out = out->accepted;
    std::memset(out, 0, sizeof(*out));
    out->accepted = accepted_out;
    const std::uint32_t n = in->count;
    if (n == 0) { out->status = 1; return 0; }
    if (out->accepted == nullptr) return 1;

    // NaN/Inf 输入：标记无效
    for (std::uint32_t i = 0; i < n; ++i) {
        if (!std::isfinite(in->values[i])) {
            out->accepted[i] = 0;
            ++out->rejected_low;  // 无效归为拒绝（诊断）
            out->status = 3;
        } else {
            out->accepted[i] = 1;
        }
    }

    if (in->method == P2_REJECT_NONE) {
        // none：有效样本全接受
        std::uint32_t kept = 0;
        for (std::uint32_t i = 0; i < n; ++i) {
            if (in->valid != nullptr && !in->valid[i]) continue;
            out->accepted[i] = 1;
            ++kept;
        }
        out->accepted_count = kept;
        out->status = 0;
        return 0;
    }

    // 收集有效值（UPM-calibrated）
    std::vector<double> vals;
    std::vector<std::uint32_t> idx;
    for (std::uint32_t i = 0; i < n; ++i) {
        if (in->valid != nullptr && !in->valid[i]) continue;
        if (!std::isfinite(in->values[i])) continue;
        vals.push_back(in->values[i]);
        idx.push_back(i);
    }
    if (vals.size() < static_cast<std::size_t>(in->min_samples)) {
        out->status = 1;  // min-samples
        return 0;
    }

    const double lo = in->sigma_low != 0.0 ? in->sigma_low : -4.0;
    const double hi = in->sigma_high != 0.0 ? in->sigma_high : 3.0;
    const int max_iter = in->max_iterations > 0 ? in->max_iterations : 8;

    if (in->method == P2_REJECT_AVERAGED_SIGMA) {
        // IRAF AVSIGCLIP 语义（公开定义）：迭代中按"当前保留样本"计算均值与
        // 平均 sigma（对每样本残差绝对值的平均，而非 MAD），再按 lo/hi sigma 拒绝。
        std::vector<bool> accept(vals.size(), true);
        int it = 0;
        for (; it < max_iter; ++it) {
            double sum = 0.0; std::size_t n = 0;
            for (std::size_t i = 0; i < vals.size(); ++i) {
                if (accept[i]) { sum += vals[i]; ++n; }
            }
            if (n < 2) break;
            const double mean = sum / static_cast<double>(n);
            double s = 0.0;
            for (std::size_t i = 0; i < vals.size(); ++i) {
                if (accept[i]) s += std::fabs(vals[i] - mean);
            }
            s = (s / static_cast<double>(n)) * std::sqrt(3.141592653589793 / 2.0);
            if (s <= 1e-12) break;
            bool changed = false;
            for (std::size_t i = 0; i < vals.size(); ++i) {
                if (!accept[i]) continue;
                const double z = (vals[i] - mean) / s;
                if (z < lo || z > hi) { accept[i] = false; changed = true; }
            }
            if (!changed) break;
        }
        out->iterations = static_cast<std::uint32_t>(it);
        std::uint32_t kept = 0;
        for (std::size_t i = 0; i < vals.size(); ++i) {
            out->accepted[idx[i]] = accept[i] ? 1 : 0;
            if (accept[i]) { ++kept; }
            else if (vals[i] < 0.0) ++out->rejected_low; else ++out->rejected_high;
        }
        out->accepted_count = kept;
        out->status = kept == 0 ? 2 : 0;
        return 0;
    }

    if (in->method == P2_REJECT_GENERALIZED_ESD) {
        // NIST Generalized ESD（独立实现，NIST EDA Handbook / Rosner 1983）。
        // 正确流程（P0-05）：
        //   1. 连续移除到上限 r，记录 R_1..R_r 与 λ_1..λ_r；
        //   2. 最终取满足 R_i > λ_i 的**最大 i**，前 i 个 provisional
        //      extremes 为离群（不做"第一轮不显著就停止"的错误简化）。
        // 临界值 λ_i = t_{ν,p}·(n-i)/sqrt((n-i-1+t²)·(n-i+1))，
        // ν = n-i-1，p = 1 - α/(2·(n-i+1))，α=0.05。
        std::vector<bool> accept(vals.size(), true);
        std::vector<double> working = vals;
        const std::size_t max_out = static_cast<std::size_t>(max_iter);
        const double alpha = 0.05;
        std::vector<double> R(max_out, 0.0);
        std::vector<double> Lambda(max_out, 0.0);
        std::vector<std::size_t> removed_idx(max_out, vals.size());
        const std::size_t n0 = vals.size();
        std::size_t n_removed = 0;
        // phase 1：连续移除记录 R_i / λ_i
        for (std::size_t r = 0; r < max_out; ++r) {
            double mean = 0.0; std::size_t n = 0;
            for (std::size_t i = 0; i < working.size(); ++i) {
                if (accept[i]) { mean += working[i]; ++n; }
            }
            if (n < 3) break;
            mean /= static_cast<double>(n);
            double s = 0.0;
            for (std::size_t i = 0; i < working.size(); ++i) {
                if (accept[i]) s += (working[i] - mean) * (working[i] - mean);
            }
            s = std::sqrt(s / static_cast<double>(n - 1));
            if (s <= 1e-12) break;
            std::size_t worst = n;
            double max_r = 0.0;
            std::uint64_t worst_fid = ~0ULL;
            for (std::size_t i = 0; i < working.size(); ++i) {
                if (!accept[i]) continue;
                const double rv = std::fabs(working[i] - mean) / s;
                const std::uint64_t fid = in->frame_ids
                    ? in->frame_ids[idx[i]] : (std::uint64_t)i;
                if (rv > max_r + 1e-15 ||
                    (std::fabs(rv - max_r) <= 1e-15 && fid < worst_fid)) {
                    max_r = rv;
                    worst = i;
                    worst_fid = fid;
                }
            }
            // NIST 临界值（i 为 1-indexed 检测步；n-i = n0 - (r+1)）
            const double n_minus_i = (double)n0 - (double)(r + 1);
            const double nu = n_minus_i - 1.0;
            const double p = 1.0 - alpha / (2.0 * (n_minus_i + 1.0));
            const double tcrit = t_quantile(p, nu);
            const double crit = (tcrit * n_minus_i) /
                std::sqrt((nu + tcrit * tcrit) * (n_minus_i + 1.0));
            R[r] = max_r;
            Lambda[r] = crit;
            removed_idx[r] = worst;
            accept[worst] = false;
            ++n_removed;
        }
        // phase 2：取满足 R_i > λ_i 的最大 i
        std::size_t k_out = 0;
        for (std::size_t r = 0; r < n_removed; ++r)
            if (R[r] > Lambda[r]) k_out = r + 1;
        std::fill(accept.begin(), accept.end(), true);
        for (std::size_t r = 0; r < k_out && r < removed_idx.size(); ++r)
            accept[removed_idx[r]] = false;
        out->iterations = static_cast<std::uint32_t>(k_out);
        std::uint32_t kept = 0;
        for (std::size_t i = 0; i < vals.size(); ++i) {
            out->accepted[idx[i]] = accept[i] ? 1 : 0;
            if (accept[i]) { ++kept; }
            else if (vals[i] < 0.0) ++out->rejected_low; else ++out->rejected_high;
        }
        out->accepted_count = kept;
        out->status = kept == 0 ? 2 : 0;
        return 0;
    }

    if (in->method == P2_REJECT_LINEAR_FIT) {
        // Siril linear-fit 语义（公开文档，ORACLE ONLY）：对 pixel stack 拟合
        // 最佳直线 y=a*x+b 后基于残差迭代 clipping。
        // P1-05 修复：x 使用稳定 frame identity 排序后的序号（输入帧重排
        // 不改变排序结果 → 结果与输入顺序无关）；无 frame_ids 时回退输入序号。
        std::vector<std::pair<std::uint64_t, std::size_t>> order;
        if (in->frame_ids != nullptr) {
            for (std::size_t i = 0; i < vals.size(); ++i)
                order.push_back({in->frame_ids[idx[i]], i});
            std::stable_sort(order.begin(), order.end(),
                             [](const auto& a, const auto& b) {
                                 return a.first < b.first;
                             });
        }
        std::vector<bool> accept(vals.size(), true);
        int it = 0;
        for (; it < max_iter; ++it) {
            std::vector<double> xv, yv;
            if (in->frame_ids != nullptr) {
                for (std::size_t j = 0; j < order.size(); ++j) {
                    const std::size_t i = order[j].second;
                    if (accept[i]) {
                        xv.push_back((double)j);
                        yv.push_back(vals[i]);
                    }
                }
            } else {
                for (std::size_t i = 0; i < vals.size(); ++i)
                    if (accept[i]) {
                        xv.push_back((double)i);
                        yv.push_back(vals[i]);
                    }
            }
            if (yv.size() < 2) break;
            double a = 0.0, b = 0.0;
            ls_fit_line(xv, yv, &a, &b);
            std::vector<double> resid;
            auto x_of = [&](std::size_t i) -> double {
                if (in->frame_ids == nullptr) return (double)i;
                for (std::size_t j = 0; j < order.size(); ++j)
                    if (order[j].second == i) return (double)j;
                return (double)i;
            };
            for (std::size_t i = 0; i < vals.size(); ++i)
                if (accept[i]) resid.push_back(vals[i] - (a * x_of(i) + b));
            // 残差稳健尺度（MAD），避免单个离群拉高均方根 σ
            const double rmed = median(resid);
            std::vector<double> dev;
            dev.reserve(resid.size());
            for (double r : resid) dev.push_back(std::fabs(r - rmed));
            const double s = 1.4826 * median(std::move(dev));
            if (s <= 1e-12) break;
            bool changed = false;
            for (std::size_t i = 0; i < vals.size(); ++i) {
                if (!accept[i]) continue;
                const double r = vals[i] - (a * x_of(i) + b);
                const double z = r / s;
                if (z < lo || z > hi) {
                    accept[i] = false;
                    changed = true;
                }
            }
            if (!changed) break;
        }
        out->iterations = static_cast<std::uint32_t>(it);
        std::uint32_t kept = 0;
        for (std::size_t i = 0; i < vals.size(); ++i) {
            out->accepted[idx[i]] = accept[i] ? 1 : 0;
            if (accept[i]) { ++kept; }
            else if (vals[i] < 0.0) ++out->rejected_low; else ++out->rejected_high;
        }
        out->accepted_count = kept;
        out->status = kept == 0 ? 2 : 0;
        return 0;
    }

    if (in->method == P2_REJECT_RCR) {
        // Robust Chauvenet Rejection（Maples et al. 2018, arXiv:1807.05276，
        // 论文独立实现；UNC 官方非商业源码 ORACLE ONLY）。
        // P0-06 修复：核心 sequential robust→precise 流程：
        //   stage 1（鲁棒）：median + MAD，Chauvenet 判据迭代；
        //   stage 2（精确）：对 stage1 保留样本用 mean + std 再迭代。
        // weighted：权重按 √w 缩放残差进入尺度估计与 z。
        std::vector<bool> accept(vals.size(), true);
        const bool weighted = in->weights != nullptr;
        int removed_total = 0;
        auto chauvenet_stage = [&](bool use_precise, int max_stage_iter) {
            for (int it = 0; it < max_stage_iter; ++it) {
                std::vector<double> cur;
                std::vector<double> cur_w;
                for (std::size_t i = 0; i < vals.size(); ++i) {
                    if (!accept[i]) continue;
                    cur.push_back(vals[i]);
                    cur_w.push_back(weighted ? in->weights[i] : 1.0);
                }
                if (cur.size() < 3) break;
                double loc = 0.0, scale = 0.0;
                if (use_precise) {
                    // stage 2：加权 mean + 加权 std
                    double wsum = 0.0;
                    for (std::size_t i = 0; i < cur.size(); ++i) {
                        loc += cur_w[i] * cur[i];
                        wsum += cur_w[i];
                    }
                    loc /= wsum > 0 ? wsum : 1.0;
                    double vsum = 0.0;
                    for (std::size_t i = 0; i < cur.size(); ++i)
                        vsum += cur_w[i] * (cur[i] - loc) * (cur[i] - loc);
                    scale = std::sqrt(vsum / (wsum > 0 ? wsum : 1.0));
                } else {
                    const double med = median(cur);
                    std::vector<double> dev(cur.size());
                    for (std::size_t i = 0; i < cur.size(); ++i)
                        dev[i] = std::fabs(cur[i] - med) *
                                 std::sqrt(cur_w[i]);
                    loc = med;
                    scale = 1.4826 * median(std::move(dev));
                }
                if (scale <= 1e-12) break;
                std::size_t worst = 0;
                double max_z = 0.0;
                std::uint64_t worst_fid = ~0ULL;
                for (std::size_t i = 0; i < cur.size(); ++i) {
                    const double z = std::sqrt(cur_w[i]) *
                                     (cur[i] - loc) / scale;
                    std::uint64_t fid = (std::uint64_t)i;
                    if (in->frame_ids) {
                        std::size_t orig = vals.size();
                        for (std::size_t k = 0, j = 0; k < vals.size(); ++k) {
                            if (!accept[k]) continue;
                            if (j == i) { orig = k; break; }
                            ++j;
                        }
                        if (orig < vals.size())
                            fid = in->frame_ids[idx[orig]];
                    }
                    if (std::fabs(z) > max_z + 1e-15 ||
                        (std::fabs(std::fabs(z) - max_z) <= 1e-15 &&
                         fid < worst_fid)) {
                        max_z = std::fabs(z);
                        worst = i;
                        worst_fid = fid;
                    }
                }
                const double p = std::erfc(max_z / std::sqrt(2.0));
                if (p * (double)cur.size() >= 0.5) break;
                std::size_t orig = vals.size();
                for (std::size_t i = 0, j = 0; i < vals.size(); ++i) {
                    if (!accept[i]) continue;
                    if (j == worst) { orig = i; break; }
                    ++j;
                }
                if (orig >= vals.size()) break;
                accept[orig] = false;
                ++removed_total;
            }
        };
        chauvenet_stage(false, std::max(2, max_iter / 2 + 1));  // 鲁棒
        chauvenet_stage(true, std::max(2, max_iter / 2 + 1));   // 精确
        out->iterations = static_cast<std::uint32_t>(removed_total);
        std::uint32_t kept = 0;
        for (std::size_t i = 0; i < vals.size(); ++i) {
            out->accepted[idx[i]] = accept[i] ? 1 : 0;
            if (accept[i]) { ++kept; }
            else if (vals[i] < 0.0) ++out->rejected_low; else ++out->rejected_high;
        }
        out->accepted_count = kept;
        out->status = kept == 0 ? 2 : 0;
        return 0;
    }

    if (in->method == P2_REJECT_WINSORIZED_SIGMA) {
        // 真 Winsorized Sigma（P0-04 修复，AstroCS 精确定义）：
        //   每轮：普通 mean/std → 把当前样本 winsorize 到
        //   [mean+lo·std, mean+hi·std] → winsorized mean/std 估计 →
        //   用原始样本按 winsorized 估计做 sigma clip。
        // 与普通 Sigma（median/MAD + clip）是不同算法：winsorization
        // 抑制极端值对位置/尺度估计的影响。
        std::vector<bool> accept(vals.size(), true);
        int iters = 0;
        for (; iters < max_iter; ++iters) {
            std::vector<double> cur;
            for (std::size_t i = 0; i < vals.size(); ++i)
                if (accept[i]) cur.push_back(vals[i]);
            if (cur.size() < 2) break;
            double m0 = 0.0;
            for (double x : cur) m0 += x;
            m0 /= (double)cur.size();
            double s0 = 0.0;
            for (double x : cur) s0 += (x - m0) * (x - m0);
            s0 = std::sqrt(s0 / (double)(cur.size() - 1));
            if (s0 <= 1e-12) break;
            const double wlo = m0 + lo * s0;
            const double whi = m0 + hi * s0;
            std::vector<double> wcur = cur;
            for (double& x : wcur) x = std::clamp(x, wlo, whi);
            double wmean = 0.0;
            for (double x : wcur) wmean += x;
            wmean /= (double)wcur.size();
            double ws = 0.0;
            for (double x : wcur) ws += (x - wmean) * (x - wmean);
            ws = std::sqrt(ws / (double)(wcur.size() - 1));
            if (ws <= 1e-12) ws = s0;
            bool changed = false;
            std::vector<bool> next(vals.size(), true);
            for (std::size_t i = 0; i < vals.size(); ++i) {
                if (!accept[i]) { next[i] = false; continue; }
                const double z = (vals[i] - wmean) / ws;
                if (z < lo || z > hi) { next[i] = false; changed = true; }
            }
            accept = std::move(next);
            if (!changed) break;
        }
        out->iterations = static_cast<std::uint32_t>(iters);
        std::uint32_t kept = 0;
        for (std::size_t i = 0; i < vals.size(); ++i) {
            out->accepted[idx[i]] = accept[i] ? 1 : 0;
            if (accept[i]) {
                ++kept;
            } else {
                if (vals[i] < 0.0) ++out->rejected_low;
                else ++out->rejected_high;
            }
        }
        out->accepted_count = kept;
        if (out->accepted_count == 0) out->status = 2;
        return 0;
    }

    // Sigma（普通 median/MAD 迭代 clip）
    std::vector<bool> accept(vals.size(), true);
    int iters = 0;
    for (; iters < max_iter; ++iters) {
        std::vector<double> cur;
        for (std::size_t i = 0; i < vals.size(); ++i) {
            if (accept[i]) cur.push_back(vals[i]);
        }
        if (cur.size() < 2) break;
        double m = median(cur);
        double s = mad(cur, m);
        if (s <= 1e-12) {
            // 全同值：无离群
            break;
        }
        bool changed = false;
        std::vector<bool> next(vals.size(), true);
        for (std::size_t i = 0; i < vals.size(); ++i) {
            if (!accept[i]) { next[i] = false; continue; }
            const double z = (vals[i] - m) / s;
            if (z < lo || z > hi) {
                next[i] = false;
                changed = true;
            }
        }
        accept = std::move(next);
        if (!changed) break;
    }
    out->iterations = static_cast<std::uint32_t>(iters);

    std::uint32_t kept = 0;
    for (std::size_t i = 0; i < vals.size(); ++i) {
        out->accepted[idx[i]] = accept[i] ? 1 : 0;
        if (accept[i]) {
            ++kept;
        } else {
            if (vals[i] < 0.0) ++out->rejected_low; else ++out->rejected_high;
        }
    }
    out->accepted_count = kept;
    if (out->accepted_count == 0) out->status = 2;
    return 0;
}

} // extern "C"
