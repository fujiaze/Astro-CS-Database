// hp_drizzle_api.cpp - Drizzle C API 导出层
// 编译 DLL 时需定义 HP_DRIZZLE_EXPORTS 宏
// 输出格式: .hiss (通过 healpix_io.dll 的 hiss_write)

#define HP_DRIZZLE_EXPORTS
#include "hp_drizzle_api.h"

#include "drizzle_engine.h"
#include "fits_reader.h"
#include "wcs_sip.h"
#include "astro_image_io.h"   // aio_frame_get_block / aio_frame_kv_get
#include "gradient/snr_evaluator.h"  // SnrEvaluator (KD-tree IDW 重建逐像素 SNR)
#include "aio_healpix_io.h"         // HioSnrModel, HioSnrControlPoint (向后兼容宏)

#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

using namespace drizzle;

// ============================================================================
// 辅助: 将 std::string 错误信息拷贝到 result->error_msg (截断到 511 字节)
// ============================================================================
static void setErrorMsg(HpDrizzleResult* result, const std::string& msg) {
    if (!result) return;
    size_t n = msg.copy(result->error_msg, sizeof(result->error_msg) - 1);
    result->error_msg[n] = '\0';
}

// ============================================================================
// hp_drizzle_fits_to_ahpx - 执行 Drizzle: FITS → .hiss
// ============================================================================
HP_DRIZZLE_API int hp_drizzle_fits_to_ahpx(
    const char* fits_path,
    const char* output_path,
    int nside,
    int nested,
    double pixfrac,
    const char* snr_path,
    const char* weight_path,
    HpDrizzleResult* result)
{
    // 1. 参数校验
    if (!fits_path || !output_path || !result) {
        fprintf(stderr, "[hp_drizzle_api] 参数非法: fits_path/output_path/result 不能为空\n");
        if (result) {
            std::string msg = "参数非法: fits_path/output_path/result 不能为空";
            setErrorMsg(result, msg);
        }
        return 1;
    }

    if (nside <= 0 || (nside & (nside - 1)) != 0) {
        fprintf(stderr, "[hp_drizzle_api] nside=%d 非法 (必须是 2 的幂)\n", nside);
        setErrorMsg(result, "nside 非法 (必须是 2 的幂): " + std::to_string(nside));
        return 2;
    }

    if (pixfrac < 0.0 || pixfrac > 1.0) {
        fprintf(stderr, "[hp_drizzle_api] pixfrac=%.4f 超出范围 [0.0, 1.0]\n", pixfrac);
        setErrorMsg(result, "pixfrac 超出范围 [0.0, 1.0]");
        return 3;
    }

    // 初始化 result
    std::memset(result, 0, sizeof(HpDrizzleResult));
    result->nside   = nside;
    result->nested  = nested ? 1 : 0;
    result->pixfrac = pixfrac;

    fprintf(stderr, "[hp_drizzle_api] 开始 Drizzle: fits=%s output=%s nside=%d nested=%d pixfrac=%.4f\n",
            fits_path, output_path, nside, nested ? 1 : 0, pixfrac);

    // 2. 读取主 FITS 图像
    FitsImage img;
    std::string errMsg;
    if (!readFits(fits_path, img, errMsg)) {
        fprintf(stderr, "[hp_drizzle_api] 读取 FITS 失败: %s\n", errMsg.c_str());
        setErrorMsg(result, "读取 FITS 失败: " + errMsg);
        return 4;
    }

    if (!img.wcs.has_wcs) {
        fprintf(stderr, "[hp_drizzle_api] FITS 无 WCS 信息\n");
        setErrorMsg(result, "FITS 无 WCS 信息");
        return 5;
    }

    // 3. 读取可选 SNR FITS
    std::vector<float> snrData;
    const float* snrPtr = nullptr;
    if (snr_path && snr_path[0] != '\0') {
        FitsImage snrImg;
        if (!readFits(snr_path, snrImg, errMsg)) {
            fprintf(stderr, "[hp_drizzle_api] 读取 SNR FITS 失败: %s\n", errMsg.c_str());
            setErrorMsg(result, "读取 SNR FITS 失败: " + errMsg);
            return 6;
        }
        if (snrImg.width != img.width || snrImg.height != img.height) {
            fprintf(stderr, "[hp_drizzle_api] SNR 图尺寸不匹配 (%dx%d vs %dx%d)\n",
                    snrImg.width, snrImg.height, img.width, img.height);
            setErrorMsg(result, "SNR 图尺寸不匹配");
            return 7;
        }
        // 取第一通道
        if (snrImg.channels == 1) {
            snrData = std::move(snrImg.pixels);
        } else {
            size_t plane = (size_t)snrImg.width * snrImg.height;
            snrData.resize(plane);
            for (size_t i = 0; i < plane; i++) {
                snrData[i] = snrImg.pixels[i * snrImg.channels + 0];
            }
        }
        snrPtr = snrData.data();
        fprintf(stderr, "[hp_drizzle_api] SNR 图加载成功: %s (%dx%d)\n",
                snr_path, snrImg.width, snrImg.height);
    }

    // 4. 读取可选权重 FITS
    std::vector<float> weightData;
    const float* weightPtr = nullptr;
    if (weight_path && weight_path[0] != '\0') {
        FitsImage weightImg;
        if (!readFits(weight_path, weightImg, errMsg)) {
            fprintf(stderr, "[hp_drizzle_api] 读取权重 FITS 失败: %s\n", errMsg.c_str());
            setErrorMsg(result, "读取权重 FITS 失败: " + errMsg);
            return 8;
        }
        if (weightImg.width != img.width || weightImg.height != img.height) {
            fprintf(stderr, "[hp_drizzle_api] 权重图尺寸不匹配 (%dx%d vs %dx%d)\n",
                    weightImg.width, weightImg.height, img.width, img.height);
            setErrorMsg(result, "权重图尺寸不匹配");
            return 9;
        }
        // 取第一通道
        if (weightImg.channels == 1) {
            weightData = std::move(weightImg.pixels);
        } else {
            size_t plane = (size_t)weightImg.width * weightImg.height;
            weightData.resize(plane);
            for (size_t i = 0; i < plane; i++) {
                weightData[i] = weightImg.pixels[i * weightImg.channels + 0];
            }
        }
        weightPtr = weightData.data();
        fprintf(stderr, "[hp_drizzle_api] 权重图加载成功: %s (%dx%d)\n",
                weight_path, weightImg.width, weightImg.height);
    }

    // 5. 构造 DrizzleConfig
    DrizzleConfig config;
    config.nside    = nside;
    config.nested   = nested ? true : false;
    config.pixfrac  = pixfrac;

    // 6. 执行 Drizzle
    DrizzleEngine engine;
    std::unordered_map<uint64_t, PixelAccumulator> accumulators;
    DrizzleStats stats;

    if (!engine.drizzle(img, config, snrPtr, weightPtr, accumulators, stats, errMsg)) {
        fprintf(stderr, "[hp_drizzle_api] Drizzle 失败: %s\n", errMsg.c_str());
        setErrorMsg(result, "Drizzle 失败: " + errMsg);
        return 10;
    }

    // 7. 写入 .hiss 文件
    //    DrizzleMeta: FitsImage 未保存 FILTER/EXPTIME/DATE-OBS 等 KV, 留空
    //    output_path 后缀规范化为 .hiss (兼容旧 .ahpx 调用)
    std::string hissPath = output_path;
    {
        size_t plen = hissPath.size();
        if (plen >= 5 && (hissPath.compare(plen - 5, 5, ".ahpx") == 0)) {
            hissPath.replace(plen - 5, 5, ".hiss");
        }
    }
    DrizzleMeta meta;  // FITS 路径无 header KV, meta 留空
    if (!engine.writeHis(accumulators, stats, img.wcs, config, meta, fits_path, hissPath,
                         nullptr, errMsg)) {
        fprintf(stderr, "[hp_drizzle_api] 写入 .hiss 失败: %s\n", errMsg.c_str());
        setErrorMsg(result, "写入 .hiss 失败: " + errMsg);
        return 11;
    }

    // 8. 填充结果
    result->n_healpix_pixels = stats.nHealpixPixels;
    result->n_source_pixels  = stats.nSourcePixels;
    result->nside            = stats.nside;
    result->nested           = stats.nested ? 1 : 0;
    result->pixfrac          = config.pixfrac;
    result->elapsed_sec      = stats.elapsedSec;

    fprintf(stderr, "[hp_drizzle_api] 完成: %lld 源像素 → %lld HEALPix 像素, 耗时 %.3fs\n",
            (long long)result->n_source_pixels, (long long)result->n_healpix_pixels,
            result->elapsed_sec);

    return 0;
}

