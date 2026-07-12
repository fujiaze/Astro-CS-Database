#include "../include/aio_pipeline.h"
#include "../include/astro_image_io.h"
#include "aio_log.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <new>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

/* ===========================================================================
 * aio_pipeline.cpp - PipelineFrame 动态内存管理 + XML 调试导出
 *
 * 设计要点:
 *   - 内存管理使用 C++ 风格 new[] / delete[]
 *   - 所有 alloc_* 在重复分配时先释放旧缓冲, 避免泄漏
 *   - 所有函数对 nullptr 入参做防御性检查
 *   - XML 导出为纯字符串拼接, 不依赖第三方库
 *   - Windows 下路径按 UTF-8 -> wchar 转换后 _wfopen, 支持中文路径
 * =========================================================================== */

/* ============================================================================
 * 内部辅助函数
 * ========================================================================= */

/* 标准 Base64 编码 (A-Z a-z 0-9 + /, 带 '=' padding) */
static std::string base64_encode(const unsigned char* data, size_t len) {
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        unsigned int n = (unsigned int)data[i] << 16;
        if (i + 1 < len) n |= (unsigned int)data[i + 1] << 8;
        if (i + 2 < len) n |= (unsigned int)data[i + 2];
        out.push_back(table[(n >> 18) & 0x3F]);
        out.push_back(table[(n >> 12) & 0x3F]);
        out.push_back(i + 1 < len ? table[(n >> 6) & 0x3F] : '=');
        out.push_back(i + 2 < len ? table[n & 0x3F] : '=');
    }
    return out;
}

/* XML 转义: < > & " (用于外部 C 字符串, 假定以 '\0' 结尾) */
static std::string xml_escape(const char* s) {
    std::string out;
    if (!s) return out;
    for (const char* p = s; *p; ++p) {
        switch (*p) {
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '&':  out += "&amp;";  break;
            case '"':  out += "&quot;"; break;
            default:   out.push_back(*p); break;
        }
    }
    return out;
}

/* XML 转义 (定长字符数组版本, 防止未终止数组越界读取) */
static std::string xml_escape_bounded(const char* s, size_t maxlen) {
    std::string out;
    if (!s) return out;
    for (size_t i = 0; i < maxlen && s[i] != 0; ++i) {
        switch (s[i]) {
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '&':  out += "&amp;";  break;
            case '"':  out += "&quot;"; break;
            default:   out.push_back(s[i]); break;
        }
    }
    return out;
}

/* 紧凑 double -> string */
static std::string fmt_double(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return std::string(buf);
}

/* 拼接 double 数组为 [a, b, c, ...] */
static std::string join_doubles(const double* arr, int n) {
    std::string out = "[";
    for (int i = 0; i < n; ++i) {
        if (i > 0) out += ", ";
        out += fmt_double(arr[i]);
    }
    out += "]";
    return out;
}

/* UTF-8 路径打开文件 (Windows: 转 wchar 后 _wfopen, 支持中文路径) */
static FILE* open_utf8_file(const char* path, const char* mode) {
    if (!path || !mode) return nullptr;
#ifdef _WIN32
    int wpath_len = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
    if (wpath_len <= 0) return nullptr;
    std::vector<wchar_t> wpath(wpath_len);
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath.data(), wpath_len);

    int wmode_len = MultiByteToWideChar(CP_UTF8, 0, mode, -1, nullptr, 0);
    if (wmode_len <= 0) return nullptr;
    std::vector<wchar_t> wmode(wmode_len);
    MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode.data(), wmode_len);

    return _wfopen(wpath.data(), wmode.data());
#else
    return std::fopen(path, mode);
#endif
}

/* ============================================================================
 * 动态内存管理
 * ========================================================================= */

AIO_EXPORT PipelineFrame* aio_pipeline_frame_create(void) {
    PipelineFrame* frame = new (std::nothrow) PipelineFrame;
    if (!frame) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "frame_create: alloc failed");
        return nullptr;
    }
    std::memset(frame, 0, sizeof(PipelineFrame));
    aio_log(AIO_LOG_DEBUG, "PIPELINE", "frame_create: ok (%zu bytes)", sizeof(PipelineFrame));
    return frame;
}

