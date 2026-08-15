#include "aio_fits.h"
#include "aio_log.h"
#include "aio_util.h"
#include "fitsio.h"
#include <cstdio>
#include <cctype>

// R10 修复: AIO 模块内部精度模式查询 (替代跨 DLL 的 PrecisionContext)
extern "C" int aio_internal_is_fp64();
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

// 安全定长拷贝 (memcpy + 显式 NUL; 规避 strncpy 截断警告)
static inline void aio_safe_copy(char* dst, std::size_t cap, const char* src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    const std::size_t n = cap - 1;
    std::size_t i = 0;
    while (i < n && src[i] != '\0') { dst[i] = src[i]; ++i; }
    dst[n] = '\0';
}

static const size_t FITS_BLOCK_SIZE = 2880;
static const size_t FITS_CARD_SIZE = 80;

static std::string trim_str(const std::string &s) {
    size_t start = s.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t");
    return s.substr(start, end - start + 1);
}

static std::string strip_fits_quotes(const std::string &s) {
    std::string v = trim_str(s);
    if (v.size() >= 2 && v.front() == '\'' && v.back() == '\'') {
        v = v.substr(1, v.size() - 2);
        size_t pos;
        while ((pos = v.find("''")) != std::string::npos) {
            v.replace(pos, 2, "'");
        }
        size_t end = v.find_last_not_of(' ');
        if (end != std::string::npos) v = v.substr(0, end + 1);
    }
    return v;
}

static void parse_card(const char card[80], AIOFITSKeyword &kw) {
    std::string key, val, cmt;
    std::string line(card, 80);

    size_t eq_pos = line.find('=');
    if (eq_pos == std::string::npos) {
        key = trim_str(line.substr(0, 8));
        if (key == "COMMENT" || key == "HISTORY" || key.empty()) {
            memset(&kw, 0, sizeof(kw));
            return;
        }
        aio_safe_copy(kw.name, AIO_KEYWORD_NAME_MAX, key.c_str());
        kw.value[0] = '\0';
        kw.comment[0] = '\0';
        return;
    }

    key = trim_str(line.substr(0, eq_pos));
    if (key.empty() || key.size() > 8) {
        memset(&kw, 0, sizeof(kw));
        return;
    }

    std::string rest = line.substr(eq_pos + 1);

    int slash_pos = -1;
    bool in_string = false;
    for (size_t i = 0; i < rest.size(); i++) {
        if (rest[i] == '\'') {
            in_string = !in_string;
        } else if (rest[i] == '/' && !in_string) {
            slash_pos = (int)i;
            break;
        }
    }

    if (slash_pos >= 0) {
        val = rest.substr(0, slash_pos);
        cmt = rest.substr(slash_pos + 1);
    } else {
        val = rest;
        cmt = "";
    }

    val = trim_str(val);
    if (!val.empty() && val[0] == '\'') {
        val = strip_fits_quotes(val);
    }

    cmt = trim_str(cmt);

    aio_safe_copy(kw.name, AIO_KEYWORD_NAME_MAX, key.c_str());
    aio_safe_copy(kw.value, AIO_KEYWORD_VALUE_MAX, val.c_str());
    aio_safe_copy(kw.comment, AIO_KEYWORD_COMMENT_MAX, cmt.c_str());
}

static int parse_fits_header(FILE *fp, FITSHeader &hdr) {
    hdr.keywords.clear();
    hdr.bitpix = 16;
    hdr.naxis = 0;
    hdr.naxis1 = hdr.naxis2 = hdr.naxis3 = 0;
    hdr.bscale = 1.0;
    hdr.bzero = 0.0;
    hdr.data_offset = 0;
    hdr.data_size = 0;

    char block[FITS_BLOCK_SIZE];
    bool end_found = false;
    size_t total_header_size = 0;

    while (!end_found) {
        size_t nread = std::fread(block, 1, FITS_BLOCK_SIZE, fp);
        if (nread < FITS_BLOCK_SIZE) {
            aio_log(AIO_LOG_ERROR, "FITS", "Header read incomplete: %zu bytes", nread);
            return -1;
        }
        total_header_size += FITS_BLOCK_SIZE;

        for (size_t i = 0; i < FITS_BLOCK_SIZE; i += FITS_CARD_SIZE) {
            char card[81];
            std::memcpy(card, block + i, FITS_CARD_SIZE);
            card[80] = '\0';

            std::string key = trim_str(std::string(card, 8));

            if (key == "END") {
                end_found = true;
                break;
            }

            if (key == "COMMENT" || key == "HISTORY" || key.empty()) continue;

            AIOFITSKeyword kw;
            memset(&kw, 0, sizeof(kw));
            parse_card(card, kw);

            if (kw.name[0] == '\0') continue;

            hdr.keywords.push_back(kw);

            if (key == "BITPIX") hdr.bitpix = std::atoi(kw.value);
            else if (key == "NAXIS") hdr.naxis = std::atoi(kw.value);
            else if (key == "NAXIS1") hdr.naxis1 = std::atoi(kw.value);
            else if (key == "NAXIS2") hdr.naxis2 = std::atoi(kw.value);
            else if (key == "NAXIS3") hdr.naxis3 = std::atoi(kw.value);
            else if (key == "BSCALE") { try { hdr.bscale = std::stod(kw.value); } catch (...) {} }
            else if (key == "BZERO") { try { hdr.bzero = std::stod(kw.value); } catch (...) {} }
        }
    }

    hdr.data_offset = total_header_size;

    if (hdr.naxis >= 1 && hdr.naxis1 <= 0) return -1;
    if (hdr.naxis >= 2 && hdr.naxis2 <= 0) return -1;

    int axes[3] = {1, 1, 1};
    if (hdr.naxis >= 1) axes[0] = hdr.naxis1;
    if (hdr.naxis >= 2) axes[1] = hdr.naxis2;
    if (hdr.naxis >= 3) axes[2] = std::max(hdr.naxis3, 1);

    int bytes_per_pixel = std::abs(hdr.bitpix) / 8;
    if (bytes_per_pixel == 0) bytes_per_pixel = 1;
    hdr.data_size = (size_t)axes[0] * axes[1] * axes[2] * bytes_per_pixel;

    return 0;
}

