// hp_drizzle_api.cpp - Drizzle C API 导出层
// 编译 DLL 时需定义 HP_DRIZZLE_EXPORTS 宏
// 输出格式: .hiss (通过 healpix_io.dll 的 hiss_write)

#define HP_DRIZZLE_EXPORTS
#include "hp_drizzle_api.h"

#include "drizzle_engine.h"
#include "fits_reader.h"
#include "wcs_sip.h"
#include "reverse_drizzle.h"
#include "astro_image_io.h"   // aio_frame_get_block / aio_frame_kv_get
#include "snr_evaluator.h"  // SnrEvaluator (KD-tree IDW 重建逐像素 SNR; V18 模块内私有实现)
#include "aio_healpix_io.h"         // HioSnrModel, HioSnrControlPoint (向后兼容宏)
#include "astro_sphere_sink.h"      // Phase1 V3: Drizzle -> AIO HiPS 直写

#include <cstdio>
#include <cstring>
#include <cmath>
#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

using namespace drizzle;

// ============================================================================
// 反向 Drizzle (Sphere -> Plane) 正式 C ABI (签字修正 REV-101)
// ============================================================================

static void setReverseErr(HpReverseDrizzleResult* result,
                          const std::string& msg) {
    if (!result) return;
    size_t n = msg.copy(result->error_msg, sizeof(result->error_msg) - 1);
    result->error_msg[n] = '\0';
}

HP_DRIZZLE_API int hp_drizzle_reverse_run(
    const HpReverseDrizzleInput* in,
    void* signal_out,
    void* coverage_out,
    HpReverseDrizzleResult* result)
{
    if (!in || !result) {
        fprintf(stderr, "[hp_drizzle_api] reverse: 参数非法 (in/result 不能为空)\n");
        return 1;
    }
    std::memset(result, 0, sizeof(HpReverseDrizzleResult));
    if (!signal_out || !coverage_out) {
        setReverseErr(result, "reverse: signal_out/coverage_out 不能为空");
        return 2;
    }
    if (in->nested != 1) {
        setReverseErr(result, "reverse: 仅支持 NESTED (nested=1)");
        return 3;
    }
    if (in->n_leaf < 0 || in->target_width <= 0 || in->target_height <= 0 ||
        (in->leaf_ipix == nullptr && in->n_leaf > 0)) {
        setReverseErr(result, "reverse: 输入参数非法 (n_leaf/尺寸/ipix)");
        return 4;
    }
    // REV-105: 两种 signal 同时提供 → 拒绝 (C ABI 层, 与 ReverseDrizzle::run 一致)
    if (in->leaf_signal_f32 && in->leaf_signal_f64) {
        setReverseErr(result, "reverse: 必须且只能提供一种 signal (f32 或 f64)");
        return 5;
    }

    ReverseDrizzleInput rin;
    rin.nside = (uint32_t)in->nside;
    rin.nested = true;
    rin.target_width = in->target_width;
    rin.target_height = in->target_height;
    rin.pixfrac = in->pixfrac;
    rin.output_fp64 = in->output_fp64 != 0;
    rin.no_data_as_zero = in->no_data_as_zero != 0;

    WcsParams& w = rin.wcs;
    w.has_wcs = true;
    std::strncpy(w.ctype1, "RA---TAN-SIP", sizeof(w.ctype1) - 1);
    std::strncpy(w.ctype2, "DEC--TAN-SIP", sizeof(w.ctype2) - 1);
    w.crval[0] = in->crval[0]; w.crval[1] = in->crval[1];
    w.crpix[0] = in->crpix[0]; w.crpix[1] = in->crpix[1];
    w.cd[0] = in->cd[0]; w.cd[1] = in->cd[1];
    w.cd[2] = in->cd[2]; w.cd[3] = in->cd[3];
    // MICROFIX #3: SIP order 正式支持 0~5 (6×6 系数数组), 非法值硬失败,
    // 禁止静默截断 (6×6 系数数组, 非法值硬失败)
    if (in->sip_order < 0 || in->sip_order > 5 ||
        in->sip_ap_order < 0 || in->sip_ap_order > 5) {
        setReverseErr(result, "reverse: SIP order 必须在 [0,5]");
        return 6;
    }
    w.sip.order = (int)in->sip_order;
    w.sip.ap_order = (int)in->sip_ap_order;
    for (int k = 0; k < 36; k++) {
        w.sip.a[k] = in->sip_a[k];  w.sip.b[k] = in->sip_b[k];
        w.sip.ap[k] = in->sip_ap[k]; w.sip.bp[k] = in->sip_bp[k];
    }

    if (in->n_leaf > 0) {
        rin.leaf_ipix.assign(in->leaf_ipix, in->leaf_ipix + in->n_leaf);
        if (in->leaf_signal_f32) {
            rin.leaf_signal_f32.assign(in->leaf_signal_f32,
                                       in->leaf_signal_f32 + in->n_leaf);
        } else if (in->leaf_signal_f64) {
            rin.leaf_signal.assign(in->leaf_signal_f64,
                                   in->leaf_signal_f64 + in->n_leaf);
        }
        if (in->leaf_support) {
            rin.leaf_support.assign(in->leaf_support,
                                    in->leaf_support + in->n_leaf);
        }
    }

    ReverseDrizzle rdz;
    ReverseDrizzleOutput rout;
    std::string err;
    if (!rdz.run(rin, rout, err)) {
        setReverseErr(result, err);
        return 5;
    }

    const size_t npix = (size_t)in->target_width * in->target_height;
    if (rin.output_fp64) {
        std::memcpy(signal_out, rout.signal.data(), npix * sizeof(double));
        std::memcpy(coverage_out, rout.coverage.data(), npix * sizeof(double));
    } else {
        std::memcpy(signal_out, rout.signal_f32.data(), npix * sizeof(float));
        std::memcpy(coverage_out, rout.coverage_f32.data(), npix * sizeof(float));
    }
    result->n_source_leaf = rout.n_source_leaf;
    result->n_target_pixel_touched = rout.n_target_pixel_touched;
    result->n_candidates = rout.n_candidates;
    result->n_overlaps = rout.n_overlaps;
    result->total_signal_in = rout.total_signal_in;
    result->total_signal_out = rout.total_signal_out;
    result->total_covered_area_in = rout.total_covered_area_in;
    result->total_covered_area_out = rout.total_covered_area_out;
    result->n_invalid_ipix = rout.n_invalid_ipix;
    result->n_nonfinite = rout.n_nonfinite;
    result->n_skipped_outside = rout.n_skipped_outside;
    return 0;
}

