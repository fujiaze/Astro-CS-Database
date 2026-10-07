// lib/algorithms/fits_output/p3_output.cpp — 输出原子写/校验 (ALG-P3-004) — P3-004
// 原址 lib/phase3_session/p3_output.cpp, 按 docs/ACSD_DESIGN
// §7.1「fits_output」行迁入本模块; 源逐字节等价 (仅头注 | include 面改锚)。
#include "p3_output.h"

#if defined(_WIN32)
// MSVC 无 unistd.h; 以 _ 前缀 CRT 提供 pid 别名。
// open/fsync/close/unlink 别名已删除 —— 这些文件系统原语现全部
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
// 原相对路径 ../infrastructure/aio/src/... 只在旧址 (lib/phase3_session/)
// 成立; 该目录已由 acsd_aio 的 PUBLIC include 面提供 ⇒ 扁平引用 (迁址无关)。
#include "aio_cfitsio_mutex.h"
#include "sha256.h"
// docs/ACSD_DESIGN §10「统一 I/O 是文件级唯一边界」:
// 本模块**不再**自持文件系统原语 —— 临时文件/fsync/原子 rename/删除经 aio
// 机制原语 (aio_atomic_file.h), 文件内容摘要经 aio 摘要原语
// (aio_file_io.h)。二者均为 aio 内唯一实现 (header-only 机制面)。
#include "aio_atomic_file.h"
#include "aio_file_io.h"

#include <memory>
#include <vector>

