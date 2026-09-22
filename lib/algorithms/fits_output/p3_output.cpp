// lib/algorithms/fits_output/p3_output.cpp — 输出原子写/校验 (ALG-P3-004) — P3-004
// W4-A9 批次 3 (2026-09-17): 原址 lib/phase3_session/p3_output.cpp, 按 ASTROCS_DESIGN
// §7.1「fits_output」行迁入本模块; 源逐字节等价 (仅头注 | include 面改锚)。
#include "p3_output.h"

#if defined(_WIN32)
// MSVC 无 unistd.h; 以 _ 前缀 CRT 提供 pid 别名。
// FIX-201: open/fsync/close/unlink 别名已删除 —— 这些文件系统原语现全部
// 经 aio (aio_atomic_file.h), 本模块不再直接调用 (死宏清理)。
#include <windows.h>
#include <io.h>
#include <process.h>
#include <direct.h>
#ifndef getpid
#define getpid _getpid
#endif
#else
#include <unistd.h>
#endif

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "aio_fits.h"
#include "astro_image_io.h"
#include "fitsio.h"
// W4-A9 批次 3: 原相对路径 ../infrastructure/aio/src/... 只在旧址 (lib/phase3_session/)
// 成立; 该目录已由 astrocs_aio 的 PUBLIC include 面提供 ⇒ 扁平引用 (迁址无关)。
#include "aio_cfitsio_mutex.h"
#include "sha256.h"
// FIX-201 (ASTROCS_DESIGN §9「aio 是文件级唯一 I/O 边界」+ §9.73 裁决 U5):
// 本模块**不再**自持文件系统原语 —— 临时文件/fsync/原子 rename/删除经 aio
// 机制原语 (aio_atomic_file.h), 文件内容摘要经 aio 摘要原语
// (aio_file_io.h)。二者均为 aio 内唯一实现 (header-only 机制面)。
#include "aio_atomic_file.h"
#include "aio_file_io.h"

#include <memory>
#include <vector>

