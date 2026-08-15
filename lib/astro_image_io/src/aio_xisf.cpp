#include "aio_xisf.h"
#include "aio_log.h"
#include "aio_util.h"
#include <cstdio>

// 修复: AIO 模块内部精度模式查询 (替代跨 DLL 的 PrecisionContext)
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

static const uint8_t XISF_MAGIC[8] = {'X', 'I', 'S', 'F', '0', '1', '0', '0'};

struct XISFSampleFormat {
    int dtype_size;
    int is_float;
    int bits_per_sample;
};

static XISFSampleFormat parse_sample_format(const std::string &fmt) {
    XISFSampleFormat sf = {4, 1, 32};
    if (fmt == "Float32")  { sf = {4, 1, 32}; }
    else if (fmt == "Float64")  { sf = {8, 1, 64}; }
    else if (fmt == "UInt8")    { sf = {1, 0, 8};  }
    else if (fmt == "UInt16")   { sf = {2, 0, 16}; }
    else if (fmt == "UInt32")   { sf = {4, 0, 32}; }
    else if (fmt == "UInt64")   { sf = {8, 0, 64}; }
    else if (fmt == "Int8")     { sf = {1, 0, 8};  }
    else if (fmt == "Int16")    { sf = {2, 0, 16}; }
    else if (fmt == "Int32")    { sf = {4, 0, 32}; }
    else if (fmt == "Int64")    { sf = {8, 0, 64}; }
    else {
        aio_log(AIO_LOG_WARN, "XISF", "Unknown sampleFormat '%s', fallback Float32", fmt.c_str());
    }
    return sf;
}

static std::string get_attr(const std::string &tag, const std::string &attr_name) {
    std::string search = attr_name + "=\"";
    size_t pos = tag.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    size_t end = tag.find('"', pos);
    if (end == std::string::npos) return "";
    return tag.substr(pos, end - pos);
}

static void parse_geometry(const std::string &geo, int &w, int &h, int &c) {
    w = 0; h = 0; c = 1;
    size_t p1 = geo.find(':');
    if (p1 == std::string::npos) { w = std::atoi(geo.c_str()); return; }
    w = std::atoi(geo.substr(0, p1).c_str());
    size_t p2 = geo.find(':', p1 + 1);
    if (p2 == std::string::npos) { h = std::atoi(geo.substr(p1 + 1).c_str()); return; }
    h = std::atoi(geo.substr(p1 + 1, p2 - p1 - 1).c_str());
    c = std::atoi(geo.substr(p2 + 1).c_str());
    if (c <= 0) c = 1;
}

static void parse_location(const std::string &loc, int64_t &offset, int64_t &size_bytes) {
    offset = 0; size_bytes = 0;
    if (loc.find("attachment:") != 0) return;
    size_t p1 = loc.find(':', 11);
    if (p1 == std::string::npos) return;
    offset = std::atoll(loc.substr(11, p1 - 11).c_str());
    size_bytes = std::atoll(loc.substr(p1 + 1).c_str());
}

static std::string normalize_fits_value(const std::string &value) {
    std::string v = value;
    while (v.size() >= 2 && v.front() == '\'' && v.back() == '\'') {
        std::string inner = v.substr(1, v.size() - 2);
        bool has_unescaped = false;
        for (size_t i = 0; i < inner.size(); i++) {
            if (inner[i] == '\'' && (i + 1 >= inner.size() || inner[i + 1] != '\'')) {
                has_unescaped = true;
                break;
            }
            if (inner[i] == '\'' && i + 1 < inner.size() && inner[i + 1] == '\'') i++;
        }
        if (!has_unescaped) v = inner;
        else break;
    }
    size_t pos;
    while ((pos = v.find("''")) != std::string::npos) {
        v.replace(pos, 2, "'");
    }
    size_t end = v.find_last_not_of(' ');
    if (end != std::string::npos) v = v.substr(0, end + 1);
    return v;
}

