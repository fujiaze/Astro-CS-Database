// cosmetic_corrector.cpp
// 坏点修复模块 - 天文CCD校准 (Astro Calibration)
//
// 功能：
// 在校准后的 Light 图像上检测坏点并修复。
// 检测方法：Dark全局统计检测热像素 + Bias全局统计检测冷像素。
// 连通区域大小过滤排除星点（保留 < max_structure_size 的结构）。
// 修复方法：5x5中值滤波 或 双线性插值。
//
// 设计要点：
// - 纯 C++17 标准库 + OpenMP，不依赖外部库
// - 核心算法不含文件 IO
// - Dark/Bias 主帧保留坏点（校准时扣除），仅用它们定位坏点位置
// - 连通区域标记：label 从 1 起分配，背景 label 0 在 sizes 统计与过滤两轮中均被
//   `labels[i] > 0` 跳过 (sizes[0] 恒为 0)，不会被误当作结构
// - median 用 std::nth_element（O(n)），MAD = median(|v - median|)

#include "../include/astro_calibration.h"

#include <vector>
#include <algorithm>
#include <cmath>
#include <queue>
#include <cstdio>
#ifdef _OPENMP
#include <omp.h>
#endif

namespace ac {

// ======================== 内部辅助 ========================

namespace {

float median_inplace(std::vector<float>& v) {
    int n = static_cast<int>(v.size());
    if (n <= 0) return 0.0f;
    int mid = n / 2;
    std::nth_element(v.begin(), v.begin() + mid, v.end());
    if (n % 2 == 1) return v[mid];
    float hi = v[mid];
    float lo = *std::max_element(v.begin(), v.begin() + mid);
    return (hi + lo) * 0.5f;
}

float compute_global_median(const float* data, int n) {
    std::vector<float> tmp(data, data + n);
    return median_inplace(tmp);
}

float compute_global_mad(const float* data, int n, float med) {
    std::vector<float> absdev(n);
    for (int i = 0; i < n; i++) absdev[i] = std::fabs(data[i] - med);
    return median_inplace(absdev);
}

} // namespace

// ======================== 连通区域过滤 ========================
// 8连通区域标记 (3x3 邻域) + 大小统计，只保留 size < max_size 的结构（排除星点）
// 关键：背景(label 0)在两轮扫描中都被 `labels[i] > 0` 跳过，故不会整片误清除
void filter_by_structure_size(char* mask, int w, int h, int max_size) {
    const int npix = w * h;
    if (npix <= 0) return;

    std::vector<int> labels(npix, 0);
    int num_features = 0;

    for (int seed = 0; seed < npix; seed++) {
        if (!mask[seed] || labels[seed] != 0) continue;

        int label = ++num_features;

        std::queue<int> q;
        q.push(seed);
        labels[seed] = label;

        while (!q.empty()) {
            int idx = q.front();
            q.pop();

            int x = idx % w;
            int y = idx / w;

            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                    int nidx = ny * w + nx;
                    if (mask[nidx] && labels[nidx] == 0) {
                        labels[nidx] = label;
                        q.push(nidx);
                    }
                }
            }
        }
    }

    if (num_features == 0) return;

    std::vector<int> sizes(num_features + 1, 0);
    for (int i = 0; i < npix; i++) {
        if (labels[i] > 0) sizes[labels[i]]++;
    }

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < npix; i++) {
        if (labels[i] > 0 && sizes[labels[i]] >= max_size) {
            mask[i] = 0;
        }
    }
}

// ======================== 热像素检测（从Dark） ========================
// 全局统计：median + threshold_sigma * 1.482602218505602 * MAD
// dark > threshold 的像素为候选，经结构过滤后返回
void detect_hot_pixels(const float* dark, int w, int h, char* hot_mask,
                       float threshold_sigma, int max_size) {
    int n = w * h;
    if (!dark || n <= 0) return;

    float med = compute_global_median(dark, n);
    float mad = compute_global_mad(dark, n, med);
    float sigma = 1.482602218505602f * mad;
    float threshold = med + threshold_sigma * sigma;

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++) {
        hot_mask[i] = (dark[i] > threshold) ? 1 : 0;
    }

    filter_by_structure_size(hot_mask, w, h, max_size);
}