static void swap_bytes_16(void *ptr) {
    uint8_t *p = (uint8_t *)ptr;
    uint8_t tmp = p[0]; p[0] = p[1]; p[1] = tmp;
}

static void swap_bytes_32(void *ptr) {
    uint8_t *p = (uint8_t *)ptr;
    uint8_t tmp;
    tmp = p[0]; p[0] = p[3]; p[3] = tmp;
    tmp = p[1]; p[1] = p[2]; p[2] = tmp;
}

static void swap_bytes_64(void *ptr) {
    uint8_t *p = (uint8_t *)ptr;
    uint8_t tmp;
    tmp = p[0]; p[0] = p[7]; p[7] = tmp;
    tmp = p[1]; p[1] = p[6]; p[6] = tmp;
    tmp = p[2]; p[2] = p[5]; p[5] = tmp;
    tmp = p[3]; p[3] = p[4]; p[4] = tmp;
}

static int needs_swap() {
    union { uint32_t i; uint8_t b[4]; } u;
    u.i = 1;
    return u.b[0] == 1;
}

static float *convert_to_float32(const uint8_t *raw, size_t n_pixels, int bitpix) {
    float *out = (float *)malloc(n_pixels * sizeof(float));
    if (!out) return nullptr;

    int do_swap = needs_swap();

    switch (bitpix) {
    case 8:
        for (size_t i = 0; i < n_pixels; i++)
            out[i] = (float)raw[i];
        break;

    case 16: {
        const int16_t *src = (const int16_t *)raw;
        for (size_t i = 0; i < n_pixels; i++) {
            int16_t v = src[i];
            if (do_swap) swap_bytes_16(&v);
            out[i] = (float)v;
        }
        break;
    }

    case 32: {
        const int32_t *src = (const int32_t *)raw;
        for (size_t i = 0; i < n_pixels; i++) {
            int32_t v = src[i];
            if (do_swap) swap_bytes_32(&v);
            out[i] = (float)v;
        }
        break;
    }

    case -32: {
        const float *src = (const float *)raw;
        for (size_t i = 0; i < n_pixels; i++) {
            float v = src[i];
            if (do_swap) swap_bytes_32(&v);
            out[i] = v;
        }
        break;
    }

    case -64: {
        const double *src = (const double *)raw;
        for (size_t i = 0; i < n_pixels; i++) {
            double v = src[i];
            if (do_swap) swap_bytes_64(&v);
            out[i] = (float)v;
        }
        break;
    }

    case 20: {
        const uint16_t *src = (const uint16_t *)raw;
        for (size_t i = 0; i < n_pixels; i++) {
            uint16_t v = src[i];
            if (do_swap) swap_bytes_16(&v);
            out[i] = (float)v;
        }
        break;
    }

    default:
        aio_log(AIO_LOG_ERROR, "FITS", "Unsupported BITPIX: %d", bitpix);
        free(out);
        return nullptr;
    }

    return out;
}

// ============================================================================
// convert_to_float64 - 将原始像素转换为 double 数组 (FP64 模式)
// 与 convert_to_float32 对应, 但输出 double, 不损失精度:
//   - BITPIX=-64 (double): 直接拷贝, 零精度损失
//   - BITPIX=-32 (float) : float -> double 提升, 不损失精度
//   - 整数类型           : 整数 -> double, 不损失精度 (在 double 表示范围内)
// 返回 malloc 分配的 double 数组 (调用方负责 free), 失败返回 nullptr
// ============================================================================
static double *convert_to_float64(const uint8_t *raw, size_t n_pixels, int bitpix) {
    double *out = (double *)malloc(n_pixels * sizeof(double));
    if (!out) return nullptr;

    int do_swap = needs_swap();

    switch (bitpix) {
    case 8:
        for (size_t i = 0; i < n_pixels; i++)
            out[i] = (double)raw[i];
        break;

    case 16: {
        const int16_t *src = (const int16_t *)raw;
        for (size_t i = 0; i < n_pixels; i++) {
            int16_t v = src[i];
            if (do_swap) swap_bytes_16(&v);
            out[i] = (double)v;
        }
        break;
    }

    case 32: {
        const int32_t *src = (const int32_t *)raw;
        for (size_t i = 0; i < n_pixels; i++) {
            int32_t v = src[i];
            if (do_swap) swap_bytes_32(&v);
            out[i] = (double)v;
        }
        break;
    }

    case -32: {
        const float *src = (const float *)raw;
        for (size_t i = 0; i < n_pixels; i++) {
            float v = src[i];
            if (do_swap) swap_bytes_32(&v);
            out[i] = (double)v;
        }
        break;
    }

    case -64: {
        const double *src = (const double *)raw;
        for (size_t i = 0; i < n_pixels; i++) {
            double v = src[i];
            if (do_swap) swap_bytes_64(&v);
            out[i] = v;  // double 直接拷贝, 零精度损失
        }
        break;
    }

    case 20: {
        const uint16_t *src = (const uint16_t *)raw;
        for (size_t i = 0; i < n_pixels; i++) {
            uint16_t v = src[i];
            if (do_swap) swap_bytes_16(&v);
            out[i] = (double)v;
        }
        break;
    }

    default:
        aio_log(AIO_LOG_ERROR, "FITS", "Unsupported BITPIX: %d", bitpix);
        free(out);
        return nullptr;
    }

    return out;
}