HP_DRIZZLE_API uint32_t hp_drizzle_reverse_capability(void) {
    return 0x01u | 0x02u | 0x04u | 0x08u | 0x10u | 0x20u;
}

HP_DRIZZLE_API const char* hp_drizzle_reverse_version(void) {
    return "1.0.0";
}

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

    // B5 修复: 从 FitsImage 读取测光校准元数据 (readFits 已解析 PHOTSCAL/PHOTAPPL)
    // PHOTOMETRIC 阶段 (pc_calibrate_simple) 已把 photscal 乘入像素值,
    // drizzle 不再应用 photscal, 仅记录元数据。
    config.apply_photometry          = false;                       // drizzle 不再应用
    config.photometry_applied_upstream = (img.photappl != 0);       // PHOTOMETRIC 阶段已应用
    config.photscal                  = img.photscal;

    // 正式 Stage1 要求 photscal 为正且有限 (FITS 文件应含 PHOTSCAL 关键字)
    if (!std::isfinite(config.photscal) || config.photscal <= 0.0) {
        fprintf(stderr, "[hp_drizzle_api] photscal 非法 (%.6f), FITS 文件缺少 PHOTSCAL 关键字或 "
                        "未经过 PHOTOMETRIC 阶段校准, 正式 Stage1 拒绝生成未校准 ADU HISS\n",
                config.photscal);
        setErrorMsg(result, "photscal 非法 (<=0 或非有限): " + std::to_string(config.photscal) +
                    ", FITS 文件缺少 PHOTSCAL 关键字或未经过 PHOTOMETRIC 阶段校准");
        return 12;
    }

    fprintf(stderr, "[hp_drizzle_api] 测光元数据 photscal=%.6f photappl=%d "
                    "(photometry_applied_upstream=%d)\n",
            config.photscal, img.photappl, (int)config.photometry_applied_upstream);

    // 6. 执行 Drizzle (R11 阶段6: Tile 级累加, 正式路径不恢复全局 leaf map)
    //    FP32: Scalar=float 真 FP32 累计 (阶段7)
    DrizzleEngine engine;
    std::vector<drizzle::TileAccumulatorT<float>> tiles;
    DrizzleStats stats;

    if (!engine.drizzleTiled(img, config, snrPtr, weightPtr, tiles, stats, errMsg)) {
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
    if (!engine.writeHisTilesT<float>(tiles, stats, img.wcs, config, meta, fits_path, hissPath,
                                      nullptr, nullptr, errMsg)) {
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
// run_drizzle_internal - 共享 Drizzle 执行 (parse frame -> drizzle ->
//                        [legacy .hiss] / [HiPS 直写] 输出)
//
// write_hips       : 是否直写 HiPS 产品集 (Drizzle -> AIO, 无 HISS 中转)
// hips_dir         : HiPS 输出根目录 (write_hips 时必需)
// write_legacy_hiss: 是否同时写 legacy .hiss (validation.legacy_hiss_compare)
// output_path      : legacy .hiss 路径 (write_legacy_hiss 时必需)
// ============================================================================
static int run_drizzle_internal(PipelineFrame* frame,
                                int nside, int nested, double pixfrac,
                                const char* output_path,
                                const char* hips_dir,
                                bool write_hips,
                                bool /*write_legacy_hiss*/,
                                HpDrizzleResult* result,
                                int precision_mode)
{
    // 0. V4 G4: actual-buffer trace 状态清理 (env 由 drizzleTiledImpl 内读取)
    drizzle_trace::reset();

    // V18 (G1): Drizzle 内部粗粒度阶段计时（每段一次 clock，低开销，
    // 生产默认打印；逐像素 fine profile 由 drizzle_engine 环境变量门控）
    const auto t_func0 = std::chrono::steady_clock::now();
    auto t_mark = t_func0;
    double prof_parse = 0.0, prof_snr = 0.0, prof_drizzle = 0.0,
           prof_hips = 0.0, prof_hiss = 0.0;
    auto stamp = [&](double& acc) {
        const auto n = std::chrono::steady_clock::now();
        acc += std::chrono::duration<double>(n - t_mark).count();
        t_mark = n;
    };

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

    // 2. 读取 data 块 (支持 FLOAT32[H,W] 和 FLOAT64[H,W])
    //    双精度 ABI (R10): FP64 模式下 data 块为 FLOAT64, 走 drizzle_f64 路径
    //                      FP32 模式下 data 块为 FLOAT32, 走 drizzle 路径 (向后兼容)
    const AioBlock* data_blk = aio_frame_get_block(frame, "data");
    if (!data_blk) {
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: frame 中缺少 'data' 块\n");
        setErrorMsg(result, "frame 中缺少 'data' 块");
        return -4;
    }
    // 解除 FLOAT32 硬限制: 接受 FLOAT32 或 FLOAT64
    if (data_blk->type != AIO_BLOCK_FLOAT32 && data_blk->type != AIO_BLOCK_FLOAT64) {
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: 'data' 块类型非 FLOAT32/FLOAT64 (type=%d)\n",
                (int)data_blk->type);
        setErrorMsg(result, "'data' 块类型非 FLOAT32/FLOAT64 (type=" + std::to_string((int)data_blk->type) + ")");
        return -5;
    }
    bool data_is_f64 = (data_blk->type == AIO_BLOCK_FLOAT64);
    if (data_blk->n_dims < 2) {
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: 'data' 块维度数 < 2 (n_dims=%d)\n",
                data_blk->n_dims);
        setErrorMsg(result, "'data' 块维度数 < 2");
        return -6;
    }
    // B4 修复: Stage1 只接受单色输入, 多通道 data 块 (n_dims >= 3) 硬报错
    if (data_blk->n_dims > 2) {
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: 多通道输入 (n_dims=%d) 不被支持, "
                        "Stage1 只接受单色输入\n", data_blk->n_dims);
        setErrorMsg(result, "多通道输入 (n_dims=" + std::to_string(data_blk->n_dims)
                  + ") 不被支持, Stage1 只接受单色输入");
        return -6;
    }

    int height = data_blk->dims[0];
    int width  = data_blk->dims[1];
    if (width <= 0 || height <= 0) {
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: 'data' 块尺寸非法 (%dx%d)\n", width, height);
        setErrorMsg(result, "'data' 块尺寸非法: " + std::to_string(width) + "x" + std::to_string(height));
        return -7;
    }

    if (!data_blk->data) {
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: 'data' 块数据指针为空\n");
        setErrorMsg(result, "'data' 块数据指针为空");
        return -8;
    }

    fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: data 块 %dx%d %s\n", width, height,
            data_is_f64 ? "float64" : "float32");

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

    // 4. 构造 WCS 参数 + 像素数据
    //    双精度 ABI: 根据 data 块类型填充 pixels (float32) 或 pixels_f64 (float64)
    //    FP64 模式下 pixels_f64 被填充, use_f64=true, drizzle_f64 从此字段读取
    //    FP32 模式下 pixels 被填充, use_f64=false, drizzle 从此字段读取 (向后兼容)
    FitsImage img;
    img.width = width;
    img.height = height;
    img.channels = 1;
    if (data_is_f64) {
        const double* pixels_f64 = (const double*)data_blk->data;
        img.pixels_f64.assign(pixels_f64, pixels_f64 + (size_t)width * height);
        img.use_f64 = true;
    } else {
        const float* pixels_f32 = (const float*)data_blk->data;
        img.pixels.assign(pixels_f32, pixels_f32 + (size_t)width * height);
        img.use_f64 = false;
    }
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
    // V18 (PERF-010): SNR model 控制点用 RAII vector 持有（points 指向
    // vector.data()），禁止手工 malloc/free——生产 HiPS-only 路径
    // legacy_hiss_compare=false 时旧代码 free 不执行 → 每帧泄漏。
    std::vector<HioSnrControlPoint>   snr_pts_f32;
    std::vector<HioSnrControlPointF64> snr_pts_f64;
    HioSnrModel snrModelData = {};       // f32 模型 (writeHis 用, 对齐拷贝)
    HioSnrModelF64 snrModelDataF64 = {}; // f64 模型 (BLOCKER-TYPE-002)
    const HioSnrModel* snrModelPtr = nullptr;
    const HioSnrModelF64* snrModelF64Ptr = nullptr;
    // V4: lineage 字段 (与 snr_model 控制点对齐, 供 HiPS Catalogue 使用)
    std::vector<int64_t> cp_star_id_all;
    std::vector<uint32_t> cp_qf_all;
    std::vector<uint32_t> cp_ps_all;
    // 重建 SNR 的公共部分 (lambda, nPix 由调用点确定)
    auto rebuild_snr = [&](gradient::SnrEvaluator& evaluator,
                           size_t nPix, uint32_t n_points,
                           const double* /*cp_ra*/, const double* /*cp_dec*/) -> void {
        std::vector<double> xy(nPix * 2), radec(nPix * 2);
        for (size_t i = 0; i < nPix; i++) {
            int x = (int)(i % (size_t)width);
            int y = (int)(i / (size_t)width);
            xy[i * 2] = (double)x;
            xy[i * 2 + 1] = (double)y;
        }
        WcsSip wcsBatch(img.wcs);
        wcsBatch.pixelToSkyBatch(xy.data(), (int)nPix, radec.data());
        std::vector<double> qra(nPix), qdec(nPix);
        for (size_t i = 0; i < nPix; i++) {
            qra[i] = radec[i * 2];
            qdec[i] = radec[i * 2 + 1];
        }
        snrRebuilt.resize(nPix);
        evaluator.evaluateBatch(qra.data(), qdec.data(), nPix, snrRebuilt.data());
        snrPtr = snrRebuilt.data();
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: SNR 重建完成 (%zu 像素, %u 控制点)\n",
                nPix, n_points);
    };
    {
        const AioBlock* snr_blk = aio_frame_get_block(frame, "snr_model");
        if (snr_blk && snr_blk->type == AIO_BLOCK_RAW && snr_blk->data && snr_blk->count >= 4) {
            const uint8_t* raw = (const uint8_t*)snr_blk->data;
            size_t raw_size = (size_t)snr_blk->count;
            bool is_v1 = (raw_size >= 28 && std::memcmp(raw, "SNRM", 4) == 0);
            size_t nPix = (size_t)width * height;
            if (is_v1) {
                // 版本化 v1/v2 头: magic4 + version4 + vd1 + res1 + stride2 + n4 + payload8 + cs4
                uint32_t version = 0, n_points = 0, stored_cs = 0;
                uint64_t payload_bytes = 0;
                uint16_t stride = 0;
                uint8_t vd = raw[8];
                std::memcpy(&version, raw + 4, 4);
                std::memcpy(&stride, raw + 10, 2);
                std::memcpy(&n_points, raw + 12, 4);
                std::memcpy(&payload_bytes, raw + 16, 8);
                std::memcpy(&stored_cs, raw + 24, 4);
                size_t expect_stride = (vd == 1) ? ((version == 2) ? 40 : 24)
                                                 : ((version == 2) ? 36 : 20);
                uint64_t expect_payload = (uint64_t)n_points * expect_stride + 24;
                bool header_ok = ((version == 1 || version == 2) && (vd == 0 || vd == 1) &&
                                  stride == expect_stride &&
                                  payload_bytes == expect_payload &&
                                  raw_size >= 28 + payload_bytes);
                if (!header_ok) {
                    fprintf(stderr, "[hp_drizzle_api] snr_model 头非法 (version=%u vd=%u "
                                    "stride=%u payload=%llu n=%u)\n",
                            version, vd, stride, (unsigned long long)payload_bytes, n_points);
                } else {
                    const uint8_t* body = raw + 28;
                    uint32_t cs = 2166136261u;
                    for (size_t i = 0; i < (size_t)payload_bytes; i++) {
                        cs ^= body[i];
                        cs *= 16777619u;
                    }
                    if (cs != stored_cs) {
                        fprintf(stderr, "[hp_drizzle_api] snr_model v1 checksum 不匹配\n");
                    } else if (n_points > 0) {
                        std::vector<double> cp_ra(n_points), cp_dec(n_points);
                        std::vector<double> cp_snr_f64(n_points);
                        cp_star_id_all.assign(n_points, 0);
                        cp_qf_all.assign(n_points, 0);
                        cp_ps_all.assign(n_points, 0);
                        for (uint32_t i = 0; i < n_points; i++) {
                            const uint8_t* pt = body + (size_t)i * expect_stride;
                            std::memcpy(&cp_ra[i], pt, 8);
                            std::memcpy(&cp_dec[i], pt + 8, 8);
                            if (vd == 1) {
                                std::memcpy(&cp_snr_f64[i], pt + 16, 8);
                            } else {
                                float s;
                                std::memcpy(&s, pt + 16, 4);
                                cp_snr_f64[i] = (double)s;
                            }
                            if (version == 2) {
                                // V4 v2 块为 #pragma pack(1) 布局 (G4 修复):
                                //   f64 (vd==1, stride 40): star_id @ +24, qf @ +32, ps @ +36
                                //   f32 (vd==0, stride 36): star_id @ +20, qf @ +28, ps @ +32
                                // 旧代码固定 f64 偏移导致 FP32 路径字段错位
                                const size_t o_sid = (vd == 1) ? 24 : 20;
                                const size_t o_qf  = (vd == 1) ? 32 : 28;
                                const size_t o_ps  = (vd == 1) ? 36 : 32;
                                int64_t sid = 0;
                                uint32_t qf = 0, ps = 0;
                                std::memcpy(&sid, pt + o_sid, 8);
                                std::memcpy(&qf, pt + o_qf, 4);
                                std::memcpy(&ps, pt + o_ps, 4);
                                cp_star_id_all[i] = sid;
                                cp_qf_all[i] = qf;
                                cp_ps_all[i] = ps;
                            }
                        }
                        double snr_phot, median_snr, idw_power;
                        const uint8_t* tail = body + (size_t)n_points * expect_stride;
                        std::memcpy(&snr_phot, tail, 8);
                        std::memcpy(&median_snr, tail + 8, 8);
                        std::memcpy(&idw_power, tail + 16, 8);
                        if (vd == 1) {
                            // 对齐拷贝到 HioSnrModelF64 (writeHis 用, 避免未对齐读取)
                            snrModelDataF64.n_points = n_points;
                            snr_pts_f64.resize(n_points);
                            if (!snr_pts_f64.empty()) {
                                snrModelDataF64.points = snr_pts_f64.data();
                                for (uint32_t i = 0; i < n_points; i++) {
                                    snrModelDataF64.points[i].ra = cp_ra[i];
                                    snrModelDataF64.points[i].dec = cp_dec[i];
                                    snrModelDataF64.points[i].snr_psf = cp_snr_f64[i];
                                }
                                snrModelDataF64.snr_phot = snr_phot;
                                snrModelDataF64.median_snr = median_snr;
                                snrModelDataF64.idw_power = idw_power;
                                snrModelF64Ptr = &snrModelDataF64;
                                gradient::SnrEvaluator evaluator;
                                if (evaluator.buildF64(n_points, cp_ra.data(), cp_dec.data(),
                                                       cp_snr_f64.data(), snr_phot,
                                                       median_snr, idw_power)) {
                                    rebuild_snr(evaluator, nPix, n_points,
                                                cp_ra.data(), cp_dec.data());
                                }
                            }
                        } else {
                            snrModelData.n_points = n_points;
                            snr_pts_f32.resize(n_points);
                            if (!snr_pts_f32.empty()) {
                                snrModelData.points = snr_pts_f32.data();
                                for (uint32_t i = 0; i < n_points; i++) {
                                    snrModelData.points[i].ra = cp_ra[i];
                                    snrModelData.points[i].dec = cp_dec[i];
                                    snrModelData.points[i].snr_psf = (float)cp_snr_f64[i];
                                }
                                snrModelData.snr_phot = snr_phot;
                                snrModelData.median_snr = median_snr;
                                snrModelData.idw_power = idw_power;
                                snrModelPtr = &snrModelData;
                                std::vector<float> cp_snr(n_points);
                                for (uint32_t i = 0; i < n_points; i++)
                                    cp_snr[i] = (float)cp_snr_f64[i];
                                gradient::SnrEvaluator evaluator;
                                if (evaluator.build(n_points, cp_ra.data(), cp_dec.data(),
                                                    cp_snr.data(), snr_phot,
                                                    median_snr, idw_power)) {
                                    rebuild_snr(evaluator, nPix, n_points,
                                                cp_ra.data(), cp_dec.data());
                                }
                            }
                        }
                        fprintf(stderr, "[hp_drizzle_api] snr_model v%u 加载: n=%u dtype=%u "
                                        "snr_phot=%.4f median=%.4f power=%.2f\n",
                                version, n_points, vd, snr_phot, median_snr, idw_power);
                    }
                }
            } else {
                // 旧 v0 兼容: [n_points u32][points n×20B][snr_phot f64][median f64][idw f64]
                // 全部 memcpy 安全读取 (BLOCKER-STRUCT-001: 禁止未对齐强转解引用)
                uint32_t n_points = 0;
                std::memcpy(&n_points, raw, 4);
                size_t expected = 4 + (size_t)n_points * 20 + 24;
                if (n_points == 0 || raw_size < expected) {
                    fprintf(stderr, "[hp_drizzle_api] snr_model v0 块不完整 (count=%lld)\n",
                            (long long)snr_blk->count);
                } else {
                    std::vector<double> cp_ra(n_points), cp_dec(n_points);
                    std::vector<float> cp_snr(n_points);
                    cp_star_id_all.assign(n_points, 0);
                    cp_qf_all.assign(n_points, 0);
                    cp_ps_all.assign(n_points, 0);
                    for (uint32_t i = 0; i < n_points; i++) {
                        const uint8_t* pt = raw + 4 + (size_t)i * 20;
                        std::memcpy(&cp_ra[i], pt, 8);
                        std::memcpy(&cp_dec[i], pt + 8, 8);
                        std::memcpy(&cp_snr[i], pt + 16, 4);
                    }
                    double snr_phot, median_snr, idw_power;
                    const uint8_t* tail = raw + 4 + (size_t)n_points * 20;
                    std::memcpy(&snr_phot, tail, 8);
                    std::memcpy(&median_snr, tail + 8, 8);
                    std::memcpy(&idw_power, tail + 16, 8);
                    snrModelData.n_points = n_points;
                    snr_pts_f32.resize(n_points);
                    if (!snr_pts_f32.empty()) {
                        snrModelData.points = snr_pts_f32.data();
                        for (uint32_t i = 0; i < n_points; i++) {
                            snrModelData.points[i].ra = cp_ra[i];
                            snrModelData.points[i].dec = cp_dec[i];
                            snrModelData.points[i].snr_psf = cp_snr[i];
                        }
                        snrModelData.snr_phot = snr_phot;
                        snrModelData.median_snr = median_snr;
                        snrModelData.idw_power = idw_power;
                        snrModelPtr = &snrModelData;
                        gradient::SnrEvaluator evaluator;
                        if (evaluator.build(n_points, cp_ra.data(), cp_dec.data(),
                                            cp_snr.data(), snr_phot,
                                            median_snr, idw_power)) {
                            rebuild_snr(evaluator, nPix, n_points,
                                        cp_ra.data(), cp_dec.data());
                        }
                    }
                }
            }
        } else {
            fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: 无 snr_model 块, 使用 nullptr (snr=1.0)\n");
        }
    }

    // Phase1 Final Signoff V4: 收集 SNR 控制点 (HiPS Catalogue 用),
    // star_id/quality_flags/photometric_status 来自 snr_model v2 块
    // (禁止 i+1 重新编号)
    std::vector<AioHipsSnrPoint> snr_pts;
    if (snrModelPtr && snrModelPtr->points) {
        snr_pts.reserve(snrModelPtr->n_points);
        for (uint32_t i = 0; i < snrModelPtr->n_points; i++) {
            AioHipsSnrPoint pt;
            pt.ra_deg = snrModelPtr->points[i].ra;
            pt.dec_deg = snrModelPtr->points[i].dec;
            pt.snr = snrModelPtr->points[i].snr_psf;
            pt.star_id = (i < cp_star_id_all.size()) ? cp_star_id_all[i] : 0;
            pt.quality_flags = (i < cp_qf_all.size()) ? cp_qf_all[i] : 0u;
            pt.photometric_status = (i < cp_ps_all.size()) ? cp_ps_all[i] : 0u;
            snr_pts.push_back(pt);
        }
    } else if (snrModelF64Ptr && snrModelF64Ptr->points) {
        snr_pts.reserve(snrModelF64Ptr->n_points);
        for (uint32_t i = 0; i < snrModelF64Ptr->n_points; i++) {
            AioHipsSnrPoint pt;
            pt.ra_deg = snrModelF64Ptr->points[i].ra;
            pt.dec_deg = snrModelF64Ptr->points[i].dec;
            pt.snr = snrModelF64Ptr->points[i].snr_psf;
            pt.star_id = (i < cp_star_id_all.size()) ? cp_star_id_all[i] : 0;
            pt.quality_flags = (i < cp_qf_all.size()) ? cp_qf_all[i] : 0u;
            pt.photometric_status = (i < cp_ps_all.size()) ? cp_ps_all[i] : 0u;
            snr_pts.push_back(pt);
        }
    }
    fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: SNR 控制点 %zu 个 (HiPS Catalogue)\n",
            snr_pts.size());
    stamp(prof_snr);   // V18: SNR 重建结束

    // 6. 构造 DrizzleConfig
    DrizzleConfig config;
    config.nside   = nside;
    config.nested  = nested ? true : false;
    config.pixfrac = pixfrac;
    // HiPS 直写要求 depth=9 分组 (512x512 叶 tile 与 HiPS NorderK tile 1:1)
    if (write_hips) config.tile_depth = 9;

    // B5 修复: 从 header KV 读取测光校准信息
    // PHOTOMETRIC 阶段 (pc_calibrate_simple) 已把 photscal 乘入像素值,
    // drizzle 不再应用 photscal (避免双重缩放), 仅记录元数据。
    double photscal = aio_frame_kv_get_double(frame, "header", "PHOTSCAL", 0.0);
    const char* photappl_str = aio_frame_kv_get(frame, "header", "PHOTAPPL");
    int photappl = photappl_str ? std::atoi(photappl_str) : 0;
    config.apply_photometry          = false;                  // drizzle 不再应用
    config.photometry_applied_upstream = (photappl != 0);      // PHOTOMETRIC 阶段已应用
    config.photscal                  = photscal;

    // photscal 必须为正且有限 (正式 Stage1 流水线必经 PHOTOMETRIC 阶段)
    if (!std::isfinite(photscal) || photscal <= 0.0) {
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: photscal 非法 (%.6f), "
                        "正式 Stage1 要求 PHOTOMETRIC 阶段已应用测光校准\n", photscal);
        setErrorMsg(result, "photscal 非法 (<=0 或非有限): " + std::to_string(photscal) +
                    ", 正式 Stage1 要求 PHOTOMETRIC 阶段已应用测光校准");
        return -12;
    }

    fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: 测光元数据 photscal=%.6f photappl=%d "
                    "(photometry_applied_upstream=%d)\n",
            photscal, photappl, (int)config.photometry_applied_upstream);

    // R10: 精度模式设置
    //   precision_mode 参数优先 (0=FP32, 1=FP64)
    //   若参数为 -1 (未指定), 从 header KV "PRECISION" 读取 (向后兼容)
    //   写入 HISS metadata precision_mode/signal_dtype 字段
    if (precision_mode == 0 || precision_mode == 1) {
        config.precision_mode = (uint8_t)precision_mode;
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: precision=%s (参数指定)\n",
                precision_mode == 1 ? "FP64" : "FP32");
    } else {
        const char* prec_str = aio_frame_kv_get(frame, "header", "PRECISION");
        if (prec_str) {
            if (std::strcmp(prec_str, "fp64") == 0) {
                config.precision_mode = 1;
                fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: precision=FP64 (header KV PRECISION=fp64)\n");
            } else {
                config.precision_mode = 0;
                fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: precision=FP32 (header KV PRECISION='%s')\n", prec_str);
            }
        } else {
            config.precision_mode = 0;
            fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: header 无 PRECISION 字段, 默认 FP32\n");
        }
    }

    // 5.6 V19 (P1-003/DRZ-014): 读取 "variance" 块 (逐像素方差图,
    //     FLOAT32/64 [H,W], 由 SNR 阶段 NoiseWeightModelV1 填充)
    //     → 方差传播 sumVarNum += v_j × w_jp²
    const float* variancePtr = nullptr;
    std::vector<float> varianceConv;
    {
        const AioBlock* vblk = aio_frame_get_block(frame, "variance");
        if (vblk && vblk->data && vblk->count == (int64_t)width * (int64_t)height) {
            if (vblk->type == AIO_BLOCK_FLOAT32) {
                variancePtr = static_cast<const float*>(vblk->data);
                fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: variance 块 FLOAT32 [%dx%d]\n",
                        width, height);
            } else if (vblk->type == AIO_BLOCK_FLOAT64) {
                const double* vd = static_cast<const double*>(vblk->data);
                varianceConv.assign((size_t)width * (size_t)height, 0.0f);
                for (size_t i = 0; i < varianceConv.size(); ++i)
                    varianceConv[i] = (float)vd[i];
                variancePtr = varianceConv.data();
                fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: variance 块 FLOAT64 [%dx%d] → FLOAT32 转换\n",
                        width, height);
            } else {
                fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: variance 块类型 %d 不支持, 跳过方差传播\n",
                        (int)vblk->type);
            }
        } else if (vblk) {
            fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: variance 块尺寸不匹配 (count=%lld, 期望 %lld), 跳过方差传播\n",
                    (long long)(vblk ? vblk->count : 0),
                    (long long)width * (long long)height);
        }
    }

    stamp(prof_parse);  // V18: parse frame + WCS + config 结束

    // 7. 执行 Drizzle (R11 阶段6: Tile 级累加, 正式路径不恢复全局 leaf map)
    //    双精度 ABI: 根据 data 块类型选择 drizzleTiled (FP32) 或 drizzleTiled_f64 (FP64)
    //    FP64 模式: 从 img.pixels_f64 (double) 读取像素, 不降级到 float32
    DrizzleEngine engine;
    std::vector<drizzle::TileAccumulatorT<float>> tiles_f32;
    std::vector<drizzle::TileAccumulatorT<double>> tiles_f64;
    DrizzleStats stats;
    std::string errMsg;

    bool drizzle_ok;
    if (img.use_f64) {
        drizzle_ok = engine.drizzleTiled_f64(img, config, snrPtr, nullptr,
                                             variancePtr, tiles_f64, stats, errMsg);
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: 调用 drizzleTiled_f64 (FP64 路径)\n");
    } else {
        drizzle_ok = engine.drizzleTiled(img, config, snrPtr, nullptr,
                                         variancePtr, tiles_f32, stats, errMsg);
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: 调用 drizzleTiled (FP32 路径)\n");
    }

    if (!drizzle_ok) {
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: Drizzle 失败: %s\n", errMsg.c_str());
        setErrorMsg(result, "Drizzle 失败: " + errMsg);
        return -10;
    }

    fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: Drizzle 完成 (%lld 源像素 → %lld HEALPix 像素, 耗时 %.3fs)\n",
            (long long)stats.nSourcePixels, (long long)stats.nHealpixPixels, stats.elapsedSec);
    stamp(prof_drizzle);  // V18: Drizzle kernel 结束

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

        bool write_ok = img.use_f64
            ? engine.writeHisTilesT<double>(tiles_f64, stats, img.wcs, config, meta,
                                            sourcePath, hissPath, snrModelPtr,
                                            snrModelF64Ptr, errMsg)
            : engine.writeHisTilesT<float>(tiles_f32, stats, img.wcs, config, meta,
                                           sourcePath, hissPath, snrModelPtr,
                                           nullptr, errMsg);
        if (!write_ok) {
            fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: 写入 .hiss 失败: %s\n", errMsg.c_str());
            setErrorMsg(result, "写入 .hiss 失败: " + errMsg);
            return -11;
        }
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: .hiss 已写入 %s\n", hissPath.c_str());
        stamp(prof_hiss);  // V18: legacy .hiss 写入结束
    }

    // 8.5 Phase1 Final Closure V3: HiPS 直写 (Drizzle -> AIO, 无 HISS 中转)
    if (write_hips) {
        if (!hips_dir || !hips_dir[0]) {
            fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: write_hips 但 hips_dir 为空\n");
            setErrorMsg(result, "write_hips 但 hips_dir 为空");
            return -12;
        }
        DrizzleMeta meta;
        const char* filter_str = aio_frame_kv_get(frame, "header", "FILTER");
        if (filter_str) meta.filter = filter_str;
        const char* exptime_str = aio_frame_kv_get(frame, "header", "EXPTIME");
        if (exptime_str) meta.exposure_s = std::atof(exptime_str);
        const char* dateobs_str = aio_frame_kv_get(frame, "header", "DATE-OBS");
        if (dateobs_str) meta.obs_time = dateobs_str;

        bool hips_ok = img.use_f64
            ? write_hips_direct<double>(tiles_f64, config, meta, hips_dir, snr_pts,
                                        variancePtr ? 1 : 0, errMsg)
            : write_hips_direct<float>(tiles_f32, config, meta, hips_dir, snr_pts,
                                       variancePtr ? 1 : 0, errMsg);
        if (!hips_ok) {
            fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: HiPS 直写失败: %s\n", errMsg.c_str());
            setErrorMsg(result, "HiPS 直写失败: " + errMsg);
            return -13;
        }
        fprintf(stderr, "[hp_drizzle_api] hp_drizzle_run: HiPS 已直写 %s (无 HISS 中转)\n",
                hips_dir);
        // V19 (DRIZZLE_OPTIMIZATION): 操作计数证据
        {
            const std::string ops_path = std::string(hips_dir) + "/operation_counts.json";
            FILE* f = std::fopen(ops_path.c_str(), "wb");
            if (f) {
                const double cand_eff = stats.op_candidates > 0
                    ? (double)stats.op_true_overlaps / (double)stats.op_candidates
                    : 0.0;
                std::fprintf(f,
                    "{\n"
                    "  \"format\": \"astrocs-drizzle-operation-counts-v1\",\n"
                    "  \"nside\": %d,\n"
                    "  \"source_pixels\": %lld,\n"
                    "  \"candidates\": %lld,\n"
                    "  \"true_overlaps\": %lld,\n"
                    "  \"quick_rejects\": %lld,\n"
                    "  \"pix2radec_calls\": %lld,\n"
                    "  \"boundary_builds\": %lld,\n"
                    "  \"geometry_builds\": %lld,\n"
                    "  \"spherical_overlap_calls\": %lld,\n"
                    "  \"tile_lookups\": %lld,\n"
                    "  \"hot_loop_heap_allocations\": %lld,\n"
                    "  \"candidate_efficiency\": %.6f,\n"
                    "  \"overlaps_per_source_pixel\": %.6f\n"
                    "}\n",
                    stats.nside,
                    (long long)stats.op_source_pixels,
                    (long long)stats.op_candidates,
                    (long long)stats.op_true_overlaps,
                    (long long)stats.op_quick_rejects,
                    (long long)stats.op_pix2radec,
                    (long long)stats.op_boundary_builds,
                    (long long)stats.op_geometry_builds,
                    (long long)stats.op_sh_calls,
                    (long long)stats.op_tile_lookups,
                    (long long)stats.op_heap_allocations,
                    cand_eff,
                    stats.op_source_pixels > 0
                        ? (double)stats.op_true_overlaps /
                              (double)stats.op_source_pixels : 0.0);
                std::fclose(f);
                std::fprintf(stderr, "[hp_drizzle_api] 操作计数已写 %s\n",
                             ops_path.c_str());
            }
        }
        stamp(prof_hips);  // V18: HiPS 直写结束
    }

    // V18 (G1): Drizzle 内部阶段计时汇总（exclusive，粗粒度）
    fprintf(stderr,
            "[drizzle_profile] parse_frame=%.3fs snr_rebuild=%.3fs "
            "drizzle_run=%.3fs hips_write=%.3fs legacy_hiss=%.3fs total=%.3fs\n",
            prof_parse, prof_snr, prof_drizzle, prof_hips, prof_hiss,
            std::chrono::duration<double>(
                std::chrono::steady_clock::now() - t_func0).count());

    // 9. 填充结果
    result->n_healpix_pixels = stats.nHealpixPixels;
    result->n_source_pixels  = stats.nSourcePixels;
    result->nside            = stats.nside;
    result->nested           = stats.nested ? 1 : 0;
    result->pixfrac          = config.pixfrac;
    result->elapsed_sec      = stats.elapsedSec;

    // V4 G4: 清理 trace 状态 (文件已由 drizzleTiledImpl 写出)
    drizzle_trace::reset();

    return 0;
}