static void parse_fits_keywords(const std::string &xml, std::vector<AIOFITSKeyword> &keywords) {
    size_t pos = 0;
    const std::string tag_start = "<FITSKeyword ";
    while ((pos = xml.find(tag_start, pos)) != std::string::npos) {
        size_t tag_end = xml.find("/>", pos);
        if (tag_end == std::string::npos) break;

        std::string tag = xml.substr(pos + tag_start.size(), tag_end - pos - tag_start.size());

        AIOFITSKeyword kw;
        memset(&kw, 0, sizeof(kw));

        std::string name = get_attr(tag, "name");
        std::string value = get_attr(tag, "value");
        std::string comment = get_attr(tag, "comment");

        aio_safe_copy(kw.name, AIO_KEYWORD_NAME_MAX, name.c_str());
        std::string norm_val = normalize_fits_value(value);
        aio_safe_copy(kw.value, AIO_KEYWORD_VALUE_MAX, norm_val.c_str());
        aio_safe_copy(kw.comment, AIO_KEYWORD_COMMENT_MAX, comment.c_str());

        keywords.push_back(kw);
        pos = tag_end + 2;
    }
}

struct XISFImageInfo {
    int width, height, channels;
    std::string sample_format;
    std::string byte_order;
    int64_t data_offset;
    int64_t data_size;
};

static int find_first_image(const std::string &xml, XISFImageInfo &info) {
    size_t pos = 0;
    const std::string img_tag = "<Image ";
    while ((pos = xml.find(img_tag, pos)) != std::string::npos) {
        size_t tag_end = xml.find('>', pos);
        if (tag_end == std::string::npos) break;

        std::string attrs = xml.substr(pos + img_tag.size(), tag_end - pos - img_tag.size());

        std::string geo = get_attr(attrs, "geometry");
        std::string fmt = get_attr(attrs, "sampleFormat");
        std::string loc = get_attr(attrs, "location");

        if (geo.empty()) { pos = tag_end + 1; continue; }

        parse_geometry(geo, info.width, info.height, info.channels);
        info.sample_format = fmt.empty() ? "Float32" : fmt;
        info.byte_order = get_attr(attrs, "byteOrder");
        parse_location(loc, info.data_offset, info.data_size);

        return 0;
    }
    return -1;
}

static float *convert_xisf_pixels(const uint8_t *raw, size_t n_pixels, const XISFSampleFormat &sf, int do_swap) {
    float *out = (float *)malloc(n_pixels * sizeof(float));
    if (!out) return nullptr;

    switch (sf.dtype_size) {
    case 1:
        if (sf.is_float) {
            for (size_t i = 0; i < n_pixels; i++) out[i] = 0.0f;
        } else {
            for (size_t i = 0; i < n_pixels; i++) out[i] = (float)raw[i];
        }
        break;

    case 2: {
        const uint16_t *src = (const uint16_t *)raw;
        for (size_t i = 0; i < n_pixels; i++) {
            uint16_t v = src[i];
            if (do_swap) { uint8_t *p = (uint8_t *)&v; uint8_t t = p[0]; p[0] = p[1]; p[1] = t; }
            out[i] = (float)v;
        }
        break;
    }

    case 4: {
        if (sf.is_float) {
            const float *src = (const float *)raw;
            for (size_t i = 0; i < n_pixels; i++) {
                float v = src[i];
                if (do_swap) {
                    uint8_t *p = (uint8_t *)&v;
                    uint8_t t; t = p[0]; p[0] = p[3]; p[3] = t;
                    t = p[1]; p[1] = p[2]; p[2] = t;
                }
                out[i] = v;
            }
        } else {
            const uint32_t *src = (const uint32_t *)raw;
            for (size_t i = 0; i < n_pixels; i++) {
                uint32_t v = src[i];
                if (do_swap) {
                    uint8_t *p = (uint8_t *)&v;
                    uint8_t t; t = p[0]; p[0] = p[3]; p[3] = t;
                    t = p[1]; p[1] = p[2]; p[2] = t;
                }
                out[i] = (float)(int32_t)v;
            }
        }
        break;
    }

    case 8: {
        if (sf.is_float) {
            const double *src = (const double *)raw;
            for (size_t i = 0; i < n_pixels; i++) {
                double v = src[i];
                if (do_swap) {
                    uint8_t *p = (uint8_t *)&v;
                    uint8_t t; t = p[0]; p[0] = p[7]; p[7] = t;
                    t = p[1]; p[1] = p[6]; p[6] = t;
                    t = p[2]; p[2] = p[5]; p[5] = t;
                    t = p[3]; p[3] = p[4]; p[4] = t;
                }
                out[i] = (float)v;
            }
        } else {
            const uint64_t *src = (const uint64_t *)raw;
            for (size_t i = 0; i < n_pixels; i++) {
                uint64_t v = src[i];
                if (do_swap) {
                    uint8_t *p = (uint8_t *)&v;
                    uint8_t t; t = p[0]; p[0] = p[7]; p[7] = t;
                    t = p[1]; p[1] = p[6]; p[6] = t;
                    t = p[2]; p[2] = p[5]; p[5] = t;
                    t = p[3]; p[3] = p[4]; p[4] = t;
                }
                out[i] = (float)(int64_t)v;
            }
        }
        break;
    }

    default:
        aio_log(AIO_LOG_ERROR, "XISF", "Unsupported dtype_size: %d", sf.dtype_size);
        free(out);
        return nullptr;
    }

    return out;
}