static void build_metadata(const FITSHeader &hdr, AIOImageMetadata &meta) {
    auto find_kw = [&](const char *name) -> const char* {
        for (auto &kw : hdr.keywords) {
            if (strcmp(kw.name, name) == 0) return kw.value;
        }
        return nullptr;
    };

    auto kw_float = [&](const char *name, double def = 0.0) -> double {
        const char *v = find_kw(name);
        if (!v || v[0] == '\0') return def;
        try { return std::stod(v); } catch (...) { return def; }
    };

    meta.geometry.width = hdr.naxis1;
    meta.geometry.height = hdr.naxis2;
    meta.geometry.channels = std::max(hdr.naxis3, 1);

    meta.options.bits_per_sample = std::abs(hdr.bitpix);
    meta.options.float_sample = (hdr.bitpix < 0) ? 1 : 0;

    AIOWCSKeywords &wcs = meta.wcs;
    memset(&wcs, 0, sizeof(wcs));
    wcs.crpix1 = kw_float("CRPIX1");
    wcs.crpix2 = kw_float("CRPIX2");
    wcs.crval1 = kw_float("CRVAL1");
    wcs.crval2 = kw_float("CRVAL2");
    const char *ct1 = find_kw("CTYPE1");
    const char *ct2 = find_kw("CTYPE2");
    if (ct1) aio_safe_copy(wcs.ctype1, AIO_CTYPE_MAX, ct1);
    if (ct2) aio_safe_copy(wcs.ctype2, AIO_CTYPE_MAX, ct2);

    wcs.cd1_1 = kw_float("CD1_1", kw_float("CD001001"));
    wcs.cd1_2 = kw_float("CD1_2", kw_float("CD001002"));
    wcs.cd2_1 = kw_float("CD2_1", kw_float("CD002001"));
    wcs.cd2_2 = kw_float("CD2_2", kw_float("CD002002"));

    const char *cdelt1_s = find_kw("CDELT1");
    const char *cdelt2_s = find_kw("CDELT2");
    wcs.has_cdelt1 = cdelt1_s ? 1 : 0;
    wcs.has_cdelt2 = cdelt2_s ? 1 : 0;
    if (cdelt1_s) wcs.cdelt1 = kw_float("CDELT1");
    if (cdelt2_s) wcs.cdelt2 = kw_float("CDELT2");

    const char *radesys = find_kw("RADESYS");
    if (radesys) aio_safe_copy(wcs.radesys, AIO_RADESYS_MAX, radesys);
    else aio_safe_copy(wcs.radesys, AIO_RADESYS_MAX, "ICRS");

    const char *equinox_s = find_kw("EQUINOX");
    wcs.has_equinox = equinox_s ? 1 : 0;
    if (equinox_s) wcs.equinox = kw_float("EQUINOX", 2000.0);
    else wcs.equinox = 2000.0;

    const char *lonpole_s = find_kw("LONPOLE");
    const char *latpole_s = find_kw("LATPOLE");
    wcs.has_lonpole = lonpole_s ? 1 : 0;
    wcs.has_latpole = latpole_s ? 1 : 0;
    if (lonpole_s) wcs.lonpole = kw_float("LONPOLE");
    if (latpole_s) wcs.latpole = kw_float("LATPOLE");

    wcs.has_wcs = (wcs.ctype1[0] != '\0' && wcs.ctype2[0] != '\0' &&
                   (std::abs(wcs.cd1_1) > 1e-15 || std::abs(wcs.cd1_2) > 1e-15 ||
                    std::abs(wcs.cd2_1) > 1e-15 || std::abs(wcs.cd2_2) > 1e-15)) ? 1 : 0;

    AIOObservationMetadata &obs = meta.observation;
    memset(&obs, 0, sizeof(obs));
    const char *date_obs = find_kw("DATE-OBS");
    if (date_obs) aio_safe_copy(obs.date_obs, AIO_DATE_MAX, date_obs);
    const char *date_end = find_kw("DATE-END");
    if (date_end) aio_safe_copy(obs.date_end, AIO_DATE_MAX, date_end);
    const char *jd_s = find_kw("JD");
    obs.has_jd_obs = jd_s ? 1 : 0;
    if (jd_s) obs.jd_obs = kw_float("JD");
    const char *longobs = find_kw("LONG-OBS");
    obs.has_longobs = longobs ? 1 : 0;
    if (longobs) obs.longobs = kw_float("LONG-OBS");
    const char *latobs = find_kw("LAT-OBS");
    obs.has_latobs = latobs ? 1 : 0;
    if (latobs) obs.latobs = kw_float("LAT-OBS");
    const char *altobs = find_kw("ALT-OBS");
    obs.has_altobs = altobs ? 1 : 0;
    if (altobs) obs.altobs = kw_float("ALT-OBS");
    const char *observat = find_kw("OBSERVAT");
    if (observat) aio_safe_copy(obs.observat, AIO_OBSERVAT_MAX, observat);
    const char *focallen = find_kw("FOCALLEN");
    obs.has_focallen = focallen ? 1 : 0;
    if (focallen) obs.focallen = kw_float("FOCALLEN");
    const char *xpixsz = find_kw("XPIXSZ");
    obs.has_xpixsz = xpixsz ? 1 : 0;
    if (xpixsz) obs.xpixsz = kw_float("XPIXSZ");
    const char *aperture = find_kw("APERTURE");
    obs.has_aperture = aperture ? 1 : 0;
    if (aperture) obs.aperture = kw_float("APERTURE");
    const char *focal_ratio = find_kw("FOCAL_RATIO");
    obs.has_focal_ratio = focal_ratio ? 1 : 0;
    if (focal_ratio) obs.focal_ratio = kw_float("FOCAL_RATIO");
    const char *object_name = find_kw("OBJECT");
    if (object_name) aio_safe_copy(obs.object_name, AIO_OBJECT_MAX, object_name);

    AIOCalibrationMetadata &cal = meta.calibration;
    memset(&cal, 0, sizeof(cal));
    cal.exptime = kw_float("EXPTIME");
    const char *filter = find_kw("FILTER");
    if (filter) aio_safe_copy(cal.filter_name, AIO_FILTER_MAX, filter);
    else aio_safe_copy(cal.filter_name, AIO_FILTER_MAX, "Unknown");
    cal.gain = kw_float("GAIN", 1.0);
    const char *ccd_temp = find_kw("CCD-TEMP");
    if (!ccd_temp) ccd_temp = find_kw("TEMP");
    cal.has_ccd_temp = ccd_temp ? 1 : 0;
    if (ccd_temp) cal.ccd_temp = std::stod(ccd_temp);
    const char *imagetyp = find_kw("IMAGETYP");
    if (imagetyp) aio_safe_copy(cal.frame_type, AIO_FRAME_TYPE_MAX, imagetyp);
    const char *bunit = find_kw("BUNIT");
    if (bunit) aio_safe_copy(cal.bunit, AIO_BUNIT_MAX, bunit);
    else aio_safe_copy(cal.bunit, AIO_BUNIT_MAX, "ADU");
}