// ======================== 冷像素检测（从Bias） ========================
// 全局统计：median - threshold_sigma * 1.482602218505602 * MAD
// bias < threshold 的像素为候选，经结构过滤后返回
void detect_cold_pixels(const float* bias, int w, int h, char* cold_mask,
                        float threshold_sigma, int max_size) {
    int n = w * h;
    if (!bias || n <= 0) return;

    float med = compute_global_median(bias, n);
    float mad = compute_global_mad(bias, n, med);
    float sigma = 1.482602218505602f * mad;
    float threshold = med - threshold_sigma * sigma;

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++) {
        cold_mask[i] = (bias[i] < threshold) ? 1 : 0;
    }

    filter_by_structure_size(cold_mask, w, h, max_size);
}

// ======================== 插值修复 ========================
// method=0 (median): 用5x5中值滤波结果替换坏像素
// method=1 (名义 bilinear, 实为 4 正交方向 1/dist 反比加权, 见 DISP-COS-003)
void interpolate_pixels(const float* data, const char* bad_mask, int w, int h,
                        float* out, int method) {
    int n = w * h;

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i++) {
        if (!bad_mask[i]) {
            out[i] = data[i];
            continue;
        }

        int x = i % w;
        int y = i / w;

        if (method == AC_METHOD_MEDIAN) {
            // 5x5中值
            std::vector<float> vals;
            vals.reserve(25);
            for (int dy = -2; dy <= 2; dy++) {
                for (int dx = -2; dx <= 2; dx++) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx < 0) nx = -nx;
                    if (ny < 0) ny = -ny;
                    if (nx >= w) nx = 2 * w - nx - 2;
                    if (ny >= h) ny = 2 * h - ny - 2;
                    if (nx < 0) nx = 0;
                    if (ny < 0) ny = 0;
                    if (nx >= w) nx = w - 1;
                    if (ny >= h) ny = h - 1;
                    int nidx = ny * w + nx;
                    if (!bad_mask[nidx]) {
                        vals.push_back(data[nidx]);
                    }
                }
            }
            if (vals.empty()) {
                out[i] = data[i];
            } else {
                out[i] = median_inplace(vals);
            }
        } else {
            // 双线性插值：用4个方向最近的好像素做距离反比加权
            float sum_wv = 0.0f, sum_w = 0.0f;
            int dirs[4][2] = {{-1,0},{1,0},{0,-1},{0,1}};
            for (int d = 0; d < 4; d++) {
                int dx = dirs[d][0], dy = dirs[d][1];
                int nx = x + dx, ny = y + dy;
                int dist = 1;
                while (nx >= 0 && nx < w && ny >= 0 && ny < h && bad_mask[ny*w+nx]) {
                    nx += dx; ny += dy; dist++;
                }
                if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                    float weight = 1.0f / static_cast<float>(dist);
                    sum_wv += data[ny * w + nx] * weight;
                    sum_w += weight;
                }
            }
            if (sum_w > 0.0f) {
                out[i] = sum_wv / sum_w;
            } else {
                out[i] = data[i];
            }
        }
    }
}

