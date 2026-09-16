// hp_stack_hiss.cpp - 内存 sigma-clip 堆叠实现 (.hiss → .hcsd)
// 功能: 逐帧读入 .hiss (含 snr 通道), 按 ipix 对齐累加 SNR² 加权统计量,
//       sigma-clip 剔除离群值, 输出加权平均到 .hcsd
// 用途: 替代旧版 .ahps 落盘路径, 实现内存内 SNR² 加权堆叠
// 依赖: healpix_io.dll (hiss_read / hcsd_write / hio_free)
//
// 断层4 修复 (SNR² 加权 sigma-clip):
//   - 累加数组增加 weight_arr (SNR² 权重)
//   - sigma-clip 改为加权 mean + std (weighted_mean = sum_w/weight, weighted_std = sqrt(var))
//   - 输出改为加权平均 (stacked = sum_w / weight)
//   - 向后兼容: 旧 .hiss 文件无 snr 通道时, weight=1.0 退化为等权

#include "hp_stack_hiss.h"
#include "aio_healpix_io.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

// ============================================================================
// 错误码
// ============================================================================
static const int HPS_OK            = 0;
static const int HPS_ERR_PARAM     = -1;
static const int HPS_ERR_NSIDE     = -3;
static const int HPS_ERR_HIO       = -5;
static const int HPS_ERR_EMPTY     = -6;

// ============================================================================
// 简单 JSON 字段提取 (避免外部 JSON 库依赖)
// ============================================================================

// 从 JSON 字符串提取字符串字段 (如 "filter":"Lum")
static std::string extract_json_string(const std::string& json,
                                       const std::string& key) {
    std::string pat = "\"" + key + "\"";
    size_t pos = json.find(pat);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + pat.size());
    if (pos == std::string::npos) return "";
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos >= json.size() || json[pos] != '"') return "";
    pos++;  // 跳过开引号
    size_t start = pos;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) pos += 2;
        else pos++;
    }
    return json.substr(start, pos - start);
}

// 从 JSON 字符串提取数值字段
static double extract_json_number(const std::string& json,
                                  const std::string& key,
                                  double default_val) {
    std::string pat = "\"" + key + "\"";
    size_t pos = json.find(pat);
    if (pos == std::string::npos) return default_val;
    pos = json.find(':', pos + pat.size());
    if (pos == std::string::npos) return default_val;
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos >= json.size()) return default_val;
    return std::strtod(json.c_str() + pos, nullptr);
}

// 构建 .hcsd 输出 meta_json (不含 nside/nested/n_pix, 由 hcsd_write 自动添加)
static std::string build_output_meta(const std::string& filter, int n_frames,
                                     double total_exposure,
                                     double sigma, int max_iter,
                                     double mean_pixel_count,
                                     double median_exposure) {
    char buf[1024];
    std::snprintf(buf, sizeof(buf),
        "\"filter\":\"%s\",\"n_frames\":%d,\"total_exposure_s\":%.3f,"
        "\"sigma_clip\":{\"sigma\":%.4f,\"max_iter\":%d},"
        "\"stack_stats\":{\"mean_pixel_count\":%.4f,\"median_exposure\":%.3f}",
        filter.c_str(), n_frames, total_exposure,
        sigma, max_iter,
        mean_pixel_count, median_exposure);
    return std::string("{") + buf + "}";
}

// ============================================================================
// RAII 包装: 自动释放 hiss_read 分配的内存
// ============================================================================
struct HissData {
    uint32_t nside = 0;
    int      nested = 0;
    uint64_t n_pix = 0;
    uint64_t* ipix = nullptr;
    float*    pixel = nullptr;
    float*    snr = nullptr;   // SNR 通道 (旧文件为 nullptr)
    char*     meta = nullptr;

    ~HissData() {
        if (ipix)  hio_free(ipix);
        if (pixel) hio_free(pixel);
        if (snr)   hio_free(snr);
        if (meta)  hio_free(meta);
    }
};