AIO_EXPORT void aio_pipeline_frame_destroy(PipelineFrame* frame) {
    if (!frame) return;
    aio_pipeline_frame_free_pixels(frame);
    aio_pipeline_frame_free_snr(frame);
    aio_pipeline_frame_free_weight(frame);
    aio_pipeline_frame_free_healpix(frame);
    delete frame;
    aio_log(AIO_LOG_DEBUG, "PIPELINE", "frame_destroy: ok");
}

AIO_EXPORT int aio_pipeline_frame_alloc_pixels(PipelineFrame* frame, int width, int height, int channels) {
    if (!frame || width <= 0 || height <= 0 || channels <= 0) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "alloc_pixels: invalid args");
        return 1;
    }
    size_t count = (size_t)width * (size_t)height * (size_t)channels;
    if (count > (SIZE_MAX / sizeof(float))) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "alloc_pixels: size overflow");
        return 2;
    }
    if (frame->pixel_data) {
        delete[] frame->pixel_data;
        frame->pixel_data = nullptr;
    }
    frame->pixel_data = new (std::nothrow) float[count];
    if (!frame->pixel_data) {
        frame->width = frame->height = frame->channels = 0;
        aio_log(AIO_LOG_ERROR, "PIPELINE", "alloc_pixels: new[] failed (count=%zu)", count);
        return 3;
    }
    std::memset(frame->pixel_data, 0, count * sizeof(float));
    frame->width = width;
    frame->height = height;
    frame->channels = channels;
    aio_log(AIO_LOG_DEBUG, "PIPELINE", "alloc_pixels: %dx%dx%d (%zu bytes)",
            width, height, channels, count * sizeof(float));
    return 0;
}

AIO_EXPORT int aio_pipeline_frame_alloc_snr(PipelineFrame* frame, int width, int height) {
    if (!frame || width <= 0 || height <= 0) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "alloc_snr: invalid args");
        return 1;
    }
    size_t count = (size_t)width * (size_t)height;
    if (count > (SIZE_MAX / sizeof(float))) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "alloc_snr: size overflow");
        return 2;
    }
    if (frame->snr_data) {
        delete[] frame->snr_data;
        frame->snr_data = nullptr;
    }
    frame->snr_data = new (std::nothrow) float[count];
    if (!frame->snr_data) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "alloc_snr: new[] failed (count=%zu)", count);
        return 3;
    }
    std::memset(frame->snr_data, 0, count * sizeof(float));
    aio_log(AIO_LOG_DEBUG, "PIPELINE", "alloc_snr: %dx%d (%zu bytes)",
            width, height, count * sizeof(float));
    return 0;
}

AIO_EXPORT int aio_pipeline_frame_alloc_weight(PipelineFrame* frame, int width, int height) {
    if (!frame || width <= 0 || height <= 0) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "alloc_weight: invalid args");
        return 1;
    }
    size_t count = (size_t)width * (size_t)height;
    if (count > (SIZE_MAX / sizeof(float))) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "alloc_weight: size overflow");
        return 2;
    }
    if (frame->weight_data) {
        delete[] frame->weight_data;
        frame->weight_data = nullptr;
    }
    frame->weight_data = new (std::nothrow) float[count];
    if (!frame->weight_data) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "alloc_weight: new[] failed (count=%zu)", count);
        return 3;
    }
    std::memset(frame->weight_data, 0, count * sizeof(float));
    aio_log(AIO_LOG_DEBUG, "PIPELINE", "alloc_weight: %dx%d (%zu bytes)",
            width, height, count * sizeof(float));
    return 0;
}