// ============================================================================
// convert_xisf_pixels_f64 - 将原始 XISF 像素转换为 double 数组 (FP64 模式)
// 与 convert_xisf_pixels 对应, 但输出 double, 不损失精度:
// - Float64: 直接拷贝, 零精度损失
// - Float32: float -> double 提升, 不损失精度
// - 整数类型: 整数 -> double, 不损失精度
// 返回 malloc 分配的 double 数组 (调用方负责 free), 失败返回 nullptr
// ============================================================================
static double *convert_xisf_pixels_f64(const uint8_t *raw, size_t n_pixels,
                                       const XISFSampleFormat &sf, int do_swap) {
    double *out = (double *)malloc(n_pixels * sizeof(double));
    if (!out) return nullptr;

    switch (sf.dtype_size) {
    case 1:
        if (sf.is_float) {
            for (size_t i = 0; i < n_pixels; i++) out[i] = 0.0;
        } else {
            for (size_t i = 0; i < n_pixels; i++) out[i] = (double)raw[i];
        }
        break;

    case 2: {
        const uint16_t *src = (const uint16_t *)raw;
        for (size_t i = 0; i < n_pixels; i++) {
            uint16_t v = src[i];
            if (do_swap) { uint8_t *p = (uint8_t *)&v; uint8_t t = p[0]; p[0] = p[1]; p[1] = t; }
            out[i] = (double)v;
        }
        break;
    }

    case 4: {
        if (sf.is_float) {
            const float *src = (const float *)raw;
            for (size_t i = 0; i < n_pixels; i++) {
                float v = src[i];
                if (do_swap) {
                    uint8_t *p = (uint8_t *)&v;
                    uint8_t t; t = p[0]; p[0] = p[3]; p[3] = t;
                    t = p[1]; p[1] = p[2]; p[2] = t;
                }
                out[i] = (double)v;
            }
        } else {
            const uint32_t *src = (const uint32_t *)raw;
            for (size_t i = 0; i < n_pixels; i++) {
                uint32_t v = src[i];
                if (do_swap) {
                    uint8_t *p = (uint8_t *)&v;
                    uint8_t t; t = p[0]; p[0] = p[3]; p[3] = t;
                    t = p[1]; p[1] = p[2]; p[2] = t;
                }
                out[i] = (double)(int32_t)v;
            }
        }
        break;
    }

    case 8: {
        if (sf.is_float) {
            const double *src = (const double *)raw;
            for (size_t i = 0; i < n_pixels; i++) {
                double v = src[i];
                if (do_swap) {
                    uint8_t *p = (uint8_t *)&v;
                    uint8_t t; t = p[0]; p[0] = p[7]; p[7] = t;
                    t = p[1]; p[1] = p[6]; p[6] = t;
                    t = p[2]; p[2] = p[5]; p[5] = t;
                    t = p[3]; p[3] = p[4]; p[4] = t;
                }
                out[i] = v;  // double 直接拷贝, 零精度损失
            }
        } else {
            const uint64_t *src = (const uint64_t *)raw;
            for (size_t i = 0; i < n_pixels; i++) {
                uint64_t v = src[i];
                if (do_swap) {
                    uint8_t *p = (uint8_t *)&v;
                    uint8_t t; t = p[0]; p[0] = p[7]; p[7] = t;
                    t = p[1]; p[1] = p[6]; p[6] = t;
                    t = p[2]; p[2] = p[5]; p[5] = t;
                    t = p[3]; p[3] = p[4]; p[4] = t;
                }
                out[i] = (double)(int64_t)v;
            }
        }
        break;
    }

    default:
        aio_log(AIO_LOG_ERROR, "XISF", "Unsupported dtype_size: %d", sf.dtype_size);
        free(out);
        return nullptr;
    }

    return out;
}