// ============================================================================
// fpack (.fits.fz) 支持: CFITSIO 透明解压读取
// CFITSIO 4.6.4 已静态编入 AIO DLL (Phase1 Final Closure V3), 这里把主 FITS
// 读取路径接到 CFITSIO 的 .fz 自动检测/解压 (RICE_1/RICE_ONE/GZIP/HCOMPRESS/
// PLIO)。普通未压缩 FITS 仍走原有手写解析路径, 零回归风险。
// ============================================================================
static bool fits_is_fpack_compressed(const char *path) {
    const char *ext = strrchr(path, '.');
    if (ext) {
        std::string e = ext;
        for (auto &ch : e) ch = (char)std::tolower((unsigned char)ch);
        if (e == ".fz") return true;
    }
    // 内容级检测: 主头之后第二 HDU 为 COMPRESSED_IMAGE (ZIMAGE)
    FILE *fp = aio_fopen_utf8(path, "rb");
    if (!fp) return false;
    char buf[FITS_BLOCK_SIZE * 2];
    size_t n = std::fread(buf, 1, sizeof(buf), fp);
    std::fclose(fp);
    if (n < FITS_BLOCK_SIZE * 2) return false;
    std::string second(buf + FITS_BLOCK_SIZE, FITS_BLOCK_SIZE);
    return second.find("ZIMAGE") != std::string::npos ||
           second.find("COMPRESSED_IMAGE") != std::string::npos;
}

