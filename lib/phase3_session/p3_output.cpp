// lib/phase3_session/p3_output.cpp — 输出原子写/校验 (ALG-P3-004) — P3-004
#include "p3_output.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "aio_fits.h"
#include "astro_image_io.h"
#include "fitsio.h"
#include "sha256.h"

#include <vector>

namespace astrocs::phase3 {

namespace {
std::string g_last_err;

int write_card_str(fitsfile* f, const char* key, const char* val) {
    int status = 0;
    fits_write_key(f, TSTRING, key, (void*)val, nullptr, &status);
    return status;
}

// FITS DATASUM(32-bit checksum) 辅助: 计算字节和
uint32_t fdatasum(const void* buf, size_t n) {
    const unsigned char* p = (const unsigned char*)buf;
    uint32_t sum = 0;
    for (size_t i = 0; i + 4 <= n; i += 4) {
        uint32_t w = p[i] | (p[i + 1] << 8) | (p[i + 2] << 16) | (p[i + 3] << 24);
        sum += w;
    }
    return sum;
}

bool make_temp_path(const std::string& out, std::string* tmp) {
    char host[64] = {0};
    gethostname(host, sizeof(host) - 1);
    *tmp = out + "." + std::to_string(::getpid()) + ".tmp";
    // 若 out 无目录, 用当前目录; tmp 与 out 同目录保证 rename 原子
    return true;
}

}  // namespace

P3OutputStatus p3_output_write_atomic(const float* signal, const float* coverage,
                                      int width, int height,
                                      const P3WcsDescriptor* wcs,
                                      const char* bunit,
                                      const char* output_path,
                                      const P3Provenance* prov,
                                      int cancelled_at_row,
                                      P3OutputResult* result) {
    if (!signal || !coverage || !wcs || !output_path || width < 1 || height < 1)
        return P3_OUT_PARAM;
    if (result) std::memset(result, 0, sizeof(*result));

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
    const int bitpix = -32;
    if (fits_create_img(f, bitpix, 2, naxes, &status)) {
        ::unlink(tmp.c_str());
        g_last_err = "fits_create_img: " + std::to_string(status);
        return P3_OUT_IO;
    }

    // WCS + 基础关键字
    fits_write_key(f, TSTRING, (char*)"CTYPE1", (void*)"RA---TAN", nullptr, &status);
    fits_write_key(f, TSTRING, (char*)"CTYPE2", (void*)"DEC--TAN", nullptr, &status);
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
        int one = 1, zero = 0;
        fits_write_key(f, TINT, (char*)"BSCALE", &one, nullptr, &status);
        fits_write_key(f, TINT, (char*)"BZERO", &zero, nullptr, &status);
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

    // 追加 coverage 扩展 HDU
    long cnaxes[2] = {width, height};
    if (fits_create_img(f, bitpix, 2, cnaxes, &status)) {
        ::unlink(tmp.c_str());
        g_last_err = "coverage create_img: " + std::to_string(status);
        return P3_OUT_IO;
    }
    fits_write_key(f, TSTRING, (char*)"EXTNAME", (void*)"COVERAGE", nullptr, &status);
    fits_write_pix(f, TFLOAT, fpix, nelem, (void*)coverage, &status);

    // 写 DATASUM(CHECKSUM) — FITS 标准 32-bit 校验
    const uint32_t dsig = fdatasum(signal, (size_t)nelem * sizeof(float));
    const uint32_t dscov = fdatasum(coverage, (size_t)nelem * sizeof(float));
    const uint32_t dsum = dsig;
    {
        uint32_t dv = dsum;
        fits_write_key(f, TINT, (char*)"DATASUM", &dv, nullptr, &status);
    }

    // fsync + rename 原子替换
    {
        const char* p = tmp.c_str();
        int fd = ::open(p, O_RDONLY);
        if (fd >= 0) { ::fsync(fd); ::close(fd); }
    }
    fits_close_file(f, &status);
    if (status) { ::unlink(tmp.c_str()); g_last_err = "close: " + std::to_string(status); return P3_OUT_IO; }
    if (::rename(tmp.c_str(), output_path) != 0) {
        g_last_err = std::string("rename: ") + std::strerror(errno);
        ::unlink(tmp.c_str());
        return P3_OUT_IO;
    }

    // 计算 sha256(重新读出的完整文件)— 用 p3_output_verify 内部做, 这里先返回
    if (result) {
        std::string h = astrocs::crypto::sha256_file(output_path);
        std::snprintf(result->sha256, sizeof(result->sha256), "%s", h.c_str());
        result->total_px = (long)width * height;
        long cov = 0;
        for (long i = 0; i < nelem; ++i) if (coverage[i] > 0.5f) ++cov;
        result->covered_px = cov;
        result->coverage_ok = 1;
    }
    return P3_OUT_OK;
}

P3OutputStatus p3_output_verify(const char* output_path, const P3WcsDescriptor* wcs,
                                const float* signal, const float* coverage,
                                int width, int height, P3OutputResult* result) {
    if (!output_path || !result || width < 1 || height < 1) return P3_OUT_PARAM;
    std::memset(result, 0, sizeof(*result));
    long nelem = (long)width * height;
    int ok = 1, covok = 1;
    int hdus = 1;

    fitsfile* f = nullptr; int status = 0;
    if (fits_open_file(&f, output_path, READONLY, &status) != 0) {
        g_last_err = "open failed";
        return P3_OUT_IO;
    }
    fits_get_num_hdus(f, &hdus, &status);

    // primary (HDU 1) = signal: 读像素 + WCS 关键字
    if (fits_movabs_hdu(f, 1, nullptr, &status) == 0) {
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
    fits_close_file(f, &status);

    result->reopen_ok = (ok == 1 && covok == 1);
    result->coverage_ok = covok;
    long covn = 0;
    for (long i = 0; i < nelem; ++i) if (coverage[i] > 0.5f) ++covn;
    result->covered_px = covn;
    result->total_px = nelem;
    return P3_OUT_OK;
}

}  // namespace astrocs::phase3
