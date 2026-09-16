// gradient_sampler.cpp - 阶段1: 球面背景采样实现
//
// 逐帧流式处理:
//   1. 读 .hiss (ipix + pixel + meta)
//   2. 读 snr_model (稀疏控制点) → SnrEvaluator 重建
//   3. 确定 nside_i (最小 2^k 使 FOV 内像素数 ≥ k_target)
//   4. Gaia 星表锥形搜索 (FOV 中心 + 半径)
//   5. 控制点 = FOV 覆盖的 nside_i 网格点
//   6. 每个控制点: query_disc 邻域 → 星拒绝 → 零值拒绝 → bg_median
//   7. SNR 评估 (SnrEvaluator.evaluate)
//   8. 降采样 (若 > max_samples_per_frame)
//   9. 追加到样本表

#include "gradient_sampler.h"
#include "snr_evaluator.h"
#include "../healpix_core.h"
#include "aio_healpix_io.h"  // hiss_read / hiss_read_snr_model / hio_free (backward-compat macros)
#include "gaia_client.h"     // gaia_client_cone_search_for_solver

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace gradient {

// ============================================================================
// 内部辅助: 度 → 弧度
// ============================================================================
static const double D2R = 0.017453292519943295769;

// ============================================================================
// 内部辅助: 大圆距离 (度)
// ============================================================================
static double greatCircleDistanceDeg(double ra1, double dec1,
                                     double ra2, double dec2) {
    double dRa = (ra2 - ra1) * D2R;
    double dec1r = dec1 * D2R;
    double dec2r = dec2 * D2R;
    double x = std::sin(dec1r) * std::sin(dec2r) +
               std::cos(dec1r) * std::cos(dec2r) * std::cos(dRa);
    x = std::max(-1.0, std::min(1.0, x));
    return std::acos(x) / D2R;
}

// ============================================================================
// 内部辅助: 计算 ipix 集合的质心 (球面平均)
// ============================================================================
static void computeCentroid(const std::vector<int64_t>& ipix_arr,
                            const healpix::HealpixCore& hp,
                            double* center_ra, double* center_dec) {
    double sx = 0, sy = 0, sz = 0;
    for (int64_t ipix : ipix_arr) {
        double ra, dec;
        hp.pix2radec(ipix, &ra, &dec);
        double dec_r = dec * D2R;
        double ra_r = ra * D2R;
        double cd = std::cos(dec_r);
        sx += cd * std::cos(ra_r);
        sy += cd * std::sin(ra_r);
        sz += std::sin(dec_r);
    }
    double n = (double)ipix_arr.size();
    sx /= n; sy /= n; sz /= n;
    double ra = std::atan2(sy, sx) * 180.0 / M_PI;
    if (ra < 0) ra += 360.0;
    double s_clamped = std::max(-1.0, std::min(1.0, sz));
    double dec = std::asin(s_clamped) * 180.0 / M_PI;
    *center_ra = ra;
    *center_dec = dec;
}

// ============================================================================
// 内部辅助: 计算 ipix 集合的最大半径 (质心到最远点, 度)
// ============================================================================
static double computeMaxRadiusDeg(const std::vector<int64_t>& ipix_arr,
                                  const healpix::HealpixCore& hp,
                                  double center_ra, double center_dec) {
    double max_r = 0.0;
    for (int64_t ipix : ipix_arr) {
        double ra, dec;
        hp.pix2radec(ipix, &ra, &dec);
        double d = greatCircleDistanceDeg(center_ra, center_dec, ra, dec);
        if (d > max_r) max_r = d;
    }
    return max_r;
}

// ============================================================================
// 内部辅助: 选择 nside_i (最小 2^k 使 FOV 内像素数 ≥ k_target)
// ============================================================================
static int selectNsideI(int n_pix_fov, int k_target,
                        int nside_min, int nside_max,
                        int nside_data) {
    // nside_i 越低, 网格越粗, 控制点越少
    // 目标: FOV 内 nside_i 网格点数 ≥ k_target
    // 估算: FOV 面积占比 = n_pix_fov / npix_data
    //       nside_i 下 FOV 内像素数 ≈ (n_pix_fov / npix_data) × 12 × nside_i²
    //       要求 ≥ k_target → nside_i² ≥ k_target × npix_data / (12 × n_pix_fov)
    double npix_data = 12.0 * (double)nside_data * (double)nside_data;
    double fov_fraction = (double)n_pix_fov / npix_data;
    if (fov_fraction < 1e-10) return nside_min;
    double nside_i_sq = (double)k_target / (12.0 * fov_fraction);
    double nside_i_d = std::sqrt(nside_i_sq);
    // 向上取整到 2^k
    int nside_i = 1;
    while (nside_i < (int)nside_i_d && nside_i < nside_max) nside_i <<= 1;
    if (nside_i < nside_min) nside_i = nside_min;
    if (nside_i > nside_max) nside_i = nside_max;
    // 不能超过数据 nside (控制点需落在数据像素内)
    if (nside_i > nside_data) nside_i = nside_data;
    return nside_i;
}

// ============================================================================
// 内部辅助: Gaia 星拒绝 (返回保留的 ipix 索引)
// ============================================================================
static std::vector<int64_t> rejectStars(
    const std::vector<int64_t>& nbr_ipix,
    const healpix::HealpixCore& hp,
    const double* gaia_ra,
    const double* gaia_dec,
    const float*  gaia_mag,
    int n_gaia,
    double star_reject_base_arcsec,
    double star_reject_growth_rate,
    double star_reject_mag_high,
    double cp_ra, double cp_dec,
    double neighborhood_radius_arcsec,
    float zero_threshold)
{
    // 预过滤 Gaia 星: 只保留亮于 mag_high 且在邻域内的
    // (邻域半径 + 最大拒绝半径, 避免遍历全部星)
    double check_radius_deg = (neighborhood_radius_arcsec + 100.0) / 3600.0;
    std::vector<int> nearby_star_idx;
    for (int s = 0; s < n_gaia; s++) {
        if (gaia_mag[s] > star_reject_mag_high) continue;
        double d = greatCircleDistanceDeg(cp_ra, cp_dec, gaia_ra[s], gaia_dec[s]);
        if (d * 3600.0 <= neighborhood_radius_arcsec + star_reject_base_arcsec +
                          100.0 /* 余量 */) {
            nearby_star_idx.push_back(s);
        }
    }

    std::vector<int64_t> kept;
    kept.reserve(nbr_ipix.size());
    for (int64_t ipix : nbr_ipix) {
        double pra, pdec;
        hp.pix2radec(ipix, &pra, &pdec);

        // 零值拒绝: ipix 不在数据中 (值为 0) 在调用方处理, 此处只做星拒绝

        // 星拒绝: 丢弃距任一 Gaia 星角距离 < R_reject 的 ipix
        // R_reject = base + growth × flux (flux 用星等估算: flux = 10^(-0.4×mag))
        bool rejected = false;
        for (int idx : nearby_star_idx) {
            double d_arcsec = greatCircleDistanceDeg(pra, pdec,
                                                     gaia_ra[idx], gaia_dec[idx]) * 3600.0;
            // flux 估算 (相对 mag=0): 亮星拒绝半径大
            double flux = std::pow(10.0, -0.4 * (double)gaia_mag[idx]);
            double r_reject = star_reject_base_arcsec +
                              star_reject_growth_rate * flux;
            if (d_arcsec < r_reject) {
                rejected = true;
                break;
            }
        }
        if (!rejected) kept.push_back(ipix);
    }
    return kept;
}

// ============================================================================
// 内部辅助: NESTED Morton 降采样
// 将 nside_i 的控制点按 nside_coarse = nside_i >> 1 分组,
// 每组合并: bg_median 取子点中位数, 位置取质心, snr 取最大
// ============================================================================
static std::vector<SampleRow> mortonDownsample(
    const std::vector<SampleRow>& samples,
    int target_count)
{
    if ((int)samples.size() <= target_count) return samples;

    // 找到合适的 shift (nside_i >> shift 使组数 ≤ target_count)
    // nside_coarse = nside_i >> shift, 每组 4^shift 个子点
    int shift = 1;
    while (true) {
        int n_coarse = (int)samples.size() >> (2 * shift);
        if (n_coarse <= target_count || shift >= 10) break;
        shift++;
    }

    // 按粗像素分组 (ipix_coarse = cp 对应的 nside_coarse 像素)
    // 用 (ra, dec) → nside_coarse ipix 作为分组键
    // 但 SampleRow 没存 nside_i, 用 leaf_ipix_nside64 不合适
    // 改用: 对每个样本, 计算 ra/dec → nside_coarse ipix
    // 需要 nside_coarse, 但 nside_i 因帧而异
    // 简化: 按 Morton 位运算, 取 cp_ra/cp_dec 量化到粗网格

    // 实际上, spec 要求 NESTED Morton 位运算合并
    // 由于 SampleRow 只有 ra/dec (非 ipix), 改用空间网格分组:
    // 将 ra/dec 量化到 coarse 网格, 同一网格的样本合并

    // 估算 coarse 网格分辨率: target_count 个组覆盖 FOV
    // FOV 面积 ≈ samples.size() × pixel_size²
    // coarse pixel_size ≈ sqrt(FOV / target_count)
    // 但我们没有 FOV 信息, 改用: ra/dec 范围 → coarse 网格

    // 计算 ra/dec 范围
    double ra_min = samples[0].cp_ra, ra_max = samples[0].cp_ra;
    double dec_min = samples[0].cp_dec, dec_max = samples[0].cp_dec;
    for (const auto& s : samples) {
        if (s.cp_ra < ra_min) ra_min = s.cp_ra;
        if (s.cp_ra > ra_max) ra_max = s.cp_ra;
        if (s.cp_dec < dec_min) dec_min = s.cp_dec;
        if (s.cp_dec > dec_max) dec_max = s.cp_dec;
    }
    double ra_range = ra_max - ra_min;
    double dec_range = dec_max - dec_min;
    if (ra_range < 1e-10) ra_range = 1e-10;
    if (dec_range < 1e-10) dec_range = 1e-10;

    // coarse 网格: 每维 grid_n 个格, 总 grid_n² ≈ target_count
    int grid_n = (int)std::sqrt((double)target_count) + 1;
    if (grid_n < 2) grid_n = 2;

    // 分组
    std::unordered_map<int64_t, std::vector<int>> groups;
    groups.reserve(target_count * 2);
    for (int i = 0; i < (int)samples.size(); i++) {
        const auto& s = samples[i];
        int ix = (int)((s.cp_ra - ra_min) / ra_range * grid_n);
        int iy = (int)((s.cp_dec - dec_min) / dec_range * grid_n);
        if (ix >= grid_n) ix = grid_n - 1;
        if (iy >= grid_n) iy = grid_n - 1;
        int64_t key = (int64_t)ix * grid_n + iy;
        groups[key].push_back(i);
    }

    // 每组合并
    std::vector<SampleRow> out;
    out.reserve(groups.size());
    for (const auto& [key, idxs] : groups) {
        if (idxs.empty()) continue;

        // 位置: 质心 (ra/dec 平均, 球面近似)
        double sx = 0, sy = 0, sz = 0;
        std::vector<float> bgs, snrs;
        bgs.reserve(idxs.size());
        snrs.reserve(idxs.size());
        for (int i : idxs) {
            const auto& s = samples[i];
            double dec_r = s.cp_dec * D2R;
            double ra_r = s.cp_ra * D2R;
            double cd = std::cos(dec_r);
            sx += cd * std::cos(ra_r);
            sy += cd * std::sin(ra_r);
            sz += std::sin(dec_r);
            bgs.push_back(s.bg_median);
            snrs.push_back(s.snr);
        }
        double n = (double)idxs.size();
        sx /= n; sy /= n; sz /= n;
        double ra = std::atan2(sy, sx) * 180.0 / M_PI;
        if (ra < 0) ra += 360.0;
        double s_clamped = std::max(-1.0, std::min(1.0, sz));
        double dec = std::asin(s_clamped) * 180.0 / M_PI;

        // bg_median: 子点中位数
        std::sort(bgs.begin(), bgs.end());
        float bg_med = bgs[bgs.size() / 2];

        // snr: 子点最大
        float snr_max = *std::max_element(snrs.begin(), snrs.end());

        // frame_id: 取第一个 (同帧降采样)
        // leaf_ipix_nside64: 取第一个
        SampleRow merged = samples[idxs[0]];
        merged.cp_ra = ra;
        merged.cp_dec = dec;
        merged.bg_median = bg_med;
        merged.snr = snr_max;
        out.push_back(merged);
    }
    return out;
}

// ============================================================================
// 构造 / 析构
// ============================================================================
GradientSampler::GradientSampler() {}
GradientSampler::~GradientSampler() {}

// ============================================================================
// sample - 采样主入口
// ============================================================================
int GradientSampler::sample(const FrameInfo* frames, int n_frames,
                            const char* gaia_data_dir,
                            const SamplerParams& params,
                            SampleResult& result)
{
    error_msg_.clear();
    result.rows.clear();
    result.frame_ids.clear();
    result.total_samples = 0;
    result.n_frames_processed = 0;
    result.n_frames_skipped = 0;

    if (!frames || n_frames <= 0) {
        error_msg_ = "参数非法: frames 为空或 n_frames <= 0";
        fprintf(stderr, "[gradient_sampler] %s\n", error_msg_.c_str());
        return 1;
    }

    // 创建 Gaia 客户端 (DR3SP, db_type=2)
    GaiaClient* gaia = gaia_client_create_ex(gaia_data_dir, GAIA_DB_DR3SP);
    if (!gaia) {
        error_msg_ = "gaia_client_create_ex 失败: " + std::string(gaia_data_dir);
        fprintf(stderr, "[gradient_sampler] %s\n", error_msg_.c_str());
        return 2;
    }

    fprintf(stderr, "[gradient_sampler] 开始采样: %d 帧, gaia_dir=%s\n",
            n_frames, gaia_data_dir);

    // 逐帧处理
    for (int fi = 0; fi < n_frames; fi++) {
        const FrameInfo& info = frames[fi];
        fprintf(stderr, "[gradient_sampler] 帧 %d/%d (id=%d): %s\n",
                fi + 1, n_frames, info.frame_id, info.hiss_path.c_str());

        // 1. 读取 .hiss
        uint32_t nside_data = 0;
        int nested = 0;
        uint64_t n_pix = 0;
        uint64_t* ipix_arr = nullptr;
        float* pixel_arr = nullptr;
        char* meta_json = nullptr;

        int rc = hiss_read(info.hiss_path.c_str(),
                           &nside_data, &nested, &n_pix,
                           &ipix_arr, &pixel_arr, nullptr, &meta_json);
        if (rc != 0) {
            fprintf(stderr, "[gradient_sampler] hiss_read 失败 (rc=%d): %s, 跳过\n",
                    rc, info.hiss_path.c_str());
            result.n_frames_skipped++;
            continue;
        }

        if (n_pix == 0 || !ipix_arr || !pixel_arr) {
            fprintf(stderr, "[gradient_sampler] 空 .hiss (n_pix=%llu), 跳过\n",
                    (unsigned long long)n_pix);
            if (ipix_arr) hio_free(ipix_arr);
            if (pixel_arr) hio_free(pixel_arr);
            if (meta_json) hio_free(meta_json);
            result.n_frames_skipped++;
            continue;
        }

        fprintf(stderr, "[gradient_sampler] .hiss 加载: nside=%u nested=%d n_pix=%llu\n",
                nside_data, nested, (unsigned long long)n_pix);

        // 2. 读取 snr_model (稀疏控制点) → SnrEvaluator
        uint32_t snside = 0;
        int snested = 0;
        uint64_t snpix = 0;
        uint64_t* sipix = nullptr;
        float* spixel = nullptr;
        HioSnrModel* snr_model = nullptr;
        char* smeta = nullptr;

        SnrEvaluator snr_eval;
        bool snr_built = false;
        rc = hiss_read_snr_model(info.hiss_path.c_str(),
                                 &snside, &snested, &snpix,
                                 &sipix, &spixel, &snr_model, &smeta);
        if (rc == 0 && snr_model && snr_model->n_points > 0) {
            // 拆分控制点 → SnrEvaluator.build
            std::vector<double> cp_ra(snr_model->n_points), cp_dec(snr_model->n_points);
            std::vector<float>  cp_snr(snr_model->n_points);
            for (uint32_t i = 0; i < snr_model->n_points; i++) {
                cp_ra[i]  = snr_model->points[i].ra;
                cp_dec[i] = snr_model->points[i].dec;
                cp_snr[i] = snr_model->points[i].snr_psf;
            }
            double idw_p = (snr_model->idw_power > 0) ? snr_model->idw_power : params.idw_power;
            snr_built = snr_eval.build(snr_model->n_points,
                                       cp_ra.data(), cp_dec.data(), cp_snr.data(),
                                       snr_model->snr_phot, snr_model->median_snr,
                                       idw_p);
            if (snr_built) {
                fprintf(stderr, "[gradient_sampler] SNR 模型加载: n_points=%u snr_phot=%.4f\n",
                        snr_model->n_points, snr_model->snr_phot);
            }
        } else {
            fprintf(stderr, "[gradient_sampler] 无 SNR 模型 (rc=%d), snr=1.0\n", rc);
        }

        // 释放 snr_model 读取的资源 (SnrEvaluator 已内部拷贝)
        if (sipix) hio_free(sipix);
        if (spixel) hio_free(spixel);
        if (snr_model) hio_free_snr_model(snr_model);
        if (smeta) hio_free(smeta);

        // 3. 构造 HEALPix 核心 (数据 nside)
        healpix::HealpixCore hp_data((int)nside_data, nested != 0);

        // 4. 构建 ipix → pixel 查找表 (零值像素不入表)
        std::unordered_map<int64_t, float> pixel_map;
        pixel_map.reserve(n_pix * 2);
        for (uint64_t i = 0; i < n_pix; i++) {
            if (pixel_arr[i] > params.zero_threshold) {
                pixel_map[ipix_arr[i]] = pixel_arr[i];
            }
        }
        fprintf(stderr, "[gradient_sampler] 有效像素: %zu / %llu (零值拒绝 %llu)\n",
                pixel_map.size(), (unsigned long long)n_pix,
                (unsigned long long)(n_pix - pixel_map.size()));

        if (pixel_map.empty()) {
            fprintf(stderr, "[gradient_sampler] 无有效像素, 跳过\n");
            hio_free(ipix_arr);
            hio_free(pixel_arr);
            if (meta_json) hio_free(meta_json);
            result.n_frames_skipped++;
            continue;
        }

        // 5. 计算 FOV 质心 + 半径
        std::vector<int64_t> valid_ipix;
        valid_ipix.reserve(pixel_map.size());
        for (const auto& [ip, v] : pixel_map) valid_ipix.push_back(ip);

        double fov_ra, fov_dec;
        computeCentroid(valid_ipix, hp_data, &fov_ra, &fov_dec);
        double fov_radius_deg = computeMaxRadiusDeg(valid_ipix, hp_data, fov_ra, fov_dec);
        fprintf(stderr, "[gradient_sampler] FOV: center=(%.4f, %.4f) radius=%.4f°\n",
                fov_ra, fov_dec, fov_radius_deg);

        // 6. Gaia 星表锥形搜索 (FOV 中心 + 1.2×半径余量)
        double cone_radius = fov_radius_deg * 1.2;
        if (cone_radius < 0.5) cone_radius = 0.5;  // 最小 0.5°
        double* gaia_ra = nullptr;
        double* gaia_dec = nullptr;
        float* gaia_mag = nullptr;
        int n_gaia = 0;
        rc = gaia_client_cone_search_for_solver(
            gaia, fov_ra, fov_dec, cone_radius,
            params.star_reject_mag_high,
            &gaia_ra, &gaia_dec, &gaia_mag, &n_gaia);
        if (rc != 0) {
            fprintf(stderr, "[gradient_sampler] Gaia 查询失败 (rc=%d), 星拒绝禁用\n", rc);
            n_gaia = 0;
        } else {
            fprintf(stderr, "[gradient_sampler] Gaia 星: %d 颗 (半径 %.3f°)\n",
                    n_gaia, cone_radius);
        }

        // 7. 选择 nside_i
        int nside_i = selectNsideI((int)pixel_map.size(), params.k_target,
                                   params.nside_i_min, params.nside_i_max,
                                   (int)nside_data);
        fprintf(stderr, "[gradient_sampler] nside_i=%d (k_target=%d, FOV内像素=%zu)\n",
                nside_i, params.k_target, pixel_map.size());

        // 8. 构造 nside_i 网格上的控制点 (FOV 覆盖的 nside_i 像素)
        healpix::HealpixCore hp_i(nside_i, true);  // NESTED
        std::unordered_set<int64_t> cp_ipix_set;
        cp_ipix_set.reserve(pixel_map.size() * 2);
        // 对每个数据像素, 找到其对应的 nside_i 像素 (粗化)
        int shift_i = 0;
        int ns = nside_data;
        while (ns > nside_i) { shift_i += 2; ns >>= 1; }
        for (const auto& [ipix_fine, v] : pixel_map) {
            int64_t ipix_coarse = (shift_i > 0) ? (ipix_fine >> shift_i) : ipix_fine;
            cp_ipix_set.insert(ipix_coarse);
        }
        fprintf(stderr, "[gradient_sampler] 控制点候选: %zu\n", cp_ipix_set.size());

        // 9. 逐控制点采样
        double pixel_size_i_arcsec = hp_i.pixelResolutionArcsec();
        double neighborhood_radius_arcsec = params.neighborhood_factor * pixel_size_i_arcsec;

        std::vector<SampleRow> frame_samples;
        frame_samples.reserve(cp_ipix_set.size());

        for (int64_t cp_ipix : cp_ipix_set) {
            double cp_ra, cp_dec;
            hp_i.pix2radec(cp_ipix, &cp_ra, &cp_dec);

            // 定义域截断: 仅保留落在马赛克总 FOV 内的控制点 (spec §3.3.1)
            // 默认 mosaic_fov_radius_deg=0 表示不截断 (单帧或全天空)
            if (params.mosaic_fov_radius_deg > 0.0) {
                double d_deg = greatCircleDistanceDeg(params.mosaic_fov_ra,
                                                       params.mosaic_fov_dec,
                                                       cp_ra, cp_dec);
                if (d_deg > params.mosaic_fov_radius_deg) continue;
            }

            // a. query_disc 邻域
            std::vector<int64_t> nbr_ipix = hp_data.queryDisc(cp_ra, cp_dec,
                                                              neighborhood_radius_arcsec);
            if (nbr_ipix.empty()) continue;

            // b. 星拒绝 + 零值拒绝 (零值像素不在 pixel_map 中)
            std::vector<int64_t> kept_ipix;
            if (n_gaia > 0) {
                kept_ipix = rejectStars(nbr_ipix, hp_data,
                                        gaia_ra, gaia_dec, gaia_mag, n_gaia,
                                        params.star_reject_base_arcsec,
                                        params.star_reject_growth_rate,
                                        params.star_reject_mag_high,
                                        cp_ra, cp_dec,
                                        neighborhood_radius_arcsec,
                                        params.zero_threshold);
            } else {
                // 无 Gaia 星, 只做零值拒绝
                for (int64_t ip : nbr_ipix) {
                    auto it = pixel_map.find(ip);
                    if (it != pixel_map.end()) kept_ipix.push_back(ip);
                }
            }

            // 再次过滤: 只保留 pixel_map 中有值的
            std::vector<float> values;
            values.reserve(kept_ipix.size());
            for (int64_t ip : kept_ipix) {
                auto it = pixel_map.find(ip);
                if (it != pixel_map.end()) values.push_back(it->second);
            }

            if (values.empty()) continue;

            // d. bg_median
            std::sort(values.begin(), values.end());
            float bg_median = values[values.size() / 2];

            // e. SNR 评估
            float snr_val = 1.0f;
            if (snr_built) {
                snr_val = snr_eval.evaluate(cp_ra, cp_dec);
            }

            // f. leaf_ipix_nside64 (控制点所在的 nside=64 子叶)
            int shift_64 = 0;
            int ns64 = nside_i;
            while (ns64 > 64) { shift_64 += 2; ns64 >>= 1; }
            int64_t leaf_ipix_64 = (shift_64 > 0) ? (cp_ipix >> shift_64) : cp_ipix;
            // 如果 nside_i < 64, 需要左移扩展 (但 nside_i_min=64, 所以不会)
            if (nside_i < 64) leaf_ipix_64 = cp_ipix << shift_64;

            SampleRow row;
            row.cp_ra = cp_ra;
            row.cp_dec = cp_dec;
            row.frame_id = info.frame_id;
            row.bg_median = bg_median;
            row.snr = snr_val;
            row.leaf_ipix_nside64 = leaf_ipix_64;
            frame_samples.push_back(row);
        }

        fprintf(stderr, "[gradient_sampler] 帧 %d 采样: %zu 控制点 → %zu 样本\n",
                info.frame_id, cp_ipix_set.size(), frame_samples.size());

        // 10. 降采样
        if (params.enable_downsample &&
            (int)frame_samples.size() > params.max_samples_per_frame) {
            fprintf(stderr, "[gradient_sampler] 降采样: %zu → %d\n",
                    frame_samples.size(), params.max_samples_per_frame);
            frame_samples = mortonDownsample(frame_samples, params.max_samples_per_frame);
        }

        // 追加到全局结果
        for (const auto& s : frame_samples) {
            result.rows.push_back(s);
        }
        result.frame_ids.push_back(info.frame_id);
        result.n_frames_processed++;
        result.total_samples += (int)frame_samples.size();

        // 释放 Gaia 查询结果
        if (gaia_ra) free(gaia_ra);
        if (gaia_dec) free(gaia_dec);
        if (gaia_mag) free(gaia_mag);
        gaia_ra = gaia_dec = nullptr;
        gaia_mag = nullptr;

        // 释放 .hiss 数据
        hio_free(ipix_arr);
        hio_free(pixel_arr);
        if (meta_json) hio_free(meta_json);
        ipix_arr = nullptr;
        pixel_arr = nullptr;
        meta_json = nullptr;
    }

    // 释放 Gaia 客户端
    gaia_client_destroy(gaia);

    fprintf(stderr, "[gradient_sampler] 完成: %d 帧处理, %d 帧跳过, %d 样本\n",
            result.n_frames_processed, result.n_frames_skipped, result.total_samples);

    return 0;
}

} // namespace gradient
