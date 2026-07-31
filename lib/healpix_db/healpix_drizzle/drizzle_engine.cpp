#include "drizzle_engine.h"
#include "healpix_core.h"
#include "spherical_overlap.h"   // WP-D: 球面 HEALPix 重叠计算
#include "aio_healpix_io.h"   // aio.dll C API: hiss_write (向后兼容宏)
// WP-E 步骤8: 接入新 HissWriter (替代旧 hiss_write)
#include "../../astro_image_io/include/hiss_format.h"
#include "../../astro_image_io/src/hiss_tile_model.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <string>
#include <vector>
#include <omp.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace drizzle {

// 度 → 弧度
static const double D2R = 0.017453292519943295769;

// ============================================================================
// 辅助函数: 大圆距离 (度)
// ============================================================================
static double greatCircleDistance(double ra1, double dec1, double ra2, double dec2) {
    double dRa = (ra2 - ra1) * D2R;
    double dec1r = dec1 * D2R;
    double dec2r = dec2 * D2R;
    double x = std::sin(dec1r) * std::sin(dec2r) +
               std::cos(dec1r) * std::cos(dec2r) * std::cos(dRa);
    x = std::max(-1.0, std::min(1.0, x));
    return std::acos(x) / D2R;
}

// ============================================================================
// 辅助函数: JSON 字符串转义
// ============================================================================
static std::string escapeJsonString(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\"': out += "\\\""; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

// ============================================================================
// 构造 / 析构
// ============================================================================
DrizzleEngine::DrizzleEngine() {}
DrizzleEngine::~DrizzleEngine() {}

// ============================================================================
// compute_auto_nside - 自动 NSIDE 计算 (02_FROZEN §5, WP-B 步骤5 修复)
//
// 依据最终 WCS/SIP 在有效视场内的局部 Jacobian:
//   1. 在图像范围内以至少 9×9 网格采样 (81 个采样点), 用有限差分计算局部像素
//      尺度 (角秒/像素). 9×9 网格覆盖整个有效视场, 避免 SIP 畸变极值仅在边缘
//      时被漏掉 (原 5 点采样仅中心+四角, 边缘畸变极值可能位于四角之间).
//   2. 取所有有效采样点局部像素尺度的最小值作为"最细局部输入像素尺度";
//   3. HEALPix 线性像素尺度 ≈ 210960/nside 角秒 (即 58.6/nside 度);
//   4. 选择最小的 2 次幂 NSIDE, 使 HEALPix 线性像素尺度不粗于该最细尺度
//      (即 210960/nside <= finest_arcsec, 结果约为 1~2 倍线性过采样);
//   5. NSIDE 钳位到 [16, 4194304] (2^4 到 2^22), 覆盖约 0.05" 到 3.7° 像素尺度,
//      支持 0.1"/px 输入的 1~2 倍过采样要求 (210960/0.1 ≈ 2.1e6, 需要 2^22).
//
// 有限差分法天然包含 SIP 多项式 + TAN 投影的非线性 Jacobian, 比 CD 矩阵
// 行列式法 (仅 CRVAL 附近的线性近似) 更准确, 符合"局部 Jacobian"语义.
// ============================================================================
int compute_auto_nside(const WcsParams& wcs, int img_w, int img_h)
{
    // NSIDE 钳位范围 (WP-B 步骤5 修复)
    // NSIDE_MIN = 16 (2^4): 像素尺度 ~3.66°, 覆盖大视场粗像素
    // NSIDE_MAX = 4194304 (2^22): 像素尺度 ~0.0503", 支持 0.1"/px 输入 1~2 倍过采样
    static const int NSIDE_MIN = 16;
    static const int NSIDE_MAX = 4194304;  // 2^22

    if (!wcs.has_wcs || img_w <= 0 || img_h <= 0) {
        fprintf(stderr, "[drizzle_engine] compute_auto_nside: WCS 无效或图像尺寸非法 "
                "(has_wcs=%d, w=%d, h=%d)\n", (int)wcs.has_wcs, img_w, img_h);
        return 0;
    }

    // 构造 WCS+SIP 转换器 (含 SIP 前向多项式 + TAN 投影 + CD 矩阵)
    WcsSip wcsip(wcs);
    if (!wcsip.hasWcs()) {
        fprintf(stderr, "[drizzle_engine] compute_auto_nside: WcsSip 初始化失败\n");
        return 0;
    }

    // 9×9 网格采样 (81 个采样点)
    // 采样点在图像范围内均匀分布, 0.5 像素内缩避免越界
    // 与原 5 点采样相比, 9×9 网格能捕捉到 SIP 畸变极值位于边缘中间位置的情况
    // (例如: 边缘中点的 SIP 畸变可能比四角更细)
    const int GRID_N = 9;
    const double margin = 0.5;  // 像素内缩, 避免边界越界

    // 有限差分半步长 (像素): dh=0.5 → 步长 1 像素, 平衡精度与效率
    const double dh = 0.5;

    double finest_arcsec = 1e30;  // 最细局部输入像素尺度 (角秒/像素)
    int n_valid_samples = 0;      // 有效采样点计数 (诊断用)
    int n_invalid_samples = 0;    // 无效采样点计数 (诊断用)

    for (int iy = 0; iy < GRID_N; iy++) {
        // y 坐标在 [margin, img_h - margin] 范围内均匀分布
        double y = (img_h > 1)
            ? margin + (double)iy * (img_h - 2.0 * margin) / (double)(GRID_N - 1)
            : (double)img_h * 0.5;
        for (int ix = 0; ix < GRID_N; ix++) {
            // x 坐标在 [margin, img_w - margin] 范围内均匀分布
            double x = (img_w > 1)
                ? margin + (double)ix * (img_w - 2.0 * margin) / (double)(GRID_N - 1)
                : (double)img_w * 0.5;

            // x 方向有限差分: pixelToSky(x-dh, y) 与 pixelToSky(x+dh, y) 的角距离
            double ra1, dec1, ra2, dec2;
            wcsip.pixelToSky(x - dh, y, ra1, dec1);
            wcsip.pixelToSky(x + dh, y, ra2, dec2);
            double scale_x = greatCircleDistance(ra1, dec1, ra2, dec2) * 3600.0;

            // y 方向有限差分
            wcsip.pixelToSky(x, y - dh, ra1, dec1);
            wcsip.pixelToSky(x, y + dh, ra2, dec2);
            double scale_y = greatCircleDistance(ra1, dec1, ra2, dec2) * 3600.0;

            // 跳过无效采样点 (投影发散或退化)
            if (!std::isfinite(scale_x) || scale_x <= 0.0 ||
                !std::isfinite(scale_y) || scale_y <= 0.0) {
                n_invalid_samples++;
                continue;
            }

            // 该点局部像素尺度 = 两方向最细 (最小), 单位归一化为 角秒/像素 (步长=1px)
            double local_scale = std::min(scale_x, scale_y);
            if (local_scale < finest_arcsec) {
                finest_arcsec = local_scale;
            }
            n_valid_samples++;
        }
    }

    fprintf(stderr, "[drizzle_engine] compute_auto_nside: 9x9 网格采样完成, "
            "有效=%d, 无效=%d\n", n_valid_samples, n_invalid_samples);

    if (finest_arcsec >= 1e30 || finest_arcsec <= 0.0) {
        fprintf(stderr, "[drizzle_engine] compute_auto_nside: 所有采样点局部尺度无效, 无法计算\n");
        return 0;
    }

    // HEALPix 线性像素尺度 ≈ 210960/nside 角秒 (58.6/nside 度)
    // 找最小 2 次幂 NSIDE 使 210960/nside <= finest_arcsec
    // 即 nside >= 210960/finest_arcsec
    double nside_min_real = 210960.0 / finest_arcsec;
    if (nside_min_real < 1.0) nside_min_real = 1.0;

    // 找最小 2 次幂 >= nside_min_real (从 1 开始左移)
    int nside = 1;
    while ((double)nside < nside_min_real) {
        int next = nside << 1;
        if (next <= nside) {  // 溢出保护 (达到 int 上限)
            nside = NSIDE_MAX;
            break;
        }
        nside = next;
    }

    // 钳位到 [NSIDE_MIN, NSIDE_MAX] = [16, 4194304] (2^4 到 2^22)
    if (nside < NSIDE_MIN) nside = NSIDE_MIN;
    if (nside > NSIDE_MAX) nside = NSIDE_MAX;

    // 过采样倍数 = hp_res / finest (<=1 表示 HEALPix 更细, 0.5~1 即 1~2 倍过采样)
    double hp_res_arcsec = 210960.0 / (double)nside;
    double oversample = hp_res_arcsec / finest_arcsec;

    fprintf(stderr, "[drizzle_engine] compute_auto_nside: finest=%.6f\"/px, "
            "nside_min=%.4f, nside=%d (hp_res=%.6f\"/px, %.4fx 线性过采样), "
            "钳位范围=[%d, %d]\n",
            finest_arcsec, nside_min_real, nside, hp_res_arcsec, oversample,
            NSIDE_MIN, NSIDE_MAX);

    return nside;
}

// ============================================================================
// drizzle - 执行 Drizzle: FITS 图像 → HEALPix 累加器
//
// WP-B 步骤6 修复: 入口校验
//   1. pixfrac <= 0.0 或 pixfrac > 1.0 → 返回错误 (拒绝, 不进入"点采样快速路径")
//   2. nested == false (RING 模式) → 返回错误 (HISS 内部统一 NESTED)
//   3. 移除所有"点采样快速路径"代码, 任何 pixfrac 非法值都被拒绝
// ============================================================================
bool DrizzleEngine::drizzle(const FitsImage& img, const DrizzleConfig& config,
                            const float* snrData, const float* weightData,
                            std::unordered_map<uint64_t, PixelAccumulator>& accumulators,
                            DrizzleStats& stats, std::string& error_msg)
{
    error_msg.clear();
    accumulators.clear();

    // ---- WP-B 步骤6: 入口参数校验 (pixfrac + RING) ----
    // pixfrac 必须在 (0, 1] 范围内 (02_FROZEN §9)
    // pixfrac <= 0: 旧代码进入"点采样快速路径", 现在直接拒绝
    // pixfrac > 1: 非法值, 拒绝
    if (config.pixfrac <= 0.0 || config.pixfrac > 1.0) {
        char buf[256];
        snprintf(buf, sizeof(buf), "pixfrac must be in (0, 1], got %.6f", config.pixfrac);
        error_msg = buf;
        fprintf(stderr, "[drizzle_engine] drizzle: 拒绝非法 pixfrac (%.6f), %s\n",
                config.pixfrac, error_msg.c_str());
        return false;
    }

    // HISS 内部统一 NESTED (02_FROZEN §6)
    // RING 模式不被支持, 直接拒绝
    if (!config.nested) {
        error_msg = "HISS requires NESTED ordering, RING not supported";
        fprintf(stderr, "[drizzle_engine] drizzle: 拒绝 RING 模式 (HISS 内部统一 NESTED)\n");
        return false;
    }

    // 1. 检查 WCS
    if (!img.wcs.has_wcs) {
        error_msg = "图像无 WCS 信息, 无法 drizzle";
        fprintf(stderr, "[drizzle_engine] %s\n", error_msg.c_str());
        return false;
    }
    if (img.width <= 0 || img.height <= 0) {
        error_msg = "图像尺寸非法";
        fprintf(stderr, "[drizzle_engine] %s\n", error_msg.c_str());
        return false;
    }

    // 2. 构造 WCS 转换器
    WcsSip wcs(img.wcs);
    if (!wcs.hasWcs()) {
        error_msg = "WcsSip 初始化失败";
        fprintf(stderr, "[drizzle_engine] %s\n", error_msg.c_str());
        return false;
    }

    // 3. 构造 HEALPix 核心
    healpix::HealpixCore hp(config.nside, config.nested);
    fprintf(stderr, "[drizzle_engine] HEALPix: nside=%d nested=%d npix=%lld res=%.2f\"\n",
            hp.getNside(), hp.isNested() ? 1 : 0,
            (long long)hp.getNpix(), hp.pixelResolutionArcsec());

    // B5 修复: Gaia 测光比例语义变更
    // PHOTOMETRIC 阶段 (pc_calibrate_simple) 已把 photscal 乘入像素值,
    // drizzle 不再重复应用 photscal (避免双重缩放)。
    // apply_photometry / photometry_applied_upstream 仅作为元数据标记,
    // 由 writeHis 写入 BUNIT/PHOTAPPL/PHOTSCAL。
    fprintf(stderr,
            "[drizzle_engine] Photometry status: apply_photometry=%d "
            "photometry_applied_upstream=%d photscal=%.6f "
            "(drizzle 不再应用 photscal, 由 PHOTOMETRIC 阶段上游处理)\n",
            (int)config.apply_photometry,
            (int)config.photometry_applied_upstream,
            config.photscal);

    // 4. 记录开始时间
    auto tStart = std::chrono::high_resolution_clock::now();

    // 5. OpenMP 并行 Drizzle
    // schedule(guided): 动态分配迭代块, 初始大块减少调度开销, 后期小块保证负载均衡
    // 比 schedule(dynamic, 64) 更适合 drizzle 这种处理时间不均匀的场景
    const int NUM_THREADS = 16;
    omp_set_num_threads(NUM_THREADS);
    std::vector<std::unordered_map<uint64_t, PixelAccumulator>> threadAccums(NUM_THREADS);
    // 预分配哈希表桶, 减少 rehash 开销 (假设每帧约 60M HEALPix 像素, 每线程约 4M)
    for (auto& acc : threadAccums) {
        acc.reserve(1 << 22);  // 4M 桶
    }

    int64_t nSourcePixels = 0;

    #pragma omp parallel for schedule(guided) reduction(+:nSourcePixels)
    for (int y = 0; y < img.height; y++) {
        int tid = omp_get_thread_num();
        auto& localAccum = threadAccums[tid];

        for (int x = 0; x < img.width; x++) {
            // 获取像素值 (单通道: 直接索引, RGB: 取第一通道)
            float pixelValue;
            if (img.channels == 1) {
                pixelValue = img.pixels[(size_t)y * img.width + x];
            } else {
                pixelValue = img.pixels[((size_t)y * img.width + x) * img.channels + 0];
            }

            // 跳过 NaN / Inf
            if (!std::isfinite(pixelValue)) continue;

            // B5 修复: photscal 已由 PHOTOMETRIC 阶段 (pc_calibrate_simple) 上游应用,
            // drizzle 不再重复应用 (避免双重缩放)。pixelValue 保持上游传入的相对通量值。

            // 获取 SNR
            float snrValue = 1.0f;
            if (snrData) {
                snrValue = snrData[(size_t)y * img.width + x];
                if (!std::isfinite(snrValue)) continue;
            }

            // 获取权重
            float weightValue = 1.0f;
            if (weightData) {
                weightValue = weightData[(size_t)y * img.width + x];
                if (!std::isfinite(weightValue) || weightValue <= 0.0f) continue;
            }

            nSourcePixels++;

            // 调用 6 步流水线
            processPixel((double)x, (double)y, pixelValue, snrValue, weightValue,
                         wcs, config, hp, localAccum);
        }
    }

    // 6. 合并所有线程的 localAccum 到全局 accumulators
    for (int t = 0; t < NUM_THREADS; t++) {
        for (auto& [ipix, acc] : threadAccums[t]) {
            auto& dst = accumulators[ipix];
            dst.sumFlux   += acc.sumFlux;
            dst.sumWeight += acc.sumWeight;
            dst.sumSnrSq  += acc.sumSnrSq;
            dst.sumArea   += acc.sumArea;    // support 面积累加 (02_FROZEN §10)
            dst.nContrib  += acc.nContrib;   // 贡献源像素计数
        }
    }

    // 7. 计算统计信息
    auto tEnd = std::chrono::high_resolution_clock::now();
    double elapsedSec = std::chrono::duration<double>(tEnd - tStart).count();

    stats.nHealpixPixels = (int64_t)accumulators.size();
    stats.nSourcePixels  = nSourcePixels;
    stats.nside          = config.nside;
    stats.nested         = config.nested;
    stats.elapsedSec     = elapsedSec;

    fprintf(stderr, "[drizzle_engine] 完成: %lld 源像素 → %lld HEALPix 像素, 耗时 %.3fs\n",
            (long long)nSourcePixels, (long long)accumulators.size(), elapsedSec);

    return true;
}

// ============================================================================
// processPixel - 处理单个像素的 Drizzle (球面几何流水线, WP-D 步骤3-4 修复)
//
// 修复内容 (02_FROZEN §8/§9/§10):
//   1. 源像素通过 WCS/SIP 映射到球面顶点 (radec_to_vec), 不再用切平面近似
//   2. 球面多边形面积 (Girard 定理), float64 内部精度
//   3. 候选像素查询基于球面包围圆 + queryDisc, 不限于 1-ring (可 > 48)
//   4. 球面 Sutherland-Hodgman 裁剪 + Girard 面积计算真实球面重叠
//   5. 通量守恒: sumFlux += L_j * (a_jp / A_j_drop), sumArea += a_jp (球面度)
//
// 6 步流水线:
//   Step 1: 取像素四角 (0-based)
//   Step 2: Pixfrac 收缩 (平面空间, 局部线性尺寸 × pixfrac)
//   Step 3: SIP+WCS 逐角映射 (像素→天球) → 球面向量
//   Step 4: 计算 drop 球面面积 (Girard 定理)
//   Step 5: 候选像素查询 (query_candidate_pixels, 不限于 1-ring)
//   Step 6: 对每个候选像素计算球面重叠面积, 累加通量
// ============================================================================
void DrizzleEngine::processPixel(
    double px, double py,
    float pixelValue,
    float snrValue,
    float weightValue,
    const WcsSip& wcs,
    const DrizzleConfig& config,
    const healpix::HealpixCore& hp,
    std::unordered_map<uint64_t, PixelAccumulator>& accum) const
{
    // ---- Step 1: 取像素四角 (0-based) ----
    double corners_xy[4][2] = {
        {px - 0.5, py - 0.5},  // 左下
        {px + 0.5, py - 0.5},  // 右下
        {px + 0.5, py + 0.5},  // 右上
        {px - 0.5, py + 0.5}   // 左上
    };

    // ---- Step 2: Pixfrac 收缩 (平面空间) ----
    // WP-B 步骤6: pixfrac <= 0 已在 drizzle() 入口拒绝, 这里 pixfrac 必然 > 0
    // pixfrac == 1.0: 不收缩, 四角保持原位
    // 0 < pixfrac < 1.0: 向中心收缩, 局部线性尺寸 × pixfrac, 面积 × pixfrac²
    // (02_FROZEN §9: 标准 drop 语义, 源像素总信号不变, drop 内信号面密度 × 1/pixfrac²)
    if (config.pixfrac < 1.0) {
        for (int i = 0; i < 4; i++) {
            corners_xy[i][0] = px + config.pixfrac * (corners_xy[i][0] - px);
            corners_xy[i][1] = py + config.pixfrac * (corners_xy[i][1] - py);
        }
    }

    // ---- Step 3: SIP+WCS 逐角映射 (像素→天球) → 球面单位向量 ----
    // WP-D 步骤3: 不再用切平面近似, 直接使用球面向量
    // drop_corners 是收缩后源像素的球面多边形顶点 (逆时针顺序, 单位向量)
    std::vector<spherical::Vec3> drop_corners(4);
    for (int i = 0; i < 4; i++) {
        double ra, dec;
        wcs.pixelToSky(corners_xy[i][0], corners_xy[i][1], ra, dec);
        if (!std::isfinite(ra) || !std::isfinite(dec)) return;
        drop_corners[i] = spherical::radec_to_vec(ra, dec);
    }

    // ---- Step 4: 计算 drop 球面面积 (Girard 定理) ----
    // A_j_drop = pixfrac 收缩后的 drop 球面面积 (球面度)
    // 用于标准 Drizzle 通量守恒: weight = a_jp / A_j_drop, Σweight = 1 → Σout = Σin
    double drop_area = spherical::spherical_polygon_area(drop_corners);
    if (drop_area < 1e-20) {
        // 几何退化: 源像素投影到球面后面积接近 0 (例如极区或投影背面)
        // 直接跳过该像素, 不破坏 support 几何意义
        return;
    }

    // ---- Step 5: 候选像素查询 (球面包围圆 + queryDisc, 不限于 1-ring) ----
    // WP-D 步骤4: 修复固定 1-ring 限制 (候选数 ≤ 48 → 可 > 48)
    // 高 NSIDE + 大源像素时, drop 跨越多个 HEALPix 像素, queryDisc 自动覆盖
    std::vector<uint64_t> candidates;
    spherical::query_candidate_pixels(drop_corners, hp, candidates);

    if (candidates.empty()) {
        return;
    }

    // ---- Step 6: 对每个候选像素计算球面重叠面积, 累加通量 ----
    // 通量守恒 (02_FROZEN §8): F_p = Σ_j L_j * (a_jp / A_j_drop)
    //   - a_jp = 球面重叠面积 (球面度)
    //   - A_j_drop = drop 球面面积 (球面度)
    //   - drop 未截断时, Σ_p a_jp = A_j_drop, 故 Σ_p F_p = L_j (通量守恒)
    // support 累加 (02_FROZEN §10): sumArea += a_jp (球面度)
    //   - support = Σ a_jp / A_p (A_p = HEALPix 像素面积, 由下游归一化)
    for (uint64_t ipix : candidates) {
        double overlap_area = spherical::compute_overlap_area(drop_corners, hp, ipix);
        if (overlap_area < 1e-20) continue;  // 不相交或面积过小

        // weight = a_jp / A_j_drop (标准 Drizzle 通量守恒权重)
        double weight = overlap_area / drop_area;
        if (weight <= 0.0) continue;

        // 通量累加 (float64 内部精度)
        // 02_FROZEN §8: signal = Σ L_j * (a_jp / A_j_drop), 不得乘质量权重
        // weightValue 仅作为有效性掩膜 (0=跳过, >0=参与), 不乘入 signal
        auto& acc = accum[ipix];
        acc.sumFlux   += (double)pixelValue * weight;
        acc.sumWeight += weight;
        acc.sumSnrSq  += (double)snrValue * snrValue * weight;
        // support: 累加球面重叠面积 a_jp (球面度), 由下游 HISS Writer 用 sumArea/A_p 归一化
        acc.sumArea   += overlap_area;
        acc.nContrib++;
    }
}

// ============================================================================
// getHealpixCorners - 获取 HEALPix 像素的四角球面坐标
// 菱形近似 (与 processPixel 一致): 北/东/南/西 4 个顶点
// ============================================================================
void DrizzleEngine::getHealpixCorners(const healpix::HealpixCore& hp, int64_t ipix,
                                      double ra0, double dec0,
                                      std::vector<SkyCoord>& corners) const
{
    // ra0, dec0 为切平面中心, 当前简化方案未使用 (预留)
    (void)ra0;
    (void)dec0;

    corners.clear();
    corners.resize(4);

    double ra_c, dec_c;
    hp.pix2radec(ipix, &ra_c, &dec_c);

    // HEALPix 像素分辨率 (度) = sqrt(area)
    // 赤道带 HEALPix 像素为菱形 (diamond), 不是方形
    //   - NS 对角线 d_ns = sqrt(sqrt(3)) * res ≈ 1.316 * res
    //   - EW 对角线 d_ew = 2/sqrt(sqrt(3)) * res ≈ 1.516 * res
    double res_deg = hp.pixelResolutionArcsec() / 3600.0;
    double cos_dec = std::cos(dec_c * D2R);

    static const double SQRT_SQRT3 = 1.3160740129524924;
    static const double D_NS_HALF_FACTOR = SQRT_SQRT3 / 2.0;       // ≈ 0.658
    static const double D_EW_HALF_FACTOR = 1.0 / SQRT_SQRT3;       // ≈ 0.760

    double half_dec = D_NS_HALF_FACTOR * res_deg;  // NS 对角线半长 (Dec 度)
    double half_ra;
    if (std::abs(cos_dec) < 1e-10) {
        half_ra = D_EW_HALF_FACTOR * res_deg;
    } else {
        half_ra = D_EW_HALF_FACTOR * res_deg / cos_dec;  // EW 对角线半长 (RA 度)
    }

    // 菱形 4 顶点 (北/西/南/东, 逆时针, 兼容 PolyClip::clipPolygon)
    corners[0] = {ra_c,           dec_c + half_dec};  // 北
    corners[1] = {ra_c - half_ra, dec_c            };  // 西
    corners[2] = {ra_c,           dec_c - half_dec};  // 南
    corners[3] = {ra_c + half_ra, dec_c            };  // 东
}

// ============================================================================
// writeHis - 将累加器按 Tile 分组并写入 .hiss 文件 (WP-E 步骤8)
//
// 改造要点 (02_FROZEN §8/§14/§16, 00_COMMON_CONTRACTS §4.4):
//   1. 不再调用旧 hiss_write()/hiss_write_snr_model(), 改为构造 HissWriter
//   2. 按 Tile 父像素分组累加器 (NESTED 位运算: parent = ipix >> 2d)
//   3. signal = 累计通量 (步骤7, finalize_signal 已在 hiss_common.cpp 修复)
//   4. support = 面积比 (pixel_area = A_p, 02_FROZEN §10)
//   5. 元数据不含完整 WCS/SIP (cd/crval/crpix/sip_order/sip 系数全部移除,
//      02_FROZEN §16: HISS 像素由 NSIDE/NESTED/ipix/ICRS 直接定位)
//   6. 旧 aio_hiss_write/read 改造成新 Writer/Reader 后端 (aio_healpix_io.cpp)
// ============================================================================
bool DrizzleEngine::writeHis(const std::unordered_map<uint64_t, PixelAccumulator>& accumulators,
                             const DrizzleStats& stats, const WcsParams& wcs,
                             const DrizzleConfig& config, const DrizzleMeta& meta,
                             const std::string& fitsPath,
                             const std::string& outputPath,
                             const HioSnrModel* snr_model,
                             std::string& error_msg)
{
    error_msg.clear();

    // B5 修复: 正式 Stage1 HISS 必须测光校准已应用
    // PHOTOMETRIC 阶段 (pc_calibrate_simple) 应已把 photscal 乘入像素值,
    // 或调用方显式设置 apply_photometry=true。两者均未设置时拒绝生成 HISS,
    // 避免输出未校准 ADU signal 违反 02_FROZEN §7 规范。
    if (!config.apply_photometry && !config.photometry_applied_upstream) {
        error_msg = "正式 Stage1 HISS 要求测光校准已应用 "
                    "(apply_photometry=false 且 photometry_applied_upstream=false), "
                    "拒绝生成未校准 ADU signal HISS";
        fprintf(stderr, "[drizzle_engine] writeHis: %s\n", error_msg.c_str());
        return false;
    }

    // 1. 计算 Tile 几何 (02_FROZEN §11)
    uint32_t nside = (uint32_t)config.nside;
    uint32_t depth = hiss::compute_tile_depth(nside);
    uint32_t tile_nside = hiss::compute_tile_nside(nside);
    uint32_t n_leaf_per_tile = 1u << (2 * depth);  // 4^depth
    int shift = 2 * (int)depth;

    // HEALPix 像素面积 A_p (球面度, 02_FROZEN §10): A_p = 4π / (12 * NSIDE²)
    double A_p = 4.0 * M_PI / (12.0 * (double)nside * (double)nside);

    fprintf(stderr,
            "[drizzle_engine] writeHis: nside=%u depth=%u tile_nside=%u n_leaf=%u A_p=%.6e\n",
            nside, depth, tile_nside, n_leaf_per_tile, A_p);

    // 2. 按 Tile 父像素分组累加器 (NESTED 位运算)
    //    parent_ipix = global_ipix >> (2*depth)
    //    local_ipix  = global_ipix & ((1 << (2*depth)) - 1)
    struct TileGroup {
        uint64_t parent_ipix = 0;
        std::vector<std::pair<uint32_t, const PixelAccumulator*>> pixels;
    };
    std::map<uint64_t, TileGroup> tile_groups;

    for (const auto& [ipix, acc] : accumulators) {
        // 有效像素: sumFlux != 0 或 sumArea > 0
        if (acc.sumFlux == 0.0 && acc.sumArea <= 0.0) continue;
        uint64_t parent = (shift > 0) ? (ipix >> shift) : ipix;
        uint32_t local  = (shift > 0) ? (uint32_t)(ipix & ((1ULL << shift) - 1)) : 0;
        tile_groups[parent].parent_ipix = parent;
        tile_groups[parent].pixels.push_back({local, &acc});
    }

    if (tile_groups.empty()) {
        error_msg = "无有效像素可写入";
        fprintf(stderr, "[drizzle_engine] %s\n", error_msg.c_str());
        return false;
    }

    fprintf(stderr, "[drizzle_engine] 写入 %zu 个 Tile 到 %s (nside=%u)\n",
            tile_groups.size(), outputPath.c_str(), nside);

    // 3. 构造 HissGridSpec (02_FROZEN §16: 不保存完整 WCS)
    hiss::HissGridSpec grid;
    grid.nside      = nside;
    grid.tile_nside = tile_nside;
    grid.ordering   = 1;  // NESTED
    grid.radesys    = 0;  // ICRS
    grid.pixfrac    = config.pixfrac;

    // 4. 构造 HissMetadata (精简, 不含完整 WCS/SIP, 02_FROZEN §16)
    //    移除: cd/crval/crpix/sip_order/sip 系数
    //    保留: NSIDE/ORDERING/RADESYS/TILENSID/PIXFRAC + FITS 常用字段 + 测光/校准字段
    hiss::HissMetadata hmeta;
    hmeta.nside      = nside;
    hmeta.tile_nside = tile_nside;
    hmeta.ordering   = 1;  // NESTED
    hmeta.radesys    = 0;  // ICRS
    hmeta.pixfrac    = config.pixfrac;
    // B5 修复: 测光已由 PHOTOMETRIC 阶段上游应用, drizzle 不再应用
    // 元数据标记: apply_photometry || photometry_applied_upstream → PHOTAPPL=1
    bool photometry_done = config.apply_photometry || config.photometry_applied_upstream;
    hmeta.photscal   = config.photscal;
    hmeta.photappl   = photometry_done ? 1 : 0;
    // BUNIT: 测光已应用 → ASTROCS_RELATIVE_FLUX (02_FROZEN §7)
    // 正式 Stage1 不允许输出未校准 ADU signal (验证已在函数入口完成)
    if (photometry_done) {
        std::snprintf(hmeta.bunit, sizeof(hmeta.bunit), "ASTROCS_RELATIVE_FLUX");
    } else {
        std::snprintf(hmeta.bunit, sizeof(hmeta.bunit), "ADU");
    }
    // 传统 FITS 字段 (按输入继承)
    std::snprintf(hmeta.filter, sizeof(hmeta.filter), "%s", meta.filter.c_str());
    hmeta.exptime = meta.exposure_s;
    std::snprintf(hmeta.date_obs, sizeof(hmeta.date_obs), "%s", meta.obs_time.c_str());
    // 从 fits_meta 提取常用字段
    auto get_meta = [&](const std::string& key) -> std::string {
        auto it = meta.fits_meta.find(key);
        return (it != meta.fits_meta.end()) ? it->second : std::string();
    };
    std::string obj = get_meta("OBJECT");
    std::snprintf(hmeta.object, sizeof(hmeta.object), "%s", obj.c_str());
    std::snprintf(hmeta.telescop, sizeof(hmeta.telescop), "%s", get_meta("TELESCOP").c_str());
    std::snprintf(hmeta.instrume, sizeof(hmeta.instrume), "%s", get_meta("INSTRUME").c_str());
    std::string gain_str = get_meta("GAIN");
    if (!gain_str.empty()) {
        try { hmeta.gain = std::stod(gain_str); } catch (...) {}
    }
    // 历史/诊断 (不含完整 WCS, 仅记录摘要)
    char hist[512];
    std::snprintf(hist, sizeof(hist),
                  "Stage1 drizzle: n_source=%lld n_healpix=%lld elapsed=%.3fs n_tiles=%zu "
                  "(WCS/SIP not stored in HISS per 02_FROZEN §16)",
                  (long long)stats.nSourcePixels, (long long)stats.nHealpixPixels,
                  stats.elapsedSec, tile_groups.size());
    hmeta.history = hist;

    // B7 修复: SNR 控制点按 Tile 分组
    // snr_model 含 ra/dec 控制点 (HioSnrControlPoint), 需转换为当前 NSIDE 的 NESTED ipix,
    // 再拆分为 (parent_ipix, local_ipix) 按 Tile 分组存储。
    // HISS SNR 子块格式 (02_FROZEN §17): 每点 local_ipix(uint32) + snr(float32), 8 字节
    std::map<uint64_t, std::vector<std::pair<uint32_t, float>>> tile_snr_points;
    if (snr_model && snr_model->n_points > 0) {
        // 构造 HEALPix 核心 (NESTED, 用于 radec2pix 转换)
        healpix::HealpixCore hp_snr((int)nside, true);
        fprintf(stderr, "[drizzle_engine] SNR 控制点分组: %u 点, nside=%u depth=%u shift=%d\n",
                snr_model->n_points, nside, depth, shift);

        uint32_t n_valid = 0, n_invalid = 0;
        for (uint32_t i = 0; i < snr_model->n_points; i++) {
            double ra  = snr_model->points[i].ra;
            double dec = snr_model->points[i].dec;
            float  snr_val = snr_model->points[i].snr_psf;

            // 跳过无效值 (NaN/Inf 或 ra/dec 越界)
            if (!std::isfinite(ra) || !std::isfinite(dec) || !std::isfinite(snr_val)) {
                n_invalid++;
                continue;
            }
            if (ra < 0.0 || ra >= 360.0 || dec < -90.0 || dec > 90.0) {
                n_invalid++;
                continue;
            }

            // ra/dec → NESTED ipix (当前 NSIDE)
            int64_t ipix = hp_snr.radec2pix(ra, dec);
            if (ipix < 0) {
                n_invalid++;
                continue;
            }

            // 拆分为 parent_ipix 和 local_ipix (NESTED 位运算)
            uint64_t global_ipix = (uint64_t)ipix;
            uint64_t parent = (shift > 0) ? (global_ipix >> shift) : global_ipix;
            uint32_t local  = (shift > 0) ? (uint32_t)(global_ipix & ((1ULL << shift) - 1)) : 0;

            tile_snr_points[parent].push_back({local, snr_val});
            n_valid++;
        }

        fprintf(stderr, "[drizzle_engine] SNR 控制点分组完成: %u 有效, %u 无效, %zu 个 Tile 含 SNR\n",
                n_valid, n_invalid, tile_snr_points.size());
    } else {
        fprintf(stderr, "[drizzle_engine] 无 snr_model 或控制点数为 0, 不写 SNR 子块\n");
    }

    // 5. 构造 HissWriter 并写入
    hiss::HissWriter writer;
    int wret = writer.open(outputPath, grid, hmeta);
    if (wret != 0) {
        error_msg = "HissWriter.open 失败 (rc=" + std::to_string(wret) + "): " + outputPath;
        fprintf(stderr, "[drizzle_engine] %s\n", error_msg.c_str());
        return false;
    }

    // 6. 逐 Tile 构造 DrizzleTileAccumulator 并写入
    //    signal = 累计通量 (步骤7), support = 面积比 (步骤10, A_p 归一化)
    for (const auto& [parent_ipix, tg] : tile_groups) {
        hiss::DrizzleTileAccumulator acc;
        acc.tile_nside  = tile_nside;
        acc.parent_ipix = parent_ipix;
        acc.pixel_area  = A_p;  // 02_FROZEN §10: support = sum_area / A_p
        acc.pixels.resize(n_leaf_per_tile);

        for (const auto& [local_ipix, pacc] : tg.pixels) {
            if (local_ipix < n_leaf_per_tile) {
                acc.pixels[local_ipix].sum_flux  = pacc->sumFlux;
                acc.pixels[local_ipix].sum_area  = pacc->sumArea;
                acc.pixels[local_ipix].n_contrib = pacc->nContrib;
            }
        }

        // B7 修复: 构造当前 Tile 的 SNR 控制点块
        // 从 tile_snr_points 查找当前 parent_ipix 的控制点, 构造 HissSnrBlock
        hiss::HissSnrBlock snr_block_local;
        const hiss::HissSnrBlock* snr_block = nullptr;
        auto snr_it = tile_snr_points.find(parent_ipix);
        if (snr_it != tile_snr_points.end() && !snr_it->second.empty()) {
            const auto& pts = snr_it->second;
            snr_block_local.points.resize(pts.size());
            for (size_t i = 0; i < pts.size(); i++) {
                snr_block_local.points[i].local_ipix = pts[i].first;
                snr_block_local.points[i].snr        = pts[i].second;
            }
            snr_block = &snr_block_local;
        }

        // occ_mode 由 Writer 自动选择 (步骤11), 传入 FULL 作为建议 (Writer 会忽略)
        int tret = writer.add_tile(parent_ipix, acc, snr_block, hiss::OccupancyMode::FULL);
        if (tret != 0) {
            error_msg = "HissWriter.add_tile 失败 (rc=" + std::to_string(tret) +
                        ") parent=" + std::to_string(parent_ipix);
            fprintf(stderr, "[drizzle_engine] %s\n", error_msg.c_str());
            writer.cancel();
            return false;
        }
    }

    // 7. finalize: 生成 Header + 原子替换
    int fret = writer.finalize();
    if (fret != 0) {
        error_msg = "HissWriter.finalize 失败 (rc=" + std::to_string(fret) + "): " + outputPath;
        fprintf(stderr, "[drizzle_engine] %s\n", error_msg.c_str());
        return false;
    }

    fprintf(stderr,
            "[drizzle_engine] 写入成功: %s (%zu Tile, signal=累计通量, 无完整 WCS, "
            "SNR 控制点=%zu Tile)\n",
            outputPath.c_str(), tile_groups.size(), tile_snr_points.size());
    return true;
}

} // namespace drizzle
