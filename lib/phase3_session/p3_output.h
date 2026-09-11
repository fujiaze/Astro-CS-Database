// lib/phase3_session/p3_output.h — Phase3 输出 FITS 原子写 (ALG-P3-004) — P3-004
// 覆盖: 原子写(tmp+rename)、BITPIX=-32/BSCALE=1/BZERO=0/BUNIT、WCS 关键字、
// HISTORY provenance(源 hips/order_sel/sampler/软件版本/manifest hash)、
// FITS checksum、失败清理(取消→不落盘)、独立 FITS/WCS reader 重开验证。
#ifndef ASTROCS_P3_OUTPUT_H
#define ASTROCS_P3_OUTPUT_H

#include <cstdint>
#include <string>

#include "p3_wcs.h"

namespace astrocs::phase3 {

struct P3Provenance {
    const char* hips_id;            // ivo:// 标识
    const char* manifest_hash;      // 源 manifest 哈希(hex)
    const int* missing_tiles;       // 缺 tile ipix 数组
    int missing_count;
    const char* software_version;   // 版本串
    const char* run_id;             // 本次 run
    const char* order_sel_used;     // 阶串
    const char* sampler_used;       // "nearest"|"bilinear"
    /* DATA-P3-UNC-001 §30.4 (DATA-UNC-001 2026-09-09 冻结, 宪章 §7.1/§7.3 上位):
     * uncertainty 子产品来源 ("variance"|"ivar"|NULL=unavailable) 与覆盖不一致
     * 像素计数 (leaf signal 有限而 u 缺失 → 输出 NaN + provenance 计数不中断)。 */
    const char* uncertainty_source;
    long uncertainty_missing_pixels;
};

struct P3OutputResult {
    char sha256[65];                // 输出文件哈希(hex, 小写); 仅在完整读取成功时填写
    int coverage_ok;                // 1=coverage 头/数据一致
    int reopen_ok;                  // 1=独立 reader 重开成功且数据回环一致
    long covered_px;
    long total_px;
};

enum P3OutputStatus {
    P3_OUT_OK = 0,
    P3_OUT_PARAM = 1,
    P3_OUT_IO = 2,
    P3_OUT_CANCELLED = 3
};

/* 原子写 S+C 合成 FITS(主 HDU=signal, 扩展=coverage 二值):
 * 写入 <dir>/.<base>.<pid>.tmp → flush(cfitsio 缓冲全部写出) → fsync(fd) →
 * rename 到 output_path (IO_003 §4: 关闭/fsync → sha256 → 原子 rename);
 * 取消/失败 → 删除 tmp/产物, 不留完整假文件, 也不发布无完整性锚的输出。 */
P3OutputStatus p3_output_write_atomic(const float* signal, const float* coverage,
                                      int width, int height,
                                      const P3WcsDescriptor* wcs,
                                      const char* bunit,
                                      const char* output_path,
                                      const P3Provenance* prov,
                                      int bitpix,             // -32|-64 真实决定 buffer
                                      int cancelled_at_row,   // -1=不取消
                                      P3OutputResult* result);

/* 独立重开: 读回 header 数字+数据回环(用 fits_read_file)并重算 checksum。
 * sha256 计算失败(文件不可读等) → P3_OUT_IO, result 不携带假哈希。 */
P3OutputStatus p3_output_verify(const char* output_path, const P3WcsDescriptor* wcs,
                                const float* signal, const float* coverage,
                                int width, int height,
                                P3OutputResult* result);

/* ── 不确定度扩展面 (DATA-P3-UNC-001 §30.4; DATA_SEMANTICS §27.2 目标态行) ──
 * variance/ivar 同时非 NULL → 原子发布序内追加 EXTNAME="VARIANCE"/"IVAR"
 * 扩展 HDU (BITPIX 同主 HDU; BUNIT=<signal BUNIT>^2 与 1/(<BUNIT>^2);
 * DATASUM 逐 HDU 同 COVERAGE 模式), 单边 NULL → P3_OUT_PARAM (成对要求);
 * 双 NULL → 与旧签名完全同面 (unavailable: 无占位 HDU)。
 * verify_ex: 平面非 NULL → 校验 HDU 存在+尺寸+逐像素回环(NaN==NaN 同态);
 * 平面 NULL → 校验无 VARIANCE HDU (双向防: available 静默缺 HDU /
 * unavailable 静默占位)。 */
P3OutputStatus p3_output_write_atomic_ex(const float* signal, const float* coverage,
                                         const float* variance, const float* ivar,
                                         int width, int height,
                                         const P3WcsDescriptor* wcs,
                                         const char* bunit,
                                         const char* output_path,
                                         const P3Provenance* prov,
                                         int bitpix,
                                         int cancelled_at_row,
                                         P3OutputResult* result);

P3OutputStatus p3_output_verify_ex(const char* output_path,
                                   const P3WcsDescriptor* wcs,
                                   const float* signal, const float* coverage,
                                   const float* variance, const float* ivar,
                                   int width, int height,
                                   P3OutputResult* result);

}  // namespace astrocs::phase3

#endif  // ASTROCS_P3_OUTPUT_H