// ============================================================================
// hp_drizzle_run - 正式 API (兼容旧签名): FITS 帧 -> .hiss
// Phase1 V3 后生产末端为 HiPS; .hiss 仅 validation.legacy_hiss_compare 时使用。
// ============================================================================
HP_DRIZZLE_API int hp_drizzle_run(PipelineFrame* frame,
                                   int nside, int nested, double pixfrac,
                                   const char* output_path,
                                   HpDrizzleResult* result,
                                   int precision_mode)
{
    return run_drizzle_internal(frame, nside, nested, pixfrac,
                                output_path, nullptr,
                                /*write_hips=*/false,
                                /*write_legacy_hiss=*/(output_path && output_path[0] != '\0'),
                                result, precision_mode);
}

// ============================================================================
// hp_drizzle_run_hips - Phase1 Final Closure V3 正式末端:
//   Drizzle TileAccumulator -> AIO HiPS 直写 (无 HISS 中转)
// hips_dir        : HiPS 产品集根目录 (signal/support/snr 子产品)
// legacy_hiss_path: 可选 legacy .hiss 路径 (nullptr=不写, 仅 validation 用)
// ============================================================================
HP_DRIZZLE_API int hp_drizzle_run_hips(PipelineFrame* frame,
                                       int nside, int nested, double pixfrac,
                                       const char* hips_dir,
                                       const char* legacy_hiss_path,
                                       HpDrizzleResult* result,
                                       int precision_mode)
{
    return run_drizzle_internal(frame, nside, nested, pixfrac,
                                legacy_hiss_path, hips_dir,
                                /*write_hips=*/true,
                                /*write_legacy_hiss=*/(legacy_hiss_path && legacy_hiss_path[0] != '\0'),
                                result, precision_mode);
}