namespace acsd::phase3 {

namespace {
std::string g_last_err;

// B2-A9: 标准 FITS 校验和 = CFITSIO fits_write_chksum（IAU FITS 4.0
// §4.4.2.5 的 1 补码 32-bit 累加，DATASUM 十进制字符串 + CHECKSUM 16 字符）
// 逐 HDU 写出并由 cfitsio 自行归属。旧实现自算 "little-endian 无进位字节和"
// 并以 TINT 整数写入保留字 DATASUM，是非法关键字值（astropy checksum=True
// 报 Datasum verification failed），已删除。
// FZ-P3-BUNIT-QUADRATIC / docs/science/unified/DATA_SEMANTICS.md
// 「单位与量纲表」+「面亮度单位的推导」两节:
// variance BUNIT = (signal BUNIT)^2, ivar = 1/variance —— 用**冻结单位表的 canonical
// 串**（ADU^a × 立体角幂次代数；写侧一律 "sr"），禁朴素字符串拼接（"ADU/sr" + "^2"
// = "ADU/sr^2" 既非 canonical 也不可判）。解析失败 → false（调用方显式拒绝，
// 禁写出非二次律 BUNIT）。
bool bunit_square_canonical(const std::string& signal, std::string* variance,
                            std::string* ivar) {
    // 解析单因子 "ADU^a"（幂次可省略=1、可带负号；"1" = 0/0）
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
    // 分母因子的立体角幂次（符号 "sr"；legacy 读侧别名 "px"/"pixel" 同幂次，
    // docs/science/unified/DATA_SEMANTICS.md「单位与量纲表」一节：写盘单位串一律用
    // `sr`，读侧兼容同幂次的 `px`、`pixel` 写法 ⇒ 旧冻结表把像元面积记作 px^N
    // 与 sr^(N/2) 是同一立体角维）。
    auto parse_area_pow = [&parse_pow](const std::string& s, int* sr_out) -> bool {
        int e = 0;
        if (parse_pow(s, "sr", &e)) { *sr_out = e; return true; }
        if (parse_pow(s, "pixel", &e) || parse_pow(s, "px", &e)) {
            if (e % 2 != 0) return false;   // 面积幂次须为偶（无整数 sr 等价）
            *sr_out = e / 2;
            return true;
        }
        return false;
    };
    // 冻结表书写形式（分母正幂次，符号一律 "sr"）: {+1,-1} → "ADU/sr";
    // {+2,-2} → "ADU^2/sr^2"; {-2,+2} → "sr^2/ADU^2"; {+1,0} → "ADU";
    // {-2,0} → "ADU^-2"（(adu, sr) = ADU 幂次 × 立体角符号幂次）。
    auto canon = [](int adu, int sr) -> std::string {
        if (adu == 0 && sr == 0) return "1";
        if (adu > 0) {
            std::string s = "ADU";
            if (adu != 1) s += "^" + std::to_string(adu);
            if (sr != 0) {
                s += "/sr";
                if (sr != -1) s += "^" + std::to_string(-sr);
            }
            return s;
        }
        if (adu < 0) {
            if (sr != 0) {
                std::string s = "sr";
                if (sr != 1) s += "^" + std::to_string(sr);
                s += "/ADU";
                if (adu != -1) s += "^" + std::to_string(-adu);
                return s;
            }
            // 纯 ADU 负幂次: 冻结表写带符号指数（flux ivar = "ADU^-2"）
            return std::string("ADU^-") + std::to_string(-adu);
        }
        std::string s = "sr";
        if (sr != 1) s += "^" + std::to_string(sr);
        return s;
    };
    std::string t;
    for (char c : signal) {
        if (c != ' ' && c != '\t') t += c;
    }
    if (t.empty()) return false;
    int adu = 0, sr = 0;
    if (t == "1") {
        adu = 0; sr = 0;
    } else {
        const size_t slash = t.find('/');
        const std::string left = (slash == std::string::npos) ? t : t.substr(0, slash);
        if (!parse_pow(left, "ADU", &adu)) return false;
        if (slash != std::string::npos) {
            int written = 0;
            if (!parse_area_pow(t.substr(slash + 1), &written)) return false;
            sr = -written;   // 分母形式 "sr^N" ⇒ 带符号幂次 -N
        }
    }
    if (variance) *variance = canon(adu * 2, sr * 2);
    if (ivar) *ivar = canon(-adu * 2, -sr * 2);
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

// P-206 (台账 B-1 = 在册 P-085): verify 面的 DATASUM/CHECKSUM 对拍。
// 与写侧 fits_write_chksum 同一 cfitsio 实现（标准 FITS 1 补码 32-bit 累加），
// 读回重算并与关键字比对 —— 此前 verify 只比像素/关键字，对「数据被改成另一份
// 合法数值」或「DATASUM 卡被篡改」零鉴别力（sha256 只对文件整体成立，
// 改写方重算一次 sha256 即可盖过）。
// 标准语义（cfitsio quick.tex fits_verify_chksum）: dataok/hduok = 1 校验通过,
// 0 关键字缺失, -1 校验不符。合同判据（IO_003 §8.4）: 写入侧恒写 DATASUM 卡
// ⇒ DATASUM 必须存在且正确（0/-1 皆判红）；CHECKSUM 卡存在且非占位时同样校验
// （缺失 = 0 可接受，不符 = -1 判红）。
bool verify_hdu_chksum(fitsfile* f, std::string* why) {
    int dataok = 0, hduok = 0, status = 0;
    if (fits_verify_chksum(f, &dataok, &hduok, &status)) {
        if (why) *why = "fits_verify_chksum: " + std::to_string(status);
        return false;
    }
    if (dataok != 1) {
        if (why) *why = std::string("DATASUM ") +
                        (dataok == 0 ? "missing" : "mismatch");
        return false;
    }
    if (hduok == -1) {
        if (why) *why = "CHECKSUM mismatch";
        return false;
    }
    return true;
}

// P-205/P-206 (流式面): 发布前对 tmp 逐 HDU 做结构 + DATASUM/CHECKSUM 对拍
// （IO_003 §4 的「fitsverify（结构 + DATASUM）」步骤，位于 rename 之前）。
// 直接 cfitsio 调用: 调用方（P3FitsStream::publish/open）已持进程级 cfitsio 锁
// (RT-008)，CfitsioLockGuard 不可重入。
bool verify_tmp_chksums(const std::string& path, std::string* why) {
    fitsfile* f = nullptr;
    int status = 0;
    if (fits_open_file(&f, path.c_str(), READONLY, &status) != 0) {
        if (why) *why = "pre-publish open(tmp) failed: " + std::to_string(status);
        return false;
    }
    int hdus = 0;
    status = 0;
    fits_get_num_hdus(f, &hdus, &status);
    bool ok = hdus >= 1;
    for (int i = 1; i <= hdus && ok; ++i) {
        status = 0;
        if (fits_movabs_hdu(f, i, nullptr, &status) != 0) { ok = false; break; }
        ok = verify_hdu_chksum(f, why);
    }
    status = 0;
    fits_close_file(f, &status);
    return ok;
}

bool make_temp_path(const std::string& out, std::string* tmp) {
    // 死变量清理: 原实现取 hostname 到 host[] 后从未读取, 且该
    // 取值不参与临时名 ⇒ 删除 (临时名语义逐位不变: <out>.<pid>.tmp)。
    *tmp = out + "." + std::to_string(::getpid()) + ".tmp";
    // 若 out 无目录, 用当前目录; tmp 与 out 同目录保证 rename 原子
    return true;
}

// R10-C(bughunt p2): sha256_file 的失败可见封装。lib/algorithms/shared/crypto::sha256_file
// 对文件打开失败返回空串、对读取中途错误静默返回前缀(部分数据)哈希 —— 任一
// 形态写进 provenance 即为无意义完整性锚。该纪律下沉到 aio
// (aio_file_io.h): 只有完整读取成功才产出 64hex; 失败返回 false, 调用方必须把错误向上传播
// (整体输出失败), 禁止把空串/前缀哈希当作结果。
// ACSD_HASH_FAIL_INJECT (仅测试构建, -Dacsd_hash_fail_inject 编入):
// 在完整读出后于 final 前注入一次 I/O 错误 → 走失败分支, 供单测断言不写假哈希。
bool sha256_file_checked(const char* path, std::string* hex_out) {
    hex_out->clear();
    // 文件打开/流式读取/关闭机制在 aio 内 (aio_file_io.h
    // aio_file::sha256_hex) —— 本模块不再自持 FILE* 通道。R10-C 语义不变:
    // 只有完整读取成功才产出 64hex, 失败返回 false 且清空输出。
    if (!aio_file::sha256_hex(path, hex_out)) return false;
#ifdef acsd_hash_fail_inject
    if (std::getenv("ACSD_HASH_FAIL_INJECT")) { hex_out->clear(); return false; }
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
    // slim 唯一形态（task-9）：只写 PRIMARY signal 单 HDU。coverage 入参保留
    // 仅供覆盖统计（covered_px），不落盘。
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
    // slim 唯一形态（task-9）：禁 variance/ivar（整幅+流式+参考路径均不写
    // VARIANCE/IVAR；见 docs/detail/registry/acsd.phase3.writer.md 单 HDU 节）。
    if (variance != nullptr || ivar != nullptr) {
        g_last_err = "slim: variance/ivar planes are not written (single-HDU product)";
        return P3_OUT_PARAM;
    }
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
        // P-076 (台账 A2): 先关闭句柄再删除 —— aio_atomic::remove_file 不关句柄,
        // 对仍打开的文件 Windows (_unlink) 必败 → tmp 残留 + fd 泄漏; 全平台同理。
        fits_close_file(f, &status);
        aio_atomic::remove_file(tmp);
        g_last_err = "bitpix must be -32|-64";
        return P3_OUT_PARAM;
    }
    if (fits_create_img(f, bitpix, 2, naxes, &status)) {
        fits_close_file(f, &status);   // P-076: close 先于 remove (Windows _unlink 必败点)
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
        // 主 HDU = 重采样后的**面亮度**平面（Phase3 FITS 写出与 HiPS 重采样两模块的
        // 端口见 docs/detail/registry/acsd.phase3.writer.md 与
        // docs/detail/registry/acsd.phase3.resample2.md 的
        // 「输入输出端口、DATA、单位、坐标、invalid」一节），单位口径见
        // `docs/science/unified/DATA_SEMANTICS.md`「面亮度单位的推导」一节；缺省串取该平面的物理单位 canonical "ADU/sr"
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

    // slim 唯一形态（task-9）：只写 PRIMARY signal 单 HDU，不写 COVERAGE /
    // VARIANCE / IVAR 扩展层。coverage 入参只用于 covered_px 统计。

    // 发布序（正本 = IO_003 §4 / ACSD_DESIGN.md §10:732）:
    //   私有临时区 → 关闭/fsync → 校验（结构 + DATASUM/CHECKSUM）→ 算哈希 → 原子改名
    // P-205 (台账 A-4): 原序是 flush → close → fsync → **rename → sha256/校验**
    // —— 哈希与校验落在 rename 之后，与 §10 及 IO_003 §4 的逐步骤序相悖（rename
    // 先于完整性判据 ⇒ 校验/哈希失败时正式路径已出现对象，只能回滚删除）。
    // R10-C 子序（cfitsio 缓冲 flush → fsync(fd)）保持不变。
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
        // fsync 机制在 aio (aio_atomic::fsync_path) —— Windows 的
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
    // ③ 校验（rename 之前，针对 tmp）：独立重开回环 —— dimensions/WCS/
    //    BUNIT 冻结表+二次律/mask/uncertainty HDU 面 + **DATASUM/CHECKSUM
    //    对拍**（P-206）。任一判据不过 ⇒ 删 tmp、正式路径零出现，绝不把
    //    「无完整性锚/自证失败」的对象放到用户路径上（fail-closed）。
    {
        P3OutputResult v{};
        const P3OutputStatus vst = p3_output_verify_ex(
            tmp.c_str(), wcs, signal, coverage, variance, ivar, width, height, &v);
        if (vst != P3_OUT_OK) {
            g_last_err = "pre-publish verify(tmp) failed: " + g_last_err;
            aio_atomic::remove_file(tmp);
            return vst;
        }
        if (v.reopen_ok != 1) {
            g_last_err = "pre-publish verify(tmp): reopen_ok=0 (integrity)";
            aio_atomic::remove_file(tmp);
            return P3_OUT_IO;
        }
        if (result) {
            result->reopen_ok = v.reopen_ok;
            result->total_px = v.total_px;
            result->covered_px = v.covered_px;
            result->coverage_ok = v.coverage_ok;
        }
    }
    // ④ 内容哈希（rename 之前，针对 tmp）：仅完整读出后填写；失败 = 完整性锚
    //    缺失 ⇒ 整体失败，不写空串/前缀哈希（R10-C 语义不变，位置移到发布前）。
    std::string pub_sha;
    if (!sha256_file_checked(tmp.c_str(), &pub_sha)) {
        g_last_err = "sha256_file(tmp) failed";
        aio_atomic::remove_file(tmp);
        return P3_OUT_IO;
    }
    // ⑤ 原子 rename 机制在 aio (aio_atomic::atomic_replace)。此前所有判据已过，
    //    故 rename 之后不再有任何可能导致「已发布但无效」的判据。
    if (aio_atomic::atomic_replace(tmp, output_path) != 0) {
        g_last_err = std::string("rename: ") + std::strerror(errno);
        aio_atomic::remove_file(tmp);
        return P3_OUT_IO;
    }
    if (result) {
        std::snprintf(result->sha256, sizeof(result->sha256), "%s", pub_sha.c_str());
        long cov = 0;
        for (long i = 0; i < nelem; ++i) if (coverage[i] > 0.5f) ++cov;
        result->total_px = (long)width * height;
        result->covered_px = cov;
        result->coverage_ok = 1;
        result->reopen_ok = 1;
    }
    return P3_OUT_OK;
}

P3OutputStatus p3_output_verify(const char* output_path, const P3WcsDescriptor* wcs,
                                const float* signal, const float* coverage,
                                int width, int height, P3OutputResult* result) {
    // slim 唯一形态（task-9）：单 HDU（仅 PRIMARY signal）。
    // coverage 入参只用于 covered_px 统计，不对应落盘 HDU。
    return p3_output_verify_ex(output_path, wcs, signal, coverage, nullptr, nullptr,
                               width, height, result);
}

P3OutputStatus p3_output_verify_ex(const char* output_path,
                                   const P3WcsDescriptor* wcs,
                                   const float* signal, const float* coverage,
                                   const float* variance, const float* ivar,
                                   int width, int height, P3OutputResult* result) {
    if (!output_path || !result || width < 1 || height < 1) return P3_OUT_PARAM;
    // slim 唯一形态（task-9）：禁 variance/ivar（下游校验按单 HDU 预期执行）。
    if (variance != nullptr || ivar != nullptr) {
        g_last_err = "slim: variance/ivar verify is not supported (single-HDU product)";
        return P3_OUT_PARAM;
    }
    // B2-A9: verify 对 WCS 零鉴别力是审计缺陷（AUD-COORD F-05）。此处读回
    // CTYPE/CUNIT/CRPIX/CRVAL/CD 与传入 descriptor 逐项对拍，任何 CRPIX 平移、
    // origin 双桥接或 CD 篮改都会被检出并置 reopen_ok=0（AUD-P2P3 F24）。
    // P-075 (台账 A1): BUNIT 同入对拍面 —— PRIMARY 要求存在且在冻结单位表内
    // （bunit_square_canonical 可解析）；不一致 ⇒ reopen_ok=0。
    // 容差来源: 写路径以 TDOUBLE 写 double，读回亦为 double，round-trip 应为
    // 位精确；1e-12(度/像素) / 1e-15(CD deg/px) 仅吸收格式层十进制往返。
    std::memset(result, 0, sizeof(*result));
    long nelem = (long)width * height;
    int ok = 1, covok = 1, uncok = 1, wcsok = 1, bunitok = 1;
    // P-206 (在册 P-085): 逐 HDU 的 DATASUM/CHECKSUM 对拍位（含于 reopen_ok）。
    int chksumok = 1;
    int hdus = 1;
    // slim 唯一形态（task-9）：无 VARIANCE/IVAR BUNIT 二次律对拍。

    fitsfile* f = nullptr; int status = 0;
    if (fits_open_file(&f, output_path, READONLY, &status) != 0) {
        g_last_err = "open failed";
        return P3_OUT_IO;
    }
    fits_get_num_hdus(f, &hdus, &status);

    // primary (HDU 1) = signal: 读像素 + WCS 关键字(C1 对拍)
    if (fits_movabs_hdu(f, 1, nullptr, &status) == 0) {
        // P-206: PRIMARY 的 DATASUM/CHECKSUM 对拍（与像素/关键字对拍同面）
        if (!verify_hdu_chksum(f, nullptr)) chksumok = 0;
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
        // P-075 (台账 A1): BUNIT 读回对拍 —— 与 CTYPE/CUNIT 同模式（读键→去
        // 补白→比较）。写侧只发布冻结单位表内的 signal BUNIT（缺省 canonical
        // "ADU/sr"），故读回要求: 键存在、非空、bunit_square_canonical 可解析；
        // 表外串只能来自篡改/损坏 ⇒ bunitok=0。
        {
            char bunit_val[81] = {0};
            status = 0;
            if (fits_read_key(f, TSTRING, (char*)"BUNIT", bunit_val, nullptr,
                              &status) != 0) {
                bunitok = 0;   // BUNIT 缺失 = 对拍面不完整
            } else {
                std::string got(bunit_val);
                while (!got.empty() && (got.back() == ' ' || got.back() == '\t'))
                    got.pop_back();
                if (got.empty() || !bunit_square_canonical(got, nullptr, nullptr))
                    bunitok = 0;   // 表外/空串单位: 写侧禁发布, 读回即篡改/损坏
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

    // slim 唯一形态（task-9）：单 HDU 预期 —— 任何扩展 HDU（COVERAGE /
    // VARIANCE / IVAR / 其他占位）一律判红（下游校验按单 HDU 预期执行）。
    if (hdus != 1) {
        uncok = 0;
        covok = 0;
        status = 0;
    }
    fits_close_file(f, &status);

    result->reopen_ok = (ok == 1 && covok == 1 && uncok == 1 && wcsok == 1 &&
                         bunitok == 1 && chksumok == 1);
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
// 规范：docs/ACSD_DESIGN §8.3 export 行「子块流式：读子块 → 投影重采样 → 写
// FITS，有界队列 + 背压，不整幅驻留；I/O 与计算重叠，内存占用与子块大小成
// 正比、与总图大小无关」；docs/engineering/contracts/SCHEDULER.md「三阶段调度形态（冻结）」一节的 export 行同文。
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
    int cur_hdu = 0;          // slim 唯一形态（task-9）：恒 0=PRIMARY(signal)
    bool hdu_open = false;    // 当前 HDU 已建、尚未写 DATASUM/CHECKSUM
    bool failed = false;
    // slim 唯一形态（task-9）：发布门状态 —— hdu_done[0] = PRIMARY 已 end_hdu
    // 收尾（DATASUM/CHECKSUM 已写）即可发布。
    bool hdu_done[4] = {false, false, false, false};
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
    // 主 HDU = 重采样后的**面亮度**平面（与整幅路径同面，§27.1/§29.3）:
    // 缺省串取 `docs/science/unified/DATA_SEMANTICS.md`「面亮度单位的推导」一节
    // 的 canonical "ADU/sr"（裸 "ADU" 是每像素计数口径）。
    impl_->bunit = (bunit && *bunit) ? bunit : "ADU/sr";
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
    // slim 唯一形态（task-9）：只写 PRIMARY signal 单 HDU，禁 COVERAGE /
    // VARIANCE / IVAR 扩展层（流式路径与整幅路径同口径）。
    g_last_err = "slim: only PRIMARY HDU is written (single-HDU product)";
    impl_->failed = true;
    return P3_OUT_PARAM;
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
    impl_->hdu_done[impl_->cur_hdu] = true;   // P-077: 该 HDU 校验和已写、收尾完成
    return P3_OUT_OK;
}

P3OutputStatus P3FitsStream::publish(P3OutputResult* result) {
    if (!impl_ || !impl_->f || impl_->failed || impl_->hdu_open) return P3_OUT_IO;
    // slim 唯一形态（task-9）：发布门 —— 仅 PRIMARY 单 HDU 收尾即可发布
    //（无 COVERAGE / VARIANCE / IVAR 扩展层；fail-closed，不产半成品产品面）。
    if (!impl_->hdu_done[0]) {
        g_last_err = "publish gate: PRIMARY HDU not finished";
        abort();
        return P3_OUT_IO;
    }
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
    // 发布序（正本 = IO_003 §4 / ACSD_DESIGN.md §10:732）:
    //   临时写 → 关闭/fsync → 校验（结构 + DATASUM/CHECKSUM）→ 算哈希 → 原子改名
    // P-205 (台账 A-4): 原序把 rename 排在哈希之前 ⇒ 与 §10/IO_003 §4 相悖。
    // ② 内容已完整写出后再 fsync（机制在 aio）。
    const int frc = aio_atomic::fsync_path(impl_->tmp, 0);
    if (frc != 0) {
        g_last_err = std::string("fsync(tmp): ") + std::strerror(frc);
        abort();
        return P3_OUT_IO;
    }
    // ③ 校验（rename 之前，针对 tmp）: 逐 HDU 结构 + DATASUM/CHECKSUM 对拍。
    {
        std::string why;
        if (!verify_tmp_chksums(impl_->tmp, &why)) {
            g_last_err = "pre-publish verify(tmp): " + why;
            abort();
            return P3_OUT_IO;
        }
    }
    // ④ 内容哈希（rename 之前，针对 tmp）: 失败即整体失败，不留无完整性锚输出。
    {
        std::string h;
        if (!sha256_file_checked(impl_->tmp.c_str(), &h)) {
            g_last_err = "sha256_file(tmp) failed";
            abort();
            return P3_OUT_IO;
        }
        if (result) {
            std::snprintf(result->sha256, sizeof(result->sha256), "%s", h.c_str());
            result->total_px = (long)impl_->width * impl_->height;
        }
    }
    // ⑤ 原子 rename（此前所有判据已过 ⇒ rename 后不再有「已发布但无效」窗口）。
    if (aio_atomic::atomic_replace(impl_->tmp, impl_->out) != 0) {
        g_last_err = std::string("rename: ") + std::strerror(errno);
        abort();
        return P3_OUT_IO;
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
    // slim 唯一形态（task-9）：单 HDU 预期；has_uncertainty=true 即显式拒绝。
    if (has_uncertainty) {
        g_last_err = "slim: has_uncertainty=true is not supported (single-HDU product)";
        return P3_OUT_PARAM;
    }
    impl_->width = width;
    impl_->height = height;
    impl_->unc = false;
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
    // slim 唯一形态（task-9）：单 HDU 预期 —— 无 COVERAGE / VARIANCE /
    // IVAR 扩展层；任何扩展 HDU 一律判红（下游校验按单 HDU 预期执行）。
    // coverage 统计改由 check_block(plane=0) 的 signal NaN 计数承载（见下）。
    if (impl_->hdus != 1) {
        impl_->covok = 0;
        impl_->uncok = 0;
    }
    impl_->status = 0;
    return P3_OUT_OK;
}

P3OutputStatus P3FitsVerifyStream::check_block(int plane, int x0, int y0, int w,
                                               int h, const float* expected) {
    if (!impl_ || !impl_->f || !expected) return P3_OUT_PARAM;
    // slim 唯一形态（task-9）：只校验 plane=0（PRIMARY signal 单 HDU）。
    if (plane != 0 || w < 1 || h < 1) return P3_OUT_PARAM;
    if (x0 < 0 || y0 < 0 || x0 + w > impl_->width || y0 + h > impl_->height)
        return P3_OUT_PARAM;
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
    // slim 唯一形态（task-9）：仅 plane=0 signal 回环（NaN==NaN 同态）；
    // 覆盖统计 = signal 有限像素计数（无 COVERAGE HDU）。
    const std::size_t n = static_cast<std::size_t>(w) * h;
    for (std::size_t i = 0; i < n; ++i) {
        // NaN 语义: 双方都是 NaN → 一致（源无覆盖 = NaN）；否则逐值精确回环
        const bool sn = (expected[i] != expected[i]);
        const bool rd = (buf[i] != buf[i]);
        if (buf[i] != expected[i] && !(sn && rd)) {
            impl_->ok = 0;
            break;
        }
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (buf[i] == buf[i]) ++impl_->covered;
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

}  // namespace acsd::phase3