// ======================== 主入口：坏点修复 ========================
// 检测热像素(Dark) + 冷像素(Bias) -> 合并掩码 -> 插值修复
void correct_frame(const float* data, int w, int h,
                   const float* dark, const float* bias,
                   float* out,
                   float hot_sigma, float cold_sigma,
                   int method, int max_size,
                   int* out_hot, int* out_cold) {
    int n = w * h;
    if (!data || !out || n <= 0) return;

    std::vector<char> hot_mask(n, 0);
    std::vector<char> cold_mask(n, 0);

    // 从 Dark 检测热像素
    if (dark && hot_sigma > 0.0f) {
        detect_hot_pixels(dark, w, h, hot_mask.data(), hot_sigma, max_size);
    }

    // 从 Bias 检测冷像素
    if (bias && cold_sigma > 0.0f) {
        detect_cold_pixels(bias, w, h, cold_mask.data(), cold_sigma, max_size);
    }

    // 合并掩码
    std::vector<char> all_bad(n, 0);
    int n_hot = 0, n_cold = 0;
    for (int i = 0; i < n; i++) {
        if (hot_mask[i]) n_hot++;
        if (cold_mask[i]) n_cold++;
        all_bad[i] = hot_mask[i] || cold_mask[i];
    }

    // 插值修复
    interpolate_pixels(data, all_bad.data(), w, h, out, method);

    if (out_hot) *out_hot = n_hot;
    if (out_cold) *out_cold = n_cold;
}

// ======================== 坏列（linear defect, 单列）检测与修复 ========================
//
// 【独立路径】本节任何函数都不被 detect_hot_pixels / detect_cold_pixels /
// filter_by_structure_size / interpolate_pixels / correct_frame 调用，也不改变
// 它们的判据、阈值或语义（frozen 面零改动）；反之亦然。两套逻辑共存且互不
// 覆盖：坏点路径按**像素**判定，本节按**列**判定。
//
// 【方法依据】run/LINDEF-IMPL-01/REPORT.md §1（方法确认：项目+版本+文件:行）。
//   检测：整列缺陷在横向（x）表现为"该列稳健列统计量与其邻列不一致"，而天体
//     结构在横向连续 ⇒ 以**横向邻域中值**为基准、以**逐列中位数**为统计量，
//     两者都对列内少数污染像素（亮星、宇宙线、卫星线）稳健。
//     尺度取帧内 MAD(dev) 自校准 ⇒ 对检测源正标度变换严格不变（与 ALG-COS-001
//     §1 登记的检出不变性同性质），不引入绝对阈值常数。
//   修复：隔壁列插值。单列缺陷 = **左右两邻的算术平均**；连续坏列段 = 段两端
//     最近好列之间的**线性插值**（单列时退化为两邻平均）。全部插值取自**输入
//     data**（不级联）⇒ 结果有闭式解析预期。
//
// 【降级必须留痕】status 位标志显式登记每种降级，不静默。

#define AC_COLSTAT_OK                 0
#define AC_COLSTAT_SCALE_DEGENERATE   1   // MAD(dev)==0 ⇒ 检测不可用，不判任何列
#define AC_COLSTAT_FRAME_TOO_SMALL    2   // 列数不足 ⇒ 无列可判
#define AC_COLSTAT_EDGE_ONE_SIDED     4   // 坏列段贴边，仅单侧锚点 ⇒ 复制该锚列
#define AC_COLSTAT_NO_ANCHOR          8   // 坏列段无任何好列锚点 ⇒ 不修复，保留原值

// 横向邻域基准所需的最少邻居列数（少于该数 ⇒ 该列不参与判定）
#define AC_COLSTAT_MIN_NEIGHBORS 2

