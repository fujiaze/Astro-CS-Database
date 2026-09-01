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
};

struct P3OutputResult {
    char sha256[65];                // 输出文件哈希(hex, 小写)
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
 * 写入 <dir>/.<base>.<pid>.tmp → fsync → rename 到 output_path;
 * 取消/失败 → 删除 tmp, 不留完整假文件。 */
P3OutputStatus p3_output_write_atomic(const float* signal, const float* coverage,
                                      int width, int height,
                                      const P3WcsDescriptor* wcs,
                                      const char* bunit,
                                      const char* output_path,
                                      const P3Provenance* prov,
                                      int bitpix,             // -32|-64 真实决定 buffer
                                      int cancelled_at_row,   // -1=不取消
                                      P3OutputResult* result);

/* 独立重开: 读回 header 数字+数据回环(用 fits_read_file)并重算 checksum。 */
P3OutputStatus p3_output_verify(const char* output_path, const P3WcsDescriptor* wcs,
                                const float* signal, const float* coverage,
                                int width, int height,
                                P3OutputResult* result);

}  // namespace astrocs::phase3

#endif  // ASTROCS_P3_OUTPUT_H