static void build_xisf_metadata(const std::vector<AIOFITSKeyword> &keywords,
                                 int w, int h, int channels,
                                 const XISFSampleFormat &sf,
                                 AIOImageMetadata &meta) {
    auto find_kw = [&](const char *name) -> const char* {
        for (auto &kw : keywords) {
            if (strcmp(kw.name, name) == 0) return kw.value;
        }
        return nullptr;
    };

    auto kw_float = [&](const char *name, double def = 0.0) -> double {
        const char *v = find_kw(name);
        if (!v || v[0] == '\0') return def;
        try { return std::stod(v); } catch (...) { return def; }
    };

    meta.geometry.width = w;
    meta.geometry.height = h;
    meta.geometry.channels = channels;
    meta.options.bits_per_sample = sf.bits_per_sample;
    meta.options.float_sample = sf.is_float;

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
    wcs.cd1_1 = kw_float("CD1_1"); wcs.cd1_2 = kw_float("CD1_2");
    wcs.cd2_1 = kw_float("CD2_1"); wcs.cd2_2 = kw_float("CD2_2");
    const char *radesys = find_kw("RADESYS");
    if (radesys) aio_safe_copy(wcs.radesys, AIO_RADESYS_MAX, radesys);
    else aio_safe_copy(wcs.radesys, AIO_RADESYS_MAX, "ICRS");
    const char *equinox_s = find_kw("EQUINOX");
    wcs.has_equinox = equinox_s ? 1 : 0;
    if (equinox_s) wcs.equinox = kw_float("EQUINOX", 2000.0);
    else wcs.equinox = 2000.0;
    wcs.has_wcs = (wcs.ctype1[0] != '\0' && wcs.ctype2[0] != '\0' &&
                   (std::abs(wcs.cd1_1) > 1e-15 || std::abs(wcs.cd1_2) > 1e-15 ||
                    std::abs(wcs.cd2_1) > 1e-15 || std::abs(wcs.cd2_2) > 1e-15)) ? 1 : 0;

    AIOObservationMetadata &obs = meta.observation;
    memset(&obs, 0, sizeof(obs));
    const char *date_obs = find_kw("DATE-OBS");
    if (date_obs) aio_safe_copy(obs.date_obs, AIO_DATE_MAX, date_obs);
    const char *object_name = find_kw("OBJECT");
    if (object_name) aio_safe_copy(obs.object_name, AIO_OBJECT_MAX, object_name);

    AIOCalibrationMetadata &cal = meta.calibration;
    memset(&cal, 0, sizeof(cal));
    cal.exptime = kw_float("EXPTIME");
    const char *filter = find_kw("FILTER");
    if (filter) aio_safe_copy(cal.filter_name, AIO_FILTER_MAX, filter);
    else aio_safe_copy(cal.filter_name, AIO_FILTER_MAX, "Unknown");
    cal.gain = kw_float("GAIN", 1.0);
    const char *bunit = find_kw("BUNIT");
    if (bunit) aio_safe_copy(cal.bunit, AIO_BUNIT_MAX, bunit);
    else aio_safe_copy(cal.bunit, AIO_BUNIT_MAX, "ADU");
}