AIO_EXPORT int aio_pipeline_frame_alloc_healpix(PipelineFrame* frame, int64_t n_pixels) {
    if (!frame || n_pixels <= 0) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "alloc_healpix: invalid args");
        return 1;
    }
    size_t n = (size_t)n_pixels;
    if (n > (SIZE_MAX / sizeof(float)) || n > (SIZE_MAX / sizeof(int64_t))) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "alloc_healpix: size overflow");
        return 2;
    }
    /* 先释放旧的三个数组 */
    if (frame->healpix_pixels) { delete[] frame->healpix_pixels; frame->healpix_pixels = nullptr; }
    if (frame->healpix_snr)    { delete[] frame->healpix_snr;    frame->healpix_snr = nullptr; }
    if (frame->healpix_ipix)   { delete[] frame->healpix_ipix;   frame->healpix_ipix = nullptr; }

    frame->healpix_pixels = new (std::nothrow) float[n];
    frame->healpix_snr    = new (std::nothrow) float[n];
    frame->healpix_ipix   = new (std::nothrow) int64_t[n];
    if (!frame->healpix_pixels || !frame->healpix_snr || !frame->healpix_ipix) {
        /* 部分失败: 全部回滚, 避免半分配状态 */
        if (frame->healpix_pixels) { delete[] frame->healpix_pixels; frame->healpix_pixels = nullptr; }
        if (frame->healpix_snr)    { delete[] frame->healpix_snr;    frame->healpix_snr = nullptr; }
        if (frame->healpix_ipix)   { delete[] frame->healpix_ipix;   frame->healpix_ipix = nullptr; }
        frame->n_healpix = 0;
        aio_log(AIO_LOG_ERROR, "PIPELINE", "alloc_healpix: new[] failed (n=%zu)", n);
        return 3;
    }
    std::memset(frame->healpix_pixels, 0, n * sizeof(float));
    std::memset(frame->healpix_snr,    0, n * sizeof(float));
    std::memset(frame->healpix_ipix,   0, n * sizeof(int64_t));
    frame->n_healpix = n_pixels;
    aio_log(AIO_LOG_DEBUG, "PIPELINE", "alloc_healpix: n=%lld (%zu bytes)",
            (long long)n_pixels, n * (sizeof(float) + sizeof(float) + sizeof(int64_t)));
    return 0;
}

AIO_EXPORT void aio_pipeline_frame_free_pixels(PipelineFrame* frame) {
    if (!frame) return;
    if (frame->pixel_data) {
        delete[] frame->pixel_data;
        frame->pixel_data = nullptr;
    }
    frame->width = 0;
    frame->height = 0;
    frame->channels = 0;
}

AIO_EXPORT void aio_pipeline_frame_free_snr(PipelineFrame* frame) {
    if (!frame) return;
    if (frame->snr_data) {
        delete[] frame->snr_data;
        frame->snr_data = nullptr;
    }
}

AIO_EXPORT void aio_pipeline_frame_free_weight(PipelineFrame* frame) {
    if (!frame) return;
    if (frame->weight_data) {
        delete[] frame->weight_data;
        frame->weight_data = nullptr;
    }
}

AIO_EXPORT void aio_pipeline_frame_free_healpix(PipelineFrame* frame) {
    if (!frame) return;
    if (frame->healpix_pixels) { delete[] frame->healpix_pixels; frame->healpix_pixels = nullptr; }
    if (frame->healpix_snr)    { delete[] frame->healpix_snr;    frame->healpix_snr = nullptr; }
    if (frame->healpix_ipix)   { delete[] frame->healpix_ipix;   frame->healpix_ipix = nullptr; }
    frame->n_healpix = 0;
}

AIO_EXPORT size_t aio_pipeline_frame_memory_usage(const PipelineFrame* frame) {
    if (!frame) return 0;
    size_t total = 0;
    if (frame->pixel_data) {
        total += (size_t)frame->width * (size_t)frame->height * (size_t)frame->channels * sizeof(float);
    }
    if (frame->snr_data) {
        total += (size_t)frame->width * (size_t)frame->height * sizeof(float);
    }
    if (frame->weight_data) {
        total += (size_t)frame->width * (size_t)frame->height * sizeof(float);
    }
    if (frame->n_healpix > 0 &&
        (frame->healpix_pixels || frame->healpix_snr || frame->healpix_ipix)) {
        size_t n = (size_t)frame->n_healpix;
        total += n * (sizeof(float) + sizeof(float) + sizeof(int64_t));
    }
    return total;
}