// ============================================================================
// hp_drizzle_run - Drizzle 阶段: PipelineFrame 命名块直通 → .hiss
//
// 从 PipelineFrame 的 "data" 块 (float32[H,W]) 和 "header" KV 块 (WCS+SIP)
// 直接构造 FitsImage, 调用 DrizzleEngine, 不经临时 FITS 文件。
// 输出 .hiss 文件通过 healpix_io.dll 的 hiss_write 写入。
// ============================================================================
HP_DRIZZLE_API int hp_drizzle_run(PipelineFrame* frame,
                                   int nside, int nested, double pixfrac,
                                   const char* output_path,
                                   HpDrizzleResult* result)
{
    // 1. 参数校验
    if (!frame || !result) {
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: 参数非法 (frame/result 为空)\n");
        if (result) {
            std::memset(result, 0, sizeof(HpDrizzleResult));
            setErrorMsg(result, "参数非法: frame/result 为空");
        }
        return -1;
    }

    if (nside <= 0 || (nside & (nside - 1)) != 0) {
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: nside=%d 非法 (必须是 2 的幂)\n", nside);
        std::memset(result, 0, sizeof(HpDrizzleResult));
        setErrorMsg(result, "nside 非法 (必须是 2 的幂): " + std::to_string(nside));
        return -2;
    }

    if (pixfrac < 0.0 || pixfrac > 1.0) {
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: pixfrac=%.4f 超出范围 [0.0, 1.0]\n", pixfrac);
        std::memset(result, 0, sizeof(HpDrizzleResult));
        setErrorMsg(result, "pixfrac 超出范围 [0.0, 1.0]");
        return -3;
    }

    // 初始化 result
    std::memset(result, 0, sizeof(HpDrizzleResult));
    result->nside   = nside;
    result->nested  = nested ? 1 : 0;
    result->pixfrac = pixfrac;

    fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: 开始 (nside=%d, nested=%d, pixfrac=%.4f, output=%s)\n",
            nside, nested ? 1 : 0, pixfrac, output_path ? output_path : "(null)");

    // 2. 读取 data 块 (float32[H,W])
    const AioBlock* data_blk = aio_frame_get_block(frame, "data");
    if (!data_blk) {
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: frame 中缺少 'data' 块\n");
        setErrorMsg(result, "frame 中缺少 'data' 块");
        return -4;
    }
    if (data_blk->type != AIO_BLOCK_FLOAT32) {
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: 'data' 块类型非 FLOAT32 (type=%d)\n",
                (int)data_blk->type);
        setErrorMsg(result, "'data' 块类型非 FLOAT32");
        return -5;
    }
    if (data_blk->n_dims < 2) {
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: 'data' 块维度数 < 2 (n_dims=%d)\n",
                data_blk->n_dims);
        setErrorMsg(result, "'data' 块维度数 < 2");
        return -6;
    }

    int height = data_blk->dims[0];
    int width  = data_blk->dims[1];
    if (width <= 0 || height <= 0) {
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: 'data' 块尺寸非法 (%dx%d)\n", width, height);
        setErrorMsg(result, "'data' 块尺寸非法: " + std::to_string(width) + "x" + std::to_string(height));
        return -7;
    }

    const float* pixels = (const float*)data_blk->data;
    if (!pixels) {
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: 'data' 块数据指针为空\n");
        setErrorMsg(result, "'data' 块数据指针为空");
        return -8;
    }

    fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: data 块 %dx%d float32\n", width, height);

    // 3. 从 header KV 块读取 WCS 字段
    double crval1 = aio_frame_kv_get_double(frame, "header", "CRVAL1", 0.0);
    double crval2 = aio_frame_kv_get_double(frame, "header", "CRVAL2", 0.0);
    double crpix1 = aio_frame_kv_get_double(frame, "header", "CRPIX1", 0.0);
    double crpix2 = aio_frame_kv_get_double(frame, "header", "CRPIX2", 0.0);
    double cd11   = aio_frame_kv_get_double(frame, "header", "CD1_1", 0.0);
    double cd12   = aio_frame_kv_get_double(frame, "header", "CD1_2", 0.0);
    double cd21   = aio_frame_kv_get_double(frame, "header", "CD2_1", 0.0);
    double cd22   = aio_frame_kv_get_double(frame, "header", "CD2_2", 0.0);

    // CDELT/CROTA2 备选 (无 CD 矩阵时使用)
    double cdelt1 = aio_frame_kv_get_double(frame, "header", "CDELT1", 0.0);
    double cdelt2 = aio_frame_kv_get_double(frame, "header", "CDELT2", 0.0);
    double crota2 = aio_frame_kv_get_double(frame, "header", "CROTA2", 0.0);

    // 4. 构造 WCS 参数
    FitsImage img;
    img.width = width;
    img.height = height;
    img.channels = 1;
    img.pixels.assign(pixels, pixels + (size_t)width * height);
    img.bzero = 0.0;
    img.bscale = 1.0;

    WcsParams& wcs = img.wcs;
    wcs.crval[0] = crval1;
    wcs.crval[1] = crval2;
    wcs.crpix[0] = crpix1;
    wcs.crpix[1] = crpix2;
    wcs.cd[0] = cd11;
    wcs.cd[1] = cd12;
    wcs.cd[2] = cd21;
    wcs.cd[3] = cd22;
    wcs.has_wcs = false;

    // CTYPE1/CTYPE2
    const char* ctype1_str = aio_frame_kv_get(frame, "header", "CTYPE1");
    const char* ctype2_str = aio_frame_kv_get(frame, "header", "CTYPE2");
    if (ctype1_str) std::strncpy(wcs.ctype1, ctype1_str, sizeof(wcs.ctype1) - 1);
    if (ctype2_str) std::strncpy(wcs.ctype2, ctype2_str, sizeof(wcs.ctype2) - 1);

    // 判断是否有效 WCS: 需要 CRVAL + CRPIX + CD (或 CDELT)
    bool has_cd = (cd11 != 0.0 || cd22 != 0.0);
    bool has_cdelt = (cdelt1 != 0.0 && cdelt2 != 0.0);

    if (has_cd) {
        wcs.has_wcs = true;
    } else if (has_cdelt && crval1 != 0.0 && crval2 != 0.0) {
        // 无 CD 矩阵时, 用 CDELT+CROTA2 构造
        const double DEG2RAD = 0.017453292519943295769;
        double cosr = std::cos(crota2 * DEG2RAD);
        double sinr = std::sin(crota2 * DEG2RAD);
        wcs.cd[0] = cdelt1 * cosr;
        wcs.cd[1] = -cdelt2 * sinr;
        wcs.cd[2] = cdelt1 * sinr;
        wcs.cd[3] = cdelt2 * cosr;
        wcs.has_wcs = true;
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: 使用 CDELT+CROTA2 构造 CD 矩阵\n");
    }

    if (!wcs.has_wcs) {
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: header 缺少 WCS 信息 (CD 或 CDELT+CRVAL+CRPIX)\n");
        setErrorMsg(result, "header 缺少 WCS 信息");
        return -9;
    }

    fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: WCS CRVAL=(%.6f,%.6f) CRPIX=(%.3f,%.3f) CD=[%.3e,%.3e,%.3e,%.3e]\n",
            wcs.crval[0], wcs.crval[1], wcs.crpix[0], wcs.crpix[1],
            wcs.cd[0], wcs.cd[1], wcs.cd[2], wcs.cd[3]);

    // 5. 读取 SIP 系数 (若存在 A_ORDER)
    const char* a_order_str = aio_frame_kv_get(frame, "header", "A_ORDER");
    if (a_order_str) {
        int a_order = atoi(a_order_str);
        const char* b_order_str = aio_frame_kv_get(frame, "header", "B_ORDER");
        int b_order = b_order_str ? atoi(b_order_str) : a_order;
        wcs.sip.order = a_order;

        // 读取 A_i_j / B_i_j (跳过 (0,0), i+j<=order)
        for (int i = 0; i <= a_order; i++) {
            for (int j = 0; j <= a_order; j++) {
                if (i + j == 0 || i + j > a_order) continue;
                char key[16];
                std::snprintf(key, sizeof(key), "A_%d_%d", i, j);
                const char* val = aio_frame_kv_get(frame, "header", key);
                if (val) wcs.sip.a[i * 6 + j] = std::atof(val);

                std::snprintf(key, sizeof(key), "B_%d_%d", i, j);
                val = aio_frame_kv_get(frame, "header", key);
                if (val) wcs.sip.b[i * 6 + j] = std::atof(val);
            }
        }

        // 读取 AP_i_j / BP_i_j (逆向 SIP)
        const char* ap_order_str = aio_frame_kv_get(frame, "header", "AP_ORDER");
        if (ap_order_str) {
            int ap_order = atoi(ap_order_str);
            const char* bp_order_str = aio_frame_kv_get(frame, "header", "BP_ORDER");
            int bp_order = bp_order_str ? atoi(bp_order_str) : ap_order;
            wcs.sip.ap_order = ap_order;

            for (int i = 0; i <= ap_order; i++) {
                for (int j = 0; j <= ap_order; j++) {
                    if (i + j == 0 || i + j > ap_order) continue;
                    char key[16];
                    std::snprintf(key, sizeof(key), "AP_%d_%d", i, j);
                    const char* val = aio_frame_kv_get(frame, "header", key);
                    if (val) wcs.sip.ap[i * 6 + j] = std::atof(val);

                    std::snprintf(key, sizeof(key), "BP_%d_%d", i, j);
                    val = aio_frame_kv_get(frame, "header", key);
                    if (val) wcs.sip.bp[i * 6 + j] = std::atof(val);
                }
            }
            fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: SIP A_ORDER=%d B_ORDER=%d AP_ORDER=%d BP_ORDER=%d\n",
                    a_order, b_order, ap_order, bp_order);
        } else {
            fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: SIP A_ORDER=%d B_ORDER=%d (无逆向 AP/BP)\n",
                    a_order, b_order);
        }
    }

    // 5.5 读取 "snr_model" 块 (稀疏控制点, AIO_BLOCK_RAW) → SnrEvaluator 重建逐像素 SNR
    //     序列化格式 (与 .hiss snr_format=1 一致):
    //       [n_points: uint32][points: n_points×20B][snr_phot: f64][median_snr: f64][idw_power: f64]
    //     注: 必须在 img.wcs 构造完成后执行 (pixelToSkyBatch 需要 WCS)
    const float* snrPtr = nullptr;
    std::vector<float> snrRebuilt;       // 重建的逐像素 SNR (生命周期需覆盖 drizzle 调用)
    HioSnrModel snrModelData = {};       // 用于 writeHis (生命周期需覆盖 writeHis 调用)
    const HioSnrModel* snrModelPtr = nullptr;
    {
        const AioBlock* snr_blk = aio_frame_get_block(frame, "snr_model");
        if (snr_blk && snr_blk->type == AIO_BLOCK_RAW && snr_blk->data && snr_blk->count >= 4) {
            const uint8_t* raw = (const uint8_t*)snr_blk->data;
            uint32_t n_points = *(const uint32_t*)raw;
            size_t expected = 4 + (size_t)n_points * sizeof(HioSnrControlPoint) + 24;
            if (n_points > 0 && (size_t)snr_blk->count >= expected) {
                // 解析稀疏模型 (零拷贝: points 指针直接指向块数据)
                snrModelData.n_points   = n_points;
                snrModelData.points     = (HioSnrControlPoint*)(raw + 4);
                snrModelData.snr_phot   = *(const double*)(raw + 4 + (size_t)n_points * 20);
                snrModelData.median_snr = *(const double*)(raw + 4 + (size_t)n_points * 20 + 8);
                snrModelData.idw_power  = *(const double*)(raw + 4 + (size_t)n_points * 20 + 16);
                snrModelPtr = &snrModelData;

                fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: snr_model 块加载: n_points=%u, "
                                "snr_phot=%.4f, median_snr=%.4f, idw_power=%.2f\n",
                        n_points, snrModelData.snr_phot, snrModelData.median_snr,
                        snrModelData.idw_power);

                // 拆分控制点数组 → SnrEvaluator.build
                std::vector<double> cp_ra(n_points), cp_dec(n_points);
                std::vector<float>  cp_snr(n_points);
                for (uint32_t i = 0; i < n_points; i++) {
                    cp_ra[i]  = snrModelData.points[i].ra;
                    cp_dec[i] = snrModelData.points[i].dec;
                    cp_snr[i] = snrModelData.points[i].snr_psf;
                }

                gradient::SnrEvaluator evaluator;
                if (evaluator.build(n_points, cp_ra.data(), cp_dec.data(), cp_snr.data(),
                                    snrModelData.snr_phot, snrModelData.median_snr,
                                    snrModelData.idw_power)) {
                    // 批量像素→球面→SNR 重建
                    size_t nPix = (size_t)width * height;
                    std::vector<double> xy(nPix * 2), radec(nPix * 2);
                    for (int y = 0; y < height; y++) {
                        for (int x = 0; x < width; x++) {
                            xy[(y * width + x) * 2]     = (double)x;
                            xy[(y * width + x) * 2 + 1] = (double)y;
                        }
                    }

                    WcsSip wcsBatch(img.wcs);
                    wcsBatch.pixelToSkyBatch(xy.data(), (int)nPix, radec.data());

                    std::vector<double> qra(nPix), qdec(nPix);
                    for (size_t i = 0; i < nPix; i++) {
                        qra[i]  = radec[i * 2];
                        qdec[i] = radec[i * 2 + 1];
                    }

                    snrRebuilt.resize(nPix);
                    evaluator.evaluateBatch(qra.data(), qdec.data(), nPix, snrRebuilt.data());
                    snrPtr = snrRebuilt.data();

                    fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: SNR 重建完成 (%zu 像素)\n", nPix);
                } else {
                    fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: SnrEvaluator build 失败, snr=1.0\n");
                }
            } else {
                fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: snr_model 块数据不完整 "
                                "(count=%lld, expected>=%zu)\n",
                        (long long)snr_blk->count, expected);
            }
        } else {
            fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: 无 snr_model 块, 使用 nullptr (snr=1.0)\n");
        }
    }

    // 6. 构造 DrizzleConfig
    DrizzleConfig config;
    config.nside   = nside;
    config.nested  = nested ? true : false;
    config.pixfrac = pixfrac;

    // 7. 执行 Drizzle
    DrizzleEngine engine;
    std::unordered_map<uint64_t, PixelAccumulator> accumulators;
    DrizzleStats stats;
    std::string errMsg;

    if (!engine.drizzle(img, config, snrPtr, nullptr, accumulators, stats, errMsg)) {
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: Drizzle 失败: %s\n", errMsg.c_str());
        setErrorMsg(result, "Drizzle 失败: " + errMsg);
        return -10;
    }

    fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: Drizzle 完成 (%lld 源像素 → %lld HEALPix 像素, 耗时 %.3fs)\n",
            (long long)stats.nSourcePixels, (long long)stats.nHealpixPixels, stats.elapsedSec);

    // 8. 写入 .hiss 文件 (若指定 output_path)
    if (output_path && output_path[0] != '\0') {
        // output_path 后缀规范化为 .hiss (兼容旧 .ahpx 调用)
        std::string hissPath = output_path;
        {
            size_t plen = hissPath.size();
            if (plen >= 5 && (hissPath.compare(plen - 5, 5, ".ahpx") == 0)) {
                hissPath.replace(plen - 5, 5, ".hiss");
            }
        }

        // 源路径 (用于元数据), 从 header KV 读取 SOURCE_PATH, 没有则用空串
        std::string sourcePath;
        const char* src = aio_frame_kv_get(frame, "header", "SOURCE_PATH");
        if (src) sourcePath = src;

        // 从 header KV 读取 FILTER/EXPTIME/DATE-OBS 等元数据
        DrizzleMeta meta;
        const char* filter_str = aio_frame_kv_get(frame, "header", "FILTER");
        if (filter_str) meta.filter = filter_str;

        const char* exptime_str = aio_frame_kv_get(frame, "header", "EXPTIME");
        if (exptime_str) meta.exposure_s = std::atof(exptime_str);

        const char* dateobs_str = aio_frame_kv_get(frame, "header", "DATE-OBS");
        if (dateobs_str) meta.obs_time = dateobs_str;

        // 收集 FITS 头 KV 到 fits_meta (OBJCTRA/OBJCTDEC/IMAGETYP/SITELAT/SITELONG 等)
        static const char* FITS_META_KEYS[] = {
            "OBJCTRA", "OBJCTDEC", "IMAGETYP", "SITELAT", "SITELONG",
            "OBJECT", "RADESYS", "EQUINOX", "INSTRUME", "TELESCOP",
            "XPIXSZ", "YPIXSZ", "XBINNING", "YBINNING", "GAIN", "OFFSET"
        };
        for (const char* k : FITS_META_KEYS) {
            const char* v = aio_frame_kv_get(frame, "header", k);
            if (v && v[0] != '\0') {
                meta.fits_meta[k] = v;
            }
        }

        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: 写入 .hiss (filter=%s, exptime=%.1f, date=%s, fits_meta=%zu)\n",
                meta.filter.c_str(), meta.exposure_s, meta.obs_time.c_str(), meta.fits_meta.size());

        if (!engine.writeHis(accumulators, stats, img.wcs, config, meta, sourcePath, hissPath,
                             snrModelPtr, errMsg)) {
            fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: 写入 .hiss 失败: %s\n", errMsg.c_str());
            setErrorMsg(result, "写入 .hiss 失败: " + errMsg);
            return -11;
        }
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: .hiss 已写入 %s\n", hissPath.c_str());
    }

    // 9. 填充结果
    result->n_healpix_pixels = stats.nHealpixPixels;
    result->n_source_pixels  = stats.nSourcePixels;
    result->nside            = stats.nside;
    result->nested           = stats.nested ? 1 : 0;
    result->pixfrac          = config.pixfrac;
    result->elapsed_sec      = stats.elapsedSec;

    return 0;
}