namespace astrocs::phase3 {

namespace {
std::string g_last_err;

// B2-A9: 标准 FITS 校验和 = CFITSIO fits_write_chksum（IAU FITS 4.0
// §4.4.2.5 的 1 补码 32-bit 累加，DATASUM 十进制字符串 + CHECKSUM 16 字符）
// 逐 HDU 写出并由 cfitsio 自行归属。旧实现自算 "little-endian 无进位字节和"
// 并以 TINT 整数写入保留字 DATASUM，是非法关键字值（astropy checksum=True
// 报 Datasum verification failed），已删除。
// FIX-402 (FZ-P3-BUNIT-QUADRATIC / docs/contracts/DATA_SEMANTICS.md §31.1 §1):
// variance BUNIT = (signal BUNIT)^2, ivar = 1/variance —— 用**冻结单位表的 canonical
// 串**（ADU^a/px^p 幂次代数），禁朴素字符串拼接（"ADU/sr" + "^2" = "ADU/sr^2"
// 既非 canonical 也不可判）。解析失败 → false（调用方显式拒绝，禁写出非二次律 BUNIT）。
bool bunit_square_canonical(const std::string& signal, std::string* variance,
                            std::string* ivar) {
    // 解析 ADU^a / px^p（幂次可省略=1、可带负号；"1" = 0/0）
    auto parse_pow = [](const std::string& s, const char* base, int* out) -> bool {
        if (s.rfind(base, 0) != 0) return false;
        const size_t bl = std::strlen(base);
        if (s.size() == bl) { *out = 1; return true; }
        if (s.size() > bl + 1 && s[bl] == '^') {
            try {
                size_t used = 0;
                const int v = std::stoi(s.substr(bl + 1), &used);
                if (used != s.size() - bl - 1) return false;
                *out = v;
                return true;
            } catch (...) { return false; }
        }
        return false;
    };
    // 冻结表书写形式（分母正幂次）: {+1,-2} → "ADU/sr"; {+2,-4} → "ADU^2/sr^2";
    // {-2,+4} → "sr^2/ADU^2"; {+1,0} → "ADU"; {-2,0} → "ADU^-2"。
    auto canon = [](int adu, int px) -> std::string {
        if (adu == 0 && px == 0) return "1";
        if (adu > 0) {
            std::string s = "ADU";
            if (adu != 1) s += "^" + std::to_string(adu);
            if (px != 0) {
                s += "/px";
                if (px != -1) s += "^" + std::to_string(-px);
            }
            return s;
        }
        if (adu < 0) {
            if (px != 0) {
                std::string s = "px";
                if (px != 1) s += "^" + std::to_string(px);
                s += "/ADU";
                if (adu != -1) s += "^" + std::to_string(-adu);
                return s;
            }
            // 纯 ADU 负幂次: 冻结表写带符号指数（flux ivar = "ADU^-2"）
            return std::string("ADU^-") + std::to_string(-adu);
        }
        std::string s = "px";
        if (px != 1) s += "^" + std::to_string(px);
        return s;
    };
    std::string t;
    for (char c : signal) {
        if (c != ' ' && c != '\t') t += c;
    }
    if (t.empty()) return false;
    int adu = 0, px = 0;
    if (t == "1") {
        adu = 0; px = 0;
    } else {
        const size_t slash = t.find('/');
        const std::string left = (slash == std::string::npos) ? t : t.substr(0, slash);
        if (!parse_pow(left, "ADU", &adu)) return false;
        if (slash != std::string::npos) {
            int written = 0;
            if (!parse_pow(t.substr(slash + 1), "px", &written)) return false;
            px = -written;   // 分母形式 "px^N" ⇒ 带符号幂次 -N
        }
    }
    if (variance) *variance = canon(adu * 2, px * 2);
    if (ivar) *ivar = canon(-adu * 2, -px * 2);
    return true;
}

bool fits_write_std_chksum(fitsfile* f, std::string* why) {
    int status = 0;
    if (fits_write_chksum(f, &status)) {
        if (why) *why = "fits_write_chksum: " + std::to_string(status);
        return false;
    }
    return true;
}

bool make_temp_path(const std::string& out, std::string* tmp) {
    // 死变量清理 (FIX-201): 原实现取 hostname 到 host[] 后从未读取, 且该
    // 取值不参与临时名 ⇒ 删除 (临时名语义逐位不变: <out>.<pid>.tmp)。
    *tmp = out + "." + std::to_string(::getpid()) + ".tmp";
    // 若 out 无目录, 用当前目录; tmp 与 out 同目录保证 rename 原子
    return true;
}

// R10-C(bughunt p2): sha256_file 的失败可见封装。lib/algorithms/shared/crypto::sha256_file
// 对文件打开失败返回空串、对读取中途错误静默返回前缀(部分数据)哈希 —— 任一
// 形态写进 provenance 即为无意义完整性锚。FIX-201 起该纪律下沉到 aio
// (aio_file_io.h): 只有完整读取成功才产出 64hex; 失败返回 false, 调用方必须把错误向上传播
// (整体输出失败), 禁止把空串/前缀哈希当作结果。
// ASTROCS_HASH_FAIL_INJECT (仅测试构建, -Dastrocs_hash_fail_inject 编入):
// 在完整读出后于 final 前注入一次 I/O 错误 → 走失败分支, 供单测断言不写假哈希。
bool sha256_file_checked(const char* path, std::string* hex_out) {
    hex_out->clear();
    // FIX-201: 文件打开/流式读取/关闭机制在 aio 内 (aio_file_io.h
    // aio_file::sha256_hex) —— 本模块不再自持 FILE* 通道。R10-C 语义不变:
    // 只有完整读取成功才产出 64hex, 失败返回 false 且清空输出。
    if (!aio_file::sha256_hex(path, hex_out)) return false;
#ifdef astrocs_hash_fail_inject
    if (std::getenv("ASTROCS_HASH_FAIL_INJECT")) { hex_out->clear(); return false; }
#endif
    return true;
}

}  // namespace

const char* p3_output_last_error() { return g_last_err.c_str(); }

P3OutputStatus p3_output_write_atomic(const float* signal, const float* coverage,
                                      int width, int height,
                                      const P3WcsDescriptor* wcs,
                                      const char* bunit,
                                      const char* output_path,
                                      const P3Provenance* prov,
                                      int bitpix,
                                      int cancelled_at_row,
                                      P3OutputResult* result) {
    // 旧签名 = unavailable 面 (variance/ivar 双 NULL)
    return p3_output_write_atomic_ex(signal, coverage, nullptr, nullptr, width,
                                     height, wcs, bunit, output_path, prov,
                                     bitpix, cancelled_at_row, result);
}

P3OutputStatus p3_output_write_atomic_ex(const float* signal, const float* coverage,
                                         const float* variance, const float* ivar,
                                         int width, int height,
                                         const P3WcsDescriptor* wcs,
                                         const char* bunit,
                                         const char* output_path,
                                         const P3Provenance* prov,
                                         int bitpix,
                                         int cancelled_at_row,
                                         P3OutputResult* result) {
    if (!signal || !coverage || !wcs || !output_path || width < 1 || height < 1)
        return P3_OUT_PARAM;
    // uncertainty 平面成对要求 (单边 NULL = 合同违规, 禁半可用发布)
    if ((variance == nullptr) != (ivar == nullptr)) return P3_OUT_PARAM;
    // B2-A4: 投影由已校验 descriptor 决定；未实现投影 fail-closed —— 在创建任何
    // 临时/输出文件之前拒绝 (不写 FITS, 不与请求不符的 CTYPE 混淆)。
    {
        std::string perr;
        if (p3_wcs_validate_request(wcs->projection, nullptr, nullptr, &perr) != P3_WCS_OK) {
            g_last_err = perr;
            return P3_OUT_PARAM;
        }
    }
    if (result) std::memset(result, 0, sizeof(*result));
    // RT-008: cfitsio 全局表非线程安全 → 进程级串行化（覆盖内部 verify 重开）
    aio::CfitsioLockGuard cfitsio_guard;

    std::string tmp;
    make_temp_path(output_path, &tmp);
    // 清理历史残留 tmp
    aio_atomic::remove_file(tmp);
    fitsfile* f = nullptr;
    int status = 0;

    long naxes[2] = {width, height};
    if (fits_create_file(&f, tmp.c_str(), &status)) {
        g_last_err = "fits_create_file(tmp): " + std::to_string(status);
        return P3_OUT_IO;
    }
    if (bitpix != -32 && bitpix != -64) {
        aio_atomic::remove_file(tmp);
        g_last_err = "bitpix must be -32|-64";
        return P3_OUT_PARAM;
    }
    if (fits_create_img(f, bitpix, 2, naxes, &status)) {
        aio_atomic::remove_file(tmp);
        g_last_err = "fits_create_img: " + std::to_string(status);
        return P3_OUT_IO;
    }

    // WCS + 基础关键字 (B2-A4: CTYPE 由已校验投影决定; TAN 字节与此前一致)
    {
        const char* pj = (wcs->projection && *wcs->projection) ? wcs->projection : "TAN";
        const std::string ctype1 = std::string("RA---") + pj;
        const std::string ctype2 = std::string("DEC--") + pj;
        fits_write_key(f, TSTRING, (char*)"CTYPE1", (void*)ctype1.c_str(), nullptr, &status);
        fits_write_key(f, TSTRING, (char*)"CTYPE2", (void*)ctype2.c_str(), nullptr, &status);
    }
    fits_write_key(f, TSTRING, (char*)"CUNIT1", (void*)"deg", nullptr, &status);
    fits_write_key(f, TSTRING, (char*)"CUNIT2", (void*)"deg", nullptr, &status);
    {
        const double cp = wcs->crpix_x, cq = wcs->crpix_y;
        const double rv1 = wcs->crval_ra_deg, rv2 = wcs->crval_dec_deg;
        const double c11 = wcs->cd[0][0], c12 = wcs->cd[0][1];
        const double c21 = wcs->cd[1][0], c22 = wcs->cd[1][1];
        double val;
        val = cp;  fits_write_key(f, TDOUBLE, (char*)"CRPIX1", &val, nullptr, &status);
        val = cq;  fits_write_key(f, TDOUBLE, (char*)"CRPIX2", &val, nullptr, &status);
        val = rv1; fits_write_key(f, TDOUBLE, (char*)"CRVAL1", &val, nullptr, &status);
        val = rv2; fits_write_key(f, TDOUBLE, (char*)"CRVAL2", &val, nullptr, &status);
        val = c11; fits_write_key(f, TDOUBLE, (char*)"CD1_1", &val, nullptr, &status);
        val = c12; fits_write_key(f, TDOUBLE, (char*)"CD1_2", &val, nullptr, &status);
        val = c21; fits_write_key(f, TDOUBLE, (char*)"CD2_1", &val, nullptr, &status);
        val = c22; fits_write_key(f, TDOUBLE, (char*)"CD2_2", &val, nullptr, &status);
    }
    {
        // B2-A9: BSCALE/BZERO 改 TDOUBLE。FITS 4.0（IAU FITS Standard 4.0
        // §4.4.2.4）规定 BSCALE/BZERO 为浮点数据类型关键字；旧实现以 TINT
        // 写整型 1/0，astropy 7.0.1 可读但严格校验器判类型违规（AUD-COORD
        // F-09）。此处保持恒等缩放语义（BSCALE=1.0 / BZERO=0.0）不变，仅
        // 数据类型改为标准要求的 double。
        double bscale = 1.0, bzero = 0.0;
        fits_write_key(f, TDOUBLE, (char*)"BSCALE", &bscale, nullptr, &status);
        fits_write_key(f, TDOUBLE, (char*)"BZERO", &bzero, nullptr, &status);
        // 主 HDU = 重采样后的**面亮度**平面（§27.1/§29.3），单位口径见
        // DATA_SEMANTICS §31.1a；缺省串取该平面的物理单位 canonical "ADU/sr"
        // （裸 "ADU" 是每像素计数口径，与本平面数值不符且量纲不可判）。
        const char* unit = (bunit && *bunit) ? bunit : "ADU/sr";
        fits_write_key(f, TSTRING, (char*)"BUNIT", (void*)unit, nullptr, &status);
    }
    if (prov) {
        fits_write_key(f, TSTRING, (char*)"HIPSID", (void*)prov->hips_id, nullptr, &status);
        fits_write_key(f, TSTRING, (char*)"RUNID", (void*)prov->run_id, nullptr, &status);
        fits_write_key(f, TSTRING, (char*)"ORDERSEL", (void*)prov->order_sel_used,
                       nullptr, &status);
        fits_write_key(f, TSTRING, (char*)"SAMPLER", (void*)prov->sampler_used,
                       nullptr, &status);
        fits_write_key(f, TSTRING, (char*)"SWVER", (void*)prov->software_version,
                       nullptr, &status);
        char hist[160];
        std::snprintf(hist, sizeof(hist), "HISTORY phase3 source=%s manifest=%s",
                      prov->hips_id, prov->manifest_hash ? prov->manifest_hash : "");
        fits_write_history(f, hist, &status);
    }

    // 写 signal 像素
    long fpix[2] = {1, 1};
    const long nelem = (long)width * height;
    fits_write_pix(f, TFLOAT, fpix, nelem, (void*)signal, &status);

    // Signal HDU 完成; 若取消于某行 → 不落盘
    if (cancelled_at_row >= 0) {
        fits_close_file(f, &status);
        aio_atomic::remove_file(tmp);
        return P3_OUT_CANCELLED;
    }

    // B2-A9: PRIMARY HDU 标准 DATASUM/CHECKSUM（旧实现 PRIMARY 无 DATASUM）。
    {
        std::string why;
        if (!fits_write_std_chksum(f, &why)) {
            g_last_err = why;
            fits_close_file(f, &status);
            aio_atomic::remove_file(tmp);
            return P3_OUT_IO;
        }
    }

    // 追加 coverage 扩展 HDU
    long cnaxes[2] = {width, height};
    if (fits_create_img(f, bitpix, 2, cnaxes, &status)) {
        aio_atomic::remove_file(tmp);
        g_last_err = "coverage create_img: " + std::to_string(status);
        return P3_OUT_IO;
    }
    fits_write_key(f, TSTRING, (char*)"EXTNAME", (void*)"COVERAGE", nullptr, &status);
    fits_write_pix(f, TFLOAT, fpix, nelem, (void*)coverage, &status);
    // B2-A9: COVERAGE HDU 标准校验和。旧实现在 COVERAGE HDU 写 signal 数据的
    // 自算 DATASUM（归属错 + 值非法），ASTROPY 对 COVERAGE/VARIANCE/IVAR 报
    // "Datasum verification failed"。
    {
        std::string why;
        if (!fits_write_std_chksum(f, &why)) {
            g_last_err = "coverage " + why;
            fits_close_file(f, &status);
            aio_atomic::remove_file(tmp);
            return P3_OUT_IO;
        }
    }

    // 追加 uncertainty 扩展 HDU (DATA-P3-UNC-001 §30.4/§27.2 目标态行):
    // VARIANCE (BUNIT=<BUNIT>^2) + IVAR (BUNIT=1/(<BUNIT>^2)), DATASUM 逐 HDU;
    // 与主/扩展 HDU 同一原子发布序 (取消不落盘语义由上方 cancelled 分支保持)。
    if (variance && ivar) {
        const char* unit = (bunit && *bunit) ? bunit : "ADU/sr";
        // FIX-402: 二次律 canonical 推导（FZ-P3-BUNIT-QUADRATIC）; 单位不在冻结
        // 表内 → 显式拒绝, 不写出不可判的 variance BUNIT。
        std::string var_bunit, ivar_bunit;
        if (!bunit_square_canonical(unit, &var_bunit, &ivar_bunit)) {
            g_last_err = std::string("variance BUNIT undecidable for signal BUNIT '") +
                         unit + "' (FZ-P3-BUNIT-QUADRATIC)";
            fits_close_file(f, &status);
            aio_atomic::remove_file(tmp);
            return P3_OUT_PARAM;
        }
        for (int h = 0; h < 2; ++h) {
            if (fits_create_img(f, bitpix, 2, cnaxes, &status)) {
                aio_atomic::remove_file(tmp);
                g_last_err = std::string(h == 0 ? "variance" : "ivar") +
                             " create_img: " + std::to_string(status);
                return P3_OUT_IO;
            }
            fits_write_key(f, TSTRING, (char*)"EXTNAME",
                           (void*)(h == 0 ? "VARIANCE" : "IVAR"), nullptr, &status);
            fits_write_key(f, TSTRING, (char*)"BUNIT",
                           (void*)(h == 0 ? var_bunit.c_str() : ivar_bunit.c_str()),
                           nullptr, &status);
            fits_write_pix(f, TFLOAT, fpix, nelem,
                           (void*)(h == 0 ? variance : ivar), &status);
            // B2-A9: 每个 uncertainty HDU 的标准 DATASUM/CHECKSUM，归属自身数据。
            std::string why;
            if (!fits_write_std_chksum(f, &why)) {
                g_last_err = std::string(h == 0 ? "variance " : "ivar ") + why;
                fits_close_file(f, &status);
                aio_atomic::remove_file(tmp);
                return P3_OUT_IO;
            }
        }
    }

    // fsync + rename 原子替换
    // R10-C: 顺序必须是 cfitsio 缓冲 flush → fsync(fd) → 原子 rename。
    // 原实现在 fits_close_file 之前对 fd 做 fsync —— cfitsio 的 IO 缓冲
    // (2880B 扇区 buffer) 尚未写出, fd 级 fsync 只能落已写入内核页缓存的
    // 前缀, 崩溃时可丢失数据或留半成品 (违反 IO_003 §4 "关闭/fsync → … →
    // 原子 rename"、§6 "任何失败都不产生成功对象")。
    {
        int fstatus = 0;
        // ① 先把 cfitsio 内部缓冲全部推到 OS (ffflus: 写 dirty buffer +
        //    ffflushx; 返回值必须检查, 失败即无成功对象)
        if (fits_flush_file(f, &fstatus)) {
            g_last_err = "fits_flush_file: " + std::to_string(fstatus);
            fits_close_file(f, &fstatus);
            aio_atomic::remove_file(tmp);
            return P3_OUT_IO;
        }
        fits_close_file(f, &status);
        if (status) { aio_atomic::remove_file(tmp); g_last_err = "close: " + std::to_string(status); return P3_OUT_IO; }
        // ② 内容已完整写出后再 fsync; 打开/fsync/关闭失败都是发布失败。
        // FIX-201: fsync 机制在 aio (aio_atomic::fsync_path) —— Windows 的
        // _commit 可写句柄语义 (R18 34201181796 诊断 errno=9 实证) 由 aio
        // 承接; 本模块不再自行 open/fsync/close。
        {
            const int frc = aio_atomic::fsync_path(tmp, 0);
            if (frc != 0) {
                g_last_err = std::string("fsync(tmp): ") + std::strerror(frc);
                aio_atomic::remove_file(tmp);
                return P3_OUT_IO;
            }
        }
    }
    // FIX-201: 原子 rename 机制在 aio (aio_atomic::atomic_replace)。
    if (aio_atomic::atomic_replace(tmp, output_path) != 0) {
        g_last_err = std::string("rename: ") + std::strerror(errno);
        aio_atomic::remove_file(tmp);
        return P3_OUT_IO;
    }

    // 计算 sha256(重新读出的完整文件) 并独立重开验证(重开验证)
    if (result) {
        std::string h;
        // R10-C: 哈希失败 = 完整性锚缺失 → 不写空串/前缀哈希, 整体输出失败
        if (!sha256_file_checked(output_path, &h)) {
            g_last_err = "sha256_file(published output) failed";
            aio_atomic::remove_file(output_path);
            return P3_OUT_IO;
        }
        std::snprintf(result->sha256, sizeof(result->sha256), "%s", h.c_str());
        result->total_px = (long)width * height;
        long cov = 0;
        for (long i = 0; i < nelem; ++i) if (coverage[i] > 0.5f) ++cov;
        result->covered_px = cov;
        result->coverage_ok = 1;
        // 独立重开读回验证 (dimensions/WCS/BUNIT/checksum/mask/uncertainty HDU 面)
        P3OutputResult v{};
        P3OutputStatus vst = p3_output_verify_ex(output_path, wcs, signal, coverage,
                                                 variance, ivar, width, height, &v);
        result->reopen_ok = (vst == P3_OUT_OK) ? v.reopen_ok : 0;
    }
    return P3_OUT_OK;
}

P3OutputStatus p3_output_verify(const char* output_path, const P3WcsDescriptor* wcs,
                                const float* signal, const float* coverage,
                                int width, int height, P3OutputResult* result) {
    // 旧签名 = unavailable 面 (variance/ivar 双 NULL)
    return p3_output_verify_ex(output_path, wcs, signal, coverage, nullptr, nullptr,
                               width, height, result);
}

P3OutputStatus p3_output_verify_ex(const char* output_path,
                                   const P3WcsDescriptor* wcs,
                                   const float* signal, const float* coverage,
                                   const float* variance, const float* ivar,
                                   int width, int height, P3OutputResult* result) {
    if (!output_path || !result || width < 1 || height < 1) return P3_OUT_PARAM;
    if ((variance == nullptr) != (ivar == nullptr)) return P3_OUT_PARAM;
    // B2-A9: verify 对 WCS 零鉴别力是审计缺陷（AUD-COORD F-05）。此处读回
    // CTYPE/CUNIT/CRPIX/CRVAL/CD 与传入 descriptor 逐项对拍，任何 CRPIX 平移、
    // origin 双桥接或 CD 篡改都会被检出并置 reopen_ok=0（AUD-P2P3 F24）。
    // 容差来源: 写路径以 TDOUBLE 写 double，读回亦为 double，round-trip 应为
    // 位精确；1e-12(度/像素) / 1e-15(CD deg/px) 仅吸收格式层十进制往返。
    std::memset(result, 0, sizeof(*result));
    long nelem = (long)width * height;
    int ok = 1, covok = 1, uncok = 1, wcsok = 1;
    int hdus = 1;

    fitsfile* f = nullptr; int status = 0;
    if (fits_open_file(&f, output_path, READONLY, &status) != 0) {
        g_last_err = "open failed";
        return P3_OUT_IO;
    }
    fits_get_num_hdus(f, &hdus, &status);

    // primary (HDU 1) = signal: 读像素 + WCS 关键字(C1 对拍)
    if (fits_movabs_hdu(f, 1, nullptr, &status) == 0) {
        // B2-A9: 期望值由 descriptor 的冻结写码决定（与写路径同一公式），
        // 仅读回对比，不改写。
        const std::string pj = (wcs->projection && *wcs->projection)
                                   ? wcs->projection : "TAN";
        // fits_read_keyword 返回的是「值字段」：(a) keyword/= 已剥离；(b) 字符串
        // 值保留两侧单引号且定长右补空格（如 ['RA---TAN'] / ['deg     ']）。
        // 故先剥引号再去尾空白，与期望裸值全等比较。
        const std::string want_ctype1 = std::string("RA---") + pj;
        const std::string want_ctype2 = std::string("DEC--") + pj;
        auto card_equals = [](const char* card, const char* want) {
            std::string got(card ? card : "");
            // 先剥字符串值两端的单引号，再去定长补位空白（顺序不可颠倒：
            // ['deg     '] 剥引号后仍有尾部补白）。
            if (got.size() >= 2 && got.front() == '\'' && got.back() == '\'')
                got = got.substr(1, got.size() - 2);
            while (!got.empty() && (got.back() == ' ' || got.back() == '\t'))
                got.pop_back();
            return got == want;
        };
        const struct { const char* key; const char* want; } skeys[] = {
            {"CTYPE1", want_ctype1.c_str()}, {"CTYPE2", want_ctype2.c_str()},
            {"CUNIT1", "deg"}, {"CUNIT2", "deg"}};
        for (const auto& sk : skeys) {
            char card[81] = {0};
            status = 0;
            if (fits_read_keyword(f, sk.key, card, nullptr, &status) != 0 ||
                !card_equals(card, sk.want)) {
                wcsok = 0;
                break;
            }
        }
        status = 0;
        const struct { const char* key; double want; } dkeys[] = {
            {"CRPIX1", wcs->crpix_x}, {"CRPIX2", wcs->crpix_y},
            {"CRVAL1", wcs->crval_ra_deg}, {"CRVAL2", wcs->crval_dec_deg},
            {"CD1_1", wcs->cd[0][0]}, {"CD1_2", wcs->cd[0][1]},
            {"CD2_1", wcs->cd[1][0]}, {"CD2_2", wcs->cd[1][1]}};
        for (const auto& dk : dkeys) {
            double got = 0.0;
            status = 0;
            if (fits_read_key(f, TDOUBLE, (char*)dk.key, &got, nullptr, &status) != 0 ||
                std::fabs(got - dk.want) > 1e-12) {
                wcsok = 0;
                break;
            }
        }
        status = 0;
        int naxis = 0, imgtype = 0;
        long nax[2] = {0, 0};
        fits_get_img_param(f, 2, &imgtype, &naxis, nax, &status);
        if ((long)nax[0] == width && (long)nax[1] == height) {
            std::vector<float> sig((size_t)nelem);
            long fp[2] = {1, 1};
            fits_read_pix(f, TFLOAT, fp, (LONGLONG)nelem, NULL, sig.data(), NULL, &status);
            for (long i = 0; i < nelem; ++i) {
                // NaN 语义: 双方都是 NaN → 视为一致(源无覆盖=NaN); 否则逐值精确回环
                const bool sn = (signal[i] != signal[i]);
                const bool rd = (sig[(size_t)i] != sig[(size_t)i]);
                if (sig[(size_t)i] != signal[i] && !(sn && rd)) { ok = 0; break; }
            }
        } else ok = 0;
    } else ok = 0;

    // extension (HDU 2) = coverage
    if (hdus >= 2 && fits_movabs_hdu(f, 2, nullptr, &status) == 0) {
        int naxis = 0, imgtype = 0;
        long nax[2] = {0, 0};
        fits_get_img_param(f, 2, &imgtype, &naxis, nax, &status);
        if ((long)nax[0] == width && (long)nax[1] == height) {
            std::vector<float> cov((size_t)nelem);
            long fp[2] = {1, 1};
            fits_read_pix(f, TFLOAT, fp, (LONGLONG)nelem, NULL, cov.data(), NULL, &status);
            for (long i = 0; i < nelem; ++i)
                if ((cov[(size_t)i] > 0.5f) != (coverage[i] > 0.5f)) { covok = 0; break; }
        } else covok = 0;
    }

    // uncertainty HDU 面 (双向防: available 静默缺 HDU / unavailable 静默占位)
    if (variance && ivar) {
        const float* unc[2] = {variance, ivar};
        const char* want[2] = {"VARIANCE", "IVAR"};
        for (int h = 0; h < 2 && uncok; ++h) {
            if (hdus < 3 + h || fits_movabs_hdu(f, 3 + h, nullptr, &status) != 0) {
                uncok = 0; break;          // 静默缺 HDU
            }
            char card[81] = {0};
            if (fits_read_keyword(f, "EXTNAME", card, nullptr, &status) != 0 ||
                !std::strstr(card, want[h])) {
                uncok = 0; break;
            }
            status = 0;
            int naxis = 0, imgtype = 0;
            long nax[2] = {0, 0};
            fits_get_img_param(f, 2, &imgtype, &naxis, nax, &status);
            if ((long)nax[0] != width || (long)nax[1] != height) { uncok = 0; break; }
            std::vector<float> plane((size_t)nelem);
            long fp[2] = {1, 1};
            if (fits_read_pix(f, TFLOAT, fp, (LONGLONG)nelem, NULL, plane.data(),
                              NULL, &status)) {
                uncok = 0; break;
            }
            for (long i = 0; i < nelem; ++i) {
                const bool sn = (unc[h][i] != unc[h][i]);
                const bool rd = (plane[(size_t)i] != plane[(size_t)i]);
                if (plane[(size_t)i] != unc[h][i] && !(sn && rd)) { uncok = 0; break; }
            }
        }
    } else if (hdus >= 3) {
        // unavailable → 不允许任何占位 uncertainty HDU
        if (fits_movabs_hdu(f, 3, nullptr, &status) == 0) {
            char card[81] = {0};
            if (fits_read_keyword(f, "EXTNAME", card, nullptr, &status) == 0 &&
                (std::strstr(card, "VARIANCE") || std::strstr(card, "IVAR")))
                uncok = 0;
            status = 0;
        }
    }
    fits_close_file(f, &status);

    result->reopen_ok = (ok == 1 && covok == 1 && uncok == 1 && wcsok == 1);
    result->coverage_ok = covok;
    long covn = 0;
    for (long i = 0; i < nelem; ++i) if (coverage[i] > 0.5f) ++covn;
    result->covered_px = covn;
    result->total_px = nelem;
    // 重算 checksum (独立重开 + checksum)
    {
        std::string h;
        // R10-C: 哈希失败 = 完整性锚缺失 → 返回 IO, 不写空串/前缀哈希
        if (!sha256_file_checked(output_path, &h)) {
            g_last_err = "sha256_file(verify) failed";
            return P3_OUT_IO;
        }
        std::snprintf(result->sha256, sizeof(result->sha256), "%s", h.c_str());
    }
    return P3_OUT_OK;
}

// ══════════════════════════════════════════════════════════════════════════
// 子块流式写 / 独立重开流式校验（P3-STREAM-01）
//
// 规范：ASTROCS_DESIGN §8.3 export 行「子块流式：读子块 → 投影重采样 → 写
// FITS，有界队列 + 背压，不整幅驻留；I/O 与计算重叠，内存占用与子块大小成
// 正比、与总图大小无关」；docs/contracts/SCHEDULER_CONTRACT.md §2 export 行同文。
// 产品语义与整幅 API 逐条同面（FITS 关键字、BSCALE/BZERO、HISTORY provenance、
// 逐 HDU DATASUM/CHECKSUM、flush→close→fsync→rename 原子发布序、独立重开
// 对拍），差别只在**驻留面**：像素按矩形子块经 cfitsio 子集接口进出，
// 任何时刻只持有一个子块的缓冲。
// ══════════════════════════════════════════════════════════════════════════

struct P3FitsStream::Impl {
    std::unique_ptr<aio::CfitsioLockGuard> lock;   // cfitsio 进程级串行化（RT-008）
    fitsfile* f = nullptr;
    int status = 0;
    std::string tmp;
    std::string out;
    int width = 0, height = 0, bitpix = -32;
    std::string bunit;
    int cur_hdu = 0;          // 0=PRIMARY(signal) 1=COVERAGE 2=VARIANCE 3=IVAR
    bool hdu_open = false;    // 当前 HDU 已建、尚未写 DATASUM/CHECKSUM
    bool failed = false;
};

P3FitsStream::P3FitsStream() : impl_(new Impl()) {}
P3FitsStream::~P3FitsStream() {
    if (impl_) { abort(); delete impl_; impl_ = nullptr; }
}

P3OutputStatus P3FitsStream::open(const char* output_path,
                                  const P3WcsDescriptor* wcs, int width,
                                  int height, int bitpix, const char* bunit,
                                  const P3Provenance* prov) {
    if (!output_path || !wcs || width < 1 || height < 1) return P3_OUT_PARAM;
    // B2-A4: 投影由已校验 descriptor 决定；未实现投影 fail-closed —— 在创建
    // 任何临时/输出文件之前拒绝（与整幅路径同判）。
    {
        std::string perr;
        if (p3_wcs_validate_request(wcs->projection, nullptr, nullptr, &perr) !=
            P3_WCS_OK) {
            g_last_err = perr;
            return P3_OUT_PARAM;
        }
    }
    if (bitpix != -32 && bitpix != -64) {
        g_last_err = "bitpix must be -32|-64";
        return P3_OUT_PARAM;
    }
    impl_->width = width;
    impl_->height = height;
    impl_->bitpix = bitpix;
    impl_->bunit = bunit ? bunit : "ADU";
    impl_->out = output_path;
    impl_->lock.reset(new aio::CfitsioLockGuard());
    make_temp_path(impl_->out, &impl_->tmp);
    aio_atomic::remove_file(impl_->tmp);
    long naxes[2] = {width, height};
    if (fits_create_file(&impl_->f, impl_->tmp.c_str(), &impl_->status)) {
        g_last_err = "fits_create_file(tmp): " + std::to_string(impl_->status);
        abort();
        return P3_OUT_IO;
    }
    if (fits_create_img(impl_->f, bitpix, 2, naxes, &impl_->status)) {
        g_last_err = "fits_create_img: " + std::to_string(impl_->status);
        abort();
        return P3_OUT_IO;
    }
    // ── 合同③：PRIMARY 头（WCS + 单位 + provenance + HISTORY）在**首像素
    //    写出之前**全部组装完成；后续 write_block 只碰数据区，不补头。
    fitsfile* f = impl_->f;
    int& status = impl_->status;
    {
        const char* pj = (wcs->projection && *wcs->projection) ? wcs->projection : "TAN";
        const std::string ctype1 = std::string("RA---") + pj;
        const std::string ctype2 = std::string("DEC--") + pj;
        fits_write_key(f, TSTRING, (char*)"CTYPE1", (void*)ctype1.c_str(), nullptr, &status);
        fits_write_key(f, TSTRING, (char*)"CTYPE2", (void*)ctype2.c_str(), nullptr, &status);
    }
    fits_write_key(f, TSTRING, (char*)"CUNIT1", (void*)"deg", nullptr, &status);
    fits_write_key(f, TSTRING, (char*)"CUNIT2", (void*)"deg", nullptr, &status);
    {
        double val;
        val = wcs->crpix_x;        fits_write_key(f, TDOUBLE, (char*)"CRPIX1", &val, nullptr, &status);
        val = wcs->crpix_y;        fits_write_key(f, TDOUBLE, (char*)"CRPIX2", &val, nullptr, &status);
        val = wcs->crval_ra_deg;   fits_write_key(f, TDOUBLE, (char*)"CRVAL1", &val, nullptr, &status);
        val = wcs->crval_dec_deg;  fits_write_key(f, TDOUBLE, (char*)"CRVAL2", &val, nullptr, &status);
        val = wcs->cd[0][0];       fits_write_key(f, TDOUBLE, (char*)"CD1_1", &val, nullptr, &status);
        val = wcs->cd[0][1];       fits_write_key(f, TDOUBLE, (char*)"CD1_2", &val, nullptr, &status);
        val = wcs->cd[1][0];       fits_write_key(f, TDOUBLE, (char*)"CD2_1", &val, nullptr, &status);
        val = wcs->cd[1][1];       fits_write_key(f, TDOUBLE, (char*)"CD2_2", &val, nullptr, &status);
    }
    {
        double bscale = 1.0, bzero = 0.0;
        fits_write_key(f, TDOUBLE, (char*)"BSCALE", &bscale, nullptr, &status);
        fits_write_key(f, TDOUBLE, (char*)"BZERO", &bzero, nullptr, &status);
        fits_write_key(f, TSTRING, (char*)"BUNIT", (void*)impl_->bunit.c_str(), nullptr, &status);
    }
    if (prov) {
        fits_write_key(f, TSTRING, (char*)"HIPSID", (void*)prov->hips_id, nullptr, &status);
        fits_write_key(f, TSTRING, (char*)"RUNID", (void*)prov->run_id, nullptr, &status);
        fits_write_key(f, TSTRING, (char*)"ORDERSEL", (void*)prov->order_sel_used, nullptr, &status);
        fits_write_key(f, TSTRING, (char*)"SAMPLER", (void*)prov->sampler_used, nullptr, &status);
        fits_write_key(f, TSTRING, (char*)"SWVER", (void*)prov->software_version, nullptr, &status);
        char hist[160];
        std::snprintf(hist, sizeof(hist), "HISTORY phase3 source=%s manifest=%s",
                      prov->hips_id, prov->manifest_hash ? prov->manifest_hash : "");
        fits_write_history(f, hist, &status);
    }
    if (status) {
        g_last_err = "PRIMARY header write failed: " + std::to_string(status);
        abort();
        return P3_OUT_IO;
    }
    impl_->cur_hdu = 0;
    impl_->hdu_open = true;
    return P3_OUT_OK;
}

P3OutputStatus P3FitsStream::begin_hdu(int plane) {
    if (!impl_ || !impl_->f || impl_->failed) return P3_OUT_PARAM;
    if (plane == 0) {
        // PRIMARY 已在 open() 内建好（头先于像素）
        if (impl_->cur_hdu != 0 || !impl_->hdu_open) return P3_OUT_PARAM;
        return P3_OUT_OK;
    }
    if (plane < 1 || plane > 3) return P3_OUT_PARAM;
    if (impl_->hdu_open) return P3_OUT_PARAM;   // 上一 HDU 未收尾（校验和未写）
    fitsfile* f = impl_->f;
    int& status = impl_->status;
    long cnaxes[2] = {impl_->width, impl_->height};
    if (fits_create_img(f, impl_->bitpix, 2, cnaxes, &status)) {
        g_last_err = "extension create_img: " + std::to_string(status);
        impl_->failed = true;
        return P3_OUT_IO;
    }
    if (plane == 1) {
        fits_write_key(f, TSTRING, (char*)"EXTNAME", (void*)"COVERAGE", nullptr, &status);
    } else {
        // FIX-402: 二次律 canonical 推导（FZ-P3-BUNIT-QUADRATIC）; 表外单位显式拒绝
        std::string var_bunit, ivar_bunit;
        if (!bunit_square_canonical(impl_->bunit, &var_bunit, &ivar_bunit)) {
            g_last_err = std::string("variance BUNIT undecidable for signal BUNIT '") +
                         impl_->bunit + "' (FZ-P3-BUNIT-QUADRATIC)";
            impl_->failed = true;
            return P3_OUT_PARAM;
        }
        fits_write_key(f, TSTRING, (char*)"EXTNAME",
                       (void*)(plane == 2 ? "VARIANCE" : "IVAR"), nullptr, &status);
        fits_write_key(f, TSTRING, (char*)"BUNIT",
                       (void*)(plane == 2 ? var_bunit.c_str() : ivar_bunit.c_str()),
                       nullptr, &status);
    }
    if (status) {
        g_last_err = "extension header write failed: " + std::to_string(status);
        impl_->failed = true;
        return P3_OUT_IO;
    }
    impl_->cur_hdu = plane;
    impl_->hdu_open = true;
    return P3_OUT_OK;
}

P3OutputStatus P3FitsStream::write_block(int x0, int y0, int w, int h,
                                         const float* data) {
    if (!impl_ || !impl_->f || !data) return P3_OUT_PARAM;
    if (!impl_->hdu_open || impl_->failed) return P3_OUT_PARAM;
    if (w < 1 || h < 1 || x0 < 0 || y0 < 0 || x0 + w > impl_->width ||
        y0 + h > impl_->height) {
        g_last_err = "sub-block out of image bounds";
        impl_->failed = true;
        return P3_OUT_PARAM;
    }
    long fp[2] = {x0 + 1, y0 + 1};   // FITS 1-based
    long lp[2] = {x0 + w, y0 + h};
    int& status = impl_->status;
    if (fits_write_subset(impl_->f, TFLOAT, fp, lp, (void*)data, &status)) {
        g_last_err = "fits_write_subset failed: " + std::to_string(status);
        impl_->failed = true;
        return P3_OUT_IO;
    }
    return P3_OUT_OK;
}

P3OutputStatus P3FitsStream::end_hdu() {
    if (!impl_ || !impl_->f) return P3_OUT_PARAM;
    if (!impl_->hdu_open) return P3_OUT_PARAM;
    // B2-A9: 标准 DATASUM/CHECKSUM 逐 HDU（与整幅路径同一实现）
    std::string why;
    if (!fits_write_std_chksum(impl_->f, &why)) {
        g_last_err = why;
        impl_->failed = true;
        return P3_OUT_IO;
    }
    impl_->hdu_open = false;
    return P3_OUT_OK;
}

P3OutputStatus P3FitsStream::publish(P3OutputResult* result) {
    if (!impl_ || !impl_->f || impl_->failed || impl_->hdu_open) return P3_OUT_IO;
    if (result) std::memset(result, 0, sizeof(*result));
    int fstatus = 0;
    if (fits_flush_file(impl_->f, &fstatus)) {
        g_last_err = "fits_flush_file: " + std::to_string(fstatus);
        abort();
        return P3_OUT_IO;
    }
    int status = 0;
    fits_close_file(impl_->f, &status);
    impl_->f = nullptr;
    if (status) {
        g_last_err = "close: " + std::to_string(status);
        abort();
        return P3_OUT_IO;
    }
    // ② 内容已完整写出后再 fsync（机制在 aio）；③ 原子 rename。
    const int frc = aio_atomic::fsync_path(impl_->tmp, 0);
    if (frc != 0) {
        g_last_err = std::string("fsync(tmp): ") + std::strerror(frc);
        abort();
        return P3_OUT_IO;
    }
    if (aio_atomic::atomic_replace(impl_->tmp, impl_->out) != 0) {
        g_last_err = std::string("rename: ") + std::strerror(errno);
        abort();
        return P3_OUT_IO;
    }
    if (result) {
        std::string h;
        if (!sha256_file_checked(impl_->out.c_str(), &h)) {
            g_last_err = "sha256_file(published output) failed";
            aio_atomic::remove_file(impl_->out);
            abort();
            return P3_OUT_IO;
        }
        std::snprintf(result->sha256, sizeof(result->sha256), "%s", h.c_str());
        result->total_px = (long)impl_->width * impl_->height;
    }
    published_ = true;
    impl_->lock.reset();
    return P3_OUT_OK;
}

void P3FitsStream::abort() {
    if (!impl_) return;
    if (impl_->f) {
        int st = 0;
        fits_close_file(impl_->f, &st);
        impl_->f = nullptr;
    }
    if (!impl_->tmp.empty()) aio_atomic::remove_file(impl_->tmp);
    impl_->lock.reset();
}

// ── 独立重开流式校验（与 p3_output_verify_ex 同判据，按子块读回）─────────
struct P3FitsVerifyStream::Impl {
    std::unique_ptr<aio::CfitsioLockGuard> lock;
    fitsfile* f = nullptr;
    int status = 0;
    int width = 0, height = 0;
    bool unc = false;
    int hdus = 1;
    int ok = 1, covok = 1, uncok = 1, wcsok = 1;
    long covered = 0;
    std::string path;
};

P3FitsVerifyStream::P3FitsVerifyStream() : impl_(new Impl()) {}
P3FitsVerifyStream::~P3FitsVerifyStream() {
    if (impl_) {
        if (impl_->f) { int st = 0; fits_close_file(impl_->f, &st); impl_->f = nullptr; }
        impl_->lock.reset();
        delete impl_;
        impl_ = nullptr;
    }
}

P3OutputStatus P3FitsVerifyStream::open(const char* output_path,
                                        const P3WcsDescriptor* wcs, int width,
                                        int height, bool has_uncertainty) {
    if (!output_path || !wcs || width < 1 || height < 1) return P3_OUT_PARAM;
    impl_->width = width;
    impl_->height = height;
    impl_->unc = has_uncertainty;
    impl_->path = output_path;
    impl_->lock.reset(new aio::CfitsioLockGuard());
    int& status = impl_->status;
    if (fits_open_file(&impl_->f, output_path, READONLY, &status) != 0) {
        g_last_err = "open failed";
        return P3_OUT_IO;
    }
    fits_get_num_hdus(impl_->f, &impl_->hdus, &status);
    fitsfile* f = impl_->f;
    // primary (HDU 1) = signal: WCS 关键字逐项对拍（B2-A9/AUD-COORD F-05）
    if (fits_movabs_hdu(f, 1, nullptr, &status) == 0) {
        const std::string pj = (wcs->projection && *wcs->projection)
                                   ? wcs->projection : "TAN";
        const std::string want_ctype1 = std::string("RA---") + pj;
        const std::string want_ctype2 = std::string("DEC--") + pj;
        auto card_equals = [](const char* card, const char* want) {
            std::string got(card ? card : "");
            if (got.size() >= 2 && got.front() == '\'' && got.back() == '\'')
                got = got.substr(1, got.size() - 2);
            while (!got.empty() && (got.back() == ' ' || got.back() == '\t')) got.pop_back();
            return got == want;
        };
        const struct { const char* key; const char* want; } skeys[] = {
            {"CTYPE1", want_ctype1.c_str()}, {"CTYPE2", want_ctype2.c_str()},
            {"CUNIT1", "deg"}, {"CUNIT2", "deg"}};
        for (const auto& sk : skeys) {
            char card[81] = {0};
            status = 0;
            if (fits_read_keyword(f, sk.key, card, nullptr, &status) != 0 ||
                !card_equals(card, sk.want)) {
                impl_->wcsok = 0;
                break;
            }
        }
        status = 0;
        const struct { const char* key; double want; } dkeys[] = {
            {"CRPIX1", wcs->crpix_x}, {"CRPIX2", wcs->crpix_y},
            {"CRVAL1", wcs->crval_ra_deg}, {"CRVAL2", wcs->crval_dec_deg},
            {"CD1_1", wcs->cd[0][0]}, {"CD1_2", wcs->cd[0][1]},
            {"CD2_1", wcs->cd[1][0]}, {"CD2_2", wcs->cd[1][1]}};
        for (const auto& dk : dkeys) {
            double got = 0.0;
            status = 0;
            if (fits_read_key(f, TDOUBLE, (char*)dk.key, &got, nullptr, &status) != 0 ||
                std::fabs(got - dk.want) > 1e-12) {
                impl_->wcsok = 0;
                break;
            }
        }
        status = 0;
        int naxis = 0, imgtype = 0;
        long nax[2] = {0, 0};
        fits_get_img_param(f, 2, &imgtype, &naxis, nax, &status);
        if ((long)nax[0] != width || (long)nax[1] != height) impl_->ok = 0;
    } else {
        impl_->ok = 0;
    }
    // extension (HDU 2) = coverage
    if (impl_->hdus >= 2 && fits_movabs_hdu(f, 2, nullptr, &status) == 0) {
        int naxis = 0, imgtype = 0;
        long nax[2] = {0, 0};
        status = 0;
        fits_get_img_param(f, 2, &imgtype, &naxis, nax, &status);
        if ((long)nax[0] != width || (long)nax[1] != height) impl_->covok = 0;
    } else {
        impl_->covok = 0;
    }
    // uncertainty HDU 面（双向防: available 静默缺 HDU / unavailable 静默占位）
    if (impl_->unc) {
        const char* want[2] = {"VARIANCE", "IVAR"};
        for (int h = 0; h < 2 && impl_->uncok; ++h) {
            if (impl_->hdus < 3 + h ||
                fits_movabs_hdu(f, 3 + h, nullptr, &status) != 0) {
                impl_->uncok = 0;
                break;
            }
            char card[81] = {0};
            status = 0;
            if (fits_read_keyword(f, "EXTNAME", card, nullptr, &status) != 0 ||
                !std::strstr(card, want[h])) {
                impl_->uncok = 0;
                break;
            }
            int naxis = 0, imgtype = 0;
            long nax[2] = {0, 0};
            status = 0;
            fits_get_img_param(f, 2, &imgtype, &naxis, nax, &status);
            if ((long)nax[0] != width || (long)nax[1] != height) { impl_->uncok = 0; break; }
        }
    } else if (impl_->hdus >= 3) {
        if (fits_movabs_hdu(f, 3, nullptr, &status) == 0) {
            char card[81] = {0};
            status = 0;
            if (fits_read_keyword(f, "EXTNAME", card, nullptr, &status) == 0 &&
                (std::strstr(card, "VARIANCE") || std::strstr(card, "IVAR")))
                impl_->uncok = 0;
        }
    }
    impl_->status = 0;
    return P3_OUT_OK;
}

P3OutputStatus P3FitsVerifyStream::check_block(int plane, int x0, int y0, int w,
                                               int h, const float* expected) {
    if (!impl_ || !impl_->f || !expected) return P3_OUT_PARAM;
    if (plane < 0 || plane > 3 || w < 1 || h < 1) return P3_OUT_PARAM;
    if (x0 < 0 || y0 < 0 || x0 + w > impl_->width || y0 + h > impl_->height)
        return P3_OUT_PARAM;
    if (plane >= 2 && !impl_->unc) return P3_OUT_PARAM;
    int status = 0;
    if (fits_movabs_hdu(impl_->f, plane + 1, nullptr, &status) != 0) {
        if (plane == 0) impl_->ok = 0;
        else if (plane == 1) impl_->covok = 0;
        else impl_->uncok = 0;
        return P3_OUT_OK;   // 判定已置位；不在读路径上抛 IO
    }
    long fp[2] = {x0 + 1, y0 + 1};
    long lp[2] = {x0 + w, y0 + h};
    // cfitsio 的 ffgsv 会**无条件**解引用 inc（getcol.c:433 起）⇒ 必须给合法步长，
    // 传 nullptr 是空指针解引用（实测 SIGSEGV, si_addr=NULL）。
    long inc[2] = {1, 1};
    std::vector<float> buf(static_cast<std::size_t>(w) * h);
    int anynul = 0;
    if (fits_read_subset(impl_->f, TFLOAT, fp, lp, inc, nullptr, buf.data(),
                         &anynul, &status) != 0) {
        g_last_err = "fits_read_subset failed: " + std::to_string(status);
        return P3_OUT_IO;
    }
    const std::size_t n = static_cast<std::size_t>(w) * h;
    if (plane == 1) {
        for (std::size_t i = 0; i < n; ++i) {
            if ((buf[i] > 0.5f) != (expected[i] > 0.5f)) { impl_->covok = 0; break; }
        }
        for (std::size_t i = 0; i < n; ++i) if (buf[i] > 0.5f) ++impl_->covered;
    } else {
        for (std::size_t i = 0; i < n; ++i) {
            // NaN 语义: 双方都是 NaN → 一致（源无覆盖 = NaN）；否则逐值精确回环
            const bool sn = (expected[i] != expected[i]);
            const bool rd = (buf[i] != buf[i]);
            if (buf[i] != expected[i] && !(sn && rd)) {
                if (plane == 0) impl_->ok = 0;
                else impl_->uncok = 0;
                break;
            }
        }
    }
    return P3_OUT_OK;
}

P3OutputStatus P3FitsVerifyStream::close(P3OutputResult* result) {
    if (!impl_) return P3_OUT_PARAM;
    if (impl_->f) {
        int st = 0;
        fits_close_file(impl_->f, &st);
        impl_->f = nullptr;
    }
    impl_->lock.reset();
    if (result) {
        std::memset(result, 0, sizeof(*result));
        result->reopen_ok = (impl_->ok == 1 && impl_->covok == 1 &&
                             impl_->uncok == 1 && impl_->wcsok == 1)
                                ? 1 : 0;
        result->coverage_ok = impl_->covok;
        result->covered_px = impl_->covered;
        result->total_px = (long)impl_->width * impl_->height;
        std::string h;
        if (!sha256_file_checked(impl_->path.c_str(), &h)) {
            g_last_err = "sha256_file(verify) failed";
            return P3_OUT_IO;
        }
        std::snprintf(result->sha256, sizeof(result->sha256), "%s", h.c_str());
    }
    return P3_OUT_OK;
}

}  // namespace astrocs::phase3