// ======================== 坏列检测 ========================
// data: 检测源帧 [h][w] float32（生产 = 校准后科学帧，或 dark/bias 母版）
// column_sigma: 判据阈值（帧内 MAD 倍数，<=0 = 显式禁用）
// neighbor_k: 锚点搜索半径（每侧列数；<=0 归一到 3）；段长上限 = 2k-1
//
// 【判据：列统计量的跳变配对分段】
// 整列缺陷在横向表现为 cs[x]=median_y data 的一个**电平段**：段内各列彼此
// 一致（因而互相"印证"、逐列比较看不出异常），只有**段的两条边界**是显著跳变。
// 因此判据建在**一阶差分** d[x] = cs[x]-cs[x-1] 上：
//   σ_d = 1.482602218505602·MAD(d)          （帧内自校准，对正标度变换不变）
//   J   = { x : |d[x]-median(d)| >= column_sigma·σ_d }   （显著跳变位置）
//   边界 B = {0} ∪ J ∪ {w}，把 [0,w-1] 切成若干电平段
//   段长 <= 2k-1 且非全宽  ⇒  判为坏列段
//
// 【为什么不是"逐列与邻域基准比较"】该形态有三个已实测的失败模式：
//   ① **负旁瓣**：紧邻基准被坏列污染 ⇒ 段两侧各出现约半幅反向假阳性
//      （实测单列 +68 ADU ⇒ 两邻各 −34 ADU）。
//   ② **基准曲率偏差**：邻域中值对强曲率有偏 ⇒ 横向结构尺度接近邻域半径时
//      整片列同向偏移被判坏（实测 64 列合成帧上 29 列假阳性）。
//   ③ **段内无信号**：段内各列互为邻居、彼此一致 ⇒ 逐列判据在段内恒为零，
//      贪心扩展只会从旁瓣出发越滚越大（实测 3 列段被扩成 6 列旁瓣）。
// 差分判据对三者同时免疫：缓变结构在 d 上是小量（被 MAD 吸收），段的边界
// 是局部尖峰；且**判据与修复算子同构**——段 [a,b] 的预测值正是用段外锚点
// L=a-1、R=b+1 的线性插值，与 repair_bad_columns 的处方逐式一致。
//
// 【边缘】a==0 或 b==w-1 时只有单侧锚点，跳变仍可判（边界 B 含 0/w），
// 但**修复**退化为复制该锚列并置 AC_COLSTAT_EDGE_ONE_SIDED（与 IRAF fixpix
// 贴边退化为单侧复制同向；见 run/LINDEF-IMPL-01/LIT-methods.md）。
void detect_bad_columns(const float* data, int w, int h,
                        float column_sigma, int neighbor_k, int max_seg_len,
                        unsigned char* col_mask,
                        int* out_n_cols, float* out_sigma_col, int* out_status) {
    int status = AC_COLSTAT_OK;
    if (out_n_cols) *out_n_cols = 0;
    if (out_sigma_col) *out_sigma_col = 0.0f;
    if (out_status) *out_status = status;
    if (!data || !col_mask || w <= 0 || h <= 0) return;
    for (int x = 0; x < w; x++) col_mask[x] = 0;
    if (column_sigma <= 0.0f) return;   // 显式禁用（不是降级）
    // 段长上限（负责人 2026-09-24 裁决：**只修单列**；相邻多列不属本任务）
    //   <=0 ⇒ 默认 1（只判单列缺陷）。超限的段**不修复**，仅以掩膜值 2 标记
    //   并置 AC_COLSTAT_WIDE_DEFECT，如实登记而不静默。
    const int maxlen = (max_seg_len > 0) ? max_seg_len : 1;
    if (w < 3) {
        status |= AC_COLSTAT_FRAME_TOO_SMALL;
        if (out_status) *out_status = status;
        return;
    }

    // 1) 逐列中位数（列间独立 ⇒ 并行；每线程一个长度 h 的缓冲）
    std::vector<float> colstat(static_cast<size_t>(w), 0.0f);
    #pragma omp parallel
    {
        std::vector<float> buf(static_cast<size_t>(h));
        #pragma omp for schedule(static)
        for (int x = 0; x < w; x++) {
            for (int y = 0; y < h; y++) buf[y] = data[static_cast<size_t>(y) * w + x];
            colstat[x] = median_inplace(buf);
        }
    }

    // 2) 一阶差分与帧内稳健尺度
    std::vector<float> d(static_cast<size_t>(w), 0.0f);
    std::vector<float> dv;
    dv.reserve(static_cast<size_t>(w));
    for (int x = 1; x < w; x++) {
        d[x] = colstat[x] - colstat[x - 1];
        dv.push_back(d[x]);
    }
    const float med_d = median_inplace(dv);
    for (size_t i = 0; i < dv.size(); i++) dv[i] = std::fabs(dv[i] - med_d);
    const float mad_d = median_inplace(dv);
    const float sigma_d = 1.482602218505602f * mad_d;
    if (out_sigma_col) *out_sigma_col = sigma_d;
    if (!(sigma_d > 0.0f)) {
        // 退化：列统计量的横向变化完全一致（典型：常数列帧、或整数样本使
        // 列中位数落格）⇒ "跳变"失去尺度基准。显式降级为"检测不可用"，
        // **不判任何列**（禁按 0 尺度判成全坏或全好）。真实锚：M42 原始整数
        // 帧上 6 帧有 3 帧 σ_d==0（见 REPORT §4）。
        status |= AC_COLSTAT_SCALE_DEGENERATE;
        if (out_status) *out_status = status;
        return;
    }

    // 3) 显著跳变位置（升序）
    std::vector<int> J;
    J.reserve(static_cast<size_t>(w));
    for (int x = 1; x < w; x++) {
        const float z = std::fabs(d[x] - med_d) / sigma_d;
        if (z >= column_sigma) J.push_back(x);
    }
    if (J.empty()) {   // 无跳变 ⇒ 无候选（含"整帧缓变结构"这一正确情形）
        if (out_status) *out_status = status;
        return;
    }

    // 4) 跳变**就近配对**成段，再判缺陷。
    //    两个独立缺陷之间夹着的正常区，会被"缺陷A的离开跳变"与"缺陷B的进入
    //    跳变"围成一个反号对；若按位置顺序配对就会把它误当宽凹陷（实测：
    //    合成帧上 col12 与 col25 各 −400 ⇒ 误判 [13,24] 为宽缺陷）。因此按
    //    **相邻距离最小**优先配对：单列缺陷的两条跳变相距 1，总先被配掉；
    //    宽缺陷段的两条跳变相距 = 段长 + 1。
    std::vector<char> paired(J.size(), 0);
    std::vector<std::pair<int, int> > segs;
    for (;;) {
        int best = -1, best_d = 0;
        for (size_t k = 0; k + 1 < J.size(); ++k) {
            if (paired[k] || paired[k + 1]) continue;
            if ((d[J[k]] > 0.0f) == (d[J[k + 1]] > 0.0f)) continue;   // 同号不成段
            const int dist = J[k + 1] - J[k];
            if (best < 0 || dist < best_d) { best_d = dist; best = static_cast<int>(k); }
        }
        if (best < 0) break;
        paired[best] = paired[best + 1] = 1;
        segs.push_back(std::make_pair(J[best], J[best + 1] - 1));
    }
    // 未配对的跳变：只有**贴边且够窄**时才构成缺陷段（电平在边界处改变且不再
    // 变回 ⇒ 边缘列与内部不一致）。否则不判——无法确定哪一侧才是缺陷。
    for (size_t k = 0; k < J.size(); ++k) {
        if (paired[k]) continue;
        const int i = J[k];
        const bool is_first = (k == 0);
        const bool is_last = (k + 1 == J.size());
        if (is_first && i <= maxlen) segs.push_back(std::make_pair(0, i - 1));
        if (is_last && (w - i) <= maxlen) segs.push_back(std::make_pair(i, w - 1));
    }
    if (segs.empty()) {
        if (out_status) *out_status = status;
        return;
    }

    // 5) 段分类：len <= maxlen ⇒ 掩膜值 1（可修，单列缺陷）；否则掩膜值 2
    //    （仅标记）+ AC_COLSTAT_WIDE_DEFECT —— 相邻多列不属本任务，如实登记。
    int n = 0;
    for (size_t t = 0; t < segs.size(); ++t) {
        int a = segs[t].first, b = segs[t].second;
        if (a < 0) a = 0;
        if (b > w - 1) b = w - 1;
        const int len = b - a + 1;
        if (len <= 0 || len >= w) continue;     // 全宽 ⇒ 无参照系
        if (len <= maxlen) {
            for (int x = a; x <= b; x++)
                if (col_mask[x] == AC_COLSTAT_MASK_CLEAN)
                    col_mask[x] = AC_COLSTAT_MASK_REPAIRED;
            n += len;
        } else {
            for (int x = a; x <= b; x++)
                if (col_mask[x] == AC_COLSTAT_MASK_CLEAN)
                    col_mask[x] = AC_COLSTAT_MASK_WIDE;
            status |= AC_COLSTAT_WIDE_DEFECT;
        }
    }
    if (out_n_cols) *out_n_cols = n;
    if (out_status) *out_status = status;
}