// ============================================================================
// 主函数: hp_stack_hiss
// ============================================================================
HP_STACK_HISS_EXPORT int hp_stack_hiss(const char** hiss_paths, int n_frames,
                                       const char* output_hcsd_path,
                                       double sigma, int max_iter) {
    if (!hiss_paths || n_frames <= 0 || !output_hcsd_path) {
        fprintf(stderr, "[hp_stack_hiss] 无效参数 (paths=%p n_frames=%d out=%p)\n",
                (const void*)hiss_paths, n_frames, (const void*)output_hcsd_path);
        return HPS_ERR_PARAM;
    }
    if (sigma <= 0.0) sigma = 3.0;
    if (max_iter <= 0) max_iter = 5;

    fprintf(stderr, "[hp_stack_hiss] 开始堆叠: %d 帧 → %s (sigma=%.2f, max_iter=%d)\n",
            n_frames, output_hcsd_path, sigma, max_iter);

    // ---- 数据结构: ipix → index 映射 + SNR² 加权累加数组 ----
    // weight_arr:  Σ(SNR²)           — SNR² 权重累加 (旧文件无 snr 时为 Σ(1.0)=count)
    // sum_w_arr:   Σ(value × SNR²)   — 加权和
    // sum_wsq_arr: Σ(value² × SNR²)  — 加权平方和
    std::unordered_map<uint64_t, size_t> ipix_map;
    std::vector<double> weight_arr;    // Σ(SNR²)
    std::vector<double> sum_w_arr;     // Σ(value × SNR²)
    std::vector<double> sum_wsq_arr;   // Σ(value² × SNR²)
    std::vector<double> count_arr;     // 帧计数 (用于低样本数判断)

    uint32_t nside = 0;
    int nested = 0;
    std::string filter;
    double total_exposure = 0.0;
    std::vector<double> exposures;  // 用于计算 median

    // ---- 第一遍扫描: 建立 ipix → index 映射, 提取元数据 ----
    for (int f = 0; f < n_frames; f++) {
        if (!hiss_paths[f]) {
            fprintf(stderr, "[hp_stack_hiss] 帧 %d 路径为空, 跳过\n", f);
            continue;
        }

        HissData hd;
        int ret = hiss_read(hiss_paths[f], &hd.nside, &hd.nested, &hd.n_pix,
                            &hd.ipix, &hd.pixel, &hd.snr, &hd.meta);
        if (ret != 0) {
            fprintf(stderr, "[hp_stack_hiss] hiss_read 失败: %s (ret=%d)\n",
                    hiss_paths[f], ret);
            return HPS_ERR_HIO;
        }

        // 校验 nside/nested 一致性
        if (nside == 0) {
            nside = hd.nside;
            nested = hd.nested;
        } else if (hd.nside != nside || hd.nested != nested) {
            fprintf(stderr, "[hp_stack_hiss] nside/nested 不一致: 帧 %d (%u/%d) vs 首帧 (%u/%d)\n",
                    f, hd.nside, hd.nested, nside, nested);
            return HPS_ERR_NSIDE;
        }

        // 提取元数据 (filter 取首帧, exposure_s 求和)
        if (hd.meta) {
            std::string meta_str(hd.meta);
            if (filter.empty()) {
                filter = extract_json_string(meta_str, "filter");
            }
            double exp = extract_json_number(meta_str, "exposure_s", 0.0);
            total_exposure += exp;
            exposures.push_back(exp);
        }

        // 建立 ipix 映射
        for (uint64_t i = 0; i < hd.n_pix; i++) {
            uint64_t ip = hd.ipix[i];
            if (ipix_map.find(ip) == ipix_map.end()) {
                ipix_map[ip] = count_arr.size();
                count_arr.push_back(0.0);
                weight_arr.push_back(0.0);
                sum_w_arr.push_back(0.0);
                sum_wsq_arr.push_back(0.0);
            }
        }

        fprintf(stderr, "[hp_stack_hiss] 帧 %d: %s (nside=%u, n_pix=%llu, has_snr=%d)\n",
                f, hiss_paths[f], hd.nside, (unsigned long long)hd.n_pix,
                hd.snr ? 1 : 0);
    }

    if (nside == 0 || ipix_map.empty()) {
        fprintf(stderr, "[hp_stack_hiss] 无有效数据 (nside=%u, unique_pix=%zu)\n",
                nside, ipix_map.size());
        return HPS_ERR_EMPTY;
    }

    fprintf(stderr, "[hp_stack_hiss] 第一遍扫描完成: %zu 唯一像素, nside=%u\n",
            ipix_map.size(), nside);

    // ---- 第二遍扫描: 按 ipix 对齐累加 SNR² 加权统计量 ----
    for (int f = 0; f < n_frames; f++) {
        if (!hiss_paths[f]) continue;

        HissData hd;
        int ret = hiss_read(hiss_paths[f], &hd.nside, &hd.nested, &hd.n_pix,
                            &hd.ipix, &hd.pixel, &hd.snr, &hd.meta);
        if (ret != 0) {
            fprintf(stderr, "[hp_stack_hiss] 第二遍 hiss_read 失败: %s (ret=%d)\n",
                    hiss_paths[f], ret);
            return HPS_ERR_HIO;
        }

        for (uint64_t i = 0; i < hd.n_pix; i++) {
            auto it = ipix_map.find(hd.ipix[i]);
            if (it == ipix_map.end()) continue;  // 不应发生
            size_t idx = it->second;
            double v = (double)hd.pixel[i];
            // SNR² 权重: 有 snr 通道时用 snr², 无 snr 通道时用 1.0 (等权, 向后兼容)
            double w = hd.snr ? (double)hd.snr[i] * hd.snr[i] : 1.0;
            // 钳位 weight 到 [0, 1e6] 避免数值溢出 (spec §9.3)
            if (w > 1e6) w = 1e6;
            if (w < 0.0) w = 0.0;
            count_arr[idx]    += 1.0;
            weight_arr[idx]   += w;
            sum_w_arr[idx]    += v * w;
            sum_wsq_arr[idx]  += v * v * w;
        }
    }

    fprintf(stderr, "[hp_stack_hiss] 第二遍累加完成 (SNR² 加权)\n");

    // ---- sigma-clip 迭代 (最多 max_iter 次) ----
    size_t n_pix = count_arr.size();
    std::vector<double> mean_arr(n_pix), std_arr(n_pix);

    // rejected[f] 标记第 f 帧的每个像素是否已被剔除, 避免重复剔除
    // (不增加帧数相关内存: 只在 sigma-clip 阶段临时使用)
    std::vector<std::vector<bool>> rejected(n_frames);

    // 初始化 rejected 数组: 读取每帧的 n_pix 来分配空间
    for (int f = 0; f < n_frames; f++) {
        if (!hiss_paths[f]) {
            rejected[f].clear();
            continue;
        }
        HissData hd;
        int ret = hiss_read(hiss_paths[f], &hd.nside, &hd.nested, &hd.n_pix,
                            &hd.ipix, &hd.pixel, &hd.snr, &hd.meta);
        if (ret != 0) {
            fprintf(stderr, "[hp_stack_hiss] rejected 初始化 hiss_read 失败: %s (ret=%d)\n",
                    hiss_paths[f], ret);
            return HPS_ERR_HIO;
        }
        rejected[f].resize(hd.n_pix, false);
    }

    for (int iter = 0; iter < max_iter; iter++) {
        // 计算加权 mean 和 std
        // weighted_mean[i] = sum_w[i] / weight[i]
        // weighted_var[i]  = sum_wsq[i]/weight[i] - mean[i]^2
        // weighted_std[i]  = sqrt(max(var, 0))
        for (size_t i = 0; i < n_pix; i++) {
            if (weight_arr[i] > 0.0 && count_arr[i] > 0.0) {
                mean_arr[i] = sum_w_arr[i] / weight_arr[i];
                double var = sum_wsq_arr[i] / weight_arr[i] - mean_arr[i] * mean_arr[i];
                if (var < 0.0) var = 0.0;
                std_arr[i] = std::sqrt(var);
            } else {
                mean_arr[i] = 0.0;
                std_arr[i] = 0.0;
            }
        }

        // 重新遍历所有帧, 标记并剔除离群值 (跳过已剔除的值)
        int64_t total_rejected = 0;
        for (int f = 0; f < n_frames; f++) {
            if (!hiss_paths[f]) continue;

            HissData hd;
            int ret = hiss_read(hiss_paths[f], &hd.nside, &hd.nested, &hd.n_pix,
                                &hd.ipix, &hd.pixel, &hd.snr, &hd.meta);
            if (ret != 0) {
                fprintf(stderr, "[hp_stack_hiss] sigma-clip hiss_read 失败: %s (ret=%d)\n",
                        hiss_paths[f], ret);
                return HPS_ERR_HIO;
            }

            for (uint64_t i = 0; i < hd.n_pix; i++) {
                // 跳过已被剔除的值, 避免重复剔除
                if (i < rejected[f].size() && rejected[f][i]) continue;

                auto it = ipix_map.find(hd.ipix[i]);
                if (it == ipix_map.end()) continue;
                size_t idx = it->second;

                // 只在 count >= 2 且 std > 0 时才考虑剔除
                if (count_arr[idx] < 2.0 || std_arr[idx] <= 0.0) continue;

                double v = (double)hd.pixel[i];
                double dev = std::fabs(v - mean_arr[idx]);
                if (dev > sigma * std_arr[idx]) {
                    // 标记为离群值, 从累加中减去 (SNR² 加权)
                    double w = hd.snr ? (double)hd.snr[i] * hd.snr[i] : 1.0;
                    if (w > 1e6) w = 1e6;
                    if (w < 0.0) w = 0.0;
                    count_arr[idx]    -= 1.0;
                    weight_arr[idx]   -= w;
                    sum_w_arr[idx]    -= v * w;
                    sum_wsq_arr[idx]  -= v * v * w;
                    if (i < rejected[f].size()) rejected[f][i] = true;
                    total_rejected++;
                }
            }
        }

        fprintf(stderr, "[hp_stack_hiss] sigma-clip 迭代 %d: 剔除 %lld 个离群值 (SNR² 加权)\n",
                iter, (long long)total_rejected);

        if (total_rejected == 0) {
            fprintf(stderr, "[hp_stack_hiss] 迭代 %d 无剔除, 提前收敛\n", iter);
            break;
        }
    }

    // ---- 输出: 计算加权平均, 按 ipix 升序排序 ----
    std::vector<uint64_t> sorted_ipix;
    sorted_ipix.reserve(n_pix);
    for (const auto& kv : ipix_map) {
        sorted_ipix.push_back(kv.first);
    }
    std::sort(sorted_ipix.begin(), sorted_ipix.end());

    std::vector<uint64_t> out_ipix;
    std::vector<float>    out_pixel;
    out_ipix.reserve(sorted_ipix.size());
    out_pixel.reserve(sorted_ipix.size());

    double total_count = 0.0;
    for (uint64_t ip : sorted_ipix) {
        size_t idx = ipix_map[ip];
        // 加权平均: stacked = sum_w / weight
        if (weight_arr[idx] > 0.0 && count_arr[idx] > 0.0) {
            double weighted_mean = sum_w_arr[idx] / weight_arr[idx];
            out_ipix.push_back(ip);
            out_pixel.push_back((float)weighted_mean);
            total_count += count_arr[idx];
        }
    }

    double mean_pixel_count = out_ipix.empty() ? 0.0
                                                : total_count / (double)out_ipix.size();

    // 计算 median exposure
    double median_exposure = 0.0;
    if (!exposures.empty()) {
        std::sort(exposures.begin(), exposures.end());
        median_exposure = exposures[exposures.size() / 2];
    }

    fprintf(stderr, "[hp_stack_hiss] 输出 %zu 像素, mean_pixel_count=%.4f\n",
            out_ipix.size(), mean_pixel_count);

    // ---- 调用 hcsd_write 写入 .hcsd (含子叶块索引自动构建) ----
    std::string meta_json = build_output_meta(
        filter.empty() ? "stacked" : filter,
        n_frames, total_exposure, sigma, max_iter,
        mean_pixel_count, median_exposure);

    int ret = hcsd_write(output_hcsd_path, nside, nested,
                         (uint64_t)out_ipix.size(),
                         out_ipix.data(), out_pixel.data(),
                         meta_json.c_str());
    if (ret != 0) {
        fprintf(stderr, "[hp_stack_hiss] hcsd_write 失败: ret=%d\n", ret);
        return HPS_ERR_HIO;
    }

    fprintf(stderr, "[hp_stack_hiss] 完成: %s (%zu 像素, %d 帧)\n",
            output_hcsd_path, out_ipix.size(), n_frames);
    return HPS_OK;
}