static int fits_read_file_cfitsio(const char *path, AIOImageData *out, bool header_only) {
    fitsfile *fptr = nullptr;
    int status = 0;
    if (fits_open_file(&fptr, path, READONLY, &status)) {
        aio_log(AIO_LOG_ERROR, "FITS", "fits_open_file failed (%s) status=%d", path, status);
        return -1;
    }

    // fpack 压缩图像: 主头 NAXIS=0, 实际图像在第一个扩展 (COMPRESSED_IMAGE)。
    // 必须移动到该 HDU 才触发 CFITSIO 解压 (ZIMAGE -> compressimg -> imcomp_get_compressed_image_par)。
    int hdutype = 0;
    if (fits_movabs_hdu(fptr, 2, &hdutype, &status)) {
        aio_log(AIO_LOG_ERROR, "FITS", "fits_movabs_hdu(2) failed (%s) status=%d", path, status);
        fits_close_file(fptr, &status);
        return -1;
    }

    int bitpix = 0, naxis = 0;
    long naxes[3] = {0, 0, 0};
    if (fits_get_img_param(fptr, 3, &bitpix, &naxis, naxes, &status)) {
        aio_log(AIO_LOG_ERROR, "FITS", "fits_get_img_param failed (%s) status=%d", path, status);
        fits_close_file(fptr, &status);
        return -1;
    }

    int w = (naxis >= 1) ? (int)naxes[0] : 1;
    int h = (naxis >= 2) ? (int)naxes[1] : 1;
    int c = (naxis >= 3 && naxes[2] > 1) ? (int)naxes[2] : 1;
    size_t n_pixels = (size_t)w * h * c;
    bool is_fp64 = (aio_internal_is_fp64() != 0);

    if (!header_only) {
        if (is_fp64) {
            double *pixel_data_f64 = (double *)malloc(n_pixels * sizeof(double));
            if (!pixel_data_f64) {
                fits_close_file(fptr, &status);
                return -1;
            }
            long fpixel[3] = {1, 1, 1};
            if (fits_read_pix(fptr, TDOUBLE, fpixel, (LONGLONG)n_pixels, nullptr,
                              pixel_data_f64, nullptr, &status)) {
                aio_log(AIO_LOG_ERROR, "FITS", "fits_read_pix(FP64) failed (%s) status=%d",
                        path, status);
                free(pixel_data_f64);
                fits_close_file(fptr, &status);
                return -1;
            }
            if (c > 1) {
                double *gray = (double *)malloc((size_t)w * h * sizeof(double));
                if (gray) {
                    for (int y = 0; y < h; y++)
                        for (int x = 0; x < w; x++)
                            gray[y * w + x] = pixel_data_f64[y * w + x];
                    free(pixel_data_f64);
                    pixel_data_f64 = gray;
                }
                c = 1;
            }
            out->data = nullptr;
            out->data_f64 = pixel_data_f64;
            out->dtype = 1;  // FP64
        } else {
            float *pixel_data = (float *)malloc(n_pixels * sizeof(float));
            if (!pixel_data) {
                fits_close_file(fptr, &status);
                return -1;
            }
            long fpixel[3] = {1, 1, 1};
            if (fits_read_pix(fptr, TFLOAT, fpixel, (LONGLONG)n_pixels, nullptr,
                              pixel_data, nullptr, &status)) {
                aio_log(AIO_LOG_ERROR, "FITS", "fits_read_pix(FP32) failed (%s) status=%d",
                        path, status);
                free(pixel_data);
                fits_close_file(fptr, &status);
                return -1;
            }
            if (c > 1) {
                float *gray = (float *)malloc((size_t)w * h * sizeof(float));
                if (gray) {
                    for (int y = 0; y < h; y++)
                        for (int x = 0; x < w; x++)
                            gray[y * w + x] = pixel_data[y * w + x];
                    free(pixel_data);
                    pixel_data = gray;
                }
                c = 1;
            }
            out->data = pixel_data;
            out->data_f64 = nullptr;
            out->dtype = 0;  // FP32
        }
    } else {
        // 与普通 FITS header-only 行为一致: 分配零缓冲
        out->data = (float *)calloc((size_t)w * h, sizeof(float));
        out->data_f64 = nullptr;
        out->dtype = is_fp64 ? 1 : 0;
    }

    // 关键字: 压缩扩展头含原始科学关键字, 重建为与 funpack 后等效的原始图像头
    FITSHeader hdr;
    hdr.keywords.clear();
    char *hdr_str = nullptr;
    int nkeys = 0;
    status = 0;
    if (fits_hdr2str(fptr, 0, nullptr, 0, &hdr_str, &nkeys, &status)) {
        aio_log(AIO_LOG_WARN, "FITS", "fits_hdr2str failed (%s) status=%d", path, status);
        status = 0;
    } else if (hdr_str && nkeys > 0) {
        for (int i = 0; i < nkeys; i++) {
            char card[81];
            std::memcpy(card, hdr_str + i * 80, 80);
            card[80] = '\0';
            // 与手写解析路径一致: 跳过 COMMENT/HISTORY/空白卡
            std::string card_key = trim_str(std::string(card, 8));
            if (card_key == "COMMENT" || card_key == "HISTORY" || card_key.empty())
                continue;
            AIOFITSKeyword kw;
            memset(&kw, 0, sizeof(kw));
            parse_card(card, kw);
            if (kw.name[0] == '\0' || strcmp(kw.name, "END") == 0) continue;
            // 剥掉 BINTABLE/压缩专用关键字 (原始图像头不含这些)
            const char *kn = kw.name;
            if (strcmp(kn, "XTENSION") == 0 || strcmp(kn, "BITPIX") == 0 ||
                strcmp(kn, "NAXIS") == 0 || strcmp(kn, "NAXIS1") == 0 ||
                strcmp(kn, "NAXIS2") == 0 || strcmp(kn, "NAXIS3") == 0 ||
                strcmp(kn, "PCOUNT") == 0 || strcmp(kn, "GCOUNT") == 0 ||
                strcmp(kn, "TFIELDS") == 0 || strcmp(kn, "EXTNAME") == 0 ||
                strcmp(kn, "ZSCALE") == 0 || strcmp(kn, "ZZERO") == 0 ||
                kn[0] == 'Z' || strncmp(kn, "TTYPE", 5) == 0 ||
                strncmp(kn, "TFORM", 5) == 0 || strncmp(kn, "TDIM", 4) == 0)
                continue;
            hdr.keywords.push_back(kw);
        }
        status = 0;
        fits_free_memory(hdr_str, &status);
        status = 0;
    }

    // 前置标准图像头卡 (与 funpack 后的原始主头等效)
    std::vector<AIOFITSKeyword> std_cards;
    auto add_kw = [&](const char *name, const char *value, const char *comment) {
        AIOFITSKeyword k;
        memset(&k, 0, sizeof(k));
        aio_safe_copy(k.name, AIO_KEYWORD_NAME_MAX, name);
        aio_safe_copy(k.value, AIO_KEYWORD_VALUE_MAX, value);
        aio_safe_copy(k.comment, AIO_KEYWORD_COMMENT_MAX, comment);
        std_cards.push_back(k);
    };
    char tmp[32];
    add_kw("SIMPLE", "T", "Conforms to FITS standard");
    snprintf(tmp, sizeof(tmp), "%d", bitpix);
    add_kw("BITPIX", tmp, "Array data type");
    snprintf(tmp, sizeof(tmp), "%d", naxis);
    add_kw("NAXIS", tmp, "Number of dimensions");
    snprintf(tmp, sizeof(tmp), "%d", w);
    add_kw("NAXIS1", tmp, "Axis 1 size");
    snprintf(tmp, sizeof(tmp), "%d", h);
    add_kw("NAXIS2", tmp, "Axis 2 size");
    if (naxis >= 3) {
        snprintf(tmp, sizeof(tmp), "%d", c);
        add_kw("NAXIS3", tmp, "Axis 3 size");
    }
    hdr.keywords.insert(hdr.keywords.begin(), std_cards.begin(), std_cards.end());

    hdr.bitpix = bitpix;
    hdr.naxis = naxis;
    hdr.naxis1 = w;
    hdr.naxis2 = (naxis >= 2) ? h : 1;
    hdr.naxis3 = (naxis >= 3) ? c : 1;
    hdr.bscale = 1.0;
    hdr.bzero = 0.0;
    double dval = 0.0;
    status = 0;
    if (!fits_read_key(fptr, TDOUBLE, "BSCALE", &dval, nullptr, &status)) hdr.bscale = dval;
    status = 0;
    if (!fits_read_key(fptr, TDOUBLE, "BZERO", &dval, nullptr, &status)) hdr.bzero = dval;
    hdr.data_offset = 0;
    hdr.data_size = 0;

    status = 0;
    if (fits_close_file(fptr, &status)) {
        aio_log(AIO_LOG_WARN, "FITS", "fits_close_file status=%d", status);
    }

    out->width = w;
    out->height = h;
    out->channels = c;
    out->bits_per_sample = std::abs(bitpix);
    out->float_sample = (bitpix < 0) ? 1 : 0;
    strncpy(out->source_format, "fits", sizeof(out->source_format) - 1);
    aio_safe_copy(out->source_path, AIO_PATH_MAX, path);

    out->keyword_count = (int)hdr.keywords.size();
    if (out->keyword_count > 0) {
        out->keywords = (AIOFITSKeyword *)malloc(out->keyword_count * sizeof(AIOFITSKeyword));
        std::memcpy(out->keywords, hdr.keywords.data(),
                    out->keyword_count * sizeof(AIOFITSKeyword));
    } else {
        out->keywords = nullptr;
    }

    build_metadata(hdr, out->metadata);

    aio_log(AIO_LOG_INFO, "FITS", "Read OK (fpack .fz): %dx%d %dch BITPIX=%d", w, h, c, bitpix);
    return 0;
}