// ======================== 坏列修复（隔壁列插值） ========================
// 段 = 极大连通坏列区间 [a,b]。两侧锚点 L=a-1、R=b+1 均在 ⇒ 线性插值
//   out[y][x] = data[y][L] + (data[y][R]-data[y][L]) * (x-L)/(R-L)
// 单列段（a==b）时 (x-L)/(R-L) = 1/2 ⇒ out = (data[L]+data[R])/2，即
// **左右两邻的算术平均**。仅单侧锚点 ⇒ 复制该锚列 + 置 EDGE_ONE_SIDED；
// 无锚点 ⇒ 不修复、保留原值 + 置 NO_ANCHOR。全列内不做纵向平滑（保持解析可预期）。
void repair_bad_columns(const float* data, int w, int h,
                        const unsigned char* col_mask, float* out,
                        int* out_px_repaired, int* out_status) {
    int status = 0;
    if (out_px_repaired) *out_px_repaired = 0;
    if (!data || !out || !col_mask || w <= 0 || h <= 0) {
        if (out_status) *out_status = status;
        return;
    }

    // 只处理"已判定可修"的列（掩膜值 1）。宽缺陷段（值 2）与干净列（值 0）
    // 一律逐位拷贝 —— 不修复、不改值，但仍写入 out 保证 out 是完整帧。
    long long px = 0;
    for (int x = 0; x < w; ) {
        if (col_mask[x] != AC_COLSTAT_MASK_REPAIRED) { x++; continue; }
        const int a = x;
        while (x < w && col_mask[x] == AC_COLSTAT_MASK_REPAIRED) x++;
        const int b = x - 1;
        const bool has_l = (a - 1 >= 0) && col_mask[a - 1] != AC_COLSTAT_MASK_REPAIRED;
        const bool has_r = (b + 1 <= w - 1) && col_mask[b + 1] != AC_COLSTAT_MASK_REPAIRED;
        if (!has_l && !has_r) status |= AC_COLSTAT_NO_ANCHOR;
        else if (!has_l || !has_r) status |= AC_COLSTAT_EDGE_ONE_SIDED;
        px += static_cast<long long>(b - a + 1) * h;
    }

    #pragma omp parallel for schedule(static)
    for (int y = 0; y < h; y++) {
        const size_t row = static_cast<size_t>(y) * w;
        int x = 0;
        while (x < w) {
            if (col_mask[x] != AC_COLSTAT_MASK_REPAIRED) { out[row + x] = data[row + x]; x++; continue; }
            const int a = x;
            while (x < w && col_mask[x] == AC_COLSTAT_MASK_REPAIRED) x++;
            const int b = x - 1;
            const int L = ((a - 1 >= 0) && col_mask[a - 1] != AC_COLSTAT_MASK_REPAIRED) ? a - 1 : -1;
            const int R = ((b + 1 <= w - 1) && col_mask[b + 1] != AC_COLSTAT_MASK_REPAIRED) ? b + 1 : -1;
            if (L >= 0 && R >= 0) {
                const float vl = data[row + L];
                const float vr = data[row + R];
                const float inv = 1.0f / static_cast<float>(R - L);
                for (int xx = a; xx <= b; xx++)
                    out[row + xx] = vl + (vr - vl) * (static_cast<float>(xx - L) * inv);
            } else if (L >= 0) {
                for (int xx = a; xx <= b; xx++) out[row + xx] = data[row + L];
            } else if (R >= 0) {
                for (int xx = a; xx <= b; xx++) out[row + xx] = data[row + R];
            } else {
                for (int xx = a; xx <= b; xx++) out[row + xx] = data[row + xx];
            }
        }
    }

    if (out_px_repaired) *out_px_repaired = static_cast<int>(px);
    if (out_status) *out_status = status;
}

