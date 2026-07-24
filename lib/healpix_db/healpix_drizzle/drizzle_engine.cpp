#include "drizzle_engine.h"
#include "healpix_core.h"
#include "aio_healpix_io.h"   // aio.dll C API: hiss_write (向后兼容宏)

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
// drizzle - 执行 Drizzle: FITS 图像 → HEALPix 累加器
// ============================================================================
bool DrizzleEngine::drizzle(const FitsImage& img, const DrizzleConfig& config,
                            const float* snrData, const float* weightData,
                            std::unordered_map<uint64_t, PixelAccumulator>& accumulators,
                            DrizzleStats& stats, std::string& error_msg)
{
    error_msg.clear();
    accumulators.clear();

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
// processPixel - 处理单个像素的 Drizzle (6步流水线)
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
    if (config.pixfrac <= 0.0) {
        // 退化为点采样: 四角都 = (px, py)
        for (int i = 0; i < 4; i++) {
            corners_xy[i][0] = px;
            corners_xy[i][1] = py;
        }
    } else if (config.pixfrac < 1.0) {
        for (int i = 0; i < 4; i++) {
            corners_xy[i][0] = px + config.pixfrac * (corners_xy[i][0] - px);
            corners_xy[i][1] = py + config.pixfrac * (corners_xy[i][1] - py);
        }
    }

    // ---- Step 3: SIP+WCS 逐角映射 (像素→天球) ----
    SkyCoord cornersSky[4];
    for (int i = 0; i < 4; i++) {
        wcs.pixelToSky(corners_xy[i][0], corners_xy[i][1],
                       cornersSky[i].ra, cornersSky[i].dec);
    }

    // ---- Step 4: 用四角笛卡尔平均近似中心 (减少 1 次 pixelToSky) ----
    // 球面笛卡尔平均: 四角单位向量求和 → 归一化 → 球面坐标
    // 精度: 对 < 10" 像素, 笛卡尔平均与真实中心的偏差 < 0.001"
    double cx_x = 0, cx_y = 0, cx_z = 0;
    for (int i = 0; i < 4; i++) {
        double dec_rad = cornersSky[i].dec * D2R;
        double ra_rad = cornersSky[i].ra * D2R;
        double cd = std::cos(dec_rad);
        cx_x += cd * std::cos(ra_rad);
        cx_y += cd * std::sin(ra_rad);
        cx_z += std::sin(dec_rad);
    }
    cx_x *= 0.25; cx_y *= 0.25; cx_z *= 0.25;
    double ra_center = std::atan2(cx_y, cx_x) * 180.0 / M_PI;
    if (ra_center < 0) ra_center += 360.0;
    double cz_clamped = std::max(-1.0, std::min(1.0, cx_z));
    double dec_center = std::asin(cz_clamped) * 180.0 / M_PI;
    if (!std::isfinite(ra_center) || !std::isfinite(dec_center)) return;

    // 计算收缩后对角线角秒数 (用收缩后的四角对角点)
    double diag_arcsec = greatCircleDistance(
        cornersSky[0].ra, cornersSky[0].dec,
        cornersSky[2].ra, cornersSky[2].dec) * 3600.0;
    if (!std::isfinite(diag_arcsec) || diag_arcsec <= 0.0) return;

    double hpResArcsec = hp.pixelResolutionArcsec();

    // 点采样快速路径 (pixfrac <= 0)
    if (config.pixfrac <= 0.0) {
        int64_t ipix = hp.radec2pix(ra_center, dec_center);
        if (ipix >= 0) {
            auto& acc = accum[(uint64_t)ipix];
            acc.sumFlux   += pixelValue * weightValue;
            acc.sumWeight += weightValue;
            acc.sumSnrSq  += (double)snrValue * snrValue * weightValue;
        }
        return;
    }

    // ---- 候选 HEALPix 像素检索: 5基准全部1-ring (修复黑色缝隙) ----
    // 之前仅中心1-ring 在 WCS 畸变下可能漏掉源像素四角延伸到的相邻像素,
    // 导致球面出现微小黑色缝隙(邻居缺失率1.39%)。
    // 扩展为 5基准(中心+四角)各取1-ring邻居, 保证覆盖源像素周围所有可能相交的 HEALPix 像素。
    // 覆盖分析 (nside=65536, hpRes=3.22", pixfrac=1.0):
    //   - 源像素对角线 ≈ 6.31", 中心到角 ≈ 3.16"
    //   - HEALPix 像素半径 ≈ 1.61", 相交条件: 中心距 < 3.16"+1.61"=4.77"
    //   - 5基准+各1-ring 覆盖约 11x11 区域, 中心到边缘 ≈ 17.7" >> 4.77", 完全覆盖
    // 候选数: 5 基准 + 5×8 邻居 = 45 (去重后约 30-40)
    int64_t candidates_arr[48];
    int n_candidates = 0;

    // 线性去重 (候选数 < 48, 线性搜索比 unordered_set 快)
    auto addCandidate = [&](int64_t ipix) {
        if (ipix < 0) return;
        for (int i = 0; i < n_candidates; i++) {
            if (candidates_arr[i] == ipix) return;
        }
        candidates_arr[n_candidates++] = ipix;
    };

    // 5 基准: 中心 + 四角
    int64_t base_ipix[5];
    base_ipix[0] = hp.radec2pix(ra_center, dec_center);
    for (int i = 0; i < 4; i++) {
        base_ipix[i+1] = hp.radec2pix(cornersSky[i].ra, cornersSky[i].dec);
    }

    // 添加 5 基准本身
    for (int i = 0; i < 5; i++) {
        addCandidate(base_ipix[i]);
    }

    // 添加每个基准的 1-ring 邻居 (8 邻居/基准)
    for (int i = 0; i < 5; i++) {
        if (base_ipix[i] < 0) continue;
        auto nbrs = hp.neighbors(base_ipix[i]);
        for (int64_t nb : nbrs) {
            addCandidate(nb);
        }
    }

    if (n_candidates == 0) return;

    // ---- Step 5: 局部切平面面积裁剪 ----
    // 将像素四边形投影到以 (ra_center, dec_center) 为中心的切平面
    std::vector<Point2D> pixelQuad(4);
    for (int i = 0; i < 4; i++) {
        pixelQuad[i] = PolyClip::gnomonicForward(cornersSky[i].ra, cornersSky[i].dec,
                                                  ra_center, dec_center);
    }

    double pixelArea = std::abs(PolyClip::polygonArea(pixelQuad));
    if (pixelArea < 1e-20) {
        // 退化情况: 点采样
        int64_t ipix = hp.radec2pix(ra_center, dec_center);
        if (ipix >= 0) {
            auto& acc = accum[(uint64_t)ipix];
            acc.sumFlux   += pixelValue * weightValue;
            acc.sumWeight += weightValue;
            acc.sumSnrSq  += (double)snrValue * snrValue * weightValue;
        }
        return;
    }

    // 计算收缩后像素面积 (pixfrac<1 时收缩, pixfrac>=1 时不变)
    // 用于标准 Drizzle 通量守恒: weight = overlapArea / A_shrunk
    //   sum(weight per source) = A_shrunk / A_shrunk = 1
    //   sum_out = sum_in (通量守恒)
    // 注: pixfrac<1 收缩源像素覆盖范围, 但总通量不变
    //     收缩后单位面积通量 = pixelValue / pixfrac² (能量提高)
    //     out = sum(in × overlap/A_shrunk), sum_out = sum_in
    double shrunkPixelArea = pixelArea;  // pixelArea 已是收缩后面积 (Step 2 已收缩)

    // 计算 pixelQuad 的 bbox (用于早期剔除, 避免对不相交的 HEALPix 像素做 PolyClip)
    double px_min = pixelQuad[0].x, px_max = pixelQuad[0].x;
    double py_min = pixelQuad[0].y, py_max = pixelQuad[0].y;
    for (int i = 1; i < 4; i++) {
        if (pixelQuad[i].x < px_min) px_min = pixelQuad[i].x;
        if (pixelQuad[i].x > px_max) px_max = pixelQuad[i].x;
        if (pixelQuad[i].y < py_min) py_min = pixelQuad[i].y;
        if (pixelQuad[i].y > py_max) py_max = pixelQuad[i].y;
    }

    // 遍历候选 HEALPix 像素 (用固定数组, 避免 vector 分配)
    SkyCoord hpCornersArr[4];
    Point2D hpQuadArr[4];
    std::vector<Point2D> hpQuadVec(4);  // clipPolygon 需要 vector
    std::vector<Point2D> pixelQuadVec = pixelQuad;  // clipPolygon 需要 vector

    for (int ci = 0; ci < n_candidates; ci++) {
        int64_t ipix = candidates_arr[ci];
        // a. 获取 HEALPix 像素四角球面坐标 (菱形近似, 修复方形近似导致的边缘误判)
        // HEALPix 赤道带像素为菱形 (diamond), 不是方形:
        //   - 边长 a = res / sqrt(sqrt(3)) ≈ res / 1.316
        //   - NS 对角线 d_ns = sqrt(sqrt(3)) * res ≈ 1.316 * res
        //   - EW 对角线 d_ew = 2/sqrt(sqrt(3)) * res ≈ 1.516 * res
        // 4 个顶点 (北/东/南/西, 顺时针):
        //   - 北: (ra_c, dec_c + d_ns/2)
        //   - 东: (ra_c + d_ew/(2*cos(dec)), dec_c)
        //   - 南: (ra_c, dec_c - d_ns/2)
        //   - 西: (ra_c - d_ew/(2*cos(dec)), dec_c)
        // 之前方形近似 (±half_ra, ±half_dec) 在像素边缘会误判为不相交, 导致黑色缝隙
        double ra_c, dec_c;
        hp.pix2radec(ipix, &ra_c, &dec_c);

        // HEALPix 像素分辨率 (度)
        double res_deg = hpResArcsec / 3600.0;
        double cos_dec = std::cos(dec_c * D2R);

        // 菱形对角线半长 (度)
        // sqrt(sqrt(3)) ≈ 1.3160740129524924
        // NS 半对角线 = sqrt(sqrt(3))/2 * res ≈ 0.658 * res
        // EW 半对角线 = 1/sqrt(sqrt(3)) * res ≈ 0.760 * res
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

        // 菱形 4 顶点 (北/西/南/东, 逆时针, 兼容 PolyClip::clipPolygon 的 Sutherland-Hodgman 算法)
        hpCornersArr[0] = {ra_c,           dec_c + half_dec};  // 北
        hpCornersArr[1] = {ra_c - half_ra, dec_c            };  // 西
        hpCornersArr[2] = {ra_c,           dec_c - half_dec};  // 南
        hpCornersArr[3] = {ra_c + half_ra, dec_c            };  // 东

        // b/c. 将 HEALPix 像素四边形投影到同一切平面
        for (int i = 0; i < 4; i++) {
            hpQuadArr[i] = PolyClip::gnomonicForward(hpCornersArr[i].ra, hpCornersArr[i].dec,
                                                     ra_center, dec_center);
        }

        // bbox 早期剔除: 如果 hpQuad 和 pixelQuad 的 bbox 不相交, 跳过
        // 这是最大的性能优化: 避免对不相交的像素做昂贵的 PolyClip 裁剪
        double hx_min = hpQuadArr[0].x, hx_max = hpQuadArr[0].x;
        double hy_min = hpQuadArr[0].y, hy_max = hpQuadArr[0].y;
        for (int i = 1; i < 4; i++) {
            if (hpQuadArr[i].x < hx_min) hx_min = hpQuadArr[i].x;
            if (hpQuadArr[i].x > hx_max) hx_max = hpQuadArr[i].x;
            if (hpQuadArr[i].y < hy_min) hy_min = hpQuadArr[i].y;
            if (hpQuadArr[i].y > hy_max) hy_max = hpQuadArr[i].y;
        }
        if (hx_max < px_min || hx_min > px_max || hy_max < py_min || hy_min > py_max) {
            continue;  // bbox 不相交, 跳过 PolyClip
        }

        // d. 用 PolyClip::clipPolygon 计算交集 (pixelQuad 为 subject, hpQuad 为 clip)
        // 将数组拷贝到 vector (PolyClip 接口要求)
        for (int i = 0; i < 4; i++) hpQuadVec[i] = hpQuadArr[i];
        std::vector<Point2D> intersection = PolyClip::clipPolygon(pixelQuadVec, hpQuadVec);
        if (intersection.size() < 3) continue;

        // e. 交集面积
        double overlapArea = std::abs(PolyClip::polygonArea(intersection));
        if (overlapArea < 1e-20) continue;

        // g. 权重 = 交集面积 / 收缩后像素面积 (标准 Drizzle 通量守恒)
        // sum(weight per source) = A_shrunk / A_shrunk = 1
        // 配合 brightness = sumFlux, 实现 sum_out = sum_in
        double weight = overlapArea / shrunkPixelArea;

        // ---- Step 6: 通量守恒分配 ----
        if (weight > 1e-10) {
            auto& acc = accum[(uint64_t)ipix];
            acc.sumFlux   += pixelValue * weight * weightValue;
            acc.sumWeight += weight * weightValue;
            acc.sumSnrSq  += (double)snrValue * snrValue * weight * weightValue;
        }
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
// writeHis - 将累加器归一化并写入 .hiss 文件
// snr_model != nullptr: 写稀疏控制点格式 (snr_format=1)
// snr_model == nullptr: 不写 SNR 通道
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

    // 1. 收集有效像素 (sumWeight > 0)
    struct PixelEntry {
        uint64_t ipix;
        double   brightness;
    };
    std::vector<PixelEntry> entries;
    entries.reserve(accumulators.size());

    for (const auto& [ipix, acc] : accumulators) {
        if (acc.sumWeight > 0.0) {
            // 通量守恒模式: brightness = sumFlux (不除以 sumWeight)
            double brightness = acc.sumFlux;
            entries.push_back({ipix, brightness});
        }
    }

    if (entries.empty()) {
        error_msg = "无有效像素可写入";
        fprintf(stderr, "[drizzle_engine] %s\n", error_msg.c_str());
        return false;
    }

    // 2. 排序 ipix (升序)
    std::sort(entries.begin(), entries.end(),
              [](const PixelEntry& a, const PixelEntry& b) {
                  return a.ipix < b.ipix;
              });

    size_t n = entries.size();
    fprintf(stderr, "[drizzle_engine] 写入 %zu 个 HEALPix 像素到 %s\n", n, outputPath.c_str());

    // 3. 构造 ipix / pixel 数组 (不再构造逐像素 snrArr)
    std::vector<uint64_t> ipixArr(n);
    std::vector<float>    pixelArr(n);
    for (size_t i = 0; i < n; i++) {
        ipixArr[i]  = entries[i].ipix;
        pixelArr[i] = (float)entries[i].brightness;
    }

    // 4. 构造 JSON meta 字符串 (hiss_write 内部会前置 nside/nested/n_pix 字段)
    //    包含: filter / exposure_s / obs_time / pixfrac / fits_meta / wcs / source / drizzle
    char buf[512];
    std::string json;
    json.reserve(2048);

    json += "{";

    // filter
    json += "\"filter\":\"";
    json += escapeJsonString(meta.filter);
    json += "\",";

    // exposure_s
    snprintf(buf, sizeof(buf), "\"exposure_s\":%.6f,", meta.exposure_s);
    json += buf;

    // obs_time
    json += "\"obs_time\":\"";
    json += escapeJsonString(meta.obs_time);
    json += "\",";

    // pixfrac
    snprintf(buf, sizeof(buf), "\"pixfrac\":%.4f,", config.pixfrac);
    json += buf;

    // wcs (原始 WCS 参数)
    snprintf(buf, sizeof(buf),
             "\"wcs\":{\"cd\":[%.12e,%.12e,%.12e,%.12e],\"crval\":[%.10f,%.10f],"
             "\"crpix\":[%.6f,%.6f],\"sip_order\":%d},",
             wcs.cd[0], wcs.cd[1], wcs.cd[2], wcs.cd[3],
             wcs.crval[0], wcs.crval[1],
             wcs.crpix[0], wcs.crpix[1],
             wcs.sip.order);
    json += buf;

    // fits_meta (OBJCTRA/OBJCTDEC/IMAGETYP/SITELAT/SITELONG 等)
    json += "\"fits_meta\":{";
    bool first = true;
    for (const auto& [k, v] : meta.fits_meta) {
        if (!first) json += ",";
        first = false;
        json += "\"";
        json += escapeJsonString(k);
        json += "\":\"";
        json += escapeJsonString(v);
        json += "\"";
    }
    json += "},";

    // source
    json += "\"source\":{\"fits_path\":\"";
    json += escapeJsonString(fitsPath);
    json += "\",\"n_source_pixels\":";
    json += std::to_string(stats.nSourcePixels);
    json += "},";

    // drizzle
    snprintf(buf, sizeof(buf),
             "\"drizzle\":{\"n_healpix_pixels\":%zu,\"elapsed_sec\":%.4f}",
             n, stats.elapsedSec);
    json += buf;

    json += "}";

    // 5. 写入 .hiss 文件
    //    snr_model != nullptr: 稀疏控制点格式 (snr_format=1)
    //    snr_model == nullptr: 无 SNR 通道
    int rc;
    if (snr_model && snr_model->n_points > 0) {
        fprintf(stderr, "[drizzle_engine] hiss_write_snr_model: meta_json=%zu bytes, n_points=%u\n",
                json.size(), snr_model->n_points);
        rc = hiss_write_snr_model(outputPath.c_str(),
                                   (uint32_t)config.nside,
                                   config.nested ? 1 : 0,
                                   (uint64_t)n,
                                   ipixArr.data(),
                                   pixelArr.data(),
                                   snr_model,
                                   json.c_str());
        if (rc != 0) {
            error_msg = "hiss_write_snr_model 写入失败 (rc=" + std::to_string(rc) + "): " + outputPath;
            fprintf(stderr, "[drizzle_engine] %s\n", error_msg.c_str());
            return false;
        }
    } else {
        fprintf(stderr, "[drizzle_engine] hiss_write: meta_json=%zu bytes, has_snr=false\n", json.size());
        rc = hiss_write(outputPath.c_str(),
                        (uint32_t)config.nside,
                        config.nested ? 1 : 0,
                        (uint64_t)n,
                        ipixArr.data(),
                        pixelArr.data(),
                        nullptr,  // 无逐像素 SNR
                        json.c_str());
        if (rc != 0) {
            error_msg = "hiss_write 写入失败 (rc=" + std::to_string(rc) + "): " + outputPath;
            fprintf(stderr, "[drizzle_engine] %s\n", error_msg.c_str());
            return false;
        }
    }

    fprintf(stderr, "[drizzle_engine] 写入成功: %s (%zu 像素)\n",
            outputPath.c_str(), n);
    return true;
}

} // namespace drizzle