int fits_read_file(const char *path, AIOImageData *out) {
    aio_log(AIO_LOG_INFO, "FITS", "Reading: %s", path);

    // fpack (.fits.fz) 支持: 交 CFITSIO 透明解压
    if (fits_is_fpack_compressed(path)) {
        return fits_read_file_cfitsio(path, out, /*header_only=*/false);
    }

    FILE *fp = aio_fopen_utf8(path, "rb");
    if (!fp) {
        aio_log(AIO_LOG_ERROR, "FITS", "Cannot open: %s", path);
        return -1;
    }

    char magic[6];
    if (std::fread(magic, 1, 6, fp) != 6 || memcmp(magic, "SIMPLE", 6) != 0) {
        aio_log(AIO_LOG_ERROR, "FITS", "Not a FITS file: %s", path);
        std::fclose(fp);
        return -1;
    }
    std::rewind(fp);

    FITSHeader hdr;
    if (parse_fits_header(fp, hdr) != 0) {
        aio_log(AIO_LOG_ERROR, "FITS", "Header parse failed: %s", path);
        std::fclose(fp);
        return -1;
    }

    aio_log(AIO_LOG_INFO, "FITS", "Header: BITPIX=%d NAXIS=%d NAXIS1=%d NAXIS2=%d NAXIS3=%d BSCALE=%.1f BZERO=%.1f keywords=%d",
            hdr.bitpix, hdr.naxis, hdr.naxis1, hdr.naxis2, hdr.naxis3,
            hdr.bscale, hdr.bzero, (int)hdr.keywords.size());

    if (std::fseek(fp, (long)hdr.data_offset, SEEK_SET) != 0) {
        aio_log(AIO_LOG_ERROR, "FITS", "Seek to data failed: %s", path);
        std::fclose(fp);
        return -1;
    }

    int w = hdr.naxis1;
    int h = (hdr.naxis >= 2) ? hdr.naxis2 : 1;
    int c = (hdr.naxis >= 3 && hdr.naxis3 > 1) ? hdr.naxis3 : 1;
    size_t n_pixels = (size_t)w * h * c;

    std::vector<uint8_t> raw(hdr.data_size);
    size_t nread = std::fread(raw.data(), 1, hdr.data_size, fp);
    std::fclose(fp);

    if (nread < hdr.data_size) {
        aio_log(AIO_LOG_WARN, "FITS", "Data read incomplete: %zu/%zu bytes", nread, hdr.data_size);
    }

    // 双精度 ABI: 根据 AIO 模块精度模式决定读取到 data (FP32) 还是 data_f64 (FP64)
    // R10 修复: 使用 aio_internal_is_fp64() 替代 PrecisionContext (DLL 边界不共享)
    bool is_fp64 = (aio_internal_is_fp64() != 0);

    if (is_fp64) {
        double *pixel_data_f64 = convert_to_float64(raw.data(), n_pixels, hdr.bitpix);
        if (!pixel_data_f64) {
            aio_log(AIO_LOG_ERROR, "FITS", "Pixel conversion (FP64) failed");
            return -1;
        }

        if (hdr.bscale != 1.0 || hdr.bzero != 0.0) {
            aio_log(AIO_LOG_INFO, "FITS", "Applying BSCALE=%.6f BZERO=%.6f (FP64)", hdr.bscale, hdr.bzero);
            for (size_t i = 0; i < n_pixels; i++) {
                pixel_data_f64[i] = hdr.bscale * pixel_data_f64[i] + hdr.bzero;
            }
        }

        if (c > 1) {
            double *gray = (double *)malloc((size_t)w * h * sizeof(double));
            if (gray) {
                for (int y = 0; y < h; y++)
                    for (int x = 0; x < w; x++)
                        gray[y * w + x] = pixel_data_f64[0 * w * h + y * w + x];
                free(pixel_data_f64);
                pixel_data_f64 = gray;
                c = 1;
            }
        }

        out->data = nullptr;
        out->data_f64 = pixel_data_f64;
        out->dtype = 1;  // FP64 (与 AstroScalarType::FP64 一致)
        aio_log(AIO_LOG_INFO, "FITS", "FP64 mode: pixels stored in data_f64 (no float32 downgrade)");
    } else {
        float *pixel_data = convert_to_float32(raw.data(), n_pixels, hdr.bitpix);
        if (!pixel_data) {
            aio_log(AIO_LOG_ERROR, "FITS", "Pixel conversion failed");
            return -1;
        }

        if (hdr.bscale != 1.0 || hdr.bzero != 0.0) {
            aio_log(AIO_LOG_INFO, "FITS", "Applying BSCALE=%.6f BZERO=%.6f", hdr.bscale, hdr.bzero);
            for (size_t i = 0; i < n_pixels; i++) {
                pixel_data[i] = (float)(hdr.bscale * (double)pixel_data[i] + hdr.bzero);
            }
        }

        if (c > 1) {
            float *gray = (float *)malloc((size_t)w * h * sizeof(float));
            if (gray) {
                for (int y = 0; y < h; y++)
                    for (int x = 0; x < w; x++)
                        gray[y * w + x] = pixel_data[0 * w * h + y * w + x];
                free(pixel_data);
                pixel_data = gray;
                c = 1;
            }
        }

        out->data = pixel_data;
        out->data_f64 = nullptr;
        out->dtype = 0;  // FP32 (与 AstroScalarType::FP32 一致)
    }
    out->width = w;
    out->height = h;
    out->channels = c;
    out->bits_per_sample = std::abs(hdr.bitpix);
    out->float_sample = (hdr.bitpix < 0) ? 1 : 0;
    strncpy(out->source_format, "fits", sizeof(out->source_format) - 1);
    aio_safe_copy(out->source_path, AIO_PATH_MAX, path);

    out->keyword_count = (int)hdr.keywords.size();
    if (out->keyword_count > 0) {
        out->keywords = (AIOFITSKeyword *)malloc(out->keyword_count * sizeof(AIOFITSKeyword));
        std::memcpy(out->keywords, hdr.keywords.data(), out->keyword_count * sizeof(AIOFITSKeyword));
    } else {
        out->keywords = nullptr;
    }

    build_metadata(hdr, out->metadata);

    aio_log(AIO_LOG_INFO, "FITS", "Read OK: %dx%d %dch BITPIX=%d", w, h, c, hdr.bitpix);
    return 0;
}