/* ============================================================================
 * XML 调试导出
 * ========================================================================= */

AIO_EXPORT int aio_pipeline_export_xml(const PipelineFrame* frame,
                                        const char* path,
                                        const char* comment) {
    if (!frame || !path) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "export_xml: frame or path is null");
        return 1;
    }

    /* 检测 comment 中的 skip_pixels 关键字 */
    bool skip_pixels = false;
    if (comment && std::strstr(comment, "skip_pixels") != nullptr) {
        skip_pixels = true;
    }

    std::string xml;
    xml.reserve(8192);

    /* ---- XML 头 ---- */
    xml += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";

    /* ---- 注释 (转义后输出) ---- */
    if (comment) {
        xml += "<!-- 调试导出: ";
        xml += xml_escape(comment);
        xml += " -->\n";
    }

    xml += "<PipelineFrame>\n";

    /* ---- Image ---- */
    {
        int w = frame->width;
        int h = frame->height;
        int c = frame->channels;
        xml += "  <Image width=\"";
        xml += std::to_string(w);
        xml += "\" height=\"";
        xml += std::to_string(h);
        xml += "\" channels=\"";
        xml += std::to_string(c);
        xml += "\">\n";

        size_t count = (w > 0 && h > 0 && c > 0)
                     ? (size_t)w * (size_t)h * (size_t)c
                     : 0;

        if (!skip_pixels && frame->pixel_data && count > 0) {
            size_t byte_len = count * sizeof(float);
            std::string b64 = base64_encode(
                reinterpret_cast<const unsigned char*>(frame->pixel_data), byte_len);
            xml += "    <Pixels encoding=\"base64\" dtype=\"float32\" size=\"";
            xml += std::to_string(count);
            xml += "\">\n";
            xml += b64;
            xml += "\n    </Pixels>\n";
        } else if (skip_pixels) {
            /* 只导出维度, 跳过像素 base64 */
            xml += "    <Pixels encoding=\"base64\" dtype=\"float32\" size=\"";
            xml += std::to_string(count);
            xml += "\" skipped=\"true\"/>\n";
        }
        xml += "  </Image>\n";
    }

    /* ---- WCS ---- */
    xml += "  <WCS>\n";
    xml += "    <CD cd11=\"";
    xml += fmt_double(frame->cd[0]);
    xml += "\" cd12=\"";
    xml += fmt_double(frame->cd[1]);
    xml += "\" cd21=\"";
    xml += fmt_double(frame->cd[2]);
    xml += "\" cd22=\"";
    xml += fmt_double(frame->cd[3]);
    xml += "\"/>\n";
    xml += "    <CRVAL ra=\"";
    xml += fmt_double(frame->crval[0]);
    xml += "\" dec=\"";
    xml += fmt_double(frame->crval[1]);
    xml += "\"/>\n";
    xml += "    <CRPIX x=\"";
    xml += fmt_double(frame->crpix[0]);
    xml += "\" y=\"";
    xml += fmt_double(frame->crpix[1]);
    xml += "\"/>\n";
    xml += "    <CTYPE1>";
    xml += xml_escape_bounded(frame->ctype1, sizeof(frame->ctype1));
    xml += "</CTYPE1>\n";
    xml += "    <CTYPE2>";
    xml += xml_escape_bounded(frame->ctype2, sizeof(frame->ctype2));
    xml += "</CTYPE2>\n";
    xml += "    <SIP order=\"";
    xml += std::to_string(frame->sip_order);
    xml += "\" ap_order=\"";
    xml += std::to_string(frame->sip_ap_order);
    xml += "\">\n";
    xml += "      <A>";
    xml += join_doubles(frame->sip_a, 36);
    xml += "</A>\n";
    xml += "      <B>";
    xml += join_doubles(frame->sip_b, 36);
    xml += "</B>\n";
    xml += "      <AP>";
    xml += join_doubles(frame->sip_ap, 36);
    xml += "</AP>\n";
    xml += "      <BP>";
    xml += join_doubles(frame->sip_bp, 36);
    xml += "</BP>\n";
    xml += "    </SIP>\n";
    xml += "  </WCS>\n";

    /* ---- HealPix ---- */
    xml += "  <HealPix nside=\"";
    xml += std::to_string(frame->nside);
    xml += "\" nested=\"";
    xml += std::to_string(frame->nested);
    xml += "\" n_pixels=\"";
    xml += std::to_string((long long)frame->n_healpix);
    xml += "\" pixfrac=\"";
    xml += fmt_double(frame->pixfrac);
    xml += "\">\n";

    if (frame->n_healpix > 0) {
        size_t n = (size_t)frame->n_healpix;

        /* HealPix 像素值 */
        if (!skip_pixels && frame->healpix_pixels) {
            std::string b64 = base64_encode(
                reinterpret_cast<const unsigned char*>(frame->healpix_pixels),
                n * sizeof(float));
            xml += "    <Pixels encoding=\"base64\" dtype=\"float32\" size=\"";
            xml += std::to_string((long long)frame->n_healpix);
            xml += "\">";
            xml += b64;
            xml += "</Pixels>\n";
        } else if (skip_pixels) {
            xml += "    <Pixels encoding=\"base64\" dtype=\"float32\" size=\"";
            xml += std::to_string((long long)frame->n_healpix);
            xml += "\" skipped=\"true\"/>\n";
        }

        /* HealPix 像素号 (索引数据, skip_pixels 时仍导出) */
        if (frame->healpix_ipix) {
            std::string b64 = base64_encode(
                reinterpret_cast<const unsigned char*>(frame->healpix_ipix),
                n * sizeof(int64_t));
            xml += "    <Ipix encoding=\"base64\" dtype=\"int64\" size=\"";
            xml += std::to_string((long long)frame->n_healpix);
            xml += "\">";
            xml += b64;
            xml += "</Ipix>\n";
        }
    }
    xml += "  </HealPix>\n";

    /* ---- Metadata ---- */
    xml += "  <Metadata>\n";
    xml += "    <SourcePath>";
    xml += xml_escape_bounded(frame->source_path, sizeof(frame->source_path));
    xml += "</SourcePath>\n";
    xml += "    <ObjectName>";
    xml += xml_escape_bounded(frame->object_name, sizeof(frame->object_name));
    xml += "</ObjectName>\n";
    xml += "    <Exptime>";
    xml += fmt_double(frame->exptime);
    xml += "</Exptime>\n";
    xml += "    <FilterName>";
    xml += xml_escape_bounded(frame->filter_name, sizeof(frame->filter_name));
    xml += "</FilterName>\n";
    xml += "    <RmsArcsec>";
    xml += fmt_double(frame->rms_arcsec);
    xml += "</RmsArcsec>\n";
    xml += "    <NPairs>";
    xml += std::to_string(frame->n_pairs);
    xml += "</NPairs>\n";
    xml += "  </Metadata>\n";

    /* ---- Status ---- */
    xml += "  <Status stages_completed=\"";
    xml += std::to_string(frame->stages_completed);
    xml += "\" has_wcs=\"";
    xml += std::to_string(frame->has_wcs);
    xml += "\" has_sip=\"";
    xml += std::to_string(frame->has_sip);
    xml += "\"/>\n";

    xml += "</PipelineFrame>\n";

    /* ---- 写文件 (UTF-8 路径) ---- */
    FILE* fp = open_utf8_file(path, "wb");
    if (!fp) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "export_xml: open file failed: %s", path);
        return 2;
    }
    size_t written = std::fwrite(xml.data(), 1, xml.size(), fp);
    std::fclose(fp);
    if (written != xml.size()) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "export_xml: write incomplete (%zu/%zu)",
                written, xml.size());
        return 3;
    }
    aio_log(AIO_LOG_INFO, "PIPELINE", "export_xml: ok -> %s (%zu bytes, skip_pixels=%d)",
            path, xml.size(), skip_pixels ? 1 : 0);
    return 0;
}
