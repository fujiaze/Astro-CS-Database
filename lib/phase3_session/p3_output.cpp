// lib/phase3_session/p3_output.cpp — 输出原子写/校验 (ALG-P3-004) — P3-004
#include "p3_output.h"

#if defined(_WIN32)
// MSVC 无 unistd.h; 以 _ 前缀 CRT 提供 POSIX 文件操作为别名
#include <windows.h>
#include <io.h>
#include <process.h>
#include <direct.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef O_RDONLY
#define O_RDONLY _O_RDONLY
#endif
#ifndef getpid
#define getpid _getpid
#endif
#ifndef unlink
#define unlink _unlink
#endif
#ifndef fsync
#define fsync _commit
#endif
#ifndef close
#define close _close
#endif
#ifndef open
#define open _open
#endif
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
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
#include "../astro_image_io/src/aio_cfitsio_mutex.h"
#include "sha256.h"

#include <vector>

namespace astrocs::phase3 {

namespace {
std::string g_last_err;

// B2-A9: 标准 FITS 校验和 = CFITSIO fits_write_chksum（IAU FITS 4.0
// §4.4.2.5 的 1 补码 32-bit 累加，DATASUM 十进制字符串 + CHECKSUM 16 字符）
// 逐 HDU 写出并由 cfitsio 自行归属。旧实现自算 "little-endian 无进位字节和"
// 并以 TINT 整数写入保留字 DATASUM，是非法关键字值（astropy checksum=True
// 报 Datasum verification failed），已删除。
bool fits_write_std_chksum(fitsfile* f, std::string* why) {
    int status = 0;
    if (fits_write_chksum(f, &status)) {
        if (why) *why = "fits_write_chksum: " + std::to_string(status);
        return false;
    }
    return true;
}

bool make_temp_path(const std::string& out, std::string* tmp) {
    char host[64] = {0};
#if defined(_WIN32)
    DWORD host_len = sizeof(host) - 1;
    GetComputerNameA(host, &host_len);
#else
    gethostname(host, sizeof(host) - 1);
#endif
    *tmp = out + "." + std::to_string(::getpid()) + ".tmp";
    // 若 out 无目录, 用当前目录; tmp 与 out 同目录保证 rename 原子
    return true;
}

// R10-C(bughunt p2): sha256_file 的失败可见封装。lib/common/crypto::sha256_file
// 对 fopen 失败返回空串、对 fread 中途错误静默返回前缀(部分数据)哈希 —— 任一
// 形态写进 provenance 即为无意义完整性锚。本封装逐项检查 fopen/ferror/fclose,
// 只有完整读取成功才产出 64hex; 失败返回 false, 调用方必须把错误向上传播
// (整体输出失败), 禁止把空串/前缀哈希当作结果。
// ASTROCS_HASH_FAIL_INJECT (仅测试构建, -Dastrocs_hash_fail_inject 编入):
// 在完整读出后于 final 前注入一次 I/O 错误 → 走失败分支, 供单测断言不写假哈希。
bool sha256_file_checked(const char* path, std::string* hex_out) {
    hex_out->clear();
    astrocs::crypto::Sha256 h;
    std::FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    unsigned char buf[64 * 1024];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) h.update(buf, n);
    const bool read_ok = (std::ferror(f) == 0);
    const bool close_ok = (std::fclose(f) == 0);
    if (!read_ok || !close_ok) return false;
#ifdef astrocs_hash_fail_inject
    if (std::getenv("ASTROCS_HASH_FAIL_INJECT")) return false;
#endif
    *hex_out = h.final_hex();
    return true;
}

}  // namespace

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
    std::lock_guard<std::mutex> cfitsio_guard(aio::cfitsio_io_mutex());

    std::string tmp;
    make_temp_path(output_path, &tmp);
    // 清理历史残留 tmp
    ::unlink(tmp.c_str());
    fitsfile* f = nullptr;
    int status = 0;

    long naxes[2] = {width, height};
    if (fits_create_file(&f, tmp.c_str(), &status)) {
        g_last_err = "fits_create_file(tmp): " + std::to_string(status);
        return P3_OUT_IO;
    }
    if (bitpix != -32 && bitpix != -64) {
        ::unlink(tmp.c_str());
        g_last_err = "bitpix must be -32|-64";
        return P3_OUT_PARAM;
    }
    if (fits_create_img(f, bitpix, 2, naxes, &status)) {
        ::unlink(tmp.c_str());
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
        const char* unit = bunit ? bunit : "ADU";
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
        ::unlink(tmp.c_str());
        return P3_OUT_CANCELLED;
    }

    // B2-A9: PRIMARY HDU 标准 DATASUM/CHECKSUM（旧实现 PRIMARY 无 DATASUM）。
    {
        std::string why;
        if (!fits_write_std_chksum(f, &why)) {
            g_last_err = why;
            fits_close_file(f, &status);
            ::unlink(tmp.c_str());
            return P3_OUT_IO;
        }
    }

    // 追加 coverage 扩展 HDU
    long cnaxes[2] = {width, height};
    if (fits_create_img(f, bitpix, 2, cnaxes, &status)) {
        ::unlink(tmp.c_str());
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
            ::unlink(tmp.c_str());
            return P3_OUT_IO;
        }
    }

    // 追加 uncertainty 扩展 HDU (DATA-P3-UNC-001 §30.4/§27.2 目标态行):
    // VARIANCE (BUNIT=<BUNIT>^2) + IVAR (BUNIT=1/(<BUNIT>^2)), DATASUM 逐 HDU;
    // 与主/扩展 HDU 同一原子发布序 (取消不落盘语义由上方 cancelled 分支保持)。
    if (variance && ivar) {
        const char* unit = (bunit && *bunit) ? bunit : "ADU";
        std::string var_bunit = std::string(unit) + "^2";
        std::string ivar_bunit = std::string("1/(") + unit + "^2)";
        for (int h = 0; h < 2; ++h) {
            if (fits_create_img(f, bitpix, 2, cnaxes, &status)) {
                ::unlink(tmp.c_str());
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
                ::unlink(tmp.c_str());
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
            ::unlink(tmp.c_str());
            return P3_OUT_IO;
        }
        fits_close_file(f, &status);
        if (status) { ::unlink(tmp.c_str()); g_last_err = "close: " + std::to_string(status); return P3_OUT_IO; }
        // ② 内容已完整写出后再 fsync fd; 打开/fsync 失败都是发布失败
        {
            const char* p = tmp.c_str();
            // Windows: _commit(=fsync 映射, FlushFileBuffers) 要求可写句柄,
            // O_RDONLY fd 必报 EBADF(R18 34201181796 诊断 errno=9 实证)。
            // O_RDWR 打开(不改内容)后 fsync/_commit 语义与 POSIX 一致。
#if defined(_WIN32)
            int fd = ::open(p, O_RDWR);
#else
            int fd = ::open(p, O_RDONLY);
#endif
            if (fd < 0) {
                g_last_err = std::string("open(tmp) for fsync: ") + std::strerror(errno);
                ::unlink(tmp.c_str());
                return P3_OUT_IO;
            }
            if (::fsync(fd) != 0) {
                const int fsync_err = errno;
                ::close(fd);
                g_last_err = std::string("fsync: ") + std::strerror(fsync_err);
                ::unlink(tmp.c_str());
                return P3_OUT_IO;
            }
            if (::close(fd) != 0) {
                g_last_err = std::string("close(fsync fd): ") + std::strerror(errno);
                ::unlink(tmp.c_str());
                return P3_OUT_IO;
            }
        }
    }
    if (::rename(tmp.c_str(), output_path) != 0) {
        g_last_err = std::string("rename: ") + std::strerror(errno);
        ::unlink(tmp.c_str());
        return P3_OUT_IO;
    }

    // 计算 sha256(重新读出的完整文件) 并独立重开验证(重开验证)
    if (result) {
        std::string h;
        // R10-C: 哈希失败 = 完整性锚缺失 → 不写空串/前缀哈希, 整体输出失败
        if (!sha256_file_checked(output_path, &h)) {
            g_last_err = "sha256_file(published output) failed";
            ::unlink(output_path);
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

}  // namespace astrocs::phase3
