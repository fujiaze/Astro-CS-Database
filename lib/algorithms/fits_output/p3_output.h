// lib/algorithms/fits_output/p3_output.h — Phase3 输出 FITS 原子写 (ALG-P3-004) — P3-004
// 原址 lib/phase3_session/p3_output.h, 按 ASTROCS_DESIGN
// §7.1「fits_output」行迁入本模块; 命名空间 astrocs::phase3 与公共符号名零改动。
// 覆盖: 原子写(tmp+rename)、BITPIX=-32/BSCALE=1/BZERO=0/BUNIT、WCS 关键字、
// HISTORY provenance(源 hips/order_sel/sampler/软件版本/manifest hash)、
// FITS checksum、失败清理(取消→不落盘)、独立 FITS/WCS reader 重开验证。
#ifndef ASTROCS_P3_OUTPUT_H
#define ASTROCS_P3_OUTPUT_H

#include <cstdint>
#include <string>

// p3_wcs.h 在 lib/algorithms/projection/, 经本 target
// 的 include 面 (astrocs_p3_projection_wcs PUBLIC) 解析 —— 不再用相对路径 (迁址即失效)。
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

/* 最近一次失败的可读原因（进程内静态；仅当日志/错误消息用，禁作状态判断）。
 * P3-STREAM-01：流式 API 与整幅 API 共用同一错误面。 */
const char* p3_output_last_error();

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

/* ── 子块流式写/校验（P3-STREAM-01；ASTROCS_DESIGN §8.3 export「子块流式」）────
 * 与上列整幅 API **同产品语义、同字节布局**，差别只在驻留面：调用方按子块
 * （矩形区间）喂像素，本类不再要求整幅平面在内存里。
 *   · 写：open 建临时对象并在**首像素写出前**完成 PRIMARY 头组装（合同③）；
 *         begin_hdu 逐个 HDU 建（PRIMARY=signal → COVERAGE → VARIANCE → IVAR，
 *         与整幅路径同序同关键字）；write_block 经 cfitsio fits_write_subset
 *         把子块写进该 HDU 的数据区；end_hdu 写该 HDU 的 DATASUM/CHECKSUM；
 *         publish 走 flush → close → fsync → 原子 rename → sha256（IO_003 §4）。
 *   · 校验：open 独立重开并逐项对拍 WCS/尺寸/HDU 存在性；check_block 读回同一
 *         矩形区间并与期望子块逐像素对拍（NaN==NaN 同态）；close 汇总
 *         reopen_ok/coverage_ok 并重算 sha256。
 * 内存上界 = 单子块（与总图大小无关）；任何失败都不发布、不留半成品。 */
class P3FitsStream {
public:
    P3FitsStream();
    ~P3FitsStream();
    P3FitsStream(const P3FitsStream&) = delete;
    P3FitsStream& operator=(const P3FitsStream&) = delete;

    // 建临时对象 + PRIMARY 头（WCS/BUNIT/provenance/HISTORY），不写像素。
    P3OutputStatus open(const char* output_path, const P3WcsDescriptor* wcs,
                        int width, int height, int bitpix, const char* bunit,
                        const P3Provenance* prov);
    // 进入第 plane 个 HDU：0=PRIMARY(signal，open 已建) / 1=COVERAGE /
    // 2=VARIANCE / 3=IVAR（2/3 成对，BUNIT 走二次律）。
    P3OutputStatus begin_hdu(int plane);
    // 写一个子块（x0/y0 为 0 基，w/h 为子块尺寸，data 为 w×h 行主序 f32）。
    P3OutputStatus write_block(int x0, int y0, int w, int h, const float* data);
    // 收尾当前 HDU：DATASUM/CHECKSUM。
    P3OutputStatus end_hdu();
    // 原子发布：flush → close → fsync → rename → sha256（失败删除产物）。
    P3OutputStatus publish(P3OutputResult* result);
    // 取消/失败：关闭并删除临时对象，**不发布**。幂等。
    void abort();
    bool published() const { return published_; }

private:
    struct Impl;
    Impl* impl_ = nullptr;
    bool published_ = false;
};

class P3FitsVerifyStream {
public:
    P3FitsVerifyStream();
    ~P3FitsVerifyStream();
    P3FitsVerifyStream(const P3FitsVerifyStream&) = delete;
    P3FitsVerifyStream& operator=(const P3FitsVerifyStream&) = delete;

    // 独立重开：尺寸/HDU 面/WCS 关键字逐项对拍（缺 HDU 或占位 HDU 均判失败）。
    P3OutputStatus open(const char* output_path, const P3WcsDescriptor* wcs,
                        int width, int height, bool has_uncertainty);
    // 对拍一个子块：plane 0=signal / 1=COVERAGE / 2=VARIANCE / 3=IVAR。
    P3OutputStatus check_block(int plane, int x0, int y0, int w, int h,
                               const float* expected);
    // 汇总（reopen_ok/coverage_ok/covered_px/total_px/sha256）并关闭。
    P3OutputStatus close(P3OutputResult* result);

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

}  // namespace astrocs::phase3

#endif  // ASTROCS_P3_OUTPUT_H