int xisf_read_file(const char *path, AIOImageData *out) {
    aio_log(AIO_LOG_INFO, "XISF", "Reading: %s", path);

    FILE *fp = aio_fopen_utf8(path, "rb");
    if (!fp) {
        aio_log(AIO_LOG_ERROR, "XISF", "Cannot open: %s", path);
        return -1;
    }

    uint8_t magic[8];
    if (std::fread(magic, 1, 8, fp) != 8 || memcmp(magic, XISF_MAGIC, 8) != 0) {
        aio_log(AIO_LOG_ERROR, "XISF", "Not a valid XISF file: %s", path);
        std::fclose(fp);
        return -1;
    }

    uint8_t len_bytes[8];
    if (std::fread(len_bytes, 1, 8, fp) != 8) {
        aio_log(AIO_LOG_ERROR, "XISF", "Cannot read XML header length");
        std::fclose(fp);
        return -1;
    }
    uint64_t xml_length = 0;
    for (int i = 0; i < 8; i++) xml_length |= ((uint64_t)len_bytes[i]) << (8 * i);

    aio_log(AIO_LOG_INFO, "XISF", "XML header length: %llu bytes", (unsigned long long)xml_length);

    std::string xml_text(xml_length, '\0');
    if (std::fread(&xml_text[0], 1, xml_length, fp) != xml_length) {
        aio_log(AIO_LOG_ERROR, "XISF", "Cannot read XML header");
        std::fclose(fp);
        return -1;
    }

    XISFImageInfo img_info;
    if (find_first_image(xml_text, img_info) != 0) {
        aio_log(AIO_LOG_ERROR, "XISF", "No Image element found in XML");
        std::fclose(fp);
        return -1;
    }

    aio_log(AIO_LOG_INFO, "XISF", "Image: %dx%dx%d fmt=%s byteOrder=%s offset=%lld size=%lld",
            img_info.width, img_info.height, img_info.channels,
            img_info.sample_format.c_str(), img_info.byte_order.c_str(),
            (long long)img_info.data_offset, (long long)img_info.data_size);

    XISFSampleFormat sf = parse_sample_format(img_info.sample_format);
    int w = img_info.width;
    int h = img_info.height;
    int c = img_info.channels;
    size_t n_pixels = (size_t)w * h * c;
    size_t expected_bytes = n_pixels * sf.dtype_size;

    if (img_info.data_size <= 0 || img_info.data_offset <= 0) {
        aio_log(AIO_LOG_ERROR, "XISF", "Invalid data location: offset=%lld size=%lld",
                (long long)img_info.data_offset, (long long)img_info.data_size);
        std::fclose(fp);
        return -1;
    }

    if ((int64_t)expected_bytes > img_info.data_size) {
        aio_log(AIO_LOG_WARN, "XISF", "Expected %zu bytes but location has %lld",
                expected_bytes, (long long)img_info.data_size);
    }

    if (std::fseek(fp, (long)img_info.data_offset, SEEK_SET) != 0) {
        aio_log(AIO_LOG_ERROR, "XISF", "Seek to data offset failed");
        std::fclose(fp);
        return -1;
    }

    size_t read_size = std::min((size_t)img_info.data_size, expected_bytes);
    std::vector<uint8_t> raw(read_size);
    if (std::fread(raw.data(), 1, read_size, fp) != read_size) {
        aio_log(AIO_LOG_ERROR, "XISF", "Pixel data read incomplete");
        std::fclose(fp);
        return -1;
    }
    std::fclose(fp);

    int is_le = 0;
    union { uint32_t i; uint8_t b[4]; } u;
    u.i = 1;
    is_le = (u.b[0] == 1);

    int do_swap = 0;
    if (img_info.byte_order == "big") {
        do_swap = is_le ? 1 : 0;
    } else if (img_info.byte_order == "little") {
        do_swap = is_le ? 0 : 1;
    } else {
        do_swap = 0;
    }

    aio_log(AIO_LOG_INFO, "XISF", "Byte order: file=%s system=%s swap=%d",
            img_info.byte_order.c_str(), is_le ? "little" : "big", do_swap);

    // 双精度 ABI: 根据 AIO 模块精度模式决定读取到 data (FP32) 还是 data_f64 (FP64)
    // 修复: 使用 aio_internal_is_fp64() 替代 PrecisionContext (DLL 边界不共享)
    bool is_fp64 = (aio_internal_is_fp64() != 0);

    if (is_fp64) {
        double *pixel_data_f64 = convert_xisf_pixels_f64(raw.data(), n_pixels, sf, do_swap);
        if (!pixel_data_f64) {
            aio_log(AIO_LOG_ERROR, "XISF", "Pixel conversion (FP64) failed");
            return -1;
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
        aio_log(AIO_LOG_INFO, "XISF", "FP64 mode: pixels stored in data_f64 (no float32 downgrade)");
    } else {
        float *pixel_data = convert_xisf_pixels(raw.data(), n_pixels, sf, do_swap);
        if (!pixel_data) {
            aio_log(AIO_LOG_ERROR, "XISF", "Pixel conversion failed");
            return -1;
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
    out->bits_per_sample = sf.bits_per_sample;
    out->float_sample = sf.is_float;
    strncpy(out->source_format, "xisf", sizeof(out->source_format) - 1);
    aio_safe_copy(out->source_path, AIO_PATH_MAX, path);

    std::vector<AIOFITSKeyword> keywords;
    parse_fits_keywords(xml_text, keywords);

    out->keyword_count = (int)keywords.size();
    if (out->keyword_count > 0) {
        out->keywords = (AIOFITSKeyword *)malloc(out->keyword_count * sizeof(AIOFITSKeyword));
        std::memcpy(out->keywords, keywords.data(), out->keyword_count * sizeof(AIOFITSKeyword));
    } else {
        out->keywords = nullptr;
    }

    build_xisf_metadata(keywords, w, h, c, sf, out->metadata);

    aio_log(AIO_LOG_INFO, "XISF", "Read OK: %dx%d %dch fmt=%s keywords=%d",
            w, h, c, img_info.sample_format.c_str(), out->keyword_count);
    return 0;
}

int xisf_read_header_only(const char *path, AIOImageData *out) {
    aio_log(AIO_LOG_INFO, "XISF", "Reading header only: %s", path);

    FILE *fp = aio_fopen_utf8(path, "rb");
    if (!fp) {
        aio_log(AIO_LOG_ERROR, "XISF", "Cannot open: %s", path);
        return -1;
    }

    uint8_t magic[8];
    if (std::fread(magic, 1, 8, fp) != 8 || memcmp(magic, XISF_MAGIC, 8) != 0) {
        std::fclose(fp);
        return -1;
    }

    uint8_t len_bytes[8];
    if (std::fread(len_bytes, 1, 8, fp) != 8) {
        std::fclose(fp);
        return -1;
    }
    uint64_t xml_length = 0;
    for (int i = 0; i < 8; i++) xml_length |= ((uint64_t)len_bytes[i]) << (8 * i);

    std::string xml_text(xml_length, '\0');
    if (std::fread(&xml_text[0], 1, xml_length, fp) != xml_length) {
        std::fclose(fp);
        return -1;
    }
    std::fclose(fp);

    XISFImageInfo img_info;
    if (find_first_image(xml_text, img_info) != 0) return -1;

    XISFSampleFormat sf = parse_sample_format(img_info.sample_format);
    int w = img_info.width;
    int h = img_info.height;

    out->data = (float *)calloc((size_t)w * h, sizeof(float));
    out->width = w;
    out->height = h;
    out->channels = img_info.channels;
    out->bits_per_sample = sf.bits_per_sample;
    out->float_sample = sf.is_float;
    strncpy(out->source_format, "xisf", sizeof(out->source_format) - 1);
    aio_safe_copy(out->source_path, AIO_PATH_MAX, path);

    std::vector<AIOFITSKeyword> keywords;
    parse_fits_keywords(xml_text, keywords);
    out->keyword_count = (int)keywords.size();
    if (out->keyword_count > 0) {
        out->keywords = (AIOFITSKeyword *)malloc(out->keyword_count * sizeof(AIOFITSKeyword));
        std::memcpy(out->keywords, keywords.data(), out->keyword_count * sizeof(AIOFITSKeyword));
    } else {
        out->keywords = nullptr;
    }

    build_xisf_metadata(keywords, w, h, img_info.channels, sf, out->metadata);
    return 0;
}

int xisf_detect(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return 0;
    std::string e = ext;
    for (auto &c : e) c = tolower(c);
    return (e == ".xisf") ? 1 : 0;
}