int fits_read_header_only(const char *path, AIOImageData *out) {
    aio_log(AIO_LOG_INFO, "FITS", "Reading header only: %s", path);

    // fpack (.fits.fz) 支持: 交 CFITSIO 透明解压
    if (fits_is_fpack_compressed(path)) {
        return fits_read_file_cfitsio(path, out, /*header_only=*/true);
    }

    FILE *fp = aio_fopen_utf8(path, "rb");
    if (!fp) {
        aio_log(AIO_LOG_ERROR, "FITS", "Cannot open: %s", path);
        return -1;
    }

    char magic[6];
    if (std::fread(magic, 1, 6, fp) != 6 || memcmp(magic, "SIMPLE", 6) != 0) {
        std::fclose(fp);
        return -1;
    }
    std::rewind(fp);

    FITSHeader hdr;
    if (parse_fits_header(fp, hdr) != 0) {
        std::fclose(fp);
        return -1;
    }
    std::fclose(fp);

    int w = hdr.naxis1;
    int h = (hdr.naxis >= 2) ? hdr.naxis2 : 1;

    out->data = (float *)calloc((size_t)w * h, sizeof(float));
    out->width = w;
    out->height = h;
    out->channels = (hdr.naxis >= 3 && hdr.naxis3 > 1) ? hdr.naxis3 : 1;
    out->bits_per_sample = std::abs(hdr.bitpix);
    out->float_sample = (hdr.bitpix < 0) ? 1 : 0;
    strncpy(out->source_format, "fits", sizeof(out->source_format) - 1);
    aio_safe_copy(out->source_path, AIO_PATH_MAX, path);

    out->keyword_count = (int)hdr.keywords.size();
    if (out->keyword_count > 0) {
        out->keywords = (AIOFITSKeyword *)malloc(out->keyword_count * sizeof(AIOFITSKeyword));
        std::memcpy(out->keywords, hdr.keywords.data(), out->keyword_count * sizeof(AIOFITSKeyword));
    } else {
        out->keywords = nullptr;
    }

    build_metadata(hdr, out->metadata);
    return 0;
}