// ======================== 坏列主入口 ========================
void correct_columns(const float* data, int w, int h, float* out,
                     float column_sigma, int neighbor_k, int max_seg_len,
                     unsigned char* col_mask,
                     int* out_n_cols, int* out_px_repaired,
                     float* out_sigma_col, int* out_status) {
    int st_det = 0, st_rep = 0;
    if (!data || !out || w <= 0 || h <= 0) {
        if (out_n_cols) *out_n_cols = 0;
        if (out_px_repaired) *out_px_repaired = 0;
        if (out_sigma_col) *out_sigma_col = 0.0f;
        if (out_status) *out_status = AC_COLSTAT_FRAME_TOO_SMALL;
        return;
    }
    std::vector<unsigned char> mask(static_cast<size_t>(w), 0);
    detect_bad_columns(data, w, h, column_sigma, neighbor_k, max_seg_len,
                       mask.data(), out_n_cols, out_sigma_col, &st_det);
    repair_bad_columns(data, w, h, mask.data(), out, out_px_repaired, &st_rep);
    if (col_mask) for (int x = 0; x < w; x++) col_mask[x] = mask[x];
    if (out_status) *out_status = st_det | st_rep;
}

// ======================== 三路径（science / dark / bias）仲裁 + 修复 ========================
// 整列缺陷的物理来源是探测器；dark/bias 母版是该缺陷最直接的观测面。三条路径
// 用**同一判据**（detect_bad_columns，跳变配对分段），各自帧内自校准 ⇒ 各路径
// 的检出集合对自身标度严格不变，故母版与科学帧标度不一致也不影响判坏集合。
//
// 仲裁 = **并集**（不是交集）：生产两条调用路径都不接线母版（DISP-COS-009），
// 科学帧路径必须能独立工作；交集会漏掉只在一处可见的缺陷。每一路的证据都
// 逐列写进 source_mask，不静默丢弃。置信度由来源组合导出：
//   dark 与 bias 一致判坏                      => HIGH（物理来源双重印证）
//   仅一个物理来源判坏 / 科学帧 + 任一母版      => MEDIUM
//   仅科学帧自身判坏                            => LOW
void correct_columns_ex(const float* data, const float* dark, const float* bias,
                        int w, int h, float* out,
                        float column_sigma, int neighbor_k, int max_seg_len,
                        unsigned char* col_mask,
                        unsigned char* source_mask, unsigned char* conf_mask,
                        int* out_n_cols, int* out_n_sci,
                        int* out_n_dark, int* out_n_bias,
                        int* out_px_repaired, float* out_sigma_col,
                        int* out_status) {
    int st_det = 0, st_rep = 0;
    if (out_n_cols) *out_n_cols = 0;
    if (out_n_sci) *out_n_sci = 0;
    if (out_n_dark) *out_n_dark = 0;
    if (out_n_bias) *out_n_bias = 0;
    if (out_px_repaired) *out_px_repaired = 0;
    if (out_sigma_col) *out_sigma_col = 0.0f;
    if (out_status) *out_status = AC_COLSTAT_FRAME_TOO_SMALL;
    if (!data || !out || w <= 0 || h <= 0) return;

    std::vector<unsigned char> m_sci(static_cast<size_t>(w), 0);
    std::vector<unsigned char> m_dark(static_cast<size_t>(w), 0);
    std::vector<unsigned char> m_bias(static_cast<size_t>(w), 0);
    int n_sci = 0, n_dark = 0, n_bias = 0;
    float sig_sci = 0.0f, sig_dark = 0.0f, sig_bias = 0.0f;

    detect_bad_columns(data, w, h, column_sigma, neighbor_k, max_seg_len,
                       m_sci.data(), &n_sci, &sig_sci, &st_det);
    if (dark) {
        int st2 = 0;
        detect_bad_columns(dark, w, h, column_sigma, neighbor_k, max_seg_len,
                           m_dark.data(), &n_dark, &sig_dark, &st2);
        // 母版路径的"帧过小"不改变科学帧路径的适用域判定；尺度退化只记在 status
        if (st2 & AC_COLSTAT_FRAME_TOO_SMALL) st_det |= AC_COLSTAT_FRAME_TOO_SMALL;
    }
    if (bias) {
        int st3 = 0;
        detect_bad_columns(bias, w, h, column_sigma, neighbor_k, max_seg_len,
                           m_bias.data(), &n_bias, &sig_bias, &st3);
        if (st3 & AC_COLSTAT_FRAME_TOO_SMALL) st_det |= AC_COLSTAT_FRAME_TOO_SMALL;
    }

    std::vector<unsigned char> merged(static_cast<size_t>(w), 0);
    int n_merged = 0;
    for (int x = 0; x < w; x++) {
        unsigned src = 0;
        if (m_sci[x]) src |= AC_COLSTAT_SRC_SCIENCE;
        if (m_dark[x]) src |= AC_COLSTAT_SRC_DARK;
        if (m_bias[x]) src |= AC_COLSTAT_SRC_BIAS;
        if (source_mask) source_mask[x] = static_cast<unsigned char>(src);
        if (conf_mask) {
            unsigned conf = 0;
            const bool has_d = (src & AC_COLSTAT_SRC_DARK) != 0;
            const bool has_b = (src & AC_COLSTAT_SRC_BIAS) != 0;
            const bool has_s = (src & AC_COLSTAT_SRC_SCIENCE) != 0;
            if (has_d && has_b) conf = AC_COLSTAT_CONF_HIGH;
            else if (has_d || has_b) conf = AC_COLSTAT_CONF_MEDIUM;
            else if (has_s) conf = AC_COLSTAT_CONF_LOW;
            conf_mask[x] = static_cast<unsigned char>(conf);
        }
        // 只有"可修"柱并入 merged；宽缺陷（值 2）保持仅标记，不参与修复
        if (m_sci[x] == AC_COLSTAT_MASK_REPAIRED || m_dark[x] == AC_COLSTAT_MASK_REPAIRED ||
            m_bias[x] == AC_COLSTAT_MASK_REPAIRED) { merged[x] = AC_COLSTAT_MASK_REPAIRED; n_merged++; }
        else if (src) { merged[x] = AC_COLSTAT_MASK_WIDE; }
        if (m_sci[x] == AC_COLSTAT_MASK_WIDE || m_dark[x] == AC_COLSTAT_MASK_WIDE ||
            m_bias[x] == AC_COLSTAT_MASK_WIDE) st_det |= AC_COLSTAT_WIDE_DEFECT;
    }

    repair_bad_columns(data, w, h, merged.data(), out, out_px_repaired, &st_rep);
    if (col_mask) for (int x = 0; x < w; x++) col_mask[x] = merged[x];
    if (out_n_cols) *out_n_cols = n_merged;
    if (out_n_sci) *out_n_sci = n_sci;
    if (out_n_dark) *out_n_dark = n_dark;
    if (out_n_bias) *out_n_bias = n_bias;
    if (out_sigma_col) *out_sigma_col = sig_sci;
    if (out_status) *out_status = st_det | st_rep;
}


} // namespace ac