static void write_card(char card[80], const char *key, const char *value, const char *comment) {
    memset(card, ' ', 80);
    size_t klen = std::min(strlen(key), (size_t)8);
    memcpy(card, key, klen);

    if (value[0] != '\0') {
        card[8] = '=';
        card[9] = ' ';

        // 判断值是否需要加引号（字符串类型需要，布尔/数值不需要）
        // FITS标准：布尔=T/F，整数/浮点=数字，字符串=单引号包围
        bool needs_quotes = true;

        // 已有引号 → 不再加
        if (value[0] == '\'') {
            needs_quotes = false;
        }
        // 布尔值 T/F → 不加引号
        else if (strcmp(value, "T") == 0 || strcmp(value, "F") == 0) {
            needs_quotes = false;
        }
        // 数值（整数或浮点，含科学计数法）→ 不加引号
        else {
            char *end;
            strtod(value, &end);
            if (*end == '\0' && end != value) {
                needs_quotes = false;
            }
        }

        char val_str[72];
        if (needs_quotes) {
            snprintf(val_str, sizeof(val_str), "'%-8s'", value);
        } else {
            strncpy(val_str, value, sizeof(val_str) - 1);
            val_str[sizeof(val_str) - 1] = '\0';
        }

        size_t vlen = strlen(val_str);
        size_t vstart = 10;
        if (vlen <= 20) vstart = 30 - vlen / 2;
        if (vstart + vlen > 80) vstart = 10;
        size_t copy_len = std::min(vlen, 80 - vstart);
        memcpy(card + vstart, val_str, copy_len);

        if (comment && comment[0] != '\0') {
            size_t cstart = vstart + vlen + 2;
            if (cstart < 79) {
                card[cstart - 1] = '/';
                size_t clen = std::min(strlen(comment), 80 - cstart - 1);
                memcpy(card + cstart + 1, comment, clen);
            }
        }
    }
}

int fits_write_file(const AIOImageData *image, const char *path) {
    aio_log(AIO_LOG_INFO, "FITS", "Writing: %s (%dx%d)", path, image->width, image->height);

    FILE *fp = aio_fopen_utf8(path, "wb");
    if (!fp) {
        aio_log(AIO_LOG_ERROR, "FITS", "Cannot open for write: %s", path);
        return -1;
    }

    char iobuf[1 << 20];
    std::setvbuf(fp, iobuf, _IOFBF, sizeof(iobuf));

    std::vector<char> header_buf;
    auto add_card = [&](const char *key, const char *value, const char *comment = "") {
        char card[80];
        write_card(card, key, value, comment);
        header_buf.insert(header_buf.end(), card, card + 80);
    };

    char tmp[32];
    add_card("SIMPLE", "T", "Conforms to FITS standard");

    snprintf(tmp, sizeof(tmp), "%d", image->bits_per_sample > 0 && !image->float_sample ? 16 : -32);
    add_card("BITPIX", tmp, "Array data type");

    add_card("NAXIS", "2", "Number of dimensions");
    snprintf(tmp, sizeof(tmp), "%d", image->width);
    add_card("NAXIS1", tmp, "Axis 1 size");
    snprintf(tmp, sizeof(tmp), "%d", image->height);
    add_card("NAXIS2", tmp, "Axis 2 size");

    if (image->keywords && image->keyword_count > 0) {
        for (int i = 0; i < image->keyword_count; i++) {
            const AIOFITSKeyword &kw = image->keywords[i];
            if (strcmp(kw.name, "SIMPLE") == 0 || strcmp(kw.name, "BITPIX") == 0 ||
                strcmp(kw.name, "NAXIS") == 0 || strcmp(kw.name, "NAXIS1") == 0 ||
                strcmp(kw.name, "NAXIS2") == 0 || strcmp(kw.name, "NAXIS3") == 0 ||
                strcmp(kw.name, "EXTEND") == 0 || strcmp(kw.name, "END") == 0 ||
                strcmp(kw.name, "BZERO") == 0 || strcmp(kw.name, "BSCALE") == 0) {
                continue;
            }
            add_card(kw.name, kw.value, kw.comment);
        }
    }

    add_card("END", "", "");

    size_t header_size = header_buf.size();
    size_t padded = ((header_size + FITS_BLOCK_SIZE - 1) / FITS_BLOCK_SIZE) * FITS_BLOCK_SIZE;
    header_buf.resize(padded, ' ');

    std::fwrite(header_buf.data(), 1, padded, fp);

    size_t n_pixels = (size_t)image->width * image->height;
    int do_swap = needs_swap();

    if (image->float_sample || image->bits_per_sample == 32) {
        if (do_swap) {
            std::vector<float> buf(n_pixels);
            std::memcpy(buf.data(), image->data, n_pixels * sizeof(float));
            for (size_t i = 0; i < n_pixels; i++) swap_bytes_32(&buf[i]);
            std::fwrite(buf.data(), sizeof(float), n_pixels, fp);
        } else {
            std::fwrite(image->data, sizeof(float), n_pixels, fp);
        }
    } else {
        std::vector<int16_t> buf(n_pixels);
        #pragma omp parallel for schedule(static)
        for (long long i = 0; i < (long long)n_pixels; i++) {
            buf[i] = (int16_t)std::round(image->data[i]);
            if (do_swap) swap_bytes_16(&buf[i]);
        }
        std::fwrite(buf.data(), sizeof(int16_t), n_pixels, fp);
    }

    size_t data_written = n_pixels * (image->float_sample ? 4 : 2);
    size_t data_padded = ((data_written + FITS_BLOCK_SIZE - 1) / FITS_BLOCK_SIZE) * FITS_BLOCK_SIZE;
    if (data_padded > data_written) {
        std::vector<char> zero_pad(data_padded - data_written, 0);
        std::fwrite(zero_pad.data(), 1, zero_pad.size(), fp);
    }

    std::fclose(fp);
    aio_log(AIO_LOG_INFO, "FITS", "Write OK: %s", path);
    return 0;
}

int fits_detect(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return 0;
    std::string e = ext;
    for (auto &c : e) c = tolower(c);
    // .fz = fpack 压缩 FITS (CFITSIO 透明解压)
    return (e == ".fits" || e == ".fit" || e == ".fts" || e == ".fz") ? 1 : 0;
}
